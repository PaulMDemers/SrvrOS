/* Actual original native owner with independent PCI/MMIO/alias/cache models. */
#include <stdint.h>
#include <stdbool.h>
#include <srvros/x86_cache.h>
#include <srvros/vmm.h>
#include <srvros/intel_gt_access.h>
static void intel_display_test_bootstrap_boundary(void);
static bool intel_display_test_bootstrap_cache(struct x86_cache_snapshot *);
static bool intel_display_test_bootstrap_alias(unsigned,uint64_t,struct vmm_alias_observation *,struct vmm_range_observation *);
static uint32_t intel_display_test_bootstrap_config(unsigned);
static uint32_t intel_display_test_bootstrap_legacy_config(unsigned);
static bool intel_display_test_bootstrap_command(unsigned);
static bool intel_display_test_bootstrap_read(uint32_t,uint32_t *);
static enum intel_gt_write intel_display_test_bootstrap_write(uint32_t,uint32_t);
static bool intel_display_test_bootstrap_expected(void);
static bool intel_display_test_bootstrap_display(unsigned,unsigned,uint32_t *);
static bool intel_display_test_bootstrap_clock(uint64_t *);
static void intel_display_test_bootstrap_final_boundary(void);
static void intel_display_test_bootstrap_receipt_boundary(void);
static void intel_display_test_bootstrap_cost_boundary(unsigned);
static void intel_display_test_bootstrap_maintain_boundary(unsigned);
#ifdef SRVROS_INTEL_GT_FLUSH
static unsigned model_engine_ro;
static bool intel_display_test_context_wp(void);
static bool intel_display_test_context_protect(unsigned,bool);
static void intel_display_test_backing_access(void);
static void intel_display_test_bootstrap_smi_cpuid(uint32_t,uint32_t *,uint32_t *,uint32_t *,uint32_t *);
static uint32_t intel_display_test_bootstrap_smi_count(unsigned);
#endif
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
static void intel_display_test_memory_boundary(void);
static bool intel_display_test_memory_translate(unsigned,uint64_t *);
static bool intel_display_test_memory_alias(unsigned,struct vmm_alias_observation *);
static bool intel_display_test_memory_resource(unsigned *);
static bool intel_display_test_memory_read(unsigned,uint32_t,uint32_t *);
static enum intel_gt_write intel_display_test_memory_write(unsigned,uint32_t,uint32_t);
static bool intel_display_test_memory_map(unsigned,uint64_t);
static bool intel_display_test_memory_unmap(unsigned,uint64_t *);
#endif
#ifdef SRVROS_INTEL_GT_ARENA
static uint64_t intel_display_test_command_alloc(void);
static bool intel_display_test_command_backing(uint64_t,uint32_t);
static bool intel_display_test_command_free(uint64_t);
static bool intel_display_test_command_map(uint64_t,uint64_t,uint64_t);
static bool intel_display_test_command_unmap(uint64_t,uint64_t,uint64_t *);
static bool intel_display_test_command_cache(struct x86_cache_snapshot *);
static bool intel_display_test_command_alias(uint64_t,uint64_t,uint64_t,struct vmm_alias_observation *);
static volatile uint64_t *intel_display_test_command_pointer(void);
static void intel_display_test_command_preserved(void);
#ifdef SRVROS_INTEL_BCS_CONTEXT
static unsigned model_engine_ro;
struct vmm_alias_span;
static bool intel_display_test_context_aliases(const struct vmm_alias_span *,unsigned);
static bool intel_display_test_context_protect(unsigned,bool);
static bool intel_display_test_context_widi_absent(uint64_t);
static bool intel_display_test_context_wp(void);
static uint32_t intel_display_test_context_traffic(unsigned);
static void intel_display_test_context_boundary(void);
static bool intel_display_test_context_pte(uint32_t,uint32_t *);
#endif

