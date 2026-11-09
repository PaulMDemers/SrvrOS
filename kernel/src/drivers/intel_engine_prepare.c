#include <srvros/intel_engine_prepare.h>
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
static bool parent(struct intel_engine_prepare_result *r) {
 struct intel_gt_access *g=r->parent;
 return r->object==r && g && g->object==g && g->engine_epoch==r->epoch &&
  g->state==GT_ACCESS_AWAKE && g->pinned && display_access_held(&g->exclusion) &&
  prerequisite(r) && r->borrow.object==&r->borrow && r->borrow.parent==g &&
  r->borrow.active && !r->borrow.next && g->borrows==1 && g->dependents==&r->borrow;
}
static bool clock_ok(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US) {
  r->clock_at=t;r->clock_prev=r->last_us;return fail(r,IEP_CLOCK);
 }
 r->last_us=t;return true;
}
static bool guard(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io) {
 if(!clock_ok(r,io))return false;
 if(!parent(r))return fail(r,IEP_PARENT);
 bool ok=io.guard(io.ctx);
 if(!parent(r))return fail(r,IEP_PARENT);
 if(!clock_ok(r,io))return false;
 if(!parent(r))return fail(r,IEP_PARENT);
 return ok?true:fail(r,IEP_GUARD);
}
static bool valid_control(uint32_t v) {return v!=UINT32_MAX && !(v&0xfffcu);}
static bool sample(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io,unsigned n,unsigned low) {
 if(!guard(r,io))return false;
 r->sample_attempts++;
 bool ok=io.sample(io.ctx,r->words[n],&r->counts[n]);
 if(r->counts[n]>6){r->counts[n]=0;return fail(r,IEP_COUNT);}
 r->reads+=r->counts[n];if(r->counts[n]==6)r->snapshots++;
 if(!guard(r,io))return false;
 if(!ok || r->counts[n]!=6)return fail(r,IEP_READ);
 for(unsigned e=0;e<5;e++)if(!valid_control(r->words[n][e]) || (r->words[n][e]&3)!=low)return fail(r,IEP_STATE);
 if(r->words[n][5])return fail(r,IEP_STATE);
 return true;
}
static bool transition(struct intel_engine_prepare_result *r,struct intel_engine_prepare_io io,unsigned e,bool request) {
 if(!guard(r,io))return false;
 uint64_t phase=r->last_us;
 unsigned n=r->acks;uint32_t ack=UINT32_MAX;
 ++r->attempts;
 enum intel_gt_write wrote=io.transition(io.ctx,e,request?INTEL_ENGINE_PREPARE_REQUEST:INTEL_ENGINE_PREPARE_CANCEL,&ack);
 if(wrote!=GT_WRITE_NONE)r->uncertain=true;
 if(wrote!=GT_WRITE_DONE)return fail(r,IEP_WRITE);
 ++r->writes;++r->reads;r->ack[n]=r->final[n]=ack;++r->acks;
 if(!guard(r,io))return false;
 if(r->last_us<phase || r->last_us-phase>INTEL_ENGINE_PREPARE_PHASE_US){r->clock_at=r->last_us;r->clock_prev=phase;return fail(r,IEP_TIMEOUT);}
 unsigned want=request?3:0;
 for(unsigned i=0;i<=INTEL_ENGINE_PREPARE_POLLS;i++) {
  if(!valid_control(ack) || (ack&1)!=(unsigned)request)return fail(r,IEP_STATE);
  if((ack&3)==want){if(request)r->ready|=1u<<e;else r->cancelled|=1u<<e;return true;}
  if(i==INTEL_ENGINE_PREPARE_POLLS)return fail(r,IEP_TIMEOUT);
  io.pause(io.ctx);
  if(!guard(r,io))return false;
  bool ok=io.control(io.ctx,e,&ack);
  if(ok){++r->reads;++r->polls;++r->phase_polls[n];r->final[n]=ack;}
  if(!guard(r,io))return false;
  if(!ok)return fail(r,IEP_READ);
  if(r->last_us<phase || r->last_us-phase>INTEL_ENGINE_PREPARE_PHASE_US){r->clock_at=r->last_us;r->clock_prev=phase;return fail(r,IEP_TIMEOUT);}
 }
 return false;
}
static bool prepare(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io,
 const struct intel_engine_prepare_action *action) {
 if(!r || r->object)return false;
 *r=(struct intel_engine_prepare_result){.object=r,.parent=g,.preflight=t};
 if(!g || !policy || !io.guard || !io.sample || !io.control || !io.transition || !io.preserve || !io.now || !io.pause || (action && !action->observe))return fail(r,IEP_INPUT);
 r->epoch=g->engine_epoch;
 if(!prerequisite(r) || g->borrows || g->dependents || !intel_gt_access_check(g))return fail(r,IEP_INPUT);
 r->begin_us=r->last_us=io.now(io.ctx);
 if(!r->begin_us || r->begin_us<=t->end_us || r->begin_us-t->end_us>100000)return fail(r,IEP_CLOCK);
 if(!intel_gt_access_borrow(g,&r->borrow))return fail(r,IEP_PARENT);
 unsigned writes=g->write_attempts;
 r->blocked=!intel_gt_access_close(g) && g->write_attempts==writes;
 if(!r->blocked || !parent(r)){fail(r,IEP_PARENT);goto done;}
 if(!sample(r,io,0,0))goto done;
 for(unsigned i=0;i<6;i++)if(r->words[0][i]!=t->words[1][i]){fail(r,IEP_STATE);goto done;}
 r->admitted=true;
 r->stage=1;for(unsigned e=0;e<5;e++){r->index=e;if(!transition(r,io,e,true))goto done;}
 r->stage=2;
 if(!sample(r,io,1,3))goto done;
 if(action) {
  if(r->ready!=31 || !guard(r,io)){fail(r,IEP_ACTION);goto done;}
  r->action_begin_us=r->last_us;r->action_attempted=true;
  bool observed=action->observe(action->ctx,r);
  /* Earlier request stores already set uncertainty. Neither callback failure
   * nor a lost parent/clock admits cancellation or retirement through it. */
  bool guarded=guard(r,io);
  r->action_end_us=r->last_us;
  if(!observed || !guarded){fail(r,IEP_ACTION);goto done;}
 }
 if(!sample(r,io,2,3))goto done;
 for(unsigned e=0;e<6;e++)if(r->words[1][e]!=r->words[2][e]){fail(r,IEP_STATE);goto done;}
 if(action)r->action_completed=true;
 r->held=true;
 r->stage=3;for(unsigned e=5;e-->0;){r->index=e;if(!transition(r,io,e,false))goto done;}
 r->stage=4;
 if(!sample(r,io,3,0))goto done;
 for(unsigned e=0;e<6;e++)if((r->words[0][e]&0xffffu)!=(r->words[3][e]&0xffffu)){fail(r,IEP_STATE);goto done;}
 if(!guard(r,io))goto done;
 r->stage=5;
 bool preserved=io.preserve(io.ctx,t->census);
 if(!guard(r,io))goto done;
 if(!preserved){fail(r,IEP_PRESERVE);goto done;}
 r->preserved=true;r->uncertain=false;r->stage=6;
done:
 if(!r->uncertain && parent(r))r->returned=intel_gt_access_return(&r->borrow);
 if(!r->returned)r->reason|=IEP_RETURN;
 r->end_us=io.now(io.ctx);
 if(!r->end_us || r->end_us<r->last_us || r->end_us<=r->begin_us ||
    r->end_us-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US){r->clock_at=r->end_us;r->clock_prev=r->last_us;r->reason|=IEP_CLOCK;}
 if(r->returned && (g->object!=g || g->state!=GT_ACCESS_AWAKE || !g->pinned || g->engine_epoch!=r->epoch ||
    !display_access_held(&g->exclusion) || g->borrows || g->dependents || !prerequisite(r)))r->reason|=IEP_PARENT;
 r->complete=!r->reason && r->admitted && r->held && r->ready==31 && r->cancelled==31 &&
  r->writes==10 && r->attempts==10 && r->acks==10 && r->snapshots==4 && r->preserved && r->returned &&
  (!action || (r->action_attempted && r->action_completed));
 return r->complete;
}

bool intel_engine_prepare(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io) {
 return prepare(r,g,t,policy,io,0);
}
bool intel_engine_prepare_with_action(struct intel_engine_prepare_result *r,struct intel_gt_access *g,
 const struct intel_takeover_result *t,bool policy,struct intel_engine_prepare_io io,
 struct intel_engine_prepare_action action) {
 return prepare(r,g,t,policy,io,&action);
}
