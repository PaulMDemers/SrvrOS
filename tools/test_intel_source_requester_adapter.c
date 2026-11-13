#include <srvros/intel_source_requester.h>
static bool intel_display_test_source_requester_clock(uint64_t *);
static void intel_display_test_source_requester_admission(void);
static void intel_display_test_source_requester_store_admission(unsigned);
static bool intel_display_test_source_requester_read(unsigned,uint32_t *);
static enum intel_gt_write intel_display_test_source_requester_command16(uint16_t);
static bool intel_display_test_source_requester_child(struct intel_source_requester *);
static unsigned intel_display_test_source_requester_command(void);
#define INTEL_TRANSFER_FIXTURE_MAIN transfer_adapter_main
#include "test_intel_startup_transfer_adapter.c"
static unsigned requester_command=7,requester_reads,requester_admissions,requester_clocks,requester_store_admissions;
static bool requester_stall;
static unsigned intel_display_test_source_requester_command(void){return requester_command;}
static bool intel_display_test_source_requester_clock(uint64_t *at) {
 ++requester_clocks;struct intel_source_requester *r=&source_requester_native.result;
 if(requester_stall){*at=r->begin;return true;}
 if(jf("requester-zero",requester_clocks)){*at=0;return true;}
 if(jf("requester-backward",requester_clocks)){*at=1;return true;}
 if(jf("requester-time",requester_clocks))ticks=r->origin+100001;
 if(jf("requester-parent-time",requester_clocks))ticks=r->parent_begin+300001;
 if(jf("requester-source",requester_clocks))r->source=0;
 if(jf("requester-transfer",requester_clocks))r->transfer=0;
 if(jf("requester-provider",requester_clocks))r->provider=0;
 if(jf("requester-epoch",requester_clocks))r->epoch++;
 if(jf("requester-route",requester_clocks))r->kind=3-r->kind;
 if(jf("requester-post",requester_clocks))r->post_mode^=3;
 if(jf("requester-root",requester_clocks))r->lifetime=0;
 if(jf("requester-registration",requester_clocks))source_transfer_native.registered_child=0;
 if(jf("requester-io",requester_clocks))r->io.ctx=0;
 if(jf("requester-busy",requester_clocks))r->busy=false;
 if(jf("requester-borrow",requester_clocks))r->borrows++;
 if(jf("requester-state",requester_clocks))r->state=r->state==ISR_RESTORING?ISR_OPENING:ISR_RESTORING;
 if(strstr(engine_mode,"requester-drain-time") && r->drain_begin && !r->child_entered)ticks=r->drain_begin+10001;
 return false;
}
static void intel_display_test_source_requester_admission(void) {
 ++requester_admissions;
 struct intel_source_requester *r=&source_requester_native.result;
 assert(intel_source_requester_live(r));
 static struct intel_source_requester copy;copy=*r;assert(!intel_source_requester_live(&copy));
 assert(!intel_source_transfer_check(&source_transfer_native) && !intel_source_transfer_close(&source_transfer_native));
 assert(!scanout_memory_hold_end(&buffers_native.hold[0]) && !scanout_memory_hold_end(&buffers_native.hold[1]));
 assert(!intel_source_requester_run(r,r->transfer,r->io,source_requester_child,&nonzero_native));
 if(jf("requester-admission",requester_admissions))source_requester_native.uncertain=true;
}
static void intel_display_test_source_requester_store_admission(unsigned n) {
 ++requester_store_admissions;
 if(jf("requester-store-time",n+1))ticks=source_requester_native.result.origin+100001;
 if(jf("requester-store-source",n+1))source_requester_native.result.epoch++;
}
static bool intel_display_test_source_requester_read(unsigned offset,uint32_t *out) {
 ++requester_reads;
 if(jf("requester-read",requester_reads))return false;
 *out=pci_read_config32(&devices[0],(uint8_t)offset);
 if(jf("requester-float",requester_reads))*out=UINT32_MAX;
 if(jf("requester-drift",requester_reads))*out^=1;
 if(offset==0xa8 && source_requester_native.issued==1 && !source_requester_native.result.child_entered &&
    (strstr(engine_mode,"requester-pending") || strstr(engine_mode,"requester-stuck") || strstr(engine_mode,"requester-drain-time"))) {
  if(!source_requester_native.result.polls || strstr(engine_mode,"requester-stuck"))*out=256;
 }
 return true;
}
static enum intel_gt_write intel_display_test_source_requester_command16(uint16_t value) {
 unsigned n=source_requester_native.issued+1;
 assert((n==1 && requester_command==7 && value==3) || (n==2 && requester_command==3 && value==7));
 if(jf("requester-none",n))return GT_WRITE_NONE;
 if(jf("requester-uncertain",n))return GT_WRITE_UNCERTAIN;
 requester_command=value;return GT_WRITE_DONE;
}
static bool intel_display_test_source_requester_child(struct intel_source_requester *r) {
 assert(r->state==ISR_INHIBITED && requester_command==3 && r->writes==1 && r->registered);
 if(strstr(engine_mode,"requester-child-fail"))return false;
 if(strstr(engine_mode,"requester-child-source"))r->epoch++;
 if(strstr(engine_mode,"requester-child-backing"))gt_bad_pci=true;
 return true;
}
int main(int argc,char **argv) {
 requester_stall=argc==2 && strstr(argv[1],"requester-stall");
 int code=transfer_adapter_main(argc,argv);
 struct intel_source_requester *r=&source_requester_native.result;
 printf("source-requester-fixture: state=%u reason=%u complete=%u registered=%u returns=%u writes=%u issued=%u reads=%u admissions=%u clocks=%u stores=%u command=%u samples=%u polls=%u child=%u child_done=%u epoch=%u kind=%u begin=%llu end=%llu\n",
  r->state,r->reason,r->complete,r->registered,r->returns,r->writes,source_requester_native.issued,requester_reads,requester_admissions,requester_clocks,requester_store_admissions,requester_command,r->samples,r->polls,r->child_entered,r->child_complete,r->epoch,r->kind,(unsigned long long)r->begin,(unsigned long long)r->end);
 if(r->complete)assert(r->state==ISR_CLOSED && r->restored && !r->registered && r->borrows==r->returns && requester_command==7 && r->samples==4 && r->child_complete && r->writes==2);
 else if(r->object)assert(r->registered && !r->returns && !r->restored && source_transfer_native.dependent==r && buffers_native.retained);
 return code;
}
