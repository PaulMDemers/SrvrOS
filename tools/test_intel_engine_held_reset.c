/* Actual GT/census/takeover/preparation plus the reset-capable composition.
 * Every mode starts a fresh process: deliberate failures leave ownership pinned. */
#define INTEL_ENGINE_PREPARE_FIXTURE_ONLY
#include "test_intel_engine_prepare.c"
#undef INTEL_ENGINE_PREPARE_FIXTURE_ONLY
#include <srvros/intel_engine_held_reset.h>
struct held_reset_model {
 struct prepare_model m;
 struct intel_engine_held_reset_result *reset;
 struct intel_completion_result post;
 struct intel_engine_held_reset_io io;bool reentered,poison;
 const char *mode;unsigned point,guards,clocks,samples,loads,stores,domains,begins,ends,actions;
};
static bool hrmode(struct held_reset_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t hrnow(void *ctx) {
 struct held_reset_model *m=ctx;++m->clocks;
 if(hrmode(m,"reentry") && !m->reentered) {
  m->reentered=true;struct intel_engine_held_reset_result saved=*m->reset,copy=saved;
  assert(!intel_engine_held_reset_run(m->reset,m->io));
  assert(!intel_engine_held_reset_run(&copy,m->io));
  assert(!memcmp(m->reset,&saved,sizeof(saved)) && !memcmp(&copy,&saved,sizeof(saved)) && !m->stores);
 }
 if(hrmode(m,"zero") && m->clocks==m->point)return 0;
 if(hrmode(m,"backward") && m->clocks==m->point)return 1;
 if(hrmode(m,"clock-epoch") && m->clocks==m->point)++m->m.base.parent->engine_epoch;
 if(hrmode(m,"settle-frozen") && m->reset->stage==3)return m->reset->last_us;
 return prepare_now(&m->m);
}
static bool hrguard(void *ctx) {
 struct held_reset_model *m=ctx;++m->guards;
 if(m->guards==m->point) {
  if(hrmode(m,"guard-time"))m->m.offset=100001;
  if(hrmode(m,"guard-epoch"))++m->m.base.parent->engine_epoch;
  if(hrmode(m,"guard-seal"))m->reset->object=NULL;
  if(hrmode(m,"guard-borrow"))m->m.result->borrow.object=NULL;
  if(hrmode(m,"guard-detach"))m->m.base.parent->dependents=NULL;
  if(hrmode(m,"guard-census"))((struct intel_completion_result *)m->m.base.census)->context[1][3]=0;
 }
 return !m->poison && !(hrmode(m,"guard") && m->guards==m->point) && guard(&m->m.base.gt);
}
static bool hrbegin(void *ctx) {
 struct held_reset_model *m=ctx;++m->begins;assert(m->begins==1 && intel_engine_prepare_reset_live(m->reset));
 assert(!m->reset->epoch_advanced && !m->stores && m->m.stores==5 && m->m.samples==3);
 /* Neither a copied nor an in-progress original result can re-enter. */
 struct intel_engine_held_reset_result copy=*m->reset;
 assert(!intel_engine_held_reset_run(&copy,(struct intel_engine_held_reset_io){0}));
 return !hrmode(m,"begin");
}
static bool hrend(void *ctx) {
 struct held_reset_model *m=ctx;++m->ends;assert(m->ends==1 && m->stores==2 && m->samples==2);
 assert(m->reset->epoch_advanced && intel_engine_prepare_reset_live(m->reset));
 if(hrmode(m,"end-time"))m->m.offset=100001;
 return !hrmode(m,"end");
}
static bool hrsnapshot(void *ctx,uint32_t words[8],uint32_t *domain,unsigned *count) {
 struct held_reset_model *m=ctx;unsigned n=m->samples++;*count=0;assert(n<2);
 assert(intel_engine_prepare_reset_live(m->reset));
 for(unsigned i=0;i<9;i++) {
  ++m->loads;
  if(hrmode(m,"snapshot-read") && m->loads==m->point)return false;
  uint32_t want=i==4?1:i==6?512:0;
  if((hrmode(m,"snapshot-value") || hrmode(m,"snapshot-value-guard")) && m->loads==m->point){want^=4;if(hrmode(m,"snapshot-value-guard"))m->poison=true;}
  if(hrmode(m,"snapshot-float") && m->loads==m->point)want=UINT32_MAX;
  if(i<8)words[i]=want;else *domain=want;
  ++*count;
 }
 if(hrmode(m,"snapshot-count") && m->samples==m->point)*count=10;
 if(hrmode(m,"snapshot-epoch") && m->samples==m->point)++m->m.base.parent->engine_epoch;
 return true;
}
static bool hrdomain(void *ctx,uint32_t *out) {
 struct held_reset_model *m=ctx;++m->domains;
 assert(intel_engine_prepare_reset_live(m->reset));*out=0;
 if(hrmode(m,"poll-read") && m->domains==m->point)return false;
 if((hrmode(m,"poll-foreign") || hrmode(m,"poll-foreign-guard")) && m->domains==m->point){*out=16;if(hrmode(m,"poll-foreign-guard"))m->poison=true;}
 if(hrmode(m,"never-ack"))*out=8;
 return true;
}
static enum intel_gt_write hrtransition(void *ctx,uint32_t value,uint32_t *ack) {
 struct held_reset_model *m=ctx;++m->stores;
 assert(value==8 && m->stores<=2 && m->m.stores==5);
 assert(intel_engine_prepare_reset_live(m->reset));
 assert(m->reset->epoch_advanced && m->reset->epoch_after==m->reset->epoch_before+1 &&
        m->m.base.parent->engine_epoch==m->reset->epoch_after);
 if(hrmode(m,"write-none") && m->stores==m->point)return GT_WRITE_NONE;
 m->m.control[4]=0;*ack=0;
 if(hrmode(m,"wait") || hrmode(m,"poll-read") || hrmode(m,"poll-foreign") || hrmode(m,"poll-foreign-guard") || hrmode(m,"never-ack"))*ack=8;
 if((hrmode(m,"ack-foreign") || hrmode(m,"ack-foreign-guard")) && m->stores==m->point){*ack=16;if(hrmode(m,"ack-foreign-guard"))m->poison=true;}
 if(hrmode(m,"ack-float") && m->stores==m->point)*ack=UINT32_MAX;
 if(hrmode(m,"write-epoch") && m->stores==m->point)++m->m.base.parent->engine_epoch;
 if(hrmode(m,"phase-time") && m->stores==m->point)m->m.offset+=10001;
 if(hrmode(m,"write-time") && m->stores==m->point)m->m.offset=100001;
 if(hrmode(m,"mixed-drift") && m->stores==2)m->m.control[m->point]=0;
 if(hrmode(m,"mixed-bcs") && m->stores==2)m->m.control[4]=3;
 if(hrmode(m,"write-uncertain") && m->stores==m->point)return GT_WRITE_UNCERTAIN;
 return GT_WRITE_DONE;
}
static void hrpause(void *ctx) {
 struct held_reset_model *m=ctx;m->m.base.gt.t+=10;
}
static bool hraction(void *ctx,const struct intel_engine_prepare_result *r) {
 struct held_reset_model *m=ctx;++m->actions;
 assert(intel_engine_prepare_action_live(r) && m->actions==1 && !m->stores && m->m.stores==5);
 assert(!intel_engine_prepare_reset_live(m->reset));
 if(hrmode(m,"near-deadline"))m->m.offset=r->begin_us+100000-75-m->m.base.gt.t;
 return !hrmode(m,"action-refuse");
}
static bool hrpreserve(void *ctx,const struct intel_completion_result *pre) {
 struct held_reset_model *m=ctx;++m->m.preserves;
 assert(pre==m->m.base.census && m->m.result->borrow.active && m->stores==2);
 assert(m->m.base.parent->engine_epoch==pre->engine_epoch+1 && !intel_engine_prepare_action_live(m->m.result));
 if(hrmode(m,"preserve"))return false;
 assert(intel_completion_observe(&m->post,m->m.base.parent,(struct intel_completion_io){&m->m.base.gt,obs_guard,group,context,obs_now,batch_sample}));
 assert(m->post.engine_epoch==pre->engine_epoch+1 && !memcmp(m->post.words,pre->words,sizeof(pre->words)) &&
        !memcmp(m->post.context,pre->context,sizeof(pre->context)));
 return true;
}
#ifndef INTEL_ENGINE_HELD_RESET_FIXTURE_ONLY
int main(int argc,char **argv) {
 (void)prepare_preserve; /* Imported legacy fixture callback; reset path uses epoch-bound preservation. */
 struct held_reset_model m={.m={.base={.gt={.t=1000,.mode="normal"},.mode="normal"},.mode="normal"},.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m.m.base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));m.m.base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;
 assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&m.m.base.gt,obs_guard,group,context,obs_now,batch_sample}));m.m.base.census=&census;
 struct intel_takeover_result pre={0};m.m.base.result=&pre;
 assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&m.m.base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result r={0};m.m.result=&r;struct intel_engine_held_reset_result x={0};m.reset=&x;
 struct intel_engine_prepare_io io={&m.m,prepare_guard,prepare_sample,prepare_control,prepare_transition,NULL,prepare_now,prepare_pause};io.ctx=&m;/* first member layout preserves prepare fixture ctx */io.preserve=hrpreserve;
 struct intel_engine_prepare_action action={&m,hraction};
 struct intel_engine_held_reset_io reset_io={&m,hrguard,hrsnapshot,hrdomain,hrtransition,hrbegin,hrend,hrnow,hrpause};
 m.io=reset_io;
 if(hrmode(&m,"missing"))reset_io.begin=NULL;
 if(hrmode(&m,"overflow")){g.engine_epoch=UINT32_MAX;census.engine_epoch=pre.epoch=UINT32_MAX;}
 bool ok=intel_engine_prepare_with_bcs_reset(&r,&g,&pre,true,io,action,&x,reset_io);
 bool positive=hrmode(&m,"normal") || hrmode(&m,"wait") || hrmode(&m,"reentry");assert(ok==positive);
 if(positive) {
  assert(x.entered && r.complete && x.complete && !r.reason && !x.reason && !r.uncertain && !x.uncertain);
  assert(r.returned && !r.borrow.active && !g.borrows && !g.dependents && r.epoch==0 && x.epoch_before==0 && x.epoch_after==g.engine_epoch && g.engine_epoch==1);
  assert(census.engine_epoch==pre.epoch && pre.epoch==0 && r.ready==31 && r.cancelled==31 && r.attempts==10 && r.writes==10 && r.acks==10 && r.snapshots==5 && r.reads==40);
  assert(x.attempts==2 && x.writes==2 && x.acks==2 && x.snapshot_attempts==2 && x.snapshots==2 && x.counts[0]==9 && x.counts[1]==9);
  assert(x.reads==20+(hrmode(&m,"wait")?2u:0u) && x.polls==(hrmode(&m,"wait")?4u:2u));
  assert(m.begins==1 && m.ends==1 && x.provider_enter && x.provider_exit && x.origin_us==r.begin_us && x.begin_us>=r.action_end_us && x.end_us<=r.end_us);
  assert(x.settle_end_us-x.settle_begin_us>=50 && r.end_us-r.begin_us<=100000 && m.m.preserves==1);
  assert(intel_gt_access_close(&g));
 } else {
  assert(r.reason && !r.complete && !r.returned);
  if(!hrmode(&m,"preserve"))assert(!r.cancelled && !m.m.preserves);
  if(hrmode(&m,"missing") || hrmode(&m,"overflow"))assert(r.reason==IEP_INPUT && !m.m.stores && !m.stores && !g.borrows && !x.attempted);
  else {
   assert(r.uncertain && r.borrow.active && !intel_gt_access_close(&g));
   if(hrmode(&m,"action-refuse"))assert(!x.attempted && !m.stores && m.m.samples==2);
   else assert(x.attempted);
  }
  if(hrmode(&m,"write-none") || hrmode(&m,"write-uncertain"))assert(x.attempts==m.point && m.stores==m.point && x.epoch_advanced && x.epoch_after==x.epoch_before+1 && x.reason&IHR_WRITE);
  if(hrmode(&m,"snapshot-read"))assert(x.reads==(m.point<=9?m.point-1:m.point+1) && x.reason&IHR_READ);
  if(hrmode(&m,"snapshot-value-guard") || hrmode(&m,"ack-foreign-guard") || hrmode(&m,"poll-foreign-guard"))assert((x.reason&(IHR_STATE|IHR_GUARD))==(IHR_STATE|IHR_GUARD));
  if(hrmode(&m,"near-deadline"))assert(!x.complete && (x.reason&IHR_CLOCK) && x.clock_at>x.origin_us+100000 && x.clock_prev<=x.origin_us+100000 && x.end_us-x.begin_us<100000);
  if(hrmode(&m,"never-ack"))assert(x.polls==256 && x.phase_polls[0]==255 && m.stores==1 && x.reason&IHR_TIMEOUT);
  if(hrmode(&m,"mixed-drift") || hrmode(&m,"mixed-bcs"))assert(x.complete && r.reason&IEP_STATE && m.m.samples==4 && m.m.stores==5);
  if(hrmode(&m,"preserve"))assert(x.complete && r.reason&IEP_PRESERVE && m.m.stores==10 && r.cancelled==31 && m.m.preserves==1);
 }
 assert(!x.active && !intel_engine_prepare_reset_live(&x));
 struct intel_engine_prepare_result saved=r,copy=r;struct intel_engine_held_reset_result savedx=x,copyx=x;unsigned stores=m.stores;
 assert(!intel_engine_prepare_with_bcs_reset(&r,&g,&pre,true,io,action,&x,reset_io));
 assert(!intel_engine_prepare_with_bcs_reset(&copy,&g,&pre,true,io,action,&copyx,reset_io));
 assert(!memcmp(&saved,&r,sizeof(r)) && !memcmp(&savedx,&x,sizeof(x)) && !memcmp(&saved,&copy,sizeof(copy)) && !memcmp(&savedx,&copyx,sizeof(copyx)) && stores==m.stores);
 printf("engine-held-reset: PASS mode=%s point=%u clocks=%u guards=%u samples=%u stores=%u reads=%u\n",m.mode,m.point,m.clocks,m.guards,m.samples,m.stores,x.reads);
 return 0;
}
#endif
