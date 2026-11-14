#include <srvros/intel_pci_held.h>
static bool fail(struct intel_pci_held_result *r,unsigned why){r->reason|=why;return false;}
static bool live(const struct intel_pci_held_result *r) {
 const struct intel_engine_prepare_result *p=r->preparation;
 return r->object==r && p && intel_engine_prepare_action_live(p) &&
  p->parent==r->parent && p->epoch==r->epoch && r->borrow==&p->borrow &&
  p->begin_us==r->origin_us;
}
static bool clock_ok(struct intel_pci_held_result *r,struct intel_pci_held_io io) {
 uint64_t at=io.now(io.ctx);
 if(!at || at<r->last_us || at<r->origin_us ||
    at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US) {
  if(!(r->reason&IPH_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  return fail(r,IPH_CLOCK);
 }
 r->last_us=at;return true;
}
static bool guard(struct intel_pci_held_result *r,struct intel_pci_held_io io) {
 if(!clock_ok(r,io))return false;
 if(!live(r))return fail(r,IPH_PARENT);
 bool ok=io.guard(io.ctx);
 if(!live(r))return fail(r,IPH_PARENT);
 if(!clock_ok(r,io))return false;
 if(!live(r))return fail(r,IPH_PARENT);
 return ok?true:fail(r,IPH_GUARD);
}
bool intel_pci_held_observe(struct intel_pci_held_result *r,
 const struct intel_engine_prepare_result *p,struct intel_pci_held_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_pci_held_result){.object=r,.preparation=p};
 if(!p || !io.guard || !io.read || !io.now)return fail(r,IPH_INPUT);
 r->parent=p->parent;r->borrow=&p->borrow;r->epoch=p->epoch;
 r->origin_us=p->begin_us;r->last_us=p->last_us;
 if(!live(r))return fail(r,IPH_PARENT);
 if(!clock_ok(r,io))return false;
 r->begin_us=r->last_us;
 for(unsigned n=0;n<2;n++) {
  for(unsigned i=0;i<INTEL_PCI_REQUESTER_WORDS;i++) {
   if(!intel_pci_requester_prefix(r->words[n],r->counts[n])){fail(r,IPH_PROFILE);goto done;}
   if(!guard(r,io))goto done;
   ++r->attempts;
   bool read=io.read(io.ctx,intel_pci_requester_offsets[i],&r->words[n][i]);
   if(read){++r->counts[n];++r->reads;if(r->counts[n]==INTEL_PCI_REQUESTER_WORDS)++r->samples;}
   bool profiled=read && intel_pci_requester_prefix(r->words[n],r->counts[n]);
   bool guarded=guard(r,io);
   if(!read)fail(r,IPH_READ);
   else if(!profiled)fail(r,IPH_PROFILE);
   if(!guarded || !profiled)goto done;
  }
  ++r->profiles;
  r->bus_master|=((r->words[n][1]>>2)&1)<<n;
  r->pending|=((r->words[n][8]>>8)&1)<<n;
  r->flr|=(r->words[n][8]&1)<<n;
 }
 r->stable=true;
 for(unsigned i=0;i<INTEL_PCI_REQUESTER_WORDS;i++)if(r->words[0][i]!=r->words[1][i])r->stable=false;
done:
 /* Never create/return a dependency or cancel a request. A failed observation
  * is handed back to prepare, whose prior writes keep uncertainty pinned. */
 (void)clock_ok(r,io);
 r->end_us=r->last_us;
 if(!live(r))fail(r,IPH_PARENT);
 if(r->end_us<=r->begin_us) {
  if(!(r->reason&IPH_CLOCK)){r->clock_at=r->end_us;r->clock_prev=r->begin_us;}
  fail(r,IPH_CLOCK);
 }
 r->complete=!r->reason && r->attempts==18 && r->reads==18 && r->samples==2 && r->profiles==2;
 return r->complete;
}
