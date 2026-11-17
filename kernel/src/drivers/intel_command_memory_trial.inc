/* Separate command-memory preparation image only. CPU aliases and private RAM;
 * no GPU address lease, executable LRC, ring submission or GPU PTE store. */
#include <srvros/intel_command_memory.h>
#define COMMAND_NATIVE_VA (BACKING_VA+0x60000)
static struct {
 struct intel_command_memory owner;
 struct x86_cache_snapshot cache;
#ifdef SRVROS_INTEL_GT_ARENA
 const void *parent;
 uint64_t clock_at,clock_previous,outer_end;
 bool clock_lost;
#else
 const struct intel_source_engines *parent;
#endif
 uint64_t dma_before,dma_after,allocated,released,cache_checks,alias_checks,zeros,word_checks,retire_checks;
 bool outer_safe;
} command_native;
static bool command_parent_guard(void *,const struct intel_command_lease *);
static bool command_parent_retired(void *,const struct intel_command_lease *);
static bool command_returned(void);
#ifdef SRVROS_INTEL_GT_ARENA
static bool command_context_live(void);
#endif
static bool command_backing(void *ctx,uint64_t physical,uint32_t pages) {
 if(ctx!=&command_native || pages!=4 || !physical || (physical&4095) || physical>0x100000000ull-16384 ||
 vmm_current_address_space()!=command_native.owner.root)return false;
#ifdef SRVROS_INTEL_GT_ARENA
 if(!command_context_live())return false;
#endif
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_command_backing(physical,pages);
#else
 if(!intel_scanout_trial_ram(physical,pages))return false;
 for(unsigned i=0;i<pages;i++){
  uint64_t pa=physical+4096*i,actual;
  if(!pmm_frame_has_tag(pa,PMM_FRAME_DMA) || !vmm_virt_to_phys((uint64_t)pmm_phys_to_virt(pa),&actual) || actual!=pa)return false;
 }
 return true;
#endif
}
static uint64_t command_alloc(void *ctx,uint32_t pages) {
 if(ctx!=&command_native || pages!=4)return 0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t pa=intel_display_test_command_alloc();
#else
 uint64_t pa=pmm_alloc_frames_tagged(pages,PMM_FRAME_DMA);
#endif
 if(pa)command_native.allocated+=pages;
 return pa;
}
static bool command_free(void *ctx,uint64_t physical,uint32_t pages) {
 struct intel_command_memory *o=&command_native.owner;
 if(!command_backing(ctx,physical,pages) || o->state!=COMMAND_RETURNED || o->retired!=4 || o->hold.active ||
 o->memory.cpu_serial || !command_native.outer_safe || !o->io.retired(ctx,&o->lease))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_command_free(physical))return false;
#else
 for(unsigned i=0;i<4;i++){
  uint64_t actual;if(vmm_virt_to_phys_in_space(o->root,COMMAND_NATIVE_VA+4096*i,&actual))return false;
 }
 pmm_free_frame_range(physical,pages);
