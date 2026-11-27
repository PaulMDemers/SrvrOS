/* Read-only child of the original reset owner. The pre-reset and post-memory
 * samples use the same exclusion and original clocks. Stable words convey no
 * interval ownership or display-reader retirement. */
#define GT_RANGE_VA (GT_MEMORY_VA + 2 * 4096ull)
#define GT_RANGE_TAIL 128u
#ifdef SRVROS_INTEL_GT_FLUSH
#define GT_SECOND_SLOT 12291u
#define GT_RANGE_WORDS (GT_RANGE_TAIL + 4)
#else
#define GT_RANGE_WORDS (GT_RANGE_TAIL + 1)
#endif
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

#ifdef SRVROS_INTEL_GT_FLUSH
#include <srvros/intel_bdw_bcs_port.h>
#include <srvros/intel_bdw_blitter.h>
#include <srvros/intel_bdw_ggtt.h>
#include "intel_alias_ram.inc"
static struct scanout_transfer independent_slot;
enum { GT_COMMAND_FIRST = 11744, GT_COMMAND_PAGES = BDW_BLITTER_GLOBAL_PAGES,
       GT_COMMAND_FRAMES = GT_COMMAND_PAGES+BDW_PRIVATE_PAGES };
#define GT_FIRMWARE_COMMAND_PHYS 0x8ede1000ull
static struct {
 struct scanout_interval firmware;
 struct scanout_transfer transfer;
 struct scanout_transfer_token token;
 struct bdw_blitter_image image;
 struct bdw_wireless_routes routes;
 uint64_t original[GT_COMMAND_PAGES],physical;
 bool ram_returned;
 unsigned samples;
 bool returned;
} command_slots;
static struct {
    uint32_t original[GT_COMMAND_PAGES];
    uint64_t baseline_at;
    unsigned mapped,pinned,samples,retired;
} command_aperture;
static bool command_slots_sample(unsigned phase);
static bool command_ram_release(void);
static bool command_aperture_close(void);
static struct scanout_transfer_manifest command_slots_source(const uint64_t *original) {
 struct scanout_transfer_manifest source=gt_slot_source(0);
 source.first=GT_COMMAND_FIRST;
 source.pages=GT_COMMAND_PAGES;
 source.original=original;
 return source;
}
static bool command_slots_live(void) {
 struct scanout_transfer_manifest source=command_slots_source(command_slots.original);
 return intel_bdw_routes_held(&command_slots.routes,&command_slots.transfer) && command_slots.firmware.firmware &&
        scanout_interval_held(&command_slots.firmware,source.mmio,source.aperture,source.aperture_bytes,
                              source.fence_end,(struct scanout_space_range){source.first,source.pages}) &&
        scanout_transfer_reservation_matches(&command_slots.transfer,&command_slots.token,&source);
}
static bool bootstrap_command_reservation(const struct bdw_boot_binding *b) {
 return b->command_pages==&command_slots.transfer &&
        b->command_pages_epoch==command_slots.token.epoch && command_slots_live();
}
static bool slot_widi_preflight(void);
static bool slot_verify_reopened(void);
static bool slot_submission_restore(void);
static struct {
 struct intel_alias_ram_catalog ram;
 struct vmm_reserved_guard guard;
 uint64_t scanout_physical;
 unsigned pages,command_pages,vmm_reason;
 bool acquired, closed;
} slot_widi;
static const uint32_t slot_widi_pages[]={0x14000,0x6e000,0x65000};

static bool slot_widi_live(void) {
 uint64_t root=gt_bootstrap_native.root;
 if(!slot_widi.acquired || slot_widi.closed || slot_widi.pages!=3 ||
    slot_widi.command_pages!=GT_COMMAND_PAGES ||
    !intel_hda_gpu_live(&joint.scope) || !vmm_reserved_guard_io_page_held(&slot_widi.guard,root,
        window_result.aperture_base+(uint64_t)GT_SECOND_SLOT*4096))return false;
 for(unsigned i=0;i<slot_widi.pages;i++)
  if(!vmm_reserved_guard_excludes_io_page(&slot_widi.guard,root,result.mmio_base+slot_widi_pages[i]))return false;
 return true;
}
static bool firmware_command_backing_reserved(void) {
 for(unsigned i=0;i<GT_COMMAND_PAGES;i++)
  if(!pmm_frame_has_tag(GT_FIRMWARE_COMMAND_PHYS+4096ull*i,PMM_FRAME_RESERVED))return false;
 return true;
}

