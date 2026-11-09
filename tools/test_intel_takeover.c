#define main prior_completion_fixture_main
#include "test_intel_completion_audit.c"
#undef main
#include <srvros/intel_takeover_preflight.h>
#include <stdlib.h>
struct takeover_model {
    struct model gt;
    struct intel_gt_access *parent;
    struct intel_completion_result *census;
    struct intel_takeover_result *result;
    struct intel_gt_borrow copy;
    const char *mode;
    unsigned point,guards,reads,samples,clocks;
    uint64_t frozen,offset;
};
static bool testmode(struct takeover_model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t takeover_now(void *p) {
    struct takeover_model *m=p;m->clocks++;
    uint64_t t=now(&m->gt);
    if(!m->frozen)m->frozen=t;
    if(testmode(m,"zero") && m->clocks==m->point)return 0;
    if(testmode(m,"frozen"))return m->frozen;
    if(testmode(m,"backward") && m->clocks==m->point)return m->frozen-1;
    if(testmode(m,"end-zero") && m->result->returned)return 0;
    if(testmode(m,"end-epoch") && m->result->returned)m->parent->engine_epoch++;
    if(testmode(m,"end-deadline") && m->result->returned)m->offset=100001;
    return t+m->offset;
}
static bool takeover_guard(void *p) {
    struct takeover_model *m=p;m->guards++;
    if(testmode(m,"guard-epoch"))m->parent->engine_epoch++;
    return guard(&m->gt) && !(testmode(m,"guard") && m->guards==m->point);
}
static bool takeover_sample(void *p,uint32_t out[6],unsigned *count) {
    struct takeover_model *m=p;m->samples++;*count=0;
    assert(m->parent->borrows==1 && !intel_gt_access_close(m->parent));
    for(unsigned i=0;i<6;i++) {
        ++m->reads;
        if(testmode(m,"read") && m->reads==m->point)return false;
        out[i]=0;
        if(testmode(m,"busy") && i==m->point)out[i]=3;
        if(testmode(m,"reserved") && i==m->point)out[i]=4;
        if(testmode(m,"mask") && i<5)out[i]=0xffff0000;
        if(testmode(m,"domain") && i==5)out[i]=8;
        if(testmode(m,"domain-reserved") && i==5)out[i]=32;
        if(testmode(m,"changed") && m->samples==2 && i==m->point)out[i]=1;
        if(testmode(m,"float") && m->reads==m->point)out[i]=UINT32_MAX;
        ++*count;
        if(out[i]==UINT32_MAX)return false;
    }
    if(testmode(m,"bad-count"))*count=7;
    if(testmode(m,"short-count"))*count=5;
    if(testmode(m,"epoch"))m->parent->engine_epoch++;
    if(testmode(m,"identity"))m->parent->object=NULL;
    if(testmode(m,"unpin"))m->parent->pinned=false;
    if(testmode(m,"detached"))m->parent->dependents=NULL;
    if(testmode(m,"extra"))m->parent->borrows=2;
    if(testmode(m,"borrow-copy")){m->copy=m->result->borrow;m->parent->dependents=&m->copy;}
    if(testmode(m,"census"))m->census->complete=false;
    if(testmode(m,"deadline"))m->gt.t+=100001;
    if(testmode(m,"observer-deadline"))m->offset=100001;
    if(testmode(m,"return-foreign") && m->samples==2)m->gt.request|=2;
    return true;
}
#ifndef INTEL_TAKEOVER_FIXTURE_ONLY
int main(int argc,char **argv) {
    struct takeover_model m={.gt={.t=1000,.mode="normal"},.mode=argc>1?argv[1]:"normal",.point=argc>2?(unsigned)atoi(argv[2]):1};
    struct display_access_token t;assert(display_access_acquire(18,&t));
    struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&t,7,(struct intel_gt_io){&m.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));m.parent=&g;
    struct intel_completion_result pre={0};batch_parent=&g;
    if(testmode(&m,"active") || testmode(&m,"pending") || testmode(&m,"flip-wait") || testmode(&m,"tlb-pending"))m.gt.mode=m.mode;
    assert(intel_completion_observe(&pre,&g,(struct intel_completion_io){&m.gt,obs_guard,group,context,obs_now,batch_sample}));m.gt.mode="normal";m.census=&pre;
    struct intel_completion_result copied=pre;
    if(testmode(&m,"geometry"))pre.words[0][4][2]=pre.words[1][4][2]=4096;
    if(testmode(&m,"pre-batches"))pre.batches=0;
    if(testmode(&m,"pre-count"))pre.reads--;
    if(testmode(&m,"pre-epoch"))g.engine_epoch++;
    if(testmode(&m,"pre-time"))pre.end_us=UINT64_MAX;
    struct intel_gt_borrow other={0};
    if(testmode(&m,"other"))assert(intel_gt_access_borrow(&g,&other));
    struct intel_takeover_result r={0};m.result=&r;
    bool ok=intel_takeover_observe(&r,&g,testmode(&m,"copy")?&copied:&pre,(struct intel_takeover_io){&m,takeover_guard,takeover_sample,takeover_now});
    bool expected=testmode(&m,"geometry") || testmode(&m,"normal") || testmode(&m,"busy") || testmode(&m,"reserved") || testmode(&m,"domain") || testmode(&m,"domain-reserved") || testmode(&m,"mask") || testmode(&m,"active") || testmode(&m,"pending") || testmode(&m,"flip-wait") || testmode(&m,"tlb-pending");
    assert(ok==expected && r.complete==expected && m.gt.writes==2);
    if(expected) {
        assert(r.attempts==2 && r.samples==2 && r.reads==12 && r.stable && r.blocked && r.returned && !r.borrow.active && !g.borrows);
        assert(r.quiescent==(testmode(&m,"normal") || testmode(&m,"mask")));
        assert(r.busy==(testmode(&m,"busy")?1u<<m.point:0));
        assert(r.reserved==(testmode(&m,"reserved")?1u<<m.point:testmode(&m,"domain-reserved")?32u:0));
    } else assert(r.reason && !r.quiescent);
    if(testmode(&m,"copy") || !strncmp(m.mode,"pre-",4) || testmode(&m,"other"))assert(!r.attempts && !r.borrow.active);
    if(testmode(&m,"guard-epoch"))assert(!r.attempts && !m.reads);
    if(testmode(&m,"observer-deadline"))assert(r.attempts==1 && r.reads==6);
    if(testmode(&m,"bad-count"))assert(r.reason&INTEL_TAKEOVER_COUNT && !r.reads && !r.counts[0]);
    if(testmode(&m,"observer-deadline") || testmode(&m,"end-deadline"))assert(r.reason&INTEL_TAKEOVER_CLOCK && r.returned);
    if(testmode(&m,"read"))assert(r.reads==m.point-1 && r.reason&INTEL_TAKEOVER_READ);
    if(testmode(&m,"float"))assert(r.reads==m.point && r.reason&INTEL_TAKEOVER_FLOAT);
    if(testmode(&m,"epoch") || testmode(&m,"identity") || testmode(&m,"unpin") || testmode(&m,"detached") || testmode(&m,"extra") || testmode(&m,"borrow-copy") || testmode(&m,"census") || testmode(&m,"guard-epoch"))assert(r.borrow.active && r.reason&INTEL_TAKEOVER_PARENT);
    unsigned reads=m.reads;struct intel_takeover_result copy=r;
    assert(!intel_takeover_observe(&r,&g,&pre,(struct intel_takeover_io){0}) && !intel_takeover_observe(&copy,&g,&pre,(struct intel_takeover_io){0}) && reads==m.reads);
    if(other.active)assert(intel_gt_access_return(&other));
    if(!r.borrow.active && g.state==GT_ACCESS_AWAKE && g.object==&g) {
        assert(intel_gt_access_close(&g) && display_access_release(&t,true));
    } else assert(!display_access_allowed());
    printf("takeover core PASS attempts=%u reads=%u guards=%u clocks=%u quiescent=%u retained=%u\n",r.attempts,r.reads,m.guards,m.clocks,r.quiescent,r.borrow.active);
}

#endif
