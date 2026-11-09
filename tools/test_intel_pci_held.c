/* The actual prepare, takeover and GT provider fixture; no replacement state machine. */
#define INTEL_ENGINE_PREPARE_FIXTURE_ONLY
#include "test_intel_engine_prepare.c"
#undef INTEL_ENGINE_PREPARE_FIXTURE_ONLY
#include <srvros/intel_pci_held.h>
struct held_model {
 struct prepare_model prepare;
 struct intel_engine_prepare_result *preparation;
 struct intel_pci_held_result result;
 struct intel_pci_held_io io;
 const char *mode;
 unsigned point,guards,loads,clocks,calls;
 bool nested;
};
static bool hmode(struct held_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t held_now(void *ctx) {
 struct held_model *m=ctx;++m->clocks;
 if(hmode(m,"clock-zero") && m->clocks==m->point)return 0;
 if(hmode(m,"clock-backward") && m->clocks==m->point)return 1;
 if(hmode(m,"frozen") && m->clocks>1)return m->result.begin_us;
 if(hmode(m,"final-epoch") && m->result.profiles==2)m->prepare.base.parent->engine_epoch++;
 if(hmode(m,"final-inactive") && m->result.profiles==2)m->preparation->action_active=false;
 return prepare_now(&m->prepare);
}
static bool held_guard(void *ctx) {
 struct held_model *m=ctx;++m->guards;
 assert(m->preparation->action_active);
 if((hmode(m,"exit-float") || hmode(m,"exit-read")) && m->loads==m->point)m->prepare.base.parent->engine_epoch++;
 if(hmode(m,"guard-epoch") && m->guards==m->point)m->prepare.base.parent->engine_epoch++;
 if(hmode(m,"guard-detach") && m->guards==m->point)m->prepare.base.parent->dependents=NULL;
 if(hmode(m,"guard-time") && m->guards==m->point)m->prepare.offset=100001;
 if(hmode(m,"origin-change") && m->guards==m->point)m->preparation->begin_us++;
 if(hmode(m,"action-inactive") && m->guards==m->point)m->preparation->action_active=false;
 return prepare_guard(&m->prepare) && !(hmode(m,"guard") && m->guards==m->point);
}
static bool held_read(void *ctx,uint32_t offset,uint32_t *value) {
 struct held_model *m=ctx;unsigned i=m->loads++%9,n=m->loads;
 const uint32_t words[9]={0x16268086,0x00100006,0x03000009,0x011b106b,0x90,0x0000d005,0x0002a401,0x03060013,0};
 assert(offset==intel_pci_requester_offsets[i]);
 assert(m->preparation->action_active && m->preparation->borrow.active);
 assert(m->prepare.base.parent->borrows==1 && m->prepare.base.parent->dependents==&m->preparation->borrow);
 assert(m->prepare.stores==5 && m->preparation->ready==31 && !m->preparation->cancelled);
 if(hmode(m,"reentry") && n==1) {
  struct intel_pci_held_result before=m->result,copy=m->result;
  assert(!intel_pci_held_observe(&m->result,m->preparation,m->io));
  assert(!intel_pci_held_observe(&copy,m->preparation,m->io));
  assert(!memcmp(&before,&m->result,sizeof(before)) && !memcmp(&before,&copy,sizeof(before)) && m->loads==1);
  m->nested=true;
 }
 if((hmode(m,"read") || hmode(m,"exit-read")) && n==m->point)return false;
 *value=words[i];
 if(hmode(m,"master-off") && i==1)*value&=~4u;
 if(hmode(m,"pending") && i==8)*value=256;
 if(hmode(m,"pending-transition") && n==18)*value=256;
 if(hmode(m,"status-transition") && n==11)*value|=1u<<19;
 if((hmode(m,"float") || hmode(m,"exit-float")) && n==m->point)*value=UINT32_MAX;
 if(hmode(m,"profile") && n==m->point)*value^=i==1?2u:1u;
 if(hmode(m,"read-epoch") && n==m->point)m->prepare.base.parent->engine_epoch++;
 if(hmode(m,"read-detach") && n==m->point)m->prepare.base.parent->dependents=NULL;
 if(hmode(m,"read-time") && n==m->point)m->prepare.offset=100001;
 if(hmode(m,"shared-budget") && n==1)m->prepare.offset=m->preparation->begin_us+100001-m->prepare.base.gt.t-1;
 return true;
}
static bool held_action(void *ctx,const struct intel_engine_prepare_result *preparation) {
 struct held_model *m=ctx;++m->calls;
 assert(m->calls==1 && preparation==m->preparation && preparation->action_active);
 assert(preparation->ready==31 && preparation->borrow.active && preparation->counts[1]==6 && !preparation->counts[2]);
 assert(intel_engine_prepare_action_live(preparation));
 struct intel_engine_prepare_result live_copy=*preparation;
 assert(!intel_engine_prepare_action_live(&live_copy));
 m->preparation->action_active=false;
 assert(!intel_engine_prepare_action_live(preparation));
 m->preparation->action_active=true;
 const struct intel_gt_borrow *borrow=m->prepare.base.parent->dependents;
 unsigned borrows=m->prepare.base.parent->borrows;
 struct intel_engine_prepare_result copied=*preparation;
 if(hmode(m,"elapsed"))m->prepare.offset=100001;
 bool ok=intel_pci_held_observe(&m->result,hmode(m,"copied-preparation")?&copied:preparation,m->io);
 assert(m->result.complete==ok);
 if(!hmode(m,"read-detach") && !hmode(m,"guard-detach")) {
  assert(m->prepare.base.parent->dependents==borrow && m->prepare.base.parent->borrows==borrows);
  assert(preparation->borrow.active); /* Sampler never returns prepare's pin. */
 }
 struct intel_pci_held_result saved=m->result,copy=m->result;
 unsigned loads=m->loads,guards=m->guards;
 assert(!intel_pci_held_observe(&m->result,preparation,m->io));
 assert(!intel_pci_held_observe(&copy,preparation,m->io));
 assert(!memcmp(&saved,&m->result,sizeof(saved)) && !memcmp(&saved,&copy,sizeof(copy)) && m->loads==loads && m->guards==guards);
 return ok;
}
int main(int argc,char **argv) {
 struct held_model m={.prepare={.base={.gt={.t=1000,.mode="normal"},.mode="normal"},.mode="normal"},.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
 struct prepare_model *p=&m.prepare;
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&p->base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));p->base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;
 assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&p->base.gt,obs_guard,group,context,obs_now,batch_sample}));p->base.census=&census;
 struct intel_takeover_result pre={0};p->base.result=&pre;
 assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&p->base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result preparation={0};assert(!intel_engine_prepare_action_live(&preparation));p->result=&preparation;m.preparation=&preparation;
 m.io=(struct intel_pci_held_io){&m,held_guard,held_read,held_now};
 bool ok=intel_engine_prepare_with_action(&preparation,&g,&pre,true,(struct intel_engine_prepare_io){p,prepare_guard,prepare_sample,prepare_control,prepare_transition,prepare_preserve,prepare_now,prepare_pause},(struct intel_engine_prepare_action){&m,held_action});
 bool positive=hmode(&m,"normal") || hmode(&m,"master-off") || hmode(&m,"pending") || hmode(&m,"pending-transition") || hmode(&m,"status-transition") || hmode(&m,"reentry") || hmode(&m,"after-action");
 assert(ok==positive && m.calls==1 && !preparation.action_active && !intel_engine_prepare_action_live(&preparation));
 struct intel_pci_held_result *r=&m.result;
 assert(r->counts[0]<=9 && r->counts[1]<=9 && r->reads==r->counts[0]+r->counts[1]);
 assert(r->samples==(unsigned)(r->counts[0]==9)+(unsigned)(r->counts[1]==9));
 assert(r->attempts>=r->reads && r->attempts<=r->reads+1);
 if(positive) {
  assert(r->complete && r->counts[0]==9 && r->counts[1]==9 && r->reads==18 && r->attempts==18 && r->samples==2 && r->profiles==2 && !r->reason);
  assert(r->preparation==&preparation && r->parent==&g && r->borrow==&preparation.borrow && r->epoch==g.engine_epoch);
  assert(r->pending==(hmode(&m,"pending")?3u:hmode(&m,"pending-transition")?2u:0u));
  assert(r->bus_master==(hmode(&m,"master-off")?0u:3u) && !r->flr);
  assert(r->stable==!(hmode(&m,"pending-transition") || hmode(&m,"status-transition")));
  assert(preparation.returned && preparation.preserved && preparation.cancelled==31 && !preparation.uncertain && p->stores==10);
  assert(!preparation.borrow.active && !g.borrows && !g.dependents);
  if(hmode(&m,"reentry"))assert(m.nested);
  struct intel_pci_held_result late={0};unsigned loads=m.loads,guards=m.guards;
  assert(!intel_pci_held_observe(&late,&preparation,m.io) && !late.complete && !late.reads && m.loads==loads && m.guards==guards);
  assert(intel_gt_access_close(&g));
 } else {
  assert(!r->complete && r->reason && preparation.reason&IEP_ACTION && preparation.uncertain && !preparation.returned);
  assert(preparation.borrow.active && !preparation.cancelled && !p->preserves && p->stores==5 && !intel_gt_access_close(&g));
  if(hmode(&m,"exit-float"))assert((r->reason&IPH_PROFILE) && (r->reason&IPH_PARENT) && r->reads==m.point && m.loads==m.point);
  if(hmode(&m,"exit-read"))assert((r->reason&IPH_READ) && (r->reason&IPH_PARENT) && r->reads==m.point-1 && m.loads==m.point);
  if(hmode(&m,"read"))assert(r->reads==m.point-1 && r->attempts==m.point && m.loads==m.point && r->reason&IPH_READ);
  if(hmode(&m,"float") || hmode(&m,"profile"))assert(r->reads==m.point && m.loads==m.point && r->reason&IPH_PROFILE);
  if(hmode(&m,"copied-preparation"))assert(!m.loads && !r->attempts);
  if(hmode(&m,"elapsed"))assert(!m.loads && !r->attempts && r->reason&IPH_CLOCK);
  if(hmode(&m,"shared-budget"))assert(m.loads==1 && r->attempts==1 && r->reads==1 &&
    (r->reason&IPH_CLOCK) && r->clock_at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US &&
    r->clock_at-r->begin_us<=INTEL_ENGINE_PREPARE_TOTAL_US);
  if(hmode(&m,"read-time") || hmode(&m,"guard-time") || hmode(&m,"elapsed"))assert(r->reason&IPH_CLOCK && r->clock_at-preparation.begin_us>INTEL_ENGINE_PREPARE_TOTAL_US);
 }
 printf("pci-held: PASS mode=%s point=%u raw=%u guards=%u under original prepare borrow\n",m.mode,m.point,m.loads,m.guards);
 return 0;
}
