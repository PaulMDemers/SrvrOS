#include <srvros/mt76_poweron_calibration.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct event {char kind;uint32_t address,value;};
struct fixture {
    struct mt76_calibration state;
    struct mt76_identity identity;
    struct mt76_ant_fields board;
    unsigned calls,auths,fail,cancel,mismatch,all_ones,restore_fail,board_loss;
    bool failed,cancelled,reenter,readonly_ibi;
    uint32_t alc,ibi;
    struct event events[40];
};
static struct mt76_poweron_io make_io(struct fixture *f);
static enum mt76_calibration_status run(struct fixture *f) {
    struct mt76_poweron_io io=make_io(f);
    return mt76_poweron_calibration_run(&f->state,&io,&f->identity,&f->board,1,20);
}
static bool auth(void *p) {
    struct fixture *f=p;f->auths++;
    if(f->auths==f->cancel)f->cancelled=true;
    if(f->auths==f->board_loss)f->board.complete=false;
    if(f->reenter) {
        struct mt76_calibration copy=f->state;
        assert(run(f)==MT76_CAL_REFUSED);
        assert(!memcmp(&copy,&f->state,sizeof copy));
    }
    return !f->cancelled;
}
static bool event(struct fixture *f,char kind,uint32_t a,uint32_t v) {
    assert(f->calls<40);
    if(kind!='s')assert(!f->failed && !f->cancelled);
    f->events[f->calls++]=(struct event){kind,a,v};
    if(f->calls==f->fail){f->failed=true;return false;}
    return true;
}
static bool read_word(void *p,uint32_t a,uint32_t *v) {
    struct fixture *f=p;bool ok=event(f,'r',a,0);
    switch(a) {
    case 0x1004:*v=12;break;
    case 0x2000:*v=0xf000f200;break;
    case 0x208:*v=0x40000050;break;
    case 0x13b0:*v=f->alc;break;
    case 0x2124:*v=f->ibi;break;
    default:assert(0);
    }
    if(f->calls==f->mismatch){*v^=1;f->failed=true;}
    if(f->calls==f->all_ones)*v=UINT32_MAX;
    return ok;
}
static bool write_word(void *p,uint32_t a,uint32_t v) {
    struct fixture *f=p;bool ok=event(f,'w',a,v);
    if(a==0x13b0)f->alc=v;
    else {assert(a==0x2124);if(!f->readonly_ibi)f->ibi=v;}
    return ok;
}
static bool command(void *p,enum mt76_poweron_command c,uint32_t v) {
    struct fixture *f=p;
    assert(c<=MT76_POWERON_RXDCOC);
    if(c==MT76_POWERON_FULL || c==MT76_POWERON_LC)
        assert(f->alc==0 && f->ibi==0xffffff7e);
    if(c==MT76_POWERON_RXDCOC)assert(f->alc==0x2f2f1b1b && f->ibi==0xffffffff);
    return event(f,'c',c,v);
}
static bool delay(void *p,unsigned us) {
    struct fixture *f=p;bool ok=event(f,'d',0,us);
    /* Proves saved IBI is read AFTER the suppression delay, not beforehand. */
    if(us==500)f->ibi=0xffffffff;
    return ok;
}
static bool restore(void *p,uint32_t a,uint32_t saved,uint32_t *seen) {
    struct fixture *f=p;bool ok=event(f,'s',a,saved);
    if(a==0x13b0){assert(saved==0x2f2f1b1b);f->alc=saved;}
    else {assert(a==0x2124 && saved==0xffffffff);f->ibi=saved;}
    *seen=saved;if(a==f->restore_fail)*seen^=1;
    return ok;
}
static struct mt76_poweron_io make_io(struct fixture *f) {
    return (struct mt76_poweron_io){f,auth,read_word,write_word,command,delay,restore};
}
static struct fixture fresh(void) {
    return (struct fixture){.identity={0x76100002,0x76502000},
        .board={.attempted=true,.complete=true,.valid=true,.reads=24,.writes=2,
            .polls=2,.blocks=2,.antenna=0x17ff,.cfg1=0xffef,.nic0=0xfd11,.nic1=0x1004},
        .alc=0x2f2f1b1b,.ibi=0x12345678};
}
static void retired(struct fixture *f) {
    assert(f->state.attempted && !f->state.complete);
    assert(f->state.restore_attempted==f->state.modified_mask);
    if(f->state.cleanup_complete) {
        assert(f->state.restored_mask==f->state.modified_mask);
        if(f->state.modified_mask&1)assert(f->alc==0x2f2f1b1b);
        if(f->state.modified_mask&2)assert(f->ibi==0xffffffff);
    }
    struct mt76_calibration copy=f->state;unsigned calls=f->calls,auths=f->auths;
    assert(run(f)==MT76_CAL_REFUSED);
    assert(f->calls==calls && f->auths==auths && !memcmp(&copy,&f->state,sizeof copy));
}
int main(void) {
    static const uint32_t ids[5]={1,12,255,3,2},args[5]={0,1,0x600,0,1};
    for(unsigned step=0;step<5;step++) {
        uint8_t packet[16],reply[16];
        assert(mt76_poweron_packet(step,packet,16)==16);
        uint32_t wire[4]={0x51f00008u|((step+1)<<16),ids[step],args[step],0};
        uint32_t response[4]={0x4a000008u|((step+1)<<16),ids[step],args[step],0x4a000008u|((step+1)<<16)};
        for(unsigned i=0;i<16;i++){assert(packet[i]==(uint8_t)(wire[i/4]>>(8*(i%4))));reply[i]=(uint8_t)(response[i/4]>>(8*(i%4)));}
        assert(mt76_poweron_reply(step,reply,16));
        for(unsigned bit=0;bit<128;bit++){reply[bit/8]^=1u<<(bit%8);assert(!mt76_poweron_reply(step,reply,16));reply[bit/8]^=1u<<(bit%8);}
        for(unsigned n=0;n<32;n++)if(n!=16)assert(!mt76_poweron_reply(step,reply,n));
        for(unsigned n=0;n<16;n++)assert(!mt76_poweron_packet(step,packet,n));
        assert(!mt76_poweron_reply(step,0,16) && !mt76_poweron_packet(step,0,16));
    }
    uint8_t invalid[16]={0};assert(!mt76_poweron_packet(5,invalid,16) && !mt76_poweron_reply(5,invalid,16));

    static const struct event expected[]={
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0},
        {'c',MT76_POWERON_R,0},{'c',MT76_POWERON_VCO,1},{'d',0,10},
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0},{'r',0x13b0,0},
        {'w',0x13b0,0},{'r',0x13b0,0},{'d',0,500},{'r',0x2124,0},
        {'w',0x2124,0xffffff7e},{'r',0x2124,0},
        {'c',MT76_POWERON_FULL,0x600},{'c',MT76_POWERON_LC,0},{'d',0,15000},
        {'s',0x2124,0xffffffff},{'s',0x13b0,0x2f2f1b1b},
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0},
        {'c',MT76_POWERON_RXDCOC,1},
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0}
    };
    const unsigned total=sizeof expected/sizeof *expected;
    struct fixture f=fresh();f.reenter=true;assert(run(&f)==MT76_CAL_OK);
    assert(f.calls==total && f.auths==27 && f.state.reads==16 && f.state.writes==2 &&
           f.state.commands==5 && f.state.waits==3 && f.state.complete && f.state.cleanup_complete &&
           f.state.saved_mask==3 && f.state.modified_mask==3 && f.state.restored_mask==3);
    for(unsigned i=0;i<total;i++)assert(f.events[i].kind==expected[i].kind &&
        f.events[i].address==expected[i].address && f.events[i].value==expected[i].value);
    struct mt76_calibration done=f.state;assert(run(&f)==MT76_CAL_REFUSED);
    assert(!memcmp(&done,&f.state,sizeof done) && f.calls==total);
    for(unsigned n=1;n<=total;n++) {
        f=fresh();f.fail=n;assert(run(&f)!=MT76_CAL_OK);retired(&f);
    }
    for(unsigned n=1;n<=27;n++) {
        f=fresh();f.cancel=n;assert(run(&f)==MT76_CAL_REFUSED);retired(&f);
        f=fresh();f.board_loss=n;assert(run(&f)==MT76_CAL_REFUSED);retired(&f);
    }
    for(unsigned i=0;i<total;i++)if(expected[i].kind=='r') {
        f=fresh();f.mismatch=i+1;assert(run(&f)!=MT76_CAL_OK);retired(&f);
        f=fresh();f.all_ones=i+1;
        if(i==13)assert(run(&f)==MT76_CAL_OK && f.state.complete);
        else {assert(run(&f)!=MT76_CAL_OK);retired(&f);}
    }
    for(unsigned i=0;i<2;i++) {
        f=fresh();f.restore_fail=i?0x13b0:0x2124;
        assert(run(&f)==MT76_CAL_RESTORE && f.state.restore_attempted==3 && !f.state.cleanup_complete);
        assert(f.state.commands==4);retired(&f);
    }
    f=fresh();f.readonly_ibi=true;assert(run(&f)==MT76_CAL_REFUSED && f.state.commands==2);retired(&f);
    for(unsigned n=0;n<16;n++) {
        f=fresh();struct mt76_poweron_io io=make_io(&f);
        const struct mt76_identity *id=&f.identity;const struct mt76_ant_fields *b=&f.board;
        switch(n) {
        case 0:io.authorize=0;break;case 1:io.read=0;break;case 2:io.write=0;break;
        case 3:io.command=0;break;case 4:io.delay=0;break;case 5:io.restore=0;break;
        case 6:id=0;break;case 7:b=0;break;case 8:f.identity.asic^=1;break;
        case 9:f.identity.mac^=1;break;case 10:f.board.nic1|=1u<<13;break;
        case 11:f.board.complete=false;break;case 12:f.board.nic0^=1;break;
        case 13:f.board.polls=0;break;case 14:f.board.reads++;break;case 15:f.board.antenna^=1;break;
        }
        assert(mt76_poweron_calibration_run(&f.state,&io,id,b,1,20)==MT76_CAL_REFUSED);
        assert(!f.state.attempted && !f.calls && !f.auths);
    }
    f=fresh();struct mt76_poweron_io io=make_io(&f);
    assert(mt76_poweron_calibration_run(0,&io,&f.identity,&f.board,1,20)==MT76_CAL_REFUSED);
    assert(mt76_poweron_calibration_run(&f.state,0,&f.identity,&f.board,1,20)==MT76_CAL_REFUSED);
    for(unsigned c=0;c<256;c++)if(c!=1)
        assert(mt76_poweron_calibration_run(&f.state,&io,&f.identity,&f.board,c,20)==MT76_CAL_REFUSED);
    assert(mt76_poweron_calibration_run(&f.state,&io,&f.identity,&f.board,1,40)==MT76_CAL_REFUSED);
    assert(!f.calls && !f.auths && !f.state.attempted);
    puts("mt76-poweron-calibration: PASS exact R/VCO/FULL/LC/RXDCOC order, three waits, fresh IBI, write effects, reverse cleanup, every callback/auth/board loss, no replay; synthetic only");
}
