#include <srvros/mt76_eapol5.h>
#include <srvros/wpa2_handshake.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixtures/wpa2-handshake-synthetic.h"
static const struct mt76_eapol5_peer peer={{2,6,7,8,9,10},{2,1,2,3,4,5},161,20};
static void zero(const void *p,size_t n){const uint8_t *v=p;for(size_t i=0;i<n;i++)assert(!v[i]);}
static void le16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void le32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static unsigned get16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static size_t rx(uint8_t *out,const uint8_t *frame,size_t n,bool qos,bool pad){
 unsigned header=qos?26:24,mpdu=header+8+(unsigned)n,dma=(32+mpdu+(pad?2:0)+3)&~3u;memset(out,0,dma+4);
 le16(out,dma);le32(out+4,2u|(pad?1u<<14:0));le32(out+8,mpdu<<16);
 uint8_t *h=out+36;le16(h,qos?0x0288:0x0208);memcpy(h+4,hs_spa,6);memcpy(h+10,hs_aa,6);memcpy(h+16,hs_aa,6);le16(h+22,0x1230);if(qos)h[24]=7;
 if(pad){h[header]=0xa5;h[header+1]=0x5a;}
 static const uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};memcpy(h+header+(pad?2:0),llc,8);memcpy(h+header+(pad?2:0)+8,frame,n);return dma+4;
}
static void refuses(uint8_t *p,size_t n){struct mt76_eapol5_view v;memset(&v,0xa5,sizeof v);assert(mt76_eapol5_rx(&peer,p,n,&v)!=MT76_RX_OK);zero(&v,sizeof v);}
int main(void){
 assert(mt76_eapol5_m1(hs_m1,sizeof hs_m1) && !mt76_eapol5_m1(hs_m3,sizeof hs_m3) && !mt76_eapol5_m1(NULL,99));
 uint8_t m1[121];memcpy(m1,hs_m1,99);for(unsigned i=49;i<97;i++){m1[i]=1;assert(!mt76_eapol5_m1(m1,99));m1[i]=0;}
 memset(m1+17,0,32);assert(!mt76_eapol5_m1(m1,99));memset(m1+17,255,32);assert(!mt76_eapol5_m1(m1,99));memcpy(m1,hs_m1,99);memset(m1+9,255,8);assert(!mt76_eapol5_m1(m1,99));
 uint8_t packet[4096],copy[4096],out[4096];struct mt76_eapol5_view v;
 for(unsigned qos=0;qos<2;qos++)for(unsigned pad=0;pad<2;pad++)for(unsigned kind=0;kind<2;kind++){
  const uint8_t *frame=kind?hs_m3:hs_m1;size_t size=kind?sizeof hs_m3:sizeof hs_m1;
  size_t n=rx(packet,frame,size,qos,pad);memcpy(copy,packet,n);
  assert(mt76_eapol5_rx(&peer,packet,n,&v)==MT76_RX_OK && v.qos==!!qos && v.tid==(qos?7:0) && v.eapol_size==size && v.sequence==0x123 && v.consumed==n);
  assert(!memcmp(v.eapol,frame,size) && !memcmp(packet,copy,n));
  struct mt76_rx_view old;assert(mt76_rx_usb_view(packet,n,&old)==MT76_RX_UNSUPPORTED);
  for(size_t shortn=0;shortn<n;shortn++){uint8_t *p=malloc(shortn?shortn:1);memcpy(p,packet,shortn);refuses(p,shortn);free(p);}
 }
 size_t n=rx(packet,hs_m3,sizeof hs_m3,true,true);memcpy(copy,packet,n);
 /* Complete FC/metadata words establish the exact accepted header profile. */
 for(unsigned fc=0;fc<65536;fc++){
  le16(packet+36,fc);bool ok=(fc&~0x2800u)==0x0288;
  assert((mt76_eapol5_rx(&peer,packet,n,&v)==MT76_RX_OK)==ok);if(!ok)zero(&v,sizeof v);
 }memcpy(packet,copy,n);
 for(unsigned qc=0;qc<65536;qc++){
  le16(packet+60,qc);bool ok=!(qc&~0x17u);assert((mt76_eapol5_rx(&peer,packet,n,&v)==MT76_RX_OK)==ok);if(!ok)zero(&v,sizeof v);
 }memcpy(packet,copy,n);
 const unsigned blocked[]={0,2,3,8,9,10,11,12,15,16,18,19,20,21};
 for(unsigned i=0;i<sizeof blocked/sizeof blocked[0];i++){le32(packet+4,2u|(1u<<14)|(1u<<blocked[i]));refuses(packet,n);}memcpy(packet,copy,n);
 for(unsigned i=4;i<22;i++)for(unsigned bit=0;bit<8;bit++){packet[36+i]^=1u<<bit;refuses(packet,n);packet[36+i]^=1u<<bit;}
 for(unsigned i=0;i<8;i++)for(unsigned bit=0;bit<8;bit++){packet[64+i]^=1u<<bit;refuses(packet,n);packet[64+i]^=1u<<bit;}
 for(unsigned frag=1;frag<16;frag++){packet[58]=(uint8_t)(0x30|frag);refuses(packet,n);}memcpy(packet,copy,n);
 for(unsigned i=0;i<sizeof hs_m3;i++)if(i<9 || i==97 || i==98){packet[72+i]^=1;refuses(packet,n);packet[72+i]^=1;}
 for(unsigned i=0;i<n;i++){memcpy(out,packet,n);assert(mt76_eapol5_rx(&peer,out,n,(void *)(out+i))==MT76_RX_INVALID);assert(!memcmp(out,packet,n));}
 struct mt76_eapol5_peer bad=peer,save=peer;assert(mt76_eapol5_rx(&bad,packet,n,(void *)&bad)==MT76_RX_INVALID && !memcmp(&bad,&save,sizeof bad));
 bad.channel=1;assert(mt76_eapol5_rx(&bad,packet,n,&v)==MT76_RX_INVALID);zero(&v,sizeof v);bad=peer;bad.bandwidth=40;assert(mt76_eapol5_rx(&bad,packet,n,&v)==MT76_RX_INVALID);zero(&v,sizeof v);
 assert(mt76_eapol5_rx(NULL,packet,n,&v)==MT76_RX_INVALID);zero(&v,sizeof v);assert(mt76_eapol5_rx(&peer,NULL,n,&v)==MT76_RX_INVALID);zero(&v,sizeof v);assert(mt76_eapol5_rx(&peer,packet,n,NULL)==MT76_RX_INVALID);
 for(unsigned kind=0;kind<2;kind++){
  const uint8_t *f=kind?hs_m4:hs_m2;size_t size=kind?sizeof hs_m4:sizeof hs_m2,total=kind?160:184;
  assert(mt76_eapol5_usb_tx(&peer,f,size,4+kind,out,sizeof out)==total);zero(out+total,sizeof out-total);
  assert(get16(out)==total-8 && out[2]==8 && out[3]==5 && get16(out+6)==0x2000 && out[8]==1 && out[9]==255 && get16(out+10)==size+32 && out[23]==128);
  assert(get16(out+24)==0x0108 && get16(out+26)==60 && get16(out+46)==((4+kind)<<4));assert(!memcmp(out+28,hs_aa,6) && !memcmp(out+34,hs_spa,6) && !memcmp(out+40,hs_aa,6));assert(!memcmp(out+56,f,size));
  for(unsigned cap=1;cap<total;cap++){memset(out,0xa5,sizeof out);assert(!mt76_eapol5_usb_tx(&peer,f,size,4+kind,out,cap));zero(out,cap);assert(out[cap]==0xa5);}
  for(size_t trunc=0;trunc<size;trunc++){uint8_t *p=malloc(trunc?trunc:1);memcpy(p,f,trunc);memset(out,0xa5,184);assert(!mt76_eapol5_usb_tx(&peer,p,trunc,4,out,184));zero(out,184);free(p);}
 }
 /* Maximum bounded EAPOL and ignored trailing USB bytes never extend a view. */
 uint8_t large[1124]={2,3};large[2]=4;large[3]=95;large[4]=2;large[5]=0x13;large[6]=0xca;large[8]=16;large[97]=4;
 n=rx(packet,large,1123,true,true);assert(mt76_eapol5_rx(&peer,packet,n,&v)==MT76_RX_OK && v.eapol_size==1123);
 memset(packet+n,0xff,64);assert(mt76_eapol5_rx(&peer,packet,n+64,&v)==MT76_RX_OK && v.consumed==n);
 large[3]=96;large[98]=1;n=rx(packet,large,1124,false,false);refuses(packet,n);
 /* Fixed EAPOL fields refuse before TX publication;MIC bytes themselves are
  * opaque here and must already be bound by the independent handshake owner. */
 uint8_t changed[121];
 const unsigned invalid_fields[]={0,1,2,3,4,5,6,7,8,49,64,65,72,73,80,97,98,99,120};
 for(unsigned i=0;i<sizeof invalid_fields/sizeof invalid_fields[0];i++){
  memcpy(changed,hs_m2,121);changed[invalid_fields[i]]^=1;assert(!mt76_eapol5_usb_tx(&peer,changed,121,4,out,184));zero(out,184);
 }
 memcpy(changed,hs_m2,121);changed[81]^=1;assert(mt76_eapol5_usb_tx(&peer,changed,121,4,out,184)==184);
 memcpy(changed,hs_m4,99);changed[17]=1;assert(!mt76_eapol5_usb_tx(&peer,changed,99,5,out,184));zero(out,184);
 uint32_t rng=0x76100002;
 for(unsigned sample=0;sample<100000;sample++){
  rng=rng*1664525u+1013904223u;size_t len=rng%sizeof packet;
  for(size_t j=0;j<len;j++){rng=rng*1664525u+1013904223u;packet[j]=(uint8_t)(rng>>24);}
  enum mt76_rx_status result=mt76_eapol5_rx(&peer,packet,len,&v);
  if(result==MT76_RX_OK){assert(v.eapol>=packet && v.eapol+v.eapol_size<=packet+v.consumed && v.consumed<=len && v.eapol_size<=1123);}else zero(&v,sizeof v);
 }
 /* Model->EAPOL USB->synthetic AP RX->model composition keeps private bytes. */
 struct wpa2_handshake s={0};struct wpa2_hs_peer hp={0};memcpy(hp.aa,hs_aa,6);memcpy(hp.spa,hs_spa,6);memcpy(hp.rsne,hs_rsne,sizeof hs_rsne);hp.rsne_size=sizeof hs_rsne;
 assert(wpa2_hs_start(&s,&hp,hs_pmk,hs_sn,7));n=rx(packet,hs_m1,sizeof hs_m1,true,true);assert(mt76_eapol5_rx(&peer,packet,n,&v)==MT76_RX_OK);assert(wpa2_hs_receive(&s,7,hs_aa,hs_spa,v.eapol,v.eapol_size));
 uint8_t e[121];assert(wpa2_hs_emit(&s,7,e,sizeof e)==121 && mt76_eapol5_usb_tx(&peer,e,121,4,out,184)==184);assert(wpa2_hs_tx_result(&s,7,WPA2_HS_CONFIRMED));n=rx(packet,hs_m3,sizeof hs_m3,false,false);assert(mt76_eapol5_rx(&peer,packet,n,&v)==MT76_RX_OK);assert(wpa2_hs_receive(&s,7,hs_aa,hs_spa,v.eapol,v.eapol_size));assert(s.phase==WPA2_HS_KEYS_READY && !s.install_handoffs);assert(wpa2_hs_retire(&s,7,false));
 for(unsigned seq=0;seq<4098;seq++)if(!seq || seq>4095){assert(!mt76_eapol5_usb_tx(&peer,hs_m2,sizeof hs_m2,seq,out,184));zero(out,184);}
 memcpy(out,hs_m2,sizeof hs_m2);memcpy(copy,out,sizeof hs_m2);assert(!mt76_eapol5_usb_tx(&peer,out,sizeof hs_m2,4,out,184));assert(!memcmp(out,copy,sizeof hs_m2));
 assert(!mt76_eapol5_usb_tx(&peer,hs_m1,sizeof hs_m1,4,out,184));zero(out,184);assert(!mt76_eapol5_usb_tx(&peer,hs_m3,sizeof hs_m3,4,out,184));zero(out,184);
 puts("mt76-eapol5: PASS separate data/QoS/L2PAD RX,exact peer/LLC/envelope,131072 FC/QoS words,truncation/alias/metadata refusal,M2/M4 USB framing and handshake composition;old management gates unchanged;no native/key authority");return 0;
}
