/* Actual GT/census/takeover/preparation/reset/interval, not a forged live token. */
#define INTEL_HELD_INTERVAL_FIXTURE_ONLY
#include "test_intel_held_interval.c"
#undef INTEL_HELD_INTERVAL_FIXTURE_ONLY
#include <srvros/intel_engine_rehold.h>
struct rehold_model {
 struct interval_model base;
 struct intel_engine_rehold result;
 struct intel_engine_rehold_child child;
 struct intel_engine_rehold_io io;
 const char *kind;unsigned point,clocks,guards,loads,stores,polls;
 bool poison,reentered;
};
static bool rmode(struct rehold_model *m,const char *s){return !strcmp(m->kind,s);}
static uint64_t rnow(void *ctx) {
 struct rehold_model *m=ctx;++m->clocks;
 if(rmode(m,"reentry") && !m->reentered) {
  m->reentered=true;struct intel_engine_rehold saved=m->result,copy=saved;
  assert(!intel_engine_rehold_run(&m->result,m->base.base.reset,m->io));
  assert(!intel_engine_rehold_run(&copy,m->base.base.reset,m->io));
  assert(!memcmp(&saved,&m->result,sizeof saved));
 }
 if(m->clocks==m->point) {
  if(rmode(m,"zero"))return 0;
  if(rmode(m,"backward"))return 1;
  if(rmode(m,"deadline"))m->base.base.m.offset=100001;
  if(rmode(m,"clock-epoch"))++m->base.base.reset->parent->engine_epoch;
 }
 return prepare_now(&m->base.base.m);
}
static bool rguard(void *ctx) {
 struct rehold_model *m=ctx;++m->guards;
 if(m->guards==m->point) {
  if(rmode(m,"guard"))return false;
  if(rmode(m,"guard-epoch"))++m->base.base.reset->parent->engine_epoch;
  if(rmode(m,"guard-detach"))m->base.base.reset->parent->dependents=NULL;
  if(rmode(m,"guard-stage"))m->base.base.reset->stage=5;
  if(rmode(m,"guard-owner"))++m->base.base.reset->parent->begin;
  if(rmode(m,"guard-binding"))m->base.base.reset->rehold=NULL;
 }
 return !m->poison;
}
static bool rsample(void *ctx,uint32_t out[6],unsigned *count) {
 struct rehold_model *m=ctx;assert(!*count);
 for(unsigned e=0;e<6;e++) {
  ++m->loads;
  if(m->loads==m->point && rmode(m,"read"))return false;
  out[e]=e<5?m->base.base.m.control[e]:0;++*count;
  if(m->loads==m->point) {
   if(rmode(m,"value") || rmode(m,"value-guard"))out[e]^=4;
   if(rmode(m,"value-guard"))m->poison=true;
   if(rmode(m,"read-epoch"))++m->base.base.reset->parent->engine_epoch;
  }
 }
 if(rmode(m,"count") && m->result.sample_attempts==m->point)*count=7;
 return true;
}
static bool rcontrol(void *ctx,uint32_t *out) {
 struct rehold_model *m=ctx;++m->polls;
 if(rmode(m,"poll-read") && m->stores==m->point)return false;
 if(rmode(m,"poll-time") && m->stores==m->point)m->base.base.m.offset+=10001;
 if(rmode(m,"never-ready") && m->stores==m->point)*out=m->stores==1?1:2;
 else *out=m->base.base.m.control[4];
 return true;
}
static enum intel_gt_write rtransition(void *ctx,uint32_t value,uint32_t *out) {
 struct rehold_model *m=ctx;++m->stores;assert(m->stores<=2);
 assert(value==(m->stores==1?0x10001u:0x10000u));assert(!m->result.child);
 assert(intel_engine_prepare_reset_live(m->base.base.reset));
 if(m->stores==m->point && rmode(m,"write-none"))return GT_WRITE_NONE;
 m->base.base.m.control[4]=m->stores==1?3:0;*out=m->base.base.m.control[4];
 if(rmode(m,"wait") || rmode(m,"poll-read") || rmode(m,"poll-time") || rmode(m,"never-ready"))*out=m->stores==1?1:2;
 if(m->stores==m->point) {
  if(rmode(m,"write-uncertain"))return GT_WRITE_UNCERTAIN;
  if(rmode(m,"write-epoch"))++m->base.base.reset->parent->engine_epoch;
  if(rmode(m,"write-time"))m->base.base.m.offset=100001;
  if(rmode(m,"ack-float"))*out=UINT32_MAX;
  if(rmode(m,"ack-mask"))*out|=0x10000;
 }
 return GT_WRITE_DONE;
}
static bool rinterval_guard(void *ctx) {
 struct rehold_model *m=ctx;
 assert(intel_engine_rehold_child_live(&m->child));
 for(unsigned i=0;i<5;i++)assert(m->base.base.m.control[i]==3);
 return iguard(&m->base);
}
static bool robserve(void *ctx,struct intel_engine_rehold *r) {
 struct rehold_model *m=ctx;assert(r==&m->result && intel_engine_rehold_live(r));
 assert(intel_engine_rehold_borrow(r,&m->child));
 assert(!intel_engine_rehold_borrow(r,&m->child));
 struct intel_engine_rehold_child copy=m->child;copy.object=&copy;
 assert(!intel_engine_rehold_return(&copy));
 struct intel_engine_rehold rebound=*r;rebound.object=&rebound;
 assert(!intel_engine_rehold_live(&rebound));
 struct intel_engine_rehold_child other={0};assert(!intel_engine_rehold_borrow(&rebound,&other));
 assert(!intel_engine_rehold_borrow(r,&other));
 if(rmode(m,"child-live"))return true;
 if(rmode(m,"child-fail"))return false;
 if(rmode(m,"child-epoch")){++r->reset->parent->engine_epoch;assert(!intel_engine_rehold_return(&m->child));return false;}
 if(rmode(m,"child-detach")){r->child=NULL;return true;}
 if(rmode(m,"child-held-corrupt")){r->words[1][4]=0;assert(!intel_engine_rehold_child_live(&m->child));assert(!intel_engine_rehold_return(&m->child));return false;}
 m->base.io=(struct intel_held_interval_io){m,rinterval_guard,iread,inow};
 assert(intel_held_interval_observe(&m->base.result,m->base.base.reset,&m->base.snapshot,m->base.io));
 struct scanout_transfer_manifest data;
 assert(intel_held_interval_manifest(&m->base.result,&data) && !data.assurances && data.obligations==255);
 m->base.result.bound=true;
 assert(intel_engine_rehold_return(&m->child));assert(!intel_engine_rehold_return(&m->child));
 if(rmode(m,"child-late"))m->base.base.m.offset=100001;
 return true;
}
static void rpause(void *ctx){struct rehold_model *m=ctx;prepare_pause(&m->base.base.m);}
static bool rehold_end(void *ctx) {
 struct rehold_model *m=ctx;assert(hrend(&m->base.base));
 if(rmode(m,"parent-deadline"))m->base.base.reset->parent->begin=m->base.base.m.base.gt.t-299999;
 return intel_engine_rehold_run(&m->result,m->base.base.reset,m->io);
}
int main(int argc,char **argv) {
 (void)interval_end;(void)prepare_preserve;
 struct rehold_model m={0};struct interval_model *im=&m.base;
 struct held_reset_model *hr=&im->base;struct prepare_model *pm=&hr->m;
 pm->base.gt.t=1000000;pm->base.gt.mode="normal";pm->base.mode="normal";pm->mode="normal";hr->mode="normal";im->kind="normal";
 m.kind=argc>1?argv[1]:"normal";m.point=argc>2?(unsigned)atoi(argv[2]):1;
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&pm->base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));pm->base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&pm->base.gt,obs_guard,group,context,obs_now,batch_sample}));pm->base.census=&census;
 struct intel_takeover_result pre={0};pm->base.result=&pre;assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&pm->base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result r={0};pm->result=&r;struct intel_engine_held_reset_result x={0};hr->reset=&x;
 im->original=0x8e201001;im->snapshot=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};im->snapshot.original[0]=im->original;
 struct intel_engine_prepare_io io={&m,prepare_guard,prepare_sample,prepare_control,prepare_transition,hrpreserve,prepare_now,prepare_pause};
 struct intel_engine_held_reset_io reset_io={&m,hrguard,hrsnapshot,hrdomain,hrtransition,hrbegin,rehold_end,hrnow,hrpause};hr->io=reset_io;
 m.io=(struct intel_engine_rehold_io){&m,rguard,rsample,rcontrol,rtransition,robserve,rnow,rpause};
 bool ok=intel_engine_prepare_with_bcs_reset(&r,&g,&pre,true,io,(struct intel_engine_prepare_action){&m,hraction},&x,reset_io);
 bool positive=rmode(&m,"normal") || rmode(&m,"wait") || rmode(&m,"reentry");assert(ok==positive);
 if(positive) {
  assert(m.result.complete && !m.result.reason && m.result.restored && !m.result.uncertain && m.result.writes==2 && m.result.acks==2 && m.result.samples==4);
  assert(m.result.child_borrows==1 && m.result.child_returns==1 && !m.result.child);
  assert(im->result.complete && im->result.reads==6 && r.returned && !g.borrows && !g.dependents);
  assert(intel_gt_access_close(&g));
 } else {
  assert(m.result.reason && !m.result.complete && !r.complete && !x.complete && r.borrow.active && r.uncertain && !intel_gt_access_close(&g));
  if(rmode(&m,"child-live") || rmode(&m,"child-fail") || rmode(&m,"child-detach"))assert(m.result.attempts==1 && m.result.writes==1 && !m.result.restored);
  if(rmode(&m,"value-guard"))assert((m.result.reason&(IER_STATE|IER_GUARD))==(IER_STATE|IER_GUARD));
  if(rmode(&m,"child-late"))assert(m.result.attempts==1 && m.result.reason&IER_CLOCK);
 }
 assert(!intel_engine_rehold_live(&m.result));assert(!intel_engine_rehold_return(&m.child));
 printf("rehold: PASS mode=%s point=%u clocks=%u guards=%u loads=%u stores=%u polls=%u\n",m.kind,m.point,m.clocks,m.guards,m.loads,m.stores,m.polls);
}
