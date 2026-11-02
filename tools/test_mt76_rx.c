#include <srvros/mt76_rx.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p, unsigned v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p, uint32_t v) {put16(p,v);put16(p+2,v>>16);}
static uint8_t wire[512];
static size_t make(bool pad, bool probe, const uint8_t *ssid, unsigned n) {
    memset(wire,0,sizeof wire);
    uint8_t *wi=wire+4,*h=wi+32,*b=h+24+(pad?2:0);
    put32(wi,pad?1u<<14:0);wi[12]=73;wi[13]=91;
    put16(wi+10,0x1234);put16(h,probe?0x50:0x80);
    memset(h+4,255,6);h[10]=h[16]=2;h[15]=h[21]=9;put16(h+22,0x1230);
    b[0]=0x78;b[7]=0x12;put16(b+8,100);put16(b+10,0x11);
    size_t pos=12;b[pos++]=0;b[pos++]=(uint8_t)n;
    if(n)memcpy(b+pos,ssid,n);
    pos+=n;b[pos++]=1;b[pos++]=2;b[pos++]=0x82;b[pos++]=0x84;
    b[pos++]=3;b[pos++]=1;b[pos++]=1;
    size_t mpdu=24+pos,dma=(32+mpdu+(pad?2:0)+3)&~(size_t)3;
    put16(wire,(unsigned)dma);put32(wi+4,(uint32_t)mpdu<<16);
    return dma+4;
}
static size_t append(size_t size, unsigned id, const uint8_t *ie, unsigned n) {
    unsigned mpdu=(wire[10]|(unsigned)wire[11]<<8)&0x3fff;
    unsigned pad=(wire[5]&0x40)?2:0;
    uint8_t *p=wire+36+mpdu+pad;p[0]=(uint8_t)id;p[1]=(uint8_t)n;
    if(n)memcpy(p+2,ie,n);
    mpdu+=n+2;size=(32+mpdu+pad+3)&~(size_t)3;
    put16(wire,(unsigned)size);put32(wire+8,(uint32_t)mpdu<<16);
    return size+4;
}
static void zero_output(const struct mt76_rx_beacon *b) {
    const uint8_t *p=(const uint8_t *)b;
    for(size_t i=0;i<sizeof *b;i++)assert(!p[i]);
}
static enum mt76_rx_status parse(size_t size, unsigned channel) {
    struct mt76_rx_beacon b;memset(&b,0xa5,sizeof b);
    enum mt76_rx_status s=mt76_rx_beacon(wire,size,channel,&b);
    if(s!=MT76_RX_OK)zero_output(&b);
    return s;
}
int main(void) {
    const uint8_t ssid[]={ 'a',0,'b',0xff };
    for(unsigned pad=0;pad<2;pad++)for(unsigned probe=0;probe<2;probe++) {
        size_t size=make(pad,probe,ssid,sizeof ssid);
        struct mt76_rx_beacon b;struct mt76_rx_view v;
        assert(mt76_rx_beacon(wire,size,1,&b)==MT76_RX_OK);
        assert(b.ssid_size==4 && !memcmp(b.ssid,ssid,4) && !b.hidden);
        assert(b.probe_response==(bool)probe && b.ds_present && b.channel==1);
        assert(b.interval==100 && b.timestamp==UINT64_C(0x1200000000000078));
        assert(b.sequence==0x123 && b.privacy && !b.rsn_present && !b.wpa_present);
        assert(b.bssid[0]==2 && b.transmitter[5]==9 && b.rssi_raw[0]==73);
        assert(mt76_rx_usb_view(wire,size,&v)==MT76_RX_OK && v.consumed==size);
        assert(v.rate==0x1234 && v.body==wire+60+(pad?2:0));
        uint8_t saved[512];memcpy(saved,wire,sizeof wire);
        /* Every exact allocation truncation, including headers/descriptors. */
        for(size_t n=0;n<size;n++) {
            uint8_t *p=malloc(n?n:1);assert(p);memcpy(p,wire,n);
            memset(&b,0xa5,sizeof b);
            assert(mt76_rx_beacon(p,n,1,&b)!=MT76_RX_OK);zero_output(&b);free(p);
        }
        uint8_t unaligned[513];memcpy(unaligned+1,wire,size);
        assert(mt76_rx_beacon(unaligned+1,size,1,&b)==MT76_RX_OK);
        assert(parse(size,0)==MT76_RX_INVALID && parse(size,12)==MT76_RX_INVALID);
        assert(parse(size,2)==MT76_RX_CHANNEL_MISMATCH);
        /* Data outside the declared DMA entry must not become an IE. */
        memset(wire+size,0xff,sizeof wire-size);assert(parse(sizeof wire,1)==MT76_RX_OK);
        memcpy(wire,saved,sizeof wire);
        for(unsigned bit=8;bit<=10;bit++) {
            put32(wire+4,(pad?1u<<14:0)|(1u<<bit));assert(parse(size,1)==MT76_RX_INVALID);
        }
        for(unsigned bit=0;bit<32;bit++)if((1u<<bit)&((1u<<11)|(1u<<15)|(1u<<16)|(7u<<19))) {
            put32(wire+4,(pad?1u<<14:0)|(1u<<bit));assert(parse(size,1)==MT76_RX_UNSUPPORTED);
        }
        memcpy(wire,saved,sizeof wire);put16(wire+36,0x88);assert(parse(size,1)==MT76_RX_UNSUPPORTED);
        for(unsigned bit=0;bit<16;bit++)if((1u<<bit)&0xc700) {
            memcpy(wire,saved,sizeof wire);put16(wire+36,(probe?0x50:0x80)|(1u<<bit));
            assert(parse(size,1)==MT76_RX_UNSUPPORTED);
        }
        memcpy(wire,saved,sizeof wire);wire[58]|=1;assert(parse(size,1)==MT76_RX_INVALID);
        memcpy(wire,saved,sizeof wire);wire[52]|=1;assert(parse(size,1)==MT76_RX_INVALID);
        memcpy(wire,saved,sizeof wire);wire[0]=0;wire[1]=0;assert(parse(size,1)==MT76_RX_INVALID);
        memcpy(wire,saved,sizeof wire);wire[0]|=1;assert(parse(size,1)==MT76_RX_INVALID);
        memcpy(wire,saved,sizeof wire);put32(wire+8,0x3fffu<<16);assert(parse(size,1)==MT76_RX_INVALID);
        memcpy(wire,saved,sizeof wire);put32(wire+8,23u<<16);assert(parse(size,1)==MT76_RX_INVALID);
    }
    struct mt76_rx_beacon b;
    size_t size=make(false,false,0,0);assert(mt76_rx_beacon(wire,size,1,&b)==MT76_RX_OK && b.hidden);
    uint8_t zeros[32]={0};size=make(true,false,zeros,32);assert(mt76_rx_beacon(wire,size,1,&b)==MT76_RX_OK && b.hidden && b.ssid_size==32);
    uint8_t longssid[33]={1};size=make(false,false,longssid,33);assert(parse(size,1)==MT76_RX_INVALID);
    size=make(false,false,ssid,4);size=append(size,0,ssid,4);assert(parse(size,1)==MT76_RX_INVALID);
    size=make(false,false,ssid,4);size=append(size,3,(const uint8_t[]){1},1);assert(parse(size,1)==MT76_RX_INVALID);
    size=make(false,false,ssid,4);size=append(size,48,(const uint8_t[]){1,0},2);
    size=append(size,221,(const uint8_t[]){0,0x50,0xf2,1},4);
    assert(mt76_rx_beacon(wire,size,1,&b)==MT76_RX_OK && b.rsn_present && b.wpa_present);
    size=append(size,48,(const uint8_t[]){1,0},2);assert(parse(size,1)==MT76_RX_INVALID);
    size=make(false,false,ssid,4);size=append(size,48,(const uint8_t[]){2,0},2);assert(parse(size,1)==MT76_RX_INVALID);
    size=make(false,false,ssid,4);size=append(size,99,(const uint8_t[]){7},1);
    wire[36+24+12+2+4+4+3+1]=255;assert(parse(size,1)==MT76_RX_INVALID);
    assert(mt76_rx_beacon(0,0,1,&b)==MT76_RX_INVALID);zero_output(&b);
    assert(mt76_rx_beacon(wire,size,1,0)==MT76_RX_INVALID);
    assert(mt76_rx_usb_view(wire,size,0)==MT76_RX_INVALID);
    /* Diagnostics expose only a channel after complete frame validation. */
    for(unsigned ch=1;ch<=11;ch++){
        size=make(false,false,ssid,4);wire[84]=(uint8_t)ch;
        assert(mt76_rx_advertised_channel(wire,size)==ch);
        assert(parse(size,ch==1?6:1)==MT76_RX_CHANNEL_MISMATCH);
        uint8_t saved[512];memcpy(saved,wire,sizeof wire);
        for(size_t n=0;n<size;n++)assert(!mt76_rx_advertised_channel(wire,n));
        size=append(size,3,(const uint8_t[]){(uint8_t)ch},1);assert(!mt76_rx_advertised_channel(wire,size));
        memcpy(wire,saved,sizeof wire);size=make(false,false,ssid,4);wire[84]=(uint8_t)ch;
        size=append(size,99,(const uint8_t[]){7},1);wire[36+24+12+2+4+4+3+1]=255;
        assert(!mt76_rx_advertised_channel(wire,size));
    }
    size=make(false,false,ssid,4);wire[82]=99;assert(!mt76_rx_advertised_channel(wire,size)); /* No DS IE. */
    for(unsigned ch=12;ch<=14;ch++){size=make(false,false,ssid,4);wire[84]=(uint8_t)ch;assert(!mt76_rx_advertised_channel(wire,size));}
    assert(!mt76_rx_advertised_channel(NULL,0));
    /* Deterministic hostile transfers: exact allocations catch overreads under
     * ASan. Successful outputs must point wholly within the declared DMA entry. */
    uint32_t seed=13;
    for(unsigned i=0;i<100000;i++) {
        seed=seed*1664525u+1013904223u;size_t n=seed%512;
        uint8_t *p=malloc(n?n:1);assert(p);
        for(size_t k=0;k<n;k++){seed=seed*1664525u+1013904223u;p[k]=(uint8_t)(seed>>24);}
        if(n>=40 && (i&1)){put16(p,(unsigned)((n-4)&~(size_t)3));put32(p+4,0);put32(p+8,(uint32_t)((n-36)&0x3fff)<<16);}
        struct mt76_rx_view v;
        if(mt76_rx_usb_view(p,n,&v)==MT76_RX_OK) {
            assert(v.consumed<=n && v.header>=p+36 && v.body>=v.header+24);
            assert(v.body<=p+v.consumed && v.body_size<=(size_t)(p+v.consumed-v.body));
        }
        memset(&b,0xa5,sizeof b);
        if(mt76_rx_beacon(p,n,1,&b)!=MT76_RX_OK)zero_output(&b);
        if(i<1000){unsigned ch=mt76_rx_advertised_channel(p,n);assert(ch<=11);if(ch)assert(mt76_rx_beacon(p,n,ch,&b)==MT76_RX_OK && b.ds_present);}
        free(p);
    }
    puts("MT76 receive: PASS padding,unaligned/truncated DMA,management framing,binary/hidden SSIDs,IE boundaries,100000 hostile transfers");
}
