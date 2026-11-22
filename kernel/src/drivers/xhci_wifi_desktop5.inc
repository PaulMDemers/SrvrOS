/* The native owner keeps the verified channel/peer and RF parent throughout
 * the desktop lifetime. System queues intent; it never runs USB or DHCP. */
#include <srvros/wifi_service.h>
static struct {
 struct xhci_device *device;
 uint64_t owner;
 void *context;
 bool (*begin)(void *),(*running)(void *);
 struct wifi_service_command connect,disconnect;
 struct net_bounded_probe scopes[2];
 struct net_bounded_probe_io io[2];
 struct scheduler_wait_queue wait;
 uint64_t epochs[2],tokens[2];
 unsigned connections,disconnects;
 bool requested,reconnect,gate,started,connected,return_safe;
} wifi_desktop5;

static bool wifi_desktop5_unattempted(void){
 return wifi_desktop5.requested && !wifi_desktop5.owner && !wifi_desktop5.started &&
  !wifi_stream5.attempted && !wifi_stack5_net.identity;
}

static bool wifi_desktop5_present(void *context){
 struct xhci_device *d=wifi_desktop5.device;
 return context==&wifi_desktop5 && d && d->present && !d->removed && !d->retiring &&
  !d->slot_disabled && xhci.operational && !xhci.stopping;
}
static bool wifi_desktop5_due(void *context){return timer_ticks()>=*(const uint64_t *)context;}
static void wifi_desktop5_turn(void){
 uint64_t tick=timer_ticks();
 if(tick!=UINT64_MAX){tick++;if(scheduler_wait_timeout(&wifi_desktop5.wait,wifi_desktop5_due,&tick,tick))return;}
 scheduler_yield();
}
static bool wifi_desktop5_gate_begin(void){
 uint64_t flags=event_irq_save();
 bool ok=!wifi_desktop5.gate && !xhci.sync_users && !xhci.command_pending &&
  !strict_control.device && !scheduler_io_active() && scheduler_io_begin();
 wifi_desktop5.gate=ok;
 event_irq_restore(flags);
 return ok;
}
static void wifi_desktop5_gate_end(void){
 uint64_t flags=event_irq_save();
 if(wifi_desktop5.gate){scheduler_io_end();wifi_desktop5.gate=false;}
 event_irq_restore(flags);
}
static bool wifi_desktop5_take(struct wifi_service_command *command){
 uint64_t flags=event_irq_save();
 bool ok=wifi_service_take(wifi_desktop5.owner,&wifi_desktop5,command);
 event_irq_restore(flags);
 return ok;
}
static bool wifi_desktop5_io_safe(void){
 return !wifi_desktop5.gate && !xhci.command_pending && !strict_control.device &&
  (!xhci.sync_users || wifi_stream5_storage_shared());
}
static bool wifi_desktop5_data(void *context,struct mt76_net5 *provider){
 if(context!=&wifi_desktop5 || provider!=&wifi_stack5_net || !wifi_desktop5.gate)return false;
 wifi_desktop5_gate_end();
 unsigned slot=wifi_stream5_current==&wifi_stream5_slots[1];
 struct net_bounded_probe_io *io=&wifi_desktop5.io[slot];
 *io=(struct net_bounded_probe_io){&wifi_stream5,wifi_stack5_probe_current,wifi_stack5_probe_retry};
 struct net_bounded_probe *scope=&wifi_desktop5.scopes[slot];
 uint64_t now=timer_monotonic_us();
 bool live=now<=UINT64_MAX-WIFI_STACK5_TOTAL_US &&
  net_bounded_probe_init_connection(scope,io,&provider->ops,provider->epoch,now+WIFI_STACK5_TOTAL_US) &&
  net_dhcp_request_bounded(scope)>0 && net_bounded_connection_start_worker(scope,wifi_desktop5_io_safe);
 if(!live && scope->identity && !scope->closed)(void)net_bounded_probe_finish(scope);
 uint64_t flags=event_irq_save();
 bool published=wifi_service_finish(wifi_desktop5.owner,&wifi_desktop5,&wifi_desktop5.connect,live);
 wifi_desktop5.connected=live && published;
 event_irq_restore(flags);
 for(;;){
  bool lost=live && (provider->state!=MT76_NET5_UP || !wifi_desktop5_present(&wifi_desktop5) ||
                     wifi_stream5.failed);
  if(live && !lost){
   if(!wifi_desktop5.disconnect.serial)(void)wifi_desktop5_take(&wifi_desktop5.disconnect);
   if(published && !wifi_desktop5.disconnect.serial && wifi_desktop5.running(wifi_desktop5.context)){
    wifi_desktop5_turn();
    continue;
   }
  }
  if(wifi_desktop5_gate_begin()){
   flags=event_irq_save();
   bool drained=!provider->count && !provider->pending && !wifi_stack5.tx_token &&
                !wifi_stream5.pending[0] && !wifi_stream5.ready_rx && !wifi_stream5.loan.ticket;
   /* A lost controller/provider cannot establish quiet retirement. Retain
    * the worker scope and all native ownership, and leave the gate held. */
   if(lost)(void)wifi_stack5_fault();
   bool stopped=!lost && (!live || (drained && net_bounded_connection_stop_worker(scope)));
   event_irq_restore(flags);
   if(lost)return false;
   if(stopped)return live && published;
   wifi_desktop5_gate_end();
  }
  wifi_desktop5_turn();
 }
}

