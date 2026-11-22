/* Independent public Ethernet peer;loss/retry semantics,not native USB/CCMP. */
#define main stream_probe_legacy_main
#include "test_mt76_stream5_probe.c"
#undef main
static struct {
 unsigned discovers,extras,fail_send,fail_result,deadline_reads,deadline_exhaustions;
 uint64_t offer_at;
 bool drop_first,drop_all,drop_ack,malformed_first,reflect_first,poll_fail;
 bool frozen,backward,rollback_before_retry,rollback_in_exhaustion,exhaust_after_poll,loss_at_deadline,global_late,delay_failure;
} retry;
static bool retry_owned(void *c){assert(c==&ap);return !ap.lost;}
static enum mt76_join5_send retry_send(void *c,const uint8_t *p,size_t n){
 assert(c==&ap && !ap.lost && n<=1514);ap.actions++;ap.sends++;
 assert(!memcmp(p+6,station,6));
 if(ap.sends==retry.fail_send){ap.n=0;return (enum mt76_join5_send)retry.fail_result;}
 if(p==probe.setup.tx){
  unsigned phase=probe.setup.phase;assert(probe.setup.sends==ap.sends);
  if(get16(p+12)==0x800){assert(p[14]==0x45 && get16(p+16)==n-14 && !check(p+14,20,0));assert(get16(p+18)==((ap.xid+ap.sends-1)&65535));}
  if(phase<2){
   assert(n==342 && get32(p+46)==ap.xid && p[42]==1 && p[43]==1 && p[44]==6 && p[284]==(phase?3:1));
   assert(get16(p+52)==0x8000 && !memcmp(p+70,station,6));
   const uint8_t *h=p+14,*u=p+34;unsigned sum=17+n-34;for(unsigned i=0;i<8;i+=2)sum+=get16(h+12+i);assert(!check(u,n-34,sum));
   if(!phase){
    retry.discovers++;assert(retry.discovers<=2 && probe.setup.discover_retries==retry.discovers-1);
    if(retry.drop_all || (retry.drop_first && retry.discovers==1)){ap.n=0;return MT76_JOIN5_SENT;}
    dhcp(2);
    if(retry.malformed_first && retry.discovers==1){ap.incoming[46]^=1;put16(ap.incoming+40,0);unsigned sum2=17+308;for(unsigned i=0;i<8;i+=2)sum2+=get16(ap.incoming+26+i);put16(ap.incoming+40,check(ap.incoming+34,308,sum2));}
    if(retry.reflect_first && retry.discovers==1){memcpy(ap.incoming,p,n);ap.n=n;}
   }else if(retry.drop_ack)ap.n=0;
   else dhcp(5);
  }else if(phase==2 || phase==4){assert(n==42 && get16(p+20)==1);arp_reply(p);}
  else if(phase==3){assert(get16(p+34)==49161 && get16(p+36)==53);dns_reply(p);}
  else{assert(phase==5 && n==298 && get16(p+40)==1);echo_reply(p);}
 }else{
  assert(p==probe.tx && n==298 && probe.setup.complete);unsigned index=retry.extras++;
  assert(index<64 && get16(p+40)==0x4000+index && get32(p+42)==0x53525354 && get32(p+46)==ap.xid && get32(p+50)==index);
  assert(!check(p+14,20,0) && !check(p+34,264,0));
  for(unsigned at=12;at<256;at++)assert(p[42+at]==(uint8_t)((ap.xid>>(8*(at%4)))+73*at+29*index+0x51));
  echo_reply(p);
 }
 return MT76_JOIN5_SENT;
}
static bool retry_poll(void *c,uint8_t *p,size_t cap,size_t *n,bool *present){
 assert(c==&ap && !ap.lost && cap==1514);ap.actions++;ap.polls++;*n=0;*present=false;
 assert(p==(probe.setup.complete?probe.rx:probe.setup.rx));
 if(retry.poll_fail)return false;
 if(!retry.frozen)ap.now+=100000;
 if(retry.backward)ap.now=0;
 if(retry.global_late)ap.now+=MT76_STREAM5_PROBE_TOTAL_US;
 if(ap.n && (!retry.offer_at || probe.setup.phase || ap.now>=retry.offer_at)){
  memcpy(p,ap.incoming,ap.n);*n=ap.n;*present=true;ap.n=0;
 }
 return true;
}
static bool retry_delay(void *c,unsigned us){assert(c==&ap && !ap.lost && us==100);ap.actions++;if(!retry.frozen)ap.now+=us;return !retry.delay_failure;}
static uint64_t retry_now(void *c){
 assert(c==&ap && !ap.lost);ap.actions++;
 if(retry.loss_at_deadline && ap.now>=2001000)ap.lost=true;
 if(retry.rollback_before_retry && ap.now>=2001000 && ++retry.deadline_reads==2)ap.now=0;
 return ap.now;
}
static bool retry_exhausted(void *c){
 assert(c==&ap && !ap.lost);ap.actions++;
 if(retry.rollback_in_exhaustion && ap.now>=2001000 && ++retry.deadline_exhaustions==2)ap.now=0;
 return retry.exhaust_after_poll && ap.polls;
}
static void retry_begin(void){begin(0x13572468);memset(&retry,0,sizeof retry);}
static bool retry_run(bool stream,bool legacy){
 const struct mt76_ip5_io io={&ap,retry_owned,retry_send,retry_poll,retry_delay,retry_now,retry_exhausted};
 if(stream)return legacy?mt76_stream5_probe_run(&probe,station,ap.xid,&io):mt76_stream5_probe_run_discover_retry(&probe,station,ap.xid,&io);
 return legacy?mt76_ip5_run(&probe.setup,station,ap.xid,&io):mt76_ip5_run_discover_retry(&probe.setup,station,ap.xid,&io);
}
static void request_erased(void){uint8_t zeros[1514]={0};assert(!memcmp(probe.setup.tx,zeros,1514) && !memcmp(probe.setup.rx,zeros,1514) && !memcmp(probe.setup.request_digest,zeros,32) && !probe.setup.request_size);}
int main(void){
 for(unsigned stream=0;stream<2;stream++){
  retry_begin();assert(retry_run(stream,false));assert(probe.setup.complete && probe.setup.sends==6 && probe.setup.discover_retries==0 && retry.discovers==1 && ap.sends==(stream?70u:6u));request_erased();if(stream)erased();
  retry_begin();retry.drop_first=true;assert(retry_run(stream,false));assert(probe.setup.complete && probe.setup.sends==7 && probe.setup.discover_retries==1 && retry.discovers==2 && ap.sends==(stream?71u:7u));assert(probe.setup.dhcp_class[0][MT76_IP5_MATCHED]==1 && probe.setup.dhcp_class[1][MT76_IP5_MATCHED]==1);request_erased();if(stream){assert(probe.complete && probe.echoes_matched==64 && probe.total_polls>71 && !probe.timed_out);erased();}
  retry_begin();retry.offer_at=1901000;assert(retry_run(stream,false));assert(!probe.setup.discover_retries && retry.discovers==1);request_erased();if(stream)erased();
  retry_begin();retry.offer_at=2101000;assert(retry_run(stream,false));assert(probe.setup.discover_retries==1 && retry.discovers==2);request_erased();if(stream)erased();
  retry_begin();retry.drop_all=true;assert(!retry_run(stream,false));assert(probe.setup.sends==2 && probe.setup.discover_retries==1 && retry.discovers==2 && !probe.setup.unsafe && !probe.setup.complete && ap.now>=4001000);request_erased();if(stream)erased();
  retry_begin();retry.drop_ack=true;assert(!retry_run(stream,false));assert(probe.setup.phase==1 && probe.setup.sends==2 && !probe.setup.discover_retries && retry.discovers==1);request_erased();if(stream)erased();
  retry_begin();retry.malformed_first=true;assert(retry_run(stream,false));assert(probe.setup.discover_retries==1 && probe.setup.dhcp_class[0][MT76_IP5_XID]==1 && probe.setup.dhcp_class[0][MT76_IP5_MATCHED]==1);request_erased();if(stream)erased();
  retry_begin();retry.reflect_first=true;assert(retry_run(stream,false));assert(probe.setup.discover_retries==1 && probe.setup.dhcp_reflected[0]==1 && probe.setup.dhcp_class[0][MT76_IP5_UDP_PORTS]==1 && probe.setup.dhcp_class[0][MT76_IP5_MATCHED]==1);request_erased();if(stream)erased();
  for(unsigned result=MT76_JOIN5_REFUSED;result<=MT76_JOIN5_UNCERTAIN;result++){
   retry_begin();retry.fail_send=1;retry.fail_result=result;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && !retry.discovers);request_erased();if(stream)erased();
   retry_begin();retry.drop_first=true;retry.fail_send=2;retry.fail_result=result;assert(!retry_run(stream,false));assert(ap.sends==2 && probe.setup.discover_retries==1 && retry.discovers==1);request_erased();if(stream)erased();
  }
  retry_begin();retry.drop_first=true;retry.backward=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries);request_erased();if(stream)erased();
  retry_begin();retry.drop_first=true;retry.frozen=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.polls==20000);request_erased();if(stream)erased();
  retry_begin();retry.drop_first=true;retry.exhaust_after_poll=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.exhausted);request_erased();if(stream)erased();
  retry_begin();retry.drop_first=true;retry.loss_at_deadline=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.unsafe);request_erased();if(stream)erased();
  retry_begin();retry.drop_first=true;retry.rollback_in_exhaustion=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.unsafe);request_erased();if(stream)erased();
  retry_begin();retry.poll_fail=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.unsafe);request_erased();if(stream)erased();
  retry_begin();retry.drop_first=true;retry.delay_failure=true;assert(!retry_run(stream,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.unsafe);request_erased();if(stream)erased();
  /* The unchanged default remains a one-Discover/no-retry operation. */
  retry_begin();retry.drop_first=true;assert(!retry_run(stream,true));assert(ap.sends==1 && retry.discovers==1 && !probe.setup.discover_retries);request_erased();if(stream)erased();
 }
 retry_begin();retry.drop_first=true;retry.rollback_before_retry=true;assert(!retry_run(false,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.setup.unsafe);request_erased();
 retry_begin();retry.drop_first=true;retry.global_late=true;assert(!retry_run(true,false));assert(ap.sends==1 && !probe.setup.discover_retries && probe.timed_out);erased();
 retry_begin();probe.setup.discover_retries=1;assert(!retry_run(false,false) && !probe.setup.attempted && !ap.sends);
 retry_begin();probe.setup.discover_retries=1;assert(!retry_run(true,false) && !probe.attempted && !ap.sends);
 puts("mt76-ip5-retry: PASS explicit one-Discover retry on true2s timeout,sameXID/actualIPv4ID/sends,categorical refusal/reflection retention,6or7 setup plus64 exact echoes,legacy no-retry,loss/delay/noOffer/ACK/refusal/uncertainty/clock/poll/exhaustion/global limits and private erasure;synthetic only");
 return 0;
}
