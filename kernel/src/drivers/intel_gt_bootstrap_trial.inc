/* Separate original full-GT owner. No legacy BCS reset/source is a parent.
 * CPU0 pre-init exclusion covers every kernel aperture client. The firmware
 * cooperation is an explicit boot policy, not an inference about SMM/DMA. */
#include <srvros/intel_bdw_gt_bootstrap.h>
#include <srvros/intel_pci_requester.h>
#ifdef SRVROS_INTEL_GT_FLUSH
#include <srvros/intel_bdw_bcs_port.h>
enum { PIPE_WRITER_SLOT = 10, PIPE_WRITER_PAGE = 0x70000 };
#endif
#include <stddef.h>
#if defined(SRVROS_INTEL_BCS_CONTEXT) || defined(SRVROS_INTEL_GT_FLUSH)
static bool context_wp(void)
{
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_context_wp();
#else
 uint64_t cr0;
 __asm__ volatile("mov %%cr0,%0" : "=r"(cr0) : : "memory");
 return (cr0 & (1ull<<16)) != 0;
#endif
}

static bool bootstrap_page_protect(uint64_t root,unsigned slot,bool writable) {
 if(vmm_current_address_space()!=root)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  return intel_display_test_context_protect(slot,writable);
#else
  uint64_t flags=VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE;
  if(writable)flags|=VMM_PAGE_WRITABLE;
  return vmm_protect_page_in_space(root,WAKE_VA+4096*slot,flags);
#endif
}
#endif
static struct {
 const void *object;
 struct wake_adapter *a;
 struct intel_gt_access *g;
 struct intel_gt_borrow borrow;
 struct bdw_boot_source source;
 struct intel_bdw_gt_bootstrap reset;
 struct intel_completion_result post;
 struct x86_cache_snapshot cache;
 uint64_t begin,last,end,root,epoch,clock,previous,prepare_phase,gt_origin,hda_origin,outer_origin;
 uint64_t guard_begin,guard_end,return_begin,return_end,final_begin,final_end;
 uint64_t maintain_begin,maintain_end,maintain_phase;
 unsigned maintain_before,maintain_after,maintained;
 unsigned closing;
#if defined(SRVROS_INTEL_BCS_CONTEXT) || defined(SRVROS_INTEL_GT_FLUSH)
 unsigned engine_ro_mask;
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 bool pipe_writer_writable;
#endif
 uint32_t state,reason,issued,prepared,prepare_polls[5],prepare_first[5],prepare_final[5];
 uint32_t pci[4][9],pci_counts[4],pci_samples,pci_reads,command_writes,guards,alias_checks,cache_checks;
 uint32_t native_reads,native_writes,epoch_calls;
 uint32_t display[2][3][13],display_counts[2],display_samples;
 bool blocked,full_before,full_after,post_bound,returned,complete,failed;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 const void *memory_child,*memory_original;
 unsigned memory_borrows,memory_returns;
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 const void *arena_child,*arena_original;
 unsigned arena_borrows,arena_returns;
#endif
} gt_bootstrap_native;
/* Receipt is data only; never a copied live ownership object. Valid only for
 * one immediate read across the trusted protocol's one clock-only callback. */
static struct {
 unsigned char bytes[sizeof(struct intel_bdw_gt_bootstrap)];
 uint64_t last,clock;
 unsigned proofs,attempts,samples,issued,consumed,rejected;
 bool active;
} bootstrap_admission;
static struct {
 uint64_t total[5],maximum[5],start,begin;
 unsigned calls[5],stage;
} bootstrap_cost;
_Static_assert(sizeof(bootstrap_admission.bytes)<=1792,"bounded native admission snapshot");
static struct intel_capture_result bootstrap_owned_capture;
static const uint32_t bootstrap_controls[5]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0};
static bool bootstrap_fail(unsigned reason) {
 gt_bootstrap_native.reason|=reason;gt_bootstrap_native.failed=true;
 if(gt_bootstrap_native.a)gt_bootstrap_native.a->uncertain=true;
 return false;
}
#ifdef SRVROS_INTEL_GT_FLUSH
/* The boot CPU is pinned by preinit and interrupts stay disabled throughout.
 * Count changes expose possible firmware interference. Equality grants no
 * access and cannot speak for other logical CPUs or independent requesters. */
enum bootstrap_smi_point {
 BOOT_SMI_START=1, BOOT_SMI_PCI, BOOT_SMI_ENGINE, BOOT_SMI_TRANSLATION,
 BOOT_SMI_RETURN, BOOT_SMI_END, BOOT_SMI_PARK, BOOT_SMI_PIPE,
};
enum {BOOT_SMI_UNAVAILABLE=1, BOOT_SMI_CHANGED=2, BOOT_SMI_FAULT=2048};
static struct {
 uint32_t first,last;
 unsigned reads,point,fault;
} bootstrap_smi;
static void bootstrap_smi_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_smi_cpuid(leaf,a,b,c,d);
#else
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0):"memory");
#endif
}
static bool bootstrap_smi_supported(void) {
 uint32_t a,b,c,d;
 bootstrap_smi_cpuid(0,&a,&b,&c,&d);
 if(a<1 || b!=0x756e6547 || d!=0x49656e69 || c!=0x6c65746e)return false;
 bootstrap_smi_cpuid(1,&a,&b,&c,&d);
 return a==0x306d4 && (d&(1u<<5)) && !(c&(1u<<31));
}
static uint32_t bootstrap_smi_count(enum bootstrap_smi_point point) {
 uint32_t count;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 count=intel_display_test_bootstrap_smi_count(point);
#else
 (void)point;
 uint32_t reserved;
 __asm__ volatile("lfence; rdmsr; lfence":"=a"(count),"=d"(reserved):"c"(0x34):"memory");
 (void)reserved;
#endif
 return count;
}
static bool bootstrap_smi_check(enum bootstrap_smi_point point) {
 if(bootstrap_smi.fault)return false;
 bootstrap_smi.point=point;
 if(!wake_irq_off() || (!bootstrap_smi.reads && (point!=BOOT_SMI_START || !bootstrap_smi_supported()))) {
  bootstrap_smi.fault=BOOT_SMI_UNAVAILABLE;
  return bootstrap_fail(BOOT_SMI_FAULT);
 }
 uint32_t count=bootstrap_smi_count(point);
 if(!bootstrap_smi.reads++)bootstrap_smi.first=count;
 bootstrap_smi.last=count;
 if(count==bootstrap_smi.first)return true;
 bootstrap_smi.fault=BOOT_SMI_CHANGED;
 return bootstrap_fail(BOOT_SMI_FAULT);
}
#endif
static bool gt_bootstrap_identity_command(uint32_t actual,uint32_t original) {
 return gt_bootstrap_native.object==&gt_bootstrap_native && !gt_bootstrap_native.failed &&
 gt_bootstrap_native.command_writes==1 && gt_bootstrap_native.borrow.active &&
 original==(gt_bootstrap_native.pci[0][1]&65535) && original==7 && actual==3;
}
/* Separate view of the intentional, independently admitted command halfword.
 * Never edit the boot capture or accept a different identity/resource. The
 * backing core still checks every GGTT/fence/display word and raw endpoint. */
