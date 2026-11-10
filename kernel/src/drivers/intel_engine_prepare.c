#include <srvros/intel_engine_prepare.h>
#ifdef SRVROS_INTEL_PREPARE_RESET
#include <srvros/intel_engine_held_reset.h>
#endif
struct intel_engine_held_reset_io;
static bool fail(struct intel_engine_prepare_result *r,unsigned why) {r->reason|=why;return false;}
static bool prerequisite(const struct intel_engine_prepare_result *r) {
 const struct intel_takeover_result *t=r->preflight;
 const struct intel_completion_result *c=t?t->census:0;
 if(!t || t->object!=t || t->parent!=r->parent || t->epoch!=r->epoch ||
    !t->complete || !t->quiescent || !t->stable || t->reason || !t->returned ||
    t->borrow.object!=&t->borrow || t->borrow.parent!=r->parent || t->borrow.active ||
    t->attempts!=2 || t->samples!=2 || t->reads!=12 || t->counts[0]!=6 || t->counts[1]!=6 ||
    !t->begin_us || t->end_us<=t->begin_us || t->end_us-t->begin_us>100000 ||
    !c || c->object!=c || c->parent!=r->parent || c->engine_epoch!=r->epoch ||
    !c->complete || !c->stable || c->reason || c->reads!=92 || c->batches!=2 ||
    c->samples!=2 || c->groups!=10 || c->contexts!=2 || !c->begin_us ||
    c->end_us<=c->begin_us || c->end_us-c->begin_us>100000 ||
    t->begin_us<=c->end_us || t->begin_us-c->end_us>100000 || c->enabled || c->pending ||
    c->flip_wait || c->tlb_pending || c->parser_idle!=3)return false;
 for(unsigned n=0;n<2;n++)for(unsigned e=0;e<5;e++) {
  const uint32_t *w=c->words[n][e];
  if(w[0] || w[1] || w[2] || w[3] || w[4]!=1 || w[5] || w[6] || w[7] ||
     !(c->context[n][0]&(1u<<9)) || !(c->context[n][3]&(1u<<9)) ||
     (t->words[n][e]&0xffffu) || t->words[n][5])return false;
 }
 return true;
}
static bool parent_epoch(const struct intel_engine_prepare_result *r,uint32_t epoch) {
 struct intel_gt_access *g=r->parent;
 return r->object==r && g && g->object==g && g->engine_epoch==epoch &&
  g->state==GT_ACCESS_AWAKE && g->pinned && display_access_held(&g->exclusion) &&
  prerequisite(r) && r->borrow.object==&r->borrow && r->borrow.parent==g &&
  r->borrow.active && !r->borrow.next && g->borrows==1 && g->dependents==&r->borrow;
}
static bool parent(const struct intel_engine_prepare_result *r) {return parent_epoch(r,r->epoch);}
static bool epoch_matches(const struct intel_engine_prepare_result *r,const struct intel_engine_held_reset_result *x) {
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(x)return x->object==x && x->preparation==r && r->reset==x && x->parent==r->parent &&
  x->epoch_before==r->epoch && x->epoch_before!=UINT32_MAX &&
  x->epoch_after==x->epoch_before+(unsigned)x->epoch_advanced && r->parent->engine_epoch==x->epoch_after;
#else
 (void)x;
#endif
 return r->parent->engine_epoch==r->epoch;
}
static bool parent_mode(const struct intel_engine_prepare_result *r,const struct intel_engine_held_reset_result *x) {
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(x)return r->parent && epoch_matches(r,x) &&
  parent_epoch(r,x->epoch_before+(unsigned)x->epoch_advanced);
#else
 (void)x;
#endif
 return parent(r);
}
#ifdef SRVROS_INTEL_PREPARE_RESET
bool intel_engine_prepare_reset_live(const struct intel_engine_held_reset_result *x) {
 if(!x || x->object!=x || !x->preparation)return false;
 const struct intel_engine_prepare_result *r=x->preparation;
 if(!parent_mode(r,x) || !x->active || !x->attempted || x->complete ||
    r->stage!=7 || !r->blocked || !r->admitted || !r->held || r->returned || r->complete || r->reason ||
    !r->uncertain || r->action_active || !r->action_attempted || !r->action_completed ||
    r->ready!=31 || r->cancelled || r->attempts!=5 || r->writes!=5 || r->acks!=5 ||
    r->sample_attempts!=3 || r->snapshots!=3 || !r->begin_us || x->origin_us!=r->begin_us ||
    r->last_us<r->begin_us || r->last_us-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US)return false;
 for(unsigned n=0;n<3;n++)if(r->counts[n]!=6 || r->words[n][5])return false;
 for(unsigned e=0;e<5;e++)if(r->words[0][e]!=r->preflight->words[1][e] ||
    r->words[1][e]==UINT32_MAX || (r->words[1][e]&0xffffu)!=3 ||
    r->words[1][e]!=r->words[2][e])return false;
 return true;
}
#endif
bool intel_engine_prepare_action_live(const struct intel_engine_prepare_result *r) {
 if(!r || !parent(r) || !r->action_active || !r->action_attempted || r->action_completed ||
    !r->blocked || !r->admitted || r->held || r->returned || r->complete || r->reason ||
    !r->uncertain || r->stage!=2 || r->ready!=31 || r->cancelled ||
    r->attempts!=5 || r->writes!=5 || r->acks!=5 || r->sample_attempts!=2 ||
    r->snapshots!=2 || r->counts[0]!=6 || r->counts[1]!=6 || !r->begin_us ||
    r->action_begin_us<r->begin_us || r->last_us<r->action_begin_us ||
    r->last_us-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US)return false;
 for(unsigned e=0;e<5;e++)if(r->words[1][e]==UINT32_MAX ||
    (r->words[1][e]&0xffffu)!=3 || r->words[0][e]!=r->preflight->words[1][e])return false;
 return !r->words[1][5] && !r->words[0][5];
}
static bool clock_ok(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US) {
  r->clock_at=t;r->clock_prev=r->last_us;return fail(r,IEP_CLOCK);
 }
 r->last_us=t;return true;
}
static bool guard(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io,const struct intel_engine_held_reset_result *x) {
 if(!clock_ok(r,io))return false;
 if(!parent_mode(r,x))return fail(r,IEP_PARENT);
 bool ok=io.guard(io.ctx);
 if(!parent_mode(r,x))return fail(r,IEP_PARENT);
 if(!clock_ok(r,io))return false;
 if(!parent_mode(r,x))return fail(r,IEP_PARENT);
 return ok?true:fail(r,IEP_GUARD);
}
static bool valid_control(uint32_t v) {return v!=UINT32_MAX && !(v&0xfffcu);}
static bool sample(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io,unsigned n,unsigned low,const struct intel_engine_held_reset_result *x) {
 if(!guard(r,io,x))return false;
 r->sample_attempts++;
 bool ok=io.sample(io.ctx,r->words[n],&r->counts[n]);
 if(r->counts[n]>6){r->counts[n]=0;return fail(r,IEP_COUNT);}
 r->reads+=r->counts[n];if(r->counts[n]==6)r->snapshots++;
 if(!guard(r,io,x))return false;
 if(!ok || r->counts[n]!=6)return fail(r,IEP_READ);
 for(unsigned e=0;e<5;e++)if(!valid_control(r->words[n][e]) || (r->words[n][e]&3)!=(low==4?(e==4?0u:3u):low))return fail(r,IEP_STATE);
 if(r->words[n][5])return fail(r,IEP_STATE);
 return true;
}
static bool transition(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io,unsigned e,bool request,const struct intel_engine_held_reset_result *x) {
 if(!guard(r,io,x))return false;
 uint64_t phase=r->last_us;
 unsigned n=r->acks;uint32_t ack=UINT32_MAX;
 ++r->attempts;
 enum intel_gt_write wrote=io.transition(io.ctx,e,request?INTEL_ENGINE_PREPARE_REQUEST:INTEL_ENGINE_PREPARE_CANCEL,&ack);
 if(wrote!=GT_WRITE_NONE)r->uncertain=true;
 if(wrote!=GT_WRITE_DONE)return fail(r,IEP_WRITE);
 ++r->writes;++r->reads;r->ack[n]=r->final[n]=ack;++r->acks;
 if(!guard(r,io,x))return false;
 if(r->last_us<phase || r->last_us-phase>INTEL_ENGINE_PREPARE_PHASE_US){r->clock_at=r->last_us;r->clock_prev=phase;return fail(r,IEP_TIMEOUT);}
 unsigned want=request?3:0;
 for(unsigned i=0;i<=INTEL_ENGINE_PREPARE_POLLS;i++) {
  if(!valid_control(ack) || (ack&1)!=(unsigned)request)return fail(r,IEP_STATE);
  if((ack&3)==want){if(request)r->ready|=1u<<e;else r->cancelled|=1u<<e;return true;}
  if(i==INTEL_ENGINE_PREPARE_POLLS)return fail(r,IEP_TIMEOUT);
  io.pause(io.ctx);
  if(!guard(r,io,x))return false;
  bool ok=io.control(io.ctx,e,&ack);
  if(ok){++r->reads;++r->polls;++r->phase_polls[n];r->final[n]=ack;}
  if(!guard(r,io,x))return false;
  if(!ok)return fail(r,IEP_READ);
  if(r->last_us<phase || r->last_us-phase>INTEL_ENGINE_PREPARE_PHASE_US){r->clock_at=r->last_us;r->clock_prev=phase;return fail(r,IEP_TIMEOUT);}
 }
 return false;
}
static bool prepare(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io,
 const struct intel_engine_prepare_action *action,struct intel_engine_held_reset_result *x,
 const struct intel_engine_held_reset_io *reset_io) {
 if(!r || r->object)return false;
 *r=(struct intel_engine_prepare_result){.object=r,.parent=g,.preflight=t,.reset=x};
#ifndef SRVROS_INTEL_PREPARE_RESET
 (void)reset_io;
#endif
 if(!g || !policy || !io.guard || !io.sample || !io.control || !io.transition || !io.preserve || !io.now || !io.pause || (action && !action->observe))return fail(r,IEP_INPUT);
 r->epoch=g->engine_epoch;
 if(!prerequisite(r) || g->borrows || g->dependents || !intel_gt_access_check(g))return fail(r,IEP_INPUT);
 r->begin_us=r->last_us=io.now(io.ctx);
 if(!r->begin_us || r->begin_us<=t->end_us || r->begin_us-t->end_us>100000)return fail(r,IEP_CLOCK);
 if(!intel_gt_access_borrow(g,&r->borrow))return fail(r,IEP_PARENT);
 unsigned writes=g->write_attempts;
 r->blocked=!intel_gt_access_close(g) && g->write_attempts==writes;
 if(!r->blocked || !parent_mode(r,x)){fail(r,IEP_PARENT);goto done;}
 if(!sample(r,io,0,0,x))goto done;
 for(unsigned i=0;i<6;i++)if(r->words[0][i]!=t->words[1][i]){fail(r,IEP_STATE);goto done;}
 r->admitted=true;
 r->stage=1;for(unsigned e=0;e<5;e++){r->index=e;if(!transition(r,io,e,true,x))goto done;}
 r->stage=2;
 if(!sample(r,io,1,3,x))goto done;
 if(action) {
  if(r->ready!=31 || !guard(r,io,x)){fail(r,IEP_ACTION);goto done;}
  r->action_begin_us=r->last_us;r->action_attempted=true;
  r->action_active=true;
  bool observed=action->observe(action->ctx,r);
  r->action_active=false;
  /* Earlier request stores already set uncertainty. Neither callback failure
   * nor a lost parent/clock admits cancellation or retirement through it. */
  bool guarded=guard(r,io,x);
  r->action_end_us=r->last_us;
  if(!observed || !guarded){fail(r,IEP_ACTION);goto done;}
 }
 if(!sample(r,io,2,3,x))goto done;
 for(unsigned e=0;e<6;e++)if(r->words[1][e]!=r->words[2][e]){fail(r,IEP_STATE);goto done;}
 if(action)r->action_completed=true;
 r->held=true;
 unsigned final_sample=3;
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(x) {
  r->stage=7;x->origin_us=r->begin_us;x->last_us=r->last_us;x->attempted=true;x->active=true;
  bool reset_ok=intel_engine_held_reset_run(x,*reset_io);x->active=false;
  bool guarded=guard(r,io,x);
  if(!reset_ok || !guarded){fail(r,IEP_ACTION);goto done;}
  /* BCS request/ready clear on reset, while the other four holds survive. */
  if(!sample(r,io,3,4,x))goto done;
  for(unsigned e=0;e<4;e++)if(r->words[3][e]!=r->words[2][e]){fail(r,IEP_STATE);goto done;}
  final_sample=4;
 }
#endif
 r->stage=3;for(unsigned e=5;e-->0;){r->index=e;if(!transition(r,io,e,false,x))goto done;}
 r->stage=4;
 if(!sample(r,io,final_sample,0,x))goto done;
 for(unsigned e=0;e<6;e++)if((r->words[0][e]&0xffffu)!=(r->words[final_sample][e]&0xffffu)){fail(r,IEP_STATE);goto done;}
 if(!guard(r,io,x))goto done;
 r->stage=5;
 bool preserved=io.preserve(io.ctx,t->census);
 if(!guard(r,io,x))goto done;
 if(!preserved){fail(r,IEP_PRESERVE);goto done;}
 r->preserved=true;r->uncertain=false;r->stage=6;
done:
 if(!r->uncertain && parent_mode(r,x))r->returned=intel_gt_access_return(&r->borrow);
 if(!r->returned)r->reason|=IEP_RETURN;
 r->end_us=io.now(io.ctx);
 if(!r->end_us || r->end_us<r->last_us || r->end_us<=r->begin_us ||
    r->end_us-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US){r->clock_at=r->end_us;r->clock_prev=r->last_us;r->reason|=IEP_CLOCK;}
 if(r->returned && (g->object!=g || g->state!=GT_ACCESS_AWAKE || !g->pinned || !epoch_matches(r,x) ||
    !display_access_held(&g->exclusion) || g->borrows || g->dependents || !prerequisite(r)))r->reason|=IEP_PARENT;
 r->complete=!r->reason && r->admitted && r->held && r->ready==31 && r->cancelled==31 &&
  r->writes==10 && r->attempts==10 && r->acks==10 && r->snapshots==4+(unsigned)(x!=0) && r->preserved && r->returned &&
  (!action || (r->action_attempted && r->action_completed));
 return r->complete;
}