#endif
 command_native.released+=pages;return true;
}
static bool command_map(void *ctx,uint64_t root,uint64_t va,uint64_t physical) {
 struct intel_command_memory *o=&command_native.owner;
 if(ctx!=&command_native || root!=o->root || va!=COMMAND_NATIVE_VA+4096*o->maps ||
 physical!=o->physical+4096*o->maps || o->maps>=4 || !command_backing(ctx,o->physical,4))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_command_map(root,va,physical);
#else
 uint64_t existing;
 if(vmm_virt_to_phys_in_space(root,va,&existing))return false;
 return vmm_map_page_in_space(root,va,physical,VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE);
#endif
}
static bool command_unmap(void *ctx,uint64_t root,uint64_t va,uint64_t *physical) {
 struct intel_command_memory *o=&command_native.owner;
 if(ctx!=&command_native || root!=o->root || va!=COMMAND_NATIVE_VA+4096*o->retired || !physical ||
 o->retired>=4 || !command_native.outer_safe || !command_backing(ctx,o->physical,4))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_command_unmap(root,va,physical);
#else
 uint64_t actual;
 if(!vmm_virt_to_phys_in_space(root,va,&actual) || actual!=o->physical+4096*o->retired ||
 !vmm_unmap_page_in_space(root,va,physical) || *physical!=actual || vmm_virt_to_phys_in_space(root,va,&actual))return false;
 return true;
#endif
}
static bool command_observe(void *ctx,uint64_t root,uint64_t va,uint64_t physical,struct intel_command_alias *out) {
 struct intel_command_memory *o=&command_native.owner;
 if(ctx!=&command_native || root!=o->root || va!=COMMAND_NATIVE_VA || physical!=o->physical || !out ||
 !command_backing(ctx,physical,4))return false;
 struct x86_cache_snapshot cache;struct vmm_alias_observation alias;
 /* The actual VMM observer requires an uninterrupted sole-root snapshot.
  * Preparation and final cleanup run outside the enclosing GPU IRQ scope. */
 uint64_t flags=wake_irq_save();
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_command_cache(&cache);
#else
 bool ok=intel_cache_probe_context(&cache);
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 ok=ok && command_context_live();
#endif
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 ok=ok && intel_display_test_command_alias(root,va,physical,&alias);
#else
 ok=ok && vmm_diagnostic_kernel_alias(root,va,physical,4,cache.cpu.physical_bits,&alias,NULL);
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 ok=ok && command_context_live();
#endif
 ok=ok && wake_irq_off();wake_irq_restore(flags);
 if(!ok || cache.status!=CACHE_OBSERVED || cache.cpu.signature!=0x306d4 ||
 !x86_cache_equal(&cache,&command_native.cache))return false;
 ++command_native.cache_checks;
 unsigned checks=0,mtrr,pat,effective;
 if(!x86_cache_range(&cache,physical,4,alias.pat_index,&mtrr,&pat,&effective,&checks) || !checks)return false;
 *out=(struct intel_command_alias){alias.pages,alias.leaves,alias.pat_index,alias.writable,alias.user,
 alias.no_execute,alias.leaf_sizes,mtrr,pat,effective};++command_native.alias_checks;return true;
}
static volatile uint64_t *command_pointer(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_command_pointer();
#else
 return (volatile uint64_t *)COMMAND_NATIVE_VA;
#endif
}
static bool command_initialize(void *ctx,uint64_t va,uint32_t bytes) {
 struct intel_command_memory *o=&command_native.owner;
 if(ctx!=&command_native || va!=COMMAND_NATIVE_VA || bytes!=16384 || o->maps!=4 || o->memory.state!=MEM_RAM ||
 !o->memory.cpu_serial || !command_backing(ctx,o->physical,4))return false;
 volatile uint64_t *ram=command_pointer();if(!ram)return false;
 for(unsigned i=0;i<2048;i++)ram[i]=0;
 __asm__ volatile("mfence":::"memory");
 for(unsigned i=0;i<2048;i++)if(ram[i])return false;
 ++command_native.zeros;return true;
}
static bool command_preserved(void *ctx,uint64_t va,uint32_t bytes) {
 struct intel_command_memory *o=&command_native.owner;
 if(ctx!=&command_native || va!=COMMAND_NATIVE_VA || bytes!=16384 || !command_native.outer_safe ||
 !o->memory.cpu_serial || !command_backing(ctx,o->physical,4))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_command_preserved();
#endif
 volatile uint64_t *ram=command_pointer();if(!ram)return false;
 for(unsigned i=0;i<2048;i++){if(ram[i])return false;++command_native.word_checks;}
 return true;
}
#ifdef SRVROS_INTEL_GT_ARENA
static uint64_t command_clock_floor(void) {
 if(command_native.owner.state==COMMAND_HELD)return gt_bootstrap_native.last;
 if(command_native.owner.state!=COMMAND_RETURNED || !command_native.outer_safe)return 0;
 uint64_t at=command_native.outer_end;
 if(joint.scope.end_us>at)at=joint.scope.end_us;
 if(joint.scope.hda.end_us>at)at=joint.scope.hda.end_us;
 if(gt_bootstrap_native.g && gt_bootstrap_native.g->last>at)at=gt_bootstrap_native.g->last;
 return at;
}
#endif
static uint64_t command_now(void *ctx) {
 if(ctx!=&command_native)return 0;
#ifdef SRVROS_INTEL_GT_ARENA
 bool parent_clock=command_native.owner.state==COMMAND_HELD ||
  (command_native.owner.state==COMMAND_RETURNED && command_native.outer_safe);
 uint64_t previous=command_clock_floor();
#endif
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at=intel_display_test_command_clock();
#else
 uint64_t at=timer_monotonic_us();
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 /* The core starts from the original preparation origin. Its first sample
  * must also follow the parent's latest sample, not merely that older origin.
  * Cleanup must follow actual parent retirement. Capture that floor BEFORE
  * the clock callback; no resource census or enclosing deadline restart. */
 if(parent_clock && (!previous || at<previous)){
  command_native.clock_lost=true;command_native.clock_at=at;
  command_native.clock_previous=previous;return 0;
 }
#endif
 return at;
}
static uint64_t command_dma(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_command_dma();
#else
 return pmm_tagged_frames(PMM_FRAME_DMA);
#endif
}
static bool command_prepare(void) {
 volatile unsigned char *s=(volatile unsigned char *)&command_native;
 for(unsigned i=0;i<sizeof(command_native);i++)s[i]=0;
 command_native.dma_before=command_dma();
 uint64_t flags=wake_irq_save();
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool observed=intel_display_test_command_cache(&command_native.cache);
#else
 bool observed=intel_cache_probe_context(&command_native.cache);
#endif
 observed=observed && wake_irq_off();wake_irq_restore(flags);
 if(!observed || command_native.cache.status!=CACHE_OBSERVED || command_native.cache.cpu.signature!=0x306d4)return false;
#ifdef SRVROS_INTEL_GT_ARENA
 /* PAT0 is fixed by this arena's WB leaf contract; reject an incompatible
  * captured policy before allocating private storage. */
 if((command_native.cache.pat&255)!=6)return false;
#endif
 return intel_command_memory_prepare(&command_native.owner,vmm_current_address_space(),COMMAND_NATIVE_VA,
 (struct intel_command_io){{&command_native,command_alloc,command_backing,command_free},command_map,command_unmap,
 command_observe,command_initialize,command_preserved,command_parent_guard,command_parent_retired,command_now});
}
static bool command_finish(bool safe) {
 if(!command_native.owner.object)return false;
 command_native.outer_safe=safe;
 bool ok=safe && intel_command_memory_close(&command_native.owner);
 command_native.dma_after=command_dma();
 return ok && command_native.owner.state==COMMAND_CLOSED && command_native.allocated==4 && command_native.released==4 &&
 command_native.dma_before==command_native.dma_after && command_native.zeros==1 && command_native.word_checks==2048 &&
 command_native.cache_checks==4 && command_native.alias_checks==4 && command_native.retire_checks==2;
}
static void command_report(void) {
 const struct intel_command_memory *o=&command_native.owner;const struct intel_command_lease *l=&o->lease;
 const struct intel_command_alias *a=&o->alias;
 const uint64_t f[]={o->state,o->reason,o->clock_reason,o->retained,o->complete,o->registered,o->busy,
 o->root,COMMAND_NATIVE_VA,o->physical,o->serial,o->prepare_begin,o->prepare_end,o->begin,o->end,o->last,o->close_begin,o->close_end,
 o->clock_at,o->clock_previous,l->epoch,l->sequence,l->prepare,l->gt,l->hda,l->outer,
 o->maps,o->retired,o->observations,o->guards,o->initializations,o->checks,
 a->pages,a->leaves,a->index,a->writable,a->user,a->nx,a->sizes,a->mtrr,a->pat,a->effective,
 command_native.allocated,command_native.released,command_native.dma_before,command_native.dma_after,
 command_native.cache_checks,command_native.alias_checks,command_native.zeros,command_native.word_checks,
 o->memory.state,o->memory.release_attempts,o->memory.releases,o->hold.active,o->hold.fault,
 (uint64_t)command_native.outer_safe|(command_native.retire_checks<<1),INTEL_DIAGNOSTIC_PREPARE_US,0,1,3,4,0,0,0};
 _Static_assert(sizeof(f)==512,"command-memory64Q");
 static uint8_t raw[512];static char data[685];unsigned k=0;
 for(unsigned i=0;i<64;i++)for(unsigned j=0;j<8;j++)raw[k++]=(uint8_t)(f[i]>>(8*j));
#ifdef SRVROS_INTEL_GT_ARENA
 entry_requester_base64(raw,k,data);console_printf("intel-arena-memory: v=1 b=%s\n",data);
#else
 entry_requester_base64(raw,k,data);console_printf("intel-command-memory: v=1 b=%s\n",data);
#endif
}