static const struct intel_capture_result *gt_bootstrap_capture(void) {
 if(!gt_bootstrap_native.object || gt_bootstrap_native.command_writes!=1)return &result;
 if(!gt_bootstrap_identity_command(3,result.after.command) ||
  gt_bootstrap_native.pci_samples<2 || gt_bootstrap_native.pci[1][1]!=(gt_bootstrap_native.pci[0][1]&~4u))return NULL;
 bootstrap_owned_capture=result;
 bootstrap_owned_capture.after.command=bootstrap_owned_capture.before.command=3;
 return &bootstrap_owned_capture;
}
static bool bootstrap_tick(void) {
 uint64_t at=now(gt_bootstrap_native.a),old=gt_bootstrap_native.last;
 const struct intel_gt_access *g=gt_bootstrap_native.g;
 if(!at || at<old || at<gt_bootstrap_native.begin || at-gt_bootstrap_native.begin>INTEL_DIAGNOSTIC_PREPARE_US ||
 !g || at<g->last || at<g->begin || at-g->begin>INTEL_DIAGNOSTIC_GT_TOTAL_US ||
 g->begin!=gt_bootstrap_native.gt_origin || joint.scope.hda.begin_us!=gt_bootstrap_native.hda_origin ||
 joint.scope.outer_origin_us!=gt_bootstrap_native.outer_origin ||
 at<gt_bootstrap_native.hda_origin || at-gt_bootstrap_native.hda_origin>INTEL_DIAGNOSTIC_HDA_US ||
 at<gt_bootstrap_native.outer_origin || at-gt_bootstrap_native.outer_origin>INTEL_DIAGNOSTIC_OUTER_US) {
  gt_bootstrap_native.clock=at;gt_bootstrap_native.previous=old;return bootstrap_fail(2);
 }
 gt_bootstrap_native.last=at;return true;
}
static bool bootstrap_child_identity(void) {
 const struct intel_bdw_gt_bootstrap *r=&gt_bootstrap_native.reset;
 const struct bdw_boot_source *s=&gt_bootstrap_native.source;
 if(!r->object)return !s->object && !s->consumer;
 if(r->object!=r || r->source!=s || s->object!=s || s->consumer!=r)return false;
 const unsigned char *a=(const unsigned char *)&r->binding,*b=(const unsigned char *)&s->binding;
 for(unsigned i=0;i<sizeof(r->binding);i++)if(a[i]!=b[i])return false;
 if(r->stage!=BDW_BOOT_DONE)return r->stage>=BDW_BOOT_CAPTURE && r->stage<=BDW_BOOT_VERIFY &&
  r->busy && r->registered && !r->retained && !r->uncertain && !r->reason && !r->complete;
 if(!r->complete || r->busy || r->registered || r->retained || r->uncertain || r->reason ||
  !r->epoch_advanced || !r->preserved || r->reset_completed!=2 || r->reset_writes!=2 || r->cancelled!=5 ||
  r->reset_attempts!=2 || r->cancel_attempts!=5 || r->cancel_writes!=5 || r->read_attempts!=r->reads ||
  r->write_attempts!=r->writes || r->reset_initial || r->reset_post || r->reset_close ||
  r->changed_mask!=r->restored_mask || r->end!=r->last || r->end<=r->begin ||
  r->binding.owner!=gt_bootstrap_native.g || r->binding.provider!=&gt_bootstrap_native ||
  r->binding.lifetime!=&gt_bootstrap_native.borrow || r->binding.root!=gt_bootstrap_native.root ||
  r->binding.epoch!=gt_bootstrap_native.epoch || r->binding.sequence!=1 ||
  r->binding.prepare!=gt_bootstrap_native.begin || r->binding.gt!=gt_bootstrap_native.gt_origin ||
  r->binding.hda!=gt_bootstrap_native.hda_origin || r->binding.outer!=gt_bootstrap_native.outer_origin)return false;
 unsigned changed=0;
 for(unsigned i=0;i<32;i++)if(r->fence_before[i]!=r->binding.fences[i] ||
  r->binding.fences[i]!=backing.sample[1].fence[i] || r->fence_final[i]!=r->binding.fences[i] ||
  (r->fence_after[i] && r->fence_after[i]!=r->binding.fences[i]))return false;
 else if(r->fence_after[i]!=r->fence_before[i])changed|=1u<<i;
 unsigned count=0;for(unsigned i=0;i<32;i++)count+=(changed>>i)&1;
 if(changed!=r->changed_mask || r->fence_writes!=count*3 || r->writes!=7+count*3)return false;
 for(unsigned i=0;i<5;i++)if((r->control_before[i]&65535)!=3 || (r->control_after[i]&65535) ||
  r->cancel_final[i]&3 || !r->cancel_polls[i] || r->cancel_polls[i]>256)return false;
 return true;
}
static bool bootstrap_owner_binding(void) {
 const struct intel_gt_access *g=gt_bootstrap_native.g;
 return gt_bootstrap_native.object==&gt_bootstrap_native && !gt_bootstrap_native.failed &&
 gt_bootstrap_native.state==1 && g && g==joint.scope.gpu && gt_bootstrap_native.a==joint.wake &&
 gt_bootstrap_native.a && !gt_bootstrap_native.a->uncertain && !gt_bootstrap_native.a->fw.uncertain &&
 !gt_bootstrap_native.a->fw.gt.a.mapped &&
 g->object==g && g->pinned && !g->reason &&
 g->engine_epoch==gt_bootstrap_native.epoch+gt_bootstrap_native.epoch_calls &&
 gt_bootstrap_native.epoch_calls<=1 && g->io.context==gt_bootstrap_native.a &&
 g->io.guard==wake_guard && g->io.read==wake_read && g->io.write==wake_write &&
 g->io.now==now && g->io.pause==wake_pause && g->io.snapshot==wake_request_snapshot &&
 display_access_held(&g->exclusion) && display_access_held(&gt_bootstrap_native.a->fw.gt.token) &&
 bootstrap_child_identity();
}
static bool bootstrap_owner_identity(void) {
 return bootstrap_owner_binding() && gt_bootstrap_native.g->state==GT_ACCESS_AWAKE;
}
static bool bootstrap_borrow_bound(void) {
 const struct intel_gt_borrow *b=&gt_bootstrap_native.borrow;
 const struct intel_gt_access *g=gt_bootstrap_native.g;
 return b->object==b && b->active && b->parent==g &&
  !b->next && g->dependents==b && g->borrows==1;
}
static bool bootstrap_parent(void) {
 return bootstrap_owner_identity() && bootstrap_borrow_bound();
}
static bool bootstrap_after_return(void) {
 const struct intel_gt_borrow *b=&gt_bootstrap_native.borrow;
 const struct intel_gt_access *g=gt_bootstrap_native.g;
 return bootstrap_owner_identity() && gt_bootstrap_native.returned && gt_bootstrap_native.reset.complete &&
  !gt_bootstrap_native.reset.registered && gt_bootstrap_native.post_bound &&
  gt_bootstrap_native.post.object==&gt_bootstrap_native.post && gt_bootstrap_native.post.parent==g &&
  gt_bootstrap_native.post.engine_epoch==g->engine_epoch && gt_bootstrap_native.post.complete &&
  b->object==b && !b->active && b->parent==g && !g->dependents && !g->borrows &&
  vmm_current_address_space()==gt_bootstrap_native.root;
}
/* Unlike the legacy whole-backing guard, this admitted critical-section guard
 * does not interpret fences while a known full reset/restore is in progress.
 * It freshly checks the original resources, requester, HDA hold, every actual
 * UC alias and complete CPU cache context. Full GGTT/display/fence equality is
 * independently required before entry and after exact restoration. No GPU
 * completion authority is inferred during the deliberately transient state. */
