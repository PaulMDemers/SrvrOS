/* Read-only child of the original reset owner. The pre-reset and post-memory
 * samples use the same exclusion and original clocks. Stable words convey no
 * interval ownership or display-reader retirement. */
#define GT_RANGE_VA (GT_MEMORY_VA + 2 * 4096ull)
#define GT_RANGE_TAIL 128u
#define GT_RANGE_WORDS (GT_RANGE_TAIL + 1)
#define GT_RANGE_FIRST 1356u
#define GT_RANGE_CANDIDATE 8704u
static struct {
 uint64_t words[2][GT_RANGE_WORDS],begin[2],end[2],mapped;
 unsigned count[2],reads,maps,retired,completed;
} gt_range;

static bool gt_range_post_memory(void) {
 return gt_bootstrap_native.reset.complete && gt_bootstrap_native.post_bound &&
  gt_bootstrap_native.command_writes==1 && gt_bootstrap_native.display_samples==2 &&
  gt_memory.d.state==2 && !gt_bootstrap_native.memory_child &&
  gt_memory.d.epoch==gt_bootstrap_native.g->engine_epoch &&
  gt_bootstrap_native.g->engine_epoch==gt_bootstrap_native.epoch+1;
}
static bool gt_range_live(unsigned phase) {
 if(phase!=gt_range.completed || !bootstrap_parent() || !wake_irq_off() ||
  vmm_current_address_space()!=gt_bootstrap_native.root)return false;
 if(!phase)return !gt_bootstrap_native.reset.object && !gt_bootstrap_native.command_writes &&
  gt_bootstrap_native.display_samples==1;
 return gt_range_post_memory();
}
static bool gt_range_translation(uint64_t *physical) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_range_translate(physical);
#else
 return vmm_virt_to_phys_in_space(gt_bootstrap_native.root,GT_RANGE_VA,physical);
#endif
}
static bool gt_range_policy(unsigned phase,uint64_t physical) {
 struct x86_cache_snapshot cache;
 unsigned mtrr,pat,effective;
 if(!gt_range_live(phase))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_bootstrap_cache(&cache);
#else
 bool ok=backing_range_reserved(physical,4096) && intel_cache_probe_context(&cache);
#endif
 return ok && gt_range_live(phase) && x86_cache_equal(&cache,&gt_bootstrap_native.cache) &&
  x86_cache_page(&cache,physical,3,&mtrr,&pat,&effective) && !effective;
}
static bool gt_range_alias(unsigned phase,uint64_t physical) {
 struct vmm_alias_observation alias;
 uint64_t actual;
 if(!gt_range_live(phase) || gt_range.mapped!=physical ||
  !gt_range_translation(&actual) || actual!=physical)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_range_alias(&alias);
#else
 bool ok=vmm_diagnostic_kernel_alias(gt_bootstrap_native.root,GT_RANGE_VA,physical,1,
    gt_bootstrap_native.cache.cpu.physical_bits,&alias,NULL);
#endif
 return ok && gt_range_live(phase) && alias.pages==1 && alias.leaves==1 &&
  alias.pat_index==3 && !alias.writable && !alias.user && alias.no_execute==1 && alias.leaf_sizes==1;
}
static bool gt_range_read(uint32_t offset,uint32_t *word) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_range_read(offset,word))return false;
#else
 *word=*(volatile const uint32_t *)(GT_RANGE_VA+(offset&4095));
#endif
 ++gt_range.reads;
 return true;
}
static bool gt_range_batch(unsigned phase,uint32_t first,unsigned count) {
 uint32_t offset=0x800000u+8*first,page=offset&~4095u;
 uint64_t physical=result.mmio_base+page,actual;
 if(!count || count>(4096-(offset&4095))/8 || count>GT_RANGE_WORDS-gt_range.count[phase] ||
  gt_range.mapped || !gt_range_live(phase))return false;
 if(gt_range_translation(&actual)){gt_range.mapped=actual;return false;}
 if(!bootstrap_guard() || !gt_range_policy(phase,physical) || !bootstrap_tick())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool mapped=intel_display_test_range_map(physical);
#else
 bool mapped=vmm_map_page_in_space(gt_bootstrap_native.root,GT_RANGE_VA,physical,
    VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_WRITE_THROUGH);
#endif
 if(!mapped) {
  if(gt_range_translation(&actual))gt_range.mapped=actual;
  return false;
 }
 gt_range.mapped=physical;++gt_range.maps;
 bool ok=gt_range_policy(phase,physical) && bootstrap_tick() && gt_range_alias(phase,physical);
 for(unsigned i=0;ok && i<count;i++) {
  uint32_t hi,lo,again,reg=offset+8*i;
  if(!gt_range_read(reg+4,&hi) || !gt_range_read(reg,&lo) ||
   !gt_range_read(reg+4,&again) || hi!=again) {
   ok=false;
   break;
  }
  uint64_t value=(uint64_t)hi<<32|lo;
  if(value==UINT64_MAX){ok=false;break;}
  unsigned index=gt_range.count[phase]++;
  gt_range.words[phase][index]=value;
  if(phase && gt_range.words[0][index]!=value)ok=false;
 }
 if(!gt_range_live(phase) || !bootstrap_tick() ||
  !gt_range_translation(&actual) || actual!=physical)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool retired=intel_display_test_range_unmap(&actual);
#else
 bool retired=vmm_unmap_page_in_space(gt_bootstrap_native.root,GT_RANGE_VA,&actual);
#endif
 if(!retired || actual!=physical || gt_range_translation(&actual))return false;
 gt_range.mapped=0;++gt_range.retired;
 return bootstrap_guard() && ok;
}
#ifdef SRVROS_INTEL_GT_SLOT
/* The original GT requester owns this software slot reservation across the
 * bus-master inhibit, reset and post-reset sampling. Keep it on every fault. */