static uint64_t intel_display_test_command_clock(void);
static void intel_display_test_command_guard(void);
static void intel_display_test_command_return(void);
static bool intel_display_test_command_retire(void);
static uint64_t intel_display_test_command_dma(void);
#endif
#ifdef SRVROS_INTEL_GT_RANGE
static bool intel_display_test_range_translate(uint64_t *);
static bool intel_display_test_range_alias(struct vmm_alias_observation *);
static bool intel_display_test_range_map(uint64_t);
static bool intel_display_test_range_read(uint32_t,uint32_t *);
static bool intel_display_test_range_unmap(uint64_t *);
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
static bool intel_display_test_primary_read(uint32_t, uint32_t *);
#ifdef SRVROS_INTEL_GT_FLUSH
static uint32_t intel_display_test_gam_request(void);
static bool intel_display_test_bank_map(unsigned, bool);
static bool intel_display_test_bank_aliases(void);
static uint32_t intel_display_test_bank_read(unsigned);
static void intel_display_test_bank_write(uint32_t);
#endif
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
static void test_queue_handoff_word(unsigned, unsigned, uint32_t *);
#endif
#include "test_intel_ring_transition_adapter.c"
static const char *bootstrap_mode="pass";
static unsigned boundaries,bcache,balias,breads,bwrites,bcommands,bpci,pulses,restore_step,bdisplay;
static unsigned fault_at,first_native_pci,receipt_calls,cost_calls;
static bool hit,preserve_fences;
static unsigned fault_writes,fault_commands;
static uint32_t control[5];
static uint64_t fences[32]={0x021ff02f00000001ull};
static uint32_t gpu_command=7;
static unsigned phase_points[BDW_BOOT_FAULT+1];
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
static unsigned memory_writes,memory_unmaps,memory_fault_writes,memory_fault_unmaps,memory_fault_reads,memory_late_boundaries,memory_reads;
static bool memory_fault_returned;
#endif
static bool bf(const char *mode,unsigned n) {
 bool yes=!strcmp(mode,bootstrap_mode) && n==fault_at;if(yes && !hit){hit=true;fault_writes=bwrites;fault_commands=bcommands;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 memory_fault_writes=memory_writes;memory_fault_unmaps=memory_unmaps;memory_fault_reads=memory_reads;memory_fault_returned=gt_bootstrap_native.returned;
#endif
 }return yes;
}
#ifdef SRVROS_INTEL_GT_FLUSH
static const uint64_t host_widi_pages[]={0xc0014000,0xc006e000,0xc0065000};
static bool host_widi_held;
static unsigned host_widi_count;
static unsigned host_port_pins;
static bool intel_display_test_context_wp(void) {
 return !bf("slot-wp",1);
}
static bool intel_display_test_context_protect(unsigned slot,bool writable) {
 assert(slot>=3 && slot<8 && wake_irq && gpu_command==(writable?7u:3u));
 assert(host_widi_held!=writable);
 unsigned bit=1u<<(slot-3);
 assert(((model_engine_ro & bit)!=0)==writable);
 if(bf(writable?"slot-restore":"slot-protect",slot-2))return false;
 if(writable)model_engine_ro &= ~bit;
 else model_engine_ro |= bit;
 return true;
}
bool vmm_reserved_guard_hold_readonly_registers(const struct vmm_reserved_guard *g,uint64_t root,
                                                const struct vmm_alias_span *spans,unsigned count) {
 assert(g==&slot_widi.guard && g->object==g && host_widi_held && wake_irq && gpu_command==3);
 assert(root==gt_bootstrap_native.root && count==(host_port_pins==4?1u:2u));
 for(unsigned i=0;i<count;i++) {
  unsigned e=host_port_pins+i;
  assert(e<5 && (model_engine_ro & (1u<<e)) && spans[i].pages==1 &&
         spans[i].virtual_base==WAKE_VA+4096*(3+e) && spans[i].physical==result.mmio_base+wake_pages[3+e]);
 }
 if(bf("slot-foreign",1+host_port_pins))return false;
 host_port_pins+=count;return true;
}
bool vmm_reserved_guard_begin_io_page(struct vmm_reserved_guard *g,uint64_t root,
 const struct vmm_physical_span *spans,unsigned count,unsigned bits,const struct vmm_alias_ram *ram,
 unsigned ranges,uint64_t aperture,struct vmm_alias_inventory *out,struct vmm_unmapped_detail *detail) {
 (void)detail;
 assert(!host_widi_held && !g->object && root==gt_bootstrap_native.root && wake_irq && gpu_command==7);
 assert(count==1 && spans[0].physical==0x8f004000 && spans[0].pages==1 && bits>=36 &&
        ranges && ram && out && aperture==0xb3003000);
 if(bf("widi-begin",1))return false;
 g->object=g;g->epoch=1;host_widi_held=true;return true;
}
bool vmm_reserved_guard_io_page_held(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page) {
 return g->object==g && wake_irq && root==gt_bootstrap_native.root && page==0xb3003000 && host_widi_held;
}
bool vmm_reserved_guard_exclude_io_range(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,unsigned n) {
 assert(vmm_reserved_guard_io_page_held(g,root,0xb3003000) && host_widi_count<3 &&
        n==1 && page==host_widi_pages[host_widi_count] && gpu_command==7);
 if(bf("widi-alias",1+host_widi_count))return false;
 ++host_widi_count;return true;
}
bool vmm_reserved_guard_excludes_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page) {
 if(!vmm_reserved_guard_io_page_held(g,root,0xb3003000))return false;
 for(unsigned i=0;i<host_widi_count;i++)
  if(page==host_widi_pages[i])return true;
 return false;
}
bool vmm_reserved_guard_close(struct vmm_reserved_guard *g,struct vmm_alias_inventory *out,
                             struct vmm_unmapped_detail *detail) {
 (void)detail;
 assert(g->object==g && host_widi_held && host_widi_count==3 && out && independent_reopened &&
        gpu_command==7 && bcommands==2 && gt_bootstrap_native.pci_samples==4);
 if(bf("widi-retire",1))return false;
 host_widi_held=false;return true;
}
static void intel_display_test_backing_access(void) {
 if(slot_backing.checks && bf("backing-content",slot_backing.checks))test_backing_page[511]^=1;
}
static void intel_display_test_bootstrap_smi_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
 assert(wake_irq && !display_access_allowed());
 if(!leaf) {
  *a=bf("smi-no-leaf",1)?0:1;*b=bf("smi-vendor",1)?0:0x756e6547;
  *c=0x6c65746e;*d=0x49656e69;
 } else {
  assert(leaf==1);
  *a=bf("smi-model",1)?0x406e3:0x306d4;*b=0;
  *c=bf("smi-hypervisor",1)?1u<<31:0;*d=bf("smi-no-msr",1)?0:1u<<5;
 }
}
static uint32_t intel_display_test_bootstrap_smi_count(unsigned point) {
 assert(wake_irq && !display_access_allowed());
 assert(point>=BOOT_SMI_START && point<=BOOT_SMI_END);
 unsigned sample=bootstrap_smi.reads+1;
 if(!strcmp(bootstrap_mode,"smi-wrap"))return bf("smi-wrap",sample)?0:UINT32_MAX;
 return bf("smi-count",sample)?14:13;
}
#endif
static void intel_display_test_bootstrap_boundary(void) {
 ++boundaries;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(gt_memory.d.state==2) {
  ++memory_late_boundaries;
  if(bf("memory-late-child",memory_late_boundaries))gt_bootstrap_native.memory_child=&gt_memory;
  if(bf("memory-late-core",memory_late_boundaries))gt_memory.vtd.p.reads++;
  if(bf("memory-late-alias",memory_late_boundaries))gt_memory.d.alias[0].pat_index++;
  if(bf("memory-late-post",memory_late_boundaries))gt_memory.post.words[0][0][0]^=8;
 }
#endif
 if(gt_bootstrap_native.reset.stage<=BDW_BOOT_FAULT && !phase_points[gt_bootstrap_native.reset.stage])
  phase_points[gt_bootstrap_native.reset.stage]=boundaries;
 assert(wake_irq && !display_access_allowed());
#ifdef SRVROS_INTEL_GT_ARENA
 assert(hda_command==2 || (hit && !strcmp(bootstrap_mode,"command-hda")));
#else
 assert(hda_command==2);
#endif
 if(!strcmp(bootstrap_mode,"maintain-slow"))ticks+=130;
 if(!strcmp(bootstrap_mode,"seconds-slow"))ticks+=1000;
 if(!strcmp(bootstrap_mode,"practical-slow"))ticks+=175;
 if(bf("zero",boundaries))ticks=UINT64_MAX;
 if(bf("back",boundaries))ticks=gt_bootstrap_native.last-2;
 if(bf("over",boundaries))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US;
 if(bf("root",boundaries))gt_bootstrap_native.root^=4096;
 if(bf("epoch",boundaries))gt_bootstrap_native.g->engine_epoch++;
 if(bf("borrow",boundaries))gt_bootstrap_native.g->dependents=NULL;
 if(bf("source",boundaries))gt_bootstrap_native.source.binding.sequence++;
 if(bf("registration",boundaries))gt_bootstrap_native.reset.registered=false;
 if(bf("request",boundaries))wake_request|=2;
 if(bf("hda",boundaries))hda_command=6;
 if(bf("alias",boundaries))pa[index_of(WAKE_VA+14*4096)]^=4096;
 if(bf("final-fence",boundaries))gt_bootstrap_native.reset.fence_final[0]^=1;
 if(bootstrap_parent()) {
  unsigned attempts=gt_bootstrap_native.g->write_attempts;
  assert(!intel_gt_access_close(gt_bootstrap_native.g));
  assert(attempts==gt_bootstrap_native.g->write_attempts);
 }
}
static bool intel_display_test_bootstrap_cache(struct x86_cache_snapshot *out) {
 assert(wake_irq);++bcache;
 *out=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x91000,39,0x80050033},
 .pat=0x0007040600070406ull,.cap=0x400,.def_type=0x800,.status=CACHE_OBSERVED,.reads=6,
 .begin_us=ticks,.end_us=ticks+1};ticks++;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(!strcmp(bootstrap_mode,"memory-cache-layout")
#ifdef SRVROS_INTEL_GT_ARENA
 || true
#endif
 ) {
  out->cap=0x501;out->count=1;out->fixed_count=11;out->reads=19;
  out->base[0]=6;out->mask[0]=0x7fc0000800ull;
  for(unsigned i=0;i<11;i++)out->fixed[i]=0x0606060606060606ull;
 }
