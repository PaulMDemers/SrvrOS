/* Real native adapter, retained HDA/GPU owner, modeled bounded hardware. */
#include <stdint.h>
#include <stdbool.h>
static uint16_t intel_display_test_gpu_hold_command(void);
static void intel_display_test_gpu_hold_admission(void);
static bool intel_display_test_gpu_hold_read(unsigned,uint32_t *);
struct pci_device;
#include <srvros/intel_gt_access.h>
static enum intel_gt_write intel_display_test_gpu_hold_command16(const struct pci_device *,unsigned,uint16_t);
#define INTEL_RING_RESET_FIXTURE_MAIN original_entry_fixture_main
#include "test_intel_ring_reset_adapter.c"
static uint16_t gpu_command=7;
static bool gpu_pending_mode;
static unsigned gpu_reads,gpu_attempts,gpu_stores,gpu_admissions;
static uint16_t intel_display_test_gpu_hold_command(void){return gpu_command;}
static void intel_display_test_gpu_hold_admission(void) {
 ++gpu_admissions;
 if(jf("gpu-last-provider",gpu_admissions))ring_reset_native.result.provider=0;
 if(jf("gpu-last-entry-return",gpu_admissions))ring_native.result.borrow.object=0;
 if(jf("gpu-last-handoff-clock",gpu_admissions))ring_reset_native.result.end=0;
 if(jf("gpu-last-parent",gpu_admissions))ring_reset_native.result.parent=0;
 if(jf("gpu-last-entry",gpu_admissions))ring_reset_native.result.entry=0;
 if(jf("gpu-last-census",gpu_admissions))ring_reset_native.result.takeover->census=ring_native.c;
 if(jf("gpu-last-takeover",gpu_admissions))ring_reset_native.result.takeover->last_us++;
 if(jf("gpu-last-bound",gpu_admissions))ring_reset_native.result.bound=false;

 if(jf("gpu-last-alias",gpu_admissions))pa[INTEL_CAPTURE_PAGES+14]^=4096;
 if(jf("gpu-last-original",gpu_admissions))gpu_hold_native.result.claim->captured.original[0]^=1;
 if(jf("gpu-last-claim",gpu_admissions))gpu_hold_native.result.claim=0;
 if(jf("gpu-last-time",gpu_admissions))ticks+=100001;
 if(jf("gpu-last-zero",gpu_admissions))held_reset_clock_fault=1;
 if(jf("gpu-last-backward",gpu_admissions))held_reset_clock_fault=2;
}
static bool intel_display_test_gpu_hold_read(unsigned off,uint32_t *out) {
 ++gpu_reads;
 assert(wake_irq && !display_access_allowed() && intel_gpu_requester_hold_live(&gpu_hold_native.result));
 if(jf("gpu-read",gpu_reads))return false;
 *out=pci_read_config32(&devices[0],(uint8_t)off);
 if(off==0xa8 && gpu_command==3 && (!strcmp(engine_mode,"gpu-timeout") || ((gpu_pending_mode || !strcmp(engine_mode,"gpu-pending")) && gpu_hold_native.result.polls<3)))*out=256;
 if(jf("gpu-drift",gpu_reads))*out^=1;
 if(jf("gpu-source",gpu_reads))gpu_hold_native.result.borrow.claim=0;
 return true;
}
static enum intel_gt_write intel_display_test_gpu_hold_command16(const struct pci_device *d,unsigned off,uint16_t value) {
 assert(d==&devices[0] && off==4 && wake_irq && !display_access_allowed());
 ++gpu_attempts;assert(gpu_attempts<=2 && value==(gpu_attempts==1?3:7));
 if(jf("gpu-none",gpu_attempts))return GT_WRITE_NONE;
 gpu_command=value;++gpu_stores;
 return jf("gpu-uncertain",gpu_attempts)?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
int main(int argc,char **argv) {
 char selected[180];char *args[3]={argv[0],selected,0};
 const char *prefix="--wake-engine-requester-pending-";
 if(argc==2 && !strncmp(argv[1],prefix,strlen(prefix))) {
  gpu_pending_mode=true;assert(strlen(argv[1]+strlen(prefix))<140);
  snprintf(selected,sizeof selected,"--wake-engine-%s",argv[1]+strlen(prefix));argv=args;
 }
 int code=original_entry_fixture_main(argc,argv);
 if(wake_mode) {
  if(!strncmp(engine_mode,"gpu-last-",9)) {
   unsigned point=(unsigned)atoi(strchr(engine_mode,':')+1);
   assert(gpu_admissions==point && gpu_reads+gpu_attempts==point-1);
  }
  const struct intel_gpu_requester_hold *r=&gpu_hold_native.result;
  if(!strcmp(engine_mode,"report") || !strcmp(engine_mode,"gpu-pending"))assert(r->complete && gpu_stores==2 && gpu_command==7 && joint.scope.complete);
  printf("gpu-fixture: reads=%u attempts=%u stores=%u admissions=%u polls=%u live_checks=%u\n",gpu_reads,gpu_attempts,gpu_stores,gpu_admissions,r->polls,gpu_hold_native.p?gpu_hold_native.p->trial->live_checks:0);
 }
 return code;
}