static bool bootstrap_cost_end(unsigned i) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_cost_boundary(i);
#endif
 if(!bootstrap_tick())return false;
 uint64_t elapsed=gt_bootstrap_native.last-bootstrap_cost.start;
 bootstrap_cost.total[i]+=elapsed;++bootstrap_cost.calls[i];
 if(elapsed>bootstrap_cost.maximum[i])bootstrap_cost.maximum[i]=elapsed;
 bootstrap_cost.start=gt_bootstrap_native.last;bootstrap_cost.stage=i+1;return true;
}
#ifdef SRVROS_INTEL_GT_FLUSH
static bool gt_trace_request(void);
#endif
static bool bootstrap_guard(void) {
 bootstrap_admission.active=false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_boundary();
#endif
 if(!bootstrap_parent() || !bootstrap_tick() || !wake_irq_off() || trial_mapping_fault ||
 vmm_current_address_space()!=gt_bootstrap_native.root)return bootstrap_fail(4);
 bootstrap_cost.begin=bootstrap_cost.start=gt_bootstrap_native.last;bootstrap_cost.stage=0;
 if(!joint_fast() || !intel_boot_check(INTEL_BOOT_OBSERVER,gt_bootstrap_native.a->fw.gt.a.common.gpu))return bootstrap_fail(4);
 if(!bootstrap_cost_end(0))return false;
 struct intel_backing_endpoint endpoint;
 if(!backing_endpoint(&gt_bootstrap_native.a->fw.gt.a,&endpoint) || !trial_endpoint_same(&endpoint))return bootstrap_fail(4);
 if(!bootstrap_cost_end(1))return false;
 struct x86_cache_snapshot cache;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool cache_ok=intel_display_test_bootstrap_cache(&cache);
#else
 bool cache_ok=intel_cache_probe_context(&cache);
#endif
 ++gt_bootstrap_native.cache_checks;
 if(!cache_ok || !x86_cache_equal(&cache,&gt_bootstrap_native.cache))return bootstrap_fail(8);
 if(!bootstrap_cost_end(2))return false;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(gt_bootstrap_native.engine_ro_mask && !context_wp())return bootstrap_fail(8);
#endif
 for(unsigned i=0;i<WAKE_MAPS;i++) {
  unsigned m,p,e;struct vmm_alias_observation alias;struct vmm_range_observation range;
  uint64_t physical=result.mmio_base+wake_pages[i];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_bootstrap_alias(i,physical,&alias,&range);
#else
  bool ok=vmm_diagnostic_kernel_alias(gt_bootstrap_native.root,WAKE_VA+4096*i,physical,1,
   cache.cpu.physical_bits,&alias,&range);
#endif
  ++gt_bootstrap_native.alias_checks;
  bool writable=i==0 || (i>=3 && i<=7) || i==9 || i==14;
#if defined(SRVROS_INTEL_BCS_CONTEXT) || defined(SRVROS_INTEL_GT_FLUSH)
  if (i>=3 && i<=7 && (gt_bootstrap_native.engine_ro_mask & (1u<<(i-3)))) writable=false;
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
  if (i==PIPE_WRITER_SLOT && gt_bootstrap_native.pipe_writer_writable) writable=true;
#endif
  if(!gt_bootstrap_native.a->mapped[i] || !ok || alias.pages!=1 || alias.leaves!=1 || alias.pat_index!=2 ||
   alias.writable!=(unsigned)writable || alias.user || !alias.no_execute || alias.leaf_sizes!=1 ||
   !x86_cache_page(&cache,physical,alias.pat_index,&m,&p,&e) || m || e)return bootstrap_fail(8);
 }
 if(!bootstrap_cost_end(3))return false;
 ++gt_bootstrap_native.guards;
 if(!engine_requesters(gt_bootstrap_native.a))return bootstrap_fail(16);
 const struct pci_device *gpu=gt_bootstrap_native.a->fw.gt.a.common.gpu;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint32_t command=intel_display_test_bootstrap_config(4);
#else
 uint32_t command=pci_read_config32(gpu,4);
#endif
 (void)gpu;
 uint32_t expected=gt_bootstrap_native.pci[0][1];
 if(gt_bootstrap_native.command_writes==1)expected&=~4u;
 if(command!=expected)return bootstrap_fail(16);
 /* The AF payload is inaccessible until the actual full prefix has first
  * passed. Revalidate that prefix in order for every subsequent AF access. */
 if(gt_bootstrap_native.pci_samples) {
  uint32_t words[9]={0};
  for(unsigned i=0;i<9;i++) {
   if(!intel_pci_requester_prefix(words,i))return bootstrap_fail(16);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   words[i]=intel_display_test_bootstrap_config(intel_pci_requester_offsets[i]);
#else
   words[i]=pci_read_config32(gpu,intel_pci_requester_offsets[i]);
#endif
   uint32_t value=i==1?expected:gt_bootstrap_native.pci[0][i];
   if(words[i]!=value || !intel_pci_requester_prefix(words,i+1) || (i==8 && words[i]))return bootstrap_fail(16);
  }
 }
 if(!bootstrap_cost_end(4))return false;
#ifdef SRVROS_INTEL_GT_FLUSH
 return gt_trace_request();
#else
 return true;
#endif
}
static bool bootstrap_pci(unsigned n) {
 if(n!=gt_bootstrap_native.pci_samples || n>=4 || !bootstrap_guard())return false;
 uint32_t *words=gt_bootstrap_native.pci[n];
 for(unsigned i=0;i<9;i++) {
  if(!intel_pci_requester_prefix(words,i) || !bootstrap_guard())return bootstrap_fail(32);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  words[i]=intel_display_test_bootstrap_config(intel_pci_requester_offsets[i]);
#else
  words[i]=pci_read_config32(gt_bootstrap_native.a->fw.gt.a.common.gpu,intel_pci_requester_offsets[i]);
#endif
  ++gt_bootstrap_native.pci_reads;++gt_bootstrap_native.pci_counts[n];
  if(!intel_pci_requester_prefix(words,i+1) || (n && words[i]!=(gt_bootstrap_native.pci[0][i]&
   (i==1 && (n==1 || n==2)?~4u:UINT32_MAX))) || (i==8 && words[i]))return bootstrap_fail(32);
 }
 ++gt_bootstrap_native.pci_samples;return bootstrap_guard();
}
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
static bool bootstrap_memory_returned(void);
#endif
#ifdef SRVROS_INTEL_GT_ARENA
static bool command_returned(void);
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
static bool gt_slot_live(void);
static bool bootstrap_command_reservation(const struct bdw_boot_binding *);
static bool slot_queue_boundary(unsigned phase);
static bool slot_pipe_writer_open(void);
static bool slot_pipe_writer_reopen_bound(void);
#endif
static bool bootstrap_command(unsigned value) {
 unsigned issued=gt_bootstrap_native.command_writes;
 if(!((issued==0 && value==3 && gt_bootstrap_native.pci_samples==1) ||
 (issued==1 && value==7 && gt_bootstrap_native.pci_samples==3 && gt_bootstrap_native.reset.complete &&
 !gt_bootstrap_native.reset.registered && !gt_bootstrap_native.reset.retained &&
 gt_bootstrap_native.full_after && gt_bootstrap_native.post_bound)) || !bootstrap_guard())return bootstrap_fail(32);
 if(!bootstrap_tick() || !bootstrap_parent())return false;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(value==7 && !bootstrap_memory_returned())return bootstrap_fail(512);
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 if(value==7 && !command_returned())return bootstrap_fail(1024);
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 if(value==7 && (!slot_queue_boundary(0) || !slot_pipe_writer_open()))return false;
 if(!gt_slot_live() || !bootstrap_smi_check(BOOT_SMI_PCI) || !bootstrap_tick())return false;
 if(value==7 && !slot_pipe_writer_reopen_bound())return false;
#endif
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_bootstrap_command(value))return bootstrap_fail(32);
#else
 pci_write_config16(gt_bootstrap_native.a->fw.gt.a.common.gpu,4,(uint16_t)value);
