#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_native_identity_reply(void) {
    /* Independently transcribed native log words, not encoder output. */
    const uint32_t words[]={0x4a010008,0x00411000,0x76502000,0x4a010008};
    uint8_t native[17]={0};
    for(size_t i=0;i<4;i++)for(size_t b=0;b<4;b++)native[4*i+b]=(uint8_t)(words[i]>>(8*b));
    assert(mt76_mcu_validate_identity_reply(native,16));
    assert(!mt76_mcu_validate_identity_reply(NULL,16));
    for(size_t n=0;n<16;n++)assert(!mt76_mcu_validate_identity_reply(native,n));
    assert(!mt76_mcu_validate_identity_reply(native,17));
    assert(!mt76_mcu_validate_identity_reply(native,SIZE_MAX));
    for(size_t i=0;i<16;i++)for(unsigned bit=0;bit<8;bit++) {
        native[i]^=(uint8_t)(1u<<bit);
        assert(!mt76_mcu_validate_identity_reply(native,16));
        native[i]^=(uint8_t)(1u<<bit);
    }
    /* An internally consistent but stale sequence is still refused. */
    native[2]=native[14]=2;
    assert(!mt76_mcu_validate_identity_reply(native,16));
}

static void test_native_disabled_write_reply(void) {
    const uint32_t words[]={0x4a020008,0x00411004,0x00000000,0x4a020008};
    uint8_t native[17]={0};
    for(size_t i=0;i<4;i++)for(size_t b=0;b<4;b++)native[4*i+b]=(uint8_t)(words[i]>>(8*b));
    assert(mt76_mcu_validate_disabled_write_reply(native,16));
    assert(!mt76_mcu_validate_identity_reply(native,16));
    assert(!mt76_mcu_validate_disabled_write_reply(NULL,16));
    for(size_t n=0;n<16;n++)assert(!mt76_mcu_validate_disabled_write_reply(native,n));
    assert(!mt76_mcu_validate_disabled_write_reply(native,17));
    assert(!mt76_mcu_validate_disabled_write_reply(native,SIZE_MAX));
    for(size_t i=0;i<16;i++)for(unsigned bit=0;bit<8;bit++) {
        native[i]^=(uint8_t)(1u<<bit);
        assert(!mt76_mcu_validate_disabled_write_reply(native,16));
        native[i]^=(uint8_t)(1u<<bit);
    }
    /* Consistent framing cannot authorize a stale/different command. */
    for(unsigned seq=0;seq<16;seq++) {
        native[2]=native[14]=(uint8_t)seq;
        assert(mt76_mcu_validate_disabled_write_reply(native,16)==(seq==2));
    }
    native[2]=native[14]=2;
    native[4]=0; /* Other register with a valid header/trailer. */
    assert(!mt76_mcu_validate_disabled_write_reply(native,16));
}

