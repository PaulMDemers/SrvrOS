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

int main(void) {
    test_native_identity_reply();
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
