/* Actual GT/census/prepare/reset composition; no hand-built live reset token. */
#define INTEL_ENGINE_HELD_RESET_FIXTURE_ONLY
#include "test_intel_engine_held_reset.c"
#undef INTEL_ENGINE_HELD_RESET_FIXTURE_ONLY
#include <srvros/intel_held_interval.h>
struct interval_model {
 struct held_reset_model base;
 struct intel_held_interval_result result;
 struct scanout_handoff_snapshot snapshot;
 struct intel_held_interval_io io;
 const char *kind;
 unsigned point,guards,clocks,loads;
 uint64_t original;
 bool poison,reentered;
};
static bool imode(struct interval_model *m,const char *s){return !strcmp(m->kind,s);}
static uint64_t inow(void *ctx) {
 struct interval_model *m=ctx;++m->clocks;
 if(imode(m,"reentry") && !m->reentered) {
  m->reentered=true;struct intel_held_interval_result saved=m->result,copy=saved;
  assert(!intel_held_interval_observe(&m->result,m->base.reset,&m->snapshot,m->io));
  assert(!intel_held_interval_observe(&copy,m->base.reset,&m->snapshot,m->io));
  assert(!memcmp(&saved,&m->result,sizeof saved));
 }
 if(m->clocks==m->point) {
  if(imode(m,"zero"))return 0;
  if(imode(m,"backward"))return 1;
  if(imode(m,"deadline"))m->base.m.offset=100001;
  if(imode(m,"clock-epoch"))++m->base.reset->parent->engine_epoch;
 }
 return prepare_now(&m->base.m);
}
static bool iguard(void *ctx) {
 struct interval_model *m=ctx;++m->guards;
 assert(m->base.m.base.parent->borrows==1);
 if(m->guards==m->point) {
  if(imode(m,"guard"))return false;
  if(imode(m,"guard-epoch"))++m->base.reset->parent->engine_epoch;
  if(imode(m,"guard-borrow"))m->base.m.result->borrow.object=NULL;
  if(imode(m,"guard-detach"))m->base.m.base.parent->dependents=NULL;
  if(imode(m,"guard-stage"))m->base.reset->stage=5;
 }
 if(imode(m,"input-mutation")){m->snapshot.original[0]^=1;m->snapshot.first=1;}
 return !m->poison;
}
static bool iread(void *ctx,uint32_t off,uint32_t *out) {
 struct interval_model *m=ctx;unsigned i=m->loads++;
 assert(i<6 && off==INTEL_WINDOW_PAGE+(i%3==1?0:4));
 assert(intel_engine_prepare_reset_live(m->base.reset));
 if(imode(m,"read") && m->loads==m->point)return false;
 *out=(uint32_t)(m->original>>(i%3==1?0:32));
 if(m->loads==m->point) {
  if(imode(m,"drift") || imode(m,"drift-guard"))*out^=1;
  if(imode(m,"drift-guard"))m->poison=true;
  if(imode(m,"read-epoch"))++m->base.reset->parent->engine_epoch;
 }
 return true;
}
static bool interval_end(void *ctx) {
 struct interval_model *m=ctx;assert(hrend(&m->base));
 const struct intel_engine_held_reset_result *x=m->base.reset;
 assert(x->stage==4 && x->epoch_advanced && !x->complete && !x->provider_exit);
 assert(m->base.m.result->borrow.active && m->base.m.base.parent->borrows==1);
 if(imode(m,"bad-input"))m->snapshot.assurances=1;
 bool ok=intel_held_interval_observe(&m->result,x,&m->snapshot,m->io);
 bool positive=imode(m,"normal") || imode(m,"reentry") || imode(m,"input-mutation") || imode(m,"high-original");
 assert(ok==positive);
 if(positive) {
  struct scanout_transfer_manifest data;
  assert(intel_held_interval_manifest(&m->result,&data));m->result.bound=true;
  assert(data.original==&m->result.expected && data.original[0]==m->original &&
   data.first==8704 && data.pages==1 && data.source_epoch==x->epoch_after && !data.assurances && data.obligations==255);
  assert(!intel_held_interval_manifest(&m->result,(void *)&m->result));
  struct intel_held_interval_result copy=m->result;
  assert(!intel_held_interval_manifest(&copy,&data));
 } else {
  assert(m->result.reason && !m->result.complete);
  assert(m->result.reads<=6 && m->base.m.result->borrow.active);
  if(imode(m,"drift-guard"))assert((m->result.reason&(IHI_CHANGED|IHI_GUARD))==(IHI_CHANGED|IHI_GUARD));
 }
 struct intel_held_interval_result saved=m->result;
 assert(!intel_held_interval_observe(&m->result,x,&m->snapshot,m->io));
 assert(!memcmp(&saved,&m->result,sizeof saved));
 return ok;
}
int main(int argc,char **argv) {
 (void)prepare_preserve;
 struct interval_model m={.base={.m={.base={.gt={.t=1000,.mode="normal"},.mode="normal"},.mode="normal"},.mode="normal"},
  .kind=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m.base.m.base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));m.base.m.base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;
 assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&m.base.m.base.gt,obs_guard,group,context,obs_now,batch_sample}));m.base.m.base.census=&census;
 struct intel_takeover_result pre={0};m.base.m.base.result=&pre;
 assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&m.base.m.base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result r={0};m.base.m.result=&r;struct intel_engine_held_reset_result x={0};m.base.reset=&x;
 m.original=imode(&m,"high-original")?0x123456788e201001ull:0x8e201001;
 m.snapshot=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};m.snapshot.original[0]=m.original;
 m.io=(struct intel_held_interval_io){&m,iguard,iread,inow};
 struct intel_engine_prepare_io io={&m,prepare_guard,prepare_sample,prepare_control,prepare_transition,hrpreserve,prepare_now,prepare_pause};
 struct intel_engine_held_reset_io reset_io={&m,hrguard,hrsnapshot,hrdomain,hrtransition,hrbegin,interval_end,hrnow,hrpause};m.base.io=reset_io;
 bool ok=intel_engine_prepare_with_bcs_reset(&r,&g,&pre,true,io,(struct intel_engine_prepare_action){&m,hraction},&x,reset_io);
 bool positive=imode(&m,"normal") || imode(&m,"reentry") || imode(&m,"input-mutation") || imode(&m,"high-original");assert(ok==positive);
 if(positive) {
  assert(m.result.complete && m.result.bound && m.result.stable && !m.result.reason && m.result.reads==6 && m.result.samples==2);
  assert(m.result.epoch_before==0 && m.result.epoch_after==1 && m.result.origin_us==r.begin_us &&
   m.result.begin_us>=x.settle_end_us && m.result.end_us<=x.end_us && m.result.end_us-m.result.origin_us<=100000);
  assert(r.complete && x.complete && r.returned && !r.borrow.active && !g.borrows && !g.dependents);
  assert(intel_gt_access_close(&g));
 } else assert(!r.complete && !x.complete && r.uncertain && r.borrow.active && !intel_gt_access_close(&g));
 struct scanout_transfer_manifest data;assert(!intel_held_interval_manifest(&m.result,&data));
 printf("held-interval: PASS mode=%s point=%u clocks=%u guards=%u loads=%u reads=%u\n",m.kind,m.point,m.clocks,m.guards,m.loads,m.result.reads);
}
