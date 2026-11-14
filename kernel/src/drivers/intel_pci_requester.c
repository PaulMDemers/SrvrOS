#include <srvros/intel_pci_requester.h>
const uint32_t intel_pci_requester_offsets[INTEL_PCI_REQUESTER_WORDS]={0,4,8,0x2c,0x34,0x90,0xd0,0xa4,0xa8};
static bool fail(struct intel_pci_requester_result *r,unsigned why){r->reason|=why;return false;}
static bool prerequisite(const struct intel_pci_requester_result *r) {
 const struct intel_takeover_result *t=r->preflight;
 const struct intel_completion_result *c=t?t->census:0;
 return t && t->object==t && t->parent==r->parent && t->epoch==r->epoch &&
  t->complete && t->stable && !t->reason && t->returned &&
  t->borrow.object==&t->borrow && t->borrow.parent==r->parent && !t->borrow.active &&
  t->attempts==2 && t->samples==2 && t->reads==12 && t->counts[0]==6 && t->counts[1]==6 &&
  t->begin_us && t->end_us>t->begin_us && t->end_us-t->begin_us<=100000 &&
  c && c->object==c && c->parent==r->parent && c->engine_epoch==r->epoch &&
  c->complete && c->stable && !c->reason && c->batches==2 && c->samples==2 &&
  c->groups==10 && c->contexts==2 && c->reads==92 && c->begin_us &&
  c->end_us>c->begin_us && c->end_us-c->begin_us<=100000 &&
  t->begin_us>c->end_us && t->begin_us-c->end_us<=100000;
}
static bool parent(struct intel_pci_requester_result *r) {
 struct intel_gt_access *g=r->parent;
 return r->object==r && g && g->object==g && g->state==GT_ACCESS_AWAKE &&
  g->pinned && g->engine_epoch==r->epoch && display_access_held(&g->exclusion) &&
  prerequisite(r) && r->borrow.object==&r->borrow && r->borrow.parent==g &&
  r->borrow.active && !r->borrow.next && g->borrows==1 && g->dependents==&r->borrow;
}
static bool clock_ok(struct intel_pci_requester_result *r,struct intel_pci_requester_io io) {
 uint64_t at=io.now(io.ctx);
 if(!at || at<r->last_us || at<r->begin_us || at-r->begin_us>INTEL_PCI_REQUESTER_TOTAL_US) {
  if(!(r->reason&IPR_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  return fail(r,IPR_CLOCK);
 }
 r->last_us=at;return true;
}
static bool guard(struct intel_pci_requester_result *r,struct intel_pci_requester_io io) {
 if(!clock_ok(r,io))return false;
 if(!parent(r))return fail(r,IPR_PARENT);
 bool ok=io.guard(io.ctx);
 if(!parent(r))return fail(r,IPR_PARENT);
 if(!clock_ok(r,io))return false;
 if(!parent(r))return fail(r,IPR_PARENT);
 return ok?true:fail(r,IPR_GUARD);
}
bool intel_pci_requester_prefix(const uint32_t *w,unsigned n) {
 if(!w || n>INTEL_PCI_REQUESTER_WORDS)return false;
 for(unsigned i=0;i<n;i++)if(w[i]==UINT32_MAX)return false;
 /* Validate identity/header and every next-capability pointer BEFORE the next
  * fixed capability/header/payload read. Never read AF status through an
  * absent, malformed, cyclic or similarly named PCIe capability. */
 return (!n || w[0]==0x16268086u) &&
  (n<2 || ((w[1]&(1u<<20)) && (w[1]&2))) &&
  (n<3 || w[2]==0x03000009u) && (n<4 || w[3]==0x011b106bu) &&
  (n<5 || (w[4]&255)==0x90) && (n<6 || (w[5]&65535)==0xd005) &&
  (n<7 || ((w[6]&65535)==0xa401 && ((w[6]>>16)&7)==2)) &&
  (n<8 || w[7]==0x03060013u) && (n<9 || !(w[8]&~0x100u));
}
bool intel_pci_requester_observe(struct intel_pci_requester_result *r,
 struct intel_gt_access *g,const struct intel_takeover_result *t,struct intel_pci_requester_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_pci_requester_result){.object=r,.parent=g,.preflight=t};
 if(!g || !io.guard || !io.sample || !io.now)return fail(r,IPR_INPUT);
 r->epoch=g->engine_epoch;
 if(!prerequisite(r) || g->borrows || g->dependents || !intel_gt_access_check(g))return fail(r,IPR_INPUT);
 r->begin_us=r->last_us=io.now(io.ctx);
 if(!r->begin_us || r->begin_us<=t->end_us || r->begin_us-t->end_us>100000) {
  r->clock_at=r->begin_us;r->clock_prev=t->end_us;return fail(r,IPR_CLOCK);
 }
 if(!intel_gt_access_borrow(g,&r->borrow))return fail(r,IPR_PARENT);
 unsigned attempts=g->write_attempts;
 r->blocked=!intel_gt_access_close(g) && g->write_attempts==attempts;
 if(!r->blocked || !parent(r)){fail(r,IPR_PARENT);goto done;}
 for(unsigned n=0;n<2;n++) {
  if(!guard(r,io))goto done;
  ++r->attempts;
  bool ok=io.sample(io.ctx,r->words[n],&r->counts[n]);
  if(r->counts[n]>INTEL_PCI_REQUESTER_WORDS){r->counts[n]=0;fail(r,IPR_COUNT);goto done;}
  r->reads+=r->counts[n];if(r->counts[n]==INTEL_PCI_REQUESTER_WORDS)++r->samples;
  bool floating=false;for(unsigned i=0;i<r->counts[n];i++)if(r->words[n][i]==UINT32_MAX)floating=true;
  if(!ok || floating || r->counts[n]!=INTEL_PCI_REQUESTER_WORDS)fail(r,IPR_READ);
  if(!guard(r,io) || r->reason)goto done;
  if(!intel_pci_requester_prefix(r->words[n],INTEL_PCI_REQUESTER_WORDS)){fail(r,IPR_PROFILE);goto done;}
  ++r->profiles;
  r->bus_master|=((r->words[n][1]>>2)&1)<<n;
  r->pending|=((r->words[n][8]>>8)&1)<<n;
  r->flr|=(r->words[n][8]&1)<<n;
 }
 r->stable=true;
 for(unsigned i=0;i<INTEL_PCI_REQUESTER_WORDS;i++)if(r->words[0][i]!=r->words[1][i])r->stable=false;
done:
 /* RO failure can return only through the still-valid original parent.
  * Copied/stale/detached owner state never authorizes forced return. */
 if(parent(r))r->returned=intel_gt_access_return(&r->borrow);
 if(!r->returned)fail(r,IPR_RETURN);
 uint64_t end=io.now(io.ctx);
 if(!end || end<r->last_us || end<=r->begin_us || end-r->begin_us>INTEL_PCI_REQUESTER_TOTAL_US) {
  if(!(r->reason&IPR_CLOCK)){r->clock_at=end;r->clock_prev=r->last_us;}
  fail(r,IPR_CLOCK);
 }
 r->end_us=end;
 if(r->returned && (g->object!=g || g->state!=GT_ACCESS_AWAKE || !g->pinned ||
    g->engine_epoch!=r->epoch || !display_access_held(&g->exclusion) ||
    g->borrows || g->dependents || !prerequisite(r)))fail(r,IPR_PARENT);
 r->complete=!r->reason && r->attempts==2 && r->samples==2 && r->profiles==2 && r->reads==18 && r->returned;
 return r->complete;
}
