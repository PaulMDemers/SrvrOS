#include <srvros/intel_blitter.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/display_access.c"
struct model {uint64_t t;uint32_t request,ack,ctl;unsigned writes,guards,bclock;const char *mode;};
static uint64_t now(void *p){return ++((struct model *)p)->t;}
static bool guard(void *p){(void)p;return !display_access_allowed();}
static bool read_gt(void *p,uint32_t reg,uint32_t *out){struct model *m=p;*out=reg==INTEL_GT_REQUEST?m->request:reg==INTEL_GT_ACK?m->ack:0;return true;}
static enum intel_gt_write write_gt(void *p,uint32_t reg,uint32_t v){struct model *m=p;assert(reg==INTEL_GT_REQUEST);m->request=(m->request&~(v>>16))|(v&65535);m->ack=m->request&65535;return GT_WRITE_DONE;}
static void pause_gt(void *p){(void)p;}
static uint64_t bnow(void *p){struct model *m=p;m->bclock++;if(!strcmp(m->mode,"zero"))return 0;if(!strcmp(m->mode,"backward"))return 2001-m->bclock;return !strcmp(m->mode,"frozen")?2000:now(p);}
static bool bguard(void *p){struct model *m=p;m->guards++;if((!strcmp(m->mode,"deadline") && m->guards==3) || (!strcmp(m->mode,"post-deadline") && m->guards==4))m->t+=100001;return guard(p);}
static bool bread(void *p,uint32_t *out){struct model *m=p;uint32_t w[8]={0,0,0x3eef000,m->ctl,1,0,0x200,0};memcpy(out,w,sizeof(w));if(!strcmp(m->mode,"busy"))out[0]=8;return true;}
static enum intel_gt_write bwrite(void *p,uint32_t v){struct model *m=p;assert(v==0xf000 && m->ctl==0xf001);m->writes++;assert(m->writes==1);if(!strcmp(m->mode,"none"))return GT_WRITE_NONE;m->ctl=v;return !strcmp(m->mode,"uncertain")?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;}
int main(int argc,char **argv){
 const char *mode=argc>1?argv[1]:"success";struct model m={.t=1000,.ctl=0xf001,.mode=mode};
 struct display_access_token token;assert(display_access_acquire(17,&token));struct intel_gt_access g={0};
 assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m,guard,read_gt,write_gt,now,pause_gt}));
 struct intel_blitter_result b={0};struct intel_gt_access copy=g;
 bool ok=intel_blitter_disable(&b,!strcmp(mode,"copy")?&copy:&g,strcmp(mode,"no-handoff")!=0,(struct intel_blitter_io){&m,bguard,bread,bwrite,bnow});
 if(!strcmp(mode,"success")){assert(ok && b.disabled && b.reads==32 && b.samples==4 && b.writes==1 && !b.uncertain);}
 else {
  assert(!ok && !b.disabled);
  if(!strcmp(mode,"uncertain") || !strcmp(mode,"post-deadline"))assert(b.uncertain && m.writes==1);
  else if(!strcmp(mode,"none"))assert(!b.uncertain && m.writes==1 && !b.writes);
  else assert(!m.writes && !b.attempts);
 }
 unsigned issued=m.writes;bool was_uncertain=b.uncertain;struct intel_blitter_result copied=b;
 assert(!intel_blitter_disable(&b,&g,true,(struct intel_blitter_io){&m,bguard,bread,bwrite,bnow}));
 assert(!intel_blitter_disable(&copied,&g,true,(struct intel_blitter_io){&m,bguard,bread,bwrite,bnow}));
 assert(m.writes==issued && b.uncertain==was_uncertain);
 if(!b.uncertain && g.state==GT_ACCESS_AWAKE){bool closed=intel_gt_access_close(&g);if(!strcmp(mode,"deadline"))assert(!closed && g.pinned);else assert(closed);}
 puts("blitter-core: PASS explicit handoff, parent owner identity, exact idle eligibility, one store, deadline/frozen clock and retained uncertainty");
}
