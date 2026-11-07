#include <srvros/wpa2_ccmp.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixtures/wpa2-handshake-synthetic.h"
#include "fixtures/wpa2-ccmp-synthetic.h"
static void zero(const void *p,size_t n){const uint8_t *v=p;for(size_t i=0;i<n;i++)assert(!v[i]);}
static void setup(struct wpa2_ccmp *s,struct wpa2_handshake *h){
 memset(s,0,sizeof *s);memset(h,0,sizeof *h);struct wpa2_hs_peer p={0};memcpy(p.aa,hs_aa,6);memcpy(p.spa,hs_spa,6);memcpy(p.rsne,hs_rsne,sizeof hs_rsne);p.rsne_size=sizeof hs_rsne;
 assert(wpa2_hs_start(h,&p,hs_pmk,hs_sn,9));assert(wpa2_hs_receive(h,9,hs_aa,hs_spa,hs_m1,sizeof hs_m1));uint8_t b[121];assert(wpa2_hs_emit(h,9,b,121)==121);assert(wpa2_hs_tx_result(h,9,WPA2_HS_CONFIRMED));assert(wpa2_hs_receive(h,9,hs_aa,hs_spa,hs_m3,sizeof hs_m3));struct wpa2_hs_keys k;assert(wpa2_hs_take_keys(h,9,&k));
 assert(!wpa2_ccmp_activate(s,h));assert(wpa2_ccmp_install(s,h,&k) && h->key_owner==(uintptr_t)s);assert(!wpa2_ccmp_install(s,h,&k));struct wpa2_ccmp other={0};assert(!wpa2_ccmp_install(&other,h,&k));zero(&other,sizeof other);memset(&k,0,sizeof k);
 assert(!wpa2_ccmp_activate(s,h));uint8_t out[256];memset(out,0xa5,sizeof out);assert(!wpa2_ccmp_seal(s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,sizeof out) && !s->tx_pn);zero(out,sizeof out);
 assert(wpa2_hs_install_result(h,9,WPA2_HS_CONFIRMED));assert(wpa2_hs_emit(h,9,b,121)==99);assert(wpa2_hs_tx_result(h,9,WPA2_HS_CONFIRMED));assert(wpa2_ccmp_activate(s,h));assert(!wpa2_ccmp_activate(s,h));
}
int main(void){
 struct wpa2_ccmp s,base;struct wpa2_handshake h;setup(&s,&h);base=s;uint8_t out[4096],packet[sizeof ccmp_rx];
 assert(wpa2_ccmp_seal(&s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,sizeof out)==sizeof ccmp_tx && s.tx_pn==1);assert(!memcmp(out,ccmp_tx,sizeof ccmp_tx));zero(out+sizeof ccmp_tx,sizeof out-sizeof ccmp_tx);
 assert(wpa2_ccmp_seal(&s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,sizeof out)==sizeof ccmp_tx && s.tx_pn==2);assert(memcmp(out,ccmp_tx,sizeof ccmp_tx));
 s.tx_pn=WPA2_CCMP_PN_MAX-1;assert(wpa2_ccmp_seal(&s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,sizeof out));assert(s.tx_pn==WPA2_CCMP_PN_MAX);assert(!wpa2_ccmp_seal(&s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,sizeof out));zero(out,sizeof out);assert(s.tx_pn==WPA2_CCMP_PN_MAX);
 s=base;assert(wpa2_ccmp_open(&s,9,ccmp_rx,sizeof ccmp_rx,out,2304)==sizeof ccmp_data);assert(!memcmp(out,ccmp_data,sizeof ccmp_data));assert(s.rx_pairwise[16]==1);zero(s.scratch,sizeof s.scratch);assert(!wpa2_ccmp_open(&s,9,ccmp_rx,sizeof ccmp_rx,out,2304));zero(out,2304);assert(s.rx_pairwise[16]==1);
 assert(wpa2_ccmp_open(&s,9,ccmp_group,sizeof ccmp_group,out,2304)==sizeof ccmp_data);assert(!memcmp(out,ccmp_data,sizeof ccmp_data));assert(!wpa2_ccmp_open(&s,9,ccmp_group,sizeof ccmp_group,out,2304));zero(out,2304);
 /* Authenticated address/header bits,ciphertext and every MIC bit refuse.
  * Masked duration/sequence/Retry/PM/MoreData are tested separately. */
 for(unsigned i=0;i<sizeof ccmp_rx;i++){
  if(i==2 || i==3 || i==22 || i==23)continue;
  for(unsigned bit=0;bit<8;bit++){
   if(i==1 && (bit==3 || bit==4 || bit==5))continue;
   s=base;memcpy(packet,ccmp_rx,sizeof packet);packet[i]^=1u<<bit;
   assert(!wpa2_ccmp_open(&s,9,packet,sizeof packet,out,2304));zero(out,2304);assert(!s.rx_pairwise[16]);zero(s.scratch,sizeof s.scratch);
  }
 }
 for(unsigned off=2;off<24;off++)if(off==2 || off==3 || off==22 || off==23){s=base;memcpy(packet,ccmp_rx,sizeof packet);packet[off]^=off==22?0x10:1;assert(wpa2_ccmp_open(&s,9,packet,sizeof packet,out,2304)==sizeof ccmp_data);}
 for(unsigned bit=3;bit<6;bit++){s=base;memcpy(packet,ccmp_rx,sizeof packet);packet[1]^=1u<<bit;assert(wpa2_ccmp_open(&s,9,packet,sizeof packet,out,2304)==sizeof ccmp_data);}
 for(unsigned n=0;n<sizeof ccmp_rx;n++){s=base;uint8_t *p=malloc(n?n:1);memcpy(p,ccmp_rx,n);assert(!wpa2_ccmp_open(&s,9,p,n,out,2304));zero(out,2304);assert(!s.rx_pairwise[16]);zero(s.scratch,sizeof s.scratch);free(p);}
 for(unsigned cap=1;cap<sizeof ccmp_data;cap++){s=base;memset(out,0xa5,4096);assert(!wpa2_ccmp_open(&s,9,ccmp_rx,sizeof ccmp_rx,out,cap));zero(out,cap);assert(out[cap]==0xa5 && !s.rx_pairwise[16]);}
 for(unsigned cap=1;cap<sizeof ccmp_tx;cap++){s=base;memset(out,0xa5,4096);assert(!wpa2_ccmp_seal(&s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,cap));zero(out,cap);assert(out[cap]==0xa5 && !s.tx_pn);}
 s=base;struct wpa2_ccmp old=s;memset(out,0xa5,4096);assert(!wpa2_ccmp_open(&s,10,ccmp_rx,sizeof ccmp_rx,out,2304));assert(!wpa2_ccmp_seal(&s,10,ccmp_tx_header,ccmp_data,sizeof ccmp_data,out,4096));assert(!wpa2_ccmp_retire(&s,10));assert(!memcmp(&s,&old,sizeof s));for(unsigned i=0;i<4096;i++)assert(out[i]==0xa5);
 assert(!wpa2_ccmp_open(&s,9,ccmp_rx,sizeof ccmp_rx,s.scratch,2304));assert(!wpa2_ccmp_seal(&s,9,ccmp_tx_header,s.tk,16,out,4096));assert(!wpa2_ccmp_seal(&s,9,ccmp_tx_header,ccmp_data,sizeof ccmp_data,s.scratch,2304));assert(!memcmp(&s,&old,sizeof s));
 memcpy(out,ccmp_rx,sizeof ccmp_rx);memcpy(packet,out,sizeof packet);assert(!wpa2_ccmp_open(&s,9,out,sizeof ccmp_rx,out,2304));assert(!memcmp(out,packet,sizeof packet));
 assert(!wpa2_ccmp_open(&s,9,NULL,99,out,2304));zero(out,2304);assert(!wpa2_ccmp_seal(&s,9,NULL,ccmp_data,sizeof ccmp_data,out,4096));zero(out,4096);
 assert(wpa2_ccmp_retire(&s,9) && wpa2_ccmp_erased(&s,9));assert(!wpa2_ccmp_retire(&s,9));assert(!wpa2_ccmp_activate(&s,&h));assert(wpa2_hs_retire(&h,9,true));zero(s.tk,16);zero(s.gtk,16);zero(s.scratch,sizeof s.scratch);
 puts("wpa2-ccmp: PASS independent packet fixtures,one backend claim/install/activate,TX reservation/exhaustion,per-key replay,MIC/header/ciphertext mutations,exact truncation,aliases and secret retirement;no native cipher/data acceptance");return 0;
}
