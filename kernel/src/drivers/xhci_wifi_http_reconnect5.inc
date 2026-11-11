/* Two bounded actual-process HTTP lifetimes on one held adapter. Each child,
 * supervisor, worker, USB context and source descriptor has permanent disjoint
 * storage. A failed/uncertain lifetime never authorizes the next preparation. */
static struct {
 struct http_network_probe probe[2];
 struct http_network_probe_io io[2];
 bool requested,prepared,attempted,complete,parent_complete,provider_retired;
 unsigned connections;
 uint64_t epochs[2],tokens[2];
} wifi_http_reconnect5;
static bool wifi_http_reconnect5_exercise(void *context,struct mt76_net5 *provider){
 unsigned g=wifi_http_reconnect5.connections;
 if(g>1 || context!=&wifi_http_reconnect5.probe[g] || provider!=&wifi_stack5_net ||
    !wifi_http_reconnect5.requested || !wifi_http_reconnect5.prepared ||
    wifi_stack5.start>UINT64_MAX-WIFI_STACK5_TOTAL_US)wifi_http5_hold(6);
 struct http_network_probe *p=&wifi_http_reconnect5.probe[g];
 wifi_http5_active_probe=p;
 wifi_http_reconnect5.io[g]=(struct http_network_probe_io){.context=&wifi_stream5,
  .current=wifi_stack5_probe_current,.retry_ready=wifi_stack5_probe_retry,
  .drained=wifi_http5_drained,.terminal_rx_handoff=wifi_stack5_terminal_rx_handoff,
  .epoch=wifi_stream5.epoch,.start_us=wifi_stack5.start,
  .deadline_us=wifi_stack5.start+WIFI_STACK5_TOTAL_US,.test_ip_override=0};
 bool good=http_network_probe_run_connection(p,&wifi_http_reconnect5.io[g],&provider->ops);
 if(!p->result.quiescent || !p->result.child_retired || !p->result.worker_finished ||
    !p->result.tcp_empty || !p->result.provider_drained || !p->result.disconnected ||
    (p->result.handoff && !p->result.receiver_closed))wifi_http5_hold(7);
 return good;
}
bool xhci_wifi_http_reconnect5_prepare(void){
 if(wifi_http_reconnect5_reserved || wifi_http_reconnect5.requested || wifi_reconnect5.attempted || wifi_http5_requested ||
    wifi_stack5.selected || wifi_stream5.attempted || wifi_stream5.stack_mode)return false;
 wifi_http_reconnect5_reserved=wifi_http_reconnect5.requested=true;
 wifi_http5_active_probe=&wifi_http_reconnect5.probe[0];
 wifi_http_reconnect5.prepared=http_network_probe_prepare_connection(wifi_http5_active_probe);
 console_printf("wifi-http-reconnect5-prepare: version=1 generation=1 prepared=%u retained=1\n",(uint64_t)wifi_http_reconnect5.prepared);
 return wifi_http_reconnect5.prepared;
}
static bool wifi_http_reconnect5_window(void){
 for(unsigned g=0;g<2;g++){
  struct http_network_probe *p=&wifi_http_reconnect5.probe[g];
  wifi_http5_active_probe=p;
  if(g){
   /* First key/network/endpoint/provider closure has already passed. The
    * adapter parents remain held, but neither connection holds live keys. */
   wifi_http_reconnect5.prepared=http_network_probe_prepare_connection(p);
   console_printf("wifi-http-reconnect5-prepare: version=1 generation=2 prepared=%u retained=1\n",(uint64_t)wifi_http_reconnect5.prepared);
   if(!wifi_http_reconnect5.prepared)return false;
  }
  wifi_stream5.connection_mode=wifi_stream5.stack_mode=true;
  wifi_stack5.selected=true;wifi_stack5.exercise=wifi_http_reconnect5_exercise;wifi_stack5.context=p;
  console_printf("wifi-http-reconnect5-begin: version=1 generation=%u adapter_parent_held=1\n",(uint64_t)g+1);
#ifdef SRVROS_XHCI_STACK5_TEST
  stack_test_connection(g);
#endif
  bool good=wifi_stream5_inspect();
  if(good){zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);zero_bytes(&wifi_stream5.scratch,sizeof wifi_stream5.scratch);}
  good=wifi_stream5_close(good,false) && good;
  if(good && wifi_stack5_retirement(&wifi_stream5,wifi_stream5.epoch))
   wifi_stack5.detached=mt76_net5_finish_detach(&wifi_stack5_net,wifi_stream5.epoch);
  good=good && p->result.complete && p->result.connection_scope && p->result.disconnected &&
   p->result.refs_scrubbed && wifi_reconnect5_closed(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma);
  wifi_stack5.exercise=NULL;wifi_stack5.context=NULL;
  /* Saved at-seal scalar observations, emitted only after native key/Stop
   * closure. They are software RX authority, not endpoint Stop proof. */
  console_printf("wifi-http5-rx-handoff: version=1 complete=%u raw_pending=%u raw_ready=%u rx_ticket=%u rx_token=%u posts=%u completions=%u provider_closing=%u cache_empty=%u loans_empty=%u callbacks_idle=%u retained=1 endpoint_stop_proof=0\n",
   (uint64_t)wifi_stack5.terminal_handoff,(uint64_t)wifi_stack5.terminal_pending,(uint64_t)wifi_stack5.terminal_ready,
   wifi_stack5.terminal_ticket,wifi_stack5.terminal_token,wifi_stack5.terminal_posts,wifi_stack5.terminal_completions,
   (uint64_t)wifi_stack5.terminal_provider_closing,(uint64_t)wifi_stack5.terminal_handoff,(uint64_t)wifi_stack5.terminal_handoff,(uint64_t)wifi_stack5.terminal_handoff);
  http_network_probe_emit(p);
  if(!good)return false;
  wifi_http_reconnect5.epochs[g]=wifi_stream5.epoch;wifi_http_reconnect5.tokens[g]=wifi_stack5_net.next_token;
  wifi_http_reconnect5.connections++;
  console_printf("wifi-http-reconnect5-connection: version=1 generation=%u complete=1 epoch=%u submitted=%u received=%u next_token=%u max_fences=2 stack_deconfigured=1 keys_erased=1 leave=1 native_closed=1 provider_detached=1 adapter_parent_restored=0 backend_retired=0\n",
   (uint64_t)g+1,wifi_stream5.epoch,wifi_stack5_net.submitted-wifi_stack5.base_submitted,
   wifi_stack5_net.received-wifi_stack5.base_received,wifi_stack5_net.next_token);
  if(!g){
   uint64_t flags=event_irq_save();
   bool next=wifi_stream5.epoch!=UINT64_MAX && wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,
    &wifi_stream5.dma,&wifi_stream5_slots[1],wifi_stream5.epoch+1,wifi_reconnect5_closed);
   if(next){wifi_stream5_current=&wifi_stream5_slots[1];wifi_stack5_current=&wifi_stack5_slots[1];}
   event_irq_restore(flags);if(!next)return false;
  }
 }
 return true;
}
bool xhci_wifi_capture_http_reconnect5(void){
 if(!wifi_http_reconnect5_reserved || !wifi_http_reconnect5.requested || !wifi_http_reconnect5.prepared || wifi_http_reconnect5.attempted ||
    wifi_reconnect5.attempted || wifi_stack5.selected || wifi_stack5_net.identity ||
    wifi_stream5.attempted || wifi_stream5.stack_mode || wifi_join5_after ||
    wifi_wpa5_requested || wifi_wpa5_data_requested || wifi_provider5_requested ||
    wifi_reuse5_requested || wifi_tx5_nop_child)return false;
 wifi_http_reconnect5.attempted=wifi_reconnect5.attempted=true;
 wifi_join5_after=wifi_http_reconnect5_window;
 bool good=
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_capture_parent();
#else
 xhci_wifi_capture_eapol5();
#endif
 wifi_join5_after=NULL;
 for(unsigned i=0;i<2;i++){
  struct wifi_stream5_state *s=&wifi_stream5_slots[i];zero_bytes(s->pmk,sizeof s->pmk);
  zero_bytes(s->usb,sizeof s->usb);zero_bytes(s->cached,sizeof s->cached);zero_bytes(&s->scratch,sizeof s->scratch);
 }
 wifi_http_reconnect5.parent_complete=good;
 if(good && wifi_http_reconnect5.connections==2 &&
    wifi_http_reconnect5.epochs[1]>wifi_http_reconnect5.epochs[0] &&
    wifi_http_reconnect5.tokens[1]>wifi_http_reconnect5.tokens[0] && wifi_stack5_net.state==MT76_NET5_DOWN)
  wifi_http_reconnect5.provider_retired=mt76_net5_retire(&wifi_stack5_net);
 wifi_http_reconnect5.complete=good && wifi_http_reconnect5.provider_retired && wifi_http_reconnect5.connections==2;
 return wifi_http_reconnect5.complete;
}
bool xhci_wifi_http_reconnect5_finish_capture(bool parent_complete){
 for(unsigned g=0;g<2;g++){
  struct http_network_probe *p=&wifi_http_reconnect5.probe[g];if(!p->self)continue;
  wifi_http5_active_probe=p;
  if(!p->result.attempted && !http_network_probe_abort_prepare(p))wifi_http5_hold(8);
  if(!p->result.quiescent)wifi_http5_hold(9);
  if(!p->result.attempted)console_printf("wifi-http-reconnect5-abort: version=1 generation=%u attempted=0 quiescent=%u child_retired=%u worker_allocated=%u worker_finished=%u retained=1\n",
   (uint64_t)g+1,(uint64_t)p->result.quiescent,(uint64_t)p->result.child_retired,
   (uint64_t)p->result.worker_allocated,(uint64_t)p->result.worker_finished);
 }
 console_printf("wifi-http-reconnect5-final: version=1 complete=%u connections=%u parent_restored=%u provider_retired=%u retained_contexts=2 backend_retired=0\n",
  (uint64_t)(parent_complete && wifi_http_reconnect5.complete),(uint64_t)wifi_http_reconnect5.connections,
  (uint64_t)wifi_http_reconnect5.parent_complete,(uint64_t)wifi_http_reconnect5.provider_retired);
 return parent_complete && wifi_http_reconnect5.complete;
}
