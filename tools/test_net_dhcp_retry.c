/* Actual production stack+registry;independent plaintext DHCP packet peer.
 * This proves protocol retry admission,not native USB/CCMP completion. */
#define SRVROS_NET_DEVICE_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/net_device.c"
#include "../kernel/src/net.c"

enum scenario { NORMAL,DROP_FIRST,LATE_OFFER,ABSENT,ACK_ABSENT,FIRST_REFUSED,
 RETRY_REFUSED,REQUEST_REFUSED,DOWN_INITIAL,DOWN_AFTER_SEND,OWNER_LOST,
 TX_UNCERTAIN,ROLL_TICK,ROLL_US,FROZEN,FROZEN_TICK,FROZEN_US,
 READY_ROLLBACK,LINK_ROLLBACK,READY_LOST,MUTATE_GUARD,LATE_ACK,UNSOLICITED_ACK,
 LINK_OWNER_LOST,LINK_OWNER_LOST_ACK,LINK_OWNER_LOST_RETRY,EXIT_ROLLBACK,
 EXIT_ROLLBACK_ACK,FINAL_CURRENT_ROLLBACK };
static enum scenario scenario;
static uint64_t test_ticks,test_us,first_tick,first_us;
static unsigned discovers,requests,polls,waits,ready_calls,guard_calls,nested_calls;
static bool up,known,owner,current_tx_completed,offer_delivered,ack_delivered;
static uint32_t peer_xid;
static uint8_t sent[3][1514];static uint16_t sent_size[3];static unsigned sent_count;
static struct net_dhcp_retry_guard guard;
static const uint8_t station[6]={2,1,2,3,4,5},server_mac[6]={2,9,8,7,6,5};
static unsigned char caller_token;
uint64_t timer_ticks(void){return test_ticks;}
uint64_t timer_monotonic_us(void){return test_us;}
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}
void console_printf(const char *f,...){(void)f;}
void console_write(const char *p){(void)p;}
struct process *process_current(void){return (struct process *)&caller_token;}
/* Inject a clock fault at the final process-state read;the production helper
 * is a plain flag read. This tests ordering,not a native process-clock issue. */
bool process_should_exit_current(void){
 if(scenario==EXIT_ROLLBACK || (scenario==EXIT_ROLLBACK_ACK && dhcp_configured))test_us--;
 return false;
}
bool process_signal_pending_current(void){return false;}
void process_file_poll_wake(void){}
uint64_t scheduler_current_thread_id(void){return 1;}
void scheduler_yield(void){}
void scheduler_wake_all(struct scheduler_wait_queue *q){(void)q;}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a){(void)q;(void)fn;(void)a;assert(0);return false;}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a,uint64_t end){
 (void)fn;(void)a;assert(q==&rx_wait_queue && end==test_ticks+1);waits++;
 if(scenario==ROLL_TICK){test_ticks--;return false;}
 if(scenario==ROLL_US){test_us--;return false;}
 if(scenario!=FROZEN && scenario!=FROZEN_TICK)test_ticks++;
 if(scenario!=FROZEN && scenario!=FROZEN_US)test_us+=10000;
 return false;
}
/* All packet construction,inspection and checksums below are independent of
 * the production serializer/parser/checksum helpers. */
