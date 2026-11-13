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
bool http5_test_RF_parent(bool (*)(void),bool);
/* Counterfactual reproducer: discard a proved local close as the old caller did. */
static bool discard_closed_failure(void){assert(wifi_join5_after());return false;}
static bool legacy_rf_parent(bool (*callback)(void),bool expected){
 (void)callback;(void)expected;return http5_test_RF_parent(discard_closed_failure,false);
}
static bool no_target_parent(void){assert(wifi_http_reconnect5_unattempted() && !wifi_stream5.attempted);return true;}
/* Inject only after production HTTP has emitted its proved first close.
 * No shortcut invokes the guard instead of the actual supervisor path. */
static unsigned handoff_recheck_mode;
static bool handoff_recheck(void *owner,uint64_t epoch,const struct mt76_dma5 *dma){
 assert(wifi_reconnect5_closed(owner,epoch,dma));
 switch(handoff_recheck_mode){
 case 0:xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.control=(34u<<10)|(xhci.event_cycle?1u:0u)};break;
 case 1:wifi_fence5_first.frontier++;break;
 case 2:wifi_stream5.dma.epoch++;break;
 case 3:xhci.command_enqueue++;break;
 case 4:xhci.sync_users++;break;
 case 5:return false;
 default:abort();
 }
 return true;
}
static unsigned handoff_injections;
static void handoff_boundary(const char *fmt){
 if(strncmp(fmt,"wifi-http-reconnect5-connection:",32) || handoff_injections)return;
 assert(wifi_http_reconnect5.connections==1 && child_done==1 && wifi_reconnect5_closed(
  &wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma));
 handoff_injections++;
 if(trial_case==18 || trial_case==19){
  assert(wifi_fence5_empty());
  xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
   .parameter_low=(uint32_t)(uintptr_t)wifi_stream5.ring[1],.parameter_high=(uint32_t)((uintptr_t)wifi_stream5.ring[1]>>32),
   .status=1u<<24,.control=(trial_case==18?(34u<<10):((32u<<10)|(9u<<16)|(wifi_cal5.d->slot_id<<24)))|(xhci.event_cycle?1u:0u)};
 }else if(trial_case==20)xhci.command_pending=true;
 else if(trial_case==21)xhci.command_enqueue=56;
 else if(trial_case==22)wifi_fence5_second.bound=true;
 else if(trial_case==23)wifi_stream5.events_empty=false;
}
int main(int argc,char **argv){
 trial_case=argc==3 && (!strcmp(argv[1],"--case") || !strcmp(argv[1],"--trace-case"))?(unsigned)strtoul(argv[2],NULL,10):0;assert(trial_case<24);
 if(argc==19 && !strcmp(argv[1],"--trace")){
  for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)reconnect_seed[d][i]=(uint32_t)strtoull(argv[2+d*8+i],NULL,0);
  reconnect_slot=(unsigned)strtoul(argv[18],NULL,0);assert(reconnect_slot && reconnect_slot<256);reconnect_seeded=true;
 }
 stream_setup();
 if(trial_case>=18)test_console_printf_hook=handoff_boundary;
 if(trial_case==15)stream_test_before_connection=no_target_parent;
 if(trial_case==16)stream_test_outer_capture=http5_test_RF_parent;
 if(trial_case==17)stream_test_outer_capture=legacy_rf_parent;
 if(reconnect_seeded){for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,d?8:9,i,reconnect_seed[d][i]);wifi_cal5.d->slot_id=(uint8_t)reconnect_slot;}
 http_reconnect_peer=true;http_reconnect_peer_no_fin=trial_case==5;reconnect_hook_count=0;reconnect_case_id=trial_case==1?5:0;
 connection_dhcp_fault=(trial_case==2 || trial_case==16 || trial_case==17)?53:trial_case==12?56:trial_case==13?58:trial_case==14?59:0;
 if(trial_case>=6 && trial_case<12)reconnect_case_id=trial_case+13;
 batch_trace=((argc==2 || argc==19) && !strcmp(argv[1],"--trace")) || (argc==3 && !strcmp(argv[1],"--trace-case"));
 if(batch_trace)puts("http-reconnect5-host: modeled_usb=1 modeled_scheduler=1 modeled_child=1 acceptance=0");
 if(setjmp(held_run)){
  struct http_network_probe *p=wifi_http5_active_probe;
  assert(wifi_http5_held_state.requested && wifi_http5_held_state.prepared &&
   wifi_http5_held_state.capture_attempted && wifi_http5_held_state.callback_attempted);
  assert(!wifi_http5_requested && !wifi_http5_prepared && !wifi_http5_capture_attempted && !wifi_http5_callback_attempted);
  if(trial_case==13){
   assert(wifi_http5_held_state.site==7 && !p->result.quiescent && !p->result.receiver_closed && !p->result.disconnected);
   assert(wifi_stream5.pending[0] && wifi_stack5.tx_token && wifi_stack5_net.pending && wifi_stream5.exchange.handshake.keys_may_live);
   assert(!wifi_stream5.exchange.keys_erased && !p->child.retired && !wifi_stream5.connection_closed && !wifi_stack5.detached);
   puts("xhci-http-reconnect5: PASS uncertain DHCP TX remains held with original child/worker/key/provider/DMA;terminal protocol cleanup refuses pending debt;acceptance=0");return 0;
  }
  if(trial_case==2){
   fprintf(stderr,"Unexpected hold after ordinary DHCP protocol failure: site=%llu phase=%llu reason=%llu quiescent=%u bootstrap_closed=%u protocol_failed=%u worker_finished=%u\n",(unsigned long long)wifi_http5_held_state.site,(unsigned long long)p->result.phase,(unsigned long long)p->result.reason,p->result.quiescent,p->bootstrap.closed,p->bootstrap.protocol_failed,p->result.worker_finished);return 1;
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
 if(trial_case>=18){
  static const unsigned reasons[]={41,41,29,39,42,43};
  assert(!good && !complete && handoff_injections==1 && wifi_http_reconnect5.connections==1);
  assert(workers==1 && child_count==1 && child_done==1 && !wifi_http_reconnect5.probe[1].self);
  assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[0].result.quiescent);
  assert(wifi_fence5_handoff.captured && wifi_fence5_handoff.reason==reasons[trial_case-18]);
  assert(!wifi_fence5_connection_switched && wifi_fence5_active==&wifi_fence5_first &&
   wifi_stream5_current==&wifi_stream5_slots[0] && !wifi_fence5_next_owner);
  assert(wifi_stream5.exchange.keys_erased && wifi_stream5.retired && wifi_stack5.detached);
  if(trial_case==18 || trial_case==19){
   assert(wifi_fence5_handoff.event_valid && !wifi_fence5_empty());
   assert(((wifi_fence5_handoff.event_control>>10)&63)==(trial_case==18?34u:32u));
   assert(wifi_fence5_handoff.event_owner==(trial_case==18?0u:3u));
  }
  if(trial_case==23)assert(wifi_reconnect5_closed_reason==14);
  unsigned reason=wifi_fence5_handoff.reason;wifi_fence5_handoff_capture(999);
  assert(wifi_fence5_handoff.reason==reason); /* later observation cannot replace first cause */
  if(trial_case==18){
   /* After the actual supervisor refusal, exercise the same retained native
    * objects at each post-callback check. Only the modeled fixture rolls back
    * mutations between independent counterexamples; production never retries. */
   test_console_printf_hook=NULL;
   xhci.event_ring[xhci.event_dequeue].control=xhci.event_cycle?0u:1u;
   assert(wifi_reconnect5_closed(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma));
   struct xhci_trb *event_ring=xhci.event_ring;
   xhci.command_pending=true;xhci.event_ring=(struct xhci_trb *)(uintptr_t)1;
   memset(&wifi_fence5_handoff,0,sizeof wifi_fence5_handoff);
   assert(!wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,
    &wifi_stream5_slots[1],wifi_stream5.epoch+1,wifi_reconnect5_closed));
   assert(wifi_fence5_handoff.reason==29 && !wifi_fence5_handoff.event_valid);
   xhci.command_pending=false;xhci.event_ring=event_ring;
   static const unsigned expected[]={56,44,45,51,54,43};
   for(handoff_recheck_mode=0;handoff_recheck_mode<6;handoff_recheck_mode++){
    struct wifi_fence5_native_state saved=wifi_fence5_first;
    struct mt76_dma5 dma=wifi_stream5.dma;unsigned frontier=xhci.command_enqueue;uint64_t users=xhci.sync_users;
    memset(&wifi_fence5_handoff,0,sizeof wifi_fence5_handoff);
    assert(!wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,
     &wifi_stream5_slots[1],wifi_stream5.epoch+1,handoff_recheck));
    assert(wifi_fence5_handoff.reason==expected[handoff_recheck_mode] &&
     !wifi_fence5_connection_switched && wifi_fence5_active==&wifi_fence5_first && !wifi_fence5_next_owner);
    wifi_fence5_first=saved;wifi_stream5.dma=dma;xhci.command_enqueue=frontier;xhci.sync_users=users;
    xhci.event_ring[xhci.event_dequeue].control=xhci.event_cycle?0u:1u;
   }
  }
  printf("xhci-http-reconnect5: PASS boundary case%u reason%u; first actual HTTP complete, no second owner/DMA/child; event retained; acceptance=0\n",trial_case,reason);return 0;
 }
 if(trial_case==15){
  const struct http_network_probe *p=&wifi_http_reconnect5.probe[0];
  assert(!good && !complete && wifi_http_reconnect5.parent_complete && !wifi_http_reconnect5.connections);
  assert(!p->result.attempted && !p->result.go && !p->result.complete && p->result.quiescent &&
   p->result.child_retired && p->result.worker_finished && p->child.retired && !p->child.published &&
   !p->bootstrap.identity && !wifi_stream5.attempted && !wifi_stack5_net.identity &&
   child_count==1 && !children[0].go && workers==1 && !wifi_http_reconnect5.probe[1].self);
  puts("xhci-http-reconnect5: PASS case15 actual prepared child/parked worker abort after missing-target parent restoration;no connection/key/provider/IP/GO or second generation;acceptance=0");return 0;
 }
 if(trial_case==2 || trial_case==12 || trial_case==14 || trial_case==16 || trial_case==17){
  const struct http_network_probe *p=&wifi_http_reconnect5.probe[0];const struct http_network_result *r=&p->result;
  assert(!good && !complete && !wifi_http_reconnect5.connections && child_count==1 && !child_done && reconnect_hook_count==1);
  assert(r->reason==(trial_case==14?HTTP_NET_DNS_FAILED:HTTP_NET_DHCP_FAILED) && r->phase==(trial_case==14?HTTP_NET_DNS:HTTP_NET_DHCP) && !r->complete && r->dhcp==(trial_case==14) && !r->handoff && !r->go);
  assert(r->quiescent && r->child_retired && r->worker_finished && r->tcp_empty && r->provider_drained && r->receiver_closed && r->refs_scrubbed && r->disconnected);
  assert(p->bootstrap.failed && p->bootstrap.protocol_failed && p->bootstrap.closed && p->bootstrap.disconnected);
  assert(p->bootstrap.dhcp_observation.end_reason==(trial_case==14?NET_DHCP_SUCCESS:NET_DHCP_TIME_LIMIT) && p->bootstrap.dhcp_observation.finished);
  assert(r->elapsed_us>=(trial_case==14?3000000u:2000000u) && r->elapsed_us<5000000 && !children[0].go);
  assert(wifi_stream5_slots[0].exchange.keys_erased && wifi_stream5_slots[0].exchange.leave_sent && !wifi_stream5_slots[0].exchange.handshake.keys_may_live);
  assert(wifi_stream5_slots[0].retired && wifi_stream5_slots[0].connection_closed && wifi_stream5_slots[0].restored && wifi_stack5.detached && wifi_stack5_net.state==MT76_NET5_DOWN);
  assert(wifi_http_reconnect5.failure_closed && wifi_http_reconnect5.parent_complete==(trial_case!=17));
  assert(wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));
  assert(!wifi_reconnect5_closed(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma));
  wifi_stream5.events_empty=false;assert(!wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));wifi_stream5.events_empty=true;
  wifi_stack5_net.callbacks=1;assert(!wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));wifi_stack5_net.callbacks=0;
  /* Every relaxation is scoped to the same failed, detached lifetime. */
