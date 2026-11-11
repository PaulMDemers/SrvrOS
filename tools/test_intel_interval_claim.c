/* Real GT/census/preparation/reset/re-arm, no forged native source token. */
#define INTEL_ENGINE_REHOLD_FIXTURE_ONLY
#include "test_intel_engine_rehold.c"
#undef INTEL_ENGINE_REHOLD_FIXTURE_ONLY
#include <srvros/intel_interval_claim.h>
struct claim_model {
 struct rehold_model base;
 struct intel_interval_claim claim,other;
 struct intel_held_interval_io io;
 const char *mode;
 unsigned point,clocks,guards,loads,transfers,check_clock;
 bool reentered;
 struct intel_interval_borrow borrow,second;
 unsigned borrow_clocks,borrow_guards;
 bool borrower_reentered;
};
static bool cmode(struct claim_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t cnow(void *ctx) {
 struct claim_model *m=ctx;++m->clocks;
 if(m->borrow.active || m->second.active) {
  ++m->borrow_clocks;
  if(!m->borrower_reentered) {
   m->borrower_reentered=true;
   unsigned before=m->clocks;
   struct scanout_transfer_manifest manifest;
   assert(!intel_interval_borrow_check(&m->borrow));
   assert(!intel_interval_borrow_return(&m->borrow));
   assert(!intel_interval_borrow_manifest(&m->borrow,&manifest));
   assert(!intel_interval_claim_close(&m->claim));
   assert(m->clocks==before);
  }
  if(m->borrow_clocks==m->point) {
   if(cmode(m,"borrow-zero"))return 0;
   if(cmode(m,"borrow-backward"))return 1;
   if(cmode(m,"borrow-deadline"))return m->claim.origin_us+100001;
  }
 }
 if(cmode(m,"reentry") && !m->reentered) {
  m->reentered=true;unsigned clocks=m->clocks,guards=m->guards,loads=m->loads;
  struct intel_interval_claim saved=m->claim,copy=saved;copy.object=&copy;
  assert(!intel_interval_claim_open(&m->claim,&m->base.child,&m->base.base.snapshot,m->io));
  assert(!intel_interval_claim_close(&m->claim));
  assert(!intel_interval_claim_check(&m->claim));
  assert(!intel_interval_claim_check(&copy));
  assert(!intel_interval_claim_open(&m->other,&m->base.child,&m->base.base.snapshot,m->io));
  assert(!memcmp(&saved,&m->claim,sizeof saved));
  assert(m->clocks==clocks && m->guards==guards && m->loads==loads);
 }
 if(m->clocks>=m->point) {
  if(cmode(m,"zero") && m->clocks==m->point)return 0;
  if(cmode(m,"backward") && m->clocks==m->point)return 1;
  if(cmode(m,"prep-deadline"))return m->claim.origin_us+100001+m->clocks-m->point;
  if(cmode(m,"owner-deadline"))return m->claim.owner_origin_us+300001+m->clocks-m->point;
 }
 if(cmode(m,"frozen"))return m->claim.last_us;
 return prepare_now(&m->base.base.base.m);
}
static bool cguard(void *ctx) {
 struct claim_model *m=ctx;++m->guards;
 struct intel_interval_borrow *b=m->borrow.active?&m->borrow:&m->second;
 if(b->active && !b->busy) {
  unsigned clocks=m->clocks;
  assert(!intel_interval_borrow_check(b));
  assert(!intel_interval_borrow_return(b));
  assert(!b->fault && m->clocks==clocks);
 }
 if(b->active && ++m->borrow_guards==m->point) {
  if(cmode(m,"borrow-guard"))return false;
  if(cmode(m,"borrow-drop"))m->claim.dependent=0;
  if(cmode(m,"borrow-count"))m->claim.borrows=0;
  if(cmode(m,"borrow-register"))m->claim.registered=0;
  if(cmode(m,"borrow-parent"))b->claim=0;
  if(cmode(m,"borrow-epoch"))++b->epoch;
  if(cmode(m,"borrow-sequence"))++b->sequence;
  if(cmode(m,"borrow-object"))b->object=0;
  if(cmode(m,"borrow-inactive"))b->active=false;
  if(cmode(m,"borrow-rebind")) {
   m->second=m->borrow;m->second.object=&m->second;
   m->claim.dependent=&m->second;
  }
 }
 assert(m->claim.pinned && m->base.child.dependent_borrows==1 && !m->base.child.dependent_returns);
 assert(!intel_engine_rehold_return(&m->base.child));
 assert(!intel_gt_access_close(m->base.base.base.reset->parent));
 if(cmode(m,"input-mutation")){m->base.base.snapshot.original[0]^=1;m->base.base.snapshot.mmio=0;}
 if(m->guards==m->point) {
  if(cmode(m,"guard"))return false;
  if(cmode(m,"guard-epoch"))++m->base.base.base.reset->parent->engine_epoch;
  if(cmode(m,"guard-owner"))++m->base.base.base.reset->parent->begin;
  if(cmode(m,"guard-detach")){m->base.child.dependent=0;assert(!intel_engine_rehold_return(&m->base.child));}
  if(cmode(m,"guard-rebind"))m->base.child.dependent=&m->other;
  if(cmode(m,"guard-snapshot"))m->claim.captured.original[0]^=1;
 }
 for(unsigned i=0;i<5;i++)assert(m->base.base.base.m.control[i]==3);
 return true;
}
static bool cread(void *ctx,uint32_t off,uint32_t *out) {
 struct claim_model *m=ctx;unsigned n=m->loads++;
 assert(n<12 && off==INTEL_WINDOW_PAGE+(n%3==1?0:4));
 assert(intel_engine_rehold_child_live(&m->base.child));
 if(cmode(m,"read") && m->loads==m->point)return false;
 *out=(uint32_t)(m->base.base.original>>(n%3==1?0:32));
 if(m->loads==m->point) {
  if(cmode(m,"drift"))*out^=1;
  if(cmode(m,"read-epoch"))++m->base.base.base.reset->parent->engine_epoch;
  if(cmode(m,"read-detach"))m->base.child.dependent=0;
 }
 return true;
}
static bool transfer_guard(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase p) {
 (void)m;(void)p;struct claim_model *model=ctx;++model->transfers;return true;
}
static bool exercise_borrow(struct claim_model *m) {
 struct intel_interval_claim *c=&m->claim;
 struct intel_interval_borrow *b=&m->borrow;
 c->borrows=c->returns=UINT32_MAX;
 assert(!intel_interval_borrow_open(c,b) && !b->object && !c->dependent);
 c->borrows=c->returns=0;
 assert(!intel_interval_borrow_open(c,(void *)c));
 assert(!intel_interval_borrow_open(c,(void *)&m->base.child));
 if(!intel_interval_borrow_open(c,b))return false;
 assert(c->dependent==b && c->registered==b && c->borrows==1 && !c->returns);
 unsigned clocks=m->clocks;
 struct intel_interval_borrow copy=*b;copy.object=&copy;
 assert(!intel_interval_borrow_check(&copy) && !intel_interval_borrow_return(&copy));
 assert(!intel_interval_claim_close(c) && m->clocks==clocks);
 assert(!intel_interval_borrow_open(c,&m->second));
 if(!intel_interval_claim_check(c))return false;
 if(cmode(m,"borrow-lost-between")) {
  c->dependent=0;
  assert(!intel_interval_borrow_check(b) && b->fault && c->reason==IIC_SOURCE);
  c->dependent=b;
  assert(!intel_interval_borrow_check(b) && !intel_interval_borrow_return(b));
  assert(!intel_interval_claim_close(c));
  return false;
 }
 struct scanout_transfer_manifest manifest;
 assert(intel_interval_borrow_manifest(b,&manifest) && !manifest.assurances && manifest.obligations==255);
 assert(!intel_interval_claim_manifest(c,(void *)b));
 assert(!intel_interval_borrow_manifest(b,(void *)b));
 struct intel_bdw_ggtt_page page;
 assert(intel_interval_borrow_page(b,0x12345000,&page));
 assert(page.index==8704 && page.mmio_offset==0x811000 && page.gpu_address==0x2200000);
 assert(page.physical==0x12345000 && page.replacement==0x12345001 && page.original==c->expected);
 assert(!intel_interval_borrow_page(b,0x12345000,(void *)c));
 assert(!intel_interval_borrow_page(b,0x12345000,(void *)b));
 assert(!intel_interval_borrow_page(b,0x12345000,(void *)c->parent));
 if(!intel_interval_borrow_check(b) || !intel_interval_borrow_return(b))return false;
 assert(!b->active && !c->dependent && !c->registered && c->returns==c->borrows);
 assert(!intel_interval_borrow_check(b) && !intel_interval_borrow_return(b));
 assert(!intel_interval_borrow_open(c,b) && !intel_interval_borrow_manifest(b,&manifest));
 assert(!intel_interval_borrow_page(b,0x12345000,&page));
 if(!intel_interval_borrow_open(c,&m->second) || !intel_interval_borrow_return(&m->second))return false;
 assert(c->borrows==2 && c->returns==2);
 return true;
}
static bool claim_observe(void *ctx,struct intel_engine_rehold *r) {
 struct claim_model *m=ctx;assert(r==&m->base.result);
 assert(intel_engine_rehold_borrow(r,&m->base.child));
 if(cmode(m,"bad-input"))m->base.base.snapshot.assurances=1;
 bool ok=intel_interval_claim_open(&m->claim,&m->base.child,&m->base.base.snapshot,m->io);
 if(!ok)return false;
 struct intel_interval_claim saved=m->claim,copy=saved;
 copy.object=&copy;copy.opening.object=&copy.opening;copy.opening.manifest.original=&copy.opening.expected;
 unsigned copy_clocks=m->clocks,copy_guards=m->guards,copy_loads=m->loads;
 assert(!intel_interval_claim_check(&copy) && !intel_interval_claim_close(&copy));
 assert(!memcmp(&saved,&m->claim,sizeof saved) && copy_clocks==m->clocks && copy_guards==m->guards && copy_loads==m->loads);
 struct scanout_transfer_manifest manifest;
 assert(intel_interval_claim_manifest(&m->claim,&manifest));
 assert(manifest.original==&m->claim.expected && manifest.original[0]==m->base.base.original &&
  manifest.source_epoch==r->epoch && manifest.pages==1 && manifest.first==8704 &&
  !manifest.assurances && manifest.obligations==255);
 assert(!intel_interval_claim_manifest(&m->claim,(void *)&m->base.child));
 assert(!intel_interval_claim_manifest(&m->claim,(void *)r));
 assert(!intel_interval_claim_manifest(&m->claim,(void *)r->reset));
 assert(!intel_interval_claim_manifest(&m->claim,(void *)r->preparation));
 assert(!intel_interval_claim_manifest(&m->claim,(void *)r->parent));
 static struct scanout_transfer transfer;struct scanout_transfer_pool pool={0};
 struct scanout_space_pool spaces={0};struct scanout_transfer_token out={0};
 assert(!scanout_transfer_open(&transfer,&pool,&spaces,&r->parent->exclusion,&manifest,
  (struct scanout_transfer_io){m,transfer_guard,cnow},&out));
 assert(!m->transfers && !transfer.object && !spaces.epoch && !pool.epoch && !out.object);
 if(!strncmp(m->mode,"borrow",6) && !exercise_borrow(m))return false;
 if(cmode(m,"leave-open"))return true;
 m->check_clock=m->clocks+1;
 bool checked=intel_interval_claim_check(&m->claim);
 if((cmode(m,"prep-deadline") || cmode(m,"owner-deadline")) && m->point>=m->check_clock && m->point<=m->clocks)assert(!checked);
 if(!checked)return false;
 if(!intel_interval_claim_close(&m->claim))return false;
 assert(m->claim.state==IIC_CLOSED && !m->claim.pinned && m->loads==12);
 assert(!m->base.child.dependent && m->base.child.dependent_borrows==1 && m->base.child.dependent_returns==1);
 assert(!intel_interval_claim_manifest(&m->claim,&manifest));
 assert(!intel_interval_claim_close(&m->claim) && !intel_interval_claim_check(&m->claim));
 unsigned before=m->clocks;
 assert(!intel_interval_claim_open(&m->claim,&m->base.child,&m->base.base.snapshot,m->io));
 assert(!intel_interval_claim_open(&m->other,&m->base.child,&m->base.base.snapshot,m->io));
 assert(before==m->clocks);
 return intel_engine_rehold_return(&m->base.child);
}
static bool claim_end(void *ctx) {
 struct claim_model *m=ctx;assert(hrend(&m->base.base.base));
 if(cmode(m,"owner-deadline"))m->base.base.base.reset->parent->begin=m->base.base.base.m.base.gt.t-299500;
 return intel_engine_rehold_run(&m->base.result,m->base.base.base.reset,m->base.io);
}
#ifndef INTEL_INTERVAL_CLAIM_FIXTURE_ONLY
int main(int argc,char **argv) {
 (void)rehold_end;(void)robserve;(void)interval_end;(void)prepare_preserve;
 struct claim_model m={0};struct rehold_model *rm=&m.base;struct interval_model *im=&rm->base;
 struct held_reset_model *hr=&im->base;struct prepare_model *pm=&hr->m;
 m.mode=argc>1?argv[1]:"normal";m.point=argc>2?(unsigned)atoi(argv[2]):1;
 pm->base.gt.t=1000000;pm->base.gt.mode="normal";pm->base.mode="normal";pm->mode="normal";
 hr->mode="normal";im->kind="normal";rm->kind="normal";
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&pm->base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));pm->base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&pm->base.gt,obs_guard,group,context,obs_now,batch_sample}));pm->base.census=&census;
 struct intel_takeover_result pre={0};pm->base.result=&pre;assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&pm->base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result preparation={0};pm->result=&preparation;
 struct intel_engine_held_reset_result reset={0};hr->reset=&reset;
 im->original=0x8e201001;im->snapshot=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};im->snapshot.original[0]=im->original;
 struct intel_engine_prepare_io io={&m,prepare_guard,prepare_sample,prepare_control,prepare_transition,hrpreserve,prepare_now,prepare_pause};
 struct intel_engine_held_reset_io reset_io={&m,hrguard,hrsnapshot,hrdomain,hrtransition,hrbegin,claim_end,hrnow,hrpause};hr->io=reset_io;
 rm->io=(struct intel_engine_rehold_io){&m,rguard,rsample,rcontrol,rtransition,claim_observe,rnow,rpause};
 m.io=(struct intel_held_interval_io){&m,cguard,cread,cnow};
 bool ok=intel_engine_prepare_with_bcs_reset(&preparation,&g,&pre,true,io,(struct intel_engine_prepare_action){&m,hraction},&reset,reset_io);
 bool positive=cmode(&m,"normal") || cmode(&m,"input-mutation") || cmode(&m,"reentry") || cmode(&m,"borrow");assert(ok==positive);
 if(positive) {
  assert(m.claim.state==IIC_CLOSED && !m.claim.pinned && !m.claim.reason && m.claim.opening.reads==6 && m.claim.closing.reads==6);
  assert(rm->result.complete && preparation.complete && reset.complete && preparation.returned && !g.borrows);
  assert(intel_gt_access_close(&g));
 } else {
  assert(!rm->result.complete && !preparation.complete && preparation.uncertain && preparation.borrow.active && !intel_gt_access_close(&g));
  assert(m.claim.pinned || cmode(&m,"bad-input"));
  unsigned clocks=m.clocks,guards=m.guards,loads=m.loads;
  assert(!intel_interval_claim_check(&m.claim) && !intel_interval_claim_close(&m.claim));
  if(m.claim.reason)assert(clocks==m.clocks && guards==m.guards && loads==m.loads);
  assert(!intel_engine_rehold_return(&rm->child));
  assert(rm->result.writes==1 && !rm->result.restored);
  if(!cmode(&m,"leave-open"))assert(m.claim.reason && m.claim.state==IIC_FAULT);
  if(cmode(&m,"zero") || cmode(&m,"backward") || cmode(&m,"prep-deadline") || cmode(&m,"owner-deadline") || cmode(&m,"frozen")) {
   assert(m.claim.reason&IIC_CLOCK);assert(m.claim.clock_prev);
   if(cmode(&m,"zero"))assert(!m.claim.clock_at);
   if(cmode(&m,"backward"))assert(m.claim.clock_at==1);
   if(cmode(&m,"prep-deadline"))assert(m.claim.clock_at==m.claim.origin_us+100001);
   if(cmode(&m,"owner-deadline"))assert(m.claim.clock_at==m.claim.owner_origin_us+300001);
   if(cmode(&m,"frozen"))assert(m.claim.clock_at==m.claim.clock_prev);
  }
 }
 printf("interval-claim: PASS mode=%s point=%u clocks=%u guards=%u loads=%u check_clock=%u borrow_clocks=%u borrow_guards=%u\n",m.mode,m.point,m.clocks,m.guards,m.loads,m.check_clock,m.borrow_clocks,m.borrow_guards);
 return 0;
}

#endif