/* The image census has already revalidated every original display PTE.
 * Join all 128 separately sampled tail entries to it before pinning the
 * full physical span. The VMM checks PMM tags and the complete CPU root. */
static uint64_t inherited_scanout_span(void) {
 uint64_t first=backing.pte[0];
 if((first&4095)!=1 || first-1<backing.stolen_base || first>=backing.stolen_end ||
    4096ull*(GT_RANGE_FIRST+GT_RANGE_TAIL)>backing.stolen_end-(first-1))return 0;
 for(unsigned i=0;i<GT_RANGE_FIRST;i++)
  if(backing.pte[i]!=first+4096ull*i)return 0;
 for(unsigned i=0;i<GT_RANGE_TAIL;i++)
  if(gt_range.words[0][i]!=first+4096ull*(GT_RANGE_FIRST+i))return 0;
 return first-1;
}

/* Exclude the whole inherited image and tail, the slot's original backing
 * and aperture, the command backing and WiDi programming pages before
 * touching the requester. The display HDA owner stays borrowed throughout. */
static bool slot_widi_begin(uint64_t original) {
 struct vmm_alias_inventory inventory;
 struct vmm_unmapped_detail detail={0};
 uint64_t scanout_physical=inherited_scanout_span();
 const struct vmm_physical_span backing_pages[]={
  {original&~4095ull,1},{scanout_physical,GT_RANGE_FIRST+GT_RANGE_TAIL}
 };
 uint64_t root=gt_bootstrap_native.root;
 if(!wake_irq_off() || !intel_hda_gpu_live(&joint.scope) || original!=0x8f004001ull || !scanout_physical ||
    !firmware_command_backing_reserved() ||
    !intel_alias_ram_prepare(&slot_widi.ram,memmap_request.response))return false;
 if(!vmm_reserved_guard_begin_io_page(&slot_widi.guard,root,backing_pages,2,
       gt_bootstrap_native.cache.cpu.physical_bits,slot_widi.ram.ranges,slot_widi.ram.count,
       window_result.aperture_base+(uint64_t)GT_SECOND_SLOT*4096,&inventory,&detail)) {
  slot_widi.vmm_reason=detail.walk.reason;
  return false;
 }
 slot_widi.acquired=true;
 slot_widi.scanout_physical=scanout_physical;
 for(unsigned i=0;i<3;i++) {
  if(!vmm_reserved_guard_exclude_io_range(&slot_widi.guard,root,result.mmio_base+slot_widi_pages[i],1))
   return false;
  ++slot_widi.pages;
 }
 if(!vmm_reserved_guard_exclude_io_range(&slot_widi.guard,root,GT_FIRMWARE_COMMAND_PHYS,GT_COMMAND_PAGES))return false;
 slot_widi.command_pages=GT_COMMAND_PAGES;
 return slot_widi_live();
}

static struct {
 uint64_t physical;
 struct intel_bdw_ggtt_page plan;
 unsigned checks;
 bool port, freed;
} slot_backing;
static struct scanout_transfer_token independent_token;
static bool independent_returned,independent_reopened;
static struct scanout_transfer_manifest independent_source(unsigned sample) {
 struct scanout_transfer_manifest m=gt_slot_source(sample);
 m.first=GT_SECOND_SLOT;
 m.original=&gt_range.words[sample][GT_RANGE_TAIL+2];
 return m;
}
/* Only CPU access to private PMM RAM. Keep the plan and submission port
 * on any failed proof. The GPU continues to see the inherited translation. */
