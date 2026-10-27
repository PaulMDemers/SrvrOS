#include <srvros/mt76_rx_window.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum fault { NONE, NO_EVENT, FOREIGN, STALE, POST_FAIL, POLL_FAIL, LOST_OWNERSHIP,
    CLEANUP_LOST, STOP_FAIL, BAD_STATE, BAD_SLOT, BAD_DCI, BAD_DCS, BAD_DEQUEUE,
    BACKWARD, FROZEN, JUMP, BAD_RESIDUE, EVENT_DATA, CC26, CC27, CC28, CC_ERROR,
    ZERO, FULL, MALFORMED, UNSUPPORTED, OFF_CHANNEL, SHORT_DEADLINE, STOP_CURRENT, ZERO_ALL, INITIAL_LOST };
struct fixture {
    struct mt76_rx_window s;
    struct mt76_rx_window_plan p;
    struct mt76_rx_window_io io;
    enum fault fault;
    unsigned posts,stops,polls,owned,cleanup,fail_index,checks;
    uint64_t clock;
};
static void put16(uint8_t *p,unsigned n) {p[0]=n;p[1]=n>>8;}
static void put32(uint8_t *p,unsigned n) {put16(p,n);put16(p+2,n>>16);}
static void packet(uint8_t *p,unsigned index) {
    memset(p,0,4096);
    /* 36-byte RX prefix + 24-byte header + 12-byte fixed body + 9 IE bytes. */
    put16(p,80);put32(p+8,45u<<16);put16(p+36,0x80);
    memset(p+40,255,6);p[46]=p[52]=2;p[51]=p[57]=(uint8_t)(index+1);
    put16(p+68,100);put16(p+70,0x11);
    const uint8_t ies[]={0,0,1,2,0x82,0x84,3,1,1};memcpy(p+72,ies,sizeof ies);
}
static bool owned(void *ctx,bool cleanup) {
    struct fixture *f=ctx;f->checks++;
    if (cleanup) {
        assert(f->s.stopped && f->stops==1);f->cleanup++;
        return !(f->fault==CLEANUP_LOST && f->cleanup==f->fail_index+1);
    }
    f->owned++;
    return f->fault!=INITIAL_LOST && !(f->fault==LOST_OWNERSHIP && f->posts>f->fail_index);
}
static bool post(void *ctx,unsigned index,uint64_t trb,uint64_t physical) {
    struct fixture *f=ctx;
    assert(index==f->posts && f->posts<8 && !f->stops && !f->cleanup);
    assert(trb==f->p.ring+16*index && physical==f->p.physical[index]);
    assert(f->s.attempts==index+1);
    if(index)assert(f->s.item[index-1].completed);
    for(unsigned i=0;i<8;i++)assert(!f->s.item[i].inspected);
    f->posts++;
    return !(f->fault==POST_FAIL && index==f->fail_index);
}
static bool poll(void *ctx,struct mt76_rx_window_event *e,bool *present) {
    struct fixture *f=ctx;assert(f->posts && !f->stops && !f->cleanup);f->polls++;
    if(f->fault==POLL_FAIL && f->posts==f->fail_index+1)return false;
    *present=f->fault!=NO_EVENT && f->fault!=FROZEN && f->fault!=SHORT_DEADLINE;
    *e=(struct mt76_rx_window_event){.trb=f->p.ring+16*(f->posts-1),
        .slot=f->p.slot,.dci=9,.completion=13,.remaining=4096-84};
    if(f->fault==FOREIGN && f->polls%2) {
        switch(f->polls%6){case 1:e->slot++;break;case 3:e->dci++;break;default:e->trb+=16;break;}
    }
    if(f->fault==STALE && f->posts>1 && f->polls%2)e->trb-=16;
    if(f->fault==ZERO_ALL) {e->remaining=4096;return true;}
    if(f->posts!=f->fail_index+1)return true;
    switch(f->fault) {
    case BAD_RESIDUE:e->remaining=4097;break;
    case EVENT_DATA:e->event_data=true;break;
    case CC26:e->completion=26;e->remaining=0;break;
    case CC27:e->completion=27;e->remaining=0xffffff;break;
    case CC28:e->completion=28;break;
    case CC_ERROR:e->completion=6;break;
    case ZERO:e->completion=1;e->remaining=4096;break;
    case FULL:e->completion=1;e->remaining=0;break;
    default:break;
    }
    return true;
}
static bool stop(void *ctx,struct mt76_rx_window_stop *s) {
    struct fixture *f=ctx;assert(f->posts && !f->stops && !f->cleanup);
    for(unsigned i=0;i<8;i++)assert(!f->s.item[i].inspected);
    f->stops++;
    *s=(struct mt76_rx_window_stop){.slot=f->p.slot,.dci=9,.state=3,
        .dequeue=(f->p.ring+16*f->posts)|1};
    switch(f->fault) {
    case STOP_CURRENT:s->dequeue-=16;break;
    case STOP_FAIL:return false;
    case BAD_STATE:s->state=1;break;
    case BAD_SLOT:s->slot++;break;
    case BAD_DCI:s->dci++;break;
    case BAD_DCS:s->dequeue|=2;break;
    case BAD_DEQUEUE:s->dequeue+=16;break;
    default:break;
    }
    return true;
}
static uint64_t now(void *ctx) {
    struct fixture *f=ctx;
    if(f->fault==FROZEN)return 0;
    if(f->fault==BACKWARD && f->posts)return 0;
    if(f->fault==JUMP && f->posts)return 2000000;
    f->clock+=f->fault==SHORT_DEADLINE?100000:100;
    return f->clock;
}
static void init(struct fixture *f,enum fault fault,unsigned index) {
    memset(f,0,sizeof *f);f->fault=fault;f->fail_index=index;
    f->p.ring=0x100000;f->p.slot=1;f->p.dci=9;
    for(unsigned i=0;i<8;i++) {
        uint8_t *p=aligned_alloc(4096,4096);assert(p);packet(p,i);
        f->p.buffer[i]=p;f->p.physical[i]=0x200000+4096*i;
    }
    f->io=(struct mt76_rx_window_io){.context=f,.owned=owned,.post=post,
        .poll=poll,.stop=stop,.now=now};
    if(fault==MALFORMED)((uint8_t *)f->p.buffer[index])[0]=0;
    if(fault==UNSUPPORTED)put16((uint8_t *)f->p.buffer[index]+36,0x88);
    if(fault==OFF_CHANNEL)((uint8_t *)f->p.buffer[index])[80]=2;
}
static void destroy(struct fixture *f) {
    for(unsigned i=0;i<8;i++)free((void *)f->p.buffer[i]);
}
static unsigned scenarios;
static void run(enum fault fault,unsigned index,bool want) {
    struct fixture f;init(&f,fault,index);
    assert(mt76_rx_window_run(&f.s,&f.p,&f.io)==want);
    assert(f.s.attempted && f.s.complete==want && f.s.failed==!want);
    assert(f.posts<=8 && f.stops==(f.posts?1u:0u));
    if(want) {
        assert(f.s.stopped && f.cleanup==f.posts);
        if(fault==NO_EVENT)assert(f.posts==1 && f.s.beacons==0 && !f.s.item[0].inspected);
        else if(fault!=SHORT_DEADLINE) {
            assert(f.posts==8 && f.s.completions==8);
            unsigned beacons=fault==ZERO_ALL?0:(fault==ZERO || fault==MALFORMED || fault==UNSUPPORTED || fault==OFF_CHANNEL)?7:8;
            assert(f.s.beacons==beacons);
        }
        if(fault==FOREIGN || fault==STALE)assert(f.s.ignored>0);
        if(fault==ZERO)assert(!f.s.item[index].inspected && f.s.item[index].view_status==255);
        if(fault==FULL)assert(f.s.item[index].actual==4096 && f.s.item[index].consumed==84);
        if(fault==MALFORMED)assert(f.s.item[index].view_status==MT76_RX_INVALID);
        if(fault==UNSUPPORTED)assert(f.s.item[index].view_status==MT76_RX_UNSUPPORTED);
        if(fault==OFF_CHANNEL)assert(f.s.item[index].beacon_status==MT76_RX_CHANNEL_MISMATCH);
    } else if(fault!=CLEANUP_LOST)for(unsigned i=0;i<8;i++)assert(!f.s.item[i].inspected);
    unsigned posts=f.posts,stops=f.stops,checks=f.checks;
    assert(!mt76_rx_window_run(&f.s,&f.p,&f.io));
    assert(f.posts==posts && f.stops==stops && f.checks==checks);
    destroy(&f);scenarios++;
}
int main(void) {
    for(unsigned i=0;i<8;i++) {
        for(enum fault x=POST_FAIL;x<=CC_ERROR;x++)run(x,i,false);
        for(enum fault x=ZERO;x<=OFF_CHANNEL;x++)run(x,i,true);
    }
    for(enum fault x=NONE;x<=STALE;x++)run(x,0,true);
    run(SHORT_DEADLINE,0,true);
    run(STOP_CURRENT,0,true);run(ZERO_ALL,0,true);run(INITIAL_LOST,0,false);
    /* Bad plan/callback identities refuse before any post or stop. Restore
     * allocation pointers before freeing, including alias test cases. */
    for(unsigned mutation=0;mutation<17;mutation++) {
        struct fixture f;init(&f,NONE,0);struct mt76_rx_window_plan saved=f.p;
        switch(mutation) {
        case 0:f.p.ring=0;break;case 1:f.p.ring++;break;
        case 2:f.p.slot=0;break;case 3:f.p.slot=256;break;case 4:f.p.dci=8;break;
        case 5:f.p.physical[7]++;break;case 6:f.p.physical[7]=0;break;
        case 7:f.p.physical[7]=f.p.ring;break;
        case 8:f.p.physical[7]=f.p.physical[0];break;
        case 9:f.p.buffer[7]=f.p.buffer[0];break;case 10:f.p.buffer[7]=0;break;
        case 11:f.p.buffer[7]++;break;case 12:f.io.owned=0;break;
        case 13:f.io.post=0;break;case 14:f.io.poll=0;break;
        case 15:f.io.stop=0;break;case 16:f.io.now=0;break;
        }
        assert(!mt76_rx_window_run(&f.s,&f.p,&f.io));assert(!f.posts && !f.stops);
        f.p=saved;destroy(&f);scenarios++;
    }
    assert(!mt76_rx_window_run(0,0,0));
    struct mt76_rx_window s={0};assert(!mt76_rx_window_run(&s,0,0));
    /* Poison DMA pointers: no-data normal completion and cancellation never
     * inspect memory. All pages still have distinct aligned virtual identities. */
    for(unsigned variant=0;variant<4;variant++) {
        struct fixture f;init(&f,variant==0?NO_EVENT:variant==1?CC26:variant==2?CC27:CC28,0);
        struct mt76_rx_window_plan saved=f.p;
        for(unsigned i=0;i<8;i++)f.p.buffer[i]=(const uint8_t *)(uintptr_t)(0x1000+4096*i);
        assert(mt76_rx_window_run(&f.s,&f.p,&f.io)==(variant==0));
        for(unsigned i=0;i<8;i++)assert(!f.s.item[i].inspected);
        f.p=saved;destroy(&f);scenarios++;
    }
    /* Every submitted page can complete with zero actual bytes; no poison
     * pointer may be read merely because a Transfer Event exists. */
    {
        struct fixture f;init(&f,ZERO_ALL,0);struct mt76_rx_window_plan saved=f.p;
        for(unsigned i=0;i<8;i++)f.p.buffer[i]=(const uint8_t *)(uintptr_t)(0x1000+4096*i);
        assert(mt76_rx_window_run(&f.s,&f.p,&f.io) && f.s.completions==8);
        for(unsigned i=0;i<8;i++)assert(!f.s.item[i].inspected && !f.s.item[i].actual);
        f.p=saved;destroy(&f);scenarios++;
    }
    printf("mt76-rx-window: PASS %u ownership/event/deadline/stop/parser scenarios; no native adapter\n",scenarios);
}
