/* Isolated CPU image lifetime only: prospective addresses stay UNMAPPED. */
#include <srvros/intel_bdw_bcs_context.h>
#include <srvros/scanout_space.h>
/* Slot zero in the observed window is the transaction target. The four
 * following slots can be reserved by software before any GGTT edit. */
static const struct scanout_space_range command_slots = { INTEL_WINDOW_FIRST + 1, 4 };
static struct {
 struct intel_bdw_bcs_context_config config;
 struct scanout_interval slots;
 uint64_t original[5];
 bool reserved, released;
 unsigned pte_reads;
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
 if(!bcs_context_native.begin || !command_slot_vector(true))return false;
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
/* The parent has just checked all UC aliases. Keep PCI memory decode on and
 * bus mastering off for every read, within its original clock budget. */
static bool command_slots_sample(void)
{
 for(unsigned i=0;i<=command_slots.pages;++i){
  uint32_t words[3];
  for(unsigned j=0;j<3;++j){
   if(!command_context_live() || !bootstrap_tick() || !command_slots_held() ||
      wake_pages[13]!=INTEL_WINDOW_PAGE || !completion_mapping(gt_bootstrap_native.a,13))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   uint32_t command=intel_display_test_bootstrap_config(4);
#else
   uint32_t command=pci_read_config32(gt_bootstrap_native.a->fw.gt.a.common.gpu,4);
#endif
   if((command&0xffff)!=3 || command!=(gt_bootstrap_native.pci[0][1]&~4u))return false;
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
 return bootstrap_tick() && command_context_live() && command_slots_held();
}
static bool bcs_context_held(void) {
 unsigned expected=bcs_context_native.held+1;
 if(expected>2 || !bcs_context_check(expected) || bcs_context_native.held+1!=expected)return false;
 if(!command_slots_sample()){
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
 console_printf("intel-bcs-context: v=2 pte_reads=%u reserved=%u released=%u initialized=%u verified=%u held=%u erased=%u fault=%u physical=%x root=%x descriptor=%x begin=%u end=%u erase_begin=%u erase_end=%u ring=%x context=%x completion=%x aperture=%u fence_begin=%u fence_end=%u nonce=%x id=%u vendor=%x device=%x mapped=0 submissions=0 ownership=0\n",
 (uint64_t)bcs_context_native.pte_reads,(uint64_t)bcs_context_native.reserved,(uint64_t)bcs_context_native.released,
 (uint64_t)bcs_context_native.initialized,(uint64_t)bcs_context_native.verified,(uint64_t)bcs_context_native.held,
 (uint64_t)bcs_context_native.erased,(uint64_t)bcs_context_native.fault,bcs_context_native.physical,bcs_context_native.root,
 bcs_context_native.descriptor,bcs_context_native.begin,bcs_context_native.end,bcs_context_native.erase_begin,bcs_context_native.erase_end,
 bcs_context_native.config.ring_gpu,bcs_context_native.config.context_gpu,bcs_context_native.config.completion_gpu,
 bcs_context_native.config.aperture_bytes,bcs_context_native.config.fence_begin,bcs_context_native.config.fence_end,
 bcs_context_native.config.nonce,(uint64_t)bcs_context_native.config.context_id,(uint64_t)bcs_context_native.config.vendor,
 (uint64_t)bcs_context_native.config.device);
}