#include <srvros/scanout_transfer.h>
static struct scanout_transfer gt_slot;
static struct scanout_transfer_pool gt_slot_pool;
static struct scanout_transfer_token gt_slot_token;
static bool gt_slot_reserved,gt_slot_checked,gt_slot_returned;

static struct scanout_transfer_manifest gt_slot_source(unsigned sample) {
 return (struct scanout_transfer_manifest){
  .mmio=result.mmio_base,.aperture=window_result.aperture_base,.aperture_bytes=window_result.aperture_bytes,
  .fence_end=window_result.endpoint.fence_end,.first=GT_RANGE_CANDIDATE,.pages=1,
  .original=&gt_range.words[sample][GT_RANGE_TAIL],.source_epoch=gt_bootstrap_native.g->exclusion.epoch,
  .obligations=INTEL_COMPLETION_OPEN
 };
}

static bool gt_slot_live(void) {
 if(!gt_bootstrap_native.g)return false;
 struct scanout_transfer_manifest source=gt_slot_source(gt_slot_checked?1:0);
 return scanout_transfer_reservation_matches(&gt_slot,&gt_slot_token,&source);
}

static bool gt_slot_boundary(unsigned phase) {
 if(phase>1 || !gt_range_live(phase))return false;
 struct scanout_transfer_manifest source=gt_slot_source(phase);
 if(source.fence_end!=(uint64_t)source.first*4096 ||
    !scanout_interval_protect_boot_framebuffer(source.mmio,source.aperture,source.aperture_bytes,source.fence_end))
  return false;
 if(!phase) {
  if(gt_slot_reserved || !scanout_transfer_reserve(&gt_slot,&gt_slot_pool,&gt_bootstrap_native.g->exclusion,
                                                   &source,&gt_slot_token))return false;
  gt_slot_reserved=true;
 }
 if(!scanout_transfer_reservation_matches(&gt_slot,&gt_slot_token,&source))return false;
 if(phase)gt_slot_checked=true;
 return true;
}

static bool gt_slot_return(void) {
 if(!gt_slot_reserved || !gt_slot_checked || gt_slot_returned || !gt_slot_live() || !gt_range_post_memory() ||
    !bootstrap_guard() || !bootstrap_tick())return false;
 if(!scanout_transfer_close(&gt_slot,&gt_slot_token))return false;
 gt_slot_returned=true;
 return true;
}
#endif
static bool gt_range_capture(unsigned phase) {
 if(phase>1 || !gt_range_live(phase) ||
  backing.offset || backing.count!=GT_RANGE_FIRST || !backing.tiled ||
  backing.bytes!=GT_RANGE_FIRST*4096ull || backing.ggtt_bytes!=0x800000 ||
  window_result.aperture_bytes!=0x10000000 ||
  backing.sample[1].fence[0]!=0x021ff02f00000001ull ||
  !bootstrap_tick())return false;
 for(unsigned i=1;i<INTEL_BACKING_FENCES;i++)if(backing.sample[1].fence[i])return false;
 gt_range.begin[phase]=gt_bootstrap_native.last;
 if(!gt_range_batch(phase,GT_RANGE_FIRST,GT_RANGE_TAIL) ||
  !gt_range_batch(phase,GT_RANGE_CANDIDATE,1) || gt_range.count[phase]!=GT_RANGE_WORDS ||
  gt_bootstrap_native.last<=gt_range.begin[phase])return false;
#ifdef SRVROS_INTEL_GT_SLOT
 if(!gt_slot_boundary(phase) || !bootstrap_tick())return false;
#endif
 gt_range.end[phase]=gt_bootstrap_native.last;
 ++gt_range.completed;
 return true;
}
static void gt_range_report(void) {
#ifdef SRVROS_INTEL_GT_SLOT
 const unsigned version=2;
#else
 const unsigned version=1;
#endif
 console_printf("intel-range-summary: v=%u complete=%u phase=%u maps=%u retired=%u live=%x reads=%u tail=%u candidate=%u begin0=%u end0=%u begin1=%u end1=%u words0=%u words1=%u authority=none\n",
  (uint64_t)version,(uint64_t)(gt_range.completed==2),(uint64_t)gt_range.completed,(uint64_t)gt_range.maps,(uint64_t)gt_range.retired,gt_range.mapped,
  (uint64_t)gt_range.reads,(uint64_t)GT_RANGE_TAIL,(uint64_t)GT_RANGE_CANDIDATE,
  gt_range.begin[0],gt_range.end[0],gt_range.begin[1],gt_range.end[1],
  (uint64_t)gt_range.count[0],(uint64_t)gt_range.count[1]);
#ifdef SRVROS_INTEL_GT_SLOT
 console_printf("intel-slot-summary: v=1 reserved=%u rechecked=%u returned=%u candidate=%u pages=1 pte_writes=0\n",
  (uint64_t)gt_slot_reserved,(uint64_t)gt_slot_checked,(uint64_t)gt_slot_returned,(uint64_t)GT_RANGE_CANDIDATE);
#endif
 static char encoded[(GT_RANGE_WORDS*8+2)/3*4+1];
 for(unsigned sample=0;sample<2;sample++) {
  entry_requester_base64((const uint8_t *)gt_range.words[sample],gt_range.count[sample]*8,encoded);
  console_printf("intel-range-vector: v=1 sample=%u count=%u data=%s\n",
   (uint64_t)sample,(uint64_t)gt_range.count[sample],encoded);
 }
}