#endif
 ++gt_bootstrap_native.command_writes;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(value==7 && !slot_pipe_writer_reopen_bound())return false;
#endif
 return bootstrap_guard();
}
static unsigned bootstrap_slot(uint32_t offset) {
 if(offset==BDW_BOOT_GDRST)return 9;
 if(offset>=BDW_BOOT_FENCE_BASE && offset<BDW_BOOT_FENCE_BASE+256 && !(offset&3))return 14;
 for(unsigned i=0;i<5;i++)if(offset==bootstrap_controls[i])return i+3;
 return WAKE_MAPS;
}
static bool bootstrap_load(uint32_t offset,uint32_t *value) {
 unsigned slot=bootstrap_slot(offset);
 if(!value || slot>=WAKE_MAPS || !bootstrap_guard() || !bootstrap_tick())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_bootstrap_read(offset,value);
#else
 *value=*(volatile const uint32_t *)(WAKE_VA+4096*slot+(offset&4095));bool ok=true;
#endif
 if(ok)++gt_bootstrap_native.native_reads;
 return ok && bootstrap_guard();
}
#ifdef SRVROS_INTEL_GT_FLUSH
static bool bcs_stop_at_reset(void);
#endif
static enum intel_gt_write bootstrap_store(uint32_t offset,uint32_t value,bool preparing) {
 unsigned slot=bootstrap_slot(offset);
 if(slot>=WAKE_MAPS || !bootstrap_guard())return GT_WRITE_NONE;
 if(preparing) {
  if(slot<3 || slot>7 || value!=0x10001 || slot-3!=gt_bootstrap_native.issued ||
   gt_bootstrap_native.issued!=gt_bootstrap_native.prepared || gt_bootstrap_native.reset.object ||
   gt_bootstrap_native.command_writes!=1)return GT_WRITE_NONE;
 } else if(!intel_bdw_gt_bootstrap_write_ready(&gt_bootstrap_native.reset,offset,value) ||
  gt_bootstrap_native.source.consumer!=&gt_bootstrap_native.reset ||
  !gt_bootstrap_native.reset.registered || gt_bootstrap_native.prepared!=5)return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(offset==BDW_BOOT_GDRST && !gt_bootstrap_native.reset.reset_writes && !bcs_stop_at_reset())
  return GT_WRITE_NONE;
 if(!gt_slot_live() || !bootstrap_smi_check(BOOT_SMI_ENGINE))return GT_WRITE_NONE;
#endif
 if(!bootstrap_tick() || !bootstrap_parent() || (preparing &&
  (!gt_bootstrap_native.prepare_phase || gt_bootstrap_native.last<gt_bootstrap_native.prepare_phase ||
   gt_bootstrap_native.last-gt_bootstrap_native.prepare_phase>10000)))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write written=intel_display_test_bootstrap_write(offset,value);
#else
 __asm__ volatile("mfence":::"memory");
 *(volatile uint32_t *)(WAKE_VA+4096*slot+(offset&4095))=value;
 __asm__ volatile("mfence":::"memory");enum intel_gt_write written=GT_WRITE_DONE;
#endif
 if(written==GT_WRITE_DONE){++gt_bootstrap_native.native_writes;if(preparing)++gt_bootstrap_native.issued;}
 if(written!=GT_WRITE_DONE)bootstrap_fail(64);
 return written;
}
#ifdef SRVROS_INTEL_GT_FLUSH
static bool gt_prepare_messages(unsigned engine);
#endif
static bool bootstrap_prepare(void) {
 for(unsigned i=0;i<5;i++) {
  uint32_t value;if(!bootstrap_load(bootstrap_controls[i],&value) || value==UINT32_MAX || (value&65535))return bootstrap_fail(64);
  uint64_t phase=gt_bootstrap_native.last;gt_bootstrap_native.prepare_phase=phase;
  if(bootstrap_store(bootstrap_controls[i],0x10001,true)!=GT_WRITE_DONE)return bootstrap_fail(64);
  for(unsigned n=0;n<256;n++) {
   if(!bootstrap_load(bootstrap_controls[i],&value) || gt_bootstrap_native.last-phase>10000 ||
    value==UINT32_MAX || (value&0xfffcu))return bootstrap_fail(64);
   if(!n)gt_bootstrap_native.prepare_first[i]=value;
   gt_bootstrap_native.prepare_final[i]=value;++gt_bootstrap_native.prepare_polls[i];
   if((value&3)==3){++gt_bootstrap_native.prepared;break;}
   if((value&3)!=1 || n==255)return bootstrap_fail(64);
   wake_pause(gt_bootstrap_native.a);
  }
#ifdef SRVROS_INTEL_GT_FLUSH
  if(!gt_prepare_messages(i+1))return bootstrap_fail(64);
#endif
 }
 return gt_bootstrap_native.prepared==5;
}
static bool bootstrap_display(unsigned n) {
 static const unsigned offsets[13]={0x180,0x19c,0x1ac,0x1b0,0x1bc,0x80,0x84,0xac,0x280,0x29c,0x2ac,0x2b0,0x2bc};
 if(n!=gt_bootstrap_native.display_samples || n>=2 || !bootstrap_guard())return false;
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<13;i++) {
  if(!bootstrap_guard() || !completion_mapping(gt_bootstrap_native.a,10+p))return false;
  uint32_t value;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_bootstrap_display(p,i,&value))return false;
#else
  value=*(volatile const uint32_t *)(WAKE_VA+(10+p)*4096+offsets[i]);
#endif
  gt_bootstrap_native.display[n][p][i]=value;++gt_bootstrap_native.display_counts[n];
  if(value==UINT32_MAX || (n && value!=gt_bootstrap_native.display[0][p][i]))return false;
  const uint32_t *old=result.sample[1].pipe[p];
  if((i==0 && value!=old[2]) || (i==1 && value!=old[4]) || (i==2 && value!=old[6]) ||
   (i==5 && value!=old[7]) || (i==6 && value!=old[8]))return false;
 }
 (void)offsets;++gt_bootstrap_native.display_samples;return bootstrap_guard();
}
static bool bootstrap_core_io(void);
static bool bootstrap_proof(void *ctx,const struct bdw_boot_source *source,struct bdw_boot_proof *out) {
 if(ctx!=&gt_bootstrap_native || source!=&gt_bootstrap_native.source || !out || !bootstrap_guard() ||
 gt_bootstrap_native.command_writes!=1 || gt_bootstrap_native.prepared!=5 || !bootstrap_core_io())return false;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!intel_bdw_bcs_port_held(source->binding.blitter_port_owner) ||
    !bootstrap_command_reservation(&source->binding))return false;
