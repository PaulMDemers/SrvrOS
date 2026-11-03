#include <srvros/display_owner.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct display_owner_profile profile(void) {
    return (struct display_owner_profile){.gpu=0x0a268086,.pch=0x9c438086,.hda=0x0a0c8086,
        .subsystem=0x011a106b,.access_valid=true};
}
int main(void) {
    struct display_owner_pool pool={0};struct display_owner o={0},other={0};
    struct display_owner_profile p=profile(),changed;struct display_owner_token t[8],bad;
    assert(display_owner_open(&o,&pool,&p));assert(o.state==DISPLAY_OWNER_OBSERVED && o.guard==1 && o.epoch==1);
    assert(display_owner_check(&o,1,&p)==DISPLAY_OWNER_REQUIRED);
    /* Quiet observed controls must never synthesize backing/ownership proof. */
    assert(!display_owner_open(&o,&pool,&p));
    for(unsigned bit=0;bit<7;bit++) {
        struct display_owner fixture={0};changed=p;changed.assured=DISPLAY_OWNER_REQUIRED^(1u<<bit);
        assert(display_owner_open(&fixture,&pool,&changed));
        assert(display_owner_check(&fixture,fixture.epoch,&changed)==(1u<<bit));
    }
    changed=p;changed.assured=DISPLAY_OWNER_REQUIRED;
    assert(display_owner_open(&other,&pool,&changed));assert(other.state==DISPLAY_OWNER_ADOPTED);
    assert(!display_owner_check(&other,other.epoch,&changed)); /* Host model only. */
    for(unsigned i=0;i<8;i++)assert(display_owner_hold(&o,1,i%2,&t[i]));
    assert(o.refs[0]==4 && o.refs[1]==4);bad=t[0];assert(!display_owner_hold(&o,1,0,&bad));
    assert(display_owner_begin(&o,&t[0]));assert(!display_owner_begin(&o,&t[0]));
    assert(!display_owner_release(&o,&t[0]));assert(display_owner_check(&o,1,&p)&DISPLAY_OWNER_INFLIGHT);
    bad=t[1];bad.epoch++;assert(!display_owner_release(&o,&bad));bad=t[1];bad.slot=8;assert(!display_owner_release(&o,&bad));
    assert(!display_owner_release(&other,&t[1]));assert(!display_owner_close(&o,2));
    assert(display_owner_close(&o,1));assert(o.state==DISPLAY_OWNER_CLOSING && o.guard==1);
    assert(!display_owner_hold(&o,1,0,&bad));assert(!display_owner_begin(&o,&t[1]));
    for(unsigned i=1;i<8;i++)assert(display_owner_release(&o,&t[i]));
    assert(o.guard && o.inflight==1);assert(display_owner_end(&o,&t[0]));assert(!display_owner_end(&o,&t[0]));
    assert(display_owner_release(&o,&t[0]));assert(o.state==DISPLAY_OWNER_RETIRED && !o.guard);
    assert(!display_owner_release(&o,&t[0]));assert(!display_owner_close(&o,1));
    uint64_t old=o.epoch;assert(display_owner_open(&o,&pool,&p));assert(o.epoch>old);
    assert(!display_owner_release(&o,&t[0]));assert(display_owner_check(&o,old,&p)==DISPLAY_OWNER_STALE);
    assert(display_owner_hold(&o,o.epoch,0,&t[0]));assert(display_owner_release(&o,&t[0]));
    bad=t[0];assert(display_owner_hold(&o,o.epoch,0,&t[0]));assert(t[0].slot==bad.slot && t[0].serial!=bad.serial);
    assert(!display_owner_begin(&o,&bad));assert(display_owner_begin(&o,&t[0]));
    assert(display_owner_fault(&o,o.epoch,7));assert(o.guard && o.state==DISPLAY_OWNER_FAULT);
    assert(!display_owner_close(&o,o.epoch));assert(!display_owner_open(&o,&pool,&p));
    assert(display_owner_end(&o,&t[0]));assert(display_owner_release(&o,&t[0]));assert(o.guard && o.state==DISPLAY_OWNER_FAULT);
    assert(display_owner_check(&o,o.epoch,&p)&DISPLAY_OWNER_POISON);
    struct display_owner empty={0};pool.epoch=UINT64_MAX;assert(!display_owner_open(&empty,&pool,&p));
    assert(empty.state==DISPLAY_OWNER_EMPTY);pool.epoch=20;
    for(unsigned j=0;j<6;j++) {
        changed=p;
        if(j==0)changed.gpu++;
        if(j==1)changed.pch++;
        if(j==2)changed.hda++;
        if(j==3)changed.subsystem++;
        if(j==4)changed.access_valid=false;
        if(j==5)changed.assured=128;
        assert(!display_owner_open(&empty,&pool,&changed));assert(!empty.epoch);
    }
    assert(display_owner_open(&empty,&pool,&p));
    for(unsigned i=0;i<DISPLAY_OWNER_WORDS;i++) {changed=p;changed.raw[i]=1;assert(display_owner_check(&empty,empty.epoch,&changed)&DISPLAY_OWNER_CHANGED);}
    changed=p;changed.active=1;assert(display_owner_check(&empty,empty.epoch,&changed)&DISPLAY_OWNER_ACTIVE);
    empty.serial=UINT64_MAX;assert(!display_owner_hold(&empty,empty.epoch,0,&bad));
    empty.serial=0;empty.refs[0]=UINT32_MAX;assert(!display_owner_hold(&empty,empty.epoch,0,&bad));
    assert(!display_owner_close(&empty,empty.epoch));
    puts("display-owner: PASS immutable profile, unknown/active/changed/stale refusal, bounded references, epoch/serial exhaustion, cancellation/drain, retained fault; no writer API");
}
