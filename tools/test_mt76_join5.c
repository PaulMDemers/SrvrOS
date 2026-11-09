#include <srvros/mt76_join5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct mt76_join5 session;
static struct mt76_peer5 peer;
static struct {unsigned ownership,lose,sends,fail_send,polls;enum mt76_join5_send send_result;uint64_t now;bool extra,empty,spoof,reject,malformed,poll_fail,backward,assoc_only,freeze,delay_fail,oversize,inconsistent,short_reply,assoc_backward;}fake;
static uint64_t trace_hash;static unsigned trace_events,clock_reads;
static void trace(unsigned kind,uint64_t value){trace_hash^=kind;trace_hash*=1099511628211ull;for(unsigned i=0;i<8;i++){trace_hash^=(uint8_t)(value>>(8*i));trace_hash*=1099511628211ull;}trace_events++;}
static bool fault(void){return !fake.assoc_only || session.auth_matched;}
static bool owned(void *p,bool cleanup){assert(p==&fake);bool ok=++fake.ownership!=fake.lose;trace(1,((uint64_t)cleanup<<32)|ok);return ok;}
static enum mt76_join5_send send_frame(void *p,enum mt76_mgmt5_kind kind,const uint8_t *b,size_t n){
 assert(p==&fake);uint8_t expected[124];size_t count=mt76_mgmt5_usb_request(&peer.peer,kind,(unsigned)kind+1,false,expected,sizeof expected);assert(n==count && !memcmp(b,expected,n));
 fake.sends++;enum mt76_join5_send result=fake.sends==fake.fail_send?fake.send_result:MT76_JOIN5_SENT;trace(2,((uint64_t)kind<<48)|((uint64_t)n<<16)|result);return result;
}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static bool poll_reply(void *p,uint8_t *b,size_t capacity,size_t *n,bool *present){
 assert(p==&fake && capacity>=64);fake.polls++;*n=0;*present=!(fake.empty && fault());if(fake.poll_fail && fault()){trace(3,0);return false;}if(!*present){if(fake.inconsistent && fault())*n=1;trace(3,1|((uint64_t)*n<<8));return true;}
 memset(b,0,64);bool auth=!session.auth_matched;put16(b,auth?0xb0:0x10);memcpy(b+4,peer.peer.station,6);memcpy(b+10,peer.peer.bssid,6);memcpy(b+16,peer.peer.bssid,6);
 if(auth){put16(b+26,2);put16(b+28,(fake.reject && fault())?13:0);*n=30;if(fake.extra){b[30]=221;b[31]=9;for(unsigned i=0;i<9;i++)b[32+i]=(uint8_t)(i+1);*n=41;}}
 else {put16(b+24,0x411);put16(b+26,(fake.reject && fault())?13:0);put16(b+28,0xc007);b[30]=1;b[31]=8;const uint8_t r[]={0x8c,18,0x98,36,0xb0,72,96,108};memcpy(b+32,r,8);*n=40;}
 if(fake.spoof && fake.polls==1)b[10]^=2;
 if(fake.malformed && fault())*n=29;
 if(fake.short_reply && fault())*n=20;
 if(fake.oversize && fault())*n=capacity+1;
 trace(3,1|((uint64_t)*present<<1)|((uint64_t)*n<<8));
 return true;
}
static bool delay(void *p,unsigned us){assert(p==&fake && us==100);if(!fake.freeze)fake.now+=us;bool ok=!(fake.delay_fail && fault());trace(4,((uint64_t)us<<32)|ok);return ok;}
static uint64_t now(void *p){assert(p==&fake);clock_reads++;uint64_t n=((fake.backward && fake.polls) || (fake.assoc_backward && session.auth_matched && clock_reads>=3))?0:fake.now;trace(5,n);return n;}
static struct mt76_join5_io io={&fake,owned,send_frame,poll_reply,delay,now};
static void setup(void){
 session=(struct mt76_join5){0};memset(&fake,0,sizeof fake);fake.now=1000;trace_hash=1469598103934665603ull;trace_events=clock_reads=0;
 peer=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1,.observations=2,.rates=255,.peer={.station={2,1,2,3,4,5},.bssid={2,8,8,8,8,8},.ssid="Demers5",.ssid_size=7,.channel=161,.basic_rates=21}};
 peer.peer.rsn_size=mt76_rsn_station_ie(peer.peer.rsn,sizeof peer.peer.rsn);
}
static bool run(void){bool ok=mt76_join5_run(&session,&peer,&io);for(unsigned i=0;i<sizeof session.reply;i++)assert(!session.reply[i]);for(unsigned i=0;i<sizeof session.packet;i++)assert(!session.packet[i]);return ok;}
static void scenario(unsigned n){
 setup();
 switch(n){
 case 0:break;case 1:fake.extra=true;break;case 2:fake.spoof=true;break;
 case 3:fake.empty=true;break;case 4:fake.reject=true;break;case 5:fake.malformed=true;break;
 case 6:fake.poll_fail=true;break;case 7:fake.backward=fake.empty=true;break;
 case 8:fake.assoc_only=fake.empty=true;break;case 9:fake.assoc_only=fake.reject=true;break;
 case 10:fake.assoc_only=fake.malformed=true;break;case 11:fake.assoc_only=fake.empty=fake.freeze=true;break;
 case 12:fake.assoc_only=fake.empty=fake.delay_fail=true;break;
 case 13:fake.assoc_only=fake.poll_fail=true;break;case 14:fake.assoc_only=fake.oversize=true;break;
 case 15:fake.assoc_only=fake.empty=fake.inconsistent=true;break;
 case 16:fake.assoc_only=fake.empty=fake.assoc_backward=true;break;
 case 17:fake.assoc_only=fake.short_reply=true;break;
 default:fake.lose=n-17;break;
 }
}
#ifdef JOIN5_TRACE_ONLY
int main(void){
 for(unsigned i=0;i<48;i++){
  scenario(i);bool ok=run();
  printf("%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %llu %u %u %llu\n",i,ok,session.attempted,session.complete,session.failed,session.unsafe,session.auth_sent,session.auth_matched,session.assoc_sent,session.assoc_matched,session.leave_sent,session.sends,session.polls,session.ignored,session.status,(unsigned long long)session.aid,trace_events,clock_reads,(unsigned long long)trace_hash);
 }
 return 0;
}
#else
static void observations(void){
 const unsigned reasons[]={MT76_JOIN5_RESPONSE_DEADLINE,MT76_JOIN5_RESPONSE_REJECTED,MT76_JOIN5_RESPONSE_INVALID,MT76_JOIN5_RESPONSE_TURN_CAP,MT76_JOIN5_RESPONSE_DELAY,MT76_JOIN5_RESPONSE_POLL,MT76_JOIN5_RESPONSE_SHAPE,MT76_JOIN5_RESPONSE_SHAPE,MT76_JOIN5_RESPONSE_ROLLBACK,MT76_JOIN5_RESPONSE_DEADLINE};
 for(unsigned n=8;n<=17;n++){
  scenario(n);assert(!run());const struct mt76_join5_response_observation *a=&session.response[0],*b=&session.response[1];
  assert(a->attempted==1 && a->reason==MT76_JOIN5_RESPONSE_MATCHED && a->polls==a->present && a->present==a->peer && a->peer==a->clocks && a->clocks==1 && !a->span_us && !a->ignored);
  assert(b->attempted==1 && b->reason==reasons[n-8] && b->polls && b->polls<=8192 && b->clocks>=1 && b->clocks<=b->polls+1);
  assert(session.polls==a->polls+b->polls && session.ignored==a->ignored+b->ignored);
  assert(session.auth_sent && session.auth_matched && session.assoc_sent && !session.assoc_matched && !session.aid);
  if(n==9 || n==10)assert(b->peer==1 && b->present==1 && !b->span_us && b->clocks==1);
  if(n==9)assert(session.status==13 && !session.unsafe && session.leave_sent);
  if(n==10)assert(!session.status && !session.unsafe && session.leave_sent);
  if(n==11)assert(b->polls==8192 && b->clocks==8193 && !b->span_us && !session.unsafe && session.leave_sent);
  if(n==16)assert(b->clocks==2 && !b->span_us && !session.unsafe && session.leave_sent);
  if(n==8 || n==17)assert(b->span_us>=750000 && !session.unsafe && session.leave_sent);
  if(n>=12 && n<=15)assert(session.unsafe && !session.leave_sent);
 }
 setup();fake.empty=true;assert(!run() && session.response[0].reason==MT76_JOIN5_RESPONSE_DEADLINE);
 const struct mt76_join5_response_observation zero={0};assert(!memcmp(&session.response[1],&zero,sizeof zero));
 setup();fake.lose=4;assert(!run() && session.response[0].reason==MT76_JOIN5_RESPONSE_OWNER && !session.response[0].polls && session.response[0].clocks==1);
 setup();fake.lose=5;assert(!run() && session.response[0].reason==MT76_JOIN5_RESPONSE_OWNER && session.response[0].polls==1 && session.response[0].clocks==1 && !session.response[0].span_us);
}
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
 observations();
 puts("mt76-join5: PASS fixed AUTH/ASSOC/leave bytes,sequences and matched replies,all ownership/send uncertainty boundaries,no replay/leave on uncertain DMA,refusal cleanup,spoof/timeout/rejection/malformed/clock/alias refusal and scratch scrub;no native authority or WPA2/data acceptance");return 0;
}
#endif
