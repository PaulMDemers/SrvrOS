/* Isolated CPU image lifetime only: prospective addresses stay UNMAPPED. */
#include <srvros/intel_bdw_bcs_context.h>
#include <srvros/scanout_space.h>
#include "intel_alias_ram.inc"
static struct intel_alias_ram_catalog context_ram;
/* Slot zero in the observed window is the transaction target. The four
 * following slots can be reserved by software before any GGTT edit. */
static const struct scanout_space_range command_slots = { INTEL_WINDOW_FIRST + 1, 4 };
static struct {
 struct intel_bdw_bcs_context_config config;
 struct scanout_interval slots;
 uint64_t original[5];
 bool reserved, released;
 unsigned pte_reads, sealed, reopened, alias_checks, widi_unmapped, traffic_samples;
 uint32_t traffic[2][6];
 uint64_t physical,root,descriptor,begin,end,erase_begin,erase_end;
 unsigned initialized,verified,held,erased,fault;
} bcs_context_native;
static bool command_slot_vector(bool capture)
{
 const struct intel_window_result *w = &window_result;
 if (w->status != WINDOW_OBSERVED || w->retained || w->maps != 2 ||
     w->sample[0].entries != INTEL_WINDOW_ENTRIES || w->sample[1].entries != INTEL_WINDOW_ENTRIES ||
     w->endpoint.mmio != result.mmio_base || w->endpoint.fence_begin ||
     w->endpoint.fence_end != (uint64_t)INTEL_WINDOW_FIRST * 4096 ||
     w->aperture_bytes != 0x10000000) return false;
 for (unsigned i = 0; i <= command_slots.pages; ++i) {
  uint64_t entry = w->sample[0].entry[i], pa = entry & ~4095ull;
  if (!pa || (entry & 4095) != 1 || entry >> 39 || entry != w->sample[1].entry[i] ||
      (pa >= command_native.owner.physical && pa - command_native.owner.physical < BDW_BCS_IMAGE_BYTES))
   return false;
  for (unsigned j = 0; j < i; ++j)
   if (entry == w->sample[0].entry[j]) return false;
  if (!capture && entry != bcs_context_native.original[i]) return false;
 }
 if (capture)
  for (unsigned i = 0; i <= command_slots.pages; ++i)
   bcs_context_native.original[i] = w->sample[0].entry[i];
 return true;
}
static bool command_slots_held(void)
{
 return bcs_context_native.reserved && !bcs_context_native.released &&
        scanout_interval_held(&bcs_context_native.slots, result.mmio_base,
          window_result.aperture_base, window_result.aperture_bytes,
          window_result.endpoint.fence_end, command_slots);
}
static bool bcs_context_identity(void) {
 const struct intel_bdw_bcs_context_config *c=&bcs_context_native.config;
 return bcs_context_native.physical==command_native.owner.physical && bcs_context_native.root==command_native.owner.root &&
 command_slots_held() && command_slot_vector(false) &&
 c->ring_gpu==(uint64_t)command_slots.first*4096 && c->context_gpu==c->ring_gpu+4096 &&
 c->completion_gpu==c->ring_gpu+3*4096 &&
 c->aperture_bytes==0x10000000 && !c->fence_begin && c->fence_end==0x2200000 &&
 c->nonce==0x435458494d414745ull && c->context_id==1 && c->vendor==0x8086 && c->device==0x1626;
}
static bool bcs_context_check(unsigned expected) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_context_boundary();
#endif
 if(bcs_context_native.verified!=expected || bcs_context_native.initialized!=1 || bcs_context_native.erased || bcs_context_native.fault || !bcs_context_identity() ||
 !intel_bdw_bcs_context_verify(&bcs_context_native.config,(volatile const uint32_t *)command_pointer(),bcs_context_native.descriptor)){
  bcs_context_native.fault=1;return false;
 }
 ++bcs_context_native.verified;return true;
}
static bool bcs_context_reserve(void) {
 if(bcs_context_native.initialized || bcs_context_native.begin || bcs_context_native.fault)return false;
 bcs_context_native.begin=timer_monotonic_us();
 if(!bcs_context_native.begin || !intel_alias_ram_prepare(&context_ram,memmap_request.response) || !command_slot_vector(true))return false;
 uint64_t flags=wake_irq_save();
 bool reserved=scanout_interval_claim(&bcs_context_native.slots,result.mmio_base,
  window_result.aperture_base,window_result.aperture_bytes,window_result.endpoint.fence_end,command_slots);
 wake_irq_restore(flags);
 bcs_context_native.reserved=reserved;
 return reserved;
}
static bool bcs_context_initialize(void) {
 if(!command_slots_held() || !command_slot_vector(false))return false;
 uint64_t ring=(uint64_t)command_slots.first*4096;
 bcs_context_native.config=(struct intel_bdw_bcs_context_config){ring,ring+4096,ring+3*4096,
 window_result.aperture_bytes,0,window_result.endpoint.fence_end,
 0x435458494d414745ull,1,0x8086,0x1626};
 bcs_context_native.physical=command_native.owner.physical;bcs_context_native.root=command_native.owner.root;
 bool ok=bcs_context_native.begin && intel_bdw_bcs_context_build(&bcs_context_native.config,
  (volatile uint32_t *)command_pointer(),&bcs_context_native.descriptor);
 if(!ok){bcs_context_native.fault=1;return false;}
 bcs_context_native.initialized=1;
 __asm__ volatile("mfence":::"memory");
 bcs_context_native.end=timer_monotonic_us();
 return bcs_context_native.end>=bcs_context_native.begin && bcs_context_check(0);
}
/* The five pages contain both the legacy ring controls and execlist ports.
 * This lease covers the diagnostic kernel VAs. It supplies no DMA/cache ACK.
 * CPU0 remains under the parent's interrupt exclusion across both samples. */
