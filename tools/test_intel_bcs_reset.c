#define main prior_completion_fixture_main
#include "test_intel_completion_audit.c"
#undef main
#include <srvros/intel_bcs_reset.h>
#include <stdlib.h>
struct reset_model {
 struct model gt;
 struct intel_gt_access *parent;
 const char *mode;
 unsigned point,guards,reads,clocks,control_stores,domain_stores,groups,pauses,closing;
 uint32_t ctl,domain,start_epoch;
};
static bool mode(struct reset_model *m,const char *s){return !strcmp(m->mode,s);}
static bool rguard(void *p) {
 struct reset_model *m=p;++m->guards;
 if(mode(m,"detached-before") || (mode(m,"detached-after") && m->control_stores))m->parent->dependents=0;
 if(mode(m,"extra-dependent"))m->parent->borrows=2;
 if(mode(m,"foreign-epoch") && m->control_stores)m->parent->engine_epoch++;
 if(mode(m,"foreign-request") && m->control_stores)m->gt.request|=2;
 if(mode(m,"guard") && m->guards==m->point)return false;
 return guard(&m->gt) && (m->gt.request&65535)==1 && m->gt.ack==1;
}
static uint64_t rnow(void *p) {
 struct reset_model *m=p;++m->clocks;
 if(mode(m,"zero"))return 0;
 if(mode(m,"frozen"))return 2000;
 if(mode(m,"backward"))return 2000-m->clocks;
 if(mode(m,"clock-jump") && m->control_stores)m->gt.t+=100001;
 if(mode(m,"phase-deadline") && m->domain_stores)m->gt.t+=10001;
 return now(&m->gt);
}
static bool read_ok(struct reset_model *m) {
 ++m->reads;
 return !(mode(m,"read") && m->reads==m->point);
}
static bool rcontrol(void *p,uint32_t *v) {
 struct reset_model *m=p;if(!read_ok(m))return false;
 *v=m->ctl;
 if(mode(m,"initial-control"))*v=1;
 if(mode(m,"control-reserved"))*v=4;
 if(mode(m,"control-float"))*v=UINT32_MAX;
 if(mode(m,"ready-timeout") && m->control_stores==1)*v=1;
 if(mode(m,"ready-lost") && m->control_stores==1)*v=0;
 if(mode(m,"cancel-timeout") && m->control_stores==2)*v=2;
 if(mode(m,"cancel-foreign") && m->control_stores==2)*v=1;
 return true;
}
static bool rdomain(void *p,uint32_t *v) {
 struct reset_model *m=p;if(!read_ok(m))return false;*v=m->domain;
 if(mode(m,"initial-domain"))*v=8;
 if(mode(m,"domain-foreign") || mode(m,"domain-float"))*v=mode(m,"domain-float")?UINT32_MAX:2;
 if(mode(m,"reset-timeout") && m->domain_stores==m->point)*v=8;
 if(mode(m,"late-domain") && m->domain_stores)*v=2;
 if(mode(m,"delayed") && m->domain_stores && m->pauses<3)*v=8;
 return true;
}
static bool rgroup(void *p,uint32_t w[8]) {
 struct reset_model *m=p;++m->groups;if(!read_ok(m))return false;
 memset(w,0,8*sizeof(*w));w[4]=1;w[6]=512;
 if(mode(m,"busy"))w[0]=8;
 if(mode(m,"post-drift") && m->groups==2)w[2]=4096;
 if(mode(m,"return-deadline") && m->groups==2)m->gt.t+=20000;
 return true;
}
static enum intel_gt_write rcontrol_write(void *p,uint32_t value) {
 struct reset_model *m=p;
 assert(m->control_stores<2 && value==(m->control_stores?0x10000u:0x10001u));
 unsigned n=++m->control_stores;
 if(mode(m,"control-none") && n==m->point)return GT_WRITE_NONE;
 m->ctl=(n==1?3:0)|(mode(m,"mask")?0x10000u:0);
 return mode(m,"control-uncertain") && n==m->point?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static enum intel_gt_write rdomain_write(void *p,uint32_t value) {
 struct reset_model *m=p;
 assert(value==8 && m->domain_stores<2 && m->control_stores==1);
 unsigned n=++m->domain_stores;
 assert(m->parent->engine_epoch==m->start_epoch+1);
 if(mode(m,"domain-none") && n==m->point)return GT_WRITE_NONE;
 m->domain=0;m->ctl=mode(m,"mask")?0x10000:0;
 return mode(m,"domain-uncertain") && n==m->point?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static void rpause(void *p) {
 struct reset_model *m=p;++m->pauses;
 if(mode(m,"settle-timeout") && m->domain_stores==2)m->gt.t+=10001;
}
static void parent_pause(void *p) {
 struct reset_model *m=p;
 if(mode(m,"return-deadline") && m->groups==2 && ++m->closing==2)m->gt.t+=90000;
}
static struct intel_gt_access *changing_parent;
static bool changing_guard(void *p) {++changing_parent->engine_epoch;return obs_guard(p);}
static bool changing_gt_guard(void *p) {
 struct reset_model *m=p;
 if(m->gt.groups==10)++changing_parent->engine_epoch;
 return guard(p);
}
int main(int argc,char **argv) {
 struct reset_model m={.gt={.t=1000,.mode="normal"},.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
 struct display_access_token token;assert(display_access_acquire(18,&token));struct intel_gt_access g={0};m.parent=&g;
 assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m,guard,read_gt,write_gt,now,parent_pause,NULL}));
 if(mode(&m,"nonzero-epoch"))g.engine_epoch=m.start_epoch=17;
 struct intel_completion_result c={0};assert(intel_completion_observe(&c,&g,(struct intel_completion_io){&m.gt,obs_guard,group,context,obs_now}));
 if(mode(&m,"epoch-during-census")) {
  changing_parent=&g;struct intel_completion_result bad={0};
  assert(!intel_completion_observe(&bad,&g,(struct intel_completion_io){&m.gt,changing_guard,group,context,obs_now}));
  assert(bad.reason==3 && !bad.complete && !bad.groups);
  assert(intel_gt_access_close(&g) && display_access_release(&token,true));puts("epoch during census PASS");return 0;
 }
 if(mode(&m,"epoch-final-census")) {
  changing_parent=&g;g.io.guard=changing_gt_guard;m.gt.groups=m.gt.contexts=0;
  struct intel_completion_result bad={0};
  assert(!intel_completion_observe(&bad,&g,(struct intel_completion_io){&m.gt,obs_guard,group,context,obs_now}));
  assert(!bad.complete && bad.reason==3 && bad.groups==10 && g.engine_epoch!=bad.engine_epoch);
  assert(intel_gt_access_close(&g) && display_access_release(&token,true));puts("epoch during final check PASS");return 0;
 }
 struct intel_gt_borrow other={0};
 if(mode(&m,"other-borrow"))assert(intel_gt_access_borrow(&g,&other));
 if(mode(&m,"stale-epoch"))g.engine_epoch=1;
 if(mode(&m,"overflow"))c.engine_epoch=g.engine_epoch=UINT32_MAX;
 if(mode(&m,"active"))c.enabled=16;
 if(mode(&m,"pending"))c.pending=1;
 if(mode(&m,"flip"))c.flip_wait=1;
 if(mode(&m,"tlb"))c.tlb_pending=1;
 if(mode(&m,"stale-parent"))c.parent=0;
 if(mode(&m,"census-drift"))c.words[1][4][2]=4096;
 if(mode(&m,"context-drift"))c.context[1][5]=1;
 struct intel_completion_result copy=c;
 struct intel_bcs_reset_result r={0};
 struct intel_bcs_reset_io io={&m,rguard,rcontrol,rdomain,rgroup,rcontrol_write,rdomain_write,rnow,rpause};
 if(mode(&m,"epoch-admission") || mode(&m,"epoch-ready-admission")) {
  changing_parent=&g;g.io.guard=changing_gt_guard;
 }
 if(mode(&m,"epoch-ready-admission")) {
  struct intel_reset_ready_result bad={0};
  assert(!intel_reset_ready(&bad,&g,&c,true,(struct intel_reset_ready_io){&m,rguard,rcontrol,rgroup,rcontrol_write,rnow,rpause}));
  assert(bad.reason==IRR_INPUT && !m.control_stores && !bad.borrow.active);
  assert(intel_gt_access_close(&g) && display_access_release(&token,true));puts("epoch during readiness admission PASS");return 0;
 }
 if(mode(&m,"missing-io"))io.domain_write=0;
 bool qualified=argc>3 && !strcmp(argv[3],"shutdown");
 bool ok=(qualified?intel_bcs_reset_after_shutdown:intel_bcs_reset)(&r,&g,mode(&m,"copy")?&copy:&c,!mode(&m,"policy"),io);
 if(qualified && ok)assert(r.shutdown.object==&r.shutdown && r.shutdown.entry_disabled && r.shutdown.disabled && !r.shutdown.writes && !r.shutdown.attempts && r.shutdown.samples==4 && r.shutdown.reads==32 && r.begin_us<r.shutdown.begin_us && r.shutdown.end_us<r.settle_begin_us);
 bool expected=mode(&m,"normal") || mode(&m,"mask") || mode(&m,"delayed") || mode(&m,"nonzero-epoch") || mode(&m,"fresh-reset") ||
  (mode(&m,"guard") && m.point>m.guards) || (mode(&m,"read") && m.point>m.reads);
 assert(ok==expected);
 if(mode(&m,"detached-before") || mode(&m,"extra-dependent"))assert(!m.control_stores && !m.domain_stores && r.uncertain && r.borrow.active);
 if(mode(&m,"detached-after"))assert(m.control_stores==1 && !m.domain_stores && r.uncertain && r.borrow.active);
 if(ok) {
  assert(r.complete && r.admitted && r.ready && r.cancelled && r.blocked_close && !r.uncertain && !r.borrow.active);
  assert(r.writes==2 && r.reset_writes==2 && r.reset_acks==2 && r.reset_attempts==2 && r.attempts==2);
  assert(r.samples==2 && r.controls_read==4 && r.domains_read==4 && r.reads==r.polls+20);
  assert(r.settle_end_us-r.settle_begin_us>=50 && r.epoch_before==m.start_epoch && r.epoch_after==g.engine_epoch && g.engine_epoch==m.start_epoch+1);
  assert(r.end_us-r.begin_us<=100000 && g.borrows==0);
  unsigned writes=m.control_stores+m.domain_stores;
  struct intel_bcs_reset_result stale={0};assert(!intel_bcs_reset(&stale,&g,&c,true,io) && stale.reason==IBR_INPUT);
  struct intel_reset_ready_result stale_ready={0};
  assert(!intel_reset_ready(&stale_ready,&g,&c,true,(struct intel_reset_ready_io){&m,rguard,rcontrol,rgroup,rcontrol_write,rnow,rpause}));
  assert(writes==m.control_stores+m.domain_stores);
  struct intel_completion_result fresh={0};assert(intel_completion_observe(&fresh,&g,(struct intel_completion_io){&m.gt,obs_guard,group,context,obs_now}));
  assert(fresh.complete && fresh.engine_epoch==m.start_epoch+1);
  if(mode(&m,"fresh-reset")) {
   m.start_epoch=g.engine_epoch;m.guards=m.reads=m.control_stores=m.domain_stores=m.groups=m.pauses=0;
   struct intel_bcs_reset_result second={0};
   assert(intel_bcs_reset(&second,&g,&fresh,true,io));
   assert(second.complete && second.epoch_before==1 && second.epoch_after==2 && g.engine_epoch==2 && !g.borrows);
  }
 }
 if(!ok && (r.writes || (m.ctl&3) || r.reset_writes) && !mode(&m,"return-deadline"))assert(r.uncertain && r.borrow.active);
 assert(m.control_stores<=2 && m.domain_stores<=2 && r.polls<=4*256);
 if(m.domain_stores)assert(g.engine_epoch!=0);
 if(mode(&m,"late-domain"))assert(r.reason==IBR_DRIFT && r.reset_writes==1 && r.polls==1);
 struct intel_bcs_reset_result copied=r;unsigned calls=m.reads+m.control_stores+m.domain_stores;
 assert(!intel_bcs_reset(&r,&g,&c,true,io) && !intel_bcs_reset(&copied,&g,&c,true,io));
 assert(calls==m.reads+m.control_stores+m.domain_stores);
 if(mode(&m,"return-deadline"))assert(!r.complete && r.reason==IBR_CLOCK && !r.uncertain && !r.borrow.active && r.end_us-r.begin_us>100000);
 if(r.uncertain || r.borrow.active) {
  assert(g.borrows==(mode(&m,"extra-dependent")?2u:1u) && g.pinned && !intel_gt_access_close(&g));
  assert(!display_access_release(&token,true));
 } else {
  if(other.active)assert(intel_gt_access_return(&other));
  if(g.state==GT_ACCESS_AWAKE)assert(intel_gt_access_close(&g));
  if(!g.pinned)assert(display_access_release(&token,true));
 }
 printf("BCS reset protocol %s PASS guards=%u reads=%u; host model only, no memory retirement\n",m.mode,m.guards,m.reads);
 return 0;
}
