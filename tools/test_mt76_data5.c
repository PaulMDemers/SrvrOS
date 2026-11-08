#include <srvros/mt76_data5.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixtures/wpa2-handshake-synthetic.h"
#include "fixtures/wpa2-ccmp-synthetic.h"
static struct mt76_data5_scratch scratch;
static struct wpa2_ccmp keys,base;
static struct wpa2_handshake handshake;
static void zero(const void *p,size_t n){const uint8_t *v=p;while(n--)assert(!*v++);}
static void setup(void){
 memset(&keys,0,sizeof keys);memset(&handshake,0,sizeof handshake);
 struct wpa2_hs_peer p={.rsne_size=sizeof hs_rsne};memcpy(p.aa,hs_aa,6);memcpy(p.spa,hs_spa,6);memcpy(p.rsne,hs_rsne,sizeof hs_rsne);
 assert(wpa2_hs_start(&handshake,&p,hs_pmk,hs_sn,9));
 assert(wpa2_hs_receive(&handshake,9,hs_aa,hs_spa,hs_m1,sizeof hs_m1));uint8_t b[121];
 assert(wpa2_hs_emit(&handshake,9,b,sizeof b)==121 && wpa2_hs_tx_result(&handshake,9,WPA2_HS_CONFIRMED));
 assert(wpa2_hs_receive(&handshake,9,hs_aa,hs_spa,hs_m3,sizeof hs_m3));struct wpa2_hs_keys k;
 assert(wpa2_hs_take_keys(&handshake,9,&k) && wpa2_ccmp_install(&keys,&handshake,&k));memset(&k,0,sizeof k);
 assert(wpa2_hs_install_result(&handshake,9,WPA2_HS_CONFIRMED));
 assert(wpa2_hs_emit(&handshake,9,b,sizeof b)==99 && wpa2_hs_tx_result(&handshake,9,WPA2_HS_CONFIRMED));
 assert(wpa2_ccmp_activate(&keys,&handshake));base=keys;
}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static size_t usb_rx(uint8_t *out,const uint8_t *mpdu,size_t n,bool pad){
 size_t header=(mpdu[0]&0x80)?26:24,bytes=(32+n+(pad?2:0)+3)&~(size_t)3;
 memset(out,0,4096);put16(out,(unsigned)bytes);out[4]=0x92;out[5]=0x20|(pad?0x40:0);put16(out+10,(unsigned)n);
 memcpy(out+36,mpdu,header);memcpy(out+36+header+(pad?2:0),mpdu+header,n-header);return bytes+4;
}
static void metadata_tests(void){
 uint8_t usb[4096],out[1514];
 const struct {unsigned fc,want;} cases[]={
  {0x80,1},{0x50,2},{0xb0,3},{0x10,4},{0x30,4},{0xc0,5},{0xa0,6},{0xd0,7},{0x40,8},
  {0xd4,9},{0x0208,10},{0x4208,12},{0x4288,12},{0x4108,13},{0x0003,14}};
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
  memset(usb,0,sizeof usb);put16(usb,232);put16(usb+10,200);put16(usb+36,cases[i].fc);keys=base;
  struct mt76_data5_rx_result r=mt76_data5_rx_report(&scratch,&keys,9,usb,236,out,sizeof out);
  assert(!r.bytes && !r.flags && r.frame==cases[i].want && !memcmp(&keys,&base,sizeof keys));zero(out,sizeof out);zero(&scratch,sizeof scratch);
 }
 /* Independent pinned RXWI bit positions;all three PN-length bits map to
  * the same category. Neither permitted flags nor malformed spans relax RX. */
 const unsigned bits[]={0,2,3,8,9,10,11,12,15,16,18,19,20,21};
 for(unsigned i=0;i<14;i++){
  memset(usb,0,sizeof usb);put16(usb,232);put16(usb+10,200);put16(usb+36,0x4208);usb[4+bits[i]/8]=1u<<(bits[i]%8);keys=base;
  struct mt76_data5_rx_result r=mt76_data5_rx_report(&scratch,&keys,9,usb,236,out,sizeof out);
  assert(!r.bytes && r.reason==MT76_DATA5_FLAGS && r.frame==12 && r.flags==(1u<<(i<12?i:11)) && !memcmp(&keys,&base,sizeof keys));
 }
 memset(usb,0,sizeof usb);put16(usb,232);put16(usb+10,200);put16(usb+36,0x0208);
 memcpy(usb+60,(uint8_t[]){0xaa,0xaa,3,0,0,0,0x88,0x8e,2,3,0,95},12);
 keys=base;struct mt76_data5_rx_result r=mt76_data5_rx_report(&scratch,&keys,9,usb,236,out,sizeof out);assert(!r.bytes && r.frame==11 && r.reason==MT76_DATA5_FRAME);
 for(unsigned i=0;i<12;i++){
  uint8_t old=usb[60+i];usb[60+i]^=0x80;keys=base;r=mt76_data5_rx_report(&scratch,&keys,9,usb,236,out,sizeof out);
  assert(!r.bytes && r.frame==10 && !memcmp(&keys,&base,sizeof keys));usb[60+i]=old;
 }
 /* QoS + declared L2 padding is classified at its own bounded header end. */
 memset(usb+60,0,176);put16(usb+36,0x0288);usb[5]=0x40;put16(usb,236);
 memcpy(usb+64,(uint8_t[]){0xaa,0xaa,3,0,0,0,0x88,0x8e,2,3,0,95},12);
 keys=base;r=mt76_data5_rx_report(&scratch,&keys,9,usb,240,out,sizeof out);assert(!r.bytes && r.frame==11);
 for(size_t n=0;n<240;n++){
  keys=base;r=mt76_data5_rx_report(&scratch,&keys,9,usb,n,out,sizeof out);assert(!r.bytes && !r.flags && r.frame==0 && !memcmp(&keys,&base,sizeof keys));
 }
 keys=base;r=mt76_data5_rx_report(&scratch,&keys,8,usb,240,out,sizeof out);assert(!r.bytes && !r.flags && !r.frame);
 /* Independently valid transport with missing/oversized MPDU remains unknown. */
 for(unsigned length=0;length<2;length++){put16(usb+10,length);keys=base;r=mt76_data5_rx_report(&scratch,&keys,9,usb,240,out,sizeof out);assert(!r.bytes && !r.frame);}
 put16(usb+10,300);keys=base;r=mt76_data5_rx_report(&scratch,&keys,9,usb,240,out,sizeof out);assert(!r.bytes && !r.frame);
}
int main(void){
 setup();metadata_tests();uint8_t eth[1514]={0},usb[4096],out[1514],bad[4096];
 memcpy(eth,ccmp_tx_header+16,6);memcpy(eth+6,hs_spa,6);memcpy(eth+12,ccmp_data+6,sizeof ccmp_data-6);size_t n=sizeof ccmp_data+6;
 size_t size=mt76_data5_tx(&scratch,&keys,9,eth,n,0x123,usb,sizeof usb);
 assert(size==((20+sizeof ccmp_tx+3)&~3u)+8 && keys.tx_pn==1);
 uint8_t expected[sizeof ccmp_tx];memcpy(expected,ccmp_tx,sizeof expected);expected[2]=60;
 assert(!memcmp(usb+24,expected,sizeof expected));zero(&scratch,sizeof scratch);
 assert(usb[6]==0 && usb[7]==0x20 && usb[8]==1 && usb[9]==255 && usb[23]==128);
 zero(usb+24+sizeof expected,size-24-sizeof expected);
 assert(mt76_data5_tx(&scratch,&keys,9,eth,n,0x124,usb,sizeof usb)==size && keys.tx_pn==2);
 for(unsigned group=0;group<2;group++)for(unsigned pad=0;pad<2;pad++){
  const uint8_t *p=group?ccmp_group:ccmp_rx;size_t count=group?sizeof ccmp_group:sizeof ccmp_rx;
  keys=base;size=usb_rx(usb,p,count,pad);
  assert(mt76_data5_rx(&scratch,&keys,9,usb,size,out,sizeof out)==n);
  assert(!memcmp(out,p+4,6) && !memcmp(out+6,p+16,6) && !memcmp(out+12,ccmp_data+6,n-12));zero(out+n,sizeof out-n);zero(&scratch,sizeof scratch);
  struct wpa2_ccmp used=keys;assert(!mt76_data5_rx(&scratch,&keys,9,usb,size,out,sizeof out));zero(out,sizeof out);assert(!memcmp(&keys,&used,sizeof keys));
 }
 /* Sole WAPI-key flag stays refused even with valid peer/MIC. The same
  * original synthetic key context and replay floors survive every audit. */
 for(unsigned group=0;group<2;group++)for(unsigned pad=0;pad<2;pad++){
  const uint8_t *packet=group?ccmp_group:ccmp_rx;size_t count=group?sizeof ccmp_group:sizeof ccmp_rx;
  keys=base;size=usb_rx(usb,packet,count,pad);usb[6]|=4;
  for(unsigned repeat=0;repeat<2;repeat++){
   struct mt76_data5_rx_result a=mt76_data5_rx_report(&scratch,&keys,9,usb,size,out,sizeof out);
   assert(!a.bytes && a.reason==MT76_DATA5_FLAGS && a.flags==1024 && a.audit==MT76_DATA5_AUDIT_VERIFIED && !memcmp(&keys,&base,sizeof keys));zero(out,sizeof out);zero(&scratch,sizeof scratch);
  }
  memcpy(bad,usb,size);bad[46]^=2;struct mt76_data5_rx_result a=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(a.audit==MT76_DATA5_AUDIT_PEER && !memcmp(&keys,&base,sizeof keys));
  memcpy(bad,usb,size);bad[36+(pad?2:0)+count-1]^=1;a=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(a.audit==MT76_DATA5_AUDIT_CCMP && !memcmp(&keys,&base,sizeof keys));
  memcpy(bad,usb,size);bad[4]|=1;a=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(a.audit==MT76_DATA5_AUDIT_NONE && !memcmp(&keys,&base,sizeof keys));
  memcpy(bad,usb,size);put16(bad+36,0x80);a=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(a.audit==MT76_DATA5_AUDIT_HEADER && !memcmp(&keys,&base,sizeof keys));
  usb[6]&=~4u;assert(mt76_data5_rx(&scratch,&keys,9,usb,size,out,sizeof out)==n);
  struct wpa2_ccmp used=keys;usb[6]|=4;a=mt76_data5_rx_report(&scratch,&keys,9,usb,size,out,sizeof out);assert(!a.bytes && a.audit==MT76_DATA5_AUDIT_CCMP && !memcmp(&keys,&used,sizeof keys));zero(out,sizeof out);
 }
 /* Classifications share the same parser and cryptographic acceptance. */
 keys=base;size=usb_rx(usb,ccmp_rx,sizeof ccmp_rx,false);
 struct mt76_data5_rx_result r=mt76_data5_rx_report(&scratch,&keys,9,usb,size,out,sizeof out);
 assert(r.bytes==n && r.reason==MT76_DATA5_OK && !r.flags && r.frame==MT76_DATA5_PROTECTED_FROMDS);
 r=mt76_data5_rx_report(&scratch,&keys,9,usb,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_CCMP);zero(out,sizeof out);
 keys=base;r=mt76_data5_rx_report(&scratch,&keys,8,usb,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_ADMISSION);
 keys=base;r=mt76_data5_rx_report(&scratch,&keys,9,usb,12,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_SPAN);zero(out,sizeof out);
 keys=base;memcpy(bad,usb,size);bad[4]|=1;r=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_FLAGS);
 keys=base;memcpy(bad,usb,size);bad[10]=0;bad[11]=0;r=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_SHAPE);
 keys=base;memcpy(bad,usb,size);put16(bad+36,0x80);r=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_FRAME);
 keys=base;memcpy(bad,usb,size);bad[58]|=1;r=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_HEADER);
 keys=base;memcpy(bad,usb,size);bad[46]^=2;r=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_PEER);
 keys=base;memcpy(bad,usb,size);bad[36+sizeof ccmp_rx-1]^=1;r=mt76_data5_rx_report(&scratch,&keys,9,bad,size,out,sizeof out);assert(!r.bytes && r.reason==MT76_DATA5_CCMP);
 zero(&scratch,sizeof scratch);zero(out,sizeof out);assert(keys.rx_pairwise[16]==0);
 size=usb_rx(usb,ccmp_rx,sizeof ccmp_rx,true);
 for(unsigned off=0;off<size;off++){
  if(off>=12 && off<36)continue; /* unrelated RXWI metadata */
  if(off==38 || off==39 || off==58 || off==59 || off==60 || off==61 || off>=36+2+sizeof ccmp_rx)continue;
  for(unsigned bit=0;bit<8;bit++){
   /* DMA word high bytes/reserved bits and RX classification aren't a cipher input. */
   if(off==2 || off==3 || (off>=4 && off<=9) || off==11 || (off==37 && bit>=3 && bit<=5))continue;
   keys=base;memcpy(bad,usb,size);bad[off]^=1u<<bit;
   size_t got=mt76_data5_rx(&scratch,&keys,9,bad,size,out,sizeof out);
   if(got){/* RX DMA length can include extra transport padding only. */assert(off==0 || off==1);}
   else {zero(out,sizeof out);assert(keys.rx_pairwise[16]==0);}
   zero(&scratch,sizeof scratch);
  }
 }
 for(unsigned bit=0;bit<32;bit++)if((1u|(1u<<2)|(1u<<3)|(7u<<8)|(1u<<11)|(1u<<12)|(1u<<15)|(1u<<16)|(1u<<18)|(7u<<19))&(1u<<bit)){
  keys=base;memcpy(bad,usb,size);bad[4+bit/8]|=1u<<(bit%8);assert(!mt76_data5_rx(&scratch,&keys,9,bad,size,out,sizeof out));assert(!keys.rx_pairwise[16]);
 }
 for(size_t len=0;len<size;len++){
  uint8_t *shortened=malloc(len?len:1);memcpy(shortened,usb,len);keys=base;
  assert(!mt76_data5_rx(&scratch,&keys,9,shortened,len,out,sizeof out));zero(out,sizeof out);assert(!keys.rx_pairwise[16]);free(shortened);
 }
 for(size_t cap=1;cap<n;cap++){keys=base;memset(out,0xa5,sizeof out);assert(!mt76_data5_rx(&scratch,&keys,9,usb,size,out,cap));zero(out,cap);assert(out[cap]==0xa5);}
 for(size_t cap=1;cap<((20+sizeof ccmp_tx+3)&~3u)+8;cap++){keys=base;memset(usb,0xa5,sizeof usb);assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,1,usb,cap));zero(usb,cap);assert(usb[cap]==0xa5 && !keys.tx_pn);}
 keys=base;keys.tx_pn=WPA2_CCMP_PN_MAX-1;assert(mt76_data5_tx(&scratch,&keys,9,eth,n,1,usb,sizeof usb));assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,2,usb,sizeof usb) && keys.tx_pn==WPA2_CCMP_PN_MAX);
 keys=base;memset(usb,0xa5,sizeof usb);assert(!mt76_data5_tx(&scratch,&keys,10,eth,n,1,usb,sizeof usb));for(unsigned i=0;i<sizeof usb;i++)assert(usb[i]==0xa5);
 assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,1,scratch.mpdu,sizeof scratch.mpdu));assert(!memcmp(&keys,&base,sizeof keys));
 assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,0,usb,sizeof usb));assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,4096,usb,sizeof usb));
 eth[6]^=2;assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,1,usb,sizeof usb) && !keys.tx_pn);eth[6]^=2;
 eth[12]=0x86;assert(!mt76_data5_tx(&scratch,&keys,9,eth,n,1,usb,sizeof usb) && !keys.tx_pn);eth[12]=8;
 for(unsigned len=14;len<=1514;len++)for(unsigned protocol=0;protocol<2;protocol++){
  keys=base;eth[13]=protocol?6:0;size=mt76_data5_tx(&scratch,&keys,9,eth,len,4095,usb,sizeof usb);
  assert(size && size<=MT76_DATA5_USB_MAX && keys.tx_pn==1);zero(&scratch,sizeof scratch);
 }
 puts("mt76-data5: PASS independent CCMP fixtures,USB TX/RX framing,raw cipher/pad/QoS/group,replay,MIC mutations,truncation,aliases,stale generation,PN exhaustion and full Ethernet lengths;no native data acceptance");
}