#endif
 if(bf("cache-change",bcache))out->pat^=1;
 return !bf("cache",bcache);
}
static bool intel_display_test_bootstrap_alias(unsigned i,uint64_t physical,struct vmm_alias_observation *out,struct vmm_range_observation *range) {
 ++balias;uint64_t actual=0;
 if(gt_bootstrap_native.root!=0x1000)return false;
 if(!vmm_virt_to_phys_in_space(0x1000,WAKE_VA+4096*i,&actual) || actual!=physical || bf("walk",balias))return false;
 bool writable=i==0 || (i>=3 && i<=7) || i==9 || i==14;
#if defined(SRVROS_INTEL_BCS_CONTEXT) || defined(SRVROS_INTEL_GT_FLUSH)
 if(i>=3 && i<=7 && (model_engine_ro & (1u<<(i-3))))writable=false;
#endif
 *out=(struct vmm_alias_observation){1,1,2,writable,0,1,1};*range=(struct vmm_range_observation){0};
 if(bf("user-alias",balias))out->user=1;
 if(bf("exec-alias",balias))out->no_execute=0;
 if(bf("wc-alias",balias))out->pat_index=1;
 if(bf("read-only",balias))out->writable^=1;
 return true;
}
static uint32_t bootstrap_model_config(unsigned off) {
 switch(off) {
 case 0:return 0x16268086;case 4:return 0x00900000|gpu_command;case 8:return 0x03000009;
 case 0x2c:return 0x011b106b;case 0x34:return 0x90;case 0x90:return 0xd005;
 case 0xd0:return 0x0022a401;case 0xa4:return 0x03060013;
 case 0xa8:return 0;
 default:assert(false);return UINT32_MAX;
 }
}
/* Fault only authoritative native reads; legacy optional capability traversal
 * has its own existing regression coverage. */
static uint32_t intel_display_test_bootstrap_config(unsigned off) {
 ++bpci;
 if(!first_native_pci)first_native_pci=bpci;
 if(bf("pci-float",bpci))return UINT32_MAX;
 if(off==0xa8 && bf("pending",bpci))return 256;
 return bootstrap_model_config(off);
}
static uint32_t intel_display_test_bootstrap_legacy_config(unsigned off) {
 if(gt_bootstrap_native.complete && off==4 && bf("outer-pci",1))return UINT32_MAX;
 return bootstrap_model_config(off);
}
static bool intel_display_test_bootstrap_command(unsigned value) {
 ++bcommands;assert(bcommands<=2 && value==(bcommands==1?3u:7u));
 assert(bootstrap_parent() && hda_command==2 && wake_irq && !display_access_allowed());
#ifdef SRVROS_INTEL_GT_FLUSH
 assert(gt_slot_live() && !independent_returned && test_backing_allocated);
 if(value==7) {
  const struct intel_bdw_gt_bootstrap *reset=&gt_bootstrap_native.reset;
  assert(reset->binding.blitter_port_owner==&slot_backing);
  assert(intel_bdw_bcs_port_held(reset->binding.blitter_port_owner));
  assert(intel_bdw_reset_epoch_live(reset,&gt_bootstrap_native.post));
 }
 assert(!intel_bdw_bcs_port_claim(&gpu_command) && !intel_bdw_bcs_port_release(&gpu_command));
 struct scanout_interval competing={0};
 assert(!scanout_interval_claim(&competing,result.mmio_base,window_result.aperture_base,
   window_result.aperture_bytes,window_result.endpoint.fence_end,(struct scanout_space_range){GT_SECOND_SLOT,1}));
#endif
 if(value==7)assert(gt_bootstrap_native.reset.complete && gt_bootstrap_native.post_bound && !gt_bootstrap_native.reset.registered);
 if(bf("command-none",bcommands))return false;
 gpu_command=value;
 return !bf("command-unknown",bcommands);
}
static bool intel_display_test_bootstrap_read(uint32_t offset,uint32_t *out) {
 ++breads;assert(gpu_command==3 && bootstrap_parent());
 if(bf("read",breads))return false;
 if(offset==BDW_BOOT_GDRST)*out=bf("reset-stuck",breads)?1:0;
 else if(offset>=BDW_BOOT_FENCE_BASE) {
  unsigned n=(offset-BDW_BOOT_FENCE_BASE)/8;
  *out=(uint32_t)(fences[n]>>((offset&4)?32:0));
  if(n==0 && !(offset&4) && restore_step==1){assert(!*out);restore_step=2;}
  else if(n==0 && !(offset&4) && restore_step==4){assert(*out==1);restore_step=5;}
 } else {
  unsigned n=0;while(n<5 && bootstrap_controls[n]!=offset)n++;assert(n<5);*out=control[n];
 }
 if(bf("float",breads))*out=UINT32_MAX;
 if(bf("foreign",breads))*out^=16;
 if(bf("read-over",breads))ticks+=INTEL_DIAGNOSTIC_PREPARE_US+1;
 return true;
}
static enum intel_gt_write intel_display_test_bootstrap_write(uint32_t offset,uint32_t value) {
 ++bwrites;assert(gpu_command==3 && bootstrap_parent() && wake_irq && hda_command==2 && !display_access_allowed());
 if(bf("write-none",bwrites))return GT_WRITE_NONE;
 if(offset==BDW_BOOT_GDRST) {
  assert(value==1 && ++pulses<=2 && gt_bootstrap_native.epoch_calls==1);
  assert(gt_bootstrap_native.g->engine_epoch==gt_bootstrap_native.epoch+1);
  if(pulses==1)for(unsigned i=0;i<5;i++)assert(control[i]==3);
  for(unsigned i=0;i<5;i++)control[i]=0;
  ring_ctl=0;
  if(!preserve_fences)for(unsigned i=0;i<32;i++)fences[i]=0;
 } else if(offset>=BDW_BOOT_FENCE_BASE) {
  assert(!preserve_fences && pulses==2);
  unsigned n=(offset-BDW_BOOT_FENCE_BASE)/8;assert(n==0);
  if(!(offset&4) && !restore_step){assert(!value);restore_step=1;}
  else if(offset&4){assert(restore_step==2 && value==0x021ff02f);restore_step=3;}
  else {assert(restore_step==3 && value==1);restore_step=4;}
  if(offset&4)fences[n]=(fences[n]&UINT32_MAX)|((uint64_t)value<<32);
  else fences[n]=(fences[n]&0xffffffff00000000ull)|value;
 } else {
  unsigned n=0;while(n<5 && bootstrap_controls[n]!=offset)n++;assert(n<5);
  if(value==0x10001){assert(!pulses && !control[n] && n==gt_bootstrap_native.issued);control[n]=3;}
  else {assert(value==0x10000 && pulses==2 && n==4-gt_bootstrap_native.reset.cancelled);control[n]=0;}
 }
 if(bf("write-unknown",bwrites))return GT_WRITE_UNCERTAIN;
 if(bf("write-over",bwrites))ticks+=INTEL_DIAGNOSTIC_PREPARE_US+1;
 return GT_WRITE_DONE;
}
static bool intel_display_test_bootstrap_display(unsigned p,unsigned i,uint32_t *out) {
 ++bdisplay;assert(p<3 && i<13 && bootstrap_parent());
 *out=(!p && !i)?0xd8000400u:0;
 if(bf("display-change",bdisplay))*out^=1;
 if(bf("display-float",bdisplay))*out=UINT32_MAX;
 return !bf("display-read",bdisplay);
}
static bool intel_display_test_bootstrap_expected(void) {
#ifdef SRVROS_INTEL_GT_FLUSH
 if(!strcmp(bootstrap_mode,"flush-quiet") || !strcmp(bootstrap_mode,"flush-stale") ||
    !strcmp(bootstrap_mode,"flush-chatter") || !strcmp(bootstrap_mode,"flush-latched"))return true;
#endif
#ifdef SRVROS_INTEL_GT_AUX
 if(!strcmp(bootstrap_mode,"aux-vga-active") || !strcmp(bootstrap_mode,"aux-fbc-active") ||
    !strcmp(bootstrap_mode,"aux-fbc-segment") || !strcmp(bootstrap_mode,"aux-psr-enable") ||
    !strcmp(bootstrap_mode,"aux-psr-state") || !strcmp(bootstrap_mode,"aux-route") ||
    !strcmp(bootstrap_mode,"aux-drift"))return true;
#endif
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(!strcmp(bootstrap_mode,"memory-cache-layout"))return true;
#endif
 return !strcmp(bootstrap_mode,"pass") || !strcmp(bootstrap_mode,"preserved") || !strcmp(bootstrap_mode,"maintain-slow") || !strcmp(bootstrap_mode,"seconds-slow") || !strcmp(bootstrap_mode,"practical-slow");}
