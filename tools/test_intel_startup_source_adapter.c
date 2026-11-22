/* Same actual adapter, a distinct image and state-aware API. */
#define INTEL_SOURCE_FIXTURE_MAIN original_source_fixture_main
#include "test_intel_nonzero_source_adapter.c"
#ifndef INTEL_STARTUP_FIXTURE_MAIN
#define INTEL_STARTUP_FIXTURE_MAIN main
#endif
int INTEL_STARTUP_FIXTURE_MAIN(int argc,char **argv) {
 char selected[160];char *args[3]={argv[0],selected,0};
 if(argc==2 && !strncmp(argv[1],"--wake-engine-entry-off-",24)) {
  ring_fixture_disabled=true;ring_ctl=0;
  assert(strlen(argv[1]+24)<120);snprintf(selected,sizeof selected,"--wake-engine-%s",argv[1]+24);argv=args;
 }
 if(argc==2 && !strcmp(argv[1],"--wake-engine-entry-nonzero-disabled"))ring_ctl=0xf000;
 if(argc==2 && (!strcmp(argv[1],"--wake-engine-ring-off") || !strcmp(argv[1],"--wake-engine-ring-zero")))ring_ctl=0;
 int code=original_source_fixture_main(argc,argv);
 if(ring_native.result.kind==IRT_KIND_DISABLED && ring_native.result.complete) {
  /* Even this image's old API refuses kind2 before a borrow or target access. */
  static struct intel_nonzero_reset legacy;
  struct nonzero_adapter *p=&nonzero_native;
  struct intel_nonzero_io io={p,nonzero_guard,nonzero_sample,nonzero_control,nonzero_request,nonzero_bcs,nonzero_domain,nonzero_reset,nonzero_post,nonzero_readers,nonzero_now,nonzero_pause};
  assert(!intel_nonzero_reset_observe(&legacy,p->g,&ring_native.result,true,io,nonzero_source_observe,p));
  assert(legacy.reason==INZ_INPUT && !legacy.eligible && !legacy.borrow.active && !legacy.controls && !legacy.resets);
 }
 return code;
}