#endif
 *out=(struct bdw_boot_proof){source,gt_bootstrap_native.g,&gt_bootstrap_native,&gt_bootstrap_native.borrow,
  gt_bootstrap_native.root,gt_bootstrap_native.g->engine_epoch,1,gt_bootstrap_native.last,BDW_BOOT_REQUIRED};
 const struct intel_bdw_gt_bootstrap *r=&gt_bootstrap_native.reset;
 const unsigned char *bytes=(const unsigned char *)r;
 for(unsigned i=0;i<sizeof(*r);i++)bootstrap_admission.bytes[i]=bytes[i];
 bootstrap_admission.last=r->last;bootstrap_admission.proofs=r->proofs;
 bootstrap_admission.attempts=r->read_attempts;bootstrap_admission.samples=0;
 bootstrap_admission.clock=0;bootstrap_admission.active=true;++bootstrap_admission.issued;
 return true;
}
static bool bootstrap_receipt_field(unsigned offset,const void *value,unsigned bytes) {
 const unsigned char *expected=value;
 for(unsigned i=0;i<bytes;i++)if(bootstrap_admission.bytes[offset+i]!=expected[i])return false;
 return true;
}
static bool bootstrap_read_receipt(void) {
 struct intel_bdw_gt_bootstrap *r=&gt_bootstrap_native.reset;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_receipt_boundary();
#endif
 bool active=bootstrap_admission.active;bootstrap_admission.active=false;
 if(!active || bootstrap_admission.samples!=1 ||
 !bootstrap_receipt_field(offsetof(struct intel_bdw_gt_bootstrap,last),&bootstrap_admission.last,sizeof(r->last)) ||
 !bootstrap_receipt_field(offsetof(struct intel_bdw_gt_bootstrap,proofs),&bootstrap_admission.proofs,sizeof(r->proofs)) ||
 !bootstrap_receipt_field(offsetof(struct intel_bdw_gt_bootstrap,read_attempts),&bootstrap_admission.attempts,sizeof(r->read_attempts)) ||
 r->last!=bootstrap_admission.clock || r->last<bootstrap_admission.last ||
 bootstrap_admission.proofs==UINT32_MAX || r->proofs!=bootstrap_admission.proofs+1 ||
 bootstrap_admission.attempts==UINT32_MAX || r->read_attempts!=bootstrap_admission.attempts+1)
  goto refuse;
 const unsigned char *bytes=(const unsigned char *)r;
 for(unsigned i=0;i<sizeof(*r);i++) {
  /* These are the ONLY trusted pure-core updates since prerequisite return.
   * Every other byte (including vectors, source and callback tables) is sealed. */
  if((i>=offsetof(struct intel_bdw_gt_bootstrap,last) && i<offsetof(struct intel_bdw_gt_bootstrap,last)+sizeof(r->last)) ||
   (i>=offsetof(struct intel_bdw_gt_bootstrap,proofs) && i<offsetof(struct intel_bdw_gt_bootstrap,proofs)+sizeof(r->proofs)) ||
   (i>=offsetof(struct intel_bdw_gt_bootstrap,read_attempts) && i<offsetof(struct intel_bdw_gt_bootstrap,read_attempts)+sizeof(r->read_attempts)))continue;
  if(bytes[i]!=bootstrap_admission.bytes[i])goto refuse;
 }
 if(!bootstrap_parent() || !bootstrap_core_io() || !wake_irq_off() || !intel_hda_gpu_live(&joint.scope) || joint.h.uncertain || !joint.h.mapped ||
 trial_mapping_fault || vmm_current_address_space()!=gt_bootstrap_native.root || !bootstrap_tick())goto refuse;
 ++bootstrap_admission.consumed;return true;
refuse:
 ++bootstrap_admission.rejected;return bootstrap_fail(256);
}
static bool bootstrap_read(void *ctx,uint32_t offset,uint32_t *value) {
 unsigned slot=bootstrap_slot(offset);
 if(ctx!=&gt_bootstrap_native || !value || slot>=WAKE_MAPS || !bootstrap_read_receipt())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_bootstrap_read(offset,value);
#else
 *value=*(volatile const uint32_t *)(WAKE_VA+4096*slot+(offset&4095));bool ok=true;
#endif
 if(ok)++gt_bootstrap_native.native_reads;
 /* The unchanged core immediately rechecks its snapshot and performs a fresh
  * full prerequisite admission AFTER this callback. No GPU access intervenes. */
 return ok && bootstrap_parent() && wake_irq_off() && !trial_mapping_fault &&
  vmm_current_address_space()==gt_bootstrap_native.root && bootstrap_tick();
}
static enum intel_gt_write bootstrap_write(void *ctx,uint32_t offset,uint32_t value) {
 return ctx==&gt_bootstrap_native?bootstrap_store(offset,value,false):GT_WRITE_NONE;
}
static bool bootstrap_epoch(void *ctx,const struct bdw_boot_source *source,uint64_t before,uint64_t after) {
 if(ctx!=&gt_bootstrap_native || source!=&gt_bootstrap_native.source || !bootstrap_guard() ||
 gt_bootstrap_native.epoch_calls || before!=gt_bootstrap_native.epoch || after!=before+1 ||
 gt_bootstrap_native.g->engine_epoch!=before || gt_bootstrap_native.prepared!=5 ||
 !gt_bootstrap_native.reset.registered || source->consumer!=&gt_bootstrap_native.reset ||
 gt_bootstrap_native.reset.stage!=BDW_BOOT_EPOCH || !gt_bootstrap_native.reset.epoch_attempted ||
 gt_bootstrap_native.reset.epoch_advanced || gt_bootstrap_native.reset.write_attempts)return false;
 /* Full backing is still original here: all fences were captured/verified,
  * no reset or restoration store has been attempted. Service the ORIGINAL
  * lease through its original snapshot/guard/table, not a timestamp refresh.
  * Never invoke that full guard in the transient reset/restore interval. */
 struct intel_gt_access *g=gt_bootstrap_native.g;
 gt_bootstrap_native.maintain_begin=gt_bootstrap_native.last;
 gt_bootstrap_native.maintain_phase=g->last;
 gt_bootstrap_native.maintain_before=g->snapshots;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_maintain_boundary(0);
#endif
 bool maintained=intel_gt_access_check(g);
 gt_bootstrap_native.maintain_end=g->last;
 gt_bootstrap_native.maintain_after=g->snapshots;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_maintain_boundary(1);
#endif
 if(!maintained || !bootstrap_guard() || !bootstrap_core_io() ||
 g->engine_epoch!=before || gt_bootstrap_native.reset.stage!=BDW_BOOT_EPOCH ||
 !gt_bootstrap_native.reset.epoch_attempted || gt_bootstrap_native.reset.epoch_advanced ||
 gt_bootstrap_native.reset.write_attempts || gt_bootstrap_native.maintain_before==UINT32_MAX ||
 gt_bootstrap_native.maintain_after!=gt_bootstrap_native.maintain_before+1 ||
 gt_bootstrap_native.maintain_end<=gt_bootstrap_native.maintain_phase ||
 gt_bootstrap_native.maintain_end<gt_bootstrap_native.maintain_begin ||
 gt_bootstrap_native.maintain_end-gt_bootstrap_native.maintain_phase>GT_ACCESS_PHASE_US)return false;
 gt_bootstrap_native.maintained=1;
 ++gt_bootstrap_native.g->engine_epoch;++gt_bootstrap_native.epoch_calls;return true;
}
static bool bootstrap_preserved(void *ctx,const struct intel_bdw_gt_bootstrap *r) {
 if(ctx!=&gt_bootstrap_native || r!=&gt_bootstrap_native.reset || !bootstrap_guard() || r->cancelled!=5 ||
 r->reset_completed!=2 || r->reset_writes!=2 || r->restored_mask!=r->changed_mask ||
 r->epoch_advanced!=true || !engine_guard(gt_bootstrap_native.a) || !intel_scanout_trial_revalidate() ||
 !bootstrap_display(1))return false;
 gt_bootstrap_native.full_after=true;return bootstrap_guard();
}
static uint64_t bootstrap_now(void *ctx) {
 if(ctx!=&gt_bootstrap_native)return 0;
 uint64_t at=now(gt_bootstrap_native.a);
 if(bootstrap_admission.active){++bootstrap_admission.samples;bootstrap_admission.clock=at;}
 return at;
}
static void bootstrap_pause(void *ctx) {if(ctx==&gt_bootstrap_native)wake_pause(gt_bootstrap_native.a);}
static bool bootstrap_core_io(void) {
 const struct bdw_boot_io *io=&gt_bootstrap_native.reset.io,*old=&gt_bootstrap_native.reset.original_io;
 return io->ctx==&gt_bootstrap_native && io->prerequisite==bootstrap_proof && io->read==bootstrap_read &&
 io->write==bootstrap_write && io->advance_epoch==bootstrap_epoch && io->preserved==bootstrap_preserved &&
 io->now==bootstrap_now && io->pause==bootstrap_pause && old->ctx==io->ctx &&
 old->prerequisite==io->prerequisite && old->read==io->read && old->write==io->write &&
 old->advance_epoch==io->advance_epoch && old->preserved==io->preserved && old->now==io->now && old->pause==io->pause;
}
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
#if defined(SRVROS_INTEL_GT_MEMORY_ADAPTER_FILE) && !defined(SRVROS_INTEL_DISPLAY_HOST_TEST)
#error "A full-GT memory adapter override is host-test-only"
#endif
#ifndef SRVROS_INTEL_GT_MEMORY_ADAPTER_FILE
#define SRVROS_INTEL_GT_MEMORY_ADAPTER_FILE "intel_gt_memory_trial.inc"
#endif
#include SRVROS_INTEL_GT_MEMORY_ADAPTER_FILE
#endif
#ifdef SRVROS_INTEL_GT_RANGE
#if !defined(SRVROS_INTEL_GT_MEMORY_PREPARE) || defined(SRVROS_INTEL_GT_ARENA)
#error "Display translation range requires the isolated original GT memory owner"
#endif
#include "intel_gt_range_trial.inc"
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
#ifndef SRVROS_INTEL_GT_RANGE
#error "Live primary geometry requires the held translation comparison"
#endif
#include "intel_gt_primary_trial.inc"
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
#ifndef SRVROS_INTEL_GT_AUX
#error "Flush acknowledgements require the original held display-reader trial"
#endif
#include "intel_gt_flush_trial.inc"
#endif
#ifdef SRVROS_INTEL_GT_ARENA
#if !defined(SRVROS_INTEL_GT_MEMORY_PREPARE) || defined(SRVROS_INTEL_COMMAND_MEMORY)
#error "Private full-GT arena needs its actual memory child, never the legacy source"
#endif
#if defined(SRVROS_INTEL_GT_ARENA_ADAPTER_FILE) && !defined(SRVROS_INTEL_DISPLAY_HOST_TEST)
#error "A full-GT arena adapter override is host-test-only"
#endif
#ifndef SRVROS_INTEL_GT_ARENA_ADAPTER_FILE
#define SRVROS_INTEL_GT_ARENA_ADAPTER_FILE "intel_gt_arena_trial.inc"
#endif
#include SRVROS_INTEL_GT_ARENA_ADAPTER_FILE
#endif
static void bootstrap_run(struct wake_adapter *a,struct intel_gt_access *g) {
 if(gt_bootstrap_native.object)return;
 gt_bootstrap_native.object=&gt_bootstrap_native;gt_bootstrap_native.a=a;gt_bootstrap_native.g=g;
 gt_bootstrap_native.root=a->fw.gt.a.common.cr3;gt_bootstrap_native.epoch=g->engine_epoch;
 gt_bootstrap_native.gt_origin=g->begin;gt_bootstrap_native.hda_origin=joint.scope.hda.begin_us;
 gt_bootstrap_native.outer_origin=joint.scope.outer_origin_us;
 gt_bootstrap_native.begin=gt_bootstrap_native.last=now(a);gt_bootstrap_native.state=1;
 if(!ring_native.result.complete || ring_native.result.borrow.active || !ring_native.result.post.complete ||
 ring_native.result.post.engine_epoch!=g->engine_epoch || !ring_native.result.post.stable ||
 ring_native.result.post.reason || ring_native.result.post.enabled || ring_native.result.post.pending ||
 ring_native.result.post.flip_wait || ring_native.result.post.tlb_pending ||
 gt_bootstrap_native.begin<ring_native.result.post.end_us ||
 gt_bootstrap_native.begin-ring_native.result.post.end_us>100000)goto fail;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!bootstrap_smi_check(BOOT_SMI_START))goto fail;
