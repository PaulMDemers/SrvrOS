#define INTEL_TAKEOVER_FIXTURE_ONLY
#include "test_intel_takeover.c"
#undef INTEL_TAKEOVER_FIXTURE_ONLY
#include <srvros/intel_engine_prepare.h>
struct prepare_model {
 struct takeover_model base;
 struct intel_engine_prepare_result *result;
 const char *mode;unsigned point,guards,stores,samples,controls,clocks,preserves;
 uint32_t control[5];uint64_t offset;
};
static bool pmode(struct prepare_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t prepare_now(void *p) {
 struct prepare_model *m=p;m->clocks++;uint64_t t=now(&m->base.gt);
 if(pmode(m,"zero") && m->clocks==m->point)return 0;
 if(pmode(m,"backward") && m->clocks==m->point)return 1;
 if(pmode(m,"clock-epoch") && m->clocks==m->point)m->base.parent->engine_epoch++;
 if(pmode(m,"end-deadline") && m->result->returned)m->offset=100001;
 if(pmode(m,"end-epoch") && m->result->returned)m->base.parent->engine_epoch++;
 return t+m->offset;
}
static bool prepare_guard(void *p) {
 struct prepare_model *m=p;m->guards++;
 if(pmode(m,"guard-time") && m->guards==m->point)m->offset=100001;
 if(pmode(m,"guard-epoch") && m->guards==m->point)m->base.parent->engine_epoch++;
 return guard(&m->base.gt) && !(pmode(m,"guard") && m->guards==m->point);
}
static bool prepare_sample(void *p,uint32_t out[6],unsigned *count) {
 struct prepare_model *m=p;m->samples++;*count=0;
 for(unsigned e=0;e<6;e++) {
  if(pmode(m,"sample-read") && (m->samples-1)*6+e+1==m->point)return false;
  out[e]=e<5?m->control[e]:0;
  if(pmode(m,"sample-float") && (m->samples-1)*6+e+1==m->point)out[e]=UINT32_MAX;
  if(pmode(m,"hold-drift") && m->samples==3 && e==m->point)out[e]=0;
  if(pmode(m,"hold-mask-drift") && m->samples==3 && e==m->point)out[e]|=0x10000;
  ++*count;
  if(out[e]==UINT32_MAX)return false;
 }
 if(pmode(m,"sample-count") && m->samples==m->point)*count=7;
 if(pmode(m,"sample-epoch") && m->samples==m->point)m->base.parent->engine_epoch++;
 return true;
}
static bool prepare_control(void *p,unsigned e,uint32_t *out) {
 struct prepare_model *m=p;assert(e<5);m->controls++;
 if(pmode(m,"poll-read") && m->stores==m->point)return false;
 if(pmode(m,"poll-epoch") && m->stores==m->point)m->base.parent->engine_epoch++;
 if(pmode(m,"phase-time") && m->stores==m->point)m->offset+=10001;
 if(pmode(m,"never-ready") && m->stores==m->point){*out=m->stores<=5?1:2;return true;}
 m->control[e]=m->stores<=5?3:0;*out=m->control[e];return true;
}
static enum intel_gt_write prepare_transition(void *p,unsigned e,uint32_t value,uint32_t *ack) {
 struct prepare_model *m=p;unsigned n=m->stores++;assert(n<10 && e==(n<5?n:9-n));
 assert(value==(n<5?0x10001u:0x10000u));
 assert(m->result->borrow.active && m->base.parent->borrows==1);
 unsigned before=m->base.parent->write_attempts;assert(!intel_gt_access_close(m->base.parent) && m->base.parent->write_attempts==before);
 if(pmode(m,"write-none") && m->stores==m->point)return GT_WRITE_NONE;
 m->control[e]=n<5?3:0;*ack=m->control[e];
 if(pmode(m,"wait") || ((pmode(m,"never-ready") || pmode(m,"phase-time") || pmode(m,"poll-read") || pmode(m,"poll-epoch")) && m->stores==m->point))m->control[e]=*ack=n<5?1:2;
 if(pmode(m,"first-ack-time") && m->stores==m->point)m->offset+=10001;
 if(pmode(m,"ack-float") && m->stores==m->point)*ack=UINT32_MAX;
 if(pmode(m,"ack-reserved") && m->stores==m->point)*ack|=4;
 if(pmode(m,"ack-foreign") && m->stores==m->point)*ack^=1;
 if(pmode(m,"write-epoch") && m->stores==m->point)m->base.parent->engine_epoch++;
 if(pmode(m,"write-detach") && m->stores==m->point)m->base.parent->dependents=NULL;
 if(pmode(m,"write-uncertain") && m->stores==m->point)return GT_WRITE_UNCERTAIN;
 return GT_WRITE_DONE;
}
static bool prepare_preserve(void *p,const struct intel_completion_result *c) {
 struct prepare_model *m=p;m->preserves++;assert(c==m->base.census && m->result->borrow.active);
 if(pmode(m,"preserve-epoch"))m->base.parent->engine_epoch++;
 if(pmode(m,"preserve-time"))m->offset=100001;
 return !pmode(m,"preserve");
}
static void prepare_pause(void *p){(void)p;}
#ifndef INTEL_ENGINE_PREPARE_FIXTURE_ONLY
int main(int argc,char **argv) {
 struct prepare_model m={.base={.gt={.t=1000,.mode="normal"},.mode="normal"},.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m.base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));m.base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&m.base.gt,obs_guard,group,context,obs_now,batch_sample}));m.base.census=&census;
 struct intel_takeover_result pre={0};m.base.result=&pre;
 assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&m.base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_takeover_result copied=pre;
 if(pmode(&m,"pre-epoch"))g.engine_epoch++;
 if(pmode(&m,"pre-geometry"))census.words[1][m.point][2]=4096;
 if(pmode(&m,"pre-domain"))pre.words[1][5]=8;
 if(pmode(&m,"pre-busy"))pre.words[0][m.point]=1;
 if(pmode(&m,"pre-time"))census.end_us=UINT64_MAX;
 if(pmode(&m,"pre-context"))census.context[1][0]=0;
 if(pmode(&m,"pre-wait"))census.words[1][m.point][6]=1;
 if(pmode(&m,"pre-active"))census.enabled=1;
 if(pmode(&m,"pre-batches"))census.batches=0;
 if(pmode(&m,"pre-count"))pre.counts[1]=5;
 struct intel_gt_borrow extra={0};if(pmode(&m,"extra"))assert(intel_gt_access_borrow(&g,&extra));
 struct intel_engine_prepare_result r={0};m.result=&r;
 struct intel_engine_prepare_io io={&m,prepare_guard,prepare_sample,prepare_control,prepare_transition,prepare_preserve,prepare_now,prepare_pause};
 bool ok=intel_engine_prepare(&r,&g,pmode(&m,"copy")?&copied:&pre,!pmode(&m,"policy"),io);
 bool positive=pmode(&m,"normal") || pmode(&m,"wait");
 assert(ok==positive);
 if(positive) {
  assert(r.ready==31 && r.cancelled==31 && r.held && r.preserved && r.returned && !r.uncertain && !r.reason && r.writes==10 && r.attempts==10 && r.acks==10 && r.snapshots==4 && r.blocked);
  assert(r.reads==34+(pmode(&m,"wait")?10:0) && r.polls==(pmode(&m,"wait")?10:0));
 } else assert(r.reason);
 if(pmode(&m,"guard-time") && m.point==1)assert(!r.attempts && !m.samples && r.reason&IEP_CLOCK);
 if(pmode(&m,"clock-epoch") && (m.point==2 || m.point==3))assert(!m.samples && !r.attempts);
 if(pmode(&m,"write-uncertain"))assert(r.uncertain && r.borrow.active && r.reason&IEP_WRITE && r.attempts==m.point && r.writes==m.point-1);
 if(pmode(&m,"first-ack-time"))assert(r.uncertain && r.borrow.active && r.reason&IEP_TIMEOUT && !r.polls && m.stores==m.point);
 if(pmode(&m,"write-none") && m.point==1)assert(r.returned && !r.uncertain && !r.writes);
 if(!strncmp(m.mode,"write-",6) || !strncmp(m.mode,"ack-",4) || !strncmp(m.mode,"poll-",5) || pmode(&m,"phase-time") || pmode(&m,"never-ready"))assert(m.stores==m.point);
 if(!positive && r.writes && !pmode(&m,"end-deadline") && !pmode(&m,"end-epoch"))assert(r.uncertain && r.borrow.active);
 if(pmode(&m,"never-ready"))assert(r.polls==256 && r.reason&IEP_TIMEOUT);
 if(r.borrow.active)assert(!intel_gt_access_close(&g));
 if(pmode(&m,"end-deadline"))assert(r.returned && !r.uncertain && r.reason&IEP_CLOCK);
 if(pmode(&m,"copy") || !strncmp(m.mode,"pre-",4) || pmode(&m,"extra") || pmode(&m,"policy"))assert(!r.attempts && !m.stores);
 if(pmode(&m,"sample-count"))assert(r.reason&IEP_COUNT && !r.counts[m.point-1]);
 struct intel_engine_prepare_result saved=r,copy=r;const struct intel_gt_borrow *dependent=g.dependents;unsigned borrows=g.borrows;
 assert(!intel_engine_prepare(&r,&g,&pre,true,io) && !memcmp(&r,&saved,sizeof(r)) && g.dependents==dependent && g.borrows==borrows);
 assert(!intel_engine_prepare(&copy,&g,&pre,true,io) && !memcmp(&copy,&saved,sizeof(copy)) && g.dependents==dependent && g.borrows==borrows);
 puts("engine-prepare: PASS bounded five-engine hold/cancel, exact stores, prefix and retained ownership");
 return 0;
}

#endif