bool intel_engine_prepare(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io) {
 return prepare(r,g,t,policy,io,0,0,0);
}
bool intel_engine_prepare_with_action(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io,
 struct intel_engine_prepare_action action) {
 return prepare(r,g,t,policy,io,&action,0,0);
}

#ifdef SRVROS_INTEL_PREPARE_RESET
bool intel_engine_prepare_with_bcs_reset(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io,
 struct intel_engine_prepare_action action,struct intel_engine_held_reset_result *x,struct intel_engine_held_reset_io reset_io) {
 if(!r || r->object || !x || x->object)return false;
 *x=(struct intel_engine_held_reset_result){.object=x,.preparation=r,.parent=g,
  .epoch_before=g?g->engine_epoch:0,.epoch_after=g?g->engine_epoch:0};
 if(!action.observe || !reset_io.guard || !reset_io.snapshot || !reset_io.domain || !reset_io.transition ||
    !reset_io.begin || !reset_io.end || !reset_io.now || !reset_io.pause || !g || g->engine_epoch==UINT32_MAX) {
  *r=(struct intel_engine_prepare_result){.object=r,.parent=g,.preflight=t,.reset=x,.reason=IEP_INPUT};
  x->reason=IHR_INPUT;return false;
 }
 return prepare(r,g,t,policy,io,&action,x,&reset_io);
}
#endif
