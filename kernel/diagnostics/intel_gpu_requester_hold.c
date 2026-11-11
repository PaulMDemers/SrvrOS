#include <srvros/intel_gpu_requester_hold.h>
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool fail(struct intel_gpu_requester_hold *r,unsigned why) {
 r->reason|=why;r->state=IGH_FAULT;
 if(r->borrow.active)r->borrow.fault=true;
 return false;
}
bool intel_gpu_requester_hold_live(const struct intel_gpu_requester_hold *r) {
 if(!r || r->object!=r || r->reason || r->state<IGH_OPENING || r->state>IGH_RESTORING ||
    !r->claim || r->claim->object!=r->claim || r->borrow.claim!=r->claim ||
    r->claim->origin_us!=r->origin_us || r->claim->owner_origin_us!=r->owner_origin_us)return false;
 /* Pure source identity also works inside the original claim's callback.
  * It never substitutes for borrow_check's complete source/hardware guard. */
 return intel_interval_borrow_live(&r->borrow);
}
bool intel_gpu_requester_hold_time(const struct intel_gpu_requester_hold *r,uint64_t at) {
 return intel_gpu_requester_hold_live(r) && r->origin_us && r->owner_origin_us &&
  at && at>=r->last_us && at>=r->claim->last_us && at>=r->origin_us &&
  at-r->origin_us<=INTEL_ENGINE_PREPARE_TOTAL_US && at>=r->owner_origin_us &&
  at-r->owner_origin_us<=GT_ACCESS_TOTAL_US;
}
bool intel_gpu_requester_hold_deadline(struct intel_gpu_requester_hold *r,uint64_t at) {
 if(!r || r->object!=r || r->reason)return false;
 if(!intel_gpu_requester_hold_live(r))return fail(r,IGH_SOURCE);
 if(!intel_gpu_requester_hold_time(r,at)) {
  if(!(r->reason&IGH_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  (void)intel_interval_borrow_deadline(&r->borrow,at);
  return fail(r,IGH_CLOCK);
 }
 if(!intel_interval_borrow_deadline(&r->borrow,at))return fail(r,IGH_SOURCE);
 r->last_us=at;
 return true;
}
static bool tick(struct intel_gpu_requester_hold *r) {
 if(!intel_gpu_requester_hold_live(r))return fail(r,IGH_SOURCE);
 uint64_t at=r->io.now(r->io.ctx);
 return intel_gpu_requester_hold_deadline(r,at);
}
static bool guard(struct intel_gpu_requester_hold *r) {
 if(!intel_gpu_requester_hold_live(r))return fail(r,IGH_SOURCE);
 if(!intel_interval_borrow_check(&r->borrow))return fail(r,IGH_SOURCE);
 return tick(r);
}
static bool profile(struct intel_gpu_requester_hold *r,unsigned n) {
 const uint32_t *w=r->words[n];
 if(!intel_pci_requester_prefix(w,INTEL_PCI_REQUESTER_WORDS) || (w[1]&65535)!=(n==1?3u:7u))return false;
 if(n)for(unsigned i=0;i<8;i++) {
  uint32_t mask=i==1?~4u:UINT32_MAX;
  if((w[i]&mask)!=(r->words[0][i]&mask))return false;
 }
 return n!=2 || !w[8];
}
static bool sample(struct intel_gpu_requester_hold *r,unsigned n) {
 r->phase=n;
 if(!guard(r))return false;
 r->sample_begin[n]=r->last_us;
 bool ok=r->io.sample(r->io.ctx,r->words[n],&r->counts[n]);
 if(r->counts[n]>INTEL_PCI_REQUESTER_WORDS){r->counts[n]=0;return fail(r,IGH_READ);}
 r->reads+=r->counts[n];if(r->counts[n]==INTEL_PCI_REQUESTER_WORDS)++r->samples;
 if(!tick(r))return false;
 r->sample_end[n]=r->last_us;
 if(!ok || r->counts[n]!=INTEL_PCI_REQUESTER_WORDS)return fail(r,IGH_READ);
 return profile(r,n) || fail(r,IGH_PROFILE);
}
static bool command(struct intel_gpu_requester_hold *r,uint16_t value) {
 if(!guard(r))return false;
 ++r->attempts;
 enum intel_gt_write result=r->io.command16(r->io.ctx,value);
 if(result==GT_WRITE_DONE)++r->writes;
 /* Never replay or restore after a possibly issued/failed command. */
 if(result!=GT_WRITE_DONE)return fail(r,IGH_WRITE);
 return tick(r);
}
bool intel_gpu_requester_hold_run(struct intel_gpu_requester_hold *r,struct intel_interval_claim *c,struct intel_gpu_hold_io io) {
 if(!r || r->object || !c || c->object!=c || !io.sample || !io.pending || !io.command16 || !io.now || !io.pause ||
    overlap(r,sizeof(*r),c,sizeof(*c)) || !c->child || !c->rehold || !c->reset || !c->parent ||
    overlap(r,sizeof(*r),c->child,sizeof(*c->child)) || overlap(r,sizeof(*r),c->rehold,sizeof(*c->rehold)) ||
    overlap(r,sizeof(*r),c->reset,sizeof(*c->reset)) || overlap(r,sizeof(*r),c->rehold->preparation,sizeof(*c->rehold->preparation)) ||
    overlap(r,sizeof(*r),c->parent,sizeof(*c->parent)))return false;
 *r=(struct intel_gpu_requester_hold){.object=r,.claim=c,.io=io,.state=IGH_OPENING,.busy=true,
  .origin_us=c->origin_us,.owner_origin_us=c->owner_origin_us,.last_us=c->last_us};
 if(!intel_interval_borrow_open(c,&r->borrow))return fail(r,IGH_SOURCE);
 r->last_us=c->last_us;r->begin_us=r->last_us;
 if(!sample(r,0))goto done;
 r->state=IGH_INHIBITING;
 if(!command(r,3) || !sample(r,1))goto done;
 r->state=IGH_INHIBITED;r->drain_begin_us=r->last_us;
 uint32_t pending=r->words[1][8];
 while(pending) {
  if(r->polls==INTEL_GPU_HOLD_POLLS){fail(r,IGH_PENDING);goto done;}
  if(!guard(r))goto done;
  if(r->last_us<r->drain_begin_us || r->last_us-r->drain_begin_us>INTEL_GPU_HOLD_PHASE_US){fail(r,IGH_PENDING);goto done;}
  unsigned p=r->polls++;
  bool ok=io.pending(io.ctx,r->poll_words[p],&r->poll_counts[p]);
  if(r->poll_counts[p]>2){r->poll_counts[p]=0;fail(r,IGH_READ);goto done;}
  r->reads+=r->poll_counts[p];
  if(!tick(r))goto done;
  r->poll_us[p]=r->last_us;
  if(r->last_us-r->drain_begin_us>INTEL_GPU_HOLD_PHASE_US){fail(r,IGH_PENDING);goto done;}
  if(!ok || r->poll_counts[p]!=2){fail(r,IGH_READ);goto done;}
  if(r->poll_words[p][0]!=r->words[1][1] || (r->poll_words[p][1]&~0x100u)){fail(r,IGH_PROFILE);goto done;}
  pending=r->poll_words[p][1];
  if(pending) {io.pause(io.ctx);if(!tick(r))goto done;}
 }
 r->state=IGH_RESTORING;
 if(!command(r,7) || !sample(r,2))goto done;
 r->restored=true;
 if(!intel_interval_borrow_return(&r->borrow)){fail(r,IGH_RETURN);goto done;}
 r->last_us=c->last_us;r->returned=true;r->state=IGH_CLOSED;r->complete=true;
done:
 r->end_us=r->last_us;r->busy=false;return r->complete;
}
