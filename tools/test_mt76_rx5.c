/* Reuse the existing independent USB/RXWI packet builder and all legacy
 * regressions. No native packet, SSID, BSSID or credential is embedded. */
#define main legacy_rx_main
#include "test_mt76_rx.c"
#undef main

static enum mt76_rx_status parse5(const uint8_t *p,size_t n,unsigned ch,unsigned bw) {
    struct mt76_rx_beacon5 b;memset(&b,0xa5,sizeof b);
    enum mt76_rx_status s=mt76_rx_beacon5(p,n,ch,bw,&b);
    if(s!=MT76_RX_OK)for(size_t i=0;i<sizeof b;i++)assert(!((uint8_t *)&b)[i]);
    return s;
}
static size_t high_make(bool pad,bool probe,bool ht) {
    size_t n=make(pad,probe,(const uint8_t *)"fixture",7);
    /* Replace the legacy DS IE with an unknown IE of the same length. */
    wire[36+24+(pad?2:0)+12+2+7+4]=99;
    if(ht){uint8_t op[22]={161};n=append(n,61,op,sizeof op);}
    return n;
}
int main(void) {
    assert(legacy_rx_main()==0);
    for(unsigned pad=0;pad<2;pad++)for(unsigned probe=0;probe<2;probe++)for(unsigned ht=0;ht<2;ht++) {
        size_t n=high_make(pad,probe,ht);struct mt76_rx_beacon5 b;
        assert(mt76_rx_beacon5(wire,n,161,20,&b)==MT76_RX_OK);
        assert(b.beacon.channel==161 && b.ht_present==(bool)ht && !b.beacon.ds_present);
        assert(b.beacon.probe_response==(bool)probe && b.beacon.ssid_size==7);
        assert(!memcmp(b.beacon.ssid,"fixture",7) && b.beacon.privacy);
        assert(!b.beacon.rsn_present && !b.beacon.wpa_present);
        assert(b.beacon.sequence==0x123 && b.beacon.interval==100);
        assert(!mt76_rx_advertised_channel(wire,n));
        assert(parse(n,161)==MT76_RX_INVALID);
        uint8_t saved[512];memcpy(saved,wire,sizeof wire);
        for(size_t k=0;k<n;k++) {
            uint8_t *p=malloc(k?k:1);assert(p);memcpy(p,wire,k);
            assert(parse5(p,k,161,20)!=MT76_RX_OK);free(p);
        }
        uint8_t unaligned[513];memcpy(unaligned+1,wire,n);
        assert(parse5(unaligned+1,n,161,20)==MT76_RX_OK);
        memset(wire+n,0xff,sizeof wire-n);assert(parse5(wire,sizeof wire,161,20)==MT76_RX_OK);
        memcpy(wire,saved,sizeof wire);
        for(unsigned ch=0;ch<=255;ch++)if(ch!=161)assert(parse5(wire,n,ch,20)==MT76_RX_INVALID);
        for(unsigned bw=0;bw<=160;bw++)if(bw!=20)assert(parse5(wire,n,161,bw)==MT76_RX_INVALID);
        /* Preserve all shared USB rejection behavior in the high-band entry. */
        for(unsigned bit=0;bit<32;bit++)if((1u<<bit)&((7u<<8)|(1u<<3)|(1u<<11)|(1u<<15)|(1u<<16)|(7u<<19))) {
            memcpy(wire,saved,sizeof wire);put32(wire+4,(pad?1u<<14:0)|(1u<<bit));
            assert(parse5(wire,n,161,20)!=MT76_RX_OK);
        }
        for(unsigned bit=0;bit<16;bit++)if((1u<<bit)&0xc70f) {
            memcpy(wire,saved,sizeof wire);put16(wire+36,(probe?0x50:0x80)|(1u<<bit));
            assert(parse5(wire,n,161,20)!=MT76_RX_OK);
        }
    }
    for(unsigned ch=0;ch<=255;ch++) {
        size_t n=high_make(false,false,false);uint8_t op[22]={(uint8_t)ch};n=append(n,61,op,22);
        assert(parse5(wire,n,161,20)==(ch==161?MT76_RX_OK:MT76_RX_CHANNEL_MISMATCH));
    }
    for(unsigned len=0;len<=24;len++)if(len!=22) {
        size_t n=high_make(false,false,false);uint8_t op[24]={161};n=append(n,61,op,len);
        assert(parse5(wire,n,161,20)==MT76_RX_INVALID);
    }
    size_t n=high_make(false,false,true);uint8_t op[22]={161};n=append(n,61,op,22);
    assert(parse5(wire,n,161,20)==MT76_RX_INVALID); /* Duplicate even if same. */
    n=high_make(false,false,false);n=append(n,3,(const uint8_t[]){161},1);
    assert(parse5(wire,n,161,20)==MT76_RX_UNSUPPORTED);
    n=high_make(false,false,true);n=append(n,99,(const uint8_t[]){1},1);
    wire[4+32+24+12+2+7+4+3+2+22+1]=255; /* malformed final unknown IE */
    assert(parse5(wire,n,161,20)==MT76_RX_INVALID);
    n=high_make(true,true,true);n=append(n,48,(const uint8_t[]){1,0},2);
    n=append(n,221,(const uint8_t[]){0,0x50,0xf2,1},4);
    struct mt76_rx_beacon5 b;
    assert(mt76_rx_beacon5(wire,n,161,20,&b)==MT76_RX_OK && b.beacon.rsn_present && b.beacon.wpa_present);
    n=append(n,48,(const uint8_t[]){1,0},2);assert(parse5(wire,n,161,20)==MT76_RX_INVALID);
    assert(parse5(NULL,0,161,20)==MT76_RX_INVALID);
    assert(mt76_rx_beacon5(wire,n,161,20,NULL)==MT76_RX_INVALID);
    uint32_t seed=161;
    for(unsigned i=0;i<100000;i++) {
        seed=seed*1664525u+1013904223u;n=seed%512;
        uint8_t *p=malloc(n?n:1);assert(p);
        for(size_t k=0;k<n;k++){seed=seed*1664525u+1013904223u;p[k]=(uint8_t)(seed>>24);}
        (void)parse5(p,n,161,20);free(p);
    }
    puts("mt76-rx5: fixed161/20 HT-primary, missing/duplicate/mismatch, truncation, unaligned, legacy isolation and100k hostile transfers PASS;no hardware acceptance");
    return 0;
}