#define REFUSES(field,value) do{__typeof__(field) saved=(field);(field)=(value);assert(!wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));(field)=saved;}while(0)
  REFUSES(wifi_stack5.terminal_epoch,wifi_stream5.epoch+1);
  REFUSES(wifi_stack5.detached,false);REFUSES(wifi_stack5.terminal_handoff,false);
  REFUSES(wifi_stream5.exchange.keys_erased,false);REFUSES(wifi_stream5.exchange.unsafe,true);
  REFUSES(wifi_stream5.exchange.handshake.keys_may_live,true);
  REFUSES(wifi_stream5.pending[0],true);REFUSES(wifi_stream5.cached_present,true);
  REFUSES(wifi_stream5.loan.ticket,1);REFUSES(wifi_stream5.dma.borrowed,wifi_stream5.dma.released+1);
  REFUSES(wifi_stream5.stop_events[1],0);REFUSES(wifi_stream5.dma.faults,1);
  REFUSES(wifi_http_reconnect5.probe[0].guard_failed,true);
  REFUSES(wifi_http_reconnect5.probe[0].result.go,true);
  REFUSES(wifi_http_reconnect5.probe[0].worker.ever_active,true);
#undef REFUSES
  assert(!xhci_wifi_capture_http_reconnect5());
  assert(!wifi_http5_held_state.frozen && !net_prepare_connection_probe_worker());
  printf("xhci-http-reconnect5: PASS case%u actual native bootstrap failure, unused child/worker and protocol closure, consuming raw RX handoff, key erase/leave/exact Stop/DMA retirement/provider detach;original deadlines;no second generation;acceptance=0\n",trial_case);return 0;
 }
 if(trial_case>=6){
  const struct wifi_stream5_state *s=&wifi_stream5_slots[1];
  assert(!good && !complete && wifi_http_reconnect5.connections==1 && child_done==1);
  assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[1].result.quiescent && !wifi_http_reconnect5.probe[1].result.attempted);
  assert(wifi_http_reconnect5.probe[1].result.child_retired && wifi_http_reconnect5.probe[1].result.worker_finished);
  assert(s->posts[0]==1 && s->completions[0]==1 && !s->fault_event_valid);
  assert(s->exchange.auth_sent && !s->exchange.auth_matched && !s->exchange.assoc_sent && !s->exchange.keys_activated && s->exchange.keys_erased && !s->exchange.leave_sent);
  assert(s->auth_observation.present==25 && !s->auth_observation.peer && !s->auth_observation.invalid && !s->auth_observation.rejected);
  assert(s->auth_observation.last-s->auth_observation.first>=750000 && s->auth_observation.last-s->auth_observation.first<755000);
  assert(s->auth_radio.complete[0] && s->auth_radio.empty[0]==(trial_case!=9));
  if(trial_case==10){assert(s->failed && !s->auth_radio.complete[1] && !s->restored && !s->stop_events[0] && !s->stop_events[1]);}
  else{assert(!s->failed && s->phase==9 && s->auth_radio.complete[1] && s->auth_radio.empty[1]);}
  if(trial_case>=7){
   assert(!radio_sim.ext_read && radio_sim.pop[0]==(trial_case==9?17u:1u));
   if(trial_case!=10){assert(radio_sim.pop[1]==(trial_case==11?1u:2u));}
   if(trial_case==7 || trial_case==9)assert(s->auth_radio.fifo[1][0]==0x2000ffa1);
   if(trial_case==8)assert(s->auth_radio.fifo[1][0]==0x2000ff81);
  }
  assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream));
  printf("xhci-http-reconnect5: PASS case%u first HTTP success/second AUTH miss with25 unrelated frames; original750ms budget; independent radio ACK/no-ACK/absent/saturated/control-failure outcomes never authorize IP;unused child/worker retired;acceptance=0\n",trial_case);return 0;
 }
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
