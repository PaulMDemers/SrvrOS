#include <srvros/mt76_mac_cycle.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    struct mt76_mac_cycle state;
    unsigned calls,auths,fail,cancel,mismatch,ones,busy,cleanup_calls;
    uint32_t mac,filter,txop;
    uint64_t now;
    bool cancelled,broken,delay_fail,cleanup_lost,freeze;
};
static bool owned(void *p,bool cleanup) {
    struct fixture *f=p;f->auths++;
    if(f->auths==f->cancel)f->cancelled=true;
    return cleanup?!f->cleanup_lost:!f->cancelled;
}
static bool event(struct fixture *f,bool cleanup) {
    if(!cleanup)assert(!f->cancelled);
    f->calls++;if(cleanup)f->cleanup_calls++;
    if(!f->freeze)f->now+=10;
    if(f->calls==f->fail){f->broken=true;return false;}
    return true;
}
static bool read_word(void *p,uint32_t a,uint32_t *v,bool cleanup) {
    struct fixture *f=p;bool ok=event(f,cleanup);
    switch(a){
    case 0x1004:*v=f->mac;break;
    case 0x2000:*v=0xf000f200;break;
    case 0x208:*v=0x40000050 | (f->busy==1?10:0);break;
    case 0x1400:*v=f->filter;break;
    case 0x1340:*v=f->txop;break;
    case 0x13b0:*v=0x2f2f;break;
    case 0x2124:*v=UINT32_MAX;break;
    case 0x1200:*v=f->busy==2?3:0;break;
    case 0x438:*v=f->busy==3?1:0;break;
    case 0x430:*v=f->busy==4?0x10000:0;break;
    case 0xa30:case 0xa34:*v=0;break;
    default:assert(0);
    }
    if(f->calls==f->mismatch)*v^=1;
    if(f->calls==f->ones)*v=UINT32_MAX;
    return ok;
}
static bool write_word(void *p,uint32_t a,uint32_t v,bool cleanup) {
    struct fixture *f=p;bool ok=event(f,cleanup);
    if(a==0x1004){assert(v==0 || v==4 || v==12);f->mac=v;}
    else if(a==0x1400){assert(v==0x17f97);f->filter=v;}
    else {assert(a==0x1340 && (v==0x10583f || v==0x583f));f->txop=v;}
    return ok;
}
static bool delay(void *p,unsigned us,bool cleanup) {
    struct fixture *f=p;bool ok=event(f,cleanup);if(!f->freeze)f->now+=us;
    return ok && !f->delay_fail;
}
static uint64_t now(void *p){return ((struct fixture *)p)->now;}
static struct fixture fresh(void){return (struct fixture){.filter=0x17f97,.txop=0x10583f};}
static bool run(struct fixture *f){const struct mt76_mac_cycle_io io={f,owned,read_word,write_word,delay,now};return mt76_mac_cycle_run(&f->state,&io);}
static void no_replay(struct fixture *f){unsigned n=f->calls;struct mt76_mac_cycle s=f->state;assert(!run(f) && f->calls==n && !memcmp(&s,&f->state,sizeof(s)));}
int main(void){
    struct fixture base=fresh();assert(run(&base));
    assert(base.state.started && base.state.snapshot_complete && base.state.ibi9==UINT32_MAX);
    assert(base.state.mac_disabled && base.state.tx_drained && base.state.rx_drained);
    assert(base.state.filter_restored && base.state.txop_restored && base.mac==0 && base.txop==0x10583f);
    assert(base.state.tx_rounds==1 && base.state.rx_rounds==7 && base.state.writes==7 && base.state.waits==1);
    assert(base.state.hold_us>=10000);no_replay(&base);
    for(unsigned n=1;n<=base.calls;n++){
        struct fixture f=fresh();f.fail=n;assert(!run(&f) && f.state.failed && !f.state.complete);
        if(f.state.mac_touched){assert(f.cleanup_calls);assert(f.mac==0);}
        no_replay(&f);
    }
    for(unsigned n=1;n<=base.auths;n++){
        struct fixture f=fresh();f.cancel=n;bool ok=run(&f);
        if(f.cancelled && !ok){assert(f.state.failed);if(f.state.mac_touched)assert(f.mac==0 && f.cleanup_calls);}
        no_replay(&f);
    }
    for(unsigned b=1;b<=4;b++){
        struct fixture f=fresh();f.busy=b;assert(!run(&f));assert(f.mac==0 && f.calls<2000);no_replay(&f);
    }
    struct fixture f=fresh();f.freeze=true;assert(!run(&f) && f.state.mac_disabled);no_replay(&f);
    f=fresh();f.delay_fail=true;assert(!run(&f) && f.state.mac_disabled);no_replay(&f);
    for(unsigned n=1;n<=base.calls;n++){
        f=fresh();f.ones=n;bool ok=run(&f);
        /* Only read operations see this injection; IBI9 is intentionally raw. */
        if(!ok){assert(f.state.failed);if(f.state.mac_touched)assert(f.mac==0);}
        no_replay(&f);
    }
    for(unsigned i=0;i<16;i++){
        static const unsigned ops[]={1,2,3,4,7,10,12,14,19,25,49,51,52,53,54,8};
        f=fresh();f.mismatch=ops[i];bool ok=run(&f);
        if(ops[i]!=8)assert(!ok && f.state.failed);
        if(f.state.mac_touched)assert(f.mac==0);
        no_replay(&f);
    }
    f=fresh();f.cleanup_lost=true;assert(!run(&f) && !f.state.mac_disabled && f.mac==12);
    assert(!f.state.filter_restored && !f.state.txop_restored);no_replay(&f);
    struct mt76_mac_cycle_io io={&f,owned,read_word,write_word,delay,now};
    assert(!mt76_mac_cycle_run(NULL,&io) && !mt76_mac_cycle_run(&f.state,NULL));
    printf("mt76 MAC cycle: PASS %u operations,%u reads,%u writes; every callback failure/cancellation,busy queues,clock/wait failure,all-ones,no replay; synthetic only\n",base.state.operations,base.state.reads,base.state.writes);
}
