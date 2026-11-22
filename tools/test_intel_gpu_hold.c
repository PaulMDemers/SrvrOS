/* Real exclusion/GT/census/prepare/reset/re-arm/claim, never a forged source. */
#define INTEL_INTERVAL_CLAIM_FIXTURE_ONLY
#include "test_intel_interval_claim.c"
#undef INTEL_INTERVAL_CLAIM_FIXTURE_ONLY
#include <srvros/intel_gpu_requester_hold.h>
struct hold_model {
 struct claim_model base;
 struct intel_gpu_requester_hold hold;
 const char *mode;
 unsigned point,clocks,guards,reads,attempts,stores;
 uint16_t command;
 bool reentered,pending;
};
static bool hmode(struct hold_model *m,const char *s){return !strcmp(m->mode,s);}
static void hreenter(struct hold_model *m) {
 if(m->reentered || !m->hold.borrow.active)return;
 m->reentered=true;
 unsigned before=m->clocks;
 struct intel_gpu_requester_hold copy=m->hold;copy.object=&copy;
 assert(!intel_gpu_requester_hold_live(&copy));
 assert(!intel_gpu_requester_hold_run(&m->hold,&m->base.claim,m->hold.io));
 assert(!intel_interval_claim_close(&m->base.claim));
 assert(!intel_engine_rehold_return(&m->base.base.child));
 assert(m->clocks==before);
}
static uint64_t hnow(void *ctx) {
 struct hold_model *m=ctx;++m->clocks;hreenter(m);
 uint64_t at=cnow(&m->base);
 if(m->clocks==m->point) {
  if(hmode(m,"zero"))return 0;
  if(hmode(m,"backward"))return m->hold.last_us-1;
  if(hmode(m,"cross-backstep"))return m->base.claim.last_us-1;
  if(hmode(m,"forward-then-backward"))return at+500;
  if(hmode(m,"deadline"))return m->hold.origin_us+100001;
  if(hmode(m,"source"))m->hold.borrow.claim=0;
 }
 return at;
}
static bool hguard(void *ctx) {
 struct hold_model *m=ctx;
 if(m->hold.borrow.active && ++m->guards==m->point) {
  if(hmode(m,"guard"))return false;
  if(hmode(m,"drop"))m->base.claim.dependent=0;
 }
 return cguard(&m->base);
}
static uint32_t hword(struct hold_model *m,unsigned i) {
 const uint32_t w[]={0x16268086,0x00900007,0x03000009,0x011b106b,0x90,0xd005,0x22a401,0x03060013,0};
 if(i==1)return (w[i]&~65535u)|m->command;
 if(i==8 && m->command==3 && (m->pending || hmode(m,"pending") || hmode(m,"timeout")))return 256;
 return w[i];
}
static bool hsample(void *ctx,uint32_t out[9],unsigned *count) {
 struct hold_model *m=ctx;assert(intel_gpu_requester_hold_live(&m->hold));
 assert(m->hold.borrow.active && m->base.claim.pinned);hreenter(m);*count=0;
 for(unsigned i=0;i<9;i++) {
  ++m->reads;
  if(hmode(m,"read") && m->reads==m->point)return false;
  out[i]=hword(m,i);++*count;
  if(hmode(m,"drift") && m->reads==m->point)out[i]^=1;
 }
 return true;
}
static bool hpending(void *ctx,uint32_t out[2],unsigned *count) {
 struct hold_model *m=ctx;assert(m->command==3 && m->hold.state==IGH_INHIBITED);
 *count=0;
 for(unsigned i=0;i<2;i++) {
  ++m->reads;
  if(hmode(m,"read") && m->reads==m->point)return false;
  out[i]=i?(hmode(m,"timeout") || m->hold.polls<3?256:0):hword(m,1);
  ++*count;
  if(hmode(m,"drift") && m->reads==m->point)out[i]^=1;
 }
 return true;
}
static enum intel_gt_write hcommand(void *ctx,uint16_t command) {
 struct hold_model *m=ctx;++m->attempts;
 assert(intel_gpu_requester_hold_live(&m->hold));
 assert(m->attempts<=2 && command==(m->attempts==1?3:7));
 assert(m->command==(m->attempts==1?7:3));
 if(hmode(m,"none") && m->attempts==m->point)return GT_WRITE_NONE;
 m->command=command;++m->stores;
 return hmode(m,"uncertain") && m->attempts==m->point?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static void hpause(void *ctx){(void)ctx;}
static bool hold_observe(void *ctx,struct intel_engine_rehold *r) {
 struct hold_model *m=ctx;struct claim_model *c=&m->base;
 assert(intel_engine_rehold_borrow(r,&c->base.child));
 if(!intel_interval_claim_open(&c->claim,&c->base.child,&c->base.base.snapshot,c->io))return false;
 if(!intel_gpu_requester_hold_run(&m->hold,&c->claim,(struct intel_gpu_hold_io){m,hsample,hcommand,hpending,hnow,hpause}))return false;
 assert(m->hold.restored && m->hold.returned && !m->hold.borrow.active && m->command==7);
 assert(!intel_gpu_requester_hold_live(&m->hold));
 assert(!intel_gpu_requester_hold_run(&m->hold,&c->claim,m->hold.io));
 if(!intel_interval_claim_close(&c->claim))return false;
 return intel_engine_rehold_return(&c->base.child);
}
static bool hold_end(void *ctx) {
 struct hold_model *m=ctx;assert(hrend(&m->base.base.base.base));
 return intel_engine_rehold_run(&m->base.base.result,m->base.base.base.base.reset,m->base.base.io);
}
int main(int argc,char **argv) {
 (void)claim_end;(void)claim_observe;(void)rehold_end;(void)robserve;(void)interval_end;(void)prepare_preserve;
 struct hold_model m={.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1,.command=7,.pending=argc>3};
 struct claim_model *cm=&m.base;struct rehold_model *rm=&cm->base;struct interval_model *im=&rm->base;
 struct held_reset_model *hr=&im->base;struct prepare_model *pm=&hr->m;
 pm->base.gt.t=1000000;pm->base.gt.mode="normal";pm->base.mode="normal";pm->mode="normal";
 hr->mode="normal";im->kind="normal";rm->kind="normal";cm->mode="normal";
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&pm->base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));pm->base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&pm->base.gt,obs_guard,group,context,obs_now,batch_sample}));pm->base.census=&census;
 struct intel_takeover_result pre={0};pm->base.result=&pre;assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&pm->base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result preparation={0};pm->result=&preparation;
 struct intel_engine_held_reset_result reset={0};hr->reset=&reset;
 im->original=0x8e201001;im->snapshot=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};im->snapshot.original[0]=im->original;
 struct intel_engine_prepare_io io={&m,prepare_guard,prepare_sample,prepare_control,prepare_transition,hrpreserve,prepare_now,prepare_pause};
 struct intel_engine_held_reset_io reset_io={&m,hrguard,hrsnapshot,hrdomain,hrtransition,hrbegin,hold_end,hrnow,hrpause};hr->io=reset_io;
 rm->io=(struct intel_engine_rehold_io){&m,rguard,rsample,rcontrol,rtransition,hold_observe,rnow,rpause};
 cm->io=(struct intel_held_interval_io){&m,hguard,cread,cnow};
 bool ok=intel_engine_prepare_with_bcs_reset(&preparation,&g,&pre,true,io,(struct intel_engine_prepare_action){&m,hraction},&reset,reset_io);
 bool positive=hmode(&m,"normal") || hmode(&m,"pending");assert(ok==positive);
 if(positive) {
  assert(m.hold.complete && m.hold.samples==3 && m.hold.writes==2 && m.hold.attempts==2 && m.hold.reads==27+2*m.hold.polls);
  if(hmode(&m,"pending") || m.pending)assert(m.hold.polls==3);
  assert(m.hold.state==IGH_CLOSED && cm->claim.state==IIC_CLOSED && !cm->claim.pinned);
  assert(!g.borrows && intel_gt_access_close(&g));
 } else {
  if((hmode(&m,"read") || hmode(&m,"drift")) && m.point<=9)assert(!m.stores);
  assert(!m.hold.complete && m.hold.reason && m.hold.state==IGH_FAULT && m.hold.borrow.active);
  assert(cm->claim.pinned && preparation.uncertain && !intel_gt_access_close(&g));
  unsigned reads=m.reads,attempts=m.attempts,clocks=m.clocks;
  assert(!intel_gpu_requester_hold_run(&m.hold,&cm->claim,m.hold.io));
  assert(!intel_interval_claim_close(&cm->claim));
  assert(reads==m.reads && attempts==m.attempts && clocks==m.clocks);
 }
 printf("gpu-hold: PASS mode=%s point=%u clocks=%u guards=%u reads=%u attempts=%u stores=%u polls=%u\n",m.mode,m.point,m.clocks,m.guards,m.reads,m.attempts,m.stores,m.hold.polls);
}
