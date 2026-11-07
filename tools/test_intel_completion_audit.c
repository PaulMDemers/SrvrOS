#include <srvros/intel_completion_audit.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/display_access.c"
struct model {uint64_t t;uint32_t request,ack;unsigned writes,groups,contexts,guards,oclock;const char *mode;};
static bool is(struct model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t now(void *p){return ++((struct model *)p)->t;}
static bool guard(void *p){(void)p;return !display_access_allowed();}
static bool read_gt(void *p,uint32_t reg,uint32_t *out){struct model *m=p;*out=reg==INTEL_GT_REQUEST?m->request:reg==INTEL_GT_ACK?m->ack:0;return true;}
static enum intel_gt_write write_gt(void *p,uint32_t reg,uint32_t v){struct model *m=p;assert(reg==INTEL_GT_REQUEST);m->writes++;m->request=(m->request&~(v>>16))|(v&65535);m->ack=m->request&65535;return GT_WRITE_DONE;}
static void pause_gt(void *p){(void)p;}
static uint64_t obs_now(void *p){struct model *m=p;m->oclock++;if(is(m,"clock-jump"))return now(p)+(m->oclock>1?100001:0);if(is(m,"zero"))return 0;if(is(m,"frozen"))return 2000;if(is(m,"backward"))return 2000-m->guards;return now(p);}
static bool obs_guard(void *p){struct model *m=p;m->guards++;if(is(m,"deadline"))m->t+=100001;if(is(m,"foreign"))m->request|=2;return guard(p) && !is(m,"guard") && !(is(m,"late-guard") && m->groups>5);}
static bool group(void *p,unsigned e,uint32_t w[8]) {
 struct model *m=p;m->groups++;assert(e<5);memset(w,0,8*sizeof(*w));w[4]=1;
 if(is(m,"active"))w[3]=1;
 if(is(m,"pending"))w[0]=8;
 if(is(m,"flip-wait"))w[6]=1u<<29;
 if(is(m,"tlb-pending"))w[7]=1;
 if(is(m,"tlb-reserved"))w[7]=2;
 if(is(m,"changed") && m->groups>5)w[2]=4096;
 if(is(m,"float"))w[7]=UINT32_MAX;
 return !is(m,"read");
}
static bool context(void *p,uint32_t w[6]) {
 struct model *m=p;m->contexts++;memset(w,0,6*sizeof(*w));w[0]=w[3]=512;
 if(is(m,"context-change") && m->contexts==2)w[1]=1;
 if(is(m,"context-float"))w[5]=UINT32_MAX;
 return !is(m,"context-read");
}
int main(int argc,char **argv){
 struct model m={.t=1000,.mode=argc>1?argv[1]:"normal"};struct display_access_token token;assert(display_access_acquire(18,&token));struct intel_gt_access g={0};
 assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m,guard,read_gt,write_gt,now,pause_gt}));
 struct intel_gt_access copied=g;struct intel_completion_result r={0};struct intel_completion_io io={&m,obs_guard,group,context,obs_now};
 bool ok=intel_completion_observe(&r,is(&m,"copy")?&copied:&g,io);
 bool expected=is(&m,"normal") || is(&m,"active") || is(&m,"pending") || is(&m,"flip-wait") || is(&m,"tlb-pending");assert(ok==expected);
 if(expected){assert(r.complete && r.stable && r.reads==92 && r.groups==10 && r.contexts==2 && r.samples==2 && r.parser_idle==3);assert(r.enabled==(is(&m,"active")?31u:0));assert(r.pending==(is(&m,"pending")?31u:0));assert(r.flip_wait==(is(&m,"flip-wait")?31u:0));assert(r.tlb_pending==(is(&m,"tlb-pending")?31u:0));}
 unsigned reads=m.groups+m.contexts;struct intel_completion_result copy=r;assert(!intel_completion_observe(&r,&g,io) && !intel_completion_observe(&copy,&g,io));assert(reads==m.groups+m.contexts && m.writes==2);
 if(g.state==GT_ACCESS_AWAKE){bool closed=intel_gt_access_close(&g);if(is(&m,"foreign") || is(&m,"deadline"))assert(!closed && g.pinned);else assert(closed);}
 puts("completion core PASS one-use/parent/deadline, raw active states; zero completion authority");return 0;
}