static bool intel_display_test_bootstrap_clock(uint64_t *out) {
 if(!gt_bootstrap_native.returned || gt_bootstrap_native.complete || hit)return false;
 if(bf("return-zero",1))*out=0;
 else if(bf("return-back",1))*out=gt_bootstrap_native.last-1;
 else if(bf("return-over",1))*out=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 else return false;
 return true;
}
static void intel_display_test_bootstrap_final_boundary(void) {
 assert(gt_bootstrap_native.returned && !gt_bootstrap_native.borrow.active && gt_bootstrap_native.g->pinned);
 if(bf("final-zero",1))ticks=UINT64_MAX;
 if(bf("final-back",1))ticks=gt_bootstrap_native.last-2;
 if(bf("final-over",1))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US;
 if(bf("final-epoch",1))gt_bootstrap_native.g->engine_epoch++;
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 if(bf("final-memory-child",1))gt_bootstrap_native.memory_child=&gt_memory;
 if(bf("final-memory-core",1))gt_memory.vtd.p.reads++;
#endif
}
static void intel_display_test_bootstrap_maintain_boundary(unsigned phase) {
 struct intel_gt_access *g=gt_bootstrap_native.g;
 assert(phase<2 && g==joint.scope.gpu && g->engine_epoch==gt_bootstrap_native.epoch);
 if(!phase)assert(bootstrap_parent());
 assert(gt_bootstrap_native.reset.stage==BDW_BOOT_EPOCH && !gt_bootstrap_native.reset.write_attempts && !pulses);
 for(unsigned i=0;i<32;i++)assert(fences[i]==gt_bootstrap_native.reset.fence_before[i]);
 if(!phase) {
  if(bf("maintain-zero",1))ticks=UINT64_MAX;
  if(bf("maintain-back",1))ticks=g->last-2;
  if(bf("maintain-phase",1))ticks=g->last+GT_ACCESS_PHASE_US+1;
  if(bf("maintain-total",1))ticks=g->begin+GT_ACCESS_TOTAL_US+1;
  if(bf("maintain-request",1))wake_request|=2;
  if(bf("maintain-root",1))gt_bootstrap_native.root^=4096;
 } else {
  if(bf("maintain-after-clock",1))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
  if(bf("maintain-after-epoch",1))g->engine_epoch++;
  if(bf("maintain-after-borrow",1))g->dependents=NULL;
  if(bf("maintain-after-table",1))g->io.snapshot=NULL;
 }
}
static void intel_display_test_bootstrap_cost_boundary(unsigned phase) {
 assert(phase<5);++cost_calls;
 if(bf("cost-zero",cost_calls))ticks=UINT64_MAX;
 if(bf("cost-back",cost_calls))ticks=gt_bootstrap_native.last-2;
 if(bf("cost-over",cost_calls))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US;
}
static void intel_display_test_bootstrap_receipt_boundary(void) {
 ++receipt_calls;
 if(bf("ticket-empty",receipt_calls))bootstrap_admission.active=false;
 if(bf("ticket-samples",receipt_calls))bootstrap_admission.samples++;
 if(bf("ticket-clock",receipt_calls))bootstrap_admission.clock++;
 if(bf("ticket-proof",receipt_calls))bootstrap_admission.proofs++;
 if(bf("ticket-attempt",receipt_calls))bootstrap_admission.attempts++;
 if(bf("ticket-state",receipt_calls))gt_bootstrap_native.reset.index++;
 if(bf("ticket-zero",receipt_calls))ticks=UINT64_MAX;
 if(bf("ticket-back",receipt_calls))ticks=gt_bootstrap_native.last-2;
 if(bf("ticket-over",receipt_calls))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US;
 if(receipt_calls==1 && bf("ticket-byte",fault_at)) {
  assert(fault_at<sizeof(bootstrap_admission.bytes));bootstrap_admission.bytes[fault_at]^=1;
 }
}
static __attribute__((noinline)) void bootstrap_churn_stack(void) {
 volatile unsigned char bytes[65536];
 for(unsigned i=0;i<sizeof(bytes);i++)bytes[i]=(unsigned char)(i*19+7);
}
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
#include "test_intel_gt_memory_model.inc"
#endif
#ifdef SRVROS_INTEL_GT_ARENA
#include "test_intel_gt_arena_model.inc"
#ifdef SRVROS_INTEL_BCS_CONTEXT
static uint32_t intel_display_test_context_traffic(unsigned reg){
 static unsigned calls;
 static const unsigned offsets[]={0x40d0,0x12870,0x12878,0x1c870,0x1c878,0xa180};
 assert(model_engine_ro==31 && wake_irq && gpu_command==3 && calls<12);
 unsigned i=calls%6;assert(reg==offsets[i]);
 assert(bcs_context_native.pte_reads==(calls<6?0u:30u));
 if(cf("context-traffic",++calls))return UINT32_MAX;
 if(reg==0xa180)return 0xc5280000;
 return i?(0x01000000u*i+0x00010000u*((calls-1)/6)):0x805fu;
}
static bool intel_display_test_context_wp(void){
 static unsigned calls;
 return !cf("context-wp",++calls);
}
static bool intel_display_test_context_protect(unsigned slot,bool writable){
 static unsigned calls;
 assert(slot>=3 && slot<=7 && wake_irq && gpu_command==3);
 unsigned bit=1u<<(slot-3);
 assert(((model_engine_ro&bit)!=0)==writable);
 ++calls;
 if(cf("context-protect",calls))return false;
 if(cf("context-protect-noop",calls))return true;
 if(writable)model_engine_ro &= ~bit;else model_engine_ro |= bit;
 return true;
}
static bool intel_display_test_context_aliases(const struct vmm_alias_span *spans,unsigned count){
 unsigned n=bcs_context_native.alias_checks,first=(n%3)*2;
 assert(model_engine_ro==31 && count==(first==4?1u:2u));
 for(unsigned i=0;i<count;++i){
  unsigned slot=3+first+i;
  assert(spans[i].pages==1 && spans[i].physical==result.mmio_base+wake_pages[slot] &&
         spans[i].virtual_base==WAKE_VA+4096*slot);
 }
 return !cf("context-foreign-alias",n+1);
}
static bool intel_display_test_context_widi_absent(uint64_t physical){
 unsigned pass=bcs_context_native.widi_unmapped;
 assert(pass<2 && physical==result.mmio_base+0x14000 && wake_irq && model_engine_ro==31);
 assert(bcs_context_native.alias_checks==3*(pass+1) && bcs_context_native.pte_reads==15*pass);
 return !cf("context-widi-alias",pass+1);
}
static bool intel_display_test_context_pte(uint32_t offset,uint32_t *value){
 assert(offset<40 && !(offset&3) && wake_irq && gpu_command==3 && model_engine_ro==31);
 uint64_t entry=0x180000001ull+(offset/8)*4096;
 *value=(uint32_t)(entry>>((offset&4)*8));
 unsigned n=bcs_context_native.pte_reads+1;
 if(cf("context-live-drift",n))*value^=1;
 return !cf("context-live-read",n);
}
static unsigned context_boundaries;
static void intel_display_test_context_boundary(void) {
 ++context_boundaries;
 if(cf("context-content",context_boundaries))command_ram[1024]^=1;
 if(cf("context-ring",context_boundaries))command_ram[0]^=1;
 if(cf("context-hwsp",context_boundaries))command_ram[512]^=1;
 if(cf("context-completion",context_boundaries))command_ram[1536]^=1;
 if(cf("context-config",context_boundaries))bcs_context_native.config.nonce++;
 if(cf("context-physical",context_boundaries))bcs_context_native.physical+=4096;
 if(cf("context-root",context_boundaries))bcs_context_native.root+=4096;
 if(cf("context-descriptor",context_boundaries))bcs_context_native.descriptor^=1;
 struct scanout_interval competing;
 assert(!scanout_interval_claim(&competing,result.mmio_base,window_result.aperture_base,
  window_result.aperture_bytes,window_result.endpoint.fence_end,command_slots));
 assert(scanout_interval_claim(&competing,result.mmio_base,window_result.aperture_base,
  window_result.aperture_bytes,window_result.endpoint.fence_end,(struct scanout_space_range){INTEL_WINDOW_FIRST,1}));
 scanout_interval_release(&competing);
 if(cf("context-target-alias",context_boundaries))
  window_result.sample[0].entry[4]=window_result.sample[1].entry[4]=window_result.sample[0].entry[0];
 if(cf("context-pte-drift",context_boundaries))window_result.sample[1].entry[2]^=4096;
 if(cf("context-cpu-alias",context_boundaries))
  window_result.sample[0].entry[3]=window_result.sample[1].entry[3]=command_native.owner.physical|1;
 if(cf("context-lost-slots",context_boundaries))scanout_interval_release(&bcs_context_native.slots);
 if(cf("context-count",context_boundaries))bcs_context_native.verified++;
}
#endif