static bool independent_backing(unsigned sample, bool initialize)
{
 if(!intel_bdw_bcs_port_held(&slot_backing) || slot_backing.freed ||
    !wake_irq_off() || vmm_current_address_space()!=gt_bootstrap_native.root)return false;
 if(initialize) {
  if(slot_backing.physical || slot_backing.checks)return false;
  slot_backing.physical=pmm_alloc_frame_tagged(PMM_FRAME_DMA);
 }
 uint64_t pa=slot_backing.physical,actual;
 if(!intel_scanout_trial_ram(pa,1) || !pmm_frame_has_tag(pa,PMM_FRAME_DMA))return false;
 volatile uint64_t *ram=pmm_phys_to_virt(pa);
 if(!ram || !vmm_virt_to_phys((uint64_t)ram,&actual) || actual!=pa)return false;
 struct scanout_transfer_manifest source=independent_source(sample);
 struct intel_bdw_ggtt_page fresh;
 if(!intel_bdw_ggtt_page_plan(&source,pa,&fresh) || fresh.original==fresh.replacement)return false;
 if(initialize)slot_backing.plan=fresh;
 const struct intel_bdw_ggtt_page *plan=&slot_backing.plan;
 if(plan->physical!=pa || plan->gpu_address!=fresh.gpu_address || plan->mmio_offset!=fresh.mmio_offset ||
    plan->index!=fresh.index || plan->original!=fresh.original || plan->replacement!=fresh.replacement)return false;
 for(unsigned i=0;i<4096/sizeof(*ram);i++) {
  uint64_t expected=((pa+i)<<32) ^ plan->original ^ independent_token.epoch;
  if(initialize)ram[i]=expected;
  if(ram[i]!=expected)return false;
 }
 ++slot_backing.checks;
 return true;
}
static bool independent_matches(unsigned sample) {
 struct scanout_transfer_manifest m=independent_source(sample);
 return scanout_transfer_reservation_matches(&independent_slot,&independent_token,&m);
}
#endif

static bool gt_slot_live(void) {
 if(!gt_bootstrap_native.g)return false;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!command_slots_live() || !slot_widi_live() || !intel_bdw_bcs_port_held(&slot_backing) || !independent_matches(gt_slot_checked?1:0))return false;
#endif
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
#ifdef SRVROS_INTEL_GT_FLUSH
 /* Bind this Air trial to the original entries from its independent loan. */
 const uint64_t *entries=&gt_range.words[phase][GT_RANGE_TAIL+1];
 if(entries[0]!=0x8f003001ull || entries[1]!=0x8f004001ull || entries[2]!=0x8f005001ull)return false;
 struct scanout_transfer_manifest second=independent_source(phase);
 if(!phase && !scanout_transfer_reserve(&independent_slot,&gt_slot_pool,&gt_bootstrap_native.g->exclusion,
                                        &second,&independent_token))return false;
 if(!phase && (!slot_widi_preflight() || !slot_widi_begin(entries[1])))return false;
 if(!slot_widi_live())return false;
 if(!phase)slot_backing.port=intel_bdw_bcs_port_claim(&slot_backing);
 if(!slot_backing.port || !independent_matches(phase) || !independent_backing(phase,!phase))return false;
 if(!command_slots_sample(phase))return false;
#endif
 if(phase)gt_slot_checked=true;
 return true;
}

static bool gt_slot_return(void) {
 if(!gt_slot_reserved || !gt_slot_checked || gt_slot_returned || !gt_slot_live() ||
#ifdef SRVROS_INTEL_GT_FLUSH
    !independent_reopened || gt_bootstrap_native.command_writes!=2 || gt_bootstrap_native.pci_samples!=4 ||
#else
    !gt_range_post_memory() ||
#endif
    !bootstrap_guard() || !bootstrap_tick())return false;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!slot_queue_boundary(1) || !command_slots_sample(2) || !slot_verify_reopened() || slot_backing.checks!=2 || !independent_backing(1,false) || !bootstrap_tick())return false;
 struct vmm_alias_inventory inventory;
 struct vmm_unmapped_detail detail={0};
 if(!command_slots_live() || !scanout_transfer_close(&command_slots.transfer,&command_slots.token))return false;
 command_slots.returned=true;
 if(!command_ram_release())return false;
 if(!slot_widi_live() || !firmware_command_backing_reserved())return false;
 if(!vmm_reserved_guard_close(&slot_widi.guard,&inventory,&detail)) {
  slot_widi.vmm_reason=detail.walk.reason;
  return false;
 }
 slot_widi.closed=true;
 if(!command_aperture_close() || !bootstrap_tick() || !scanout_transfer_close(&independent_slot,&independent_token))return false;
 independent_returned=true;
