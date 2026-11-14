/* Independent synthetic Ethernet peer;CCMP/MIC/raw USB belong to native owner. */
#include <srvros/mt76_stream5_probe.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t station[6]={2,6,7,8,9,10},gateway[6]={2,17,34,51,68,85},dns_mac[6]={2,85,102,119,136,153};
#define CLIENT 0xc0000216u
#define ROUTER 0xc0000201u
#define RESOLVER 0xc0000235u
static struct mt76_stream5_probe probe;
static struct {uint8_t incoming[1514],old[1514];size_t n,old_n;unsigned sends,polls,extra,mutation,mutate_at,loss_at,stall,waiting,actions;uint32_t xid;uint64_t now;bool clock_loss,frozen,empty,lost,refuse,uncertain,delay_fail,exhaust,backward,late,global_late,span,false_span,short_span;} ap;
static unsigned get16(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static uint32_t get32(const uint8_t *p){return (uint32_t)get16(p)*65536+get16(p+2);}
static void put16(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static void put32(uint8_t *p,uint32_t n){put16(p,n>>16);put16(p+2,n);}
static unsigned check(const uint8_t *p,size_t n,unsigned sum){for(size_t i=0;i<n;i+=2)sum+=(unsigned)p[i]*256+(i+1<n?p[i+1]:0);while(sum>>16)sum=(sum&65535)+(sum>>16);return (~sum)&65535;}
static void eth(unsigned proto,const uint8_t src[6]){memset(ap.incoming,0,sizeof ap.incoming);memcpy(ap.incoming,station,6);memcpy(ap.incoming+6,src,6);put16(ap.incoming+12,proto);}
static void ip(unsigned protocol,unsigned body,uint32_t src,uint32_t dst){uint8_t *p=ap.incoming+14;p[0]=0x45;put16(p+2,20+body);put16(p+4,123);put16(p+6,0x4000);p[8]=64;p[9]=protocol;put32(p+12,src);put32(p+16,dst);put16(p+10,check(p,20,0));ap.n=34+body;}
static void udp(unsigned sport,unsigned dport,unsigned payload,uint32_t src,uint32_t dst){ip(17,8+payload,src,dst);uint8_t *p=ap.incoming+34;put16(p,sport);put16(p+2,dport);put16(p+4,8+payload);unsigned sum=17+8+payload;const uint8_t *h=ap.incoming+26;for(unsigned i=0;i<8;i+=2)sum+=get16(h+i);put16(p+6,check(p,8+payload,sum));}
static void dhcp(unsigned kind){eth(0x800,gateway);uint8_t *p=ap.incoming+42;p[0]=2;p[1]=1;p[2]=6;put32(p+4,ap.xid);put32(p+16,CLIENT);memcpy(p+28,station,6);put32(p+236,0x63825363);unsigned at=240;p[at++]=53;p[at++]=1;p[at++]=kind;const unsigned opt[]={54,1,3,6,51};const uint32_t val[]={ROUTER,0xffffff00u,ROUTER,RESOLVER,3600};for(unsigned i=0;i<5;i++){p[at++]=opt[i];p[at++]=4;put32(p+at,val[i]);at+=4;}p[at++]=255;udp(67,68,300,ROUTER,0xffffffffu);}
static void arp_reply(const uint8_t *request){uint32_t target=get32(request+38);const uint8_t *src=target==ROUTER?gateway:dns_mac;eth(0x806,src);uint8_t *p=ap.incoming+14;put16(p,1);put16(p+2,0x800);p[4]=6;p[5]=4;put16(p+6,2);memcpy(p+8,src,6);put32(p+14,target);memcpy(p+18,station,6);put32(p+24,CLIENT);ap.n=42;}
static void dns_reply(const uint8_t *request){eth(0x800,dns_mac);uint8_t *p=ap.incoming+42;memcpy(p,request+42,29);put16(p+2,0x8180);put16(p+6,1);const uint8_t answer[]={0xc0,12,0,1,0,1,0,0,0,60,0,4,203,0,113,7};memcpy(p+29,answer,sizeof answer);udp(53,49161,45,RESOLVER,CLIENT);}
static void echo_reply(const uint8_t *request){eth(0x800,gateway);memcpy(ap.incoming+34,request+34,264);ap.incoming[34]=0;put16(ap.incoming+36,0);put16(ap.incoming+36,check(ap.incoming+34,264,0));ip(1,264,ROUTER,CLIENT);}
static void mutate(void){
 uint8_t *p=ap.incoming;
 switch(ap.mutation){case 1:p[41]^=1;break;case 2:p[39]^=1;break;case 3:p[50]^=1;break;case 4:p[20]|=0x20;break;case 5:p[14]=0x46;break;case 6:p[26]^=1;break;case 7:p[30]^=1;break;case 8:p[0]^=1;break;case 9:p[6]^=1;break;case 10:p[12]^=1;break;case 11:p[34]=8;break;case 12:p[35]=1;break;case 15:ap.n--;break;case 16:p[ap.n++]=0;break;case 17:assert(ap.old_n);memcpy(p,ap.old,ap.old_n);ap.n=ap.old_n;break;case 18:p[297]^=1;break;case 19:p[22]=0;break;case 21:put16(p+16,283);break;default:break;}
 if(ap.mutation!=13){put16(p+24,0);put16(p+24,check(p+14,20,0));}
 else p[24]^=1;
 if(ap.mutation!=14){put16(p+36,0);put16(p+36,check(p+34,264,0));}
 else p[36]^=1;
}
static bool owned(void *c){assert(c==&ap);return !ap.lost;}
static enum mt76_join5_send send_frame(void *c,const uint8_t *p,size_t n){
 assert(c==&ap && !ap.lost && !ap.n && n<=1514);ap.actions++;ap.sends++;
 assert(!memcmp(p+6,station,6));
 if(ap.sends<=6){
  assert(p==probe.setup.tx && probe.setup.sends==ap.sends);
  unsigned phase=ap.sends-1;
  if(phase<2){assert(n==342 && get32(p+46)==ap.xid && p[42]==1 && p[284]==(phase?3:1));dhcp(phase?5:2);}
  else if(phase==2 || phase==4){assert(n==42 && get16(p+20)==1);arp_reply(p);}
  else if(phase==3){assert(get16(p+34)==49161 && get16(p+36)==53);dns_reply(p);}
  else{assert(n==298 && get16(p+40)==1);echo_reply(p);}
 }else{
  unsigned index=ap.sends-7;assert(p==probe.tx && n==298 && index<64);
  assert(get16(p+12)==0x800 && p[14]==0x45 && get16(p+16)==284 && !check(p+14,20,0));
  assert(get32(p+26)==CLIENT && get32(p+30)==ROUTER && !memcmp(p,gateway,6));
  assert(p[34]==8 && !p[35] && !check(p+34,264,0));
  assert(get16(p+38)==(((ap.xid>>16)^0x5354)&65535) && get16(p+40)==0x4000+index);
  assert(get32(p+42)==0x53525354 && get32(p+46)==ap.xid && get32(p+50)==index);
  for(unsigned at=12;at<256;at++)assert(p[42+at]==(uint8_t)((ap.xid>>(8*(at%4)))+73*at+29*index+0x51));
  echo_reply(p);ap.extra++;ap.waiting=0;
  if(ap.mutation && index==ap.mutate_at)mutate();
  if(!ap.mutation || index!=ap.mutate_at){memcpy(ap.old,ap.incoming,ap.n);ap.old_n=ap.n;}
  if(ap.loss_at && index+1==ap.loss_at)ap.lost=true;
  if(ap.global_late)ap.now+=MT76_STREAM5_PROBE_TOTAL_US;
  if(ap.refuse || ap.uncertain){ap.n=0;return ap.uncertain?MT76_JOIN5_UNCERTAIN:MT76_JOIN5_REFUSED;}
 }
 return MT76_JOIN5_SENT;
}
static bool poll_frame(void *c,uint8_t *p,size_t cap,size_t *n,bool *present){
 assert(c==&ap && cap==1514 && !ap.lost);ap.actions++;assert(p==(ap.sends<=6?probe.setup.rx:probe.rx));ap.polls++;*n=0;*present=false;
 if(ap.sends>6 && ap.mutation==20)return false; /* Authenticated-transport refusal,not a fake MIC check. */
 if(ap.sends>6 && ap.span){*n=1515;*present=true;return true;}
 if(ap.sends>6 && ap.false_span){*n=1;return true;}
 if(ap.sends>6 && ap.short_span){*n=13;*present=true;return true;}
 if(ap.sends>6 && ap.waiting++<ap.stall)return true;
 if(ap.n && !(ap.sends>6 && ap.empty)){memcpy(p,ap.incoming,ap.n);*n=ap.n;*present=true;ap.n=0;}
 else if(!ap.frozen)ap.now+=100000;
 if(ap.sends>6 && ap.backward)ap.now=0;
 if(ap.sends>6 && ap.late)ap.now+=MT76_STREAM5_PROBE_WAIT_US;
 return true;
}
static bool delay(void *c,unsigned us){assert(c==&ap && !ap.lost && us==100);ap.actions++;if(!ap.frozen)ap.now+=us;return !ap.delay_fail;}
static uint64_t now(void *c){assert(c==&ap && !ap.lost);ap.actions++;if(ap.sends>6 && ap.clock_loss)ap.lost=true;return ap.now;}
static bool exhausted(void *c){assert(c==&ap && !ap.lost);ap.actions++;return ap.sends>6 && ap.exhaust;}
static void begin(uint32_t xid){memset(&probe,0,sizeof probe);memset(&ap,0,sizeof ap);ap.xid=xid;ap.now=1000;}
static bool run(void){const struct mt76_ip5_io io={&ap,owned,send_frame,poll_frame,delay,now,exhausted};return mt76_stream5_probe_run(&probe,station,ap.xid,&io);}
static void erased(void){
 uint8_t zeros[1514]={0};assert(!memcmp(probe.tx,zeros,1514) && !memcmp(probe.rx,zeros,1514));
 assert(!memcmp(probe.setup.tx,zeros,1514) && !memcmp(probe.setup.rx,zeros,1514) && !memcmp(probe.setup.request_digest,zeros,32));
 assert(!probe.xid && !probe.address && !probe.router && !probe.setup.xid && !probe.setup.address && !probe.setup.server && !probe.setup.mask && !probe.setup.router && !probe.setup.resolver && !probe.setup.answer);
 assert(!memcmp(probe.station,zeros,6) && !memcmp(probe.router_mac,zeros,6) && !memcmp(probe.setup.station,zeros,6) && !memcmp(probe.setup.dns_mac,zeros,6) && !memcmp(probe.setup.router_mac,zeros,6));
}
int main(void){
 const uint32_t xids[]={0x13572468,0xffffffff,1};
 for(unsigned i=0;i<3;i++){begin(xids[i]);assert(run());assert(probe.setup.complete && probe.setup.sends==6 && probe.setup.polls==6 && probe.setup.echo);assert(probe.complete && probe.echoes_sent==64 && probe.echoes_matched==64 && probe.polls==64 && probe.total_polls==70 && !probe.ignored && ap.sends==70);erased();assert(!run() && probe.complete);}
 for(unsigned mutation=1;mutation<=21;mutation++){
  begin(0x13572468);ap.mutation=mutation;if(mutation==17)ap.mutate_at=1;
  assert(!run() && !probe.complete && probe.setup.complete && probe.setup.sends==6);
  assert(probe.echoes_matched==(mutation==17?1u:0u));erased();
 }
 begin(7);ap.loss_at=1;assert(!run() && probe.unsafe);erased();
 begin(7);ap.loss_at=1;ap.refuse=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.clock_loss=true;ap.refuse=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.clock_loss=true;ap.late=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.lost=true;assert(!run() && probe.unsafe && !ap.actions && !ap.sends && !ap.polls);erased();
 begin(7);ap.refuse=true;assert(!run() && !probe.unsafe && probe.echoes_sent==0);erased();
 begin(7);ap.uncertain=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.span=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.false_span=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.short_span=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.backward=true;assert(!run() && probe.unsafe && probe.clock_fault);erased();
 begin(7);ap.late=true;assert(!run() && probe.timed_out);erased();
 begin(7);ap.global_late=true;assert(!run() && probe.timed_out && probe.elapsed_us>=MT76_STREAM5_PROBE_TOTAL_US);erased();
 begin(7);ap.empty=true;ap.delay_fail=true;assert(!run() && probe.unsafe);erased();
 begin(7);ap.exhaust=true;assert(!run() && probe.exhausted && !probe.unsafe);erased();
 begin(7);ap.empty=true;ap.frozen=true;assert(!run() && probe.exhausted && probe.polls==MT76_STREAM5_PROBE_POLLS);erased();
 begin(7);ap.stall=2500;ap.frozen=true;assert(!run() && probe.exhausted && probe.total_polls==MT76_STREAM5_PROBE_TOTAL_POLLS);erased();
 begin(7);struct mt76_ip5_io io={&ap,owned,send_frame,poll_frame,delay,now,exhausted};
 assert(!mt76_stream5_probe_run(NULL,station,7,&io));assert(!mt76_stream5_probe_run(&probe,station,0,&io));
 io.context=&probe;assert(!mt76_stream5_probe_run(&probe,station,7,&io));io.context=&ap;
 assert(!mt76_stream5_probe_run(&probe,probe.station,7,&io));
 uint8_t multicast[6]={1,2,3,4,5,6};assert(!mt76_stream5_probe_run(&probe,multicast,7,&io));
 probe.tx[0]=1;assert(!run() && !probe.attempted);
 puts("mt76-stream5-probe: PASS unchanged real six-request setup +64 unique strict256-byte gateway echoes,independent synthetic peer,reflection/stale/body/header/length/refusal,time/ownership/poll budgets and ephemeral erasure;no USB/CCMP/native acceptance");return 0;
}