#endif
#ifdef SRVROS_INTEL_GT_RANGE
static uint64_t range_pa;
static unsigned range_maps,range_reads,range_unmaps,range_aliases;
#ifdef SRVROS_INTEL_GT_PRIMARY
static unsigned primary_reads,primary_maps,primary_unmaps,primary_aliases;
#endif
#ifdef SRVROS_INTEL_GT_AUX
static unsigned aux_reads,aux_maps,aux_unmaps,aux_aliases;
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
static unsigned model_bank_maps;
static uint32_t model_selector = 0x10000008;
static bool intel_display_test_bank_map(unsigned i, bool create) {
 assert(gt_bank_live() && i < 2 && !!(model_bank_maps & (1u<<i)) != create);
 if(bf("bank-map", create ? i+1 : 4-i)) return false;
 model_bank_maps ^= 1u << i;
 return true;
}
static bool intel_display_test_bank_aliases(void) {
 static unsigned calls;
 return model_bank_maps == 3 && !bf("bank-alias", ++calls);
}
static void intel_display_test_bank_write(uint32_t target) {
 assert(model_bank_maps==3 && gt_bank_live() && !((target ^ model_selector) & ~(15u<<24)));
 if(!bf("bank-post",gt_banks.writes)) model_selector=target;
}
static uint32_t intel_display_test_bank_read(unsigned offset) {
 assert(model_bank_maps==3 && gt_bank_live());
 unsigned i=(model_selector>>24)&15u;
 if(offset==GT_L3_SELECTOR) {
  if(gt_banks.banks==7 && i==7 && bf("bank-foreign",1)) model_selector ^= 1u<<28;
  return model_selector;
 }
 assert(i<8 && (offset==GT_L3_PIPE || offset==GT_L3_PM));
 if(offset==GT_L3_PM && bf("bank-float",i+1)) return UINT32_MAX;
 return offset==GT_L3_PIPE ? (0x40400000u|i) : (1u|(i==7 ? 16u : 0));
}
static unsigned flush_reads,flush_maps,flush_unmaps,flush_aliases;
static uint32_t intel_display_test_gam_request(void) {
 assert(bootstrap_parent() && wake_irq && gpu_command==3 &&
        gt_bootstrap_native.reset.reset_attempts==gt_bootstrap_native.reset.reset_completed);
 if(!strcmp(bootstrap_mode,"flush-chatter"))return 0x5f | ((gt_request_trace.reads & 1u)<<8);
 if(gt_bootstrap_native.reset.reset_completed && !strcmp(bootstrap_mode,"flush-latched"))return 0x7fff;
 if(gt_bootstrap_native.reset.reset_completed)
  return gt_bootstrap_native.post_bound ? 0x5f : 0x7f5f;
 return gt_bootstrap_native.prepared==5 && strcmp(bootstrap_mode,"flush-quiet") ? 0 : 0x5f;
}
static bool flush_page(uint64_t pa) {
 return pa==0xc0008000 || pa==0xc0012000 || pa==0xc001c000 || pa==0xc0004000 || pa==0xc000a000;
}
#endif
static bool intel_display_test_range_translate(uint64_t *physical) {
 if(!range_pa)return false;
 *physical=range_pa;return true;
}
static bool intel_display_test_range_alias(struct vmm_alias_observation *out) {
 assert(bootstrap_parent() && range_pa && wake_irq && !display_access_allowed());
 *out=(struct vmm_alias_observation){1,1,3,0,0,1,1};
#ifdef SRVROS_INTEL_GT_FLUSH
 if(gt_reopened.reading || gt_widi.reading || gt_widi.sampling)return true;
 if(flush_page(range_pa)) {
  if(bf("flush-alias",++flush_aliases))out->writable=1;
  return true;
 }
#endif
#ifdef SRVROS_INTEL_GT_AUX
 if(gt_aux.active) {
  if(bf("aux-alias",++aux_aliases))out->writable=1;
  return true;
 }
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
 if(gt_range.completed==2) {
  if(bf("primary-alias",++primary_aliases))out->writable=1;
  return true;
 }
#endif
 if(bf("range-alias",++range_aliases))out->writable=1;
 return true;
}
static bool intel_display_test_range_map(uint64_t physical) {
#ifdef SRVROS_INTEL_GT_FLUSH
 if(gt_reopened.reading) {
  assert(physical==0xc0818000 && !range_pa && gpu_command==7 && slot_reopened_live());
  range_pa=physical;return true;
 }
#endif
#ifdef SRVROS_INTEL_GT_SLOT
 if(gt_range.completed) {
  assert(gpu_command==3 && gt_slot_live());
  struct scanout_interval competing={0};
  assert(!scanout_interval_claim(&competing,result.mmio_base,window_result.aperture_base,
    window_result.aperture_bytes,window_result.endpoint.fence_end,(struct scanout_space_range){GT_RANGE_CANDIDATE,1}));
 }
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
 if(gt_widi.reading) {
  assert(!host_widi_held && slot_widi_preflight_live() && !range_pa && gpu_command==7 &&
         (physical==0xc0042000 || physical==0xc006e000 || physical==0xc0065000 || physical==0xc0045000));
  range_pa=physical;return true;
 }
 if(gt_widi.sampling){assert(gt_flush_live() && !range_pa && physical==0xc0008000);range_pa=physical;return true;}
 if(flush_page(physical)) {
  assert(gt_flush_live() && !range_pa && wake_irq && gpu_command==3);
  if(bf("flush-map",++flush_maps))return false;
  range_pa=physical;
  return !bf("flush-map-retained",flush_maps);
 }
#endif
#ifdef SRVROS_INTEL_GT_AUX
 if(gt_aux.active) {
  assert(bootstrap_parent() && !range_pa && wake_irq && gpu_command==3);
  assert(physical==0xc0041000 || physical==0xc0043000 || physical==0xc006f000 || physical==0xc007f000);
  if(bf("aux-map",++aux_maps))return false;
  range_pa=physical;
  return !bf("aux-map-retained",aux_maps);
 }
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
 if(gt_range.completed==2) {
  assert(bootstrap_parent() && !range_pa && wake_irq && gpu_command==3);
  assert(physical==0xc0045000 || physical==0xc0060000 || physical==0xc0061000 || physical==0xc0062000);
  if(bf("primary-map",++primary_maps))return false;
  range_pa=physical;
  return !bf("primary-map-retained",primary_maps);
 }
#endif
 assert(bootstrap_parent() && !range_pa && wake_irq && (physical==0xc0802000 || physical==0xc0811000
#ifdef SRVROS_INTEL_GT_FLUSH
  || physical==0xc0818000
#endif
  ));
 if(bf("range-map",++range_maps))return false;
 range_pa=physical;return !bf("range-map-retained",range_maps);
}
static bool intel_display_test_range_read(uint32_t offset,uint32_t *out) {
 assert(bootstrap_parent() && range_pa==0xc0000000ull+(offset&~4095u));
 assert(gpu_command==(gt_range.completed?3:7));
 if(bf("range-read",++range_reads))return false;
 if(bf("range-clock",range_reads))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 unsigned slot=(offset-0x800000)/8;
 assert((slot>=1356 && slot<1484) || slot==8704
#ifdef SRVROS_INTEL_GT_FLUSH
  || (slot>=GT_SECOND_SLOT-1 && slot<=GT_SECOND_SLOT+1)
#endif
 );
 uint64_t value=0x200000001ull+(uint64_t)slot*4096;
#ifdef SRVROS_INTEL_GT_PRIMARY
 value=0x8c001001ull+(uint64_t)slot*4096;
 if(slot==8704 && !strcmp(bootstrap_mode,"primary-physical-overlap")) {
  value=0x8c54d001;
  if(gt_range.completed)bf("primary-physical-overlap",1);
 }
#endif
 if(bf("range-drift",range_reads))value^=0x1000;
 if(bf("range-high",range_reads))value^=1ull<<44;
 *out=(uint32_t)(value>>((offset&4)?32:0));
 return true;
}
static bool intel_display_test_range_unmap(uint64_t *physical) {
 assert(bootstrap_parent() && range_pa);
#ifdef SRVROS_INTEL_GT_FLUSH
 if(gt_reopened.reading || gt_widi.reading || gt_widi.sampling){*physical=range_pa;range_pa=0;return true;}
 if(flush_page(range_pa)) {
  if(bf("flush-unmap",++flush_unmaps))return false;
  *physical=range_pa;range_pa=0;return true;
 }
#endif
#ifdef SRVROS_INTEL_GT_AUX
 if(gt_aux.active) {
  if(bf("aux-unmap",++aux_unmaps))return false;
  *physical=range_pa;range_pa=0;return true;
 }
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
 if(gt_range.completed==2) {
  if(bf("primary-unmap",++primary_unmaps))return false;
  *physical=range_pa;range_pa=0;return true;
 }
#endif
 if(bf("range-unmap",++range_unmaps))return false;
 *physical=range_pa;range_pa=0;return true;
}
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
#ifdef SRVROS_INTEL_GT_FLUSH
static void test_queue_handoff_word(unsigned e, unsigned i, uint32_t *word) {
 if (!gt_bootstrap_native.post_bound || gt_bootstrap_native.pci_samples < 3) return;
 unsigned phase = gt_bootstrap_native.pci_samples - 3;
 if (i == 4 && bf(phase ? "queue-after" : "queue-before", e + 1)) *word ^= 1u << 4;
}
#endif
static bool intel_display_test_primary_read(uint32_t offset,uint32_t *out) {
 static const uint32_t reader_offsets[13]={0x180,0x19c,0x1ac,0x1b0,0x1bc,0x80,0x84,0xac,0x280,0x29c,0x2ac,0x2b0,0x2bc};
#ifdef SRVROS_INTEL_GT_FLUSH
 if(gt_reopened.reading) {
  assert(slot_reopened_live() && slot_pte_register(offset) && host_widi_count==3);
  *out=offset==GT_SECOND_PTE_OFFSET ? 0x8f004001 : 0;
  if(bf("widi-reopened-pte",1+gt_reopened.access.reads))*out^=0x1000;
  return true;
 }
 if(gt_widi.sampling) {
  assert(host_widi_held && host_widi_count==3 && gt_flush_live() && slot_widi_message(offset));
  *out=offset==0x8014 && (gt_flush.samples!=1 || !strcmp(bootstrap_mode,"flush-quiet")) ? 0x1f : 0;
  return !bf("widi-message",1+gt_widi.messages.reads);
 }
 if(gt_widi.reading) {
  assert(slot_widi_preflight_live() && range_pa==result.mmio_base+(offset&~4095u));
  if(offset==0x42014)*out=bf("widi-video",1)?0:0x20100042;
  else if(offset==0x6e400)*out=0;
  else if(offset==0x45404)*out=bf("widi-power",gt_widi.access.reads)?0:0xc0000000;
  else if(offset==0x65020)*out=bf("widi-codec",gt_widi.access.reads)?0:0x80862808;
  else {assert(offset==0x65800);*out=bf("widi-audio",1)?1u<<30:0x540;}
  return true;
 }

 if(offset==0x138064 || offset==0x9120) {
  assert(!range_pa && gt_flush_live());
  *out=offset==0x9120 ? 3u<<25 : (gt_flush.samples==1 ? 3u : 0u);
  if(offset==0x138064 && model_bank_maps==3 && gt_banks.captured && bf("bank-power",gt_banks.banks+1)) *out=1;
  static unsigned slice_reads;
  return !bf("slice-read", ++slice_reads);
 }
 if(flush_page(range_pa)) {
  static const uint32_t expected[]={0x40d0,0x8028,0x8000,0x8004,0x8010,0x8008,0x800c,0x8038,0x8040,
    0x12870,0x12878,0x1c870,0x1c878,0xa090,0xa094,0xa0b8,0x8038,0x8028,0x8000,0x8004,0x8010,0x8008,0x800c,0x8040,0x40d0};
  unsigned i=flush_reads%(sizeof(expected)/sizeof(expected[0])), n=gt_flush.samples;
  assert(gt_flush_live() && offset==expected[i] && range_pa==0xc0000000ull+(offset&~4095u) && gpu_command==3);
  assert((n==0 && bwrites==0 && !pulses) || (n==1 && bwrites==5 && !pulses) ||
         (n==2 && pulses==2 && !gt_bootstrap_native.post_bound) ||
         (n==3 && pulses==2 && gt_memory.d.state==2 && gt_primary.complete));
  if(bf("flush-read",++flush_reads))return false;
  bool blocked=n==1 && strcmp(bootstrap_mode,"flush-quiet");
  bool ack=blocked || !strcmp(bootstrap_mode,"flush-stale");
  if(i==0 || i==24)*out=intel_display_test_gam_request();
  else if(i==1 || i==17)*out=blocked?0x20:0x7f;
  else if((i>=2 && i<=6) || (i>=18 && i<=22))*out=ack?32:16;
  else if(i==7 || i==16)*out=ack?1:0;
  else if(i==8 || i==23)*out=blocked?0x201:0x200;
  else if(i<=12) {
    *out=0x40004321u;
    if(bf("flush-busy",4*n+i-8))*out|=(i-8)<<16;
  } else if(i==13)*out=n>=2?0:0x90040000;
  else if(i==14)*out=n>=2?0x40000:0;
  else *out=625;
  if(bf("flush-float",flush_reads))*out=UINT32_MAX;
  if(bf("flush-clock",flush_reads))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
  if(bf("flush-window",flush_reads))ticks=gt_flush.begin[n]+BDW_BOOT_PHASE_US+1;
  return true;
 }
#endif
 assert(gt_primary_live() && gpu_command==3);
 unsigned page=offset&~4095u;
#ifdef SRVROS_INTEL_GT_AUX
 if(gt_aux.active) {
  assert(range_pa==0xc0000000ull+page && gt_aux_offset_allowed(offset));
  unsigned n=0;
  while(gt_aux_registers[n]!=offset)++n;
  static const uint32_t sample[AUX_WORDS]={0x80000000,0,0,0x82200000,0,0,0xc0000000};
  *out=sample[n];
  bool fault=bf("aux-read",++aux_reads);
  if(bf("aux-float",aux_reads))*out=UINT32_MAX;
  if(bf("aux-clock",aux_reads))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
  if(n==AUX_VGA_CONTROL && !strcmp(bootstrap_mode,"aux-vga-active"))*out=0;
  if(n==AUX_FBC_CONTROL && !strcmp(bootstrap_mode,"aux-fbc-active"))*out=1u<<31;
  if(n==AUX_FBC_STATUS2 && !strcmp(bootstrap_mode,"aux-fbc-segment"))*out=1;
  if(n==AUX_PSR_CONTROL && !strcmp(bootstrap_mode,"aux-psr-enable"))*out=1u<<31;
  if(n==AUX_PSR_STATUS && !strcmp(bootstrap_mode,"aux-psr-state"))*out=1u<<29;
  if(n==AUX_EDP_CONFIG && !strcmp(bootstrap_mode,"aux-route"))*out=0;
  if(n==AUX_PSR_STATUS && gt_aux.samples==1 && !strcmp(bootstrap_mode,"aux-drift"))*out=1;
  return !fault;
 }
#endif
 if(page>=0x70000 && page<=0x72000) {
  assert(!range_pa);
  unsigned pipe=(page-0x70000)/4096,i=0;
  while(i<13 && reader_offsets[i]!=(offset&4095))++i;
  if(i<13)*out=gt_bootstrap_native.display[1][pipe][i];
  else {
   assert((offset&4095)==0x188 || (offset&4095)==0x1a4);
   *out=result.sample[1].pipe[pipe][(offset&4095)==0x188?3:5];
  }
 } else {
  assert(range_pa==0xc0000000ull+page && intel_capture_offset_allowed(offset));
  if(page==0x45000)*out=result.sample[1].well_after[(offset-0x45400)/4];
  else {
   assert(page>=0x60000 && page<=0x62000);
   *out=result.sample[1].source[(page-0x60000)/4096];
  }
 }
 if(bf("primary-read",++primary_reads))return false;
 if(bf("primary-drift",primary_reads))*out^=1;
 if(bf("primary-float",primary_reads))*out=UINT32_MAX;
 if(bf("primary-clock",primary_reads))ticks=gt_bootstrap_native.begin+INTEL_DIAGNOSTIC_PREPARE_US+1;
 return true;
}
#endif
int main(int argc,char **argv) {
#ifdef SRVROS_INTEL_GT_ARENA
 if(argc>1 && !strncmp(argv[1],"arena-core-",11))return arena_core_case(argv[1]);
#endif
 if(argc>1)bootstrap_mode=argv[1];
 if(argc>2)fault_at=(unsigned)atoi(argv[2]);
 if(argc>3){assert(!strcmp(argv[3],"disabled"));ring_fixture_disabled=true;ring_ctl=0;}
 preserve_fences=!strcmp(bootstrap_mode,"preserved");
 char *args[]={argv[0],"--wake-engine-report",NULL};
 int code=original_display_fixture_main(2,args);
 bootstrap_churn_stack();
#ifdef SRVROS_INTEL_GT_ARENA
 return arena_fixture_verify(code);
#endif
 assert(gt_bootstrap_native.a==joint.wake && gt_bootstrap_native.g==joint.scope.gpu);
 assert(gt_bootstrap_native.a->fw.gt.a.common.cr3==0x1000 && gt_bootstrap_native.a->maps<=15);
 assert(gt_bootstrap_native.g->io.context==gt_bootstrap_native.a);
 const struct intel_bdw_gt_bootstrap *r=&gt_bootstrap_native.reset;
 if(intel_display_test_bootstrap_expected()) {
  assert(gt_bootstrap_native.complete && gt_bootstrap_native.returned && !gt_bootstrap_native.failed);
  assert(!gt_bootstrap_native.borrow.active && !r->registered && !r->retained && r->complete);
  assert(!intel_bdw_reset_epoch_live(r,&gt_bootstrap_native.post));
  assert(gt_bootstrap_native.epoch_calls==1 && gt_bootstrap_native.epoch==(ring_fixture_disabled?0u:1u) &&
   gt_bootstrap_native.post.engine_epoch==gt_bootstrap_native.epoch+1);
  assert(gt_bootstrap_native.maintained==1 && gt_bootstrap_native.maintain_after==gt_bootstrap_native.maintain_before+1);
  if(!strcmp(bootstrap_mode,"maintain-slow"))assert(gt_bootstrap_native.end-gt_bootstrap_native.begin>100000);
  assert(pulses==2 && bwrites==(preserve_fences?12u:15u) && bcommands==2 && gpu_command==7);
  assert(gt_bootstrap_native.pci_samples==4 && gt_bootstrap_native.pci_reads==36);
  assert(gt_bootstrap_native.full_before && gt_bootstrap_native.full_after && gt_bootstrap_native.post_bound);
  assert(display_access_allowed() && !joint_terminal());
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
  assert(bootstrap_memory_returned() && memory_writes==2 && memory_maps==2 && memory_unmaps==2);
#endif
#ifdef SRVROS_INTEL_GT_RANGE
#ifdef SRVROS_INTEL_GT_SLOT
  assert(gt_slot_reserved && gt_slot_checked && gt_slot_returned && !gt_slot_live());
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
  assert(slot_backing.checks==3 && slot_backing.freed && !test_backing_allocated && !intel_bdw_bcs_port_held(&slot_backing));
  assert(independent_returned && gt_range.completed==2 && gt_range.maps==6 && gt_range.retired==6 && !gt_range.mapped && !range_pa);
  assert(gt_range.reads==792 && range_reads==792 && range_maps==6 && range_unmaps==6);
#else
  assert(gt_range.completed==2 && gt_range.maps==4 && gt_range.retired==4 && !gt_range.mapped && !range_pa);
  assert(gt_range.reads==774 && range_reads==774 && range_maps==4 && range_unmaps==4);
#endif
  assert(gt_range.begin[0]<gt_range.end[0] && gt_range.end[0]<r->begin &&
   gt_range.begin[1]>=gt_memory.d.end && gt_range.end[1]>gt_range.begin[1]);
  assert(!memcmp(gt_range.words[0],gt_range.words[1],sizeof(gt_range.words[0])));
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
  assert(gt_primary.complete && gt_primary.input.samples==2 && gt_primary.access.reads==112 && primary_reads==112);
  assert(gt_primary.access.maps==10 && gt_primary.access.retired==10 && primary_maps==10 && primary_unmaps==10 && !gt_primary.access.mapped);
  assert(gt_primary.begin[0]>=gt_range.end[1] && gt_primary.end[0]<=gt_primary.begin[1] &&
   gt_primary.end[1]<gt_bootstrap_native.guard_begin);
  assert(gt_primary.result.end==5554176 && gt_primary.result.pte_end==6078464 &&
   gt_primary.result.candidate_begin==0x2200000);
#endif
#ifdef SRVROS_INTEL_GT_AUX
  assert(gt_aux.samples==2 && aux_reads==14 && aux_maps==8 && aux_unmaps==8 && !gt_aux.access.mapped);
  assert(gt_primary.begin[0]<=gt_aux.begin[0] && gt_aux.end[0]<=gt_primary.end[0]);
  assert(gt_primary.begin[1]<=gt_aux.begin[1] && gt_aux.end[1]<=gt_primary.end[1]);
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
  assert(gt_flush.samples==4 && flush_reads==100 && flush_maps==28 && flush_aliases==28 && flush_unmaps==28);
  assert(!gt_flush.access.mapped && gt_flush.end[0]<gt_flush.begin[1] && gt_flush.end[1]<=r->begin);
  assert(r->end<gt_flush.begin[2] && gt_flush.end[2]<=gt_memory.d.begin &&
         gt_primary.end[1]<gt_flush.begin[3] && gt_flush.end[3]<gt_bootstrap_native.guard_begin);
#endif
 } else {
  if(!strcmp(bootstrap_mode,"maintain-phase"))assert(gt_bootstrap_native.g->clock_cause & GT_CLOCK_PHASE);
  assert(hit && intel_gt_wake_retained() && !display_access_allowed());
  assert(bwrites==fault_writes && bcommands==fault_commands);
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
  assert(memory_writes==memory_fault_writes && memory_unmaps==memory_fault_unmaps && memory_reads==memory_fault_reads);
#endif
  if(gt_bootstrap_native.complete) {
   /* A later outer-parent failure cannot undo the completed local child or
    * become a whole hardware pass. The remaining original root stays held. */
   assert(!strcmp(bootstrap_mode,"outer-pci") && !gt_bootstrap_native.failed && gt_bootstrap_native.returned);
   assert(gt_bootstrap_native.g->pinned || joint_terminal());
  } else {
   assert(gt_bootstrap_native.failed && gt_bootstrap_native.g->pinned);
#ifdef SRVROS_INTEL_GT_RANGE
   if(!strncmp(bootstrap_mode,"range-",6)) {
    assert(gt_range.completed<2 && !gt_bootstrap_native.returned);
    bool retained=!strcmp(bootstrap_mode,"range-unmap") || !strcmp(bootstrap_mode,"range-map-retained") ||
     !strcmp(bootstrap_mode,"range-clock");
    assert((gt_range.mapped!=0)==retained);
   }
#endif
#ifdef SRVROS_INTEL_GT_PRIMARY
   if(!strncmp(bootstrap_mode,"primary-",8)) {
    assert(gt_range.completed==2 && gt_memory.d.state==2 && bcommands==1 && gpu_command==3);
    assert(!gt_primary.complete && !gt_bootstrap_native.returned);
    assert((range_pa!=0)==(gt_primary.access.mapped!=0));
    assert(gt_primary.access.mapped || gt_primary.access.maps==gt_primary.access.retired);
   }
#endif
#ifdef SRVROS_INTEL_GT_AUX
   if(!strncmp(bootstrap_mode,"aux-",4)) {
    assert(gt_range.completed==2 && gt_memory.d.state==2 && bcommands==1 && gpu_command==3);
    assert(!gt_primary.complete && !gt_bootstrap_native.returned);
    assert((range_pa!=0)==(gt_aux.access.mapped!=0));
   }
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
   if(!strncmp(bootstrap_mode,"flush-",6)) {
    assert(!gt_bootstrap_native.returned && gpu_command==3);
    assert((range_pa!=0)==(gt_flush.access.mapped!=0));
    assert(gt_flush.access.mapped || gt_flush.access.maps==gt_flush.access.retired);
    assert(gt_flush.samples>=2 || !pulses);
   }
#endif
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
   if(!strncmp(bootstrap_mode,"memory-late-",12))assert(gt_bootstrap_native.returned==memory_fault_returned);
   else
#endif
#ifdef SRVROS_INTEL_GT_FLUSH
   if(!strncmp(bootstrap_mode,"smi-",4))
    assert(gt_bootstrap_native.returned==(bootstrap_smi.point==BOOT_SMI_END));
   else
#endif
   assert(gt_bootstrap_native.returned==(!strncmp(bootstrap_mode,"return-",7) || (!strncmp(bootstrap_mode,"final-",6) && strcmp(bootstrap_mode,"final-fence"))));
   if(r->object && !r->complete)assert(r->registered && r->retained);
  }
 }
 printf("native-bootstrap-fixture: boundaries=%u cache=%u aliases=%u reads=%u writes=%u commands=%u pci=%u pulses=%u hit=%u complete=%u first_pci=%u tickets=%u seal_bytes=%u cost_clocks=%u\n",
  boundaries,bcache,balias,breads,bwrites,bcommands,bpci,pulses,hit,gt_bootstrap_native.complete,first_native_pci,receipt_calls,(unsigned)sizeof(bootstrap_admission.bytes),cost_calls);
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
 printf("native-memory-fixture: boundaries=%u aliases=%u resources=%u reads=%u writes=%u maps=%u unmaps=%u complete=%u reason=%u late=%u\n",memory_boundaries,memory_aliases,memory_resources,memory_reads,memory_writes,memory_maps,memory_unmaps,gt_memory.d.state==2,gt_memory.d.reason,memory_late_boundaries);
#endif
 printf("native-bootstrap-phases:");for(unsigned i=0;i<=BDW_BOOT_FAULT;i++)printf(" %u=%u",i,phase_points[i]);puts("");
 return code;
}
