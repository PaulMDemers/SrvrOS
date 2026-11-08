#include <srvros/intel_bcs_reset.h>
static bool fail(struct intel_bcs_reset_result *r,unsigned reason) {
 if(!r->reason)r->reason=reason;
 return false;
}
static bool guard(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>INTEL_RESET_READY_TOTAL_US)return fail(r,IBR_CLOCK);
 r->last_us=t;
 struct intel_gt_access *g=r->borrow.parent;
 if(!r->borrow.active || r->borrow.object!=&r->borrow || !g || g->object!=g ||
    g->state!=GT_ACCESS_AWAKE || !g->pinned || !g->borrows ||
    g->engine_epoch!=r->epoch_after || !display_access_held(&g->exclusion) ||
    !io.guard(io.ctx) || g->engine_epoch!=r->epoch_after)return fail(r,IBR_GUARD);
 return true;
}
static bool read_control(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io,uint32_t *v) {
 if(!guard(r,io))return false;
 if(!io.control(io.ctx,v))return fail(r,IBR_READ);
 ++r->reads;
 if(*v==UINT32_MAX || (*v&0xfffcu))return fail(r,IBR_READ);
 return guard(r,io);
}
static bool read_domain(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io,uint32_t *v) {
 if(!guard(r,io))return false;
 if(!io.domain(io.ctx,v))return fail(r,IBR_READ);
 ++r->reads;
 /* A foreign domain reset is loss of admission, not a bit to clear. */
 if(*v&~INTEL_BCS_RESET_DOMAIN)return fail(r,IBR_DRIFT);
 return guard(r,io);
}
static bool group(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io,unsigned n) {
 if(!guard(r,io) || !io.group(io.ctx,r->words[n]))return fail(r,IBR_READ);
 r->reads+=8;++r->samples;
 const uint32_t *w=r->words[n];
 if(w[0] || w[1] || w[2] || w[3] || w[4]!=1 || w[5] || w[6]!=512 || w[7])return fail(r,IBR_ENTRY);
 if(n)for(unsigned i=0;i<8;i++)if(w[i]!=r->words[0][i])return fail(r,IBR_DRIFT);
 return guard(r,io);
}
static bool control_store(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io,uint32_t value) {
 if(!guard(r,io))return false;
 ++r->attempts;
 enum intel_gt_write w=io.control_write(io.ctx,value);
 if(w!=GT_WRITE_NONE)r->uncertain=true;
 if(w!=GT_WRITE_DONE)return fail(r,IBR_WRITE);
 ++r->writes;return guard(r,io);
}
static bool wait_control(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io,unsigned n,uint32_t want) {
 uint64_t phase=r->last_us;
 for(unsigned i=0;i<INTEL_RESET_READY_POLLS;i++) {
  uint32_t v=0;if(!read_control(r,io,&v))return false;++r->polls;
  if(r->last_us-phase>INTEL_RESET_READY_PHASE_US)return fail(r,IBR_TIMEOUT);
  if((v&3)==want){r->controls[n]=v;++r->controls_read;return true;}
  if((want==3 && !(v&1)) || (want==0 && (v&1)))return fail(r,IBR_DRIFT);
  io.pause(io.ctx);
 }
 return fail(r,IBR_TIMEOUT);
}
static bool pulse(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io,unsigned n) {
 if(!guard(r,io))return false;
 /* Invalidate every completion census on this owner before possible issue.
  * Even a definitely-unissued callback conservatively consumes the epoch. */
 if(!n)r->borrow.parent->engine_epoch=++r->epoch_after;
 ++r->reset_attempts;
 enum intel_gt_write w=io.domain_write(io.ctx,INTEL_BCS_RESET_DOMAIN);
 if(w!=GT_WRITE_NONE)r->uncertain=true;
 if(w!=GT_WRITE_DONE)return fail(r,IBR_WRITE);
 ++r->reset_writes;
 uint64_t phase=r->last_us;
 for(unsigned i=0;i<INTEL_RESET_READY_POLLS;i++) {
  uint32_t v=0;if(!read_domain(r,io,&v))return false;++r->polls;
  if(r->last_us-phase>INTEL_RESET_READY_PHASE_US)return fail(r,IBR_TIMEOUT);
  if(!v){r->domains[n+1]=v;++r->domains_read;++r->reset_acks;return true;}
  io.pause(io.ctx);
 }
 return fail(r,IBR_TIMEOUT);
}
static bool settle(struct intel_bcs_reset_result *r,struct intel_bcs_reset_io io) {
 r->settle_begin_us=r->last_us;
 for(unsigned i=0;i<INTEL_RESET_READY_POLLS;i++) {
  io.pause(io.ctx);
  if(!guard(r,io))return false;
  if(r->last_us-r->settle_begin_us>INTEL_RESET_READY_PHASE_US)return fail(r,IBR_TIMEOUT);
  if(r->last_us-r->settle_begin_us>=INTEL_BCS_RESET_SETTLE_US){r->settle_end_us=r->last_us;return true;}
 }
 return fail(r,IBR_TIMEOUT);
}
struct shutdown_reset_io {struct intel_bcs_reset_result *r;struct intel_bcs_reset_io io;};
static bool shutdown_reset_guard(void *ctx) {
 struct shutdown_reset_io *s=ctx;return guard(s->r,s->io);
}
static bool shutdown_reset_read(void *ctx,uint32_t out[8]) {
 struct shutdown_reset_io *s=ctx;return guard(s->r,s->io) && s->io.group(s->io.ctx,out) && guard(s->r,s->io);
}
static uint64_t shutdown_reset_now(void *ctx) {
 struct shutdown_reset_io *s=ctx;return s->io.now(s->io.ctx);
}
static bool reset_run(struct intel_bcs_reset_result *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_bcs_reset_io io,bool qualify_shutdown) {
 if(!r || r->object)return false;
 *r=(struct intel_bcs_reset_result){.object=r};
 if(!g || !c || !policy || !io.guard || !io.control || !io.domain || !io.group || !io.control_write || !io.domain_write || !io.now || !io.pause ||
    c->object!=c || c->parent!=g || c->engine_epoch!=g->engine_epoch || g->engine_epoch==UINT32_MAX || g->borrows || g->dependents ||
    !intel_gt_access_check(g) || c->engine_epoch!=g->engine_epoch || !c->complete || !c->stable || c->enabled || c->pending || c->flip_wait || c->tlb_pending || c->parser_idle!=3 ||
    c->groups!=10 || c->samples!=2 || c->contexts!=2 || c->reads!=92 || c->reason ||
    !c->begin_us || c->begin_us<=g->begin || c->end_us<=c->begin_us || c->end_us>g->last)
  return fail(r,IBR_INPUT);
 r->epoch_before=r->epoch_after=g->engine_epoch;
 if(!intel_gt_access_borrow(g,&r->borrow))return fail(r,IBR_PARENT);
 r->begin_us=r->last_us=io.now(io.ctx);
 unsigned writes=g->writes;
 r->blocked_close=!intel_gt_access_close(g) && g->writes==writes && g->state==GT_ACCESS_AWAKE && g->pinned;
 if(!r->blocked_close){fail(r,IBR_PARENT);goto done;}
 if(qualify_shutdown) {
  struct shutdown_reset_io ctx={r,io};
  if(!intel_blitter_observe_disabled(&r->shutdown,g,policy,(struct intel_blitter_io){&ctx,shutdown_reset_guard,shutdown_reset_read,0,shutdown_reset_now})) {fail(r,IBR_ENTRY);goto done;}
  /* No copied receipt: object and parent are sealed by the current reset.
   * Exact raw binding rejects upper mode bits and every nonzero geometry. */
  if(r->shutdown.object!=&r->shutdown || !r->shutdown.entry_disabled || r->shutdown.attempts || r->shutdown.writes ||
     r->shutdown.samples!=4 || r->shutdown.reads!=32 || g->engine_epoch!=r->epoch_before || !guard(r,io)) {fail(r,IBR_ENTRY);goto done;}
  for(unsigned n=0;n<4;n++)for(unsigned i=0;i<8;i++) {
   uint32_t expected=i<6?c->words[1][4][i]:c->context[1][i==6?3:5];
   if(r->shutdown.words[n][i]!=expected){fail(r,IBR_DRIFT);goto done;}
  }
 }
 if(!r->begin_us || !group(r,io,0) || !read_control(r,io,&r->controls[0]))goto done;
 ++r->controls_read;
 if(!read_domain(r,io,&r->domains[0]))goto done;
 ++r->domains_read;
 if((r->controls[0]&3) || r->domains[0]){fail(r,IBR_ENTRY);goto done;}
 for(unsigned i=0;i<6;i++)if(r->words[0][i]!=c->words[1][4][i]){fail(r,IBR_DRIFT);goto done;}
 if(r->words[0][6]!=c->context[1][3] || r->words[0][7]!=c->context[1][5]){fail(r,IBR_DRIFT);goto done;}
 r->admitted=true;
 if(!control_store(r,io,INTEL_BCS_RESET_REQUEST) || !wait_control(r,io,1,3))goto done;
 r->ready=true;
 if(!pulse(r,io,0) || !pulse(r,io,1) || !settle(r,io))goto done;
 if(!control_store(r,io,INTEL_BCS_RESET_CANCEL) || !wait_control(r,io,2,0))goto done;
 r->cancelled=true;
 if(!group(r,io,1) || !read_control(r,io,&r->controls[3]))goto done;
 ++r->controls_read;
 if(!read_domain(r,io,&r->domains[3]))goto done;
 ++r->domains_read;
 if((r->controls[3]&3) || r->domains[3]){fail(r,IBR_DRIFT);goto done;}
 if(!guard(r,io) || !intel_gt_access_check(g) || !guard(r,io))goto done;
 r->end_us=io.now(io.ctx);
 if(r->end_us<=r->last_us || r->end_us-r->begin_us>INTEL_RESET_READY_TOTAL_US){fail(r,IBR_CLOCK);goto done;}
 r->uncertain=false;
done:
 if(!r->uncertain) {
  if(!intel_gt_access_return(&r->borrow)){r->uncertain=true;fail(r,IBR_PARENT);}
  else if(r->end_us) {
   uint64_t finished=io.now(io.ctx);
   if(!finished || finished<=r->end_us || finished<r->last_us || finished-r->begin_us>INTEL_RESET_READY_TOTAL_US)fail(r,IBR_CLOCK);
   r->end_us=r->last_us=finished;
  }
 }
 r->complete=r->admitted && r->ready && r->cancelled && !r->uncertain && !r->reason &&
  r->samples==2 && r->controls_read==4 && r->domains_read==4 && r->writes==2 && r->reset_writes==2 && r->reset_acks==2;
 return r->complete;
}

bool intel_bcs_reset(struct intel_bcs_reset_result *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_bcs_reset_io io) {
 return reset_run(r,g,c,policy,io,false);
}
bool intel_bcs_reset_after_shutdown(struct intel_bcs_reset_result *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_bcs_reset_io io) {
 return reset_run(r,g,c,policy,io,true);
}
