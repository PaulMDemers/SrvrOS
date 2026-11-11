/* Opt-in fixed161 calibration comparison. Every page is published once and
 * retained until machine reset. No payload inspection, resubmission or network
 * connection is authorized by this queue or its completion records. */
#define WIFI_EARLY5_SLOTS 32u
static struct {
 bool requested,attempted,armed,failed,frozen,control_active,stop_seen;
 bool comparison,after_calibration;
 uint64_t physical[WIFI_EARLY5_SLOTS],ring,stop_index,control_setup;
 uintptr_t pointers[WIFI_EARLY5_SLOTS],ring_pointer;
 struct xhci_trb events[WIFI_EARLY5_SLOTS],stop_event,stop_command;
 unsigned published,completed,at_command[5],at_return[5],during_mcu,during_control,idle_events,fault;
} wifi_early5;
static bool wifi_cal5_owned(void *,bool);
static bool wifi_cal5_layout(void);

static bool wifi_early5_fail(unsigned reason){
 if(!wifi_early5.fault)wifi_early5.fault=reason;
 wifi_early5.failed=wifi_cal5.failed=true;
 /* Pure state quarantine; never cancel, reset or release pending DMA here. */
 if(wifi_cal5.d)wifi_mcu_fail(wifi_cal5.d);
 return false;
}
static bool wifi_early5_layout(void){
 struct xhci_device *d=wifi_cal5.d;struct xhci_bulk_endpoint *ep=&wifi_cal5.ep;
 if(!wifi_early5.attempted || !d || !wifi_cal5.configured || !wifi_early5.ring ||
    ep->ring_phys!=wifi_early5.ring || (uintptr_t)ep->ring!=wifi_early5.ring_pointer ||
    ep->ring!=pmm_phys_to_virt(ep->ring_phys) || ep->dci!=9 || ep->packet!=512 ||
    !ep->cycle || wifi_early5.published>WIFI_EARLY5_SLOTS ||
    wifi_early5.completed>wifi_early5.published || ep->enqueue!=wifi_early5.published ||
    (wifi_early5.armed && wifi_early5.published!=WIFI_EARLY5_SLOTS))return false;
 const uint64_t used[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,
  d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,
  d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,
  xhci.command_ring_phys,xhci.event_ring_phys,ep->ring_phys};
 for(unsigned i=0;i<WIFI_EARLY5_SLOTS;i++){
  uint64_t p=wifi_early5.physical[i];
  if(!p || (p&4095) || (uintptr_t)pmm_phys_to_virt(p)!=wifi_early5.pointers[i])return false;
  for(unsigned n=0;n<sizeof used/sizeof used[0];n++)if(p==used[n])return false;
  for(unsigned n=0;n<i;n++)if(p==wifi_early5.physical[n])return false;
  const struct xhci_trb *td=&ep->ring[i];
  if(i<wifi_early5.published){
   if((td->parameter_low|((uint64_t)td->parameter_high<<32))!=p || td->status!=4096 ||
      td->control!=((XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP|1))return false;
  }else if(td->parameter_low || td->parameter_high || td->status || td->control)return false;
 }
 for(unsigned i=WIFI_EARLY5_SLOTS;i<63;i++){
  const struct xhci_trb *td=&ep->ring[i];
  if(td->parameter_low || td->parameter_high || td->status || td->control)return false;
 }
 const struct xhci_trb *link=&ep->ring[63];
 return (link->parameter_low|((uint64_t)link->parameter_high<<32))==ep->ring_phys &&
  !link->status && link->control==((XHCI_TRB_TYPE_LINK<<10)|2u|1u);
}
static bool wifi_early5_allocate(void){
 if(!wifi_early5.requested || wifi_early5.attempted || wifi_cal5.cleanup_enabled)return false;
 wifi_early5.attempted=true;wifi_early5.ring=wifi_cal5.ep.ring_phys;
 wifi_early5.ring_pointer=(uintptr_t)wifi_cal5.ep.ring;
 for(unsigned i=0;i<WIFI_EARLY5_SLOTS;i++){
  void *p;if(!alloc_frame_zero(&wifi_early5.physical[i],&p))return wifi_early5_fail(1);
  wifi_early5.pointers[i]=(uintptr_t)p;
 }
 return true;
}
static bool wifi_early5_publish(void){
 if(!wifi_early5.requested || wifi_early5.armed || wifi_early5.published ||
    !wifi_cal5_owned(&wifi_cal5,false))return false;
 uint64_t flags=event_irq_save();bool ok=true;
 for(unsigned i=0;i<WIFI_EARLY5_SLOTS;i++){
  uint64_t physical=wifi_early5.physical[i];
  struct xhci_trb td={.parameter_low=(uint32_t)physical,.parameter_high=physical>>32,
   .status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
  /* Count an uncertain publication before exposing its cycle bit. */
  wifi_early5.published++;
  uint64_t got=ring_push(wifi_cal5.ep.ring,wifi_cal5.ep.ring_phys,
   &wifi_cal5.ep.enqueue,&wifi_cal5.ep.cycle,&td);
  if(got!=wifi_early5.ring+16*i){ok=false;break;}
 }
 if(ok){wifi_early5.armed=true;db_write32(wifi_cal5.d->slot_id,9);}
 event_irq_restore(flags);
 console_printf("wifi-early5-publish: version=1 pages=%u slot=%u ring=%x before_mac=%u retained=1 reused=0\n",
  (uint64_t)wifi_early5.published,(uint64_t)wifi_cal5.d->slot_id,wifi_early5.ring,(uint64_t)!wifi_cal5.mac.started);
 return ok?true:wifi_early5_fail(2);
}
/* Memory-only admission for completions overlapping our EXACT outstanding MCU,
 * EP0 or Stop request. This never grants another I/O or forward-owner claim. */
static bool wifi_early5_event_span(struct xhci_device *d){
 if(xhci.command_pending){
  uint64_t i=wifi_early5.stop_index;
  if(!wifi_cal5.stopping || !wifi_cal5.stop_attempted || !firmware_command_exact ||
     xhci.sync_users!=1 || d->control_pending || strict_control.device ||
     d->firmware.mcu.pending || i>=63 || !xhci.command_ring)return false;
  const struct xhci_trb *a=&xhci.command_ring[i],*b=&wifi_early5.stop_command;
  return a->parameter_low==b->parameter_low && a->parameter_high==b->parameter_high &&
   a->status==b->status && a->control==b->control &&
   xhci.command_enqueue==(i==62?0:i+1);
 }
 if(d->control_pending || strict_control.device){
  if(!wifi_early5.control_active || xhci.sync_users>1 || !d->control_pending ||
     d->firmware.mcu.pending ||
     (strict_control.device!=d && (strict_control.device || !strict_control.done)) ||
     strict_control.length!=4 || !d->ep0_ring)return false;
  uint64_t p=strict_control.setup;
  if(p<d->ep0_ring_phys || p>=d->ep0_ring_phys+63*16 || (p&15))return false;
  const struct xhci_trb *t=&d->ep0_ring[(p-d->ep0_ring_phys)/16];
  return (t->parameter_low|((uint64_t)t->parameter_high<<32))==wifi_early5.control_setup &&
   ((t->control>>10)&63)==XHCI_TRB_TYPE_SETUP_STAGE && t->status==8 &&
   strict_control.data_in==((wifi_early5.control_setup&0x80)!=0);
 }
 if(d->firmware.mcu.pending){
  const struct xhci_wifi_mcu *m=&d->firmware.mcu;
  const struct xhci_wifi_mcu_bulk_receipt *r=wifi_mcu_observer.receipt;
  /* The bulk observer detaches after500ms; the loss observer may then retain
   * the same IN through1.5s. No additional publication is authorized. */
  bool original=r && wifi_mcu_observer.device==d && r->sequence==m->bulk_sequence &&
   r->published_slot==d->slot_id && r->trb==m->trb && r->dci==m->dci &&
   r->requested==m->requested && r->published_ring==(m->dci==11?m->response.ring:d->firmware.ep.ring);
  bool late=wifi_mcu_loss.device==d && wifi_mcu_loss.trace &&
   wifi_mcu_loss_pinned(d,wifi_mcu_loss.trace) && m->dci==11;
  return !xhci.sync_users && (original || late);
 }
 return xhci.sync_users==0;
}
static bool wifi_early5_event_owned(struct xhci_device *d){
 return d && d==wifi_cal5.d && wifi_cal5.active && wifi_cal5.attempted &&
  d->present && !d->removed && !d->retiring && !xhci.stopping &&
  d->vendor_id==0x7392 && d->product_id==0xa711 && d->device_bcd==0x100 && d->slot_id &&
  xhci.present && xhci.operational && xhci.controller_owned && !xhci.reclaimed && !xhci.reclaiming &&
  wifi_cal5.command_ring==xhci.command_ring_phys && wifi_cal5.output==d->output_context_phys &&
  wifi_cal5.input==d->input_context_phys && wifi_cal5.ep0==d->ep0_ring_phys &&
  wifi_cal5.descriptor==d->descriptor_buffer_phys && wifi_early5_layout() && wifi_early5_event_span(d);
}
static bool wifi_early5_event(struct xhci_device *d,const struct xhci_trb *e){
 if(!wifi_early5.attempted || !wifi_cal5.configured)return false;
 uint64_t p=e->parameter_low|((uint64_t)e->parameter_high<<32),ring=wifi_early5.ring;
 unsigned dci=(e->control>>16)&31,slot=e->control>>24;
 bool retained=p>=ring && p<ring+4096;
 if(!retained && !(d==wifi_cal5.d && dci==9))return false;
 const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 if(!wifi_early5.armed || wifi_early5.failed || wifi_early5.frozen ||
    !wifi_early5_event_owned(d) || slot!=d->slot_id || dci!=9 ||
    ((e->control>>10)&63)!=32 || (e->control&~allowed)){
  wifi_early5_fail(3);return true;
 }
 unsigned n=wifi_early5.completed,cc=e->status>>24,residue=e->status&0xffffff;
 if(p!=ring+16*n){wifi_early5_fail(4);return true;}
 if(cc>=26 && cc<=28){
  bool pending=n<wifi_early5.published;
  if(!wifi_cal5.stopping || !wifi_cal5.stop_attempted || !xhci.command_pending ||
     !firmware_command_exact || wifi_early5.stop_seen ||
     (pending?((cc==27 && residue) || (cc!=27 && residue>4096)):(cc!=27 || residue))){
   wifi_early5_fail(5);return true;
  }
  wifi_early5.stop_seen=true;wifi_early5.stop_event=*e;return true;
 }
 if(wifi_early5.stop_seen || n>=wifi_early5.published ||
    (cc!=1 && cc!=13) || residue>4096){wifi_early5_fail(6);return true;}
 wifi_early5.events[n]=*e;wifi_early5.completed++;
 if(d->firmware.mcu.pending)wifi_early5.during_mcu++;
 else if(d->control_pending || strict_control.device)wifi_early5.during_control++;
 else wifi_early5.idle_events++;
 return true;
}
static bool wifi_early5_stop(void){
 if(!wifi_cal5_owned(&wifi_cal5,true) || wifi_cal5.stop_attempted || !wifi_cal5.cal.complete ||
    !wifi_cal5.mac.complete || !wifi_cal5.mac.mac_disabled || !wifi_cal5.mac.rx_drained ||
    !wifi_cal5.mac.tx_drained || !wifi_early5.armed || wifi_early5.failed)return false;
 struct xhci_device *d=wifi_cal5.d;
 wifi_cal5.stop_attempted=wifi_cal5.stopping=true;
 struct xhci_trb cmd={.control=(15u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 wifi_early5.stop_index=xhci.command_enqueue;wifi_early5.stop_command=cmd;
 wifi_early5.stop_command.control|=xhci.command_cycle?1u:0u;
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;
 bool ok=submit_command_trb(&cmd,15,&slot) && slot==d->slot_id;
 firmware_command_exact=false;wifi_cal5.stopping=false;
 if(ok){
  __atomic_thread_fence(__ATOMIC_ACQUIRE);
  wifi_cal5.stop_state=context_read32(d->output_context,9,0)&7;
  wifi_cal5.stop_dequeue=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);
  ok=!wifi_early5.failed && wifi_early5.stop_seen && wifi_cal5.stop_state==3 &&
   wifi_cal5.stop_dequeue==(wifi_early5.ring+16*wifi_early5.completed+1);
 }
 if(ok){wifi_cal5.stopped=wifi_early5.frozen=true;}
 else firmware_command_failed=true;
 event_irq_restore(flags);
 const struct xhci_trb *e=&wifi_early5.stop_event;
 console_printf("wifi-early5-stop: version=1 ok=%u state=%u dequeue=%x ring=%x completed=%u posted=%u event_seen=%u event_parameter=%x event_status=%x event_control=%x retained=1\n",
  (uint64_t)ok,(uint64_t)wifi_cal5.stop_state,wifi_cal5.stop_dequeue,wifi_early5.ring,
  (uint64_t)wifi_early5.completed,(uint64_t)wifi_early5.published,(uint64_t)wifi_early5.stop_seen,
  e->parameter_low|((uint64_t)e->parameter_high<<32),(uint64_t)e->status,(uint64_t)e->control);
 return ok?true:wifi_early5_fail(7);
}
static bool wifi_early5_emit(bool ok){
 bool complete=ok && wifi_early5.frozen && !wifi_early5.failed &&
  wifi_cal5.cal.complete && wifi_cal5.mac.complete && wifi_cal5.stopped;
 console_printf("wifi-early5: version=1 complete=%u posted=%u completed=%u exhausted=%u during_mcu=%u during_control=%u idle_events=%u fault=%u calibration=%u commands=%u operations=%u mac=%u stopped=%u frozen=%u retained=1 reused=0 inspected=0 network_out=0\n",
  (uint64_t)complete,(uint64_t)wifi_early5.published,(uint64_t)wifi_early5.completed,
  (uint64_t)(wifi_early5.completed==WIFI_EARLY5_SLOTS),(uint64_t)wifi_early5.during_mcu,
  (uint64_t)wifi_early5.during_control,(uint64_t)wifi_early5.idle_events,(uint64_t)wifi_early5.fault,
  (uint64_t)wifi_cal5.cal.complete,(uint64_t)wifi_cal5.commands,(uint64_t)wifi_cal5.operations,(uint64_t)wifi_cal5.mac.complete,
  (uint64_t)wifi_cal5.stopped,(uint64_t)wifi_early5.frozen);
 for(unsigned i=0;i<5;i++)console_printf("wifi-early5-boundary: version=1 step=%u completed_before=%u completed_after=%u\n",
  (uint64_t)i,(uint64_t)wifi_early5.at_command[i],(uint64_t)wifi_early5.at_return[i]);
 return complete;
}
