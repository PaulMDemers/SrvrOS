/* Actual HDA census/owner plus GT/census/reset/re-arm/claim, one exclusion. */
#define INTEL_INTERVAL_CLAIM_FIXTURE_ONLY
#include "test_intel_interval_claim.c"
#undef INTEL_INTERVAL_CLAIM_FIXTURE_ONLY
#include <srvros/intel_hda_gpu.h>
struct joint_model {
 struct claim_model base;
 struct intel_hda_gpu scope;
 struct display_access_token exclusion;
 const char *kind;
 unsigned point,clocks,guards,reads,attempts,stores,inspects,borrows,returns,retire_calls,endpoints;
 unsigned command,clock_cause;
 uint64_t clock_at,last_sample;
 bool mapped,inspection,reentered;
};
static bool jmode(struct joint_model *m,const char *s){return !strcmp(m->kind,s);}
static uint64_t jnow(void *ctx) {
 struct joint_model *m=ctx;++m->clocks;uint64_t at=prepare_now(&m->base.base.base.base.m);
 if(m->clocks==m->point) {
  if(jmode(m,"zero"))at=0;
  if(jmode(m,"backward"))at=1;
  if(jmode(m,"backstep"))at=m->last_sample-1;
  if(jmode(m,"hda-deadline") && m->scope.hda.begin_us)at=m->scope.hda.begin_us+400001;
  if(jmode(m,"outer-deadline"))at=m->scope.outer_origin_us+500001;
  if(jmode(m,"zero") || jmode(m,"backward") || jmode(m,"backstep") || jmode(m,"outer-deadline") ||
     (jmode(m,"hda-deadline") && m->scope.hda.begin_us))m->clock_at=at,m->clock_cause=1;
 }
 /* Model the same native immediate-clock hook, including original raw sample
  * retention before the HDA/GT core receives refusal. No source is renewed. */
 if(m->scope.object==&m->scope && m->scope.pinned && !intel_hda_gpu_deadline(&m->scope,at))return 0;
 m->last_sample=at;
 return at;
}
static bool jendpoint(void *ctx,struct intel_hda_endpoint *e) {
 struct joint_model *m=ctx;++m->endpoints;
 *e=(struct intel_hda_endpoint){0x160c8086,0x04030009,0x011b106b,m->command,1,0,1,0xc1610004,0,true};
 if(m->endpoints==m->point) {
  if(jmode(m,"endpoint"))return false;
  if(jmode(m,"endpoint-drift"))e->command^=4;
 }
 return true;
}
static bool jguard(void *ctx) {
 struct joint_model *m=ctx;++m->guards;
 if(jmode(m,"reentry") && m->scope.state==IHG_ACTIVE && !m->reentered) {
  m->reentered=true;unsigned guards=m->guards,inspects=m->inspects,stores=m->stores;
  assert(!intel_hda_gpu_check(&m->scope) && !intel_hda_gpu_close(&m->scope,false));
  assert(guards==m->guards && inspects==m->inspects && stores==m->stores);
 }
 if(m->guards==m->point && jmode(m,"guard"))return false;
 return m->mapped && display_access_held(&m->exclusion);
}
static bool jmap(void *ctx){struct joint_model *m=ctx;assert(m->mapped && !m->inspection);m->inspection=true;++m->borrows;return true;}
static bool junmap(void *ctx){struct joint_model *m=ctx;assert(m->inspection);m->inspection=false;++m->returns;return true;}
static bool jread(void *ctx,unsigned off,unsigned width,uint32_t *out) {
 struct joint_model *m=ctx;assert(m->inspection && m->mapped && intel_hda_read_allowed(off,width));++m->reads;
 if(m->reads==m->point && jmode(m,"hda-read"))return false;
 *out=0;
 if(off==0){assert(width==2);*out=0x3001;}
 else if(off==8){assert(width==4);*out=1;}
 else if(off>=0x80 && off<0xe0 && (off-0x80)%32==2){assert(width==1);*out=4;}
 if(m->reads==m->point && jmode(m,"hda-drift"))*out^=1;
 return true;
}
static bool jinspect(void *ctx,struct intel_hda_result *r) {
 struct joint_model *m=ctx;++m->inspects;
 if(jmode(m,"mid-fail") && m->inspects==3)return false;
 return intel_hda_observe(r,true,(struct intel_hda_io){m,jendpoint,jguard,jmap,jread,junmap,jnow});
}
static bool jwrite(void *ctx,uint16_t value) {
 struct joint_model *m=ctx;++m->attempts;assert(!m->inspection && m->mapped);
 assert((m->command==6 && value==2 && m->attempts==1) || (m->command==2 && value==6 && m->attempts==2));
 if(value==6)assert(!m->scope.gpu->pinned && !m->scope.gpu->borrows && !m->scope.gpu->dependents);
 if(m->attempts==m->point && jmode(m,"none"))return false;
 m->command=value;++m->stores;
 if(m->attempts==m->point && jmode(m,"uncertain"))return false;
 return true;
}
static bool jretire(void *ctx) {
 struct joint_model *m=ctx;++m->retire_calls;assert(m->command==6 && !m->inspection && m->mapped);
 assert(!m->scope.gpu->pinned && !m->scope.gpu->borrows && !m->scope.gpu->dependents);
 assert(!display_access_release(&m->exclusion,true)); /* root pin retires last */
 if(jmode(m,"retire"))return false;
 m->mapped=false;return true;
}
static bool jclaim_guard(void *ctx) {
 struct joint_model *m=ctx;assert(m->command==2);
 assert(!intel_hda_owner_unpin(&m->scope.hda,&m->scope.borrow));
 if(jmode(m,"lost-hda-list"))m->scope.hda.dependents=0;
 if(jmode(m,"lost-hda-child"))m->scope.borrow.dependent=0;
 if(jmode(m,"lost-hda-count"))m->scope.borrow.dependent_borrows=0;
 return intel_hda_gpu_check(&m->scope) && cguard(&m->base);
}
static bool jgpu_guard(void *ctx) {
 struct joint_model *m=ctx;
 if(jmode(m,"initializing-dirty") && m->scope.gpu->state==GT_ACCESS_EMPTY)m->scope.gpu->write_attempts=1;
 return intel_hda_gpu_check(&m->scope) && guard(&m->base.base.base.base.m.base.gt);
}
static bool jclaim_observe(void *ctx,struct intel_engine_rehold *r) {
 struct joint_model *m=ctx;struct claim_model *c=&m->base;
 assert(intel_engine_rehold_borrow(r,&c->base.child));
 if(!intel_hda_gpu_attach(&m->scope,&c->claim))return false;
 assert(!intel_hda_gpu_attach(&m->scope,&c->claim));
 c->io=(struct intel_held_interval_io){m,jclaim_guard,cread,jnow};
 if(!intel_interval_claim_open(&c->claim,&c->base.child,&c->base.base.snapshot,c->io))return false;
 if(jmode(m,"claim-live"))return true;
 if(!intel_interval_claim_check(&c->claim) || !intel_interval_claim_close(&c->claim))return false;
 return intel_engine_rehold_return(&c->base.child);
}
static bool joint_end(void *ctx) {
 struct joint_model *m=ctx;assert(hrend(&m->base.base.base.base));
 return intel_engine_rehold_run(&m->base.base.result,m->base.base.base.base.reset,m->base.base.io);
}
static bool joint_preserve(void *ctx,const struct intel_completion_result *pre) {
 struct joint_model *m=ctx;struct held_reset_model *h=&m->base.base.base.base;
 ++h->m.preserves;assert(pre==h->m.base.census && h->m.result->borrow.active && h->stores==2);
 bool ok=intel_completion_observe(&h->post,h->m.base.parent,
  (struct intel_completion_io){m,obs_guard,group,context,jnow,batch_sample});
 return ok && h->post.engine_epoch==pre->engine_epoch+1 &&
  !memcmp(h->post.words,pre->words,sizeof(pre->words)) && !memcmp(h->post.context,pre->context,sizeof(pre->context));
}
int main(int argc,char **argv) {
 (void)claim_end;(void)claim_observe;(void)rehold_end;(void)robserve;(void)interval_end;(void)prepare_preserve;
 (void)hrpreserve;(void)takeover_now;(void)hrnow;(void)rnow;
 struct joint_model m={.last_sample=1000000};m.kind=argc>1?argv[1]:"normal";m.point=argc>2?(unsigned)atoi(argv[2]):1;
 struct claim_model *cm=&m.base;struct rehold_model *rm=&cm->base;struct interval_model *im=&rm->base;
 struct held_reset_model *hr=&im->base;struct prepare_model *pm=&hr->m;
 pm->base.gt.t=1000000;pm->base.gt.mode="normal";pm->base.mode="normal";pm->mode="normal";
 hr->mode="normal";im->kind="normal";rm->kind="normal";cm->mode="normal";m.command=6;m.mapped=true;
 assert(display_access_acquire(27,&m.exclusion));
 struct intel_gt_access g={0};pm->base.parent=&g;
 struct intel_hda_owner_facts facts={true,true,true,true,true,true,true,true};
 struct intel_hda_gpu_io hio={{&m,jinspect,jguard,jendpoint,jwrite,jnow},&m,jretire};
 /* Separate budgets: an outer overrun can precede HDA400ms expiry, whereas
  * an HDA overrun must fail even while the original outer500ms is valid. */
 uint64_t origin=pm->base.gt.t-(jmode(&m,"outer-deadline")?150000:0);
 bool opened=intel_hda_gpu_open(&m.scope,&g,&m.exclusion,origin,facts,hio);
 bool complete=false;
 if(opened) {
  assert(m.command==2 && m.scope.hda.pins==1 && intel_hda_gpu_live(&m.scope));
  assert(!display_access_release(&m.exclusion,true));
  assert(!intel_hda_owner_unpin(&m.scope.hda,&m.scope.borrow));
  struct intel_hda_gpu copy=m.scope;copy.object=&copy;copy.hda.object=&copy.hda;
  copy.hda.dependents=&copy.borrow;copy.token.owner=&copy.hda;copy.borrow.object=&copy.borrow;
  copy.borrow.owner=&copy.hda;copy.borrow.dependent=&copy;
  unsigned clocks=m.clocks,guards=m.guards,reads=m.reads;
  assert(!intel_hda_gpu_live(&copy) && !intel_hda_gpu_check(&copy) && !intel_hda_gpu_close(&copy,false));
  assert(clocks==m.clocks && guards==m.guards && reads==m.reads);
  if(jmode(&m,"clean-refusal"))assert(intel_hda_gpu_close(&m.scope,false));
  else if(jmode(&m,"false-pass")){assert(!intel_hda_gpu_close(&m.scope,true));}
  else {
   bool gpu_ok=intel_gt_access_open(&g,&m.exclusion,7,(struct intel_gt_io){&m,jgpu_guard,read_gt,write_gt,jnow,pause_gt,NULL});
   struct intel_completion_result census={0};batch_parent=&g;
   if(gpu_ok)gpu_ok=intel_completion_observe(&census,&g,(struct intel_completion_io){&m,obs_guard,group,context,jnow,batch_sample});
   pm->base.census=&census;
   struct intel_takeover_result pre={0};pm->base.result=&pre;
   if(gpu_ok)gpu_ok=intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&m,takeover_guard,takeover_sample,jnow});
   struct intel_engine_prepare_result preparation={0};pm->result=&preparation;struct intel_engine_held_reset_result reset={0};hr->reset=&reset;
   im->original=0x8e201001;im->snapshot=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};im->snapshot.original[0]=im->original;
   struct intel_engine_prepare_io io={&m,prepare_guard,prepare_sample,prepare_control,prepare_transition,joint_preserve,jnow,prepare_pause};
   struct intel_engine_held_reset_io resetio={&m,hrguard,hrsnapshot,hrdomain,hrtransition,hrbegin,joint_end,jnow,hrpause};hr->io=resetio;
   rm->io=(struct intel_engine_rehold_io){&m,rguard,rsample,rcontrol,rtransition,jclaim_observe,jnow,rpause};
   if(gpu_ok)gpu_ok=intel_engine_prepare_with_bcs_reset(&preparation,&g,&pre,true,io,(struct intel_engine_prepare_action){&m,hraction},&reset,resetio);
   if(gpu_ok)gpu_ok=intel_gt_access_close(&g);
   complete=intel_hda_gpu_close(&m.scope,gpu_ok);
  }
 }
 bool positive=jmode(&m,"normal") || jmode(&m,"reentry");assert((m.scope.complete && complete)==positive);
 if(positive || jmode(&m,"clean-refusal")) {
  assert(m.scope.state==IHG_CLOSED && !m.scope.pinned && !m.scope.retained && !m.mapped && m.command==6 && m.stores==2 && m.retire_calls==1);
  assert(m.reads==232 && m.borrows==m.returns && m.inspects==4);
  assert(display_access_release(&m.exclusion,true));
 } else {
  assert(m.scope.reason && !m.scope.complete);
  if(m.scope.state!=IHG_CLOSED)assert(m.scope.retained && m.scope.pinned && !display_access_release(&m.exclusion,true));
  else assert(!m.scope.pinned && !m.scope.retained && !m.mapped && !m.scope.hda.attempts && m.command==6 && display_access_release(&m.exclusion,true));
  unsigned reads=m.reads,stores=m.stores,guards=m.guards,clocks=m.clocks;
  assert(!intel_hda_gpu_check(&m.scope) && !intel_hda_gpu_close(&m.scope,false));
  assert(reads==m.reads && stores==m.stores && guards==m.guards && clocks==m.clocks);
  if(m.clock_cause) {
   assert((m.scope.reason&IHG_CLOCK) && m.scope.clock_cause && m.scope.clock_at==m.clock_at);
   unsigned cause=jmode(&m,"zero")?1:(jmode(&m,"hda-deadline")?4:(jmode(&m,"outer-deadline")?8:2));
   assert(m.scope.clock_cause&cause);
  }
 }
 printf("hda-gpu: PASS mode=%s point=%u clocks=%u guards=%u endpoints=%u reads=%u attempts=%u stores=%u claim_reads=%u checks=%u\n",m.kind,m.point,m.clocks,m.guards,m.endpoints,m.reads,m.attempts,m.stores,cm->loads,m.scope.checks);
 return 0;
}