#endif
 if(!scanout_transfer_close(&gt_slot,&gt_slot_token))return false;
 gt_slot_returned=true;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!slot_submission_restore())return false;
 pmm_free_frame(slot_backing.physical);
 slot_backing.freed=pmm_frame_has_tag(slot_backing.physical,PMM_FRAME_FREE);
 if(!slot_backing.freed || !intel_bdw_bcs_port_release(&slot_backing))return false;
#endif
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
  !gt_range_batch(phase,GT_RANGE_CANDIDATE,1) ||
#ifdef SRVROS_INTEL_GT_FLUSH
  !gt_range_batch(phase,GT_SECOND_SLOT-1,3) ||
#endif
  gt_range.count[phase]!=GT_RANGE_WORDS ||
  gt_bootstrap_native.last<=gt_range.begin[phase])return false;
#ifdef SRVROS_INTEL_GT_SLOT
 if(!gt_slot_boundary(phase) || !bootstrap_tick())return false;
#endif
 gt_range.end[phase]=gt_bootstrap_native.last;
 ++gt_range.completed;
 return true;
}
static void gt_range_report(void) {
#if defined(SRVROS_INTEL_GT_FLUSH)
 const unsigned version=8;
 air_boot_report();
 console_printf("intel-command-slots: v=2 first=%u pages=%u samples=%u reserved=%u returned=%u ram=%x boot_returned=%u mapped=%u pinned=%u observations=%u retired=%u device_grant=0\n",
  (uint64_t)GT_COMMAND_FIRST,(uint64_t)GT_COMMAND_PAGES,(uint64_t)command_slots.samples,
  (uint64_t)(command_slots.token.object!=NULL),(uint64_t)command_slots.returned,command_slots.physical,
  (uint64_t)command_slots.ram_returned,(uint64_t)command_aperture.mapped,(uint64_t)command_aperture.pinned,
  (uint64_t)command_aperture.samples,(uint64_t)command_aperture.retired);
 console_printf("intel-slot-widi: v=1 acquired=%u pages=%u closed=%u device_grant=0\n",
  (uint64_t)slot_widi.acquired,(uint64_t)slot_widi.pages,(uint64_t)slot_widi.closed);
 console_printf("intel-scanout-backing-guard: v=1 physical=%x pages=%u command_pages=%u closed=%u vmm_reason=%u\n",
  slot_widi.scanout_physical,(uint64_t)(slot_widi.acquired?GT_RANGE_FIRST+GT_RANGE_TAIL:0),
  (uint64_t)slot_widi.command_pages,(uint64_t)slot_widi.closed,(uint64_t)slot_widi.vmm_reason);
 console_printf("intel-slot-backing: v=1 physical=%x original=%x planned=%x checks=%u acquired=%u held=%u freed=%u pte_writes=0\n",
  slot_backing.physical,slot_backing.plan.original,slot_backing.plan.replacement,(uint64_t)slot_backing.checks,
  (uint64_t)slot_backing.port,(uint64_t)intel_bdw_bcs_port_held(&slot_backing),(uint64_t)slot_backing.freed);
 console_printf("intel-independent-slot: v=1 slot=%u reserved=%u returned=%u reopen_verified=%u pte_writes=0\n",
  (uint64_t)GT_SECOND_SLOT,(uint64_t)(independent_token.object!=NULL),(uint64_t)independent_returned,
  (uint64_t)independent_reopened);
#elif defined(SRVROS_INTEL_GT_SLOT)
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
