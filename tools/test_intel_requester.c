#define INTEL_TAKEOVER_FIXTURE_ONLY
#include "test_intel_takeover.c"
#undef INTEL_TAKEOVER_FIXTURE_ONLY
#include <srvros/intel_pci_requester.h>
struct requester_model {struct takeover_model base;const char *mode;unsigned point,guards,loads,samples,clocks;uint64_t offset;struct intel_pci_requester_result *result;};
static bool mode(struct requester_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t requester_now(void *p) {
 struct requester_model *m=p;++m->clocks;
 if(mode(m,"clock-first") && m->clocks>=m->point)return m->clocks==m->point?1:0;
 if(mode(m,"zero") && m->clocks==m->point)return 0;
 if(mode(m,"backward") && m->clocks==m->point)return 1;
 if(mode(m,"clock-epoch") && m->clocks==m->point)m->base.parent->engine_epoch++;
 if(mode(m,"end-deadline") && m->result->returned)m->offset=100001;
 if(mode(m,"end-epoch") && m->result->returned)m->base.parent->engine_epoch++;
 uint64_t t=now(&m->base.gt)+m->offset;
 return mode(m,"frozen") && m->clocks>1?m->result->begin_us:t;
}
static bool requester_guard(void *p) {
 struct requester_model *m=p;++m->guards;
 if(mode(m,"guard-time") && m->guards==m->point)m->offset=100001;
 if(mode(m,"guard-epoch") && m->guards==m->point)m->base.parent->engine_epoch++;
 if(mode(m,"guard-detach") && m->guards==m->point)m->base.parent->dependents=NULL;
 return guard(&m->base.gt) && !(mode(m,"guard") && m->guards==m->point);
}
static bool requester_sample(void *p,uint32_t out[9],unsigned *count) {
 struct requester_model *m=p;unsigned n=m->samples++;*count=0;
 const uint32_t words[9]={0x16268086,0x00100006,0x03000009,0x011b106b,0x90,0x0000d005,0x0002a401,0x03060013,0};
 for(unsigned i=0;i<9;i++) {
  unsigned at=++m->loads;
  if(mode(m,"read") && at==m->point)return false;
  out[i]=words[i];
  if(mode(m,"float") && at==m->point)out[i]=UINT32_MAX;
  if(mode(m,"profile") && i==m->point)out[i]^=1;
  if(mode(m,"master-off") && i==1)out[i]&=~4u;
  if(mode(m,"pending") && i==8)out[i]=256;
  if(mode(m,"pending-transition") && i==8 && n)out[i]=256;
  if(mode(m,"status-transition") && i==1 && n)out[i]|=1u<<19;
  if(mode(m,"flr") && i==8)out[i]=1;
  if(mode(m,"status-reserved") && i==8)out[i]=512;
  if(mode(m,"count") && n+1==m->point){*count=10;return true;}
  ++*count;
  if(out[i]==UINT32_MAX)return false;
 }
 if(mode(m,"sample-epoch") && n+1==m->point)m->base.parent->engine_epoch++;
 if(mode(m,"sample-time") && n+1==m->point)m->offset=100001;
 return true;
}
int main(int argc,char **argv) {
 struct requester_model m={.base={.gt={.t=1000,.mode="normal"},.mode="normal"},.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m.base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));m.base.parent=&g;
 struct intel_completion_result c={0};batch_parent=&g;assert(intel_completion_observe(&c,&g,(struct intel_completion_io){&m.base.gt,obs_guard,group,context,obs_now,batch_sample}));m.base.census=&c;
 struct intel_takeover_result t={0};m.base.result=&t;assert(intel_takeover_observe(&t,&g,&c,(struct intel_takeover_io){&m.base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_takeover_result copy=t;
 if(mode(&m,"pre-epoch"))g.engine_epoch++;
 if(mode(&m,"pre-count"))t.counts[1]=5;
 if(mode(&m,"pre-time"))t.end_us=UINT64_MAX;
 if(mode(&m,"pre-census"))c.batches=0;
 struct intel_gt_borrow extra={0};if(mode(&m,"extra"))assert(intel_gt_access_borrow(&g,&extra));
 struct intel_pci_requester_result r={0};m.result=&r;
 bool ok=intel_pci_requester_observe(&r,&g,mode(&m,"copy")?&copy:&t,(struct intel_pci_requester_io){&m,requester_guard,requester_sample,requester_now});
 bool positive=mode(&m,"normal") || mode(&m,"master-off") || mode(&m,"pending") || mode(&m,"pending-transition") || mode(&m,"status-transition");
 assert(ok==positive);
 if(positive) {
  assert(r.reads==18 && r.samples==2 && r.profiles==2 && r.attempts==2 && r.returned && r.blocked && !r.reason && !r.borrow.active);
  assert(r.bus_master==(mode(&m,"master-off")?0u:3u));
  assert(r.pending==(mode(&m,"pending")?3u:mode(&m,"pending-transition")?2u:0u));
  assert(r.stable==!(mode(&m,"pending-transition") || mode(&m,"status-transition")));
 }else assert(r.reason);
 if(mode(&m,"guard-time") && m.point==1)assert(!r.attempts && !m.loads && r.reason&IPR_CLOCK);
 if(mode(&m,"clock-epoch") && (m.point==2 || m.point==3))assert(!r.attempts && !m.loads && r.reason&IPR_PARENT);
 if(mode(&m,"guard-epoch") || mode(&m,"guard-detach") || mode(&m,"sample-epoch"))assert(r.borrow.active && !r.returned && r.reason&IPR_PARENT && r.reason&IPR_RETURN);
 if(mode(&m,"copy") || !strncmp(m.mode,"pre-",4) || mode(&m,"extra"))assert(!m.loads && !r.attempts);
 if(mode(&m,"read"))assert(r.reads==m.point-1);
 if(mode(&m,"float"))assert(r.reads==m.point);
 if(mode(&m,"count"))assert(r.reason&IPR_COUNT && !r.counts[m.point-1]);
 if(mode(&m,"end-deadline"))assert(r.returned && r.reason&IPR_CLOCK);
 if(mode(&m,"clock-first"))assert(r.reason&IPR_CLOCK && r.clock_at==1 && r.clock_prev>1);
 struct intel_pci_requester_result saved=r,rcopy=r;
 const struct intel_gt_borrow *dependent=g.dependents;unsigned borrows=g.borrows;
 assert(!intel_pci_requester_observe(&r,&g,&t,(struct intel_pci_requester_io){0}) && !memcmp(&r,&saved,sizeof(r)));
 assert(!intel_pci_requester_observe(&rcopy,&g,&t,(struct intel_pci_requester_io){0}) && !memcmp(&rcopy,&saved,sizeof(rcopy)) && g.dependents==dependent && g.borrows==borrows);
 printf("requester: PASS RO AF raw prefixes, no pending/master drain grant; guards=%u reads=%u\n",m.guards,m.loads);
 return 0;
}