static const uint8_t timing_reply[16]={8,0,3,0x4a,0,0x11,0x41,0,0x10,0x10,0xa4,0x33,8,0,3,0x4a};
struct timing_fake { unsigned ops,fail,auth,deny,writes; bool bad_ack,bad_second,enabled,missing,enabled_after; };
static bool timing_authorize(void *context) {
    struct timing_fake *f=context;return ++f->auth!=f->deny;
}
static bool timing_read_fake(void *context,uint32_t address,uint32_t *value) {
    struct timing_fake *f=context;
    const uint32_t order[]={0x1004,0x1100,0x1104,0,0x1100,0x1104,0x1004};
    assert(f->ops<7 && address==order[f->ops]);
    if(++f->ops==f->fail)return false;
    if(f->missing){*value=UINT32_MAX;return true;}
    if(address==0x1004)*value=f->enabled || (f->writes && f->enabled_after)?12:0;
    else if(address==0x1100)*value=f->writes?0x33a41010:0x33a4100a;
    else *value=f->writes && !f->bad_second?0x209:0x214;
    return true;
}
static int timing_exchange_fake(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity) {
    struct timing_fake *f=context;
    const uint8_t expected[24]={16,0,0xc3,0x50,0,0x11,0x41,0,0x10,0x10,0xa4,0x33,4,0x11,0x41,0,9,2,0,0,0,0,0,0};
    assert(f->ops==3 && length==24 && capacity>=16 && !memcmp(packet,expected,24));
    if(++f->ops==f->fail)return -1;
    f->writes++;memcpy(reply,timing_reply,16);
    if(f->bad_ack)reply[4]^=4;
    return 16;
}
static enum mt76_mcu_timing_status timing_run(struct timing_fake *f,struct mt76_mcu_timing *s) {
    const struct mt76_mcu_timing_io io={f,timing_authorize,timing_read_fake,timing_exchange_fake};
    return mt76_mcu_apply_timing(s,&io);
}
static void test_timing_initialization(void) {
    uint8_t reply[17]={0};memcpy(reply,timing_reply,16);
    assert(mt76_mcu_validate_timing_reply(reply,16));
    assert(!mt76_mcu_validate_timing_reply(NULL,16));
    for(size_t n=0;n<18;n++)if(n!=16)assert(!mt76_mcu_validate_timing_reply(reply,n));
    assert(!mt76_mcu_validate_timing_reply(reply,SIZE_MAX));
    for(unsigned i=0;i<16;i++)for(unsigned bit=0;bit<8;bit++) {
        reply[i]^=1u<<bit;assert(!mt76_mcu_validate_timing_reply(reply,16));reply[i]^=1u<<bit;
    }
    for(unsigned seq=0;seq<16;seq++) {
        reply[2]=reply[14]=seq;
        assert(mt76_mcu_validate_timing_reply(reply,16)==(seq==3));
    }
    struct timing_fake f={0};struct mt76_mcu_timing s={0};
    assert(timing_run(&f,&s)==MT76_TIMING_OK && s.complete && s.ack_validated);
    assert(s.reads==6 && f.ops==7 && f.writes==1 && s.received==16);
    assert(timing_run(&f,&s)==MT76_TIMING_REFUSED && f.ops==7);
    for(unsigned fail=1;fail<=7;fail++) {
        f=(struct timing_fake){.fail=fail};s=(struct mt76_mcu_timing){0};
        assert(timing_run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==fail);
        assert(timing_run(&f,&s)==MT76_TIMING_REFUSED && f.ops==fail);
    }
    for(unsigned deny=1;deny<=8;deny++) {
        f=(struct timing_fake){.deny=deny};s=(struct mt76_mcu_timing){0};
        assert(timing_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==deny-1);
    }
    f=(struct timing_fake){.bad_ack=true};s=(struct mt76_mcu_timing){0};
    assert(timing_run(&f,&s)==MT76_TIMING_REPLY && f.ops==4 && !s.complete);
    f=(struct timing_fake){.bad_second=true};s=(struct mt76_mcu_timing){0};
    assert(timing_run(&f,&s)==MT76_TIMING_STATE && s.ack_validated && !s.complete);
    f=(struct timing_fake){.enabled=true};s=(struct mt76_mcu_timing){0};
    assert(timing_run(&f,&s)==MT76_TIMING_STATE && !f.writes);
    f=(struct timing_fake){.missing=true};s=(struct mt76_mcu_timing){0};
    assert(timing_run(&f,&s)==MT76_TIMING_STATE && !f.writes);
    f=(struct timing_fake){.enabled_after=true};s=(struct mt76_mcu_timing){0};
    assert(timing_run(&f,&s)==MT76_TIMING_STATE && !s.complete);
    assert(mt76_mcu_apply_timing(NULL,NULL)==MT76_TIMING_REFUSED);
    puts("mt76 timing init: PASS exact ACK, all operation failures, cancellation, readback and disabled-MAC gates");
}

static void test_register_packets(void) {
    struct mt76_mcu_register_pair pairs[25];uint8_t out[202];
    for(size_t i=0;i<25;i++)pairs[i]=(struct mt76_mcu_register_pair){(uint32_t)(4*i),0x12345678u+(uint32_t)i};
    for(size_t count=1;count<=24;count++) {
        memset(out,0xcc,sizeof out);
        size_t length=8*count+8;
        assert(mt76_mcu_register_packet(true,15,pairs,count,out+1,length)==length);
        assert(out[0]==0xcc && out[length+1]==0xcc);
        assert(out[1]==8*count && out[2]==0 && out[3]==0xcf && out[4]==0x50);
        for(size_t i=0;i<count;i++) {
            const uint8_t *p=out+5+8*i;
            assert(p[0]==4*i && p[1]==0 && p[2]==0x41 && p[3]==0);
            assert(p[4]==0x78+i && p[5]==0x56 && p[6]==0x34 && p[7]==0x12);
        }
        for(size_t i=length-3;i<=length;i++)assert(out[i]==0);
    }
    const struct mt76_mcu_register_pair read={0x1000,0};
    const uint8_t native_request[16]={8,0,0xa1,0x50,0,0x10,0x41,0,0,0,0,0,0,0,0,0};
    assert(mt76_mcu_register_packet(false,1,&read,1,out,sizeof out)==16);
    assert(!memcmp(out,native_request,16));
    for(unsigned fault=0;fault<10;fault++) {
        struct mt76_mcu_register_pair invalid[25];memcpy(invalid,pairs,sizeof pairs);
        size_t count=24,capacity=sizeof out;uint8_t seq=1;bool write=true;
        if(fault==0)count=0;
        if(fault==1)count=25;
        if(fault==2)count=SIZE_MAX;
        if(fault==3)capacity=199;
        if(fault==4)seq=0;
        if(fault==5)seq=16;
        if(fault==6)invalid[23].address=0x4000;
        if(fault==7)invalid[23].address=UINT32_MAX;
        if(fault==8)invalid[23].address=3;
        if(fault==9)write=false;
        memset(out,0xcc,sizeof out);
        assert(!mt76_mcu_register_packet(write,seq,invalid,count,out,capacity));
        for(size_t i=0;i<sizeof out;i++)assert(out[i]==0xcc);
    }
    const struct mt76_mcu_register_pair last={0x3ffc,0};
    assert(mt76_mcu_register_packet(true,1,&last,1,out,sizeof out)==16);
    assert(out[4]==0xfc && out[5]==0x3f && out[6]==0x41 && out[7]==0);
    assert(!mt76_mcu_register_packet(true,1,NULL,1,out,sizeof out));
    assert(!mt76_mcu_register_packet(true,1,pairs,1,NULL,sizeof out));
}