static bool context_register_alias(unsigned slot,bool writable)
{
 struct vmm_alias_observation alias;
 struct vmm_range_observation range;
 uint64_t physical=result.mmio_base+wake_pages[slot];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_bootstrap_alias(slot,physical,&alias,&range);
#else
 bool ok=vmm_diagnostic_kernel_alias(gt_bootstrap_native.root,WAKE_VA+4096*slot,
    physical,1,gt_bootstrap_native.cache.cpu.physical_bits,&alias,&range);
#endif
 return ok && alias.pages==1 && alias.leaves==1 && alias.leaf_sizes==1 &&
        alias.pat_index==2 && alias.writable==(unsigned)writable && !alias.user && alias.no_execute==1;
}

static bool context_engine_permissions(bool writable)
{
 unsigned *mask=&gt_bootstrap_native.engine_ro_mask;
 if(*mask!=(writable?31u:0u) || !context_wp() || !gt_bootstrap_native.post_bound ||
    !command_context_live() || !command_slots_held())return false;
 for(unsigned n=0;n<INTEL_ENGINE_COUNT;++n){
  unsigned engine=writable?INTEL_ENGINE_COUNT-1-n:n,bit=1u<<engine,slot=3+engine;
  if(!bootstrap_tick() || !command_context_live() || !gt_bootstrap_native.a->mapped[slot] ||
     !context_register_alias(slot,!writable) || !context_wp())return false;
  bool ok=bootstrap_page_protect(gt_bootstrap_native.root,slot,writable);
  if(!ok)return false;
  /* Record each successful change even if later verification fails. */
  if(writable){*mask &= ~bit;++bcs_context_native.reopened;}
  else {*mask |= bit;++bcs_context_native.sealed;}
  if(!context_register_alias(slot,writable) || !bootstrap_tick())return false;
 }
 return context_wp() && command_context_live();
}

/* Reject any second CPU mapping, including large leaves or direct-map
 * aliases. The existing walker accepts up to two physical spans per pass. */