#endif
 if(!intel_gt_access_borrow(g,&gt_bootstrap_native.borrow))goto fail;
 unsigned attempts=g->write_attempts;
 gt_bootstrap_native.blocked=!intel_gt_access_close(g) && g->write_attempts==attempts;
 if(!gt_bootstrap_native.blocked || !engine_guard(a) || !intel_scanout_trial_revalidate())goto fail;
 gt_bootstrap_native.full_before=true;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_bootstrap_cache(&gt_bootstrap_native.cache))goto fail;
#else
 if(!intel_cache_probe_context(&gt_bootstrap_native.cache))goto fail;
#endif
 /* Initial command is independently read before the first guarded PCI sample. */
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 gt_bootstrap_native.pci[0][1]=intel_display_test_bootstrap_config(4);
#else
 gt_bootstrap_native.pci[0][1]=pci_read_config32(a->fw.gt.a.common.gpu,4);
#endif
 if((gt_bootstrap_native.pci[0][1]&65535)!=7 || !bootstrap_pci(0) || !bootstrap_display(0))goto fail;
#ifdef SRVROS_INTEL_GT_RANGE
 if(!gt_range_capture(0))goto fail;
#endif
 if(!bootstrap_command(3) || !bootstrap_pci(1))goto fail;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!gt_flush_sample(0) || !park_before_prepare(a,g) || !bootstrap_tick())goto fail;
 air_park.prepare_us=gt_bootstrap_native.last;
 if(air_park.prepare_us<air_park.returned_us || !bcs_stop_request() || !gt_prepare_messages(0))goto fail;
#endif
 if(!bootstrap_prepare())goto fail;
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!gt_flush_sample(1))goto fail;
#endif
 struct bdw_boot_binding *binding=&gt_bootstrap_native.source.binding;
 *binding=(struct bdw_boot_binding){.owner=g,.provider=&gt_bootstrap_native,.lifetime=&gt_bootstrap_native.borrow,
  .root=gt_bootstrap_native.root,.mmio=result.mmio_base,.mmio_bytes=0x200000,
  .aperture_bytes=window_result.aperture_bytes,.epoch=gt_bootstrap_native.epoch,.sequence=1,
  .prepare=gt_bootstrap_native.begin,.gt=gt_bootstrap_native.gt_origin,.hda=gt_bootstrap_native.hda_origin,.outer=gt_bootstrap_native.outer_origin,
  .vendor=0x8086,.device=0x1626,.policy=BDW_BOOT_POLICY};
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!slot_backing.port || !intel_bdw_bcs_port_held(&slot_backing))goto fail;
 binding->blitter_port_owner=&slot_backing;
 binding->command_pages=&command_slots.transfer;
 binding->command_pages_epoch=command_slots.token.epoch;
#endif
 for(unsigned i=0;i<32;i++)binding->fences[i]=backing.sample[1].fence[i];
 if(!intel_bdw_gt_bootstrap_run(&gt_bootstrap_native.reset,&gt_bootstrap_native.source,
  (struct bdw_boot_io){&gt_bootstrap_native,bootstrap_proof,bootstrap_read,bootstrap_write,bootstrap_epoch,
   bootstrap_preserved,bootstrap_now,bootstrap_pause}))goto fail;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!bcs_stop_after_reset() || !gt_flush_sample(2))goto fail;
