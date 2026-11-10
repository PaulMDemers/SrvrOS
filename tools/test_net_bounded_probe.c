/* Reuse the independent DHCP wire peer and actual stack/registry host stubs.
 * The historical main remains separate;new APIs below may never use its wait
 * hooks (waits must stay zero). No native USB/CCMP claim follows. */
#define main dhcp_legacy_test_main
#include "test_net_dhcp_retry.c"
#undef main
static struct net_bounded_probe objects[64],*bounded;
static struct net_bounded_probe_io ios[64];
static unsigned objects_used,total_sends,bpolls,arps,queries,pings,automatic;
static uint64_t owner_epoch=42;
static bool freeze_ticks,freeze_mono,no_arp,no_dns,no_ping,first_dns_drop;
static bool lose_in_poll,lose_request,late,corrupt_body,short_body,nested,arp_request,icmp_request,refused;
static bool local_late;
static uint64_t queued_tick,queued_us;
static uint8_t incoming[1514];static uint16_t incoming_size;
static void packet(uint8_t *p,unsigned protocol,unsigned size,uint32_t source,uint32_t destination){
 memset(p,0,34+size);memcpy(p,station,6);memcpy(p+6,server_mac,6);put16(p+12,0x800);
 p[14]=0x45;put16(p+16,20+size);p[22]=64;p[23]=(uint8_t)protocol;put32(p+26,source);put32(p+30,destination);
 put16(p+24,(~sum(p+14,20,0))&65535);
}
static void queue_arp(uint32_t address,bool request){
 assert(!incoming_size);memset(incoming,0,42);memcpy(incoming,station,6);memcpy(incoming+6,server_mac,6);
 put16(incoming+12,0x806);put16(incoming+14,1);put16(incoming+16,0x800);incoming[18]=6;incoming[19]=4;
 put16(incoming+20,request?1:2);memcpy(incoming+22,server_mac,6);put32(incoming+28,address);
 memcpy(incoming+32,station,6);put32(incoming+38,0x0a00020f);incoming_size=42;queued_tick=test_ticks;queued_us=test_us;
}
static void queue_dns(const uint8_t *sent,unsigned n){
 assert(!incoming_size && n>=54);unsigned dns_size=p16(sent+38)-8;
 assert(dns_size+16<=512);packet(incoming,17,8+dns_size+16,p32(sent+30),p32(sent+26));
 put16(incoming+34,53);put16(incoming+36,53000);put16(incoming+38,8+dns_size+16);
 uint8_t *d=incoming+42;memcpy(d,sent+42,dns_size);put16(d+2,0x8180);put16(d+6,1);
 unsigned at=dns_size;d[at++]=0xc0;d[at++]=12;put16(d+at,1);put16(d+at+2,1);put32(d+at+4,60);put16(d+at+8,4);at+=10;put32(d+at,0x01020304);
 unsigned count=8+dns_size+16,check=(~sum(incoming+34,count,sum(incoming+26,8,17+count)))&65535;
 put16(incoming+40,check?check:65535);incoming_size=(uint16_t)(42+dns_size+16);queued_tick=test_ticks;queued_us=test_us;
}
static void queue_icmp(const uint8_t *sent,unsigned n,bool request){
 assert(!incoming_size && n==74);unsigned size=short_body?39:40;packet(incoming,1,size,p32(sent+30),p32(sent+26));
 memcpy(incoming+34,sent+34,size);incoming[34]=request?8:0;if(corrupt_body)incoming[42]^=1;
 put16(incoming+36,0);put16(incoming+36,(~sum(incoming+34,size,0))&65535);incoming_size=(uint16_t)(34+size);queued_tick=test_ticks;queued_us=test_us;
}
static bool bounded_send(void *context,const uint8_t *p,uint16_t n){
 assert(context==&caller_token && n>=42 && n<=1514 && !memcmp(p+6,station,6));total_sends++;
 if(refused)return false;
 if(p16(p+12)==0x806){assert(n==42);if(p16(p+20)==2){automatic++;return true;}
  assert(p16(p+20)==1);arps++;if(!no_arp)queue_arp(p32(p+38),false);return true;}
 assert(p16(p+12)==0x800 && p[14]==0x45 && p16(p+16)==(unsigned)n-14 && sum(p+14,20,0)==65535);
 if(p[23]==1){if(!p[34]){automatic++;return true;}
  assert(n==74 && p[34]==8 && sum(p+34,40,0)==65535);pings++;
  static const char text[]="srvros ping";for(unsigned i=0;i<32;i++)assert(p[42+i]==(i<sizeof(text)-1?(uint8_t)text[i]:0));
  if(!no_ping)queue_icmp(p,n,false);
  return true;}
 assert(p[23]==17 && p16(p+38)==(unsigned)n-34);
 if(p16(p+36)==67)return provider_send(context,p,n);
 assert(p16(p+36)==53 && p16(p+34)==53000);queries++;
 if(!no_dns && !(first_dns_drop && queries==1))queue_dns(p,n);
 return true;
}
static bool bounded_owned(void *context,uint64_t epoch){assert(epoch==42);return epoch==owner_epoch && owned(context);}
static bool bounded_ready(void *context,uint64_t epoch){assert(epoch==42);return ready(context);}
static uint64_t bounded_peer_poll(void *context,uint64_t budget,bool verbose){
 assert(context==&caller_token && budget==32 && !verbose);bpolls++;
 if(!freeze_ticks)test_ticks++;
 if(!freeze_mono)test_us+=10000;
 if(scenario==ROLL_TICK)test_ticks-=2;
 if(scenario==ROLL_US)test_us-=20000;
 if(nested){nested=false;assert(!net_bounded_probe_poll(bounded));return 0;}
 if(arp_request){arp_request=false;queue_arp(0x0a000202,true);owner=false;}
 if(icmp_request){icmp_request=false;uint8_t p[74];packet(p,1,40,0x0a00020f,0x0a000202);p[34]=8;
  put16(p+38,0x7372);put16(p+40,1);queue_icmp(p,74,true);owner=false;}
 if(lose_in_poll)owner=false;
 if(dhcp_in_progress){if(lose_request)owner=false;if(late && requests)test_us=bounded->deadline_us;return provider_poll(context,budget,verbose);}
 if(!incoming_size)return 0;
 uint8_t p[1514];uint16_t n=incoming_size;memcpy(p,incoming,n);incoming_size=0;memset(incoming,0,sizeof incoming);
 if(late)test_us=bounded->deadline_us;
 if(local_late){test_us=queued_us+1000000;test_ticks=queued_tick+100;}
 net_handle_ethernet_frame(p,n);return 1;
}
static void clean_bounded(void){assert(!bounded_poll_owner && !icmp_bounded_owner && !bounded->busy && !bounded->checking && !dns_query_active && !dns_waiting && !icmp_ping_waiting && !waits);}
static void finish_old(void){if(bounded && !bounded->closed){(void)net_bounded_probe_finish(bounded);assert(bounded->closed && !bounded->source && !bounded->station && !bounded->io.context && !bounded->io_guard.context);}}
static void begin_bounded(void){
 finish_old();reset(NORMAL);assert(objects_used<64);bounded=&objects[objects_used];
 ios[objects_used]=(struct net_bounded_probe_io){&caller_token,bounded_owned,bounded_ready};objects_used++;
 total_sends=bpolls=arps=queries=pings=automatic=0;
 owner_epoch=42;
 freeze_ticks=freeze_mono=no_arp=no_dns=no_ping=first_dns_drop=false;
 lose_in_poll=lose_request=late=corrupt_body=short_body=nested=arp_request=icmp_request=refused=false;
 local_late=false;
 incoming_size=0;memset(incoming,0,sizeof incoming);memset(arp_cache,0,sizeof arp_cache);
 assert(net_bounded_probe_init(bounded,&ios[objects_used-1],42,test_us+15000000));
}
static void cache_address(uint32_t ip){queue_arp(ip,false);uint16_t n=incoming_size;incoming_size=0;net_handle_ethernet_frame(incoming,n);memset(incoming,0,sizeof incoming);}
static void no_more(void){unsigned count=total_sends;assert(!net_bounded_probe_poll(bounded));assert(net_ping_bounded(bounded,0x0a000202,99,NULL)<0);assert(total_sends==count);clean_bounded();}
int main(void){
 struct net_device_ops ops={.name="bounded-peer",.context=&caller_token,.priority=40,.send=bounded_send,.poll=bounded_peer_poll,.link=provider_link};
 memcpy(ops.mac,station,6);assert(net_device_register(&ops) && net_init_bounded_probe());
 begin_bounded();assert(net_dhcp_request_bounded(bounded)==0x0a00020f && discovers==1 && requests==1);
 const struct net_dhcp_observation receipt=bounded->dhcp_observation;
 assert(receipt.version==1 && receipt.started && receipt.finished && receipt.success &&
  receipt.end_reason==NET_DHCP_SUCCESS && receipt.discovers==1 && receipt.discover_queued==1 &&
  receipt.requests==1 && receipt.request_queued==1 && receipt.accepted_offer==1 && receipt.accepted_ack==1 &&
  receipt.frames==2 && receipt.dhcp==2 && receipt.acks==1 && receipt.offers==1 && !receipt.rejected);
 uint32_t answer=0;assert(!net_dns_resolve_bounded(bounded,"example.com",&answer) && answer==0x01020304 && queries==1);
 for(unsigned i=0;i<64;i++){uint64_t elapsed=999;assert(!net_ping_bounded(bounded,0x0a000202,(uint16_t)i,&elapsed) && elapsed==1);}
 assert(bounded->pings_sent==64 && bounded->pings_matched==64 && bounded->dns_queries==1 && bounded->arp_resolutions==2);
 assert(!memcmp(&receipt,&bounded->dhcp_observation,sizeof receipt));
 assert(net_bounded_probe_finish(bounded) && bounded->closed && bounded->identity==(uintptr_t)bounded);
 assert(!memcmp(&receipt,&bounded->dhcp_observation,sizeof receipt));
 assert(!net_bounded_probe_init(bounded,&ios[objects_used-1],42,test_us+15000000));no_more();
 begin_bounded();scenario=DROP_FIRST;assert(net_dhcp_request_bounded(bounded)==0x0a00020f && discovers==2 && ready_calls==1 && bounded->dhcp_retries==1);clean_bounded();
 begin_bounded();scenario=ABSENT;freeze_ticks=freeze_mono=true;
 assert(net_dhcp_request_bounded(bounded)<0 && bpolls==20000 && discovers==1 && !ready_calls);
 assert(bounded->dhcp_observation.end_reason==NET_DHCP_POLL_LIMIT && !bounded->dhcp_observation.frames &&
  bounded->dhcp_observation.finished && !bounded->dhcp_observation.success);no_more();
 begin_bounded();scenario=TX_UNCERTAIN;assert(net_dhcp_request_bounded(bounded)<0 && discovers==1 && ready_calls==1);no_more();
 begin_bounded();lose_request=true;assert(net_dhcp_request_bounded(bounded)<0 && discovers==1 && !requests);no_more();
 begin_bounded();scenario=LINK_OWNER_LOST_ACK;assert(net_dhcp_request_bounded(bounded)<0 && discovers==1 && requests==1 && !dhcp_configured);no_more();
 begin_bounded();late=true;assert(net_dhcp_request_bounded(bounded)<0 && requests==1 && !dhcp_configured);no_more();
 begin_bounded();scenario=ABSENT;freeze_ticks=true;assert(net_dhcp_request_bounded(bounded)<0 && discovers==1 && !ready_calls);no_more();
 begin_bounded();scenario=ABSENT;freeze_mono=true;assert(net_dhcp_request_bounded(bounded)<0 && discovers==1 && !ready_calls);no_more();
 begin_bounded();freeze_ticks=freeze_mono=no_arp=true;answer=0xdeadbeef;
 assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && answer==0xdeadbeef && arps==1 && !queries && bpolls==10000);no_more();
 begin_bounded();cache_address(0x0a000203);freeze_ticks=freeze_mono=no_dns=true;
 assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && queries==1 && bpolls==10000);no_more();
 begin_bounded();first_dns_drop=true;assert(!net_dns_resolve_bounded(bounded,"example.com",&answer) && queries==2 && bounded->dns_queries==2);clean_bounded();
 begin_bounded();freeze_ticks=freeze_mono=no_arp=true;assert(net_ping_bounded(bounded,0x0a000202,1,NULL)<0 && !pings && bpolls==10000);no_more();
 begin_bounded();cache_address(0x0a000202);freeze_ticks=freeze_mono=no_ping=true;uint64_t elapsed=0xdeadbeef;
 assert(net_ping_bounded(bounded,0x0a000202,1,&elapsed)<0 && elapsed==0xdeadbeef && pings==1 && bpolls==10000);no_more();
 begin_bounded();corrupt_body=true;assert(net_ping_bounded(bounded,0x0a000202,1,NULL)<0 && pings==1 && !bounded->pings_matched);no_more();
 begin_bounded();short_body=true;assert(net_ping_bounded(bounded,0x0a000202,1,NULL)<0 && pings==1 && !bounded->pings_matched);no_more();
 begin_bounded();cache_address(0x0a000203);late=true;answer=0xdeadbeef;
 assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && answer==0xdeadbeef);no_more();
 begin_bounded();cache_address(0x0a000202);late=true;elapsed=0xdeadbeef;
 assert(net_ping_bounded(bounded,0x0a000202,1,&elapsed)<0 && elapsed==0xdeadbeef && !bounded->pings_matched);no_more();
 begin_bounded();local_late=true;answer=0xdeadbeef;
 assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && answer==0xdeadbeef && arps==1 && !queries);no_more();
 begin_bounded();cache_address(0x0a000203);local_late=true;answer=0xdeadbeef;
 assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && answer==0xdeadbeef && queries==3);no_more();
 begin_bounded();cache_address(0x0a000202);local_late=true;elapsed=0xdeadbeef;
 assert(net_ping_bounded(bounded,0x0a000202,1,&elapsed)<0 && elapsed==0xdeadbeef && !bounded->pings_matched);no_more();
 begin_bounded();cache_address(0x0a000202);scenario=LINK_OWNER_LOST;
 assert(net_ping_bounded(bounded,0x0a000202,1,NULL)<0 && !total_sends);no_more();
 begin_bounded();ios[objects_used-1].context=NULL;assert(net_dhcp_request_bounded(bounded)<0 && !total_sends);no_more();
 begin_bounded();owner_epoch=43;assert(net_dhcp_request_bounded(bounded)<0 && !total_sends);no_more();
 begin_bounded();scenario=EXIT_ROLLBACK;assert(!net_bounded_probe_current(bounded) && !total_sends);no_more();
 begin_bounded();scenario=FINAL_CURRENT_ROLLBACK;guard_calls=0;assert(!net_bounded_probe_current(bounded) && !total_sends);no_more();
 begin_bounded();lose_in_poll=true;assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && !queries);no_more();
 begin_bounded();scenario=ROLL_TICK;assert(net_ping_bounded(bounded,0x0a000202,1,NULL)<0 && arps==1 && !pings);no_more();
 begin_bounded();scenario=ROLL_US;assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0 && arps==1 && !queries);no_more();
 begin_bounded();refused=true;assert(net_dhcp_request_bounded(bounded)<0 && total_sends==1);no_more();
 begin_bounded();nested=true;assert(!net_bounded_probe_poll(bounded) && bpolls==1);no_more();
 begin_bounded();arp_request=true;assert(!net_bounded_probe_poll(bounded) && !total_sends && !automatic);no_more();
 begin_bounded();icmp_request=true;assert(!net_bounded_probe_poll(bounded) && !total_sends && !automatic);no_more();
 begin_bounded();test_us=bounded->deadline_us;assert(!net_bounded_probe_current(bounded));no_more();
 begin_bounded();cache_address(0x0a000202);test_us=bounded->deadline_us-15000;
 assert(!net_ping_bounded(bounded,0x0a000202,1,NULL));assert(net_ping_bounded(bounded,0x0a000202,2,NULL)<0 && bounded->pings_sent==2 && bounded->pings_matched==1);no_more();
 begin_bounded();freeze_ticks=freeze_mono=true;for(unsigned i=0;i<149999;i++)assert(net_bounded_probe_poll(bounded));
 assert(!net_bounded_probe_poll(bounded) && bounded->polls==150000 && bpolls==150000);no_more();
 finish_old();
 puts("net-bounded-probe: PASS production DHCP/DNS/ARP/64exact40-byteICMP,shared owner/absolute clock/poll bounds,cache/frozen/rollback/late/payload/refusal/recursive and nested-publication guards,no waits/delays,closed tombstone;synthetic plaintext peer,no native acceptance");
 return 0;
}