static bool context_submission_excluded(void)
{
 if(gt_bootstrap_native.engine_ro_mask!=31 || !context_wp() || !context_ram.ready)return false;
 for(unsigned first=0;first<INTEL_ENGINE_COUNT;first+=VMM_ALIAS_SPANS){
  struct vmm_alias_span spans[VMM_ALIAS_SPANS];
  unsigned count=INTEL_ENGINE_COUNT-first;
  if(count>VMM_ALIAS_SPANS)count=VMM_ALIAS_SPANS;
  for(unsigned i=0;i<count;++i){
   unsigned slot=3+first+i;
   spans[i]=(struct vmm_alias_span){WAKE_VA+4096*slot,result.mmio_base+wake_pages[slot],1};
  }
  if(!command_context_live() || !bootstrap_tick())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool unique=intel_display_test_context_aliases(spans,count);
#else
  bool unique=vmm_diagnostic_readonly_register_aliases(gt_bootstrap_native.root,spans,count,
    gt_bootstrap_native.cache.cpu.physical_bits,context_ram.ranges,context_ram.count);
#endif
  if(!unique)return false;
  ++bcs_context_native.alias_checks;
 }
 /* The WIN audio/video tails share one register page. This trial has no
  * reason to map it. Prove its absence, including large direct-map leaves,
  * in the same sole CPU root that owns the five protected engine pages.
  * DEProxy can also write these tails independently of CPU mappings. */
 struct vmm_physical_span widi={.physical=result.mmio_base+0x14000,.pages=1};
 if(!command_context_live() || !bootstrap_tick())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool absent=intel_display_test_context_widi_absent(widi.physical);
#else
 struct vmm_alias_inventory inventory;
 bool absent=vmm_diagnostic_kernel_physical_unmapped(gt_bootstrap_native.root,&widi,1,
    gt_bootstrap_native.cache.cpu.physical_bits,context_ram.ranges,context_ram.count,&inventory,NULL);
#endif
 if(!absent)return false;
 ++bcs_context_native.widi_unmapped;
 return bootstrap_tick() && command_context_live() && context_wp();
}
static bool command_requester_held(void)
{
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint32_t command=intel_display_test_bootstrap_config(4);
#else
 uint32_t command=pci_read_config32(gt_bootstrap_native.a->fw.gt.a.common.gpu,4);
#endif
 return (command&0xffff)==3 && command==(gt_bootstrap_native.pci[0][1]&~4u);
}
/* The parent has just checked all UC aliases. Keep PCI memory decode on and
 * bus mastering off for every read, within its original clock budget. */
static bool command_slots_sample(void)
{
 if(gt_bootstrap_native.engine_ro_mask!=31 || !context_wp())return false;
 for(unsigned i=0;i<=command_slots.pages;++i){
  uint32_t words[3];
  for(unsigned j=0;j<3;++j){
   if(!command_context_live() || !bootstrap_tick() || !command_slots_held() ||
      wake_pages[13]!=INTEL_WINDOW_PAGE || !completion_mapping(gt_bootstrap_native.a,13))return false;
   if(!command_requester_held())return false;
   uint32_t offset=8*i+(j==1?0:4);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   if(!intel_display_test_context_pte(offset,&words[j]))return false;
#else
   words[j]=*(volatile const uint32_t *)(WAKE_VA+13*4096+offset);
#endif
   ++bcs_context_native.pte_reads;
  }
  if(words[0]!=words[2] || (((uint64_t)words[2]<<32)|words[1])!=bcs_context_native.original[i])return false;
 }
 return bootstrap_tick() && command_context_live() && command_slots_held() && context_wp();
}
/* Observe the GAM request, both video queues and the CPD policy using the
 * existing UC aliases. Slot zero also contains the parent's forcewake control. */
