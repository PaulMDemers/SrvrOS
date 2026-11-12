/* Explicit connection-profile prime. The first ring and all 32 payload pages
 * remain retained and uninspected. Only a proved stopped endpoint is retargeted;
 * Set TR Dequeue preserves its USB toggle. No reset or uncertain rollback. */
static struct {
 bool enabled,saved,switching,after,resumed,first_post,failed;
 unsigned fault,first_operations;uint64_t first_post_us,old_dequeue;
 struct xhci_device *d;struct mt76_mac_cycle first_mac;
 uint32_t endpoint[8];uint8_t early[sizeof wifi_early5],ring[4096];
} wifi_prime5;
static bool wifi_prime5_equal(const void *a,const void *b,size_t n){
 const uint8_t *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return false;return true;
}
bool xhci_wifi_cal5_prime_enable(void){
 if(wifi_prime5.enabled || !wifi_cal5.cleanup_enabled || wifi_cal5.attempted ||
    wifi_cal5.active || wifi_cal5.configured || wifi_early5.requested || wifi_early5.attempted)return false;
 wifi_prime5.enabled=true;return true;
}
static bool wifi_prime5_fail(unsigned fault){
 if(!wifi_prime5.fault)wifi_prime5.fault=fault;
 wifi_prime5.failed=wifi_cal5.failed=true;
 console_printf("wifi-prime5-fault: version=1 reason=%u retained=1\n",(uint64_t)wifi_prime5.fault);
 if(wifi_cal5.d)wifi_mcu_fail(wifi_cal5.d);
 return false;
}
static bool wifi_prime5_alias(uint64_t p){
 if(wifi_survey5_alias(p))return true;
 if(!wifi_prime5.enabled || !p)return false;
 if(p==wifi_early5.ring)return true;
 for(unsigned i=0;i<WIFI_EARLY5_SLOTS;i++)if(p==wifi_early5.physical[i])return true;
 return false;
}
static bool wifi_prime5_route_new(uint64_t p){
 /* Before the exact command succeeds, even an unknown DCI9 event belongs to
  * the frozen old owner and must quarantine. Old-ring events never escape it. */
 if(!wifi_prime5.after || (p>=wifi_early5.ring && p<wifi_early5.ring+4096))return false;
 if(!wifi_cal5.stopped)return p>=wifi_cal5.plan.ring && p<wifi_cal5.plan.ring+4096;
 return true; /* Later explicit handoff/stream owners validate their own ring. */
}
static bool wifi_prime5_held(void){
 if(!wifi_prime5.enabled)return true;
 if(wifi_prime5.failed)return false;
 if(!wifi_prime5.saved)return !wifi_prime5.after && !wifi_prime5.switching;
 if(wifi_prime5.d!=wifi_cal5.d || !wifi_prime5.first_mac.complete ||
    !wifi_prime5.first_mac.mac_disabled || !wifi_prime5.first_mac.tx_drained ||
    !wifi_prime5.first_mac.rx_drained || !wifi_prime5.first_mac.filter_restored ||
    !wifi_prime5.first_mac.txop_restored || wifi_early5.failed || !wifi_early5.frozen ||
    !wifi_cal5.cal.complete || !wifi_cal5.probe.complete || wifi_cal5.commands!=5 ||
    wifi_prime5.old_dequeue!=wifi_early5.ring+16*wifi_early5.completed+1 ||
    !wifi_prime5_equal(&wifi_early5,wifi_prime5.early,sizeof wifi_early5) ||
    (uintptr_t)pmm_phys_to_virt(wifi_early5.ring)!=wifi_early5.ring_pointer ||
    !wifi_prime5_equal(pmm_phys_to_virt(wifi_early5.ring),wifi_prime5.ring,4096))return false;
 for(unsigned i=0;i<WIFI_EARLY5_SLOTS;i++)
  if((uintptr_t)pmm_phys_to_virt(wifi_early5.physical[i])!=wifi_early5.pointers[i])return false;
 return true;
}
static bool wifi_prime5_forward_state(unsigned state){
 if(!wifi_prime5.after)return state==1;
 if(!wifi_prime5_held())return false;
 if(state==1){if(!wifi_prime5.first_post)return false;wifi_prime5.resumed=true;return true;}
 if(state!=3 || wifi_prime5.resumed)return false;
 if(!wifi_prime5.first_post)return true;
 uint64_t now=timer_monotonic_us();
 return now>=wifi_prime5.first_post_us && now-wifi_prime5.first_post_us<100000;
}
static bool wifi_prime5_new_frame(uint64_t *physical,void **p){
 if(!alloc_frame_zero(physical,p))return false;
 struct xhci_device *d=wifi_cal5.d;uint64_t q=*physical;
 const uint64_t used[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,
  d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,
  d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,
  xhci.command_ring_phys,xhci.event_ring_phys,wifi_cal5.plan.ring};
 if(!q || (q&4095) || *p!=pmm_phys_to_virt(q) || wifi_prime5_alias(q))return false;
 for(unsigned i=0;i<sizeof used/sizeof used[0];i++)if(q==used[i])return false;
 for(unsigned i=0;i<8;i++)if(q==wifi_cal5.plan.physical[i] || q==wifi_cal5.cleanup_physical[i])return false;
 return true;
}
static void wifi_prime5_context_emit(unsigned phase){
 struct xhci_device *d=wifi_cal5.d;
 console_printf("wifi-prime5-context: phase=%u w0=%x w1=%x w2=%x w3=%x w4=%x w5=%x w6=%x w7=%x\n",
  (uint64_t)phase,(uint64_t)context_read32(d->output_context,9,0),(uint64_t)context_read32(d->output_context,9,1),
  (uint64_t)context_read32(d->output_context,9,2),(uint64_t)context_read32(d->output_context,9,3),
  (uint64_t)context_read32(d->output_context,9,4),(uint64_t)context_read32(d->output_context,9,5),
  (uint64_t)context_read32(d->output_context,9,6),(uint64_t)context_read32(d->output_context,9,7));
}
static bool wifi_prime5_retarget(void){
 struct xhci_device *d=wifi_cal5.d;
 /* Called immediately after the first emit, while early.requested has been
  * withdrawn and cleanup re-enabled; no forward publication is possible. */
 if(!wifi_prime5.enabled || wifi_prime5.saved || wifi_prime5.failed ||
    !d || !wifi_cal5.active || !wifi_cal5.configured || wifi_cal5.failed ||
    !wifi_cal5.stopped || wifi_cal5.stop_state!=3 || !wifi_early5.frozen ||
    !wifi_early5.stop_seen || wifi_early5.failed || !wifi_early5_layout() ||
    !wifi_cal5.cal.complete || !wifi_cal5.probe.complete || wifi_cal5.commands!=5 ||
    !wifi_cal5.mac.complete || !wifi_cal5.mac.mac_disabled ||
    !wifi_cal5.mac.rx_drained || !wifi_cal5.mac.tx_drained ||
    !wifi_cal5.mac.filter_restored || !wifi_cal5.mac.txop_restored ||
    xhci.command_pending || xhci.command_failed || firmware_command_failed ||
    xhci.sync_users || strict_control.device || d->control_pending || d->firmware.mcu.pending)
  return wifi_prime5_fail(3);
 wifi_prime5.d=d;wifi_prime5.first_mac=wifi_cal5.mac;
 wifi_prime5.first_operations=wifi_cal5.operations;wifi_prime5.old_dequeue=wifi_cal5.stop_dequeue;
 for(unsigned i=0;i<8;i++)wifi_prime5.endpoint[i]=context_read32(d->output_context,9,i);
 if((wifi_prime5.endpoint[0]&7)!=3 ||
    (wifi_prime5.endpoint[2]|((uint64_t)wifi_prime5.endpoint[3]<<32))!=wifi_prime5.old_dequeue)
  return wifi_prime5_fail(3);
 copy_bytes(wifi_prime5.early,(const uint8_t *)&wifi_early5,sizeof wifi_early5);
 copy_bytes(wifi_prime5.ring,(const uint8_t *)wifi_cal5.ep.ring,4096);wifi_prime5.saved=true;
 if(!wifi_prime5_held())return wifi_prime5_fail(3);
 /* Allocate into locals first so even partial allocation can never masquerade
  * as a configured ring. Every allocated frame remains retained on refusal. */
 uint64_t ring=0;void *p=NULL;
 if(!wifi_prime5_new_frame(&ring,&p))return wifi_prime5_fail(4);
 wifi_cal5.plan=(struct mt76_rx_window_plan){.budget_us=10000000,.ring=ring,
  .slot=d->slot_id,.dci=9,.country={'U','S'},.channel=161};
 struct xhci_trb *r=p;write_link_trb(r,ring,true);
 for(unsigned i=0;i<8;i++){
  uint64_t q=0;if(!wifi_prime5_new_frame(&q,&p))return wifi_prime5_fail(4);
  wifi_cal5.plan.physical[i]=wifi_cal5.pin_pages[i]=q;
  wifi_cal5.plan.buffer[i]=p;wifi_cal5.pin_v[i]=(uintptr_t)p;
 }
 for(unsigned i=0;i<8;i++){
  uint64_t q=0;if(!wifi_prime5_new_frame(&q,&p))return wifi_prime5_fail(4);
  wifi_cal5.cleanup_physical[i]=q;wifi_cal5.cleanup_buffer[i]=p;wifi_cal5.cleanup_pin[i]=(uintptr_t)p;
 }
 if(!mt76_rx5_window_plan_valid(&wifi_cal5.plan) || !wifi_prime5_held())return wifi_prime5_fail(4);
 uint64_t value=ring|1;struct xhci_trb cmd={.parameter_low=(uint32_t)value,.parameter_high=value>>32,
  .control=(16u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 wifi_prime5_context_emit(0);
 uint64_t flags=event_irq_save(),slot=0;wifi_prime5.switching=true;firmware_command_exact=true;
 /* Recheck the full stopped context immediately before the command. */
 bool ok=wifi_prime5_held();
 for(unsigned i=0;i<8;i++)if(context_read32(d->output_context,9,i)!=wifi_prime5.endpoint[i])ok=false;
 if(ok)ok=submit_command_trb(&cmd,16,&slot) && slot==d->slot_id;
 firmware_command_exact=false;
 __atomic_thread_fence(__ATOMIC_ACQUIRE);
 for(unsigned i=0;i<8;i++){
  uint32_t want=i==2?(uint32_t)value:i==3?(uint32_t)(value>>32):wifi_prime5.endpoint[i];
  if(context_read32(d->output_context,9,i)!=want)ok=false;
 }
 ok=ok && wifi_prime5_held();
 if(ok){
  wifi_cal5.ep=(struct xhci_bulk_endpoint){.ring=r,.ring_phys=ring,.dci=9,.packet=512,.cycle=true};
  wifi_cal5.pin_ring=ring;wifi_cal5.pin_rptr=(uintptr_t)r;
  wifi_cal5.mac=(struct mt76_mac_cycle){0};
  wifi_cal5.stop_dequeue=0;wifi_cal5.stop_state=0;
  wifi_cal5.stopped=wifi_cal5.stopping=wifi_cal5.stop_attempted=false;
  wifi_prime5.after=true;
 }else firmware_command_failed=true;
 wifi_prime5.switching=false;event_irq_restore(flags);
 wifi_prime5_context_emit(1);
 console_printf("wifi-prime5-handoff: version=1 ok=%u old_ring=%x old_dequeue=%x new_ring=%x slot=%u state=3 dcs=1 protected=33 new_pages=17 retained=1 reused=0 context_preserved=%u\n",
  (uint64_t)ok,wifi_early5.ring,wifi_prime5.old_dequeue,ring,slot,(uint64_t)ok);
 return ok?true:wifi_prime5_fail(5);
}
static bool wifi_prime5_finish(bool ok){
 ok=ok && wifi_prime5.after && wifi_prime5.resumed && wifi_prime5.first_post &&
  wifi_prime5_held() && wifi_cal5.operations>wifi_prime5.first_operations;
 console_printf("wifi-prime5: version=1 complete=%u first_operations=%u operations=%u commands=%u resumed=%u held=%u retained=1 reused=0 inspected=0\n",
  (uint64_t)ok,(uint64_t)wifi_prime5.first_operations,(uint64_t)wifi_cal5.operations,
  (uint64_t)wifi_cal5.commands,(uint64_t)wifi_prime5.resumed,(uint64_t)wifi_prime5_held());
 return ok?true:wifi_prime5_fail(7);
}
