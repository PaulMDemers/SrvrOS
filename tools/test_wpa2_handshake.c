#include <srvros/wpa2_handshake.h>
#include <srvros/wpa2_crypto.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixtures/wpa2-handshake-synthetic.h"
static void zero(const void *p,size_t n){const uint8_t *v=p;for(size_t i=0;i<n;i++)assert(!v[i]);}
static void erased(const struct wpa2_handshake *s){zero(s->pmk,32);zero(s->snonce,32);zero(s->anonce,32);zero(s->ptk,48);zero(s->gtk,16);zero(s->scratch,1016);}
static void begin(struct wpa2_handshake *s){
 memset(s,0,sizeof *s);struct wpa2_hs_peer p={0};memcpy(p.aa,hs_aa,6);memcpy(p.spa,hs_spa,6);memcpy(p.rsne,hs_rsne,sizeof hs_rsne);p.rsne_size=sizeof hs_rsne;
 assert(wpa2_hs_start(s,&p,hs_pmk,hs_sn,9) && s->phase==WPA2_HS_M1);struct wpa2_handshake old=*s;assert(!wpa2_hs_start(s,&p,hs_pmk,hs_sn,9));assert(!memcmp(&old,s,sizeof old));
}
static bool receive(struct wpa2_handshake *s,const uint8_t *p,size_t n){return wpa2_hs_receive(s,9,hs_aa,hs_spa,p,n);}
static void m3phase(struct wpa2_handshake *s){
 begin(s);assert(receive(s,hs_m1,sizeof hs_m1));zero(s->pmk,32);assert(!memcmp(s->ptk,hs_ptk,48));
 uint8_t out[121];assert(wpa2_hs_emit(s,9,out,sizeof out)==sizeof hs_m2);assert(!memcmp(out,hs_m2,sizeof out));assert(wpa2_hs_tx_result(s,9,WPA2_HS_CONFIRMED));assert(s->phase==WPA2_HS_M3);
}
static void ready(struct wpa2_handshake *s){m3phase(s);assert(receive(s,hs_m3,sizeof hs_m3) && s->phase==WPA2_HS_KEYS_READY);assert(s->replay==8);zero(s->scratch,1016);}
static void take(struct wpa2_handshake *s){struct wpa2_hs_keys k;assert(wpa2_hs_take_keys(s,9,&k));assert(k.generation==9 && k.group_id==1 && !memcmp(k.pairwise,hs_ptk+32,16) && !memcmp(k.group,hs_gtk,16));assert(!memcmp(k.group_rsc,"\1\2\3\4\5\6",6));memset(&k,0,sizeof k);assert(s->keys_may_live && s->install_handoffs==1);assert(wpa2_hs_claim_keys(s,9,0x1234));assert(!wpa2_hs_claim_keys(s,9,0x5678));}
int main(void){
 struct wpa2_handshake s,stages[10];unsigned count=0;begin(&s);stages[count++]=s;assert(receive(&s,hs_m1,sizeof hs_m1));stages[count++]=s;
 uint8_t out[121];assert(wpa2_hs_emit(&s,9,out,sizeof out)==121 && !memcmp(out,hs_m2,121));stages[count++]=s;assert(wpa2_hs_tx_result(&s,9,WPA2_HS_CONFIRMED));stages[count++]=s;
 assert(receive(&s,hs_m3,sizeof hs_m3));stages[count++]=s;take(&s);stages[count++]=s;assert(wpa2_hs_install_result(&s,9,WPA2_HS_CONFIRMED));stages[count++]=s;
 assert(wpa2_hs_emit(&s,9,out,sizeof out)==99 && !memcmp(out,hs_m4,99));zero(out+99,22);stages[count++]=s;assert(wpa2_hs_tx_result(&s,9,WPA2_HS_CONFIRMED));stages[count++]=s;erased(&s);
 assert(s.phase==WPA2_HS_COMPLETE && s.keys_may_live && s.install_handoffs==1 && s.emissions==2);
 assert(!wpa2_hs_retire(&s,9,false) && s.phase==WPA2_HS_FAILED && s.keys_may_live);erased(&s);assert(wpa2_hs_retire(&s,9,true) && s.phase==WPA2_HS_CLOSED && !s.keys_may_live);erased(&s);
 assert(!wpa2_hs_retire(&s,9,true) && !receive(&s,hs_m1,sizeof hs_m1));
 /* Every stale generation leaves state/output untouched at every valid phase. */
 for(unsigned i=0;i<count;i++){
  s=stages[i];struct wpa2_handshake old=s;struct wpa2_hs_keys k,kold;memset(&k,0xa5,sizeof k);kold=k;memset(out,0xa5,sizeof out);
  assert(!wpa2_hs_receive(&s,10,hs_aa,hs_spa,hs_m1,sizeof hs_m1));assert(!wpa2_hs_emit(&s,10,out,sizeof out));assert(!wpa2_hs_tx_result(&s,10,WPA2_HS_CONFIRMED));assert(!wpa2_hs_take_keys(&s,10,&k));assert(!wpa2_hs_install_result(&s,10,WPA2_HS_CONFIRMED));assert(!wpa2_hs_claim_keys(&s,10,0x1234));assert(!wpa2_hs_abort(&s,10));assert(!wpa2_hs_retire(&s,10,true));assert(!memcmp(&old,&s,sizeof s) && !memcmp(&k,&kold,sizeof k));for(unsigned j=0;j<sizeof out;j++)assert(out[j]==0xa5);
  /* Aliased outputs/inputs cannot corrupt the live model. */
  assert(!wpa2_hs_emit(&s,9,s.scratch,121));assert(!wpa2_hs_take_keys(&s,9,(void *)s.scratch));assert(!wpa2_hs_receive(&s,9,s.peer.aa,hs_spa,hs_m1,sizeof hs_m1));assert(!wpa2_hs_receive(&s,9,hs_aa,hs_spa,s.scratch,99));assert(!memcmp(&old,&s,sizeof s));
  assert(wpa2_hs_abort(&s,9));assert(!wpa2_hs_abort(&s,9));assert(s.phase==WPA2_HS_FAILED);erased(&s);assert(s.keys_may_live==old.keys_may_live);assert(wpa2_hs_retire(&s,9,true));
 }
 /* Duplicate/out-of-order messages never trigger a second key export. */
 for(unsigned i=0;i<count;i++){
  s=stages[i];const uint8_t *p=i==0?hs_m3:hs_m1;size_t n=i==0?sizeof hs_m3:sizeof hs_m1;
  assert(!receive(&s,p,n));assert(s.phase==WPA2_HS_FAILED);erased(&s);assert(s.install_handoffs<=1);
 }
 for(unsigned i=0;i<count;i++)if(i!=4){s=stages[i];struct wpa2_hs_keys k;assert(!wpa2_hs_take_keys(&s,9,&k));zero(&k,sizeof k);assert(s.phase==WPA2_HS_FAILED);erased(&s);}
 /* No confirmed installation without a unique backend claim. */
 s=stages[5];s.key_owner=0;assert(!wpa2_hs_claim_keys(&s,9,0));assert(!wpa2_hs_install_result(&s,9,WPA2_HS_CONFIRMED));assert(s.phase==WPA2_HS_FAILED && s.keys_may_live);erased(&s);
 for(unsigned result=1;result<4;result++){
  s=stages[2];assert(!wpa2_hs_tx_result(&s,9,(enum wpa2_hs_result)result));assert(!s.keys_may_live);erased(&s);
  ready(&s);take(&s);assert(!wpa2_hs_install_result(&s,9,(enum wpa2_hs_result)result));assert(s.keys_may_live);erased(&s);assert(!wpa2_hs_retire(&s,9,false));assert(wpa2_hs_retire(&s,9,true));
  s=stages[7];assert(!wpa2_hs_tx_result(&s,9,(enum wpa2_hs_result)result));assert(s.keys_may_live);erased(&s);
 }
 for(unsigned size=0;size<sizeof hs_m1;size++){begin(&s);uint8_t *p=malloc(size?size:1);memcpy(p,hs_m1,size);assert(!receive(&s,p,size));erased(&s);free(p);}
 for(unsigned size=0;size<sizeof hs_m3;size++){m3phase(&s);uint8_t *p=malloc(size?size:1);memcpy(p,hs_m3,size);assert(!receive(&s,p,size));erased(&s);free(p);}
 /* Categorical outcomes survive erasure without exposing message/key bytes.
  * Stale/alias invariants above also cover the new observation field. */
 m3phase(&s);assert(!receive(&s,hs_m1,sizeof hs_m1) && s.last_rx==WPA2_HS_RX_KEY_INFO);erased(&s);
 m3phase(&s);uint8_t bad_mic[sizeof hs_m3];memcpy(bad_mic,hs_m3,sizeof bad_mic);bad_mic[81]^=1;
 assert(!receive(&s,bad_mic,sizeof bad_mic) && s.last_rx==WPA2_HS_RX_MIC);erased(&s);
 assert(wpa2_hs_retire(&s,9,true) && s.last_rx==WPA2_HS_RX_MIC);
 for(unsigned category=0;category<4;category++){
  m3phase(&s);uint8_t message[sizeof hs_m3];memcpy(message,hs_m3,sizeof message);
  enum wpa2_hs_rx_result expected;
  if(category==0){message[0]=message[0]==1?2:1;expected=WPA2_HS_RX_VERSION;}
  else if(category==1){memset(message+9,0,8);expected=WPA2_HS_RX_REPLAY;}
  else if(category==2){message[17]^=1;expected=WPA2_HS_RX_NONCE;}
  else{message[49]^=1;expected=WPA2_HS_RX_RESERVED;}
  assert(!receive(&s,message,sizeof message) && s.last_rx==expected && !s.install_handoffs && !s.keys_may_live);erased(&s);
 }
 /* M3 integrity covers every bit,before any key export. */
 for(unsigned i=0;i<sizeof hs_m3;i++)for(unsigned bit=0;bit<8;bit++){
  m3phase(&s);uint8_t p[sizeof hs_m3];memcpy(p,hs_m3,sizeof p);p[i]^=1u<<bit;assert(!receive(&s,p,sizeof p));assert(!s.install_handoffs && !s.keys_may_live && s.phase==WPA2_HS_FAILED);erased(&s);
 }
 for(unsigned i=0;i<6;i++){
  uint8_t a[6];memcpy(a,hs_aa,6);a[i]^=2;begin(&s);assert(!wpa2_hs_receive(&s,9,a,hs_spa,hs_m1,sizeof hs_m1));erased(&s);
  memcpy(a,hs_spa,6);a[i]^=2;m3phase(&s);assert(!wpa2_hs_receive(&s,9,hs_aa,a,hs_m3,sizeof hs_m3));erased(&s);
 }
 begin(&s);assert(receive(&s,hs_m1,sizeof hs_m1));memset(out,0xa5,sizeof out);assert(!wpa2_hs_emit(&s,9,out,120));zero(out,120);assert(out[120]==0xa5);erased(&s);
 struct wpa2_hs_peer p={0};memcpy(p.aa,hs_aa,6);memcpy(p.spa,hs_spa,6);memcpy(p.rsne,hs_rsne,sizeof hs_rsne);p.rsne_size=sizeof hs_rsne;
 memset(&s,0,sizeof s);assert(!wpa2_hs_start(&s,&p,hs_pmk,hs_sn,0));assert(!wpa2_hs_start(&s,&p,hs_pmk,(const uint8_t[32]){0},9));p.spa[0]|=1;assert(!wpa2_hs_start(&s,&p,hs_pmk,hs_sn,9));zero(&s,sizeof s);
 puts("wpa2-handshake: PASS independent M1/M2/M3/M4 fixtures,all stage/generation/alias/refusal lifetimes,exact truncated allocations,every M3 bit,one install export,MIC+unwrap+RSNE/GTK validation and secret retirement;no native key or data acceptance");return 0;
}