int main(void) {
    test_native_identity_reply();
    test_native_disabled_write_reply();
    test_register_packets();
    test_timing_initialization();
    uint8_t payload[193], wire[202];
    memset(payload, 0xab, sizeof(payload));
    /* Independently specified Q_SELECT(1,1), command1, sequence3. */
    const uint8_t q[] = {1,0,0,0,1,0,0,0};
    const uint8_t expected[] = {8,0,0x13,0x50,1,0,0,0,1,0,0,0,0,0,0,0};
    assert(mt76_mcu_encode(1,3,q,sizeof(q),wire,sizeof(wire)) == sizeof(expected));
    assert(!memcmp(wire,expected,sizeof(expected)));
    for (size_t n=1; n<=192; ++n) {
        memset(wire,0xcc,sizeof(wire));
        size_t total=((n+3)&~(size_t)3)+8;
        assert(mt76_mcu_encode(127,15,payload,n,wire+1,total)==total);
        assert(wire[0]==0xcc && wire[total+1]==0xcc);
        assert(!memcmp(wire+5,payload,n));
        for(size_t i=5+n;i<=total;++i) assert(wire[i]==0);
        memset(wire,0xcc,sizeof(wire));
        assert(!mt76_mcu_encode(1,1,payload,n,wire,total-1));
        for(size_t i=0;i<sizeof(wire);++i) assert(wire[i]==0xcc);
    }
    assert(!mt76_mcu_encode(1,0,q,8,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(1,16,q,8,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(128,1,q,8,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(0,1,q,8,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(1,1,payload,193,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(1,1,NULL,8,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(1,1,q,0,wire,sizeof(wire)));
    assert(!mt76_mcu_encode(1,1,q,8,NULL,sizeof(wire)));
    const uint8_t queue_expected[]={8,0,0x10,0x50,1,0,0,0,1,0,0,0,0,0,0,0};
    assert(mt76_mcu_select_response_queue(wire,sizeof(wire))==16);
    assert(!memcmp(wire,queue_expected,16));
    memset(wire,0xcc,sizeof wire);
    assert(!mt76_mcu_select_response_queue(wire,15));
    for(size_t i=0;i<sizeof wire;i++)assert(wire[i]==0xcc);
    assert(!mt76_mcu_select_response_queue(NULL,16));
    uint8_t reply[1025]={0x34,0x12,0xa7,0xff};
    struct mt76_mcu_response r;
    assert(mt76_mcu_decode(reply,8,&r));
    assert(r.sequence==7 && r.event==10 && r.reported_length==0x1234);
    for(size_t n=0;n<8;++n) assert(!mt76_mcu_decode(reply,n,&r));
    assert(!mt76_mcu_decode(reply,1025,&r));
    assert(!mt76_mcu_decode(NULL,8,&r));
    assert(!mt76_mcu_decode(reply,8,NULL));
    struct mt76_mcu_session s={0}; uint8_t seq=0;
    for(unsigned i=1;i<=15;++i) {
        assert(mt76_mcu_begin(&s,&seq) && seq==i);
        assert(!mt76_mcu_begin(&s,&seq));
        assert(mt76_mcu_retire(&s,seq,true));
        assert(!mt76_mcu_retire(&s,seq,true));
    }
    assert(!mt76_mcu_begin(&s,&seq));
    for(unsigned failure=0;failure<2;++failure) {
        s=(struct mt76_mcu_session){0};
        assert(mt76_mcu_begin(&s,&seq));
        assert(!mt76_mcu_retire(&s,failure?2:seq,failure));
        assert(s.poisoned && !mt76_mcu_begin(&s,&seq));
        assert(!mt76_mcu_retire(&s,seq,true));
    }
    puts("mt76 MCU codec/session: PASS");
}
