#include <srvros/intel_reset_ready.h>
static bool fail(struct intel_reset_ready_result *r,unsigned reason) {
 if(!r->reason)r->reason=reason;
 return false;
}
static bool guard(struct intel_reset_ready_result *r,struct intel_reset_ready_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>INTEL_RESET_READY_TOTAL_US)return fail(r,IRR_CLOCK);
 r->last_us=t;
 struct intel_gt_access *g=r->borrow.parent;
 if(!r->borrow.active || r->borrow.object!=&r->borrow || !g || g->object!=g ||
    g->state!=GT_ACCESS_AWAKE || !g->pinned || !g->borrows ||
    !display_access_held(&g->exclusion) || !io.guard(io.ctx))return fail(r,IRR_GUARD);
 return true;
}
static bool control(struct intel_reset_ready_result *r,struct intel_reset_ready_io io,uint32_t *v) {
 if(!guard(r,io))return false;
 if(!io.control(io.ctx,v))return fail(r,IRR_READ);
 ++r->reads;
 /* Mask upper16 is observational only; low bits15:2 MBZ on BDW. */
 if(*v==UINT32_MAX || (*v&0xfffcu))return fail(r,IRR_READ);
 return guard(r,io);
}
static bool group(struct intel_reset_ready_result *r,struct intel_reset_ready_io io,unsigned n) {
 if(!guard(r,io) || !io.group(io.ctx,r->words[n]))return fail(r,IRR_READ);
 r->reads+=8;++r->samples;
 const uint32_t *w=r->words[n];
 if(w[0] || w[1] || w[2] || w[3] || w[4]!=1 || w[5] || w[6]!=512 || w[7])return fail(r,IRR_ENTRY);
 if(n)for(unsigned i=0;i<8;i++)if(w[i]!=r->words[0][i])return fail(r,IRR_DRIFT);
 return guard(r,io);
}
static bool store(struct intel_reset_ready_result *r,struct intel_reset_ready_io io,uint32_t value) {
 if(!guard(r,io))return false;
 ++r->attempts;enum intel_gt_write w=io.write(io.ctx,value);
 if(w!=GT_WRITE_NONE)r->uncertain=true;
 if(w!=GT_WRITE_DONE)return fail(r,IRR_WRITE);
 ++r->writes;return guard(r,io);
}
static bool wait_control(struct intel_reset_ready_result *r,struct intel_reset_ready_io io,unsigned n,uint32_t want) {
 uint64_t phase=r->last_us;
 for(unsigned i=0;i<INTEL_RESET_READY_POLLS;i++) {
  uint32_t v=0;if(!control(r,io,&v))return false;++r->polls;
  if(r->last_us<phase || r->last_us-phase>INTEL_RESET_READY_PHASE_US)return fail(r,IRR_TIMEOUT);
  if((v&3)==want){r->controls[n]=v;++r->controls_read;return true;}
  /* Request must remain ours until explicit cancellation; no foreign takeover. */
  if((want==3 && !(v&1)) || (want==0 && (v&1)))return fail(r,IRR_DRIFT);
  io.pause(io.ctx);
 }
 return fail(r,IRR_TIMEOUT);
}
bool intel_reset_ready(struct intel_reset_ready_result *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_reset_ready_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_reset_ready_result){.object=r};
 if(!g || !c || !policy || !io.guard || !io.control || !io.group || !io.write || !io.now || !io.pause ||
    c->object!=c || c->parent!=g || !intel_gt_access_check(g) || !c->complete || !c->stable || c->enabled || c->pending || c->flip_wait || c->tlb_pending || c->parser_idle!=3 ||
    c->groups!=10 || c->samples!=2 || c->contexts!=2 || c->reads!=92 || c->reason ||
    !c->begin_us || c->begin_us<=g->begin || c->end_us<=c->begin_us || c->end_us>g->last)
  return fail(r,IRR_INPUT);
 if(!intel_gt_access_borrow(g,&r->borrow))return fail(r,IRR_PARENT);
 r->begin_us=r->last_us=io.now(io.ctx);
 /* Exercise the lifetime: blocked close must not issue a requester store. */
 unsigned writes=g->writes;
 r->blocked_close=!intel_gt_access_close(g) && g->writes==writes && g->state==GT_ACCESS_AWAKE && g->pinned;
 if(!r->blocked_close){fail(r,IRR_PARENT);goto done;}
 if(!r->begin_us || !group(r,io,0) ||
    !control(r,io,&r->controls[0]))goto done;
 ++r->controls_read;
 if(r->controls[0]&3){fail(r,IRR_ENTRY);goto done;}
 for(unsigned i=0;i<6;i++)if(r->words[0][i]!=c->words[1][4][i]){fail(r,IRR_DRIFT);goto done;}
 if(r->words[0][6]!=c->context[1][3] || r->words[0][7]!=c->context[1][5]){fail(r,IRR_DRIFT);goto done;}
 r->admitted=true;
 if(!store(r,io,INTEL_BCS_RESET_REQUEST) || !wait_control(r,io,1,3))goto done;
 r->ready=true;
 if(!store(r,io,INTEL_BCS_RESET_CANCEL) || !wait_control(r,io,2,0))goto done;
 r->cancelled=true;
 if(!group(r,io,1) || !control(r,io,&r->controls[3]))goto done;
 ++r->controls_read;
 if((r->controls[3]&3) || r->controls[0]!=r->controls[3]){fail(r,IRR_DRIFT);goto done;}
 if(!guard(r,io) || !intel_gt_access_check(g) || !guard(r,io))goto done;
 r->end_us=io.now(io.ctx);
 if(r->end_us<=r->last_us || r->end_us-r->begin_us>INTEL_RESET_READY_TOTAL_US){fail(r,IRR_CLOCK);goto done;}
 r->uncertain=false;
done:
 if(!r->uncertain) {
  if(!intel_gt_access_return(&r->borrow)){r->uncertain=true;fail(r,IRR_PARENT);}
 }
 r->complete=r->admitted && r->ready && r->cancelled && !r->uncertain && !r->reason && r->samples==2 && r->controls_read==4 && r->writes==2;
 return r->complete;
}
