/* Isolated CPU image lifetime only: prospective addresses stay UNMAPPED. */
#include <srvros/intel_bdw_bcs_context.h>
static struct {
 struct intel_bdw_bcs_context_config config;
 uint64_t physical,root,descriptor,begin,end,erase_begin,erase_end;
 unsigned initialized,verified,held,erased,fault;
} bcs_context_native;
static bool bcs_context_identity(void) {
 const struct intel_bdw_bcs_context_config *c=&bcs_context_native.config;
 return bcs_context_native.physical==command_native.owner.physical && bcs_context_native.root==command_native.owner.root &&
 c->ring_gpu==0x2200000 && c->context_gpu==0x2201000 && c->completion_gpu==0x2203000 &&
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
static bool bcs_context_initialize(void) {
 if(bcs_context_native.initialized || bcs_context_native.begin || bcs_context_native.fault)return false;
 bcs_context_native.begin=timer_monotonic_us();
 bcs_context_native.config=(struct intel_bdw_bcs_context_config){0x2200000,0x2201000,0x2203000,0x10000000,0,0x2200000,
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
static bool bcs_context_held(void) {
 unsigned expected=bcs_context_native.held+1;
 if(expected>2 || !bcs_context_check(expected) || bcs_context_native.held+1!=expected)return false;
 ++bcs_context_native.held;return true;
}
static bool bcs_context_erase(void) {
 if(bcs_context_native.held!=2 || !command_native.outer_safe || !bcs_context_check(3))return false;
 bcs_context_native.erase_begin=timer_monotonic_us();
 volatile uint64_t *ram=command_pointer();for(unsigned i=0;i<2048;i++)ram[i]=0;
 __asm__ volatile("mfence":::"memory");
 bcs_context_native.erase_end=timer_monotonic_us();bcs_context_native.erased=1;
 return bcs_context_native.erase_end>=bcs_context_native.erase_begin;
}
static void bcs_context_report(void) {
 console_printf("intel-bcs-context: v=1 initialized=%u verified=%u held=%u erased=%u fault=%u physical=%x root=%x descriptor=%x begin=%u end=%u erase_begin=%u erase_end=%u ring=%x context=%x completion=%x aperture=%u fence_begin=%u fence_end=%u nonce=%x id=%u vendor=%x device=%x mapped=0 submissions=0 ownership=0\n",
 (uint64_t)bcs_context_native.initialized,(uint64_t)bcs_context_native.verified,(uint64_t)bcs_context_native.held,
 (uint64_t)bcs_context_native.erased,(uint64_t)bcs_context_native.fault,bcs_context_native.physical,bcs_context_native.root,
 bcs_context_native.descriptor,bcs_context_native.begin,bcs_context_native.end,bcs_context_native.erase_begin,bcs_context_native.erase_end,
 bcs_context_native.config.ring_gpu,bcs_context_native.config.context_gpu,bcs_context_native.config.completion_gpu,
 bcs_context_native.config.aperture_bytes,bcs_context_native.config.fence_begin,bcs_context_native.config.fence_end,
 bcs_context_native.config.nonce,(uint64_t)bcs_context_native.config.context_id,(uint64_t)bcs_context_native.config.vendor,
 (uint64_t)bcs_context_native.config.device);
}
