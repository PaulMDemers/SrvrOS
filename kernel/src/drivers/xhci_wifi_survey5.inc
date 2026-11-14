/* Runs only after the first calibration/receive MAC has closed. Later windows
 * repeat receive, not calibration. Inspection and all copies follow exact Stop. */
static struct wifi_cal5_state wifi_survey5_next;
static bool wifi_survey5_now(uint64_t *out){
 uint64_t now=timer_monotonic_us();
 if(!out || !wifi_survey5.started || now<wifi_survey5.previous || now<wifi_survey5.start)
  return wifi_survey5_fail(3);
 wifi_survey5.previous=now;*out=now;return true;
}
static void wifi_survey5_context(unsigned phase){
 struct xhci_device *d=wifi_cal5.d;
 console_printf("wifi-survey5-context: window=%u phase=%u",(uint64_t)wifi_survey5.window,(uint64_t)phase);
 for(unsigned i=0;i<8;i++)console_printf(" w%u=%x",(uint64_t)i,(uint64_t)context_read32(d->output_context,9,i));
 console_write("\n");
}
static bool wifi_survey5_frame(uint64_t *physical,void **pointer){
 if(!alloc_frame_zero(physical,pointer))return false;
 struct xhci_device *d=wifi_cal5.d;uint64_t q=*physical;
 const uint64_t used[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,
  d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,
  d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,
  xhci.command_ring_phys,xhci.event_ring_phys,wifi_survey5_next.plan.ring};
 if(!q || (q&4095) || *pointer!=pmm_phys_to_virt(q) || wifi_prime5_alias(q))return false;
 for(unsigned i=0;i<sizeof used/sizeof used[0];i++)if(q==used[i])return false;
 for(unsigned i=0;i<8;i++)if(q==wifi_survey5_next.plan.physical[i] || q==wifi_survey5_next.cleanup_physical[i])return false;
 return true;
}
static bool wifi_survey5_retarget(void){
 if(!wifi_survey5_held() || wifi_survey5.count>=WIFI_SURVEY5_WINDOWS-1 ||
    !wifi_discovery5_owned(&wifi_cal5,&wifi_cal5.window) ||
    wifi_cal5.discovery_status!=MT76_DISC5_UNAVAILABLE || wifi_cal5.choice.selected ||
    xhci.command_pending || xhci.sync_users || strict_control.device || wifi_cal5.d->control_pending)
  return wifi_survey5_fail(4);
 unsigned n=wifi_survey5.count;
 copy_bytes((uint8_t *)&wifi_survey5.prior[n].owner,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);
 copy_bytes((uint8_t *)&wifi_survey5.prior[n].guard,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);
 copy_bytes(wifi_survey5.prior[n].ring,(const uint8_t *)wifi_cal5.ep.ring,4096);
 for(unsigned i=0;i<8;i++)copy_bytes(wifi_survey5.prior[n].pages[i],wifi_cal5.plan.buffer[i],4096);
 wifi_survey5.count++;wifi_survey5.window++;
 if(!wifi_survey5_held())return wifi_survey5_fail(4);
 /* Allocate into private construction storage. An uncertain command or any
  * allocation prefix retains everything and can never publish another window. */
 zero_bytes(&wifi_survey5_next,sizeof wifi_survey5_next);
 uint64_t ring=0;void *p=NULL;
 if(!wifi_survey5_frame(&ring,&p))return wifi_survey5_fail(5);
 struct xhci_trb *trbs=p;write_link_trb(trbs,ring,true);
 wifi_survey5_next.plan=(struct mt76_rx_window_plan){.budget_us=10000000,
  .ring=ring,.slot=wifi_cal5.d->slot_id,.dci=9,.country={'U','S'},.channel=161};
 for(unsigned i=0;i<8;i++){
  uint64_t q;if(!wifi_survey5_frame(&q,&p))return wifi_survey5_fail(5);
  wifi_survey5_next.plan.physical[i]=wifi_survey5_next.pin_pages[i]=q;
  wifi_survey5_next.plan.buffer[i]=p;wifi_survey5_next.pin_v[i]=(uintptr_t)p;
 }
 for(unsigned i=0;i<8;i++){
  uint64_t q;if(!wifi_survey5_frame(&q,&p))return wifi_survey5_fail(5);
  wifi_survey5_next.cleanup_physical[i]=q;wifi_survey5_next.cleanup_buffer[i]=p;
  wifi_survey5_next.cleanup_pin[i]=(uintptr_t)p;
 }
 if(!mt76_rx5_window_plan_valid(&wifi_survey5_next.plan))return wifi_survey5_fail(5);
 struct xhci_device *d=wifi_cal5.d;uint32_t words[8];
 for(unsigned i=0;i<8;i++)words[i]=context_read32(d->output_context,9,i);
 if((words[0]&7)!=3 || (words[2]|((uint64_t)words[3]<<32))!=wifi_cal5.stop_dequeue)
  return wifi_survey5_fail(6);
 wifi_survey5_context(0);
 uint64_t value=ring|1,slot=0;struct xhci_trb cmd={.parameter_low=(uint32_t)value,
  .parameter_high=value>>32,.control=(16u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 uint64_t flags=event_irq_save();wifi_survey5.switching=true;firmware_command_exact=true;
 bool ok=wifi_survey5_held() && wifi_prime5_equal(&wifi_cal5,&wifi_survey5.prior[n].guard,sizeof wifi_cal5);
 for(unsigned i=0;i<8;i++)if(context_read32(d->output_context,9,i)!=words[i])ok=false;
 if(ok)ok=submit_command_trb(&cmd,16,&slot) && slot==d->slot_id;
 firmware_command_exact=false;__atomic_thread_fence(__ATOMIC_ACQUIRE);
 for(unsigned i=0;i<8;i++)if(context_read32(d->output_context,9,i)!=(i==2?(uint32_t)value:i==3?(uint32_t)(value>>32):words[i]))ok=false;
 ok=ok && wifi_survey5_held() && wifi_prime5_equal(&wifi_cal5,&wifi_survey5.prior[n].guard,sizeof wifi_cal5);
 if(ok){
  /* Only calibration and immutable outer identities survive into this owner. */
  wifi_survey5_next.d=d;wifi_survey5_next.cal=wifi_cal5.cal;wifi_survey5_next.probe=wifi_cal5.probe;
  wifi_survey5_next.commands=wifi_cal5.commands;wifi_survey5_next.operations=wifi_cal5.operations;
  wifi_survey5_next.command_ring=wifi_cal5.command_ring;wifi_survey5_next.output=wifi_cal5.output;
  wifi_survey5_next.input=wifi_cal5.input;wifi_survey5_next.ep0=wifi_cal5.ep0;
  wifi_survey5_next.descriptor=wifi_cal5.descriptor;
  wifi_survey5_next.ep=(struct xhci_bulk_endpoint){.ring=trbs,.ring_phys=ring,.dci=9,.packet=512,.cycle=true};
  wifi_survey5_next.pin_ring=ring;wifi_survey5_next.pin_rptr=(uintptr_t)trbs;
  wifi_survey5_next.attempted=wifi_survey5_next.active=wifi_survey5_next.configured=wifi_survey5_next.cleanup_enabled=true;
  copy_bytes((uint8_t *)&wifi_cal5,(const uint8_t *)&wifi_survey5_next,sizeof wifi_cal5);
  wifi_prime5.first_post=wifi_prime5.resumed=false;wifi_prime5.first_post_us=0;
 }else firmware_command_failed=true;
 wifi_survey5.switching=false;event_irq_restore(flags);
 wifi_survey5_context(1);
 console_printf("wifi-survey5-handoff: window=%u ok=%u old_ring=%x old_dequeue=%x new_ring=%x slot=%u retained=1 reused=0 context_preserved=%u\n",
  (uint64_t)wifi_survey5.window,(uint64_t)ok,wifi_survey5.prior[n].guard.plan.ring,
  wifi_survey5.prior[n].guard.stop_dequeue,ring,slot,(uint64_t)ok);
 return ok?true:wifi_survey5_fail(6);
}
static bool wifi_survey5_history_owned(void *c,const struct mt76_rx_window *w){
 if(!wifi_discovery5_owned(&wifi_cal5,&wifi_cal5.window) || !wifi_survey5_held())return false;
 for(unsigned n=0;n<wifi_survey5.count;n++)if(c==&wifi_survey5.prior[n].owner && w==&wifi_survey5.prior[n].owner.window)return true;
 return false;
}
static void wifi_survey5_discovery_emit(void){
 console_printf("wifi-discovery5: complete=%u inputs=%u parsed=%u rows=%u matches=%u status=%u selected=%u channel=%u rsn_bytes=%u generation=%u retained=1 network_out=0 authenticated=0\n",
  (uint64_t)wifi_cal5.discovery.complete,(uint64_t)wifi_cal5.discovery.inputs,
  (uint64_t)wifi_cal5.discovery.parsed,(uint64_t)wifi_cal5.discovery.count,
  (uint64_t)wifi_cal5.choice.matches,(uint64_t)wifi_cal5.discovery_status,
  (uint64_t)wifi_cal5.choice.selected,(uint64_t)wifi_cal5.choice.row.channel,
  (uint64_t)wifi_cal5.choice.rsn_size,wifi_cal5.choice.generation);
}
static void wifi_survey5_window_emit(unsigned begin){
 for(unsigned i=0;i<wifi_cal5.window.attempts && i<8;i++){
  const struct mt76_rx_window_item *v=&wifi_cal5.window.item[i];
  console_printf("wifi-survey5-cal-rx: index=%u submitted=%u completed=%u cc=%u remaining=%u actual=%u inspected=%u view=%u beacon_status=%u beacon=%u mpdu=%u interval=%u ssid_size=%u privacy=%u rsn=%u wpa=%u\n",
   (uint64_t)i,(uint64_t)v->submitted,(uint64_t)v->completed,(uint64_t)v->completion,
   (uint64_t)v->remaining,(uint64_t)v->actual,(uint64_t)v->inspected,(uint64_t)v->view_status,
   (uint64_t)v->beacon_status,(uint64_t)v->beacon,(uint64_t)v->mpdu,(uint64_t)v->interval,
   (uint64_t)v->ssid_size,(uint64_t)v->privacy,(uint64_t)v->rsn,(uint64_t)v->wpa);
 }
 console_printf("wifi-survey5-window: version=1 window=%u begin=%u end=%u complete=%u attempts=%u completions=%u beacons=%u elapsed_us=%u retained=1 reused=0\n",
  (uint64_t)wifi_survey5.window,(uint64_t)begin,(uint64_t)wifi_cal5.operations,
  (uint64_t)wifi_cal5.window.complete,(uint64_t)wifi_cal5.window.attempts,
  (uint64_t)wifi_cal5.window.completions,(uint64_t)wifi_cal5.window.beacons,wifi_cal5.window.elapsed_us);
}
static void wifi_survey5_clock_emit(void){
 console_printf("wifi-survey5-clock: window=%u start_us=%u elapsed_us=%u deadline_us=%u retained=1\n",
  (uint64_t)wifi_survey5.window,wifi_cal5.window.start_us,wifi_cal5.window.elapsed_us,wifi_cal5.window.deadline_us);
}
static bool wifi_survey5_run(void){
 if(!wifi_survey5.enabled || wifi_survey5.complete || !wifi_survey5.started || !wifi_prime5_held())return false;
 console_printf("wifi-survey5-begin: version=1 maximum=16 slots=8 budget_us=10000000 start_us=%u deadline_us=%u\n",wifi_survey5.start,wifi_survey5.deadline);
 wifi_survey5_clock_emit();
 for(;;){
  if(!wifi_discovery5_inspect())return wifi_survey5_fail(7);
  if(wifi_cal5.choice.selected){
   /* The existing connection handoff requires its complete eight-TD window.
    * A partial deadline observation is retained, never promoted into a new
    * connection recipe merely because one of its frames names the target. */
   if(wifi_cal5.window.attempts!=8 || wifi_cal5.window.completions!=8){
    zero_bytes(&wifi_cal5.choice,sizeof wifi_cal5.choice);wifi_cal5.choice.attempted=true;
    wifi_cal5.discovery_status=MT76_DISC5_UNAVAILABLE;wifi_survey5.reason=1;break;
   }
   for(unsigned n=0;n<wifi_survey5.count;n++){
    const struct wifi_cal5_state *s=&wifi_survey5.prior[n].owner;
    const struct mt76_discovery_io io={(void *)s,wifi_survey5_history_owned};
    enum mt76_discovery5_status status=mt76_discovery5_history(&wifi_cal5.choice,&s->discovery,&s->window,&io);
    if(status!=MT76_DISC5_OK){
     if(status==MT76_DISC5_OWNERSHIP || status==MT76_DISC5_INVALID)return wifi_survey5_fail(8);
     unsigned matches=wifi_cal5.choice.matches;
     zero_bytes(&wifi_cal5.choice,sizeof wifi_cal5.choice);wifi_cal5.choice.attempted=true;wifi_cal5.choice.matches=matches;
     wifi_cal5.discovery_status=status;wifi_survey5.reason=3;break;
    }
   }
   wifi_survey5.selected=wifi_cal5.choice.selected;break;
  }
  if(wifi_cal5.discovery_status!=MT76_DISC5_UNAVAILABLE){wifi_survey5.reason=3;break;}
  uint64_t now;if(!wifi_survey5_now(&now))return false;
  if(now>=wifi_survey5.deadline || wifi_survey5.deadline-now<100000){wifi_survey5.reason=1;break;}
  if(wifi_survey5.window+1>=WIFI_SURVEY5_WINDOWS){wifi_survey5.reason=2;break;}
  if(!wifi_survey5_retarget())return false;
  unsigned begin=wifi_cal5.operations;
  console_printf("wifi-survey5-receive: window=%u start_us=%u deadline_us=%u\n",(uint64_t)wifi_survey5.window,timer_monotonic_us(),wifi_survey5.deadline);
  const struct mt76_mac_cycle_io io={&wifi_cal5,wifi_cal5_owned,wifi_cal5_read,wifi_cal5_write,wifi_cal5_delay,wifi_cal5_now,wifi_prime5_window};
  bool good=mt76_mac_cycle_run(&wifi_cal5.mac,&io),stopped=wifi_cal5_stop();
  wifi_cal5_cleanup_emit();
  const struct mt76_rx_window_io finish={.context=&wifi_cal5,.owned=wifi_cal5_finish_owned};
  const struct mt76_rx_window_stop stop={.dequeue=wifi_cal5.stop_dequeue,.slot=wifi_cal5.d->slot_id,.dci=9,.state=wifi_cal5.stop_state,.cleanup_posts=wifi_cal5.cleanup_posts};
  good=mt76_rx5_window_finish(&wifi_cal5.window,&finish,good && stopped?&stop:NULL) && good && stopped;
  wifi_survey5_window_emit(begin);
  wifi_survey5_clock_emit();
  if(!good || !wifi_cal5.mac.complete || !wifi_cal5_cleanup_held())return wifi_survey5_fail(9);
 }
 uint64_t now;if(!wifi_survey5_now(&now) || !wifi_discovery5_owned(&wifi_cal5,&wifi_cal5.window))return false;
 wifi_survey5.complete=true;wifi_survey5_discovery_emit();
 console_printf("wifi-survey5: version=1 complete=1 selected=%u reason=%u windows=%u retained_windows=%u elapsed_us=%u retained=1 reused=0 inspected_after_stop=1\n",
  (uint64_t)wifi_survey5.selected,(uint64_t)wifi_survey5.reason,(uint64_t)wifi_survey5.window+1,
  (uint64_t)wifi_survey5.count,now-wifi_survey5.start);
 return true; /* Proved child closure; selected is a separate workload gate. */
}
static bool wifi_survey5_cleanup_only(void){
 return wifi_survey5.enabled && wifi_survey5.complete && !wifi_survey5.selected &&
  (wifi_cal5.discovery_status==MT76_DISC5_UNAVAILABLE || wifi_cal5.discovery_status==MT76_DISC5_AMBIGUOUS ||
   wifi_cal5.discovery_status==MT76_DISC5_CONFLICT || wifi_cal5.discovery_status==MT76_DISC5_SECURITY) &&
  wifi_discovery5_owned(&wifi_cal5,&wifi_cal5.window) &&
  wifi_http_reconnect5_unattempted() &&
  !wifi_power5.attempted && !wifi_handoff5.attempted && !wifi_join5.attempted;
}
