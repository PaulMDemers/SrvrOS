#include <srvros/intel_engine_rehold.h>
static bool fail(struct intel_engine_rehold *r,unsigned why){r->reason|=why;return false;}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool source(const struct intel_engine_rehold *r) {
 const struct intel_engine_held_reset_result *x=r->reset;
 return r->object==r && x && x->rehold==r && intel_engine_prepare_reset_live(x) &&
  x->stage==4 && x->entered && x->provider_enter && !x->provider_exit &&
  !x->reason && !x->complete && x->epoch_advanced &&
  x->epoch_before!=UINT32_MAX && x->epoch_after==x->epoch_before+1 &&
  x->epoch_after==r->epoch && x->preparation==r->preparation && x->parent==r->parent &&
  r->preparation->parent==r->parent && r->borrow==&r->preparation->borrow &&
  x->origin_us==r->origin_us && x->attempts==2 && x->writes==2 && x->acks==2 &&
  x->snapshots==2 && x->snapshot_attempts==2 && x->counts[0]==9 && x->counts[1]==9 &&
  x->settle_begin_us && x->settle_end_us>=x->settle_begin_us &&
  x->settle_end_us-x->settle_begin_us>=INTEL_BCS_RESET_SETTLE_US &&
  x->last_us>=x->settle_end_us && r->parent->engine_epoch==r->epoch &&
  r->parent->begin==r->owner_origin_us && r->owner_origin_us && r->owner_origin_us<=r->origin_us;
}
static bool clock_ok(struct intel_engine_rehold *r,struct intel_engine_rehold_io io) {
 uint64_t at=io.now(io.ctx);
 if(!at || at<r->last_us || at<r->origin_us || at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US ||
    !r->owner_origin_us || at<r->owner_origin_us || at-r->owner_origin_us>GT_ACCESS_TOTAL_US) {
  if(!(r->reason&IER_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  return fail(r,IER_CLOCK);
 }
 r->last_us=at;return true;
}
static bool guard(struct intel_engine_rehold *r,struct intel_engine_rehold_io io) {
 if(!clock_ok(r,io))return false;
 if(!source(r))return fail(r,IER_PARENT);
 bool ok=io.guard(io.ctx);
 if(!source(r))return fail(r,IER_PARENT);
 if(!clock_ok(r,io))return false;
 if(!source(r))return fail(r,IER_PARENT);
 return ok?true:fail(r,IER_GUARD);
}
static bool control_valid(uint32_t v){return v!=UINT32_MAX && !(v&0xfffcu);}
static bool sample(struct intel_engine_rehold *r,struct intel_engine_rehold_io io,unsigned n) {
 if(!guard(r,io))return false;
 ++r->sample_attempts;
 bool ok=io.sample(io.ctx,r->words[n],&r->counts[n]);
 if(r->counts[n]>6){r->counts[n]=0;fail(r,IER_READ);}
 r->reads+=r->counts[n];if(r->counts[n]==6)++r->samples;
 bool match=true;
 for(unsigned e=0;e<r->counts[n];e++) {
  uint32_t v=r->words[n][e];
  if(e==5){if(v)match=false;continue;}
  unsigned want=e<4 || n==1 || n==2?3:0;
  if(!control_valid(v) || (v&3)!=want)match=false;
  if(e<4 && v!=r->preparation->words[2][e])match=false;
  if(n && (v&~3u)!=(r->words[0][e]&~3u))match=false;
  if(n==2 && v!=r->words[1][e])match=false;
  if(n==3 && v!=r->words[0][e])match=false;
 }
 if(!match)fail(r,IER_STATE);
 bool guarded=guard(r,io);
 if(!ok || r->counts[n]!=6)fail(r,IER_READ);
 return guarded && match && ok && r->counts[n]==6;
}
static bool transition(struct intel_engine_rehold *r,struct intel_engine_rehold_io io,unsigned n) {
 if(r->child)return fail(r,IER_CHILD);
 if(!guard(r,io))return false;
 uint64_t phase=r->last_us;r->phase_begin[n]=phase;
 ++r->attempts;r->uncertain=true;
 uint32_t v=UINT32_MAX;
 enum intel_gt_write wrote=io.transition(io.ctx,n?INTEL_ENGINE_PREPARE_CANCEL:INTEL_ENGINE_PREPARE_REQUEST,&v);
 if(wrote==GT_WRITE_DONE){++r->writes;++r->reads;r->ack[n]=r->final[n]=v;}
 bool values=wrote==GT_WRITE_DONE && control_valid(v) && (v&1)==(unsigned)!n &&
  (v&~3u)==(r->words[0][4]&~3u);
 if(wrote==GT_WRITE_DONE && !values)fail(r,IER_STATE);
 bool guarded=guard(r,io);
 if(wrote!=GT_WRITE_DONE)fail(r,IER_WRITE);
 if(!guarded || !values)goto done;
 for(unsigned i=0;i<=INTEL_ENGINE_PREPARE_POLLS;i++) {
  if(r->last_us-phase>INTEL_ENGINE_PREPARE_PHASE_US){fail(r,IER_TIMEOUT);goto done;}
  if(!control_valid(v) || (v&1)!=(unsigned)!n || (v&~3u)!=(r->words[0][4]&~3u)){fail(r,IER_STATE);goto done;}
  if((v&3)==(n?0u:3u)){++r->acks;break;}
  if(i==INTEL_ENGINE_PREPARE_POLLS){fail(r,IER_TIMEOUT);goto done;}
  io.pause(io.ctx);
  if(!guard(r,io))goto done;
  bool ok=io.control(io.ctx,&v);
  if(ok){++r->reads;++r->polls;++r->phase_polls[n];r->final[n]=v;}
  if(ok && (!control_valid(v) || (v&1)!=(unsigned)!n || (v&~3u)!=(r->words[0][4]&~3u)))fail(r,IER_STATE);
  guarded=guard(r,io);
  if(!ok)fail(r,IER_READ);
  if(!guarded || !ok || r->reason)goto done;
 }
done:
 r->phase_end[n]=r->last_us;
 return !r->reason && r->acks==n+1;
}
bool intel_engine_rehold_live(const struct intel_engine_rehold *r) {
 bool valid=r && source(r) && r->busy && !r->reason && !r->complete && !r->restored &&
  r->uncertain && r->stage==3 && r->child_started && r->child_active && !r->child_completed &&
  r->attempts==1 && r->writes==1 && r->acks==1 && r->sample_attempts==2 && r->samples==2 &&
  r->counts[0]==6 && r->counts[1]==6 && r->begin_us && r->last_us>=r->begin_us &&
  r->last_us>=r->origin_us && r->last_us-r->origin_us<=INTEL_ENGINE_PREPARE_TOTAL_US &&
  r->last_us>=r->owner_origin_us && r->last_us-r->owner_origin_us<=GT_ACCESS_TOTAL_US;
 if(!valid || r->words[0][5] || r->words[1][5])return false;
 for(unsigned e=0;e<5;e++)if(!control_valid(r->words[1][e]) || (r->words[1][e]&3)!=3 ||
    (r->words[1][e]&~3u)!=(r->words[0][e]&~3u) ||
    (e<4 && r->words[0][e]!=r->preparation->words[2][e]))return false;
 return true;
}
bool intel_engine_rehold_borrow(struct intel_engine_rehold *r,struct intel_engine_rehold_child *c) {
 if(!intel_engine_rehold_live(r) || !c || c->object || r->child || r->child_borrows)return false;
 if(overlap(r,sizeof(*r),c,sizeof(*c)) || overlap(r->reset,sizeof(*r->reset),c,sizeof(*c)) ||
    overlap(r->preparation,sizeof(*r->preparation),c,sizeof(*c)) || overlap(r->parent,sizeof(*r->parent),c,sizeof(*c)))return false;
 *c=(struct intel_engine_rehold_child){.object=c,.parent=r,.active=true};r->child=c;++r->child_borrows;return true;
}
bool intel_engine_rehold_child_live(const struct intel_engine_rehold_child *c) {
 return c && c->object==c && c->active && c->parent &&
  intel_engine_rehold_live(c->parent) && c->parent->child==c &&
  c->parent->child_borrows==1 && !c->parent->child_returns;
}
bool intel_engine_rehold_return(struct intel_engine_rehold_child *c) {
 if(!intel_engine_rehold_child_live(c) || c->dependent || c->dependent_borrows>1 ||
    c->dependent_borrows!=c->dependent_returns)return false;
 c->parent->child=0;++c->parent->child_returns;c->active=false;return true;
}
bool intel_engine_rehold_run(struct intel_engine_rehold *r,struct intel_engine_held_reset_result *x,struct intel_engine_rehold_io io) {
 if(!r || r->object)return false;
 if(x && (overlap(r,sizeof(*r),x,sizeof(*x)) ||
    (x->preparation && overlap(r,sizeof(*r),x->preparation,sizeof(*x->preparation))) ||
    (x->parent && overlap(r,sizeof(*r),x->parent,sizeof(*x->parent)))))return false;
 *r=(struct intel_engine_rehold){.object=r,.reset=x};
 if(!x || !x->preparation || !x->parent || !io.guard || !io.sample || !io.control ||
    !io.transition || !io.observe || !io.now || !io.pause)return fail(r,IER_INPUT);
 if(x->rehold)return fail(r,IER_PARENT);
 x->rehold=r;
 r->preparation=x->preparation;r->parent=x->parent;r->borrow=&x->preparation->borrow;
 r->origin_us=x->origin_us;r->owner_origin_us=x->parent->begin;r->last_us=x->last_us;r->epoch=x->epoch_after;r->busy=true;
 if(!source(r)){fail(r,IER_PARENT);goto done;}
 if(!guard(r,io))goto done;
 r->begin_us=r->last_us;r->stage=1;
 if(!sample(r,io,0))goto done;
 r->stage=2;if(!transition(r,io,0) || !sample(r,io,1))goto done;
 r->stage=3;r->child_started=r->child_active=true;
 bool observed=io.observe(io.ctx,r);
 r->child_active=false;
 bool guarded=guard(r,io);
 if(!observed || r->child || r->child_borrows!=r->child_returns)fail(r,IER_CHILD);
 if(!guarded || r->reason)goto done;
 r->child_completed=true;
 if(!sample(r,io,2))goto done;
 r->stage=4;if(!transition(r,io,1) || !sample(r,io,3))goto done;
 r->restored=true;r->stage=5;
done:
 (void)clock_ok(r,io);r->end_us=r->last_us;
 if(!source(r))fail(r,IER_PARENT);
 if(r->begin_us && r->end_us<=r->begin_us)fail(r,IER_CLOCK);
 r->complete=!r->reason && r->stage==5 && r->restored && r->child_completed && !r->child &&
  r->attempts==2 && r->writes==2 && r->acks==2 && r->samples==4 && r->sample_attempts==4;
 if(r->complete)r->uncertain=false;
 r->busy=false;return r->complete;
}
