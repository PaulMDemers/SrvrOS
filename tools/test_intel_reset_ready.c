#define main prior_completion_fixture_main
#include "test_intel_completion_audit.c"
#undef main
#include <srvros/intel_reset_ready.h>
struct ready_model {struct model m;struct intel_gt_access *parent;const char *mode;uint32_t ctl;unsigned writes,reads,phase,groups,guards,closing;};
static bool mode(struct ready_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t rnow(void *p) {
 struct ready_model *m=p;
 if(mode(m,"zero"))return 0;
 if(mode(m,"frozen"))return 2000;
 if(mode(m,"backward"))return 900-m->reads;
 if(mode(m,"clock-jump") && m->writes)m->m.t+=100001;
 return now(&m->m);
}
static bool rguard(void *p) {
 struct ready_model *m=p;++m->guards;
 if(mode(m,"foreign") && m->writes)m->m.request|=2;
 if(mode(m,"late-guard") && m->writes)return false;
 return guard(&m->m) && (m->m.request&65535)==1 && m->m.ack==1 && !mode(m,"guard");
}
static bool rcontrol(void *p,uint32_t *v) {
 struct ready_model *m=p;++m->reads;++m->phase;*v=m->ctl;
 if(mode(m,"initial"))*v=1;
 if(mode(m,"reserved"))*v=4;
 if(mode(m,"float"))*v=UINT32_MAX;
 if(mode(m,"read"))return false;
 if(mode(m,"late-read") && m->writes==2)return false;
 if((mode(m,"timeout") || mode(m,"deadline")) && m->writes==1)*v=1;
 if(mode(m,"cancel-timeout") && m->writes==2)*v=2;
 if(mode(m,"request-lost") && m->writes==1)*v=0;
 if(mode(m,"cancel-foreign") && m->writes==2)*v=1;
 if(mode(m,"delay") && m->writes==1 && m->phase<3)*v=1;
 return true;
}
static bool rgroup(void *p,uint32_t w[8]) {
 struct ready_model *m=p;++m->groups;if(mode(m,"return-deadline") && m->groups==1)m->m.t+=20000;memset(w,0,8*sizeof(*w));w[4]=1;w[6]=512;
 if(mode(m,"busy"))w[0]=8;
 if(mode(m,"drift") && m->groups==2)w[7]=1;
 return !mode(m,"group-read");
}
static enum intel_gt_write rwrite(void *p,uint32_t value) {
 struct ready_model *m=p;assert(value==(m->writes?0x10000u:0x10001u) && m->writes<2);
 if(mode(m,"none") || (mode(m,"cancel-none") && m->writes==1))return GT_WRITE_NONE;
 m->writes++;m->phase=0;m->ctl=m->writes==1?3:0;
 return mode(m,"uncertain")?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static void rpause(void *p){struct ready_model *m=p;if(mode(m,"deadline"))m->m.t+=10001;}
static void parent_pause(void *p) {
 struct ready_model *m=p;
 if(mode(m,"return-deadline") && m->groups==2 && ++m->closing==2)m->m.t+=90000;
}
int main(int argc,char **argv) {
 struct ready_model m={.m={.t=1000,.mode="normal"},.mode=argc>1?argv[1]:"normal"};
 struct display_access_token token;assert(display_access_acquire(18,&token));struct intel_gt_access g={0};m.parent=&g;
 assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m.m,guard,read_gt,write_gt,now,parent_pause}));
 struct intel_completion_result c={0};assert(intel_completion_observe(&c,&g,(struct intel_completion_io){&m.m,obs_guard,group,context,obs_now}));
 if(mode(&m,"borrow")) {
  struct intel_gt_borrow a={0},b={0};assert(intel_gt_access_borrow(&g,&a));assert(intel_gt_access_borrow(&g,&b));
  struct intel_gt_borrow copy=a;assert(!intel_gt_access_return(&copy));
  struct intel_gt_access wrong=g;wrong.object=&wrong;
  a.parent=&wrong;assert(!intel_gt_access_return(&a));a.parent=&g;
  struct intel_gt_borrow forged={.object=&forged,.parent=&g,.active=true};assert(!intel_gt_access_return(&forged));assert(!intel_gt_access_close(&g) && m.m.writes==2);
  assert(intel_gt_access_return(&a));assert(!intel_gt_access_return(&a));assert(!intel_gt_access_borrow(&g,&a));
  assert(!intel_gt_access_close(&g));assert(intel_gt_access_return(&b));assert(!g.borrows);assert(intel_gt_access_close(&g));assert(display_access_release(&token,true));return 0;
 }
 struct intel_reset_ready_result r={0};struct intel_completion_result copied=c;
 if(mode(&m,"stale"))c.parent=0;
 if(mode(&m,"active"))c.enabled=16;
 if(mode(&m,"pending"))c.pending=1;
 bool ok=intel_reset_ready(&r,&g,mode(&m,"copy")?&copied:&c,!mode(&m,"policy"),(struct intel_reset_ready_io){&m,rguard,rcontrol,rgroup,rwrite,rnow,rpause});
 bool expected=mode(&m,"normal") || mode(&m,"delay");assert(ok==expected);
 if(m.writes && !ok && !mode(&m,"return-deadline"))assert(r.uncertain && r.borrow.active && g.borrows==1 && !intel_gt_access_close(&g));
 struct intel_reset_ready_result rcopy=r;assert(!intel_reset_ready(&r,&g,&c,true,(struct intel_reset_ready_io){0}));assert(!intel_reset_ready(&rcopy,&g,&c,true,(struct intel_reset_ready_io){0}));
 if(ok){assert(r.complete && r.ready && r.cancelled && r.blocked_close && r.writes==2 && r.samples==2 && r.controls_read==4 && r.reads==r.polls+18 && !r.uncertain && !g.borrows);}
 if(mode(&m,"return-deadline"))assert(!ok && r.reason==IRR_CLOCK && !r.uncertain && !r.borrow.active && r.end_us-r.begin_us>100000);
 if(r.uncertain || r.borrow.active) {assert(g.borrows==1 && g.pinned && !intel_gt_access_close(&g) && m.m.writes==2);assert(!display_access_release(&token,true));}
 else if(g.state==GT_ACCESS_AWAKE){assert(!g.borrows && intel_gt_access_close(&g));assert(display_access_release(&token,true));}
 puts("reset ready PASS; no reset/drain/cache authority");return 0;
}
