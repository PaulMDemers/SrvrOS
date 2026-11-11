/* Actual native adapter with modeled PCI/MMIO, one original root/exclusion. */
#include <stdint.h>
#include <stdbool.h>
static uint64_t intel_display_test_hda_irq_save(void);
static bool intel_display_test_hda_irq_off(void);
static void intel_display_test_hda_irq_restore(uint64_t);
static bool intel_display_test_hda_uc(uint64_t);
static bool intel_display_test_hda_read(unsigned,unsigned,uint32_t *);
static uint32_t intel_display_test_hda_config(unsigned);
static bool intel_display_test_hda_map(void);
static bool intel_display_test_hda_retire(void);
static void intel_display_test_joint_admission(void);
static void intel_display_test_joint_prepare(void);
static void intel_display_test_joint_entry(unsigned);
static bool intel_display_test_joint_inhibited(void);
static bool intel_display_test_joint_claim_active(void *);
static void intel_display_test_joint_snapshot_delay(void);
struct pci_device;
static bool intel_display_test_hda_command16(const struct pci_device *,unsigned,uint16_t);
#define main original_display_fixture_main
#include "test_intel_display_probe.c"
#undef main
static unsigned hda_reads,hda_cache,hda_attempts,hda_stores,hda_endpoints;
static uint16_t hda_command=6;
static bool reentered;
static bool jf(const char *name,unsigned point) {
 const char *colon=strchr(engine_mode,':');
 return colon && (size_t)(colon-engine_mode)==strlen(name) && !strncmp(engine_mode,name,strlen(name)) &&
  point==(unsigned)atoi(colon+1);
}
static uint64_t intel_display_test_hda_irq_save(void){wake_irq=true;return 512;}
static bool intel_display_test_hda_irq_off(void){return wake_irq;}
static void intel_display_test_hda_irq_restore(uint64_t f){assert(f==512);wake_irq=false;}
static bool intel_display_test_hda_uc(uint64_t p){assert(p==INTEL_HDA_BASE && wake_irq && !display_access_allowed());return !jf("joint-cache",++hda_cache);}
static bool intel_display_test_hda_map(void){return !jf("joint-map",1);}
static bool intel_display_test_hda_retire(void){assert(!joint.scope.gpu->pinned && !joint.scope.gpu->borrows && !joint.scope.gpu->dependents);return !jf("joint-retire",1);}
static uint32_t intel_display_test_hda_config(unsigned off) {
 if(joint.scope.busy && joint.scope.state==IHG_ACTIVE && joint.scope.hda.busy && !reentered) {
  reentered=true;unsigned before=hda_endpoints;
  assert(!intel_hda_gpu_check(&joint.scope) && !intel_hda_gpu_close(&joint.scope,false));
  assert(!intel_hda_owner_unpin(&joint.scope.hda,&joint.scope.borrow));
  assert(!intel_hda_owner_validate(&joint.scope.hda,&joint.scope.borrow));
  assert(before==hda_endpoints);
 }
 if(off==0)++hda_endpoints;
 switch(off) {
 case 0:return jf("joint-endpoint",hda_endpoints)?0xffffffff:0x160c8086;
 case 4:return 0x00100000|hda_command;
 case 8:return 0x04030009;
 case 0x2c:return 0x011b106b;
 case 0x10:return 0xc1610004;
 case 0x34:return 0x40;
 case 0x40:return 0x00030001;
 default:return 0;
 }
}
static bool intel_display_test_hda_read(unsigned off,unsigned width,uint32_t *out) {
 assert(wake_irq && !display_access_allowed() && intel_hda_read_allowed(off,width));
 assert(joint.inspection && joint.h.mapped);++hda_reads;
 if(jf("joint-read",hda_reads))return false;
 *out=off==0?0x3001:off==8?1:off>=0x80 && off<0xe0 && (off-0x80)%32==2?4:0;
 if(jf("joint-drift",hda_reads))*out^=1;
 if(jf("joint-read-time",hda_reads))ticks+=400001;
 if(jf("joint-alias",hda_reads))pa[INTEL_CAPTURE_PAGES+15]^=4096;
 return true;
}
static void intel_display_test_joint_admission(void) {
 if(jf("joint-store-time",hda_attempts+1))ticks+=400001;
 if(jf("joint-store-zero",hda_attempts+1))held_reset_clock_fault=1;
 if(jf("joint-store-backward",hda_attempts+1))held_reset_clock_fault=2;
}
static void intel_display_test_joint_prepare(void) {if(jf("joint-upper",1))backing.pte[0]|=1ull<<32;}
static bool intel_display_test_hda_command16(const struct pci_device *d,unsigned off,uint16_t value) {
 assert(d==&devices[3] && off==4 && wake_irq && !display_access_allowed() && !joint.inspection);
 ++hda_attempts;assert(hda_attempts<=2 && value==(hda_attempts==1?2:6));
 if(value==6)assert(!joint.scope.gpu->pinned && !joint.scope.gpu->borrows && !joint.scope.gpu->dependents);
 if(jf("joint-none",hda_attempts))return false;
 hda_command=value;++hda_stores;
 return !jf("joint-uncertain",hda_attempts);
}
static void intel_display_test_joint_entry(unsigned n) {
 assert(hda_command==2 && joint.scope.hda.pins==1);
 if(jf("joint-drop-hda-child",n))joint.scope.borrow.dependent=0;
 if(jf("joint-drop-hda-list",n))joint.scope.hda.dependents=0;
 if(jf("joint-drop-hda-count",n))joint.scope.borrow.dependent_borrows=0;
 if(jf("joint-drop-claim",n))joint.scope.claim->child->dependent=0;
}
static bool intel_display_test_joint_inhibited(void) {
 return hda_command==2 && intel_hda_owner_borrowed(&joint.scope.hda,&joint.scope.borrow) && joint.scope.borrow.dependent==&joint.scope;
}
static bool intel_display_test_joint_claim_active(void *claim) {
 assert(claim==joint.scope.claim && ((struct intel_interval_claim *)claim)->state==IIC_ACTIVE);
 return strcmp(engine_mode,"joint-leave")!=0;
}
static void intel_display_test_joint_snapshot_delay(void) {
 if(strcmp(engine_mode,"joint-return-deadline") || !joint.scope.claim || !joint.scope.claim->object)return;
 const struct engine_prepare_adapter *adapter=joint.scope.claim->io.ctx;
 const struct intel_engine_prepare_result *prepare=&adapter->trial->action;
 if(prepare->return_begin_us && prepare->borrow.active)
  ticks=prepare->begin_us+INTEL_ENGINE_PREPARE_TOTAL_US;
}
__attribute__((noinline)) static void churn_stack(void) {
 volatile uint8_t bytes[32768];
 for(unsigned i=0;i<sizeof(bytes);i++)bytes[i]=(uint8_t)(i*17+3);
}
int main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--joint-codec")) {
  reset();reporting=true;backing_attempted=true;backing.status=1;backing.count=1356;
  for(unsigned i=0;i<backing.count;i++)backing.pte[i]=((uint64_t)(0x12340000u+i/64)<<32)|(0x90000001ull+i*4096);
  backing.pte[0]^=1ull<<33; /* one full-width chunk plus common-upper chunks */
  assert(intel_display_backing_probe_report());return 0;
 }
 int code=original_display_fixture_main(argc,argv);
 churn_stack();
 if(wake_mode) {
  if(joint.entry)assert(joint.wake && joint.wake->fw.gt.a.common.cr3==0x1000 && joint.snapshot && joint.snapshot->mmio==0xc0000000);
  if(joint.scope.object) {
   const struct intel_gt_access *g=joint.scope.gpu;
   assert(g && (!g->object || g->object==g));
   if(g->object)assert(g->io.context==joint.wake);
   if(joint.scope.claim && joint.scope.claim->object) {
    struct engine_prepare_adapter *p=joint.scope.claim->io.ctx;
    assert(p && p->g==g && p->a==joint.wake && &p->trial->claim==joint.scope.claim);
   }
  }
  if(!strcmp(engine_mode,"report"))assert(joint.scope.complete && hda_stores==2 && hda_reads==232 &&
   joint.maps==1 && joint.retired==1 && joint.borrows==4 && joint.returns==4 && interval_reads==12 && reentered);
  printf("joint-fixture: reads=%u endpoints=%u cache=%u attempts=%u stores=%u claims=%u\n",hda_reads,hda_endpoints,hda_cache,hda_attempts,hda_stores,interval_reads);
 }
 return code;
}
