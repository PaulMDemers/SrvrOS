/* Explicit v139 native HTTP bridge. All process/IO state survives failure.
 * The legacy WPA callback ALWAYS erases software keys after it returns, even
 * UNCERTAIN; never return to that owner with outstanding HTTP obligations. */
#include <srvros/http_network_probe.h>
static struct http_network_probe wifi_http5_probe;
static struct http_network_probe *wifi_http5_active_probe=&wifi_http5_probe;
static struct http_network_probe_io wifi_http5_io;
static bool wifi_http5_requested,wifi_http5_prepared,wifi_http5_capture_attempted,wifi_http5_callback_attempted;
/* Failure-only CPU0 observation. Take no ownership/clock/provider callbacks and
 * touch no live capabilities. Freeze scalar records with IRQ/preemption excluded,
 * then restore before console locking/rendering or console_sync (which may yield).
 * worker is the supervisor's LAST recorded observation, not fresh retirement
 * proof. Native fields are direct scalar reads, never packet/key/address bytes.
 * Holding retains every original owner even if workers progress while printing. */
static struct {
 struct http_network_result result;
 struct net_probe_worker_info worker;
 uint64_t site,requested,prepared,capture_attempted,callback_attempted;
 uint64_t guard_failed,checking,bootstrap_open,handoff_attempted,worker_stopped;
 uint64_t selected,running,stack_failed,stream_failed,stream_phase,fault_phase;
 uint64_t provider_state,provider_pending,queued,provider_faults,services;
 uint64_t enqueued,submitted,completed,received,tx_posts,rx_posts,tx_pending,rx_pending;
 uint64_t ready_rx,cached_rx,borrowed_rx,tx_token,boundary,fence_round,command_pending;
 bool frozen;
} wifi_http5_held_state;
static void wifi_http5_freeze_hold(unsigned site) {
 uint64_t flags=event_irq_save();
 if(!wifi_http5_held_state.frozen){
  wifi_http5_held_state.result=wifi_http5_active_probe->result;
  const struct net_probe_worker_info *w=&wifi_http5_active_probe->worker;
  wifi_http5_held_state.worker=(struct net_probe_worker_info){
   .prepared=w->prepared,.parked=w->parked,.active=w->active,.handoff_started=w->handoff_started,
   .ever_active=w->ever_active,.busy=w->busy,.poll_busy=w->poll_busy,.checking=w->checking,
   .stop_requested=w->stop_requested,.finished=w->finished,.failed=w->failed,
   .parked_waiters=w->parked_waiters,.polls=w->polls,.turns=w->turns};
  wifi_http5_held_state.site=site;
  wifi_http5_held_state.requested=wifi_http5_requested;
  wifi_http5_held_state.prepared=wifi_http5_prepared;
  wifi_http5_held_state.capture_attempted=wifi_http5_capture_attempted;
  wifi_http5_held_state.callback_attempted=wifi_http5_callback_attempted;
  wifi_http5_held_state.guard_failed=wifi_http5_active_probe->guard_failed;
  wifi_http5_held_state.checking=wifi_http5_active_probe->checking;
  wifi_http5_held_state.bootstrap_open=wifi_http5_active_probe->bootstrap_open;
  wifi_http5_held_state.handoff_attempted=wifi_http5_active_probe->handoff_attempted;
  wifi_http5_held_state.worker_stopped=wifi_http5_active_probe->worker_stopped;
  wifi_http5_held_state.selected=wifi_stack5.selected;
  wifi_http5_held_state.running=wifi_stack5.running;
  wifi_http5_held_state.stack_failed=wifi_stack5.failed;
  wifi_http5_held_state.stream_failed=wifi_stream5.failed;
  wifi_http5_held_state.stream_phase=wifi_stream5.phase;
  wifi_http5_held_state.fault_phase=wifi_stream5.fault_phase;
  wifi_http5_held_state.provider_state=wifi_stack5_net.state;
  wifi_http5_held_state.provider_pending=wifi_stack5_net.pending;
  wifi_http5_held_state.queued=wifi_stack5_net.count;
  wifi_http5_held_state.provider_faults=wifi_stack5_net.faults;
  wifi_http5_held_state.services=wifi_stack5.services;
  wifi_http5_held_state.enqueued=wifi_stack5_net.enqueued;
  wifi_http5_held_state.submitted=wifi_stack5_net.submitted;
  wifi_http5_held_state.completed=wifi_stack5_net.completed;
  wifi_http5_held_state.received=wifi_stack5_net.received;
  wifi_http5_held_state.tx_posts=wifi_stream5.posts[0];
  wifi_http5_held_state.rx_posts=wifi_stream5.posts[1];
  wifi_http5_held_state.tx_pending=wifi_stream5.pending[0];
  wifi_http5_held_state.rx_pending=wifi_stream5.pending[1];
  wifi_http5_held_state.ready_rx=wifi_stream5.ready_rx;
  wifi_http5_held_state.cached_rx=wifi_stream5.cached_present;
  wifi_http5_held_state.borrowed_rx=wifi_stream5.loan.ticket!=0;
  wifi_http5_held_state.tx_token=wifi_stack5.tx_token!=0;
  wifi_http5_held_state.boundary=wifi_stack5.boundary;
  wifi_http5_held_state.fence_round=wifi_fence5_native.round;
  wifi_http5_held_state.command_pending=xhci.command_pending;
  wifi_http5_held_state.frozen=true;
 }
 event_irq_restore(flags);
}
static void wifi_http5_emit_hold(void) {
 const struct http_network_result *r=&wifi_http5_held_state.result;
 const struct net_probe_worker_info *w=&wifi_http5_held_state.worker;
 console_printf("wifi-http5-held-state: version=1 site=%u requested=%u prepared=%u capture_attempted=%u callback_attempted=%u phase=%u reason=%u attempted=%u go=%u child_status=%u child_observed=%u child_reaped=%u child_retired=%u tcp_empty=%u provider_drained=%u receiver_closed=%u worker_finished=%u quiescent=%u guard_failed=%u checking=%u bootstrap_open=%u handoff_attempted=%u worker_stopped=%u\n",
  wifi_http5_held_state.site,wifi_http5_held_state.requested,wifi_http5_held_state.prepared,
  wifi_http5_held_state.capture_attempted,wifi_http5_held_state.callback_attempted,r->phase,r->reason,(uint64_t)r->attempted,r->go,r->child_status,
  (uint64_t)r->child_observed,(uint64_t)r->child_reaped,(uint64_t)r->child_retired,
  (uint64_t)r->tcp_empty,(uint64_t)r->provider_drained,(uint64_t)r->receiver_closed,(uint64_t)r->worker_finished,(uint64_t)r->quiescent,
  wifi_http5_held_state.guard_failed,wifi_http5_held_state.checking,wifi_http5_held_state.bootstrap_open,
  wifi_http5_held_state.handoff_attempted,wifi_http5_held_state.worker_stopped);
 console_printf("wifi-http5-held-progress: prepared=%u worker_allocated=%u dhcp=%u retries=%u dns=%u route=%u handoff=%u bootstrap_polls=%u worker_polls=%u worker_turns=%u supervisor_turns=%u elapsed_us=%u\n",
  (uint64_t)r->prepared,(uint64_t)r->worker_allocated,r->dhcp,r->retries,r->dns,r->route,(uint64_t)r->handoff,
  r->bootstrap_polls,r->worker_polls,r->worker_turns,r->supervisor_turns,r->elapsed_us);
 console_printf("wifi-http5-held-worker: last_record=1 prepared=%u parked=%u active=%u handoff_started=%u ever_active=%u busy=%u poll_busy=%u checking=%u stop_requested=%u finished=%u failed=%u parked_waiters=%u polls=%u turns=%u\n",
  w->prepared,w->parked,w->active,w->handoff_started,w->ever_active,w->busy,w->poll_busy,w->checking,
  w->stop_requested,w->finished,w->failed,w->parked_waiters,w->polls,w->turns);
 console_printf("wifi-http5-held-native: selected=%u running=%u stack_failed=%u stream_failed=%u stream_phase=%u fault_phase=%u provider_state=%u provider_pending=%u queued=%u provider_faults=%u services=%u enqueued=%u submitted=%u completed=%u received=%u tx_posts=%u rx_posts=%u\n",
  wifi_http5_held_state.selected,wifi_http5_held_state.running,wifi_http5_held_state.stack_failed,
  wifi_http5_held_state.stream_failed,wifi_http5_held_state.stream_phase,wifi_http5_held_state.fault_phase,
  wifi_http5_held_state.provider_state,wifi_http5_held_state.provider_pending,wifi_http5_held_state.queued,
  wifi_http5_held_state.provider_faults,wifi_http5_held_state.services,wifi_http5_held_state.enqueued,
  wifi_http5_held_state.submitted,wifi_http5_held_state.completed,wifi_http5_held_state.received,
  wifi_http5_held_state.tx_posts,wifi_http5_held_state.rx_posts);
 console_printf("wifi-http5-held-pending: tx=%u rx=%u ready_rx=%u cached_rx=%u borrowed_rx=%u tx_token_present=%u boundary=%u fence_round=%u command_pending=%u retained=1\n",
  wifi_http5_held_state.tx_pending,wifi_http5_held_state.rx_pending,wifi_http5_held_state.ready_rx,
  wifi_http5_held_state.cached_rx,wifi_http5_held_state.borrowed_rx,wifi_http5_held_state.tx_token,
  wifi_http5_held_state.boundary,wifi_http5_held_state.fence_round,wifi_http5_held_state.command_pending);
}
/* site: 1 callback admission; 2 post-run obligations; 3 unused abort failed;
 * 4 finish not quiescent; 5 terminal RX software seal lost. A hold never returns to unconditional WPA key erase. */