static void wifi_desktop5_setup_stream(void){
 wifi_stream5.stack_mode=wifi_stream5.paired=wifi_stream5.resident=true;
 wifi_stream5.connection_mode=wifi_desktop5.reconnect;
 wifi_stream5.defer_m1_duplicate=true;
 wifi_stack5.selected=true;wifi_stack5.context=&wifi_desktop5;wifi_stack5.exercise=wifi_desktop5_data;
}
static bool wifi_desktop5_closed(void){
 return wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,false,true);
}
static bool wifi_desktop5_retire_connection(void){
 unsigned slot=wifi_stream5_current==&wifi_stream5_slots[1];
#ifdef SRVROS_XHCI_STACK5_TEST
 stack_test_connection(slot);
#endif
 bool inspected=wifi_stream5_inspect();
 if(!wifi_stream5_close(inspected,false) || !wifi_desktop5.scopes[slot].disconnected ||
    !wifi_stack5_retirement(&wifi_stream5,wifi_stream5.epoch))return false;
 wifi_stack5.detached=mt76_net5_finish_detach(&wifi_stack5_net,wifi_stream5.epoch);
 if(!wifi_stack5.detached || !wifi_desktop5_closed())return false;
 wifi_desktop5.epochs[slot]=wifi_stream5.epoch;
 wifi_desktop5.tokens[slot]=wifi_stack5_net.next_token;
 wifi_desktop5.connections++;
 if(wifi_desktop5.disconnect.serial)wifi_desktop5.disconnects++;
 console_printf("wifi-desktop5-connection: version=1 generation=%u epoch=%u token=%u disconnected=%u retired=1\n",
  (uint64_t)slot+1,wifi_stream5.epoch,wifi_stack5_net.next_token,(uint64_t)(wifi_desktop5.disconnect.serial!=0));
 return true;
}
static bool wifi_desktop5_next(void){
 uint64_t flags=event_irq_save();
 bool ok=wifi_desktop5.gate && !xhci.sync_users && !xhci.poll_users && !xhci.command_pending &&
  !strict_control.device && !wifi_fence5_native.bound && wifi_stream5_current==&wifi_stream5_slots[0] &&
  xhci.command_enqueue<=63-WIFI_STREAM5_SESSION_COMMANDS && wifi_desktop5.connections==1 && wifi_desktop5_closed();
 if(ok){
  wifi_stream5_current=&wifi_stream5_slots[1];
  wifi_stack5_current=&wifi_stack5_slots[1];
  wifi_desktop5_setup_stream();
 }
 event_irq_restore(flags);
 return ok;
}

