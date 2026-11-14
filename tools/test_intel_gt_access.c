#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/intel_gt_access.h>
/* Fresh simulated boots only; production has no gate reset entry. */
#include "../kernel/src/display_access.c"
static struct intel_gt_access g;
static struct display_access_token token;
static uint32_t request,ack,thread,trace[8];
static unsigned calls,fail_at,write_calls,pause_calls,ack_wait,guard_calls,snapshot_fault;
static bool never_ack,never_c0,frozen,backward,unissued,uncertain;
static uint64_t ticks;
static bool step(void){return ++calls!=fail_at;}
static bool guard(void *p){assert(p==&g);++guard_calls;return step() && display_access_held(&token);}
static bool read_reg(void *p,uint32_t reg,uint32_t *v) {
    assert(p==&g);if(!step())return false;
    assert(reg==INTEL_GT_REQUEST || reg==INTEL_GT_ACK || reg==INTEL_GT_THREAD);
    *v=reg==INTEL_GT_REQUEST?request:reg==INTEL_GT_ACK?ack:thread;return true;
}
static bool snapshot(void *p,uint32_t *r,uint32_t *a,uint32_t *t) {
    if(!guard(p) || !read_reg(p,INTEL_GT_REQUEST,r) || !read_reg(p,INTEL_GT_ACK,a) || !read_reg(p,INTEL_GT_THREAD,t))return false;
    if(snapshot_fault==1)return false;
    if(snapshot_fault==2)*r=UINT32_MAX;
    if(snapshot_fault==3)*a=UINT32_MAX;
    if(snapshot_fault==4)*t=UINT32_MAX;
    if(snapshot_fault==5)*r|=2;
    if(snapshot_fault==6)*a|=2;
    if(snapshot_fault==7)*t=1;
    if(snapshot_fault==8)ticks+=300001;
    if(snapshot_fault==9)ticks=0;
    return true;
}
static enum intel_gt_write write_reg(void *p,uint32_t reg,uint32_t value) {
    assert(p==&g && reg==INTEL_GT_REQUEST && !display_access_allowed());
    assert(write_calls<8);trace[write_calls++]=value;
    if(unissued)return GT_WRITE_NONE;
    request=(value&0xffff0000u)|((request&0xffffu)&~(value>>16))|(value&65535);ack_wait=2;
    if(!step() || uncertain)return GT_WRITE_UNCERTAIN;
    return GT_WRITE_DONE;
}
static uint64_t now(void *p){assert(p==&g);if(backward)return ticks--;if(!frozen)ticks+=100;return ticks;}
static void pause_io(void *p) {
    assert(p==&g);pause_calls++;
    if(!never_ack && ack_wait && !--ack_wait)ack=request&0xffffu;
    if(ack==1 && !never_c0)thread=0;
}
static struct intel_gt_io io(void){return (struct intel_gt_io){&g,guard,read_reg,write_reg,now,pause_io,NULL};}
static void fresh(void) {
    state=epoch=owner=pins=0;fault=false;
    memset(&g,0,sizeof(g));request=ack=0;thread=7;memset(trace,0,sizeof(trace));
    calls=fail_at=write_calls=pause_calls=ack_wait=guard_calls=snapshot_fault=0;never_ack=never_c0=frozen=backward=unissued=uncertain=false;ticks=1000;
    assert(display_access_acquire(33,&token));
}
static void retained(void) {
    assert(g.state==GT_ACCESS_FAULT && g.pinned && !display_access_allowed());
    unsigned writes=write_calls;assert(!intel_gt_access_check(&g) && !intel_gt_access_close(&g));
    assert(write_calls==writes && !display_access_release(&token,true));
}
int main(void) {
    fresh();assert(intel_gt_access_open(&g,&token,7,io()) && g.state==GT_ACCESS_AWAKE);
    struct intel_gt_access copy=g;unsigned call_count=calls;
    assert(!intel_gt_access_check(&copy) && !intel_gt_access_close(&copy) && calls==call_count);
    assert((request&0xffffu)==1 && ack==1 && !thread && g.writes==2 && write_calls==2 && g.pinned);
    assert(!display_access_release(&token,true) && intel_gt_access_check(&g));
    assert(intel_gt_access_close(&g) && !g.pinned && !(request&0xffffu) && !ack && g.writes==3);
    assert(trace[0]==0xffff0000u && trace[1]==0x10001 && trace[2]==0x10000);
    unsigned total=calls;assert(!intel_gt_access_close(&g) && display_access_release(&token,true));
    /* Optional snapshot still has core pre/post and provider admission. */
    fresh();assert(intel_gt_access_open(&g,&token,7,io()));g.io.snapshot=snapshot;
    unsigned guards_before=guard_calls;assert(intel_gt_access_check(&g));
    assert(guard_calls-guards_before==3 && g.snapshots==1 && !g.clock_cause);
    assert(intel_gt_access_close(&g) && g.snapshots==2 && write_calls==3 && display_access_release(&token,true));
    fresh();
    struct intel_gt_io grouped=io();grouped.snapshot=snapshot;
    assert(intel_gt_access_open_with_guarded_snapshot(&g,&token,7,grouped));
    struct intel_gt_borrow borrow={0};
    assert(intel_gt_access_borrow(&g,&borrow));
    guards_before=guard_calls;
    assert(intel_gt_access_return(&borrow) && !borrow.active && !g.borrows && !g.dependents);
    assert(guard_calls-guards_before==2 && write_calls==2);
    assert(intel_gt_access_close(&g) && display_access_release(&token,true));
    /* Both the provider admission and the independent exit can block return. */
    for(unsigned cut=1;cut<=5;cut+=4) {
        fresh();
        assert(intel_gt_access_open_with_guarded_snapshot(&g,&token,7,grouped));
        borrow=(struct intel_gt_borrow){0};
        assert(intel_gt_access_borrow(&g,&borrow));
        fail_at=calls+cut;
        assert(!intel_gt_access_return(&borrow));
        assert(calls==fail_at && borrow.active && g.dependents==&borrow && g.borrows==1);
        retained();
    }
    for(unsigned f=1;f<=9;f++){
        fresh();assert(intel_gt_access_open(&g,&token,7,io()));g.io.snapshot=snapshot;snapshot_fault=f;
        assert(!intel_gt_access_check(&g) && g.snapshots==1 && write_calls==2);
        if(f==8)assert(g.clock_cause==(GT_CLOCK_PHASE|GT_CLOCK_TOTAL) && g.clock_sample>g.begin+300000);
        if(f==9)assert(g.clock_cause==GT_CLOCK_BACKWARD && g.clock_sample<g.clock_previous);
        uint32_t why=g.clock_cause;uint64_t failed=g.clock_sample,previous=g.clock_previous,phase=g.clock_phase;retained();
        assert(g.clock_cause==why && g.clock_sample==failed && g.clock_previous==previous && g.clock_phase==phase);
    }
    for(unsigned cut=1;cut<=6;cut++){
        fresh();assert(intel_gt_access_open(&g,&token,7,io()));g.io.snapshot=snapshot;fail_at=calls+cut;
        assert(!intel_gt_access_check(&g) && write_calls==2);retained();
    }
    for(unsigned upper=0;upper<16;upper++){fresh();request=1u<<(16+upper);assert(intel_gt_access_open(&g,&token,7,io()));assert(intel_gt_access_close(&g) && display_access_release(&token,true));}
    for(unsigned bit=1;bit<=4;bit<<=1){fresh();assert(!intel_gt_access_open(&g,&token,7^bit,io()) && g.reason==bit && !calls && !write_calls && !g.pinned);assert(display_access_release(&token,true));}
    fresh();struct display_access_token wrong=token;wrong.epoch++;assert(!intel_gt_access_open(&g,&wrong,7,io()) && g.reason==GT_ACCESS_EXCLUSION && !calls);
    fresh();assert(!intel_gt_access_open(&g,&token,15,io()) && (g.reason&GT_ACCESS_INPUT) && !calls);
    for(unsigned v=0;v<4;v++) {fresh();request=v&1?1:0;ack=v&2?1:0;if(!v)request=4;assert(!intel_gt_access_open(&g,&token,7,io()) && g.reason==GT_ACCESS_FOREIGN && !write_calls && !g.pinned);assert(display_access_release(&token,true));}
    fresh();unissued=true;assert(!intel_gt_access_open(&g,&token,7,io()) && !g.pinned && g.state==GT_ACCESS_CLOSED && g.write_attempts==1 && !g.writes);assert(display_access_release(&token,true));
    fresh();uncertain=true;assert(!intel_gt_access_open(&g,&token,7,io()) && write_calls==1);retained();
    fresh();never_ack=true;assert(!intel_gt_access_open(&g,&token,7,io()) && (g.reason&GT_ACCESS_CLOCK));retained();
    fresh();never_c0=true;assert(!intel_gt_access_open(&g,&token,7,io()) && (g.reason&GT_ACCESS_CLOCK));retained();
    fresh();frozen=never_ack=true;assert(!intel_gt_access_open(&g,&token,7,io()) && (g.reason&GT_ACCESS_TIMEOUT) && g.polls<=GT_ACCESS_POLL_CAP*2);retained();
    fresh();frozen=true;assert(!intel_gt_access_open(&g,&token,7,io()) && (g.reason&GT_ACCESS_CLOCK));retained();
    fresh();assert(intel_gt_access_open(&g,&token,7,io()));frozen=true;assert(!intel_gt_access_close(&g) && (g.reason&GT_ACCESS_CLOCK));retained();
    fresh();backward=true;assert(!intel_gt_access_open(&g,&token,7,io()) && (g.reason&GT_ACCESS_CLOCK) && !g.pinned && !write_calls);
    fresh();frozen=true;ticks=0;assert(!intel_gt_access_open(&g,&token,7,io()) && g.reason==GT_ACCESS_CLOCK && !calls && !write_calls);
    fresh();frozen=true;ticks=UINT64_MAX;assert(!intel_gt_access_open(&g,&token,7,io()) && g.reason==GT_ACCESS_CLOCK && !calls && !write_calls);
    fresh();assert(intel_gt_access_open(&g,&token,7,io()));ticks+=100001;assert(!intel_gt_access_check(&g) && (g.reason&GT_ACCESS_CLOCK));assert(write_calls==2);retained();
    fresh();assert(intel_gt_access_open(&g,&token,7,io()));
    for(unsigned i=0;i<3;i++){ticks+=90000;assert(intel_gt_access_check(&g));}
    ticks+=90000;assert(!intel_gt_access_check(&g) && (g.reason&GT_ACCESS_CLOCK));retained();
    for(unsigned v=0;v<3;v++){fresh();assert(intel_gt_access_open(&g,&token,7,io()));if(v==0)request|=2;if(v==1)ack|=2;if(v==2){thread=1;never_c0=true;}assert(!intel_gt_access_check(&g) && (g.reason&GT_ACCESS_FOREIGN));assert(write_calls==2);retained();}
    for(unsigned at=1;at<=total;at++) {
        fresh();fail_at=at;
        bool ok=intel_gt_access_open(&g,&token,7,io());
        if(ok)ok=intel_gt_access_check(&g);
        if(ok)ok=intel_gt_access_close(&g);
        assert(!ok);
        if(g.pinned)retained();else assert(!g.writes && display_access_release(&token,true));
    }
    printf("intel-gt-access: PASS masked protocol/C0, missing grants, foreign requests, finite clocks/polls and %u I/O boundaries; no native authority\n",total);
}
