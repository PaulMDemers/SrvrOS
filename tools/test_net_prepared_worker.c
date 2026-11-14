/* Actual candidate net.c owner; HOST scheduler/provider/clock model only.
 * No native process, actual scheduling, USB, encrypted traffic or DMA claim. */
#define SRVROS_NET_PREPARED_WORKER_HOST_TEST 1
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/net.c"
static jmp_buf parked;
static uint64_t thread_id=1,ticks=100,us=1000000;
static scheduler_thread_fn captured;static void *captured_arg;
static unsigned spawns,polls,sends,allocs,frees,wakes,guards,acks;
static bool provider_match=true;
static uint64_t owner_epoch=42;
static struct net_device_ops provider;
static bool queued_tcp,nested_poll;static unsigned char process_token;
static bool owner=true,link_up=true,freeze,drop_arp,recurse,mutate,fail_inside_finish;
static uint32_t queued_arp;
static const uint8_t station[6]={2,0,0,0,0,1},router[6]={2,0,0,0,0,2};
static struct net_bounded_probe boot;
static struct net_bounded_probe_io io;
uint64_t scheduler_current_thread_id(void){return thread_id;}
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg){assert(!strcmp(name,"net-probe"));spawns++;captured=fn;captured_arg=arg;return true;}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg){assert((q==&probe_worker_slots[0].parked_queue || q==&probe_worker_slots[1].parked_queue) && !ready(arg));q->waiters=1;longjmp(parked,1);}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t end){assert(q==&rx_wait_queue && end==ticks+1);(void)ready;(void)arg;assert(!"turn driver must not sleep");return false;}
void scheduler_yield(void){assert(!"unexpected yield");}
void scheduler_wake_all(struct scheduler_wait_queue *q){wakes++;q->waiters=0;}
uint64_t timer_ticks(void){return ticks;}
uint64_t timer_monotonic_us(void){return us;}
bool process_should_exit_current(void){return false;}
bool process_signal_pending_current(void){return false;}
struct process *process_current(void){return (struct process *)&process_token;}
void process_file_poll_wake(void){}
void console_printf(const char *fmt,...){(void)fmt;assert(!"no live console");}
void console_write(const char *s){(void)s;assert(!"no live console");}
void *kmalloc(size_t n){allocs++;return malloc(n);}
void kfree(void *p){frees++;free(p);}
const uint8_t *net_device_mac(void){return station;}
bool net_device_link(struct net_device_link *out){*out=(struct net_device_link){.up=link_up};return true;}
bool net_device_select(void){return true;}
bool net_device_selected_is(const struct net_device_ops *p){return provider_match && p->context==provider.context && p->send==provider.send;}
const char *net_device_name(void){return "HOST-model";}
bool net_device_rx_wake_ready(void){return false;}
bool timer_delay_us(uint64_t n){(void)n;assert(!"no delay");return false;}
bool net_device_send(const uint8_t *p,uint16_t n){sends++;if(n>=54 && p[23]==6){if(p[47]&TCP_SYN)queued_tcp=true;else if(p[47]==TCP_ACK)acks++;}if(n==42 && p[12]==8 && p[13]==6 && p[21]==1)queued_arp=read_be32(p+38);return true;}
uint64_t net_device_poll(uint64_t budget,bool verbose){
 assert(budget==32 && !verbose);polls++;
 if(!freeze){us+=10000;ticks++;}
 if(nested_poll){nested_poll=false;assert(!probe_worker_device_poll(32,false));return 0;}
 if(queued_tcp){
  queued_tcp=false;uint8_t r[54]={0};struct tcp_connection *c=&tcp_connections[0];
  copy_bytes(r,station,6);copy_bytes(r+6,router,6);write_be16(r+12,0x800);r[14]=0x45;write_be16(r+16,40);r[22]=64;r[23]=6;write_be32(r+26,c->remote_ip);write_be32(r+30,local_ip);write_be16(r+24,checksum_bytes(r+14,20,0));
  write_be16(r+34,c->remote_port);write_be16(r+36,c->local_port);write_be32(r+38,3000);write_be32(r+42,c->local_seq);r[46]=0x50;r[47]=TCP_SYN|TCP_ACK;write_be16(r+48,4096);write_be16(r+50,tcp_checksum(c->remote_ip,local_ip,r+34,20));
  net_handle_ethernet_frame(r,54);return 1;
 }
 if(queued_arp && !drop_arp){uint8_t r[42]={0};copy_bytes(r,station,6);copy_bytes(r+6,router,6);write_be16(r+12,0x806);write_be16(r+14,1);write_be16(r+16,0x800);r[18]=6;r[19]=4;write_be16(r+20,2);copy_bytes(r+22,router,6);write_be32(r+28,queued_arp);copy_bytes(r+32,station,6);write_be32(r+38,local_ip);queued_arp=0;net_handle_ethernet_frame(r,42);return 1;}
 return 0;
}
static bool current(void *c,uint64_t epoch){assert(c==&owner && epoch==owner_epoch);guards++;if(fail_inside_finish && probe_worker.handoff_started && !probe_worker.ever_active)return false;if(recurse){recurse=false;assert(!probe_worker_guard());}if(mutate)io.context=NULL;return owner;}
static bool retry(void *c,uint64_t epoch){assert(c==&owner && epoch==owner_epoch);return owner;}
static void reset(void){
 probe_worker_selected=&probe_worker_slots[0];memset(probe_worker_slots,0,sizeof probe_worker_slots);memset(&bounded_connection,0,sizeof bounded_connection);provider_match=true;owner_epoch=42;
 memset(&probe_worker,0,sizeof probe_worker);memset(&boot,0,sizeof boot);memset(&io,0,sizeof io);
 memset(tcp_connections,0,sizeof tcp_connections);memset(listeners,0,sizeof listeners);memset(pending_requests,0,sizeof pending_requests);memset(udp_sockets,0,sizeof udp_sockets);memset(arp_cache,0,sizeof arp_cache);
 const uint8_t default_ip[4]={10,0,2,15};memcpy(local_ip_bytes,default_ip,4);
 initialized=worker_started=net_probe_quiet=dhcp_in_progress=dhcp_retry_active=dns_query_active=dns_waiting=icmp_ping_waiting=false;
 bounded_poll_owner=icmp_bounded_owner=NULL;local_mac=NULL;thread_id=1;ticks=100;us=1000000;spawns=polls=sends=allocs=frees=wakes=guards=acks=0;queued_tcp=nested_poll=false;owner=link_up=true;freeze=drop_arp=recurse=mutate=fail_inside_finish=false;queued_arp=0;captured=NULL;
}
static void park_entry(void){thread_id=1+spawns;if(!setjmp(parked))captured(captured_arg);thread_id=1;assert(probe_worker.parked && probe_worker.parked_queue.waiters==1);}
static void prepared(void){reset();assert(net_prepare_probe_worker() && spawns==1);assert(!net_probe_worker_prepared());park_entry();assert(net_probe_worker_prepared());assert(!net_prepare_probe_worker() && spawns==1);assert(!net_start_worker());}
static void bootstrap(void){prepared();assert(net_init_bounded_probe());io=(struct net_bounded_probe_io){&owner,current,retry};assert(net_bounded_probe_init(&boot,&io,42,15000000));}
static void active(void){bootstrap();assert(net_probe_worker_handoff(&boot));assert(boot.closed && !boot.source && !boot.io.current && !boot.station && worker_started);assert(probe_worker.start_us==1000000 && probe_worker.start_tick==100 && probe_worker.deadline_us==15000000);assert(!net_bounded_probe_poll(&boot));assert(net_probe_worker_current());}
static void turn(void){thread_id=probe_worker.thread;assert(probe_worker_turn());thread_id=1;}
static void connection_bootstrap(struct net_bounded_probe *b,struct net_bounded_probe_io *descriptor){
 *descriptor=(struct net_bounded_probe_io){&owner,current,retry};
 assert(net_bounded_probe_init_connection(b,descriptor,&provider,owner_epoch,us+15000000));
 assert(net_probe_worker_handoff(b) && b->closed && !b->failed && !bounded_connection.failed);
 assert(probe_worker.connection_scope==(uintptr_t)b && bounded_connection.active);
}
static void connection_finish(struct net_bounded_probe *b){
 assert(!net_probe_worker_disconnect(b));
 assert(net_probe_worker_empty() && net_probe_worker_stop());park_entry();
 assert(net_probe_worker_finish());
 assert(net_probe_worker_disconnect(b) && b->disconnected && !bounded_connection.active);
 assert(!net_probe_worker_disconnect(b));
}
static bool provider_send(void *c,const uint8_t *p,uint16_t n){assert(c==&provider);return net_device_send(p,n);}
static void connection_tests(void){
 static struct net_bounded_probe b[2];static struct net_bounded_probe_io descriptors[2];
 reset();memset(b,0,sizeof b);memset(descriptors,0,sizeof descriptors);
 provider=(struct net_device_ops){.context=&provider,.send=provider_send};memcpy(provider.mac,station,6);
 assert(net_prepare_connection_probe_worker());park_entry();assert(net_probe_worker_prepared());
 assert(!net_prepare_connection_probe_worker());
 connection_bootstrap(&b[0],&descriptors[0]);
 /* Real production TCP retirement, not application-close or zero refs. */
 tcp_connections[0]=(struct tcp_connection){.state=TCP_STATE_TIME_WAIT,.close_deadline=ticks+80};
 assert(!net_probe_worker_disconnect(&b[0]) && !net_probe_worker_empty());
 while(tcp_connections[0].state!=TCP_STATE_EMPTY)turn();
 connection_finish(&b[0]);
 assert(!local_ip && !dhcp_configured && !dns_answer_ready && !bounded_connection.scope_identity);
 struct probe_worker_state saved=probe_worker;
 unsigned before=spawns;assert(!net_prepare_connection_probe_worker() && spawns==before); /* still UP */
 link_up=false;
 for(unsigned fault=0;fault<9;fault++){
  if(fault==0)probe_worker.failed=true;
  if(fault==1)probe_worker.parked_queue.waiters=0;
  if(fault==2)probe_worker.busy=true;
  if(fault==3)probe_worker.poll_busy=true;
  if(fault==4)probe_worker.checking=true;
  if(fault==5)bounded_connection.active=true;
  if(fault==6)provider_match=false;
  if(fault==7)thread_id=99;
  if(fault==8)tcp_connections[0].tx_history[0].used=true;
  assert(!net_prepare_connection_probe_worker() && spawns==before && probe_worker_selected==&probe_worker_slots[0]);
  probe_worker=saved;bounded_connection.active=false;provider_match=true;thread_id=1;tcp_connections[0].tx_history[0].used=false;
 }
 assert(net_prepare_connection_probe_worker() && spawns==before+1);
 assert(!memcmp(&saved,&probe_worker_slots[0],sizeof saved));
 assert(!probe_worker_runnable(&probe_worker_slots[0]));
 /* A spurious wake of the original continuation still parks on original queue. */
 struct probe_worker_state *selected=probe_worker_selected;
 thread_id=2;if(!setjmp(parked))prepared_net_worker(&probe_worker_slots[0]);thread_id=1;
 assert(probe_worker_selected==selected && !memcmp(&saved,&probe_worker_slots[0],sizeof saved));
 park_entry();assert(net_probe_worker_prepared());
 assert(!net_probe_worker_disconnect(&b[0]) && !net_probe_worker_handoff(&b[0]));
 owner_epoch=43;link_up=true;connection_bootstrap(&b[1],&descriptors[1]);
 assert(probe_worker.thread!=probe_worker_slots[0].thread && b[1].start_us>b[0].start_us);
 connection_finish(&b[1]);link_up=false;
 assert(!net_prepare_connection_probe_worker() && spawns==2);
 assert(!memcmp(&saved,&probe_worker_slots[0],sizeof saved));
 puts("net-connection-workers: PASS two actual stack handoffs/deconfigurations,80tickTIMEWAIT,fresh retained threads,old continuation and nine next-admission refusals;HOST scheduling only");
}
int main(void){
 connection_tests();
 reset();initialized=true;assert(!net_prepare_probe_worker() && !spawns);reset();thread_id=0;assert(!net_prepare_probe_worker() && !spawns);
 prepared();assert(net_probe_worker_stop() && probe_worker.handoff_started && !probe_worker.ever_active);assert(net_probe_worker_finish() && probe_worker.finished);assert(!net_probe_worker_prepared());assert(!net_start_worker());struct net_bounded_probe unused={0};struct net_bounded_probe_io unused_io={&owner,current,retry};assert(!net_bounded_probe_init(&unused,&unused_io,42,us+1000000));
 bootstrap();uint8_t boot_before[sizeof boot];memcpy(boot_before,&boot,sizeof boot);assert(net_probe_worker_stop() && net_probe_worker_finish());assert(!net_probe_worker_prepared() && !net_probe_worker_handoff(&boot) && !memcmp(boot_before,&boot,sizeof boot) && !probe_worker.source);assert(net_bounded_probe_finish(&boot));
 bootstrap();thread_id=3;assert(!net_probe_worker_handoff(&boot) && !boot.closed && !worker_started);thread_id=1;assert(net_probe_worker_handoff(&boot));
 active();unsigned p=polls,a=allocs;for(unsigned i=0;i<5;i++)net_timer_tick();assert(polls==p && allocs==a && !probe_worker.turns);turn();assert(probe_worker.turns==1 && polls==p+1 && !allocs && !frees);
 /* A real wire-owned TIME_WAIT slot outlives zero app references and close. */
 tcp_connections[0]=(struct tcp_connection){.state=TCP_STATE_TIME_WAIT,.id=7,.close_deadline=ticks+80};
 assert(!net_probe_worker_empty());uint64_t end=tcp_connections[0].close_deadline;
 while(ticks+1<end){turn();assert(tcp_connections[0].state==TCP_STATE_TIME_WAIT && !net_probe_worker_empty());}
 turn();assert(ticks==end && tcp_connections[0].state==TCP_STATE_EMPTY && net_probe_worker_empty());
 assert(net_probe_worker_stop() && !worker_started && !probe_worker.active);assert(!net_probe_worker_finish());park_entry();assert(net_probe_worker_finish() && probe_worker.finished && !probe_worker.source && !probe_worker.io.current && !probe_worker.station);
 assert(!net_probe_worker_handoff(&boot) && !net_prepare_probe_worker());p=polls;net_timer_tick();assert(polls==p);
 /* SYN-ACK accepted while active poll legitimately emits ACK through guard;
  * that ordinary publication must not be mistaken for nested polling. */
 active();arp_cache_update(0x0a000202,router);thread_id=3;int64_t h=net_connect_try(0xc0000214,80);assert(h>0 && queued_tcp);thread_id=1;turn();assert(!probe_worker.failed && tcp_connections[0].state==TCP_STATE_ESTABLISHED && acks==1 && sends==2);
 active();nested_poll=true;thread_id=2;assert(!probe_worker_turn() && probe_worker.failed && !probe_worker.poll_busy);thread_id=1;
 /* Active table forms block clean finish; stopping is not an erase grant. */
 for(unsigned type=0;type<5;type++){
  active();if(type==0)tcp_connections[0].state=TCP_STATE_TIME_WAIT;if(type==1)listeners[0].used=true;if(type==2)pending_requests[0].used=true;if(type==3)udp_sockets[0].used=true;if(type==4)tcp_connections[0].tx_history[0].used=true;
  assert(!net_probe_worker_empty());assert(net_probe_worker_stop());park_entry();assert(!net_probe_worker_finish() && probe_worker.source);
 }
 /* Owner/link/callback/recursive/clock/deadline failures forbid publication. */
 for(unsigned type=0;type<8;type++){
  active();if(type==0)owner=false;if(type==1)link_up=false;if(type==2)io.context=NULL;if(type==3)recurse=true;if(type==4)mutate=true;if(type==5)ticks--;if(type==6)us--;if(type==7)us=probe_worker.deadline_us;
  unsigned before=sends;uint8_t frame[42]={0};assert(!send_ethernet(frame,42) && sends==before && probe_worker.failed && !probe_worker.active && !worker_started);
 }
 /* Fault stop does not release borrowed source before exact parked ack. */
 active();us=probe_worker.deadline_us;assert(!net_probe_worker_current() && probe_worker.failed);assert(probe_worker.source);probe_worker.busy=true;assert(!net_probe_worker_stop() && probe_worker.source);probe_worker.busy=false;assert(net_probe_worker_stop());assert(!net_probe_worker_finish() && probe_worker.source);park_entry();assert(!net_probe_worker_finish() && probe_worker.finished && !probe_worker.source); /* contained failure, never acceptance */
 active();probe_worker.polls=149999;thread_id=2;assert(!probe_worker_turn() && probe_worker.polls==150000);thread_id=1;
 /* Specifically lose original owner in the SECOND validation inside old
  * bootstrapfinish: no active worker ever existed, but transfer closes all
  * packet/poll paths and allows only parked/empty terminal reference scrub. */
 bootstrap();fail_inside_finish=true;assert(!net_probe_worker_handoff(&boot));assert(boot.closed && boot.failed && probe_worker.handoff_started && !probe_worker.ever_active && !worker_started && probe_worker.source);
 unsigned prior_sends=sends,prior_polls=polls;uint8_t refused[42]={0};assert(!send_ethernet(refused,42));assert(!probe_worker_device_poll(32,false) && sends==prior_sends && polls==prior_polls);
 struct net_probe_worker_info info={0};uint64_t saved_us=us,saved_tick=ticks,saved_guard_calls=guards;assert(net_probe_worker_snapshot(&info) && info.failed && info.handoff_started && !info.ever_active);assert(us==saved_us && ticks==saved_tick && guards==saved_guard_calls);
 assert(net_probe_worker_stop());assert(!net_probe_worker_finish() && probe_worker.finished && !probe_worker.source && !probe_worker.io.current && !probe_worker.station);assert(net_probe_worker_snapshot(&info) && info.finished && info.failed);
 /* Handoff retains an already-used aggregate poll budget. */
 bootstrap();boot.polls=123;assert(net_probe_worker_handoff(&boot) && probe_worker.polls==123);
 /* Cold target route ARP and cache hit use actual bounded production parser. */
 bootstrap();assert(!net_prepare_route_bounded(&boot,0xc0000214) && sends==1 && polls==1 && boot.arp_resolutions==1);uint8_t found[6];assert(arp_cache_lookup(0x0a000202,found) && !memcmp(found,router,6) && !arp_cache_lookup(0xc0000214,found));p=polls;unsigned s=sends;assert(!net_prepare_route_bounded(&boot,0xc0000214) && polls==p && sends==s);assert(net_connect_try(0,0)<0);
 bootstrap();assert(net_prepare_route_bounded(&boot,0)<0 && boot.failed && !sends);
 bootstrap();freeze=drop_arp=true;assert(net_prepare_route_bounded(&boot,0xc0000214)<0 && boot.failed && polls==10000 && sends==1);
 bootstrap();boot.deadline_us=boot.deadline_guard=us;assert(net_prepare_route_bounded(&boot,0xc0000214)<0 && !sends);
 puts("net-prepared-worker: PASS HOST lifecycle/park/handoff/IRQwake/actual80tickTIMEWAIT/tableempty/guard/clock/budget/routeARP tests;no actual scheduler/USB/native claim");
}