static bool wifi_desktop5_window(void){
 if(!wifi_stream5_parent())return false;
 if(wifi_desktop5.reconnect && (xhci.command_enqueue>63-2*WIFI_STREAM5_SESSION_COMMANDS ||
    wifi_join5.peer.generation==UINT64_MAX)){
  console_write("wifi-desktop-reconnect5-admission: resources unavailable for two connections\n");
  return true;
 }
 wifi_desktop5.device=wifi_cal5.d;
 const struct wifi_service_provider ops={&wifi_desktop5,"Edimax USB Wi-Fi",wifi_desktop5_present};
 uint64_t flags=event_irq_save();
 wifi_desktop5.owner=wifi_service_publish_connection(&ops,&wifi_join5.peer);
 event_irq_restore(flags);
 wifi_desktop5.started=wifi_desktop5.owner && wifi_desktop5.begin(wifi_desktop5.context);
 if(!wifi_desktop5.started)return false;
 while(wifi_desktop5.running(wifi_desktop5.context) && wifi_desktop5_present(&wifi_desktop5)){
  if(!wifi_desktop5.connect.serial)(void)wifi_desktop5_take(&wifi_desktop5.connect);
  if(wifi_desktop5.connect.serial && wifi_desktop5_gate_begin()){
   if(!wifi_desktop5.reconnect)return wifi_stream5_inspect();
   if(wifi_desktop5.connections && !wifi_desktop5_next())return false;
   if(!wifi_desktop5_retire_connection())return false;
   if(wifi_desktop5.connections==2 || !wifi_desktop5.disconnect.serial ||
      !wifi_desktop5.running(wifi_desktop5.context))return true;
   flags=event_irq_save();
   bool rearmed=wifi_stream5.epoch!=UINT64_MAX && wifi_service_rearm_connection(wifi_desktop5.owner,
    &wifi_desktop5,&wifi_desktop5.disconnect,wifi_stream5.epoch+1);
   if(rearmed){
    wifi_desktop5.connect=(struct wifi_service_command){0};
    wifi_desktop5.disconnect=(struct wifi_service_command){0};
    wifi_desktop5.connected=false;
   }
   event_irq_restore(flags);
   if(!rearmed)return true;
   wifi_desktop5_gate_end();
  }
  wifi_desktop5_turn();
 }
 /* Finish the original RF parent even when the user closes before Connect. */
 while(!wifi_desktop5_gate_begin())wifi_desktop5_turn();
 return true;
}

