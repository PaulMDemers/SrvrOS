/* Actual stack/registry/terminal API. Only the scheduler's initial parked
 * prepared-thread state and the wire peer are modeled. No native acceptance. */
#define SRVROS_NET_PREPARED_WORKER_HOST_TEST
#define BOUNDED_TEST_MAIN bounded_abort_legacy_main
#include "test_net_bounded_probe.c"

static unsigned inject,abort_calls;
static bool aborting;
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg){
 (void)name;(void)fn;(void)arg;assert(!"terminal abort cannot spawn a worker");return false;
}
static bool abort_owned(void *context,uint64_t epoch){
 bool good=bounded_owned(context,epoch);
 if(aborting && ++abort_calls==2)switch(inject){
  case 1:bounded->source=(void *)(uintptr_t)1;break;
  case 2:bounded->io_guard.context=(void *)(uintptr_t)1;break;
  case 3:probe_worker.finished=false;break;
  case 4:bounded->station=(void *)(uintptr_t)1;break;
  case 5:bounded_connection.provider.context=(void *)(uintptr_t)1;break;
  case 6:bounded->deadline_us=++bounded->deadline_guard;break;
  case 7:bounded->epoch=++bounded->epoch_guard;break;
  case 8:bounded->protocol_failed=false;break;
  case 9:bounded->last_us=0;break;
  default:break;
 }
 return good;
}
static void exercise(unsigned kind,const struct net_device_ops *ops){
 reset(NORMAL);owner_epoch=100;bounded=&connection_objects[0];
 probe_worker.attempted=probe_worker.prepared=probe_worker.parked=probe_worker.connection_mode=true;
 probe_worker.parked_queue.waiters=1;probe_worker.supervisor=1;probe_worker.thread=7;
 connection_ios[0]=(struct net_bounded_probe_io){&caller_token,abort_owned,bounded_ready};
 uint64_t deadline=test_us+15000000;
 assert(net_bounded_probe_init_connection(bounded,&connection_ios[0],ops,100,deadline));
 if(kind==36){
  assert(net_dhcp_request_bounded(bounded)>0 && local_ip && dhcp_configured);
  no_dns=true;uint32_t answer=0;assert(net_dns_resolve_bounded(bounded,"example.com",&answer)<0);
  assert(bounded->failed && bounded->protocol_failed && bounded->dhcp_observation.success);
 }else{
  scenario=ABSENT;
  assert(net_dhcp_request_bounded(bounded)<0 && bounded->failed && bounded->protocol_failed);
  assert(bounded->dhcp_observation.end_reason==NET_DHCP_TIME_LIMIT && bounded->dhcp_retries==1);
 }
 assert(net_probe_worker_stop() && net_probe_worker_finish());
 struct configuration original=config();const struct net_bounded_probe_io *io=&connection_ios[0];
 if(kind==1)bounded->protocol_failed=false;
 if(kind==2)bounded->failed=false;
 if(kind==3)bounded->busy=true;
 if(kind==4)bounded->checking=true;
 if(kind==5)bounded->closed=true;
 if(kind==6)bounded->source=(void *)(uintptr_t)1;
 if(kind==7)bounded->epoch++;
 if(kind==8)bounded->deadline_us++;
 if(kind==9)bounded->io_guard.context=(void *)(uintptr_t)1;
 if(kind==10)up=false;
 if(kind==11)bounded->mac[0]^=2;
 if(kind==12)bounded_connection.scope_identity++;
 if(kind==13)probe_worker.active=true;
 if(kind==14)probe_worker.busy=true;
 if(kind==15)probe_worker.failed=true;
 if(kind==16)probe_worker.finished=false;
 if(kind==17)probe_worker.ever_active=true;
 if(kind==18)probe_worker.source=(void *)(uintptr_t)1;
 if(kind==19)tcp_connections[0].state=TCP_STATE_TIME_WAIT;
 if(kind==20)udp_sockets[0].used=true;
 if(kind==21)test_us=deadline;
 if(kind==22)test_us=bounded->last_us-1;
 if(kind==23)bounded->polls=150000;
 if(kind==24)io=(void *)(uintptr_t)1;
 if(kind==25)probe_worker.guard.context=(void *)(uintptr_t)1;
 if(kind>=26 && kind<35)inject=kind-25;
 struct net_bounded_probe clone=*bounded;clone.identity=(uintptr_t)&clone;
 struct net_bounded_probe original_scope=*bounded;
 aborting=true;abort_calls=0;
 bool good=net_probe_worker_abort_connection(kind==35?&clone:bounded,io,100,deadline);
 assert(good==(kind==0 || kind==36));
 if(good){
  assert(bounded->failed && bounded->protocol_failed && bounded->closed && bounded->disconnected);
  assert(!bounded->source && !bounded->station && !bounded->io.context && !bounded->io_guard.context);
  assert(bounded_connection.failed && !bounded_connection.active && !bounded_connection.scope_identity && probe_worker.disconnected);
  assert(!local_ip && !dhcp_configured && !dhcp_have_offer && !dhcp_request_sent && !dns_server_ip && !dns_answer_ip);
  assert(!net_probe_worker_abort_connection(bounded,io,100,deadline));
  assert(!net_prepare_connection_probe_worker());
 }else{
  struct configuration after=config();assert(!memcmp(&original,&after,sizeof original));
  assert(bounded_connection.active && !bounded->disconnected && !probe_worker.disconnected);
  if(kind==35)assert(!abort_calls && !bounded_connection.failed && !memcmp(&original_scope,bounded,sizeof original_scope));
  if(kind!=12 && kind!=35)assert(bounded_connection.failed && bounded->failed);
  if(kind==9){
   bounded->io_guard.context=connection_ios[0].context;unsigned calls=abort_calls;
   assert(!net_probe_worker_abort_connection(bounded,&connection_ios[0],100,deadline) && abort_calls==calls);
  }
 }
}
int main(void){
 struct net_device_ops ops={.name="abort-peer",.context=&caller_token,.priority=40,.send=bounded_send,.poll=bounded_peer_poll,.link=provider_link};
 memcpy(ops.mac,station,6);assert(net_device_register(&ops));
 for(unsigned kind=0;kind<37;kind++){
  pid_t child=fork();assert(child>=0);
  if(!child){exercise(kind,&ops);_exit(0);}
  int status=0;assert(waitpid(child,&status,0)==child);
  if(!WIFEXITED(status) || WEXITSTATUS(status)){fprintf(stderr,"bootstrap-abort case%u failed\n",kind);return 1;}
 }
 puts("net-bootstrap-abort: 37 actual API cases PASS original DHCP2s/one retry and configured DNS failure, explicit failed-scope closure and fused reuse;foreign scope and owner/clock/source/worker/epoch/window/table/callback mutation refusals;modeled wire/parked thread;acceptance=0");
 return 0;
}
