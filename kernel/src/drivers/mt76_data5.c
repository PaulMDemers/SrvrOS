#include <srvros/mt76_data5.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static void copy(uint8_t *d,const uint8_t *s,size_t n){while(n--)*d++=*s++;}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned d=0;while(n--)d|=*a++^*b++;return !d;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){if(!an || !bn)return false;uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static unsigned le16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t le32(const uint8_t *p){return le16(p)|(uint32_t)le16(p+2)<<16;}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static bool protocol(const uint8_t *p){return p[0]==8 && (p[1]==0 || p[1]==6);}
static bool admission(struct mt76_data5_scratch *s,struct wpa2_ccmp *k,uint64_t g,const uint8_t *in,size_t n,uint8_t *out,size_t cap,size_t max){
 return s && k && g && k->generation==g && k->phase==WPA2_CCMP_ACTIVE && out && cap && cap<=max &&
 !overlap(s,sizeof *s,k,sizeof *k) && !overlap(s,sizeof *s,out,cap) && !overlap(k,sizeof *k,out,cap) &&
 (!in || (!overlap(in,n,out,cap) && !overlap(s,sizeof *s,in,n) && !overlap(k,sizeof *k,in,n)));
}
static const uint8_t snap[6]={0xaa,0xaa,3,0,0,0};
size_t mt76_data5_tx(struct mt76_data5_scratch *s,struct wpa2_ccmp *k,uint64_t g,const uint8_t *eth,size_t n,unsigned seq,uint8_t *out,size_t cap){
 if(!admission(s,k,g,eth,n,out,cap,4096))return 0;
 scrub(out,cap);scrub(s,sizeof *s);size_t result=0;
 if(!eth || n<14 || n>MT76_DATA5_ETH_MAX || !seq || seq>4095 || !equal(eth+6,k->peer.spa,6) || !protocol(eth+12))goto done;
 size_t plain=n-6,mpdu=24+plain+16,payload=(20+mpdu+3)&~(size_t)3,total=payload+8;
 if(total>cap || total>MT76_DATA5_USB_MAX)goto done;
 uint8_t h[24]={0};put16(h,0x0108);put16(h+2,60);put16(h+22,seq<<4);
 copy(h+4,k->peer.aa,6);copy(h+10,k->peer.spa,6);copy(h+16,eth,6);
 copy(s->plain,snap,6);copy(s->plain+6,eth+12,n-12);
 size_t sealed=wpa2_ccmp_seal(k,g,h,s->plain,plain,s->mpdu,sizeof s->mpdu);scrub(h,sizeof h);
 if(sealed!=mpdu)goto done;
 uint32_t info=(uint32_t)payload|(1u<<19)|(1u<<24)|(2u<<25);
 for(unsigned i=0;i<4;i++)out[i]=(uint8_t)(info>>(8*i));
 /* Pinned Linux v7.0 mt76x02:software IV/WIV,wcidff,no hardware cipher.
  * A normal 6Mbps ACK-requesting nonaggregate MPDU;IV+MIC already sealed. */
 put16(out+6,0x2000);out[8]=1;out[9]=255;put16(out+10,(unsigned)mpdu);out[23]=128;
 copy(out+24,s->mpdu,mpdu);result=total;
done:scrub(s,sizeof *s);return result;
}
size_t mt76_data5_rx(struct mt76_data5_scratch *s,struct wpa2_ccmp *k,uint64_t g,const uint8_t *usb,size_t n,uint8_t *out,size_t cap){
 if(!admission(s,k,g,usb,n,out,cap,MT76_DATA5_ETH_MAX))return 0;
 scrub(out,cap);scrub(s,sizeof *s);size_t result=0;
 if(!usb || n<36+24+16+8 || n>4096)goto done;
 unsigned dma=le16(usb);if(!dma || (dma&3) || dma>n-4 || dma<32)goto done;
 const uint8_t *wi=usb+4;uint32_t info=le32(wi);size_t mpdu=(le32(wi+4)>>16)&0x3fff,pad=(info&(1u<<14))?2:0;
 if(info&(1u|(1u<<2)|(1u<<3)|(7u<<8)|(1u<<11)|(1u<<12)|(1u<<15)|(1u<<16)|(1u<<18)|(7u<<19)))goto done;
 if(mpdu<24+16+8 || mpdu>sizeof s->mpdu || mpdu>dma-32 || pad>dma-32-mpdu)goto done;
 const uint8_t *h=wi+32;unsigned fc=le16(h),base=fc&~0x3800u;bool qos=base==0x4288;
 if(base!=0x4208 && !qos)goto done;
 size_t header=qos?26:24;if(mpdu<header+16+8 || mpdu-header-16+6>cap || (le16(h+22)&15))goto done;
 copy(s->mpdu,h,header);copy(s->mpdu+header,h+header+pad,mpdu-header);
 size_t plain=wpa2_ccmp_open(k,g,s->mpdu,mpdu,s->plain,sizeof s->plain);
 if(plain<8 || plain>1508 || plain+6>cap || !equal(s->plain,snap,6) || !protocol(s->plain+6))goto done;
 copy(out,h+4,6);copy(out+6,h+16,6);copy(out+12,s->plain+6,plain-6);result=plain+6;
done:scrub(s,sizeof *s);return result;
}