static bool wifi_desktop5_capture(void *context,bool (*begin)(void *),bool (*running)(void *),bool reconnect){
 if(!context || !begin || !running || wifi_desktop5.requested || wifi_stack5.selected ||
    wifi_stream5.attempted || wifi_join5_after || wifi_http_reconnect5_reserved)return false;
 wifi_desktop5.requested=true;wifi_desktop5.reconnect=reconnect;
 wifi_desktop5.context=context;wifi_desktop5.begin=begin;wifi_desktop5.running=running;
 wifi_desktop5_setup_stream();
 wifi_join5_after=wifi_desktop5_window;
 bool parent=
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_capture_parent();
#else
 xhci_wifi_capture_eapol5();
#endif
 wifi_join5_after=NULL;
 bool good=false;
 if(reconnect){
  bool closed=parent && (!wifi_stream5.attempted || (wifi_stream5.connection_closed &&
   wifi_stack5.detached && mt76_net5_retire(&wifi_stack5_net)));
  wifi_stack5.retired=closed && wifi_stack5.detached;
  wifi_desktop5.return_safe=closed;
  good=closed && wifi_desktop5.connections==2 && wifi_desktop5.disconnects==2 &&
   wifi_desktop5.epochs[1]>wifi_desktop5.epochs[0] && wifi_desktop5.tokens[1]>wifi_desktop5.tokens[0];
  console_printf("wifi-desktop-reconnect5-final: version=1 complete=%u connections=%u disconnects=%u provider_retired=%u return_safe=%u\n",
   (uint64_t)good,(uint64_t)wifi_desktop5.connections,(uint64_t)wifi_desktop5.disconnects,
   (uint64_t)wifi_stack5.retired,(uint64_t)closed);
 }else{
  good=wifi_stream5.attempted && wifi_stream5_close(parent,true) && wifi_desktop5.connected;
  if(good && wifi_stack5_retirement(&wifi_stream5,wifi_stream5.epoch)){
   wifi_stack5.detached=mt76_net5_finish_detach(&wifi_stack5_net,wifi_stream5.epoch);
   wifi_stack5.retired=wifi_stack5.detached && mt76_net5_retire(&wifi_stack5_net);
  }
  good=good && wifi_stack5.retired;
  wifi_desktop5.return_safe=parent && (!wifi_stream5.attempted || good);
 }
 uint64_t flags=event_irq_save();
 if(wifi_desktop5.disconnect.serial)
  (void)wifi_service_finish(wifi_desktop5.owner,&wifi_desktop5,&wifi_desktop5.disconnect,good);
 wifi_service_seal_connection(wifi_desktop5.owner,&wifi_desktop5,good);
 event_irq_restore(flags);
 console_printf("wifi-desktop5-final: started=%u connected=%u requested_disconnect=%u retired=%u return_safe=%u\n",
  (uint64_t)wifi_desktop5.started,(uint64_t)wifi_desktop5.connected,(uint64_t)(wifi_desktop5.disconnect.serial!=0),
  (uint64_t)good,(uint64_t)wifi_desktop5.return_safe);
 if(wifi_desktop5.return_safe || !wifi_desktop5.started)wifi_desktop5_gate_end();
 wifi_desktop5.context=NULL;wifi_desktop5.begin=NULL;wifi_desktop5.running=NULL;
 wifi_stack5.context=NULL;wifi_stack5.exercise=NULL;
 zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);
 return good;
}
bool xhci_wifi_capture_desktop5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 return wifi_desktop5_capture(context,begin,running,false);
}
bool xhci_wifi_capture_desktop_reconnect5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 return wifi_desktop5_capture(context,begin,running,true);
}

/* One transport rehearsal, after the original RF and network owners have
 * returned. The replacement proves USB identity and firmware EP configuration. */
bool xhci_wifi_capture_desktop_reattach5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 bool connected=wifi_desktop5_capture(context,begin,running,false);
 struct wifi_root_reattach trial={0};
 bool admitted=connected && wifi_desktop5.return_safe && wifi_stack5.retired &&
  wifi_desktop5.disconnect.serial && running(context) &&
  wifi_root_reattach_begin(&trial,wifi_desktop5.device);
 if(admitted) {
  console_write("wifi-root-reattach: waiting for Wi-Fi unplug and reinsert; workspace stays mounted\n");
  while(running(context)) {
   enum wifi_root_reattach_phase phase=wifi_root_reattach_step(&trial);
   if(phase>=WIFI_ROOT_READY)break;
   wifi_desktop5_turn();
  }
 }
 bool complete=trial.phase==WIFI_ROOT_READY;
 console_printf("wifi-root-reattach-final: version=1 complete=%u phase=%u old_retired=%u new_identity=%u firmware_ep=%u network_reconnected=0\n",
  (uint64_t)complete,(uint64_t)trial.phase,(uint64_t)(trial.previous && trial.previous->slot_disabled),
  (uint64_t)(trial.replacement && trial.replacement->wifi_probe_ok),
  (uint64_t)(trial.replacement && trial.replacement->firmware.configured));
 return complete;
}
bool xhci_wifi_desktop5_return_safe(void){return wifi_desktop5.return_safe;}
