#include <srvros/mt76_eapol5.h>
#include <srvros/mt76_rsn.h>
static void scrub(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){if(!an || !bn)return false;uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned d=0;for(size_t i=0;i<n;i++)d|=a[i]^b[i];return !d;}
static bool zero(const uint8_t *p,size_t n){unsigned v=0;for(size_t i=0;i<n;i++)v|=p[i];return !v;}
static bool unicast(const uint8_t *p){return !(p[0]&1) && !zero(p,6);}
static bool peer_ok(const struct mt76_eapol5_peer *p){return p && p->channel==161 && p->bandwidth==20 && unicast(p->station) && unicast(p->bssid) && !equal(p->station,p->bssid,6);}
static unsigned le16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t le32(const uint8_t *p){return le16(p)|(uint32_t)le16(p+2)<<16;}
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static bool eapol(const uint8_t *p,size_t n){return p && n>=99 && n<=1123 && (p[0]==1 || p[0]==2) && p[1]==3 && be16(p+2)==n-4 && p[4]==2 && be16(p+97)==n-99;}
static const uint8_t llc[8]={0xaa,0xaa,3,0,0,0,0x88,0x8e};
enum mt76_rx_status mt76_eapol5_rx(const struct mt76_eapol5_peer *peer,const uint8_t *p,size_t size,struct mt76_eapol5_view *out){
 if(!out)return MT76_RX_INVALID;
 if((peer && overlap(out,sizeof *out,peer,sizeof *peer)) || (p && overlap(out,sizeof *out,p,size)))return MT76_RX_INVALID;
 scrub(out,sizeof *out);
 if(!peer_ok(peer) || !p || size<36+24+8+99 || size>4096)return MT76_RX_INVALID;
 unsigned dma=le16(p);if(!dma || (dma&3) || dma>size-4 || dma<32)return MT76_RX_INVALID;
 const uint8_t *wi=p+4;uint32_t info=le32(wi);size_t mpdu=(le32(wi+4)>>16)&0x3fff,pad=(info&(1u<<14))?2:0;
 if(mpdu<24 || mpdu>dma-32 || pad>dma-32-mpdu)return MT76_RX_INVALID;
 if(info&((7u<<8)|(1u<<3)))return MT76_RX_INVALID;
 if(info&(1u|(1u<<2)|(1u<<11)|(1u<<12)|(1u<<15)|(1u<<16)|(1u<<18)|(7u<<19)))return MT76_RX_UNSUPPORTED;
 const uint8_t *h=wi+32;unsigned fc=le16(h),base=fc&~0x2800u;bool qos=base==0x0288;
 if(base!=0x0208 && !qos)return MT76_RX_UNSUPPORTED;
 unsigned header=qos?26:24;
 if(mpdu<header+8+99 || (le16(h+22)&15))return MT76_RX_INVALID;
 if(!equal(h+4,peer->station,6) || !equal(h+10,peer->bssid,6) || !equal(h+16,peer->bssid,6))return MT76_RX_INVALID;
 unsigned qc=qos?le16(h+24):0;if(qc&~0x17u)return MT76_RX_UNSUPPORTED;
 const uint8_t *body=h+header+pad;size_t n=mpdu-header-8;
 if(!equal(body,llc,8) || !eapol(body+8,n) || be16(body+8+7)!=16)return MT76_RX_INVALID;
 unsigned flags=be16(body+8+5);if(flags!=0x008a && flags!=0x13ca)return MT76_RX_UNSUPPORTED;
 *out=(struct mt76_eapol5_view){.eapol=body+8,.eapol_size=n,.mpdu_size=mpdu,.consumed=dma+4,.rxinfo=info,.frame_control=(uint16_t)fc,.sequence=(uint16_t)(le16(h+22)>>4),.tid=(uint8_t)(qc&7),.qos=qos};return MT76_RX_OK;
}
size_t mt76_eapol5_usb_tx(const struct mt76_eapol5_peer *peer,const uint8_t *frame,size_t n,unsigned sequence,uint8_t *out,size_t cap){
 if(!out || !cap || cap>4096 || (peer && overlap(out,cap,peer,sizeof *peer)) || (frame && overlap(out,cap,frame,n)))return 0;
 scrub(out,cap);
 if(!peer_ok(peer) || !sequence || sequence>4095 || !eapol(frame,n) || be16(frame+7) || !zero(frame+49,32))return 0;
 unsigned flags=be16(frame+5);uint8_t station[22];mt76_rsn_station_ie(station,sizeof station);
 bool m2=n==121 && flags==0x010a && !zero(frame+17,32) && equal(frame+99,station,22);
 bool m4=n==99 && flags==0x030a && zero(frame+17,32);
 if(!m2 && !m4)return 0;
 size_t mpdu=24+8+n,payload=(20+mpdu+3)&~(size_t)3,total=payload+8;if(cap<total)return 0;
 uint8_t packet[184]={0};uint32_t info=(uint32_t)payload|(1u<<19)|(1u<<24)|(2u<<25);
 for(unsigned i=0;i<4;i++)packet[i]=(uint8_t)(info>>(8*i));
 put16(packet+6,0x2000);packet[8]=1;packet[9]=255;put16(packet+10,(unsigned)mpdu);packet[23]=128;
 uint8_t *h=packet+24;put16(h,0x0108);put16(h+2,60);put16(h+22,sequence<<4);
 for(unsigned i=0;i<6;i++){h[4+i]=peer->bssid[i];h[10+i]=peer->station[i];h[16+i]=peer->bssid[i];}
 for(unsigned i=0;i<8;i++)h[24+i]=llc[i];
 for(size_t i=0;i<n;i++)h[32+i]=frame[i];
 for(size_t i=0;i<total;i++)out[i]=packet[i];
 scrub(packet,sizeof packet);scrub(station,sizeof station);return total;
}

bool mt76_eapol5_m1(const uint8_t *p,size_t n){
 static const uint8_t pmkid[]={221,20,0,15,172,4};uint8_t nonff=0;
 if(!eapol(p,n) || be16(p+5)!=0x008a || be16(p+7)!=16 || !zero(p+49,48) ||
 (n!=99 && (n!=121 || !equal(p+99,pmkid,sizeof pmkid))) || zero(p+17,32))return false;
 for(unsigned i=0;i<32;i++)nonff|=p[17+i]^255;
 bool room=false;for(unsigned i=0;i<8;i++)room=room || p[9+i]!=255;
 return nonff && room;
}
