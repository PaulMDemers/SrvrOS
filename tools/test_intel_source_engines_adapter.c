#include <srvros/intel_source_engines.h>
static bool intel_display_test_source_engines_clock(uint64_t *);
static void intel_display_test_source_engines_guard(void);
static void intel_display_test_source_engines_admission(void);
static void intel_display_test_source_engines_store_admission(unsigned);
static bool intel_display_test_source_engines_read(unsigned,uint32_t *);
static enum intel_gt_write intel_display_test_source_engines_write(unsigned,uint32_t);
static bool intel_display_test_source_engines_child(struct intel_source_engines *);
#define INTEL_REQUESTER_FIXTURE_MAIN requester_adapter_main
#include "test_intel_source_requester_adapter.c"
static uint32_t engines_control[5];
static unsigned engines_reads,engines_admissions,engines_clocks,engines_stores,engines_actual,engines_final_clock;
static bool engines_stall;
static unsigned engines_guards,engines_last_target_guard;
static void intel_display_test_source_engines_guard(void) {
 ++engines_guards;
 if(jf("engines-guard",engines_guards))source_engines_native.uncertain=true;
}
static void engines_target_guard(void) {
 assert(engines_guards>engines_last_target_guard);
 engines_last_target_guard=engines_guards;
}
static bool intel_display_test_source_engines_clock(uint64_t *at) {
 ++engines_clocks;struct intel_source_engines *r=&source_engines_native.result;
 if(engines_stall){*at=r->begin;return true;}
 if(jf("engines-zero",engines_clocks)){*at=0;return true;}
 if(jf("engines-backward",engines_clocks)){*at=1;return true;}
 if(jf("engines-time",engines_clocks))ticks=r->origin+100001;
 if(jf("engines-owner-time",engines_clocks))ticks=r->parent_begin+300001;
 if(jf("engines-source",engines_clocks))r->source=0;
 if(jf("engines-requester",engines_clocks))r->requester=0;
 if(jf("engines-transfer",engines_clocks))r->transfer=0;
 if(jf("engines-provider",engines_clocks))r->provider=0;
 if(jf("engines-epoch",engines_clocks))r->epoch++;
 if(jf("engines-route",engines_clocks))r->kind=3-r->kind;
 if(jf("engines-mode",engines_clocks))r->post_mode^=3;
 if(jf("engines-root",engines_clocks))r->lifetime=0;
 if(jf("engines-registration",engines_clocks))source_requester_native.result.registered_child=0;
 if(jf("engines-io",engines_clocks))r->io.ctx=0;
 if(jf("engines-busy",engines_clocks))r->busy=false;
 if(jf("engines-borrow",engines_clocks))r->borrows++;
 if(jf("engines-state",engines_clocks))r->state=ISE_CLOSED;
 if(jf("engines-count",engines_clocks) && r->phase<4)r->counts[r->phase]++;
 if(jf("engines-complete",engines_clocks))r->complete=true;
 if(jf("engines-restored",engines_clocks))r->restored=true;
 if(engines_final_clock) {
  unsigned n=engines_final_clock;engines_final_clock=0;
  if(jf("engines-final-count",n) && r->phase<4)r->counts[r->phase]--;
  if(jf("engines-final-origin",n)){ticks=r->phase_begin[n-1]+10001;r->phase_begin[n-1]=ticks;}
  if(jf("engines-final-phase",n))r->phase++;
  if(jf("engines-final-step",n))r->step++;
  if(jf("engines-final-index",n))r->index=(r->index+1)%5;
  if(jf("engines-final-samples",n))r->samples++;
  if(jf("engines-final-writes",n))r->writes++;
  if(jf("engines-final-issued",n))source_engines_native.issued++;
  if(jf("engines-final-admit",n))source_engines_native.admission[n-1]^=1;
  if(jf("engines-final-child",n))r->child_complete=false;
  if(jf("engines-final-state",n))r->state=ISE_HELD;
 }
 return false;
}
static void intel_display_test_source_engines_admission(void) {
 ++engines_admissions;
 if(jf("engines-admission",engines_admissions))source_engines_native.uncertain=true;
}
static void intel_display_test_source_engines_store_admission(unsigned n) {
 ++engines_stores;engines_final_clock=n+1;
 if(jf("engines-store-time",n+1))ticks=source_engines_native.result.origin+100001;
 if(jf("engines-phase-time",n+1))ticks=source_engines_native.result.phase_begin[n]+10001;
 if(jf("engines-store-source",n+1))source_engines_native.result.epoch++;
}
static bool intel_display_test_source_engines_read(unsigned e,uint32_t *out) {
 engines_target_guard();++engines_reads;assert(e<6 && requester_command==3);
 if(jf("engines-read",engines_reads))return false;
 *out=e<5?engines_control[e]:0;
 struct intel_source_engines *r=&source_engines_native.result;
 if(e<5 && r->attempts>r->acks && r->step<10 && source_engines_native.issued==r->step+1 &&
    (strstr(engine_mode,"engines-poll") || strstr(engine_mode,"engines-stuck")) &&
    (!r->phase_polls[r->step] || strstr(engine_mode,"engines-stuck"))) *out=r->step<5?1:2;
 if(jf("engines-float",engines_reads))*out=UINT32_MAX;
 if(jf("engines-drift",engines_reads))*out^=1;
 return true;
}
static enum intel_gt_write intel_display_test_source_engines_write(unsigned e,uint32_t v) {
 engines_target_guard();unsigned n=source_engines_native.issued;assert(n<10 && requester_command==3 && e==(n<5?n:9-n) && v==(n<5?0x10001u:0x10000u));
 if(jf("engines-none",n+1))return GT_WRITE_NONE;
 if(jf("engines-uncertain",n+1))return GT_WRITE_UNCERTAIN;
 if(jf("engines-uncertain-issued",n+1)){engines_actual++;engines_control[e]=n<5?3:0;return GT_WRITE_UNCERTAIN;}
 engines_actual++;engines_control[e]=n<5?3:0;return GT_WRITE_DONE;
}
static bool intel_display_test_source_engines_child(struct intel_source_engines *r) {
 assert(intel_source_engines_live(r) && r->registered && r->state==ISE_HELD && requester_command==3);
 for(unsigned e=0;e<5;e++)assert(engines_control[e]==3);
 struct intel_source_engines copy=*r;assert(!intel_source_engines_live(&copy));
 assert(!intel_source_engines_run(r,r->requester,r->io,source_engines_child,&nonzero_native));
 assert(!intel_source_requester_child_returned(r->requester));
 if(strstr(engine_mode,"engines-child-fail"))return false;
 if(strstr(engine_mode,"engines-child-source"))r->epoch++;
 if(strstr(engine_mode,"engines-child-backing"))gt_bad_pci=true;
 return true;
}
int main(int argc,char **argv) {
 engines_stall=argc==2 && strstr(argv[1],"engines-stall");
 int code=requester_adapter_main(argc,argv);struct intel_source_engines *r=&source_engines_native.result;
 printf("source-engines-fixture: state=%u reason=%u complete=%u registered=%u returns=%u writes=%u issued=%u actual=%u reads=%u admissions=%u guards=%u clocks=%u stores=%u samples=%u polls=%u child=%u child_done=%u epoch=%u kind=%u begin=%llu end=%llu command=%u\n",
 r->state,r->reason,r->complete,r->registered,r->returns,r->writes,source_engines_native.issued,engines_actual,engines_reads,engines_admissions,engines_guards,engines_clocks,engines_stores,r->samples,r->polls,r->child_entered,r->child_complete,r->epoch,r->kind,(unsigned long long)r->begin,(unsigned long long)r->end,requester_command);
 if(r->complete)assert(r->state==ISE_CLOSED && r->restored && !r->registered && r->samples==4 && r->writes==10 && r->acks==10 && r->borrows==r->returns && r->child_complete);
 if(r->complete) {
  struct intel_source_engines saved=*r;
  assert(!intel_source_engines_deadline(r,r->end+1));
  assert(!intel_source_engines_run(r,r->requester,r->io,source_engines_child,&nonzero_native));
  assert(!memcmp(&saved,r,sizeof(saved)));
 }
 else if(r->object)assert(r->registered && !r->returns && source_requester_native.result.dependent==r && buffers_native.retained);
 return code;
}
