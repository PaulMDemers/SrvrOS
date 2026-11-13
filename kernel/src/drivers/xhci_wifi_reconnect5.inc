/* Explicit bounded two-connection owner. Adapter parents remain held once;
 * connection storage, keys, DMA leases and fence records are never recycled.
 * Historical one-shot entry points retain their original gates and receipts. */
static struct {
 bool attempted,complete,parent_complete,provider_retired;
 unsigned connections;
 struct mt76_stack5_probe probe[2];
 uint64_t epochs[2],tokens[2],submitted[2],received[2];
} wifi_reconnect5;

/* Native proof for the exact old connection, while the adapter parent window
 * remains owned. This is deliberately not whole-adapter/backend retirement. */
static bool wifi_http5_failed_detached(void);
static unsigned wifi_reconnect5_closed_reason;
static bool wifi_reconnect5_closed_state(void *owner,uint64_t epoch,const struct mt76_dma5 *dma,bool failed){
 wifi_reconnect5_closed_reason=0;
 if(failed && !wifi_http5_failed_detached()){wifi_reconnect5_closed_reason=1;return false;}
 struct wifi_stream5_state *s=owner;
 if((s!=&wifi_stream5_slots[0] && s!=&wifi_stream5_slots[1])){wifi_reconnect5_closed_reason=2;return false;}
 if(s!=wifi_stream5_current){wifi_reconnect5_closed_reason=3;return false;}
 if(!s->connection_mode){wifi_reconnect5_closed_reason=4;return false;}
 if(!s->connection_closed){wifi_reconnect5_closed_reason=5;return false;}
 if(s->epoch!=epoch){wifi_reconnect5_closed_reason=6;return false;}
 if(dma!=&s->dma){wifi_reconnect5_closed_reason=7;return false;}
 if(dma->identity!=(uintptr_t)dma){wifi_reconnect5_closed_reason=8;return false;}
 if(dma->positions!=127){wifi_reconnect5_closed_reason=9;return false;}
 if(dma->epoch!=epoch){wifi_reconnect5_closed_reason=10;return false;}
 if(dma->state!=MT76_DMA5_RETIRED){wifi_reconnect5_closed_reason=11;return false;}
 if(!s->retired){wifi_reconnect5_closed_reason=12;return false;}
 if(!s->restored){wifi_reconnect5_closed_reason=13;return false;}
 if(!s->events_empty){wifi_reconnect5_closed_reason=14;return false;}
 if((!s->complete && !failed)){wifi_reconnect5_closed_reason=15;return false;}
 if(s->failed){wifi_reconnect5_closed_reason=16;return false;}
 if(s->phase!=9){wifi_reconnect5_closed_reason=17;return false;}
 if(s->active){wifi_reconnect5_closed_reason=18;return false;}
 if(s->data_active){wifi_reconnect5_closed_reason=19;return false;}
 if(!s->closed){wifi_reconnect5_closed_reason=20;return false;}
 if(s->pending[0]){wifi_reconnect5_closed_reason=21;return false;}
 if(s->pending[1]){wifi_reconnect5_closed_reason=22;return false;}
 if(s->ready_rx){wifi_reconnect5_closed_reason=23;return false;}
 if(s->cached_present){wifi_reconnect5_closed_reason=24;return false;}
 if(s->loan.ticket){wifi_reconnect5_closed_reason=25;return false;}
 if(dma->borrowed!=dma->released){wifi_reconnect5_closed_reason=26;return false;}
 if(dma->faults){wifi_reconnect5_closed_reason=27;return false;}
 if((!s->exchange.complete && !failed)){wifi_reconnect5_closed_reason=28;return false;}
 if(!s->exchange.keys_erased){wifi_reconnect5_closed_reason=29;return false;}
 if(!s->exchange.leave_sent){wifi_reconnect5_closed_reason=30;return false;}
 if(s->exchange.unsafe){wifi_reconnect5_closed_reason=31;return false;}
 if(s->exchange.handshake.keys_may_live){wifi_reconnect5_closed_reason=32;return false;}
 if(!wpa2_ccmp_erased(&s->exchange.keys,epoch)){wifi_reconnect5_closed_reason=33;return false;}
 if(!s->mac.mac_disabled){wifi_reconnect5_closed_reason=34;return false;}
 if(!s->mac.tx_drained){wifi_reconnect5_closed_reason=35;return false;}
 if(!s->mac.rx_drained){wifi_reconnect5_closed_reason=36;return false;}
 if(!s->mac.filter_restored){wifi_reconnect5_closed_reason=37;return false;}
 if(!s->mac.txop_restored){wifi_reconnect5_closed_reason=38;return false;}
 if(s->stop_events[0]!=1){wifi_reconnect5_closed_reason=39;return false;}
 if(s->stop_events[1]!=1){wifi_reconnect5_closed_reason=40;return false;}
 if(!s->d){wifi_reconnect5_closed_reason=41;return false;}
 if(s->d!=wifi_cal5.d){wifi_reconnect5_closed_reason=42;return false;}
 if(s->d->input_context_phys!=s->input_phys){wifi_reconnect5_closed_reason=43;return false;}
 if(s->d->output_context_phys!=s->output_phys){wifi_reconnect5_closed_reason=44;return false;}
 if((uintptr_t)s->d->input_context!=s->input_pointer){wifi_reconnect5_closed_reason=45;return false;}
 if((uintptr_t)s->d->output_context!=s->output_pointer){wifi_reconnect5_closed_reason=46;return false;}
 if(!wifi_stream5_parent()){wifi_reconnect5_closed_reason=47;return false;}
 if(!wifi_handoff5_equal(s->d->input_context,s->input,4096)){wifi_reconnect5_closed_reason=48;return false;}
 if(!wifi_handoff5_equal(&wifi_join5,s->parent,sizeof wifi_join5)){wifi_reconnect5_closed_reason=49;return false;}
 if(!wifi_handoff5_equal(&wifi_cal5,s->cal,sizeof wifi_cal5)){wifi_reconnect5_closed_reason=50;return false;}
 if(!wifi_handoff5_equal(&wifi_power5,s->power,sizeof wifi_power5)){wifi_reconnect5_closed_reason=51;return false;}
 if(!wifi_handoff5_equal(&wifi_handoff5,s->handoff,sizeof wifi_handoff5)){wifi_reconnect5_closed_reason=52;return false;}
 if(!wifi_handoff5_equal(&wifi_peer5,s->binding,sizeof wifi_peer5)){wifi_reconnect5_closed_reason=53;return false;}
 if(!wifi_handoff5_equal(&wifi_tx5,s->preview,sizeof wifi_tx5)){wifi_reconnect5_closed_reason=54;return false;}
 if((context_read32(s->d->output_context,8,0)&7)){wifi_reconnect5_closed_reason=55;return false;}
 const unsigned dcis[]={0,11,16,9};
 for(unsigned i=0;i<4;i++)for(unsigned w=0;w<8;w++)
  if(context_read32(s->d->output_context,dcis[i],w)!=s->context[i==3?4:i][w]){wifi_reconnect5_closed_reason=56+i*8+w;return false;}
 bool closed=!wifi_stack5.running && wifi_stack5.detached && !wifi_stack5.tx_token &&
  wifi_stack5_net.state==MT76_NET5_DOWN && !wifi_stack5_net.attached && !wifi_stack5_net.io.context &&
  !wifi_stack5_net.pending && !wifi_stack5_net.count && !wifi_stack5_net.callbacks &&
  !wifi_stack5_net.io_busy && !wifi_stack5_net.polling && !wifi_stack5_net.delivering &&
  !wifi_stack5_net.faults && !wifi_stack5_net.dropped && !wifi_stack5_net.retire_requested &&
  wifi_stack5_net.enqueued==wifi_stack5_net.submitted && wifi_stack5_net.submitted==wifi_stack5_net.completed;
 if(!closed)wifi_reconnect5_closed_reason=88;
 return closed;
}
static bool wifi_reconnect5_closed(void *owner,uint64_t epoch,const struct mt76_dma5 *dma){
 return wifi_reconnect5_closed_state(owner,epoch,dma,false);
}
static bool wifi_reconnect5_exercise(void *context,struct mt76_net5 *p){
 if(wifi_reconnect5.connections>1 || context!=&wifi_reconnect5.probe[wifi_reconnect5.connections] ||
    p!=&wifi_stack5_net || wifi_stack5.start>UINT64_MAX-WIFI_STACK5_TOTAL_US)return false;
 const struct mt76_stack5_probe_io io={&wifi_stream5,wifi_stack5_probe_current,
  wifi_stack5_probe_retry,wifi_stack5_probe_observe,wifi_stream5.epoch,wifi_stack5.start+WIFI_STACK5_TOTAL_US};
 return mt76_stack5_probe_run_connection(context,p,&io);
}
static bool wifi_reconnect5_window(void){
 for(unsigned generation=0;generation<2;generation++){
  wifi_stream5.connection_mode=wifi_stream5.stack_mode=true;
  wifi_stack5.selected=true;wifi_stack5.exercise=wifi_reconnect5_exercise;
  wifi_stack5.context=&wifi_reconnect5.probe[generation];
  console_printf("wifi-reconnect5-begin: version=1 generation=%u adapter_parent_held=1\n",(uint64_t)generation+1);
#ifdef SRVROS_XHCI_STACK5_TEST
  stack_test_connection(generation);
#endif
  bool good=wifi_stream5_inspect();
  if(good){zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);zero_bytes(&wifi_stream5.scratch,sizeof wifi_stream5.scratch);}
  /* Local parent snapshot/Stop/key/event closure precedes provider detach.
   * Full RF/outer adapter parents are restored exactly once after this loop. */
  good=wifi_stream5_close(good,false) && good;
  if(good && wifi_stack5_retirement(&wifi_stream5,wifi_stream5.epoch))
   wifi_stack5.detached=mt76_net5_finish_detach(&wifi_stack5_net,wifi_stream5.epoch);
  good=good && wifi_reconnect5.probe[generation].complete && wifi_reconnect5.probe[generation].private_erased && wifi_reconnect5.probe[generation].connection_scope && wifi_reconnect5.probe[generation].disconnected && wifi_reconnect5_closed(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma);
  wifi_stack5.exercise=NULL;wifi_stack5.context=NULL;
  mt76_stack5_probe_emit(&wifi_reconnect5.probe[generation]);
  if(!good)return false;
  wifi_reconnect5.epochs[generation]=wifi_stream5.epoch;
  wifi_reconnect5.tokens[generation]=wifi_stack5_net.next_token;
  wifi_reconnect5.submitted[generation]=wifi_stack5_net.submitted-wifi_stack5.base_submitted;
  wifi_reconnect5.received[generation]=wifi_stack5_net.received-wifi_stack5.base_received;
  wifi_reconnect5.connections++;
  console_printf("wifi-reconnect5-connection: version=1 generation=%u complete=1 epoch=%u submitted=%u received=%u next_token=%u max_fences=2 stack_deconfigured=1 keys_erased=1 leave=1 native_closed=1 provider_detached=1 adapter_parent_restored=0 backend_retired=0\n",
   (uint64_t)generation+1,wifi_stream5.epoch,wifi_reconnect5.submitted[generation],
   wifi_reconnect5.received[generation],wifi_stack5_net.next_token);
  if(!generation){
   uint64_t flags=event_irq_save();
   bool next=wifi_stream5.epoch!=UINT64_MAX &&
      wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,
        &wifi_stream5_slots[1],wifi_stream5.epoch+1,wifi_reconnect5_closed);
   if(next){wifi_stream5_current=&wifi_stream5_slots[1];wifi_stack5_current=&wifi_stack5_slots[1];}
   event_irq_restore(flags);
   if(!next)return false;
  }
 }
 return true;
}
bool xhci_wifi_capture_reconnect5_probe(void){
 if(wifi_http_reconnect5_reserved || wifi_reconnect5.attempted || wifi_stack5.selected || wifi_stack5_net.identity ||
    wifi_stream5.attempted || wifi_stream5.stack_mode || wifi_join5_after ||
    wifi_wpa5_requested || wifi_wpa5_data_requested || wifi_provider5_requested ||
    wifi_reuse5_requested || wifi_tx5_nop_child)return false;
 wifi_reconnect5.attempted=true;wifi_join5_after=wifi_reconnect5_window;
 bool good=
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_capture_parent();
#else
 xhci_wifi_capture_eapol5();
#endif
 wifi_join5_after=NULL;
 for(unsigned i=0;i<2;i++){
  struct wifi_stream5_state *s=&wifi_stream5_slots[i];
  zero_bytes(s->pmk,sizeof s->pmk);zero_bytes(s->usb,sizeof s->usb);
  zero_bytes(s->cached,sizeof s->cached);zero_bytes(&s->scratch,sizeof s->scratch);
 }
 wifi_reconnect5.parent_complete=good;
 if(good && wifi_reconnect5.connections==2 && wifi_reconnect5.epochs[1]>wifi_reconnect5.epochs[0] &&
    wifi_reconnect5.tokens[1]>wifi_reconnect5.tokens[0] && wifi_stack5_net.state==MT76_NET5_DOWN)
  wifi_reconnect5.provider_retired=mt76_net5_retire(&wifi_stack5_net);
 wifi_reconnect5.complete=good && wifi_reconnect5.provider_retired && wifi_reconnect5.connections==2;
 console_printf("wifi-reconnect5-final: version=1 complete=%u connections=%u parent_restored=%u provider_retired=%u retained_contexts=2 backend_retired=0\n",
  (uint64_t)wifi_reconnect5.complete,(uint64_t)wifi_reconnect5.connections,
  (uint64_t)wifi_reconnect5.parent_complete,(uint64_t)wifi_reconnect5.provider_retired);
 return wifi_reconnect5.complete;
}
