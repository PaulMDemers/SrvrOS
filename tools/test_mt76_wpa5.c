#include <srvros/mt76_wpa5.h>
#include <srvros/mt76_eapol5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/wpa2-handshake-synthetic.h"
static struct mt76_wpa5 s;
static struct mt76_peer5 peer;
static struct {unsigned checks,lose,nonce_calls,sends,fail_send,polls,fail_poll,fail_delay,delays,mutate,empty_phase;enum mt76_join5_send result;uint64_t time;bool lost,nonce_fail,nonce_zero,empty,late,backward,spoof,reject,malformed,bad_span,frozen_clock;}fake;
static bool owned(void *p,bool cleanup){(void)cleanup;assert(p==&fake);fake.checks++;if(fake.checks==fake.lose)fake.lost=true;return !fake.lost;}
static bool nonce(void *p,uint8_t out[32]){assert(p==&fake);fake.nonce_calls++;memcpy(out,hs_sn,32);if(fake.nonce_zero)memset(out,0,32);return !fake.nonce_fail;}
static enum mt76_join5_send send_frame(void *p,enum mt76_wpa5_tx kind,const uint8_t *bytes,size_t n){
 assert(p==&fake && !fake.lost);uint8_t expected[184]={0};size_t want;
 if(kind==MT76_WPA5_M2 || kind==MT76_WPA5_M4){
  struct mt76_eapol5_peer ep={.channel=161,.bandwidth=20};memcpy(ep.station,hs_spa,6);memcpy(ep.bssid,hs_aa,6);
  bool m2=kind==MT76_WPA5_M2;want=mt76_eapol5_usb_tx(&ep,m2?hs_m2:hs_m4,m2?sizeof hs_m2:sizeof hs_m4,(unsigned)kind+4,expected,sizeof expected);
  assert(s.handshake.phase==(m2?WPA2_HS_M2_PENDING:WPA2_HS_M4_PENDING));
  if(!m2)assert(s.m3_mic && s.keys_installed && s.keys.phase==WPA2_CCMP_INSTALLED && !s.keys_activated);
 }else{
  enum mt76_mgmt5_kind k=kind==MT76_WPA5_AUTH?MT76_MGMT5_AUTH:kind==MT76_WPA5_ASSOC?MT76_MGMT5_ASSOC:MT76_MGMT5_LEAVE;
  want=mt76_mgmt5_usb_request(&peer.peer,k,(unsigned)kind+4,false,expected,sizeof expected);
  if(kind==MT76_WPA5_LEAVE)assert(s.keys_erased && (s.handshake.phase==WPA2_HS_CLOSED || s.handshake.phase==WPA2_HS_EMPTY));
 }
 assert(want && want==n && !memcmp(expected,bytes,n));fake.sends++;
 return fake.sends==fake.fail_send?fake.result:MT76_JOIN5_SENT;
}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static bool poll_reply(void *p,enum mt76_wpa5_rx kind,uint8_t *out,size_t cap,size_t *n,bool *present){
 assert(p==&fake && cap==4096 && !fake.lost);fake.polls++;*n=0;*present=false;
 if(fake.polls==fake.fail_poll)return false;
 unsigned phase=kind==MT76_WPA5_MANAGEMENT?(s.auth_matched?2:1):(s.m1_matched?4:3);
 if(fake.empty || fake.empty_phase==phase)return true;
 if(fake.bad_span){*n=4097;*present=true;return true;}
 if(kind==MT76_WPA5_EAPOL){
  bool m1=!s.m1_matched;*n=m1?sizeof hs_m1:sizeof hs_m3;memcpy(out,m1?hs_m1:hs_m3,*n);*present=true;
  if(fake.mutate && !m1)out[fake.mutate-1]^=1;
 }else{
  bool auth=!s.auth_matched;memset(out,0,64);put16(out,auth?0xb0:0x10);memcpy(out+4,hs_spa,6);memcpy(out+10,hs_aa,6);memcpy(out+16,hs_aa,6);
  if(auth){put16(out+26,2);put16(out+28,fake.reject?13:0);*n=30;}
  else{put16(out+24,0x411);put16(out+28,0xc022);out[30]=1;out[31]=8;memcpy(out+32,(uint8_t[]){0x8c,18,0x98,36,0xb0,72,96,108},8);*n=40;}
  if(fake.spoof && fake.polls==1)out[10]^=2;
  if(fake.malformed)*n=29;
  *present=true;
 }
 if(fake.late)fake.time+=1500000;
 if(fake.backward)fake.time=0;
 return true;
}
static bool delay(void *p,unsigned us){assert(p==&fake && us==100);if(!fake.frozen_clock)fake.time+=us;return ++fake.delays!=fake.fail_delay;}
static uint64_t now(void *p){assert(p==&fake);return fake.time;}
static const struct mt76_wpa5_io io={&fake,owned,nonce,send_frame,poll_reply,delay,now};
static bool use_data,data_lost;
static unsigned data_bad_phase;
static unsigned data_calls;
static enum mt76_join5_send data_result;
static enum mt76_join5_send data_window(void *c,struct wpa2_ccmp *k,uint64_t g,const struct mt76_peer5 *p){
 assert(c==&fake && k==&s.keys && g==9 && p==&peer && !fake.lost);
 assert(s.m4_sent && s.keys_activated && !s.keys_erased && !s.leave_sent && fake.sends==4 && k->phase==WPA2_CCMP_ACTIVE && s.handshake.phase==WPA2_HS_COMPLETE);
 data_calls++;assert(data_calls==1);
 assert(!mt76_wpa5_run_data(&s,&peer,hs_pmk,&io,(void *)&s));
 uint8_t h[24]={8,1},plain[8]={0xaa,0xaa,3,0,0,0,8,0},out[64];memcpy(h+4,hs_aa,6);memcpy(h+10,hs_spa,6);memcpy(h+16,hs_aa,6);
 assert(wpa2_ccmp_seal(k,g,h,plain,sizeof plain,out,sizeof out)==48 && k->tx_pn==1);
 if(data_lost)fake.lost=true;
 if(data_bad_phase==1)s.handshake.phase=WPA2_HS_CLOSED;
 if(data_bad_phase==2)s.handshake.phase=WPA2_HS_EMPTY;
 if(data_bad_phase==3)s.keys.phase=WPA2_CCMP_CLOSED;
 if(data_bad_phase==4)s.keys.phase=WPA2_CCMP_EMPTY;
 return data_result;
}
static const struct mt76_wpa5_data extension={&fake,data_window};
static void setup(void){
 memset(&s,0,sizeof s);memset(&fake,0,sizeof fake);fake.time=1000;
 use_data=data_lost=false;data_bad_phase=0;data_calls=0;data_result=MT76_JOIN5_SENT;
 peer=(struct mt76_peer5){.attempted=true,.complete=true,.generation=9,.observations=2,.rates=255,.peer={.ssid="Synthetic5",.ssid_size=10,.channel=161,.basic_rates=21,.rsn_size=sizeof hs_rsne}};
 memcpy(peer.peer.station,hs_spa,6);memcpy(peer.peer.bssid,hs_aa,6);memcpy(peer.peer.rsn,hs_rsne,sizeof hs_rsne);
}
static void zero(const void *p,size_t n){const uint8_t *v=p;while(n--)assert(!*v++);}
static bool run(void){
 bool ok=use_data?mt76_wpa5_run_data(&s,&peer,hs_pmk,&io,&extension):mt76_wpa5_run(&s,&peer,hs_pmk,&io);
 zero(s.reply,sizeof s.reply);zero(s.packet,sizeof s.packet);zero(s.eapol,sizeof s.eapol);zero(s.snonce,sizeof s.snonce);
 zero(s.handshake.pmk,32);zero(s.handshake.snonce,32);zero(s.handshake.anonce,32);zero(s.handshake.ptk,48);zero(s.handshake.gtk,16);zero(s.handshake.scratch,sizeof s.handshake.scratch);
 zero(s.keys.tk,16);zero(s.keys.gtk,16);zero(s.keys.scratch,sizeof s.keys.scratch);
 assert(!s.handshake.keys_may_live && s.keys_erased && s.sends==fake.sends);
 return ok;
}
int main(void){
 setup();assert(run() && s.m1_matched && s.m2_sent && s.m3_mic && s.keys_installed && s.m4_sent && s.keys_activated && s.leave_sent && s.aid==34 && fake.sends==5 && fake.nonce_calls==1 && fake.polls==4);
 assert(s.handshake.phase==WPA2_HS_CLOSED && s.keys.phase==WPA2_CCMP_CLOSED && s.handshake.key_owner==(uintptr_t)&s.keys);
 for(unsigned i=0;i<4;i++){const struct mt76_wpa5_response *r=&s.responses[i];assert(r->result==MT76_WPA5_RESPONSE_OK && r->polls==1 && r->present==1 && !r->ignored);if(i>=2)assert(r->kind==i-1 && r->reject==WPA2_HS_RX_ACCEPTED);}
 unsigned checks=fake.checks;struct mt76_wpa5 old=s;assert(!mt76_wpa5_run(&s,&peer,hs_pmk,&io) && !memcmp(&s,&old,sizeof s));
 for(unsigned i=1;i<=checks;i++){setup();fake.lose=i;assert(!run() && s.unsafe);}
 for(unsigned i=1;i<=5;i++)for(unsigned r=MT76_JOIN5_REFUSED;r<=MT76_JOIN5_UNCERTAIN;r++){
  setup();fake.fail_send=i;fake.result=(enum mt76_join5_send)r;assert(!run());
  if(r==MT76_JOIN5_UNCERTAIN)assert(s.unsafe && !s.leave_sent && fake.sends==i);
  else assert(!s.unsafe && (i==1 || i==5 || s.leave_sent));
 }
 for(unsigned i=1;i<=4;i++){setup();fake.fail_poll=i;assert(!run() && s.unsafe && !s.leave_sent);}
 for(unsigned i=1;i<=sizeof hs_m3;i++){setup();fake.mutate=i;assert(!run() && s.responses[3].result==MT76_WPA5_RESPONSE_REJECTED && s.responses[3].reject>=WPA2_HS_RX_ENVELOPE && !s.m3_mic && !s.keys_installed && !s.keys_activated && s.leave_sent && !s.unsafe);}
 for(unsigned phase=1;phase<=4;phase++){
  setup();fake.empty_phase=phase;assert(!run() && !s.unsafe && s.leave_sent && !s.keys_installed && fake.polls<=20004);assert(s.responses[phase-1].result==MT76_WPA5_RESPONSE_TIMEOUT && !s.responses[phase-1].present);
 }
 setup();fake.empty=true;fake.frozen_clock=true;assert(!run() && !s.unsafe && s.leave_sent && fake.polls==20000);assert(s.responses[0].result==MT76_WPA5_RESPONSE_LIMIT);
 setup();fake.fail_send=3;fake.result=(enum mt76_join5_send)99;assert(!run() && s.unsafe && !s.leave_sent);
 setup();fake.nonce_fail=true;assert(!run() && !fake.sends && !s.handshake.generation);
 setup();fake.nonce_zero=true;assert(!run() && !fake.sends);
 setup();fake.spoof=true;assert(run() && s.ignored==1 && fake.polls==5);
 setup();fake.empty=true;assert(!run() && !s.unsafe && s.leave_sent && fake.time==751000);
 setup();fake.empty=true;fake.fail_delay=1;assert(!run() && s.unsafe && !s.leave_sent);
 setup();fake.late=true;assert(!run() && !s.unsafe && s.leave_sent && !s.auth_matched);
 setup();fake.backward=true;assert(!run() && !s.unsafe && s.leave_sent && !s.auth_matched);
 setup();fake.reject=true;assert(!run() && s.status==13 && s.leave_sent);
 setup();fake.malformed=true;assert(!run() && s.leave_sent);
 setup();fake.bad_span=true;assert(!run() && s.unsafe && !s.leave_sent);
 setup();peer.peer.channel=1;assert(!mt76_wpa5_run(&s,&peer,hs_pmk,&io) && !s.attempted);
 setup();peer.peer.rsn_size=65;assert(!mt76_wpa5_run(&s,&peer,hs_pmk,&io) && !s.attempted);
 setup();assert(!mt76_wpa5_run(&s,&peer,s.reply,&io) && !s.attempted);
 setup();assert(!mt76_wpa5_run(&s,(void *)&s,hs_pmk,&io) && !s.attempted);
 setup();struct mt76_wpa5_io alias=io;alias.context=&s;assert(!mt76_wpa5_run(&s,&peer,hs_pmk,&alias) && !s.attempted);
 setup();use_data=true;assert(run() && data_calls==1 && s.keys.tx_pn==1 && s.leave_sent);
 unsigned data_checks=fake.checks;
 for(unsigned i=1;i<=data_checks;i++){setup();use_data=true;fake.lose=i;assert(!run() && s.unsafe);}
 setup();use_data=true;data_result=MT76_JOIN5_REFUSED;assert(!run() && data_calls==1 && !s.unsafe && s.leave_sent && s.keys_erased);
 setup();use_data=true;data_result=MT76_JOIN5_UNCERTAIN;assert(!run() && data_calls==1 && s.unsafe && !s.leave_sent && s.keys_erased);
 setup();use_data=true;data_lost=true;assert(!run() && data_calls==1 && s.unsafe && !s.leave_sent && s.keys_erased);
 setup();use_data=true;data_result=(enum mt76_join5_send)99;assert(!run() && s.unsafe && !s.leave_sent);
 for(unsigned n=1;n<=4;n++){setup();use_data=true;data_bad_phase=n;assert(!run() && s.unsafe && !s.leave_sent && s.keys_erased);}
 setup();assert(!mt76_wpa5_run_data(&s,&peer,hs_pmk,&io,NULL) && !s.attempted);
 struct mt76_wpa5_data bad_extension=extension;bad_extension.context=&s;
 assert(!mt76_wpa5_run_data(&s,&peer,hs_pmk,&io,&bad_extension) && !s.attempted);
 printf("mt76-wpa5: PASS synthetic AUTH/ASSOC/M1/M2/M3/M4/leave,unique software install/activation/erasure,%u ownership boundaries,all five send refusals/uncertainties,all M3 byte corruptions,nonce/clock/deadline/poll/alias refusals;no native or encrypted data acceptance\n",checks);
}