#endif
 if(!bootstrap_memory_open())goto fail;
#endif
 if(!bootstrap_guard() || !intel_completion_observe(&gt_bootstrap_native.post,g,
  (struct intel_completion_io){a,engine_guard,completion_group,completion_context,now,COMPLETION_SAMPLE}))goto fail;
 gt_bootstrap_native.post_bound=intel_bdw_reset_epoch_live(&gt_bootstrap_native.reset,&gt_bootstrap_native.post);
 if(!gt_bootstrap_native.post_bound)goto fail;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(!bootstrap_memory_run() || !bootstrap_memory_returned())goto fail;
#endif
#ifdef SRVROS_INTEL_GT_RANGE
 if(!gt_range_capture(1))goto fail;
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
 if(!gt_primary_capture())goto fail;
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!gt_flush_sample(3))goto fail;
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 if(!command_run() || !command_returned())goto fail;
#endif
 if(!bootstrap_pci(2))goto fail;
#if defined(SRVROS_INTEL_GT_SLOT) && !defined(SRVROS_INTEL_GT_FLUSH)
 if(!gt_slot_return())goto fail;
#endif
 if(!bootstrap_command(7) || !bootstrap_pci(3))goto fail;
#ifdef SRVROS_INTEL_GT_FLUSH
 independent_reopened=true;
 if(!gt_slot_return())goto fail;
#endif
 gt_bootstrap_native.guard_begin=gt_bootstrap_native.last;
 if(!bootstrap_guard() || !engine_guard(a) || !bootstrap_tick() || gt_bootstrap_native.reset.registered ||
  gt_bootstrap_native.reset.retained || !gt_bootstrap_native.full_after)goto fail;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(!bootstrap_memory_returned())goto fail;
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 if(!command_returned())goto fail;
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!bootstrap_smi_check(BOOT_SMI_RETURN) || !bootstrap_tick())goto fail;
#endif
 gt_bootstrap_native.guard_end=gt_bootstrap_native.return_begin=gt_bootstrap_native.last;
 gt_bootstrap_native.closing=1;
 gt_bootstrap_native.returned=intel_gt_access_return(&gt_bootstrap_native.borrow);
 if(!gt_bootstrap_native.returned || gt_bootstrap_native.borrow.active || g->borrows || g->dependents ||
  !bootstrap_tick())goto fail;
 gt_bootstrap_native.return_end=gt_bootstrap_native.final_begin=gt_bootstrap_native.last;
 gt_bootstrap_native.closing=2;
 if(!engine_guard(a))goto fail;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_bootstrap_final_boundary();
#endif
 if(!bootstrap_after_return() || !bootstrap_tick())goto fail;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(!bootstrap_memory_returned())goto fail;
#endif
#ifdef SRVROS_INTEL_GT_ARENA
 if(!command_returned())goto fail;
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!bootstrap_smi_check(BOOT_SMI_END) || !bootstrap_tick())goto fail;
#endif
 gt_bootstrap_native.final_end=gt_bootstrap_native.last;gt_bootstrap_native.closing=3;
 gt_bootstrap_native.end=gt_bootstrap_native.last;gt_bootstrap_native.state=2;gt_bootstrap_native.complete=true;return;
