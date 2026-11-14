#include <stdint.h>
#include <stdbool.h>
static bool intel_display_test_nonzero_source_clock(uint64_t *);
static void intel_display_test_nonzero_source_admission(unsigned);
static bool intel_display_test_nonzero_source_read(unsigned,uint32_t *);
#define INTEL_NONZERO_FIXTURE_MAIN original_nonzero_fixture_main
#include "test_intel_nonzero_reset_adapter.c"
static unsigned source_clocks,source_reads,source_admissions;
static bool intel_display_test_nonzero_source_clock(uint64_t *at) {
 struct intel_nonzero_source *s=&nonzero_native.source;
 if(!s->object || !s->registered)return false;
 ++source_clocks;
#ifdef SRVROS_INTEL_STARTUP_SOURCE
 if(jf("source-clock-route",source_clocks))nonzero_native.r.entry_kind=3-nonzero_native.r.entry_kind;
 if(jf("source-clock-mode",source_clocks))nonzero_native.r.startup=false;
#endif
 if(jf("source-zero",source_clocks)){*at=0;return true;}
 if(jf("source-backward",source_clocks)){*at=1;return true;}
 if(jf("source-time",source_clocks))ticks=s->origin+100001;
 if(jf("source-parent-time",source_clocks))ticks=s->parent_begin+300001;
 if(jf("source-clock-parent",source_clocks))s->parent=NULL;
 if(jf("source-clock-post",source_clocks))nonzero_native.r.post.words[0][0][0]^=1;
 if(jf("source-clock-borrow",source_clocks))nonzero_native.r.borrow.active=false;
 return false;
}
static void intel_display_test_nonzero_source_admission(unsigned n) {
 struct nonzero_adapter *p=&nonzero_native;struct intel_nonzero_source *s=&p->source;
 assert(n==source_admissions++ && s->registered && intel_nonzero_source_live(s));
 if(jf("source-alias",n+1))pa[INTEL_CAPTURE_PAGES+14]^=4096;
 if(jf("source-epoch",n+1))p->g->engine_epoch++;
 if(jf("source-provider",n+1))s->provider=NULL;
 if(jf("source-parent",n+1))s->parent=NULL;
 if(jf("source-post-stamp",n+1))p->r.post.end_us++;
 if(jf("source-input",n+1))p->source_input.original[0]^=4096;
 if(jf("source-registration",n+1))p->r.dependent=NULL;
 if(jf("source-count",n+1))p->r.child_borrows=0;
 if(jf("source-copy",n+1)){static struct intel_completion_result copy;copy=p->r.post;copy.object=&copy;s->post=&copy;}
 if(jf("source-reenter",n+1)) {
  struct intel_nonzero_source copy=*s;
  assert(!intel_nonzero_source_run(s,&p->r,&p->source_input,(struct intel_nonzero_source_io){0}));
  assert(!memcmp(s,&copy,sizeof copy));
 }
}
static bool intel_display_test_nonzero_source_read(unsigned off,uint32_t *out) {
 struct nonzero_adapter *p=&nonzero_native;struct intel_nonzero_source *s=&p->source;
 unsigned n=source_reads++;
 assert(n<6 && off==INTEL_WINDOW_PAGE+(n%3==1?0:4) && s->registered && p->r.borrow.active && p->r.child_borrows==1 && !p->r.child_returns);
 assert(nz_writes==12 && hda_command==2 && p->g->engine_epoch==p->r.epoch_before+1 && wake_irq);
 if(jf("source-read",n+1))return false;
 *out=(uint32_t)(s->expected>>(n%3==1?0:32));
 if(jf("source-drift",n+1))*out^=1;
 if(jf("source-read-time",n+1))ticks=s->origin+100001;
 return true;
}
#ifndef INTEL_SOURCE_FIXTURE_MAIN
#define INTEL_SOURCE_FIXTURE_MAIN main
#endif
int INTEL_SOURCE_FIXTURE_MAIN(int argc,char **argv) {
 int result=original_nonzero_fixture_main(argc,argv);
 const struct intel_nonzero_source *s=&nonzero_native.source;
 if(s->object) {
  assert(s->object==s);
  if(nonzero_native.r.complete)assert(s->complete && s->stable && s->blocked && s->returned && !s->registered && s->count==6 && source_reads==6 && nonzero_native.r.child_borrows==1 && nonzero_native.r.child_returns==1 && nonzero_native.r.observer_complete);
  if(s->reason)assert(!nonzero_native.r.complete && nonzero_native.a->uncertain && nonzero_native.r.uncertain);
  assert(!intel_nonzero_source_live(s));
 }
 printf("nzsource-fixture: clocks=%u reads=%u admissions=%u\n",source_clocks,source_reads,source_admissions);
 return result;
}