static bool context_traffic_sample(unsigned pass)
{
 static const struct {unsigned slot,reg;} regs[]={
  {8,0x40d0},{4,0x12870},{4,0x12878},{6,0x1c870},{6,0x1c878},{0,0xa180}
 };
 if(pass>=2 || pass!=bcs_context_native.traffic_samples ||
    bcs_context_native.pte_reads!=(pass?30u:0u) || gt_bootstrap_native.engine_ro_mask!=31 ||
    !context_wp() || !command_requester_held() || !engine_requesters(gt_bootstrap_native.a))return false;
 for(unsigned i=0;i<sizeof(regs)/sizeof(regs[0]);++i){
  unsigned slot=regs[i].slot,reg=regs[i].reg;
  if(!command_context_live() || !bootstrap_tick() || !command_slots_held() ||
     !gt_bootstrap_native.a->mapped[slot] || wake_pages[slot]!=(reg&~4095u) ||
     !context_register_alias(slot,slot==0))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  uint32_t word=intel_display_test_context_traffic(reg);
#else
  uint32_t word=*(volatile const uint32_t *)(WAKE_VA+4096*slot+(reg&4095u));
#endif
  if(word==UINT32_MAX)return false;
  bcs_context_native.traffic[pass][i]=word;
 }
 if(!command_context_live() || !bootstrap_tick() || !context_wp() ||
    !command_requester_held() || !engine_requesters(gt_bootstrap_native.a))return false;
 ++bcs_context_native.traffic_samples;
 return true;
}
static bool bcs_context_held(void) {
 unsigned expected=bcs_context_native.held+1;
 if(expected>2 || !bcs_context_check(expected) || bcs_context_native.held+1!=expected)return false;
 if((expected==1 && !context_engine_permissions(false)) || !context_submission_excluded() ||
    (expected==1 && !context_traffic_sample(0)) || !command_slots_sample() ||
    (expected==2 && (!context_traffic_sample(1) || !context_engine_permissions(true)))){
  bcs_context_native.fault=1;
  return false;
 }
 ++bcs_context_native.held;return true;
}
static bool bcs_context_erase(void) {
 if(bcs_context_native.held!=2 || !command_native.outer_safe || !bcs_context_check(3))return false;
 bcs_context_native.erase_begin=timer_monotonic_us();
 volatile uint64_t *ram=command_pointer();for(unsigned i=0;i<2048;i++)ram[i]=0;
 __asm__ volatile("mfence":::"memory");
 for(unsigned i=0;i<BDW_BCS_IMAGE_BYTES/sizeof(*ram);++i)if(ram[i])return false;
 bcs_context_native.erased=1;
 bcs_context_native.erase_end=timer_monotonic_us();
 if(!bcs_context_native.erase_begin || bcs_context_native.erase_end<bcs_context_native.erase_begin)return false;
 uint64_t flags=wake_irq_save();
 bool held=command_slots_held();
 if(held){
  scanout_interval_release(&bcs_context_native.slots);
  bcs_context_native.released=true;
 }
 wake_irq_restore(flags);
 return held;
}
static void bcs_context_report(void) {
 for(unsigned n=0;n<bcs_context_native.traffic_samples;++n){
  const uint32_t *w=bcs_context_native.traffic[n];
  console_printf("intel-context-traffic: v=2 sample=%u request=%x vcs0_refs=%x vcs0_prefetch=%x vcs1_refs=%x vcs1_prefetch=%x misc_ctrl0=%x\n",
   (uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5]);
 }
 console_printf("intel-bcs-context: v=4 widi_unmapped=%u traffic_samples=%u alias_checks=%u sealed=%u reopened=%u ro_mask=%u pte_reads=%u reserved=%u released=%u initialized=%u verified=%u held=%u erased=%u fault=%u physical=%x root=%x descriptor=%x begin=%u end=%u erase_begin=%u erase_end=%u ring=%x context=%x completion=%x aperture=%u fence_begin=%u fence_end=%u nonce=%x id=%u vendor=%x device=%x mapped=0 submissions=0 ownership=0\n",
 (uint64_t)bcs_context_native.widi_unmapped,(uint64_t)bcs_context_native.traffic_samples,(uint64_t)bcs_context_native.alias_checks,(uint64_t)bcs_context_native.sealed,(uint64_t)bcs_context_native.reopened,(uint64_t)gt_bootstrap_native.engine_ro_mask,
 (uint64_t)bcs_context_native.pte_reads,(uint64_t)bcs_context_native.reserved,(uint64_t)bcs_context_native.released,
 (uint64_t)bcs_context_native.initialized,(uint64_t)bcs_context_native.verified,(uint64_t)bcs_context_native.held,
 (uint64_t)bcs_context_native.erased,(uint64_t)bcs_context_native.fault,bcs_context_native.physical,bcs_context_native.root,
 bcs_context_native.descriptor,bcs_context_native.begin,bcs_context_native.end,bcs_context_native.erase_begin,bcs_context_native.erase_end,
 bcs_context_native.config.ring_gpu,bcs_context_native.config.context_gpu,bcs_context_native.config.completion_gpu,
 bcs_context_native.config.aperture_bytes,bcs_context_native.config.fence_begin,bcs_context_native.config.fence_end,
 bcs_context_native.config.nonce,(uint64_t)bcs_context_native.config.context_id,(uint64_t)bcs_context_native.config.vendor,
 (uint64_t)bcs_context_native.config.device);
}
