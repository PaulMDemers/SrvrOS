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
 bool requested,reconnect,replug,gate,started,connected,return_safe;
 bool data_verified[2],driver_bound;
 bool active_remove,endurance_passed,removed;
} wifi_desktop5;

static bool wifi_desktop5_unattempted(void){
 return wifi_desktop5.requested && !wifi_desktop5.owner && !wifi_stream5.attempted &&
  ((!wifi_desktop5.started && !wifi_stack5_net.identity) ||
   (wifi_desktop5.replug && wifi_radio5_bank && wifi_stack5_net.state==MT76_NET5_DOWN && !wifi_stack5_net.attached));
}

static bool wifi_desktop5_present(void *context){
 struct xhci_device *d=wifi_desktop5.device;
 return context==&wifi_desktop5 && d && d->present && !d->removed && !d->retiring &&
  !d->slot_disabled && xhci.operational && !xhci.stopping &&
  (!d->parent_slot_id || wifi_hub_path_present(d));
}
/* xhci_device_snapshot reads this while holding the event IRQ lock. */
static bool wifi_desktop5_driver_bound(const struct xhci_device *d){
 return d==wifi_desktop5.device && wifi_desktop5.driver_bound &&
  !d->ep0_quarantined && !d->control_failed && !wifi_stream5.failed &&
  wifi_desktop5_present(&wifi_desktop5);
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
 if(ok)wifi_desktop5.gate=true;
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
static bool wifi_desktop5_hub_trial(void){
 return wifi_desktop5.device && wifi_desktop5.device->parent_slot_id &&
  !wifi_desktop5.reconnect && !wifi_desktop5.replug && !wifi_desktop5.active_remove;
}
static bool wifi_desktop5_verify_data(uint64_t epoch){
 struct srv_net_status_info status;
 uint32_t resolved=0;
 bool dhcp=net_status_info(&status)==0 && status.dhcp_configured && status.local_ip && status.router_ip;
 bool ping=dhcp && net_ping(status.router_ip,(uint16_t)(0x5300u+epoch),200,NULL)==0;
 bool dns=dhcp && net_dns_resolve("example.com",&resolved)==0 && resolved;
 console_printf("%s: version=1 epoch=%u dhcp=%u gateway_ping=%u dns=%u\n",
  wifi_desktop5_hub_trial()?"wifi-desktop-hub5-data":"wifi-desktop-replug5-data",
  epoch,(uint64_t)dhcp,(uint64_t)ping,(uint64_t)dns);
 return ping && dns;
}
/* This trial uses the ordinary resident worker. Verify paced traffic before
 * the desktop invites physical removal. */
static bool wifi_desktop5_endurance(void){
 struct srv_net_status_info status;
 if(net_status_info(&status)!=0 || !status.router_ip)return false;
 uint64_t start=timer_ticks();
 unsigned replies=0;
 for(unsigned i=0;i<60;i++){
  while(timer_ticks()-start<(uint64_t)i*100){
   if(!wifi_desktop5_present(&wifi_desktop5) ||
      !wifi_desktop5.running(wifi_desktop5.context) ||
      wifi_stack5_net.state!=MT76_NET5_UP)goto done;
   wifi_desktop5_turn();
  }
  if(net_ping(status.router_ip,(uint16_t)(0x5400u+i),100,NULL)!=0)break;
  replies++;
 }
done:
 console_printf("wifi-desktop-active-remove5-traffic: version=1 replies=%u required=60 elapsed_ticks=%u\n",
  (uint64_t)replies,timer_ticks()-start);
 return replies==60;
}
/* A wedged provider or corrupt event does not authorize disabling a device.
 * Require direct evidence that this exact, pinned high-speed root slot left. */
static bool wifi_desktop5_root_absent(void){
 struct xhci_device *d=wifi_stream5.d;
 if(!d || d!=wifi_desktop5.device || !d->present || !d->addressed || d->slot_disabled ||
    !d->slot_id || d->slot_id!=wifi_stream5.slot || device_by_slot(d->slot_id)!=d ||
    !d->port_id || d->port_id>xhci.port_count || d->parent_slot_id || d->hub_port_id ||
    d->route_string || d->hub_ports || d->speed!=3 || !d->wifi_probe_ok ||
    !xhci.operational || !xhci.controller_owned || xhci.stopping)return false;
 uint32_t state=op_read32(port_offset(d->port_id-1));
 return state!=UINT32_MAX && !(state&XHCI_PORTSC_CCS);
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
 if(live && (wifi_desktop5.replug || wifi_desktop5.active_remove || wifi_desktop5_hub_trial()))
  wifi_desktop5.data_verified[slot]=wifi_desktop5_verify_data(provider->epoch);
 if(live && wifi_desktop5.active_remove && wifi_desktop5.data_verified[slot])
  wifi_desktop5.endurance_passed=wifi_desktop5_endurance();
 uint64_t flags=event_irq_save();
 bool published=wifi_service_finish(wifi_desktop5.owner,&wifi_desktop5,&wifi_desktop5.connect,live);
 wifi_desktop5.connected=live && published;
 event_irq_restore(flags);
 for(;;){
  flags=event_irq_save();
  bool physically_absent=wifi_desktop5.active_remove && wifi_desktop5_root_absent();
  event_irq_restore(flags);
  bool lost=live && (provider->state!=MT76_NET5_UP || !wifi_desktop5_present(&wifi_desktop5) ||
                     wifi_stream5.failed || physically_absent);
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
   if(lost){
    if(wifi_desktop5.active_remove && wifi_desktop5_root_absent() &&
       net_bounded_connection_stop_worker(scope) && device_removed(wifi_stream5.d)){
     wifi_desktop5.removed=true;
     console_printf("wifi-desktop-active-remove5-loss: version=1 slot=%u pending_tx=%u pending_rx=%u worker_stopped=1\n",
      (uint64_t)wifi_stream5.slot,(uint64_t)wifi_stream5.pending[0],(uint64_t)wifi_stream5.pending[1]);
    }
    (void)wifi_stack5_fault(); /* Makes the WPA unwind erase locally without a Leave. */
   }
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
 wifi_stream5.connection_mode=wifi_desktop5.reconnect || wifi_desktop5.replug;
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

/* A stream cannot straddle the xHCI Link TRB during its ownership
 * change. Finish the last commands in the ring before taking its snapshot. */
static bool wifi_desktop5_command_runway(void){
 for(unsigned i=0;i<WIFI_STREAM5_SESSION_COMMANDS-1 &&
     xhci.command_enqueue>63-WIFI_STREAM5_SESSION_COMMANDS;i++){
  uint64_t slot=UINT64_MAX;
  if(!submit_command(XHCI_TRB_TYPE_NOOP,0,&slot) || slot!=0){
   xhci.command_failed=true;
   return false;
  }
 }
 return xhci.command_enqueue<=63-WIFI_STREAM5_SESSION_COMMANDS;
}

static bool wifi_desktop5_window(void){
 if(!wifi_stream5_parent())return false;
 if(wifi_desktop5.reconnect && (xhci.command_enqueue>63-2*WIFI_STREAM5_SESSION_COMMANDS ||
    wifi_join5.peer.generation==UINT64_MAX)){
  console_write("wifi-desktop-reconnect5-admission: resources unavailable for two connections\n");
  return true;
 }
 wifi_desktop5.device=wifi_cal5.d;
 if(!wifi_desktop5_present(&wifi_desktop5))return false;
 const struct wifi_service_provider ops={&wifi_desktop5,"Edimax USB Wi-Fi",wifi_desktop5_present};
 uint64_t flags=event_irq_save();
 wifi_desktop5.owner=wifi_service_publish_connection(&ops,&wifi_join5.peer);
 wifi_desktop5.driver_bound=wifi_desktop5.owner!=0;
 event_irq_restore(flags);
 if(!wifi_desktop5.owner)return false;
 if(wifi_radio5_bank)wifi_desktop5_gate_end();
 else wifi_desktop5.started=wifi_desktop5.begin(wifi_desktop5.context);
 if(!wifi_desktop5.started)return false;
 while(wifi_desktop5.running(wifi_desktop5.context) && wifi_desktop5_present(&wifi_desktop5)){
  if(!wifi_desktop5.connect.serial)(void)wifi_desktop5_take(&wifi_desktop5.connect);
  if(wifi_desktop5.connect.serial && wifi_desktop5_gate_begin()){
   if(wifi_desktop5.replug){
    if(wifi_radio5_bank && !wifi_desktop5_command_runway())return false;
    return wifi_desktop5_retire_connection();
   }
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

static bool wifi_desktop5_finish_removed(void){
 struct xhci_device *d=wifi_stream5.d;
 uint64_t flags=event_irq_save();
 bool erased=wifi_stream5.exchange.keys_erased &&
  wpa2_ccmp_erased(&wifi_stream5.exchange.keys,wifi_stream5.epoch) &&
  !wifi_stream5.exchange.session_active && !wifi_stream5.exchange.handshake.keys_may_live;
 bool disabled=erased && wifi_desktop5.gate && wifi_desktop5_root_absent() &&
  wifi_desktop5.scopes[0].disconnected && !wifi_stack5.running && !wifi_stream5.data_active &&
  !xhci.command_failed && !firmware_command_failed && !strict_control.device &&
  xhci.dcbaa && xhci_retire_slot(d->slot_id) && d->slot_disabled && !xhci.dcbaa[d->slot_id];
 if(disabled){
  wifi_stream5.active=false;
  /* Retain all old DMA and callback context. The confirmed Disable Slot
   * makes it safe to drop only the network registry's reference. */
  wifi_stack5.removed_slot_disabled=true;
  wifi_stack5.retired=mt76_net5_retire(&wifi_stack5_net);
 }
 event_irq_restore(flags);
 console_printf("wifi-desktop-active-remove5-close: version=1 disabled=%u keys_erased=%u provider_retired=%u dma_retained=1\n",
  (uint64_t)disabled,(uint64_t)erased,(uint64_t)wifi_stack5.retired);
 return disabled && wifi_stack5.retired;
}
static bool wifi_desktop5_run(void *context,bool (*begin)(void *),bool (*running)(void *)){
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
 if(wifi_desktop5.removed){
  wifi_desktop5.return_safe=wifi_desktop5_finish_removed();
  good=wifi_desktop5.return_safe && wifi_desktop5.connected &&
   wifi_desktop5.data_verified[0] && wifi_desktop5.endurance_passed;
 }else if(wifi_desktop5.replug){
  wifi_desktop5.return_safe=parent && (!wifi_stream5.attempted ||
   (wifi_stream5.connection_closed && wifi_stack5.detached));
  good=wifi_desktop5.return_safe && wifi_stream5.attempted && wifi_desktop5.connected;
 }else if(wifi_desktop5.reconnect){
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
 wifi_desktop5.driver_bound=false;
 event_irq_restore(flags);
 console_printf("wifi-desktop5-final: started=%u connected=%u requested_disconnect=%u retired=%u return_safe=%u\n",
  (uint64_t)wifi_desktop5.started,(uint64_t)wifi_desktop5.connected,(uint64_t)(wifi_desktop5.disconnect.serial!=0),
  (uint64_t)(wifi_desktop5.replug?wifi_stack5.detached:good),(uint64_t)wifi_desktop5.return_safe);
 if(wifi_desktop5_hub_trial()){
  bool complete=good && wifi_desktop5.disconnect.serial && wifi_desktop5.data_verified[0];
  console_printf("wifi-desktop-hub5-final: version=1 complete=%u parent=%u port=%u data=%u disconnected=%u retired=%u\n",
   (uint64_t)complete,(uint64_t)wifi_desktop5.device->parent_slot_id,(uint64_t)wifi_desktop5.device->hub_port_id,
   (uint64_t)wifi_desktop5.data_verified[0],(uint64_t)(wifi_desktop5.disconnect.serial!=0),(uint64_t)wifi_stack5.retired);
  good=complete;
 }
 if(wifi_desktop5.return_safe || !wifi_desktop5.started)wifi_desktop5_gate_end();
 wifi_desktop5.context=NULL;wifi_desktop5.begin=NULL;wifi_desktop5.running=NULL;
 wifi_stack5.context=NULL;wifi_stack5.exercise=NULL;
 zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);
 return good;
}
static bool wifi_desktop5_capture(void *context,bool (*begin)(void *),bool (*running)(void *),
                                  bool reconnect,bool replug,bool active_remove){
 if(!context || !begin || !running || wifi_desktop5.requested || wifi_stack5.selected ||
    wifi_stream5.attempted || wifi_join5_after || wifi_http_reconnect5_reserved)return false;
 wifi_desktop5.requested=true;wifi_desktop5.reconnect=reconnect;wifi_desktop5.replug=replug;
 wifi_desktop5.active_remove=active_remove;
 return wifi_desktop5_run(context,begin,running);
}
bool xhci_wifi_capture_desktop5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 return wifi_desktop5_capture(context,begin,running,false,false,false);
}
bool xhci_wifi_capture_desktop_reconnect5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 return wifi_desktop5_capture(context,begin,running,true,false,false);
}

static bool wifi_desktop5_wait_replacement(struct wifi_root_reattach *trial,void *context,
                                           bool (*running)(void *),bool connected){
 bool admitted=connected && wifi_desktop5.return_safe && (wifi_desktop5.replug?wifi_stack5.detached:wifi_stack5.retired) &&
  wifi_desktop5.disconnect.serial && running(context) &&
  wifi_root_reattach_begin(trial,wifi_desktop5.device);
 if(admitted) {
  console_write("wifi-root-reattach: waiting for Wi-Fi unplug and reinsert; workspace stays mounted\n");
  while(running(context)) {
   enum wifi_root_reattach_phase phase=wifi_root_reattach_step(trial);
   if(phase>=WIFI_ROOT_READY)break;
   wifi_desktop5_turn();
  }
 }
 bool complete=trial->phase==WIFI_ROOT_READY;
 console_printf("wifi-root-reattach-final: version=1 complete=%u phase=%u old_retired=%u new_identity=%u firmware_ep=%u network_reconnected=0\n",
  (uint64_t)complete,(uint64_t)trial->phase,(uint64_t)(trial->previous && trial->previous->slot_disabled),
  (uint64_t)(trial->replacement && trial->replacement->wifi_probe_ok),
  (uint64_t)(trial->replacement && trial->replacement->firmware.configured));
 return complete;
}
bool xhci_wifi_capture_desktop_reattach5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 struct wifi_root_reattach trial={0};
 bool connected=wifi_desktop5_capture(context,begin,running,false,false,false);
 return wifi_desktop5_wait_replacement(&trial,context,running,connected);
}

/* The old slot must be disabled before switching RF owners. The registered
 * station stays DOWN between connections, with no callbacks into either slot. */
static bool wifi_desktop5_select_replacement(const struct wifi_root_reattach *trial){
 uint64_t flags=event_irq_save();
 const struct xhci_device *old=trial->previous;
 struct xhci_device *replacement=trial->replacement;
 bool ready=wifi_desktop5.gate && !wifi_radio5_bank && wifi_stream5_current==&wifi_stream5_slots[0] &&
  wifi_stack5_current==&wifi_stack5_slots[0] && trial->phase==WIFI_ROOT_READY && old && old==wifi_cal5.d &&
  old->removed && old->slot_disabled && old->wifi_replaced && replacement && replacement!=old &&
  !device_removed(replacement) && replacement->wifi_probe_ok && replacement->firmware.configured &&
  !replacement->firmware.attempted &&
  old->port_id==replacement->port_id && device_by_slot(replacement->slot_id)==replacement &&
  wifi_desktop5.scopes[0].disconnected && wifi_stream5.connection_closed && wifi_stack5.detached &&
  !wifi_stack5.running && !wifi_stack5_net.attached && !wifi_stack5_net.io.context &&
  wifi_stack5_net.state==MT76_NET5_DOWN && !wifi_stack5_net.retire_requested &&
  !wifi_stack5_net.pending && !wifi_stack5_net.count && !wifi_stack5_net.callbacks &&
  !wifi_stack5_net.io_busy && !wifi_stack5_net.polling && !wifi_stack5_net.delivering &&
  !old->firmware.active && !old->firmware.pending && !old->firmware.mcu.pending &&
  !wifi_mcu_observer.receipt && !wifi_mcu_loss.trace && !wifi_channel_vco5.active &&
  !wifi_cal5.active && !wifi_join5.active && !wifi_join5_after && !xhci.sync_users &&
  !xhci.command_pending && !strict_control.device && !wifi_fence5_native.bound;
 for(unsigned i=0;ready && i<XHCI_MAX_DEVICES;i++){
  const struct xhci_device *d=&xhci.devices[i];
  if(d!=replacement && d->present && !d->slot_disabled && d->wifi_probe_attempted)ready=false;
 }
 if(ready){
  wifi_service_remove(wifi_desktop5.owner);
  wifi_desktop5.owner=0;wifi_desktop5.device=NULL;wifi_desktop5.connected=false;
  wifi_desktop5.connect=(struct wifi_service_command){0};
  wifi_desktop5.disconnect=(struct wifi_service_command){0};
  wifi_radio5_bank=1;
  wifi_stream5_current=&wifi_stream5_slots[1];
  wifi_stack5_current=&wifi_stack5_slots[1];
 }
 event_irq_restore(flags);
 return ready;
}

static bool wifi_desktop5_reload(const struct wifi_root_reattach *trial){
 bool selected=wifi_desktop5_select_replacement(trial);
 console_printf("wifi-desktop-replug5-owner: version=1 acquired=%u\n",(uint64_t)selected);
 if(!selected)return false;
 const struct xhci_wifi_firmware *old=&trial->previous->firmware;
 bool loaded=
#ifdef SRVROS_XHCI_STACK5_TEST
  stack_test_reload_firmware(trial->replacement,old->image,old->image_size) &&
#else
  xhci_wifi_capture_firmware(old->image,old->image_size) &&
#endif
  trial->replacement->firmware.ready;
 console_printf("wifi-desktop-replug5-firmware: version=1 slot=%u ready=%u\n",
  (uint64_t)trial->replacement->slot_id,(uint64_t)loaded);
 return loaded && xhci_wifi_cal5_cleanup_enable() && xhci_wifi_cal5_prime_enable() &&
  xhci_wifi_cal5_survey_enable() && xhci_wifi_join5_radio_enable();
}

bool xhci_wifi_capture_desktop_replug5(void *context,bool (*begin)(void *),bool (*running)(void *),
                                      bool (*checkpoint)(void)){
 if(!checkpoint)return false;
 bool connected=wifi_desktop5_capture(context,begin,running,false,true,false);
 struct wifi_root_reattach trial={0};
 bool replaced=wifi_desktop5_wait_replacement(&trial,context,running,connected);
 bool good=false;
 if(replaced && checkpoint()){
  while(running(context) && !wifi_desktop5_gate_begin())wifi_desktop5_turn();
  if(wifi_desktop5.gate){
   if(wifi_desktop5_reload(&trial))good=wifi_desktop5_run(context,begin,running);
   if(!good && !wifi_stream5.attempted && !wifi_stack5_net.attached){
    if(wifi_desktop5.gate || wifi_desktop5_gate_begin())
     wifi_desktop5.return_safe=xhci_retire_slot(trial.replacement->slot_id);
    else wifi_desktop5.return_safe=false;
   }
  }
 }
 bool registered=wifi_stack5_net.identity!=0;
 bool retired=wifi_desktop5.return_safe && registered && mt76_net5_retire(&wifi_stack5_net);
 wifi_stack5.retired=retired;
 wifi_desktop5.return_safe=wifi_desktop5.return_safe && (!registered || retired);
 if(wifi_desktop5.return_safe)wifi_desktop5_gate_end();
 good=good && retired && wifi_desktop5.connections==2 && wifi_desktop5.disconnects==2 &&
  wifi_desktop5.data_verified[0] && wifi_desktop5.data_verified[1];
 console_printf("wifi-desktop-replug5-final: version=1 complete=%u replaced=%u connections=%u disconnects=%u data_after=%u provider_retired=%u return_safe=%u\n",
  (uint64_t)good,(uint64_t)replaced,(uint64_t)wifi_desktop5.connections,(uint64_t)wifi_desktop5.disconnects,
  (uint64_t)wifi_desktop5.data_verified[1],(uint64_t)retired,(uint64_t)wifi_desktop5.return_safe);
 return good;
}
bool xhci_wifi_capture_desktop_active_remove5(void *context,bool (*begin)(void *),bool (*running)(void *)){
 bool completed=wifi_desktop5_capture(context,begin,running,false,false,true) && wifi_desktop5.removed;
 console_printf("wifi-desktop-active-remove5-final: version=1 complete=%u removed=%u sustained=%u return_safe=%u\n",
  (uint64_t)completed,(uint64_t)wifi_desktop5.removed,(uint64_t)wifi_desktop5.endurance_passed,
  (uint64_t)wifi_desktop5.return_safe);
 return completed;
}
bool xhci_wifi_desktop5_return_safe(void){return wifi_desktop5.return_safe;}
