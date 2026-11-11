#include <stdint.h>
#include <stdbool.h>
static bool intel_display_test_entry_read(uint32_t [8]);
#define INTEL_JOINT_FIXTURE_MAIN original_joint_fixture_main
#include "test_intel_joint_adapter.c"
static unsigned entry_reads;
static bool intel_display_test_entry_read(uint32_t out[8]) {
 ++entry_reads;
 if(entry_reads==1) {
  struct wake_adapter *a=entry.a;struct intel_gt_access *g=entry.g;const struct intel_completion_result *c=entry.c;
  unsigned before=g->write_attempts;
  assert(!entry_run(NULL,NULL,NULL,true));
  assert(entry.a==a && entry.g==g && entry.c==c);
  assert(!intel_gt_access_close(g) && g->write_attempts==before && entry.result.borrow.active);
 }
 assert(wake_irq && wake_ack==1 && !display_access_allowed() && hda_command==2);
 assert(entry.result.borrow.active && entry.result.borrow.parent==joint.scope.gpu);
 const uint32_t values[8]={0,0,0,0,1,0,512,0};
 for(unsigned i=0;i<8;i++)out[i]=values[i];
 if(jf("entry-read",entry_reads))return false;
 if(jf("entry-drift",entry_reads))out[7]=1;
 if(jf("entry-busy",entry_reads))out[0]=8;
 if(jf("entry-float",entry_reads))out[0]=UINT32_MAX;
 if(jf("entry-time",entry_reads))ticks+=100001;
 if(jf("entry-backward",entry_reads))ticks=1;
 if(jf("entry-epoch",entry_reads))++entry.g->engine_epoch;
 if(entry_reads==1)for(unsigned i=0;i<92;i++)if(jf("entry-word",i+1)) {
  struct intel_completion_result *c=(struct intel_completion_result *)entry.c;
  if(i<80)c->words[i/40][(i%40)/8][i%8]^=1;
  else c->context[(i-80)/6][(i-80)%6]^=1;
 }
 if(jf("entry-source-begin",entry_reads))((struct intel_completion_result *)entry.c)->begin_us++;
 if(jf("entry-source-last",entry_reads))((struct intel_completion_result *)entry.c)->last_us++;
 if(jf("entry-extra-borrow",entry_reads)) {
  static struct intel_gt_borrow other;assert(intel_gt_access_borrow(entry.g,&other));
 }
 if(jf("entry-source-time",entry_reads))((struct intel_completion_result *)entry.c)->end_us++;
 if(jf("entry-source-word",entry_reads))((struct intel_completion_result *)entry.c)->words[0][0][6]^=1;
 if(jf("entry-copy-census",entry_reads)) {
  static struct intel_completion_result copy;copy=*entry.c;copy.object=&copy;entry.result.census=&copy;
 }
 if(jf("entry-copy-owner",entry_reads)) {
  static struct intel_gt_access copy;copy=*entry.g;copy.object=&copy;entry.result.parent=&copy;
 }
 if(jf("entry-borrow",entry_reads))entry.g->dependents=NULL;
 if(jf("entry-alias",entry_reads))pa[INTEL_CAPTURE_PAGES+1+7]^=4096;
 return true;
}
int main(int argc,char **argv) {
 int code=original_joint_fixture_main(argc,argv);churn_stack();
 if(entry.result.object) {
  assert(entry.result.object==&entry.result && entry.a==joint.wake && entry.g==joint.scope.gpu);
  assert(!entry.result.bcs.attempts && !entry.result.bcs.writes && !entry.result.bcs.uncertain);
  if(entry.result.borrow.object)assert(entry.result.borrow.object==&entry.result.borrow);
  if(entry.result.borrow.active)assert(!display_access_allowed());
 }
 if(!strcmp(engine_mode,"report"))assert(entry.result.complete && entry_reads==4 && entry.result.returned && entry.result.bound && entry.result.epoch==0 && entry.result.epoch_after==0);
 printf("entry-fixture: reads=%u checks=%u retained=%u complete=%u\n",entry_reads,entry.result.checks,entry.result.borrow.active,entry.result.complete);
 return code;
}