static __attribute__((noreturn)) void wifi_http5_hold(unsigned site) {
 wifi_http5_freeze_hold(site);
 wifi_http5_emit_hold();
 console_write("wifi-http5-held: software obligations incomplete; key/provider/DMA retained; no key erase or disk save; photograph the five diagnostic rows above, then power off manually for Ubuntu\n");
 (void)console_sync();
#ifdef SRVROS_XHCI_HTTP5_HOST_TEST
 extern __attribute__((noreturn)) void wifi_http5_test_hold(void);
 wifi_http5_test_hold();
#else
 __asm__ volatile("cli":::"memory");
 for(;;)__asm__ volatile("hlt":::"memory");
#endif
}
bool xhci_wifi_http5_prepare(void) {
 if(wifi_http5_requested || wifi_http_reconnect5_reserved || wifi_stack5.selected || wifi_stream5.attempted || wifi_stream5.stack_mode)return false;
 wifi_http5_requested=true;wifi_http5_prepared=http_network_probe_prepare(&wifi_http5_probe);
 console_printf("wifi-http5-prepare: prepared=%u retained=1\n",(uint64_t)wifi_http5_prepared);
 return wifi_http5_prepared;
}
static bool wifi_http5_drained(void *context,uint64_t epoch) {
 struct mt76_stack5_observation observed={0};
 return wifi_stack5_probe_observe(context,epoch,&observed) && observed.drained;
}
static bool wifi_http5_exercise(void *context,struct mt76_net5 *provider) {
 if(context!=&wifi_http5_probe || provider!=&wifi_stack5_net || !wifi_http5_requested ||
    !wifi_http5_prepared || wifi_http5_callback_attempted ||
    wifi_stack5.start>UINT64_MAX-WIFI_STACK5_TOTAL_US)wifi_http5_hold(1);
 wifi_http5_callback_attempted=true;
 /* Static source and context remain valid through every retained failure.
  * These exact existing provider guards do not depend on child liveness. */
 wifi_http5_io=(struct http_network_probe_io){.context=&wifi_stream5,
  .current=wifi_stack5_probe_current,.retry_ready=wifi_stack5_probe_retry,
  .drained=wifi_http5_drained,.terminal_rx_handoff=wifi_stack5_terminal_rx_handoff,.epoch=wifi_stream5.epoch,
  .start_us=wifi_stack5.start,.deadline_us=wifi_stack5.start+WIFI_STACK5_TOTAL_US,
  .test_ip_override=0};
 bool good=http_network_probe_run(&wifi_http5_probe,&wifi_http5_io);
 if(!wifi_http5_probe.result.quiescent || !wifi_http5_probe.result.child_retired ||
    !wifi_http5_probe.result.worker_finished || !wifi_http5_probe.result.tcp_empty ||
    !wifi_http5_probe.result.provider_drained ||
    (wifi_http5_probe.result.handoff && !wifi_http5_probe.result.receiver_closed))wifi_http5_hold(2);
 return good;
}
bool xhci_wifi_capture_http5(void) {
 if(!wifi_http5_requested || !wifi_http5_prepared || wifi_http5_capture_attempted)return false;
 wifi_http5_capture_attempted=true;
 return xhci_wifi_capture_stack5(wifi_http5_exercise,&wifi_http5_probe);
}
bool xhci_wifi_http5_finish_capture(bool parent_complete) {
 /* Missing adapter, firmware/RF/handshake refusal or unsupported recipe may
  * bypass the callback. Retire the waiting prelive process and unused parked
  * worker before logger flush/unmount, without claiming an HTTP workload. */
 if(wifi_http5_requested) {
  if(!wifi_http5_probe.result.attempted && !http_network_probe_abort_prepare(&wifi_http5_probe))wifi_http5_hold(3);
  if(!wifi_http5_probe.result.quiescent)wifi_http5_hold(4);
 }
 console_printf("wifi-http5-bridge: version=1 prepared=%u capture_attempted=%u callback_attempted=%u quiescent=%u child_retired=%u worker_finished=%u retained=1\n",
  (uint64_t)wifi_http5_prepared,(uint64_t)wifi_http5_capture_attempted,
  (uint64_t)wifi_http5_callback_attempted,
  (uint64_t)(!wifi_http5_requested || wifi_http5_probe.result.quiescent),
  (uint64_t)wifi_http5_probe.result.child_retired,(uint64_t)wifi_http5_probe.result.worker_finished);
 if(wifi_http5_requested){
  console_printf("wifi-http5-rx-handoff: version=1 complete=%u raw_pending=%u raw_ready=%u rx_ticket=%u rx_token=%u posts=%u completions=%u provider_closing=%u cache_empty=%u loans_empty=%u callbacks_idle=%u retained=1 endpoint_stop_proof=0\n",
   (uint64_t)wifi_stack5.terminal_handoff,(uint64_t)wifi_stack5.terminal_pending,(uint64_t)wifi_stack5.terminal_ready,
   wifi_stack5.terminal_ticket,wifi_stack5.terminal_token,wifi_stack5.terminal_posts,wifi_stack5.terminal_completions,
   (uint64_t)wifi_stack5.terminal_provider_closing,(uint64_t)wifi_stack5.terminal_handoff,(uint64_t)wifi_stack5.terminal_handoff,(uint64_t)wifi_stack5.terminal_handoff);
  http_network_probe_emit(&wifi_http5_probe);
 }
 return parent_complete && wifi_http5_requested && wifi_http5_prepared &&
  wifi_http5_capture_attempted && wifi_http5_callback_attempted && wifi_http5_probe.result.complete &&
  wifi_http5_probe.result.quiescent && wifi_http5_probe.result.child_retired && wifi_http5_probe.result.worker_finished;
}
