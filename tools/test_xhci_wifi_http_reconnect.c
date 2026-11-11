/* Actual xHCI reconnect/HTTP supervisor + production net/worker/CCMP/AP crypto.
 * Scheduler turns and child process APIs are modeled; no ELF/VM/native claim. */
#define SRVROS_XHCI_HTTP5_HOST_TEST 1
#define STACK5_TEST_MAIN http_reconnect_legacy_main
#define scheduler_current_thread_id legacy_thread_id
#define scheduler_yield legacy_yield
#define scheduler_wait_timeout legacy_wait_timeout
#define scheduler_wait legacy_wait
#define scheduler_wake_all legacy_wake_all
#define process_current legacy_current
#include "test_xhci_wifi_stack5.c"
#undef scheduler_current_thread_id
#undef scheduler_yield
#undef scheduler_wait_timeout
#undef scheduler_wait
#undef scheduler_wake_all
#undef process_current
#include <setjmp.h>
#include "../userspace/wifihttpprobe/http_exact.h"
static jmp_buf parked_worker,held_run;
static uint64_t current_thread=1;
static bool in_worker,in_child;
static unsigned workers,child_count,child_done,trial_case;
static struct {scheduler_thread_fn fn;void *arg;uint64_t id;} threads[2];
static struct {
 struct http_child_owner *owner;uint32_t ip;int64_t socket;
 char request[HTTP_EXACT_REQUEST_CAPACITY];size_t length,offset;
 struct http_exact_parser parser;bool go,done,eof;
} children[2];
static void child_turn(void){
 if(!child_count)return;
 unsigned g=child_count-1;if(!children[g].go || children[g].done)return;
 in_child=true;
 if(children[g].socket<=0){children[g].socket=net_connect_try(children[g].ip,80);assert(children[g].socket>0);goto done;}
 if(net_error((uint64_t)children[g].socket)==SRV_NETERR_INPROGRESS)goto done;
 assert(!net_error((uint64_t)children[g].socket));
 if(children[g].offset<children[g].length){
  int64_t n=net_respond_try_checked((uint64_t)children[g].socket,children[g].request+children[g].offset,children[g].length-children[g].offset,NULL);
  assert(n>0 || n==SRV_ERR_AGAIN);if(n>0)children[g].offset+=(size_t)n;goto done;
 }
 char buffer[512];int64_t n=net_read((uint64_t)children[g].socket,buffer,sizeof buffer,true);
 assert(n>=0 || n==SRV_ERR_AGAIN);
 if(n>0)assert(http_exact_feed(&children[g].parser,buffer,(size_t)n)==HTTP_EXACT_MORE);
 else if(!n){
  assert(http_exact_eof(&children[g].parser)==HTTP_EXACT_COMPLETE);children[g].eof=true;
  assert(!net_close((uint64_t)children[g].socket));children[g].done=true;child_done++;
 }
 done:in_child=false;
}
uint64_t scheduler_current_thread_id(void){return current_thread;}
struct process *process_current(void){return in_child?(struct process *)&children[child_count-1]:NULL;}
bool timer_idle_wakeup_ready(void){return true;}
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg){
 assert(!strcmp(name,"net-probe") && workers<2 && !in_worker && !in_child);
 if(trial_case==3 && workers==1)return false;
 threads[workers].fn=fn;threads[workers].arg=arg;threads[workers].id=2+workers;workers++;return true;
}
static void worker_turn(void){
 assert(workers && !in_worker);unsigned i=workers-1;current_thread=threads[i].id;in_worker=true;
 if(!setjmp(parked_worker))threads[i].fn(threads[i].arg);
 in_worker=false;current_thread=1;
}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg){
 assert(in_worker && !ready(arg));q->waiters=1;longjmp(parked_worker,1);
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t end){
 (void)q;(void)ready;(void)arg;(void)end;
 if(in_worker)longjmp(parked_worker,1);
 test_time+=10000;worker_turn();child_turn();return true;
}
void scheduler_wake_all(struct scheduler_wait_queue *q){q->waiters=0;}
void scheduler_yield(void){assert(!in_worker);test_time+=10000;worker_turn();child_turn();}
bool http_child_prepare(struct http_child_owner *o,bool dry){
 assert(!dry && !o->self && child_count<2 && !in_child && !in_worker);
 unsigned g=child_count++;children[g].owner=o;
 o->self=(uintptr_t)o;o->token=100+g;o->pid=200+g;o->ready_us=timer_monotonic_us();o->prepared=true;
 assert(http_exact_build_request(children[g].request,sizeof children[g].request,&children[g].length) && children[g].length==4159);
 http_exact_init(&children[g].parser);return true;
}
bool http_child_publish(struct http_child_owner *o,uint64_t epoch,uint64_t start,uint64_t end,uint32_t ip){
 unsigned g=child_count-1;assert(children[g].owner==o && !children[g].go && end==start+15000000 && epoch && ip);
 o->published=true;o->epoch=epoch;children[g].go=true;children[g].ip=ip;return true;
}
bool http_child_observe(struct http_child_owner *o,uint64_t *status){
 unsigned g=child_count-1;assert(children[g].owner==o);
 if(!children[g].done || o->reaped)return false;
 o->observed=true;*status=trial_case==4?1:0;return true;
}
bool http_child_cancel(struct http_child_owner *o){
 unsigned g=child_count-1;assert(children[g].owner==o);if(!children[g].go)children[g].done=true;return true;
}
bool http_child_reap(struct http_child_owner *o){
 struct net_probe_worker_info w={0};assert(net_probe_worker_snapshot(&w) && (!w.prepared || w.finished) && !w.active);
 assert(children[child_count-1].owner==o && children[child_count-1].done);o->reaped=true;return true;
}
bool http_child_retire(struct http_child_owner *o){assert(o->reaped);o->retired=true;return true;}
__attribute__((noreturn)) void wifi_http5_test_hold(void){longjmp(held_run,1);}
int main(int argc,char **argv){
 trial_case=argc==3 && !strcmp(argv[1],"--case")?(unsigned)strtoul(argv[2],NULL,10):0;assert(trial_case<6);
 if(argc==19 && !strcmp(argv[1],"--trace")){
  for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)reconnect_seed[d][i]=(uint32_t)strtoull(argv[2+d*8+i],NULL,0);
  reconnect_slot=(unsigned)strtoul(argv[18],NULL,0);assert(reconnect_slot && reconnect_slot<256);reconnect_seeded=true;
 }
 stream_setup();
 if(reconnect_seeded){for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,d?8:9,i,reconnect_seed[d][i]);wifi_cal5.d->slot_id=(uint8_t)reconnect_slot;}
 http_reconnect_peer=true;http_reconnect_peer_no_fin=trial_case==5;reconnect_hook_count=0;reconnect_case_id=trial_case==1?5:0;
 connection_dhcp_fault=trial_case==2?53:0;
 batch_trace=(argc==2 || argc==19) && !strcmp(argv[1],"--trace");
 if(batch_trace)puts("http-reconnect5-host: modeled_usb=1 modeled_scheduler=1 modeled_child=1 acceptance=0");
 if(setjmp(held_run)){
  struct http_network_probe *p=wifi_http5_active_probe;
  assert(wifi_http5_held_state.requested && wifi_http5_held_state.prepared &&
   wifi_http5_held_state.capture_attempted && wifi_http5_held_state.callback_attempted);
  assert(!wifi_http5_requested && !wifi_http5_prepared && !wifi_http5_capture_attempted && !wifi_http5_callback_attempted);
  if(trial_case==2){
   assert(wifi_http5_held_state.site==7 && !wifi_http_reconnect5.connections && p->result.reason==HTTP_NET_DHCP_FAILED && !p->result.quiescent);
   assert(wifi_stream5.exchange.handshake.keys_may_live && !wifi_stream5.exchange.keys_erased && !wifi_stack5.detached && !child_done);
   puts("xhci-http-reconnect5: PASS bad-MIC DHCP refusal through actual native path; live key/provider/worker/child retained;no second connection;acceptance=0");return 0;
  }
  if(trial_case==5){
   assert(wifi_http5_held_state.site==7 && !wifi_http_reconnect5.connections && p->result.reason==HTTP_NET_CLOCK);
   assert(p->result.handoff && p->result.go && !p->result.child_observed && !p->result.quiescent);
   assert(children[0].offset==4159 && children[0].parser.body_bytes==3072 && !children[0].eof);
   assert(!p->child.reaped && !p->child.retired && !p->result.disconnected);
   assert(wifi_stream5.exchange.handshake.keys_may_live && !wifi_stream5.exchange.keys_erased && !wifi_stack5.detached);
   puts("xhci-http-reconnect5: PASS missing genuine EOF retains child/worker/key/provider/native ownership at original deadline;no second connection;acceptance=0");return 0;
  }
  fprintf(stderr,"HTTP reconnect held: site=%llu gen=%u phase=%llu reason=%llu quiescent=%u workerfailed=%llu elapsed=%llu services=%llu childoffset=%zu eof=%u\n",
   (unsigned long long)wifi_http5_held_state.site,wifi_http_reconnect5.connections,
   (unsigned long long)p->result.phase,(unsigned long long)p->result.reason,(unsigned)p->result.quiescent,
   (unsigned long long)p->worker.failed,(unsigned long long)p->result.elapsed_us,
   (unsigned long long)wifi_stack5.services,children[child_count-1].offset,children[child_count-1].eof);return 1;
 }
 assert(xhci_wifi_http_reconnect5_prepare());
 assert(!xhci_wifi_capture_reconnect5_probe() && !xhci_wifi_http5_prepare() && !xhci_wifi_capture_stack5_probe());
 bool good=xhci_wifi_capture_http_reconnect5();
 bool complete=xhci_wifi_http_reconnect5_finish_capture(good);
 if(trial_case==1 || trial_case==3){
  assert(!good && !complete && wifi_http_reconnect5.connections==1 && child_done==1);
  assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[1].result.quiescent && !wifi_http_reconnect5.probe[1].result.attempted);
  assert(wifi_http_reconnect5.probe[1].result.child_retired && !wifi_stream5_slots[1].exchange.handshake.keys_may_live);
  assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream) || trial_case==3);
  printf("xhci-http-reconnect5: PASS case%u safe second preparation/handshake refusal, first actual HTTP complete;unused child/worker retired;acceptance=0\n",trial_case);return 0;
 }
 if(trial_case==4){
  const struct http_network_result *r=&wifi_http_reconnect5.probe[0].result;
  assert(!good && !complete && !wifi_http_reconnect5.connections && child_done==1);
  assert(r->reason==HTTP_NET_CHILD_FAILED && r->child_status==1 && !r->complete);
  assert(r->disconnected && r->quiescent && r->child_retired && r->worker_finished && r->tcp_empty && r->provider_drained && r->receiver_closed && r->refs_scrubbed);
  assert(wifi_stream5_slots[0].exchange.keys_erased && !wifi_http_reconnect5.probe[1].self);
  puts("xhci-http-reconnect5: PASS modeled nonzero child exit after real TCP/EOF safely retires first lifetime;no second connection;acceptance=0");return 0;
 }
 assert(complete);
 assert(workers==2 && child_count==2 && child_done==2 && reconnect_hook_count==2);
 for(unsigned g=0;g<2;g++){
  const struct http_network_result *r=&wifi_http_reconnect5.probe[g].result;
  assert(r->complete && r->disconnected && r->child_retired && r->worker_finished && r->receiver_closed && r->refs_scrubbed);
  assert(children[g].offset==4159 && children[g].eof && children[g].parser.body_bytes==3072);
  assert(wifi_stream5_slots[g].exchange.keys_erased && wifi_stream5_slots[g].retired && wifi_stream5_slots[g].connection_closed);
 }
 assert(wifi_stack5_net.state==MT76_NET5_RETIRED && wifi_stack5_net.registry_drained);
 assert(reconnect_keys_seen[0] && reconnect_keys_seen[1]);
 assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream));
 assert(!memcmp(&reconnect_old_fence,&wifi_fence5_first,sizeof reconnect_old_fence));
 for(unsigned i=0;i<10;i++)assert(!memcmp(reconnect_old_physical[i],i<2?(void *)wifi_stream5_slots[0].ring[i]:(void *)wifi_stream5_slots[0].buffer[(i-2)/4][(i-2)%4],4096));
 assert(wifi_http_reconnect5.epochs[1]>wifi_http_reconnect5.epochs[0] && wifi_http_reconnect5.tokens[1]>wifi_http_reconnect5.tokens[0]);
 assert(!xhci_wifi_capture_http_reconnect5() && !xhci_wifi_http_reconnect5_prepare());
 puts("xhci-http-reconnect5: PASS two fresh native USB/HTTP connection paths, actual production net/TCP/worker/CCMP, two4159-byte requests/exact3072-byte bodies/genuine EOF and complete native cleanup;modeled scheduler+child+USB;acceptance=0");
 return 0;
}