fail:
 bootstrap_fail(128);
}
static void bootstrap_report(void) {
 const struct intel_bdw_gt_bootstrap *r=&gt_bootstrap_native.reset;
 console_printf(
#if defined(SRVROS_INTEL_GT_FLUSH)
 "intel-bootstrap-summary: v=12 "
#elif defined(SRVROS_INTEL_GT_AUX)
 "intel-bootstrap-summary: v=11 "
#elif defined(SRVROS_INTEL_GT_PRIMARY)
 "intel-bootstrap-summary: v=10 "
#elif defined(SRVROS_INTEL_GT_RANGE)
 "intel-bootstrap-summary: v=9 "
#elif defined(SRVROS_INTEL_BCS_CONTEXT)
 "intel-bootstrap-summary: v=8 "
#elif defined(SRVROS_INTEL_GT_ARENA)
 "intel-bootstrap-summary: v=7 "
#elif defined(SRVROS_INTEL_GT_MEMORY_PREPARE)
 "intel-bootstrap-summary: v=6 "
#elif defined(SRVROS_INTEL_GT_PRACTICAL)
 "intel-bootstrap-summary: v=5 "
#elif defined(SRVROS_INTEL_GT_SECONDS)
 "intel-bootstrap-summary: v=4 "
#else
 "intel-bootstrap-summary: v=3 "
#endif
 "state=%u reason=%u complete=%u failed=%u blocked=%u returned=%u active=%u epoch_before=%u epoch_after=%u epoch_calls=%u begin=%u end=%u last=%u root=%x prepare_budget=%u prepared=%u issued=%u commands=%u pci_samples=%u pci_reads=%u guards=%u alias_checks=%u cache_checks=%u native_reads=%u native_writes=%u full_before=%u full_after=%u post_bound=%u clock=%u previous=%u obligations=255 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
  (uint64_t)gt_bootstrap_native.state,(uint64_t)gt_bootstrap_native.reason,(uint64_t)gt_bootstrap_native.complete,
  (uint64_t)gt_bootstrap_native.failed,(uint64_t)gt_bootstrap_native.blocked,(uint64_t)gt_bootstrap_native.returned,
  (uint64_t)gt_bootstrap_native.borrow.active,gt_bootstrap_native.epoch,
  gt_bootstrap_native.g?(uint64_t)gt_bootstrap_native.g->engine_epoch:0,(uint64_t)gt_bootstrap_native.epoch_calls,
  gt_bootstrap_native.begin,gt_bootstrap_native.end,gt_bootstrap_native.last,gt_bootstrap_native.root,
  INTEL_DIAGNOSTIC_PREPARE_US,(uint64_t)gt_bootstrap_native.prepared,(uint64_t)gt_bootstrap_native.issued,
  (uint64_t)gt_bootstrap_native.command_writes,(uint64_t)gt_bootstrap_native.pci_samples,(uint64_t)gt_bootstrap_native.pci_reads,
  (uint64_t)gt_bootstrap_native.guards,(uint64_t)gt_bootstrap_native.alias_checks,(uint64_t)gt_bootstrap_native.cache_checks,
  (uint64_t)gt_bootstrap_native.native_reads,(uint64_t)gt_bootstrap_native.native_writes,
  (uint64_t)gt_bootstrap_native.full_before,(uint64_t)gt_bootstrap_native.full_after,(uint64_t)gt_bootstrap_native.post_bound,
  gt_bootstrap_native.clock,gt_bootstrap_native.previous);
 console_printf("intel-bootstrap-binding: v=1 owner=%u provider=%u lifetime=%u root=%x mmio=%x mmio_bytes=%u aperture_bytes=%u epoch=%u sequence=%u prepare=%u gt=%u hda=%u outer=%u policy=%x vendor=%x device=%x display_samples=%u display_before=%u display_after=%u pci0=%u pci1=%u pci2=%u pci3=%u post_epoch=%u post_parent=%u post_object=%u\n",
  (uint64_t)(r->binding.owner && r->binding.owner==gt_bootstrap_native.g),(uint64_t)(r->binding.provider==&gt_bootstrap_native),
  (uint64_t)(r->binding.lifetime==&gt_bootstrap_native.borrow),r->binding.root,r->binding.mmio,
  r->binding.mmio_bytes,r->binding.aperture_bytes,r->binding.epoch,r->binding.sequence,
  r->binding.prepare,r->binding.gt,r->binding.hda,r->binding.outer,(uint64_t)r->binding.policy,
  (uint64_t)r->binding.vendor,(uint64_t)r->binding.device,(uint64_t)gt_bootstrap_native.display_samples,
  (uint64_t)gt_bootstrap_native.display_counts[0],(uint64_t)gt_bootstrap_native.display_counts[1],
  (uint64_t)gt_bootstrap_native.pci_counts[0],(uint64_t)gt_bootstrap_native.pci_counts[1],
  (uint64_t)gt_bootstrap_native.pci_counts[2],(uint64_t)gt_bootstrap_native.pci_counts[3],
  (uint64_t)gt_bootstrap_native.post.engine_epoch,
  (uint64_t)(gt_bootstrap_native.post.parent && gt_bootstrap_native.post.parent==gt_bootstrap_native.g),
  (uint64_t)(gt_bootstrap_native.post.object==&gt_bootstrap_native.post));
 console_printf("intel-bootstrap-reset: v=1 stage=%u reason=%u complete=%u registered=%u retained=%u uncertain=%u epoch=%u reset_attempts=%u reset_writes=%u reset_completed=%u cancelled=%u changed=%x restored=%x reads=%u writes=%u proofs=%u preserved=%u begin=%u end=%u last=%u settle_begin=%u settle_end=%u clock=%u clock_previous=%u clock_reason=%u\n",
  (uint64_t)r->stage,(uint64_t)r->reason,(uint64_t)r->complete,(uint64_t)r->registered,(uint64_t)r->retained,
  (uint64_t)r->uncertain,(uint64_t)r->epoch_advanced,(uint64_t)r->reset_attempts,(uint64_t)r->reset_writes,
  (uint64_t)r->reset_completed,(uint64_t)r->cancelled,(uint64_t)r->changed_mask,(uint64_t)r->restored_mask,
  (uint64_t)r->reads,(uint64_t)r->writes,(uint64_t)r->proofs,(uint64_t)r->preserved,r->begin,r->end,r->last,
  r->settle_begin,r->settle_end,r->clock_at,r->clock_previous,(uint64_t)r->clock_reason);
 console_printf("intel-bootstrap-retire: v=1 stage=%u guard_begin=%u guard_end=%u return_begin=%u return_end=%u final_begin=%u final_end=%u\n",
  (uint64_t)gt_bootstrap_native.closing,gt_bootstrap_native.guard_begin,gt_bootstrap_native.guard_end,
  gt_bootstrap_native.return_begin,gt_bootstrap_native.return_end,gt_bootstrap_native.final_begin,gt_bootstrap_native.final_end);
#if defined(SRVROS_INTEL_GT_SECONDS) || defined(SRVROS_INTEL_GT_PRACTICAL)
 console_printf("intel-bootstrap-timing: v=1 diagnostic=1 prepare=%u gt_phase=%u gt=%u hda=%u outer=%u hardware_phase=%u poll_cap=%u gt_end=%u hda_end=%u outer_end=%u\n",
 INTEL_DIAGNOSTIC_PREPARE_US,GT_ACCESS_PHASE_US,GT_ACCESS_TOTAL_US,INTEL_DIAGNOSTIC_HDA_US,INTEL_DIAGNOSTIC_OUTER_US,BDW_BOOT_PHASE_US,(uint64_t)BDW_BOOT_POLLS,gt_bootstrap_native.g?gt_bootstrap_native.g->last:0,joint.scope.hda.end_us,joint.scope.end_us);
#endif
 console_printf("intel-bootstrap-maintain: v=1 complete=%u begin=%u end=%u phase=%u before=%u after=%u\n",
  (uint64_t)gt_bootstrap_native.maintained,gt_bootstrap_native.maintain_begin,gt_bootstrap_native.maintain_end,
  gt_bootstrap_native.maintain_phase,(uint64_t)gt_bootstrap_native.maintain_before,(uint64_t)gt_bootstrap_native.maintain_after);
 console_printf("intel-bootstrap-admission: v=1 issued=%u consumed=%u rejected=%u active=%u stage=%u begin=%u partial_begin=%u\n",
  (uint64_t)bootstrap_admission.issued,(uint64_t)bootstrap_admission.consumed,(uint64_t)bootstrap_admission.rejected,
  (uint64_t)bootstrap_admission.active,(uint64_t)bootstrap_cost.stage,bootstrap_cost.begin,bootstrap_cost.start);
 for(unsigned i=0;i<5;i++)console_printf("intel-bootstrap-cost: v=1 i=%u calls=%u total=%u max=%u\n",
  (uint64_t)i,(uint64_t)bootstrap_cost.calls[i],bootstrap_cost.total[i],bootstrap_cost.maximum[i]);
 for(unsigned i=0;i<32;i++)console_printf("intel-bootstrap-fence: v=1 i=%u before=%x after=%x final=%x\n",
  (uint64_t)i,r->fence_before[i],r->fence_after[i],r->fence_final[i]);
 for(unsigned i=0;i<5;i++)console_printf("intel-bootstrap-control: v=1 i=%u prepare_first=%x prepare_final=%x prepare_polls=%u before=%x after=%x cancel_first=%x cancel_final=%x cancel_polls=%u\n",
  (uint64_t)i,(uint64_t)gt_bootstrap_native.prepare_first[i],(uint64_t)gt_bootstrap_native.prepare_final[i],
  (uint64_t)gt_bootstrap_native.prepare_polls[i],(uint64_t)r->control_before[i],(uint64_t)r->control_after[i],
  (uint64_t)r->cancel_first[i],(uint64_t)r->cancel_final[i],(uint64_t)r->cancel_polls[i]);
 for(unsigned i=0;i<2;i++)console_printf("intel-bootstrap-pulse: v=1 i=%u first=%x final=%x polls=%u\n",
  (uint64_t)i,(uint64_t)r->reset_first[i],(uint64_t)r->reset_final[i],(uint64_t)r->reset_polls[i]);
 for(unsigned n=0;n<4;n++)for(unsigned i=0;i<gt_bootstrap_native.pci_counts[n] && i<9;i++)
  console_printf("intel-bootstrap-pci: v=1 n=%u i=%u raw=%x\n",(uint64_t)n,(uint64_t)i,(uint64_t)gt_bootstrap_native.pci[n][i]);
 for(unsigned n=0;n<2;n++)for(unsigned k=0;k<gt_bootstrap_native.display_counts[n] && k<39;k++) {
  unsigned p=k/13,i=k%13;
  console_printf("intel-bootstrap-display: v=1 n=%u p=%u i=%u raw=%x\n",(uint64_t)n,(uint64_t)p,(uint64_t)i,
   (uint64_t)gt_bootstrap_native.display[n][p][i]);
 }
 completion_report_named(&gt_bootstrap_native.post,"intel-bootstrap-post");
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 bootstrap_memory_report();
#endif
#ifdef SRVROS_INTEL_GT_RANGE
 gt_range_report();
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
 gt_primary_report();
#endif
#ifdef SRVROS_INTEL_GT_AUX
 gt_aux_report();
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 gt_flush_report();
 console_printf("intel-smi-guard: v=1 reads=%u point=%u fault=%u first=%u last=%u authority=none\n",
  (uint64_t)bootstrap_smi.reads,(uint64_t)bootstrap_smi.point,(uint64_t)bootstrap_smi.fault,
  (uint64_t)bootstrap_smi.first,(uint64_t)bootstrap_smi.last);
#endif
}
