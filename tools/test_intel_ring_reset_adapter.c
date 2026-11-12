/* Composed actual native adapter: shared ring and original joint fixtures. */
#include <stdint.h>
#include <stdbool.h>
static void intel_display_test_ring_reset_guard(void *);
static void intel_display_test_ring_reset_prepare(void);
static bool intel_display_test_ring_reset_clock(uint64_t *);
#include "test_intel_ring_transition_adapter.c"
static unsigned reset_guards,reset_clocks,reset_prepare_calls;
static bool reset_tie;
static void intel_display_test_ring_reset_prepare(void) {++reset_prepare_calls;assert(ring_reset_native.result.complete);}
static void intel_display_test_ring_reset_guard(void *ctx) {
 struct ring_reset_adapter *p=ctx;struct intel_ring_reset *r=&p->result;++reset_guards;
 if(reset_guards==1) {
  struct wake_adapter *a=p->a;struct intel_gt_access *g=p->g;struct intel_takeover_result *t=p->takeover;
  assert(!ring_reset_run(a,g,t,true));
  assert(p->a==a && p->g==g && p->takeover==t && r->entry==&ring_native.result);
 }
 if(jf("reset-takeover-source",reset_guards) && p->takeover->object)p->takeover->census=ring_native.c;
 if(reset_guards==12) {
  if(jf("reset-takeover-stamp",1))p->takeover->begin_us++;
  if(jf("reset-takeover-stamp",2))p->takeover->last_us++;
  if(jf("reset-takeover-stamp",3))p->takeover->end_us++;
 }
 if(jf("reset-epoch",reset_guards))++p->g->engine_epoch;
 if(jf("reset-provider",reset_guards))r->provider=0;
 if(jf("reset-return",reset_guards))ring_native.result.borrow.active=true;
 if(jf("reset-copy-entry",reset_guards)) {
  static struct intel_ring_transition copy;copy=ring_native.result;copy.object=&copy;r->entry=&copy;
 }
 if(jf("reset-copy-census",reset_guards)) {
  static struct intel_completion_result copy;copy=ring_native.result.post;copy.object=&copy;r->source=&copy;
 }
 if(jf("reset-copy-parent",reset_guards)) {
  static struct intel_gt_access copy;copy=*p->g;copy.object=&copy;r->parent=&copy;
 }
 if(jf("reset-copy-takeover",reset_guards)) {
  static struct intel_takeover_result copy;copy=*p->takeover;copy.object=&copy;r->takeover=&copy;
 }
 if(jf("reset-extra-borrow",reset_guards)) {static struct intel_gt_borrow other;assert(intel_gt_access_borrow(p->g,&other));}
 if(reset_guards==1) {
  for(unsigned i=0;i<92;i++) {
   struct intel_completion_result *c=jf("reset-post-word",i+1)?&ring_native.result.post:
    jf("reset-entry-source-word",i+1)?(struct intel_completion_result *)ring_native.c:0;
   if(c){if(i<80)c->words[i/40][(i%40)/8][i%8]^=1;else c->context[(i-80)/6][(i-80)%6]^=1;}
  }
  if(jf("reset-post-stamp",1))ring_native.result.post.begin_us++;
  if(jf("reset-post-stamp",2))ring_native.result.post.last_us++;
  if(jf("reset-post-stamp",3))ring_native.result.post.end_us++;
 }
}
static bool intel_display_test_ring_reset_clock(uint64_t *out) {
 ++reset_clocks;
 if(reset_tie && reset_clocks==1){*out=ring_native.result.end;return true;}
 if(jf("reset-clock-age",reset_clocks))ticks=ring_reset_native.result.source_end+100000;
 if(jf("reset-clock-zero",reset_clocks)){*out=0;return true;}
 if(jf("reset-clock-backward",reset_clocks)){*out=1;return true;}
 if(jf("reset-clock-time",reset_clocks))ticks+=100001;
 return false;
}
int main(int argc,char **argv) {
 char selected[160];char *args[3]={argv[0],selected,0};
 ring_fixture_disabled=true;ring_ctl=0;
 if(argc==2 && !strcmp(argv[1],"--wake-engine-reset-tie")) {
  reset_tie=true;snprintf(selected,sizeof selected,"--wake-engine-report");argv=args;
 }
 const char *prefix="--wake-engine-reset-enabled-";
 if(argc==2 && !strncmp(argv[1],prefix,strlen(prefix))) {
  ring_fixture_disabled=false;ring_ctl=0xf001;
  assert(strlen(argv[1]+strlen(prefix))<120);snprintf(selected,sizeof selected,"--wake-engine-%s",argv[1]+strlen(prefix));argv=args;
 }
 if(argc==2 && !strcmp(argv[1],"--wake-engine-reset-nonzero-disabled")){ring_fixture_disabled=false;ring_ctl=0xf000;}
 if(argc==2 && !strcmp(argv[1],"--wake-engine-reset-enabled")){ring_fixture_disabled=false;ring_ctl=0xf001;}
 int code=original_joint_fixture_main(argc,argv);churn_stack();
 const struct intel_ring_transition *e=&ring_native.result;const struct intel_ring_reset *r=&ring_reset_native.result;
 if(r->object && r->uncertain)assert(joint.scope.gpu->pinned && !display_access_allowed());
 if(r->complete)assert(!intel_ring_entry_reset_ready(e));
 if(!strcmp(engine_mode,"report"))assert(e->complete && r->complete && r->source==&e->post && r->takeover->census==&e->post && r->takeover->returned && !ring_writes && joint.scope.complete);
 if(!strcmp(engine_mode,"reset-enabled"))assert(e->complete && e->kind==IRT_KIND_ENABLED && ring_writes==1 && !r->eligible && !r->complete && !r->takeover->object && joint.scope.hda.complete && !joint.scope.pinned);
 printf("ring-reset-fixture: guards=%u clocks=%u prepare_calls=%u ring_reads=%u ring_clocks=%u core_clocks=%u census_reads=%u reader_reads=%u hda_reads=%u endpoints=%u cache=%u\n",reset_guards,reset_clocks,reset_prepare_calls,ring_reads,ring_clocks,ring_core_clocks,ring_census_reads,ring_reader_calls,hda_reads,hda_endpoints,hda_cache);
 return code;
}
