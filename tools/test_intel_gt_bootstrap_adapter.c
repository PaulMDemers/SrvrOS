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

static uint64_t intel_display_test_command_clock(void);
static void intel_display_test_command_guard(void);
static void intel_display_test_command_return(void);
static bool intel_display_test_command_retire(void);
static uint64_t intel_display_test_command_dma(void);
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
#ifdef SRVROS_INTEL_GT_MEMORY_PREPARE
   if(!strncmp(bootstrap_mode,"memory-late-",12))assert(gt_bootstrap_native.returned==memory_fault_returned);
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
