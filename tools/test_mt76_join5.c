#include <srvros/mt76_join5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct mt76_join5 session;
static struct mt76_peer5 peer;
static struct {unsigned ownership,lose,sends,fail_send,polls;enum mt76_join5_send send_result;uint64_t now;bool extra,empty,spoof,reject,malformed,poll_fail,backward;}fake;
static bool owned(void *p,bool cleanup){(void)cleanup;assert(p==&fake);return ++fake.ownership!=fake.lose;}
static enum mt76_join5_send send_frame(void *p,enum mt76_mgmt5_kind kind,const uint8_t *b,size_t n){
 assert(p==&fake);uint8_t expected[124];size_t count=mt76_mgmt5_usb_request(&peer.peer,kind,(unsigned)kind+1,false,expected,sizeof expected);assert(n==count && !memcmp(b,expected,n));
 fake.sends++;return fake.sends==fake.fail_send?fake.send_result:MT76_JOIN5_SENT;
}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static bool poll_reply(void *p,uint8_t *b,size_t capacity,size_t *n,bool *present){
 assert(p==&fake && capacity>=64);fake.polls++;*n=0;*present=!fake.empty;if(fake.poll_fail)return false;if(!*present)return true;
 memset(b,0,64);bool auth=!session.auth_matched;put16(b,auth?0xb0:0x10);memcpy(b+4,peer.peer.station,6);memcpy(b+10,peer.peer.bssid,6);memcpy(b+16,peer.peer.bssid,6);
 if(auth){put16(b+26,2);put16(b+28,fake.reject?13:0);*n=30;if(fake.extra){b[30]=221;b[31]=9;for(unsigned i=0;i<9;i++)b[32+i]=(uint8_t)(i+1);*n=41;}}
 else {put16(b+24,0x411);put16(b+26,fake.reject?13:0);put16(b+28,0xc007);b[30]=1;b[31]=8;const uint8_t r[]={0x8c,18,0x98,36,0xb0,72,96,108};memcpy(b+32,r,8);*n=40;}
 if(fake.spoof && fake.polls==1)b[10]^=2;
 if(fake.malformed)*n=29;
 return true;
}
static bool delay(void *p,unsigned us){assert(p==&fake && us==100);fake.now+=us;return true;}
static uint64_t now(void *p){assert(p==&fake);if(fake.backward && fake.polls)return 0;return fake.now;}
static struct mt76_join5_io io={&fake,owned,send_frame,poll_reply,delay,now};
static void setup(void){
 session=(struct mt76_join5){0};memset(&fake,0,sizeof fake);fake.now=1000;
 peer=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1,.observations=2,.rates=255,.peer={.station={2,1,2,3,4,5},.bssid={2,8,8,8,8,8},.ssid="Demers5",.ssid_size=7,.channel=161,.basic_rates=21}};
 peer.peer.rsn_size=mt76_rsn_station_ie(peer.peer.rsn,sizeof peer.peer.rsn);
}
static bool run(void){bool ok=mt76_join5_run(&session,&peer,&io);for(unsigned i=0;i<sizeof session.reply;i++)assert(!session.reply[i]);for(unsigned i=0;i<sizeof session.packet;i++)assert(!session.packet[i]);return ok;}
int main(void){
 setup();assert(run() && session.auth_sent && session.auth_matched && session.assoc_sent && session.assoc_matched && session.aid==7 && session.leave_sent && fake.sends==3);unsigned checks=fake.ownership;
 struct mt76_join5 saved=session;assert(!mt76_join5_run(&session,&peer,&io) && !memcmp(&saved,&session,sizeof saved));
 for(unsigned i=1;i<=checks;i++){setup();fake.lose=i;assert(!run());}
 for(unsigned i=1;i<=3;i++){setup();fake.fail_send=i;fake.send_result=MT76_JOIN5_UNCERTAIN;assert(!run() && session.unsafe && fake.sends==i && !session.leave_sent);}
 setup();fake.fail_send=2;fake.send_result=MT76_JOIN5_REFUSED;assert(!run() && !session.unsafe && session.leave_sent && fake.sends==3);
 setup();fake.fail_send=1;fake.send_result=MT76_JOIN5_REFUSED;assert(!run() && !session.auth_sent && fake.sends==1);
 setup();fake.extra=true;assert(run() && session.auth_matched && session.assoc_matched && session.leave_sent);
 setup();fake.spoof=true;assert(run() && session.ignored==1);
 setup();fake.empty=true;assert(!run() && !session.auth_matched && session.leave_sent && !session.unsafe && fake.polls<=8192 && fake.now>=751000);
 setup();fake.reject=true;assert(!run() && session.status==13 && session.leave_sent);
 setup();fake.malformed=true;assert(!run() && session.leave_sent);
 setup();fake.poll_fail=true;assert(!run() && session.unsafe && !session.leave_sent);
 setup();fake.backward=true;fake.empty=true;assert(!run() && session.leave_sent);
 setup();peer.complete=false;assert(!run() && !session.attempted && !fake.sends);
 setup();peer.peer.basic_rates=0;assert(!run() && !session.attempted);
 setup();struct mt76_peer5 before=peer;assert(!mt76_join5_run((void *)&peer,&peer,&io) && !memcmp(&peer,&before,sizeof peer));
 puts("mt76-join5: PASS fixed AUTH/ASSOC/leave bytes,sequences and matched replies,all ownership/send uncertainty boundaries,no replay/leave on uncertain DMA,refusal cleanup,spoof/timeout/rejection/malformed/clock/alias refusal and scratch scrub;no native authority or WPA2/data acceptance");return 0;
}