static unsigned p16(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static uint32_t p32(const uint8_t *p){return (uint32_t)p16(p)*65536+p16(p+2);}
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){put16(p,v>>16);put16(p+2,v);}
static unsigned sum(const uint8_t *p,size_t n,unsigned s){
 for(size_t i=0;i<n;i+=2)s+=(unsigned)p[i]*256+(i+1<n?p[i+1]:0);
 while(s>>16)s=(s&65535)+(s>>16);
 return s;
}
static unsigned message_type(const uint8_t *p,size_t n){
 assert(n==300 && p[0]==1 && p[1]==1 && p[2]==6 && p32(p+236)==0x63825363);
 unsigned type=0;
 for(size_t i=240;i<n;){unsigned code=p[i++];if(code==255)break;if(!code)continue;
  assert(i<n);size_t len=p[i++];assert(len<=n-i);if(code==53){assert(len==1 && !type);type=p[i];}i+=len;}
 return type;
}
static bool provider_send(void *context,const uint8_t *p,uint16_t n){
 assert(context==&caller_token && n==342 && sent_count<3);
 assert(!memcmp(p+6,station,6) && p16(p+12)==0x800 && p[14]==0x45 && p[23]==17);
 assert(p16(p+16)==328 && p16(p+34)==68 && p16(p+36)==67 && p16(p+38)==308);
 assert(sum(p+14,20,0)==65535);
 assert(!p16(p+40) || sum(p+34,308,sum(p+26,8,17+308))==65535);
 const uint8_t *dhcp=p+42;assert(!memcmp(dhcp+28,station,6));unsigned type=message_type(dhcp,300);
 memcpy(sent[sent_count],p,n);sent_size[sent_count++]=n;
 if(type==1){
  discovers++;if(discovers==1){peer_xid=p32(dhcp+4);first_tick=test_ticks;first_us=test_us;}
  else {assert(discovers==2 && p32(dhcp+4)==peer_xid && !memcmp(dhcp,sent[0]+42,300));
   assert(test_ticks-first_tick>=100 && test_us-first_us>=1000000 && test_ticks-first_tick<200 && test_us-first_us<2000000);
   assert(p16(p+18)==(p16(sent[0]+18)+1u)%65536u);}
  if(scenario==FIRST_REFUSED || (scenario==RETRY_REFUSED && discovers==2))return false;
  current_tx_completed=scenario!=TX_UNCERTAIN;
 }else {assert(type==3 && p32(dhcp+4)==peer_xid && ++requests==1 && offer_delivered);
  if(scenario==REQUEST_REFUSED)return false;}
 if(scenario==DOWN_AFTER_SEND)up=false;
 return true;
}
static void peer_frame(uint8_t frame[342],unsigned type){
 memset(frame,0,342);memcpy(frame,station,6);memcpy(frame+6,server_mac,6);put16(frame+12,0x800);
 uint8_t *ip=frame+14,*udp=frame+34,*d=frame+42;
 ip[0]=0x45;put16(ip+2,328);ip[8]=64;ip[9]=17;put32(ip+12,0x0a000202);put32(ip+16,0xffffffff);
 put16(udp,67);put16(udp+2,68);put16(udp+4,308);
 d[0]=2;d[1]=1;d[2]=6;put32(d+4,peer_xid);put32(d+16,0x0a00020f);memcpy(d+28,station,6);put32(d+236,0x63825363);
 size_t at=240;d[at++]=53;d[at++]=1;d[at++]=(uint8_t)type;
 d[at++]=54;d[at++]=4;put32(d+at,0x0a000202);at+=4;
 d[at++]=3;d[at++]=4;put32(d+at,0x0a000202);at+=4;
 d[at++]=6;d[at++]=4;put32(d+at,0x0a000203);at+=4;
 d[at++]=1;d[at++]=4;put32(d+at,0xffffff00);at+=4;
 d[at++]=51;d[at++]=4;put32(d+at,3600);at+=4;d[at]=255;
 put16(ip+10,(~sum(ip,20,0))&65535);put16(udp+6,(~sum(udp,308,sum(ip+12,8,17+308)))&65535);
}
static void peer_reply(unsigned type){
 uint8_t frame[342];peer_frame(frame,type);
 net_handle_ethernet_frame(frame,sizeof frame);
}
static uint64_t provider_poll(void *context,uint64_t budget,bool verbose){
 assert(context==&caller_token && budget==32 && !verbose);polls++;
 if(scenario==OWNER_LOST){owner=false;return 0;}
 if(!discovers || scenario==ABSENT || scenario==TX_UNCERTAIN || scenario==FROZEN ||
    scenario==FROZEN_TICK || scenario==FROZEN_US || scenario==READY_ROLLBACK ||
    scenario==LINK_ROLLBACK || scenario==READY_LOST || scenario==RETRY_REFUSED ||
    scenario==LINK_OWNER_LOST_RETRY)return 0;
 if(scenario==UNSOLICITED_ACK){if(!ack_delivered){ack_delivered=true;peer_reply(5);return 1;}return 0;}
 if(!offer_delivered){
  if((scenario==DROP_FIRST && discovers<2) || (scenario==LATE_OFFER && test_ticks-first_tick<100))return 0;
  offer_delivered=true;peer_reply(2);return 1;
 }
 if(requests && !ack_delivered && scenario!=ACK_ABSENT && scenario!=REQUEST_REFUSED){
  if(scenario==LATE_ACK){test_ticks=first_tick+200;test_us=first_us+2000000;}
  ack_delivered=true;peer_reply(5);return 1;
 }
 return 0;
}
static bool provider_link(void *context,struct net_device_link *out){
 assert(context==&caller_token);*out=(struct net_device_link){.up=up};
 /* Generic link callbacks may quarantine ownership after sampling up. The
  * current native mt76 callback only reads state;this is a contract fixture. */
 if(scenario==LINK_OWNER_LOST || (scenario==LINK_OWNER_LOST_ACK && dhcp_configured) ||
    (scenario==LINK_OWNER_LOST_RETRY && ready_calls))owner=false;
 if(scenario==LINK_ROLLBACK && ready_calls)test_us--;
 return known;
}
static bool owned(void *context){
 assert(context==&caller_token);guard_calls++;
 if(scenario==FINAL_CURRENT_ROLLBACK && guard_calls==2)test_us--;
 if(scenario==MUTATE_GUARD)guard.context=NULL;
 return owner;
}
static bool ready(void *context){
 assert(context==&caller_token);ready_calls++;
 if(scenario==READY_ROLLBACK)test_us--;
 if(scenario==READY_LOST)owner=false;
 return current_tx_completed;
}
static bool nested_owned(void *context){
 assert(net_dhcp_request()==SRV_ERR_AGAIN);nested_calls++;
 return owned(context);
}
static void reset(enum scenario mode){
 scenario=mode;test_ticks=100;test_us=1000000;discovers=requests=polls=waits=ready_calls=guard_calls=nested_calls=sent_count=0;
 up=mode!=DOWN_INITIAL;known=owner=true;current_tx_completed=offer_delivered=ack_delivered=false;
 memset(sent,0,sizeof sent);memset(sent_size,0,sizeof sent_size);
 dhcp_retry_active=dhcp_configured=dhcp_in_progress=dhcp_have_offer=dhcp_request_sent=false;
 dhcp_xid=0x73727672;ip_ident=0;set_local_ip_bytes((const uint8_t[]){10,0,2,15});
 guard=(struct net_dhcp_retry_guard){&caller_token,owned,ready};
}
static int64_t run(enum scenario mode){reset(mode);int64_t result=net_dhcp_request_discover_retry(&guard);assert(!dhcp_retry_active && !dhcp_in_progress);return result;}
struct configuration {
 uint32_t address,source,relay;uint8_t bytes[20];
 bool configured,in_progress,offered,requested;
};
static struct configuration config(void){
 struct configuration c={.address=local_ip,.source=dhcp_retry_source_ip,.relay=dhcp_retry_relay_ip,
  .configured=dhcp_configured,.in_progress=dhcp_in_progress,.offered=dhcp_have_offer,.requested=dhcp_request_sent};
 memcpy(c.bytes,local_ip_bytes,4);memcpy(c.bytes+4,dhcp_server_ip_bytes,4);
 memcpy(c.bytes+8,dhcp_router_ip_bytes,4);memcpy(c.bytes+12,dhcp_dns_ip_bytes,4);memcpy(c.bytes+16,dhcp_offered_ip_bytes,4);
 return c;
}
static void unchanged(const struct configuration *before){
 struct configuration after=config();
 assert(before->address==after.address && before->source==after.source && before->relay==after.relay &&
  before->configured==after.configured && before->in_progress==after.in_progress &&
  before->offered==after.offered && before->requested==after.requested && !memcmp(before->bytes,after.bytes,20));
}
static void rechecksum(uint8_t frame[342],unsigned payload){
 uint8_t *ip=frame+14,*udp=frame+34;put16(ip+2,28+payload);put16(udp+4,8+payload);
 put16(ip+10,0);put16(udp+6,0);put16(ip+10,(~sum(ip,20,0))&65535);
 unsigned check=(~sum(udp,8+payload,sum(ip+12,8,17+8+payload)))&65535;put16(udp+6,check?check:65535);
}
static void rejected(uint8_t frame[342],unsigned payload){
 struct configuration before=config();unsigned request_count=requests;rechecksum(frame,payload);
 net_handle_ethernet_frame(frame,(uint16_t)(42+payload));unchanged(&before);assert(requests==request_count);
}
static void admission_begin(bool ack){
 reset(NORMAL);dhcp_retry_active=dhcp_in_progress=true;peer_xid=dhcp_xid;
 dhcp_retry_source_ip=dhcp_retry_relay_ip=0;
 memset(dhcp_server_ip_bytes,0xa1,4);memset(dhcp_router_ip_bytes,0xb2,4);
 memset(dhcp_dns_ip_bytes,0xc3,4);memset(dhcp_offered_ip_bytes,0xd4,4);
 if(ack){offer_delivered=true;peer_reply(2);assert(dhcp_have_offer && dhcp_request_sent && requests==1);}
}
static size_t option_at(const uint8_t *d,unsigned option){
 for(size_t at=240;at<300;){unsigned code=d[at++];if(code==255)break;if(!code)continue;
  unsigned n=d[at++];assert(n<=300-at);if(code==option)return at-2;at+=n;}
 assert(0);return 0;
}
static void admission(void){
 uint8_t frame[342];
 for(unsigned ack=0;ack<2;ack++){
  admission_begin(ack!=0);unsigned type=ack?5:2;
  /* Every truncation before the mandatory END is a refusal,even when the
   * enclosing IP/UDP packet and checksum are otherwise exactly valid. */
  peer_frame(frame,type);size_t end=0;for(size_t at=240;at<300;){unsigned code=frame[42+at++];if(code==255){end=at-1;break;}if(code)at+=1+frame[42+at];}
  assert(end);for(unsigned n=0;n<=end;n++){peer_frame(frame,type);rejected(frame,n);}
  for(unsigned mutation=0;mutation<26;mutation++){
   peer_frame(frame,type);uint8_t *d=frame+42;
   switch(mutation){
   case 0:d[0]=1;break;
   case 1:d[1]=2;break;
   case 2:d[2]=5;break;
   case 3:d[4]^=1;break;
   case 4:d[28]^=2;break;
   case 5:d[236]^=1;break;
   case 6:put32(d+16,0);break;
   case 7:put32(d+16,0xe0000001);break;
   case 8:put32(d+16,0x7f000001);break;
   case 9:d[option_at(d,53)]=99;break;
   case 10:d[option_at(d,54)]=99;break;
   case 11:memset(d+end,0,300-end);break;
   case 12:d[end+1]=1;break;
   case 13:d[end]=52;d[end+1]=1;d[end+2]=1;d[end+3]=255;break;
   case 14:d[end]=99;d[end+1]=255;break;
   case 15:d[option_at(d,53)+1]=2;break;
   case 16:d[option_at(d,54)+1]=3;break;
   case 17:put32(d+option_at(d,54)+2,0xffffffff);break;
   case 18:put32(d+option_at(d,1)+2,0xffff00ff);break;
   case 19:d[option_at(d,3)+1]=5;break;
   case 20:d[option_at(d,6)+1]=0;break;
   case 21:put32(d+option_at(d,51)+2,0);break;
   case 22:put32(d+option_at(d,3)+2,0x0b000202);break;
   case 23:put32(d+option_at(d,6)+2,0);break;
   case 24:put32(frame+26,0);break;
   case 25:put32(d+24,0xe0000001);break;
   }
   rejected(frame,300);
  }
  static const unsigned critical[]={53,54,1,3,6,51};
  for(unsigned i=0;i<sizeof critical/sizeof critical[0];i++){
   peer_frame(frame,type);uint8_t *d=frame+42;size_t at=option_at(d,critical[i]),size=2+d[at+1];
   memcpy(d+end,d+at,size);d[end+size]=255;rejected(frame,300);
  }
  if(ack){
   peer_frame(frame,5);put32(frame+42+16,0x0a000210);rejected(frame,300);
   peer_frame(frame,5);put32(frame+42+option_at(frame+42,54)+2,0x0a000204);rejected(frame,300);
   peer_frame(frame,5);put32(frame+26,0x0a000204);rejected(frame,300);
   peer_frame(frame,5);put32(frame+42+24,0x0a000204);rejected(frame,300);
   peer_frame(frame,5);frame[42+option_at(frame+42,51)]=99;rejected(frame,300);
  }
  peer_frame(frame,type);struct configuration before=config();frame[22]=0;rechecksum(frame,300);
  net_handle_ethernet_frame(frame,sizeof frame);unchanged(&before); /* TTL0. */
  peer_frame(frame,type);before=config();rechecksum(frame,300);put16(frame+38,307);
  put16(frame+40,0);net_handle_ethernet_frame(frame,sizeof frame);unchanged(&before); /* IP tail outside UDP. */
 }
 admission_begin(false);peer_frame(frame,5);rejected(frame,300); /* No Offer/Request. */
 dhcp_retry_active=dhcp_in_progress=false;
}
int main(void){
 struct net_device_ops ops={.name="dhcp-peer",.context=&caller_token,.priority=40,.send=provider_send,.poll=provider_poll,.link=provider_link};
 memcpy(ops.mac,station,6);assert(net_device_register(&ops));net_init();assert(initialized);
 assert(run(NORMAL)==0x0a00020f && discovers==1 && requests==1 && !ready_calls);
 uint8_t ordinary_frames[2][1514];uint16_t ordinary_sizes[2];memcpy(ordinary_frames,sent,sizeof ordinary_frames);memcpy(ordinary_sizes,sent_size,sizeof ordinary_sizes);
 assert(run(DROP_FIRST)==0x0a00020f && discovers==2 && requests==1 && ready_calls==1);
 assert(run(LATE_OFFER)==0x0a00020f && discovers==1 && requests==1 && !ready_calls);
 assert(run(ABSENT)<0 && discovers==2 && !requests && ready_calls==1 && test_ticks-first_tick==200);
 assert(run(ACK_ABSENT)<0 && discovers==1 && requests==1 && !ready_calls);
 assert(run(FIRST_REFUSED)<0 && discovers==1 && !polls && !ready_calls);
 assert(run(RETRY_REFUSED)<0 && discovers==2 && !requests && ready_calls==1);
 assert(run(REQUEST_REFUSED)<0 && discovers==1 && requests==1 && !ready_calls && polls==1);
 assert(run(DOWN_INITIAL)<0 && !discovers);
 assert(run(DOWN_AFTER_SEND)<0 && discovers==1 && !polls);
 assert(run(OWNER_LOST)<0 && discovers==1 && polls==1 && !ready_calls);
 assert(run(TX_UNCERTAIN)<0 && discovers==1 && ready_calls==1 && test_ticks-first_tick==100);
 assert(run(ROLL_TICK)<0 && discovers==1 && waits==1 && !ready_calls);
 assert(run(ROLL_US)<0 && discovers==1 && waits==1 && !ready_calls);
 assert(run(FROZEN)<0 && discovers==1 && polls==20000 && waits==20000 && !ready_calls);
 assert(run(FROZEN_TICK)<0 && discovers==1 && test_us-first_us==2000000 && !ready_calls);
 assert(run(FROZEN_US)<0 && discovers==1 && test_ticks-first_tick==200 && !ready_calls);
 assert(run(READY_ROLLBACK)<0 && discovers==1 && ready_calls==1);
 assert(run(LINK_ROLLBACK)<0 && discovers==1 && ready_calls==1);
 assert(run(READY_LOST)<0 && discovers==1 && ready_calls==1);
 assert(run(MUTATE_GUARD)<0 && !discovers);
 assert(run(LATE_ACK)<0 && discovers==1 && requests==1 && !dhcp_configured);
 assert(run(UNSOLICITED_ACK)<0 && discovers==2 && !requests && !dhcp_configured);
 assert(run(LINK_OWNER_LOST)<0 && !discovers && !polls);
 assert(run(LINK_OWNER_LOST_ACK)<0 && discovers==1 && requests==1 && !dhcp_configured);
 assert(run(LINK_OWNER_LOST_RETRY)<0 && discovers==1 && ready_calls==1);
 assert(run(EXIT_ROLLBACK)<0 && !discovers && !polls);
 assert(run(EXIT_ROLLBACK_ACK)<0 && discovers==1 && requests==1 && !dhcp_configured);
 assert(run(FINAL_CURRENT_ROLLBACK)<0 && !discovers && !polls);
 reset(NORMAL);known=false;assert(net_dhcp_request_discover_retry(&guard)<0 && !discovers);
 reset(NORMAL);guard.current=nested_owned;assert(net_dhcp_request_discover_retry(&guard)==0x0a00020f && nested_calls && discovers==1);
 reset(NORMAL);assert(net_dhcp_request()==0x0a00020f && discovers==1 && requests==1 && !ready_calls && !guard_calls);
 assert(!memcmp(ordinary_frames,sent,sizeof ordinary_frames) && !memcmp(ordinary_sizes,sent_size,sizeof ordinary_sizes));
 reset(DROP_FIRST);assert(net_dhcp_request()<0 && discovers==1 && !requests && !ready_calls && !guard_calls);
 reset(NORMAL);assert(net_dhcp_request_discover_retry(NULL)<0 && !discovers);
 guard.current=NULL;assert(net_dhcp_request_discover_retry(&guard)<0 && !discovers);
 admission();
 reset(NORMAL);assert(net_device_retire(&ops));
 assert(net_dhcp_request_discover_retry(&guard)<0 && !discovers);
 puts("net-dhcp-retry: PASS actual production stack/registry,independent DHCP peer;one explicit same-XID Discover retry,late Offer,absence,ACK/refused/uncertain/down/owner/clock/poll/callback refusals,legacy packet/API compatibility;no native USB/CCMP acceptance");
 return 0;
}
