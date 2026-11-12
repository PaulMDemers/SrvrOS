/* Real original entry/native adapter; target accesses are modeled explicitly. */
#include <stdint.h>
#include <stdbool.h>
#include <srvros/intel_gt_access.h>
static bool intel_display_test_nonzero_clock(uint64_t *);
static bool intel_display_test_nonzero_read(unsigned,uint32_t *);
static bool intel_display_test_nonzero_census(unsigned,unsigned,uint32_t *);
static void intel_display_test_nonzero_admission(bool,unsigned);
static enum intel_gt_write intel_display_test_nonzero_write(unsigned,uint32_t,uint32_t *);
#include "test_intel_ring_transition_adapter.c"
static unsigned nz_reads,nz_writes,nz_clocks,nz_admissions,nz_census,nz_polls,nz_fault_clock;
static uint32_t nz_control[5],nz_domain;
static bool intel_display_test_nonzero_clock(uint64_t *out) {
 ++nz_clocks;
#ifdef SRVROS_INTEL_NONZERO_SOURCE
 if(intel_display_test_nonzero_source_clock(out))return true;
#endif
 if(jf("nz-clock-parent",nz_clocks))nonzero_native.r.parent=NULL;
 if(jf("nz-zero",nz_clocks)){*out=0;return true;}
 if(jf("nz-backward",nz_clocks)){*out=1;return true;}
 if(jf("nz-time",nz_clocks))ticks+=100001;
 if(jf("nz-parent-time",nz_clocks))nonzero_native.g->begin=ticks-300000;
 if(nz_fault_clock){*out=nz_fault_clock==1?0:1;nz_fault_clock=0;return true;}
 if(jf("nz-return",1) && nonzero_native.r.returned && !nonzero_native.r.end){*out=0;return true;}
 return false;
}
static bool intel_display_test_nonzero_read(unsigned reg,uint32_t *out) {
 ++nz_reads;assert(wake_irq && hda_command==2 && nonzero_native.r.borrow.active);
 if(jf("nz-read",nz_reads))return false;
 if(jf("nz-read-time",nz_reads))ticks+=100001;
 if(reg==0x941c){*out=nz_domain;if(nz_domain && !strcmp(engine_mode,"nz-poll")){nz_domain=0;++nz_polls;}}
 else if((reg&0xfff)==0xd0) {
  unsigned e=reg==0x20d0?0:reg==0x120d0?1:reg==0x1a0d0?2:reg==0x1c0d0?3:4;
  *out=nz_control[e];if((*out&3)==1 && !strcmp(engine_mode,"nz-poll")){nz_control[e]=3;++nz_polls;}
 } else if(reg>=0x70000)*out=reg==0x70180?0xd8000400u:0;
 else {
  const struct intel_completion_result *c=nonzero_native.r.source;unsigned i=0;
  for(;i<6 && 0x22000+intel_engine_offsets[i]!=reg;i++);
  *out=i<6?c->words[1][4][i]:reg==0x2209c?c->context[1][3]:c->context[1][5];
  if(nonzero_native.r.reset_writes && !strcmp(engine_mode,"nz-default") && (i==2 || i==3))*out=0;
 }
 if(jf("nz-drift",nz_reads))*out^=1;
 return true;
}
static bool intel_display_test_nonzero_census(unsigned e,unsigned i,uint32_t *out) {
 ++nz_census;*out=nonzero_native.r.source->words[1][e][i];
 if(!strcmp(engine_mode,"nz-default") && e==4 && (i==2 || i==3))*out=0;
 if(jf("nz-post-read",nz_census))return false;
 if(jf("nz-post-drift",nz_census))*out^=1;
 return true;
}
static void intel_display_test_nonzero_admission(bool reset,unsigned e) {
 (void)reset;(void)e;++nz_admissions;
 struct intel_nonzero_reset *r=&nonzero_native.r;
 if(jf("nz-store-zero",nz_admissions))nz_fault_clock=1;
 if(jf("nz-store-backward",nz_admissions))nz_fault_clock=2;
 if(jf("nz-store-time",nz_admissions))ticks=r->begin+100001;
 if(jf("nz-phase-time",nz_admissions))ticks=(reset?r->pulse_begin[e]:r->control_begin[r->controls-1])+10001;
 if(jf("nz-parent-stamp",nz_admissions))r->parent->begin++;
 if(jf("nz-epoch",nz_admissions))r->parent->engine_epoch++;
 if(jf("nz-borrow",nz_admissions))r->parent->dependents=NULL;
 if(jf("nz-alias",nz_admissions))pa[INTEL_CAPTURE_PAGES+4]^=4096;
 if(jf("nz-source",nz_admissions))ring_native.result.post.words[0][4][2]^=4096;
 if(jf("nz-copy",nz_admissions)){static struct intel_completion_result copy;copy=*r->source;copy.object=&copy;r->source=&copy;}
 if(jf("nz-provider",nz_admissions))r->provider=NULL;
 if(jf("nz-entry-stamp",nz_admissions))ring_native.result.end++;
 if(jf("nz-source-stamp",nz_admissions))ring_native.result.post.last_us++;
 if(jf("nz-original-stamp",nz_admissions))((struct intel_completion_result *)ring_native.c)->last_us++;
 if(jf("nz-original-word",nz_admissions))((struct intel_completion_result *)ring_native.c)->words[0][0][0]^=1;
}
static enum intel_gt_write intel_display_test_nonzero_write(unsigned reg,uint32_t value,uint32_t *out) {
 const struct intel_nonzero_reset *r=&nonzero_native.r;
 assert(intel_nonzero_reset_write_ready(r,reg==0x941c,reg==0x941c?r->resets-1:r->index,value));
 assert(wake_irq && hda_command==2 && r->borrow.active);
 if(jf("nz-none",nz_writes+1))return GT_WRITE_NONE;
 ++nz_writes;
 if(reg==0x941c) {
  assert(value==8 && r->epoch_after==2 && r->parent->engine_epoch==2 && nz_writes>=6 && nz_writes<=7);
  for(unsigned i=0;i<5;i++)assert(nz_control[i]==3 || (i==4 && nz_writes==7 && !nz_control[i]));
  nz_control[4]=0;nz_domain=(!strcmp(engine_mode,"nz-poll") || !strcmp(engine_mode,"nz-stuck-reset"))?8:0;*out=nz_domain;
 } else {
  unsigned e=reg==0x20d0?0:reg==0x120d0?1:reg==0x1a0d0?2:reg==0x1c0d0?3:reg==0x220d0?4:5;
  assert(e<5 && value==(nz_writes<=5?0x10001u:0x10000u));
  assert(e==(nz_writes<=5?nz_writes-1:12-nz_writes));
  nz_control[e]=value==0x10001?((!strcmp(engine_mode,"nz-poll") || !strcmp(engine_mode,"nz-stuck-control"))?1:3):0;*out=nz_control[e];
 }
 if(jf("nz-uncertain",nz_writes))return GT_WRITE_UNCERTAIN;
 if(jf("nz-ack",nz_writes))*out=UINT32_MAX;
 return GT_WRITE_DONE;
}
#ifndef INTEL_NONZERO_FIXTURE_MAIN
#define INTEL_NONZERO_FIXTURE_MAIN main
#endif
int INTEL_NONZERO_FIXTURE_MAIN(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--wake-engine-nz-kind2")){ring_fixture_disabled=true;ring_ctl=0;}
 int result=original_display_fixture_main(argc,argv);
 printf("laptop-capture: boot options=%s\n",wake_cmd.cmdline);
 const struct intel_nonzero_reset *r=&nonzero_native.r;
 if(r->object) {
  assert(r->object==r);
  struct intel_nonzero_reset before=*r;
  assert(!intel_nonzero_reset_run(&nonzero_native.r,joint.scope.gpu,&ring_native.result,true,(struct intel_nonzero_io){0}));
  assert(!memcmp(&before,r,sizeof before));
  if(r->uncertain)assert(joint.scope.gpu->pinned && !display_access_allowed());
  if(r->complete) {
   assert(r->returned && !r->borrow.active && r->preserved && nz_writes==12 && r->reset_writes==2 && r->control_writes==10);
   assert(!intel_ring_entry_reset_ready(&ring_native.result));
  }
 }
 printf("nonzero-fixture: reads=%u writes=%u clocks=%u admissions=%u census=%u polls=%u\n",nz_reads,nz_writes,nz_clocks,nz_admissions,nz_census,nz_polls);
 return result;
}
