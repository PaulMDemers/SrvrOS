#include <srvros/mt76_discovery5.h>
/* Explicit fixed161/20MHz owner. Pages/ring survive every outcome until reset. */
static struct {
 struct xhci_device *d;struct xhci_bulk_endpoint ep;
 struct mt76_calibration_probe probe;struct mt76_calibration cal;
 struct mt76_mac_cycle mac;struct mt76_rx_window window;
 struct mt76_rx_window_plan plan;
 struct mt76_discovery discovery;struct mt76_discovery5_choice choice;
 uint64_t command_ring,output,input,ep0,descriptor,stop_dequeue,pin_ring,pin_pages[8];
 uintptr_t pin_rptr,pin_v[8];
 unsigned operations,commands,posts,events,completion,remaining,late,stop_state;
 bool attempted,active,configured,pending,done,delivered,stopping,stop_attempted,stopped;
 bool calibrating,probing,receiving,failed;
 uint64_t cleanup_physical[8];uint8_t *cleanup_buffer[8];uintptr_t cleanup_pin[8];
 uint8_t cleanup_snapshot[8][4096];struct xhci_trb cleanup_events[8],cleanup_stop_event,cleanup_fault_event,cleanup_stop_command;
 uint64_t cleanup_stop_index,cleanup_control_setup;bool cleanup_control_active;
 unsigned cleanup_posts,cleanup_completed,cleanup_stop_seen;
 bool cleanup_enabled,cleanup_started,cleanup_pending,cleanup_stop_pending,cleanup_failed,cleanup_frozen,cleanup_needed,cleanup_fault_valid;
} wifi_cal5;
static bool wifi_cal5_cleanup_alias(uint64_t physical){
 if(!physical)return false;
 for(unsigned i=0;i<8;i++)if(wifi_cal5.cleanup_physical[i]==physical)return true;
 return false;
}
static bool wifi_cal5_cleanup_held(void){
 if(!wifi_cal5.cleanup_enabled)return true;
 if(!wifi_cal5.stopped || !wifi_cal5.cleanup_frozen || wifi_cal5.cleanup_pending || wifi_cal5.cleanup_failed || wifi_cal5.cleanup_posts>8 || wifi_cal5.cleanup_completed>wifi_cal5.cleanup_posts)return false;
 for(unsigned i=0;i<8;i++){
  if(!wifi_cal5.cleanup_physical[i] || (uintptr_t)wifi_cal5.cleanup_buffer[i]!=wifi_cal5.cleanup_pin[i] || wifi_cal5.cleanup_buffer[i]!=pmm_phys_to_virt(wifi_cal5.cleanup_physical[i]))return false;
  for(unsigned j=0;j<4096;j++)if(wifi_cal5.cleanup_buffer[i][j]!=wifi_cal5.cleanup_snapshot[i][j])return false;
 }
 return true;
}
bool xhci_wifi_cal5_cleanup_enable(void){
 if(wifi_cal5.cleanup_enabled || wifi_cal5.attempted || wifi_cal5.active || wifi_cal5.configured)return false;
 wifi_cal5.cleanup_enabled=true;return true;
}
static bool wifi_cal5_cleanup_service(void);

static bool wifi_cal5_layout(void){
 struct xhci_device *d=wifi_cal5.d;struct mt76_rx_window_plan *p=&wifi_cal5.plan;
 if(!d || !wifi_cal5.configured || !mt76_rx5_window_plan_valid(p) || p->ring!=wifi_cal5.ep.ring_phys || p->ring!=wifi_cal5.pin_ring || (uintptr_t)wifi_cal5.ep.ring!=wifi_cal5.pin_rptr ||
    wifi_cal5.ep.ring!=pmm_phys_to_virt(p->ring) || wifi_cal5.ep.dci!=9 || wifi_cal5.ep.packet!=512 ||
    !wifi_cal5.ep.cycle || wifi_cal5.posts>8 || wifi_cal5.cleanup_posts>8 || (!wifi_cal5.cleanup_enabled && wifi_cal5.cleanup_posts) || wifi_cal5.ep.enqueue!=wifi_cal5.posts+wifi_cal5.cleanup_posts ||
    p->slot!=d->slot_id || p->first_index || p->budget_us!=10000000)return false;
 const uint64_t used[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,
 d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned n=0;n<9;n++){
  uint64_t phys=n?p->physical[n-1]:p->ring;
  if(n && (p->buffer[n-1]!=pmm_phys_to_virt(phys) || phys!=wifi_cal5.pin_pages[n-1] || (uintptr_t)p->buffer[n-1]!=wifi_cal5.pin_v[n-1]))return false;
  for(unsigned i=0;i<sizeof used/sizeof used[0];i++)if(phys==used[i])return false;
 }
 if(wifi_cal5.cleanup_enabled){
  for(unsigned i=0;i<8;i++){
   uint64_t phys=wifi_cal5.cleanup_physical[i];
   if(!phys || (phys&4095) || wifi_cal5.cleanup_buffer[i]!=pmm_phys_to_virt(phys) || (uintptr_t)wifi_cal5.cleanup_buffer[i]!=wifi_cal5.cleanup_pin[i] || phys==p->ring)return false;
   for(unsigned j=0;j<9;j++)if(phys==used[j])return false;
   for(unsigned j=0;j<8;j++)if(phys==p->physical[j] || (j<i && phys==wifi_cal5.cleanup_physical[j]))return false;
   const struct xhci_trb *td=&wifi_cal5.ep.ring[8+i];
   if(i<wifi_cal5.cleanup_posts){if((td->parameter_low|((uint64_t)td->parameter_high<<32))!=phys || td->status!=4096 || td->control!=((XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP|1))return false;}
   else if(td->control&1)return false;
  }
 }
 for(unsigned i=0;i<8;i++){
  const struct xhci_trb *td=&wifi_cal5.ep.ring[i];
  if(i<wifi_cal5.posts){if((td->parameter_low|((uint64_t)td->parameter_high<<32))!=p->physical[i] || td->status!=4096 || td->control!=((XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP|1))return false;}
  else if(td->control&1)return false;
 }
 __asm__ volatile("" ::: "memory");
 unsigned state=context_read32(d->output_context,9,0)&7;
 uint64_t deq=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32),ptr=deq&~15ull;
 if((deq&15)!=1 || ptr<p->ring || ptr>p->ring+16*(wifi_cal5.posts+wifi_cal5.cleanup_posts) || (wifi_cal5.stopped?state!=3:(state!=1 && state!=2 && state!=3)))return false;
 return true;
}
/* Same-lifetime5GHz fields, not the legacy G-band mcu.trim_fields object.
 * Scalar validity alone does not admit I/O; full selected private parent and
 * controller/endpoint ownership are still required by owned(). */
static bool wifi_cal5_fields(struct xhci_device *d){
 const struct mt76_trim_fields *f=&wifi_trim5.fields;const struct mt76_efuse_map_gate *g=&wifi_trim5.gate;
 return d && d==wifi_trim5.d && mt76_ant_fields_valid(&d->firmware.mcu.ant_fields) && mt76_trim_fields_valid(f) &&
 f->nic0==d->firmware.mcu.ant_fields.nic0 && f->nic1==d->firmware.mcu.ant_fields.nic1 &&
 g->phase==15 && g->block==2 && !g->failed && !g->pending && g->polls==f->polls && g->submitted==f->reads+f->writes;
}
/* Event consumption is pure memory/state validation. A completion may arrive
 * inside our already-published EP0 request or exact Stop command;that does not
 * license nested register I/O or new DMA publication from this event callback. */
static bool wifi_cal5_event_span(struct xhci_device *d){
 if(!d)return false;
 if(xhci.command_pending){
  uint64_t i=wifi_cal5.cleanup_stop_index;const struct xhci_trb *want=&wifi_cal5.cleanup_stop_command;
  if(!wifi_cal5.stopping || !wifi_cal5.stop_attempted || !firmware_command_exact || xhci.sync_users!=1 || d->control_pending || strict_control.device || i>=63 || !xhci.command_ring)return false;
  const struct xhci_trb *have=&xhci.command_ring[i];
  return have->parameter_low==want->parameter_low && have->parameter_high==want->parameter_high && have->status==want->status && have->control==want->control &&
   ((want->control>>10)&63)==15 && ((want->control>>16)&31)==9 && (want->control>>24)==d->slot_id && xhci.command_enqueue==(i==62?0:i+1);
 }
 if(d->control_pending || strict_control.device){
  if(!wifi_cal5.cleanup_control_active || xhci.sync_users>1 || !d->control_pending || (strict_control.device!=d && (strict_control.device || !strict_control.done)) || strict_control.length!=4 || !d->ep0_ring)return false;
  uint64_t p=strict_control.setup;if(p<d->ep0_ring_phys || p>=d->ep0_ring_phys+63*16 || (p&15))return false;
  const struct xhci_trb *t=&d->ep0_ring[(p-d->ep0_ring_phys)/16];
  uint64_t setup=t->parameter_low|((uint64_t)t->parameter_high<<32);
  return setup==wifi_cal5.cleanup_control_setup && ((t->control>>10)&63)==XHCI_TRB_TYPE_SETUP_STAGE && t->status==8 && strict_control.data_in==((setup&0x80)!=0);
 }
 return xhci.sync_users==0;
}
static bool wifi_cal5_owner(void *context,bool cleanup,bool event){
 struct xhci_device *d=wifi_cal5.d;
 if(context!=&wifi_cal5 || !wifi_cal5_requested || !wifi_cal5.active || !wifi_cal5.attempted || !d ||
    !d->present || d->removed || d->retiring || xhci.stopping || xhci.command_failed || d->control_failed || !d->slot_id || d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 ||
    d->ep0_quarantined || !d->ep0_ring || firmware_command_failed ||
    wifi_cal5.command_ring!=xhci.command_ring_phys || wifi_cal5.output!=d->output_context_phys ||
    wifi_cal5.input!=d->input_context_phys || wifi_cal5.ep0!=d->ep0_ring_phys || wifi_cal5.descriptor!=d->descriptor_buffer_phys ||
    !d->firmware.mcu.configured || d->firmware.mcu.pending || !wifi_cal5_layout())return false;
 if(event?!wifi_cal5_event_span(d):(xhci.command_pending || d->control_pending || strict_control.device))return false;
 if(cleanup)return true;
 if((context_read32(d->output_context,9,0)&7)!=1)return false;
 return !wifi_cal5.failed &&
#ifdef SRVROS_XHCI_CAL5_DRAIN_TEST
 cal5_drain_test_parent();
#else
 wifi_channel_vco5_owned(&wifi_channel_vco5) &&
    mt76_channel_vco5_selected(&wifi_channel_vco5.state) && mt76_cvco5_valid(&wifi_channel_vco5.state.vco) &&
    wifi_channel_vco5.submitted==2636 && wifi_trim5.fields.trim==0x44 && wifi_bbp5.gain.lna==12 &&
    wifi_cal5_fields(d);
#endif
}
static bool wifi_cal5_owned(void *context,bool cleanup){return wifi_cal5_owner(context,cleanup,false);}
/* Teardown may proceed after a fault;payload inspection may not. Recheck
 * late events and immutable cleanup storage before EACH retained capture item. */
static bool wifi_cal5_finish_owned(void *context,bool cleanup){
 if(!wifi_cal5_owned(context,cleanup))return false;
 return !wifi_cal5.cleanup_enabled || (!wifi_cal5.failed && !wifi_cal5.cleanup_failed && wifi_cal5_cleanup_held());
}
static bool wifi_cal5_usb(bool cleanup,bool write,uint32_t a,uint32_t *v){
 if(!v || !wifi_cal5_owned(&wifi_cal5,cleanup))return false;
 switch(a){case 0x1004:case 0x2000:case 0x208:case 0x1400:case 0x1340:case 0x13b0:case 0x2124:case 0x438:case 0xa30:case 0xa34:case 0x1200:case 0x430:break;default:return false;}
 unsigned op=wifi_cal5.operations++;
 wifi_cal5.cleanup_control_setup=(uint64_t)(write?0x40:0xc0)|((uint64_t)(write?6:7)<<8)|((uint64_t)a<<32)|(4ull<<48);
 wifi_cal5.cleanup_control_active=wifi_cal5.cleanup_enabled && wifi_cal5.cleanup_started && wifi_cal5.mac.mac_disabled;
 bool ok=
#ifdef SRVROS_XHCI_CAL5_DRAIN_TEST
 cal5_drain_test_usb(wifi_cal5.d,write,a,v);
#else
 wifi_mac_private_usb(wifi_cal5.d,write,a,v);
#endif
 wifi_cal5.cleanup_control_active=false;
 console_printf("wifi-cal5-usb: op=%u part=%u cleanup=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)(wifi_cal5.probing?1:wifi_cal5.calibrating?2:0),(uint64_t)cleanup,(uint64_t)write,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
 if(!ok)wifi_cal5.failed=true;
 return ok;
}
static bool wifi_cal5_read(void *c,uint32_t a,uint32_t *v,bool clean){
 if(c!=&wifi_cal5 || !v)return false;
 *v=0;
 bool ok=(!clean || wifi_cal5_cleanup_service()) && wifi_cal5_usb(clean,false,a,v);
 if(ok && clean && wifi_cal5.cleanup_enabled && wifi_cal5.mac.mac_disabled && ((a==0x430 && (*v&0xff0000)) || ((a==0xa30 || a==0xa34) && *v)))wifi_cal5.cleanup_needed=true;
 return ok;
}
static bool wifi_cal5_write(void *c,uint32_t a,uint32_t v,bool clean){return c==&wifi_cal5 && (a==0x1004 || a==0x1400 || a==0x1340) && wifi_cal5_usb(clean,true,a,&v);}
static bool wifi_cal5_delay(void *c,unsigned us,bool clean){
 if((clean && !wifi_cal5_cleanup_service()) || !wifi_cal5_owned(c,clean) || us>10000000)return false;
 unsigned op=wifi_cal5.operations++;uint64_t start=timer_monotonic_us();bool ok=
#ifdef SRVROS_XHCI_CAL5_DRAIN_TEST
 cal5_drain_test_delay(us);
#else
 timer_delay_us(us);
#endif
 uint64_t end=timer_monotonic_us();ok=ok && end>=start && end-start>=us;
 console_printf("wifi-cal5-delay: op=%u part=%u cleanup=%u us=%u elapsed_us=%u ok=%u\n",(uint64_t)op,(uint64_t)(wifi_cal5.probing?1:wifi_cal5.calibrating?2:0),(uint64_t)clean,(uint64_t)us,end>=start?end-start:0,(uint64_t)ok);
 if(!ok)wifi_cal5.failed=true;
 return ok;
}
static uint64_t wifi_cal5_now(void *c){(void)c;return timer_monotonic_us();}
static bool wifi_cal5_cal_owned(void *c){return (wifi_cal5.probing || wifi_cal5.calibrating) && wifi_cal5.mac.started && wifi_cal5.mac.snapshot_complete && wifi_cal5_owned(c,false);}
static bool wifi_cal5_cal_read(void *c,uint32_t a,uint32_t *v){return wifi_cal5_cal_owned(c) && (a==0x1004 || a==0x2000 || a==0x208 || a==0x13b0 || a==0x2124) && wifi_cal5_read(c,a,v,false);}
static bool wifi_cal5_cal_write(void *c,uint32_t a,uint32_t v){return wifi_cal5_cal_owned(c) && ((a==0x13b0 && !v) || (a==0x2124 && v==0xffffff7e)) && wifi_cal5_usb(false,true,a,&v);}
static bool wifi_cal5_restore(void *c,uint32_t a,uint32_t saved,uint32_t *seen){
 struct mt76_calibration *s=&wifi_cal5.cal;struct mt76_calibration_probe *p=&wifi_cal5.probe;unsigned i=a==0x13b0?0:1,bit=1u<<i;
 if(c!=&wifi_cal5 || !seen || (a!=0x13b0 && a!=0x2124) ||
    !(wifi_cal5.probing?(p->saved_mask&bit)&&(p->modified_mask&bit)&&saved==p->saved[i]:(s->saved_mask&bit)&&(s->modified_mask&bit)&&saved==s->saved[i]))return false;
 bool wr=wifi_cal5_usb(true,true,a,&saved);*seen=0;bool rd=wifi_cal5_usb(true,false,a,seen);return wr && rd;
}
static bool wifi_cal5_cal_delay(void *c,unsigned us){return (us==10 || us==500 || us==15000) && wifi_cal5_cal_owned(c) && wifi_cal5_delay(c,us,false);}
static bool wifi_cal5_command(void *c,enum mt76_poweron_command cmd,uint32_t arg){
 const uint32_t args[5]={0,161,0x901,1,1};unsigned n=wifi_cal5.commands;
 if(n>=5 || (unsigned)cmd!=n || arg!=args[n] || !wifi_cal5_cal_owned(c) || !wifi_cal5.calibrating)return false;
 struct xhci_device *d=wifi_cal5.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;uint8_t packet[16];
 if(mt76_poweron5_packet(n,packet,16)!=16)return false;
 unsigned op=wifi_cal5.operations++;wifi_cal5.commands++;
 for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
 uint64_t start=timer_monotonic_us();int sent=wifi_mcu_bulk(d,false,16),received=-1;
 if(sent==16 && wifi_cal5_cal_owned(c))received=wifi_mcu_calibration_receive(d,n);
 uint64_t end=timer_monotonic_us();uint32_t words[4]={0};if(received==16)for(unsigned i=0;i<16;i++)words[i/4]|=(uint32_t)m->buffer[i]<<(8*(i%4));
 bool ok=sent==16 && received==16 && (m->completion==1 || m->completion==13) && m->remaining==MT76_MCU_RESPONSE_MAX-16 && end>=start && end-start<=1000000 && mt76_poweron5_reply(n,m->buffer,16) && wifi_cal5_cal_owned(c);
 console_printf("wifi-cal5-command: op=%u step=%u arg=%x sent=%d received=%d cc=%u residue=%u elapsed_us=%u h=%x id=%x value=%x t=%x ok=%u\n",(uint64_t)op,(uint64_t)n,(uint64_t)arg,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,end>=start?end-start:0,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3],(uint64_t)ok);
 if(!ok) {
  const struct xhci_wifi_mcu_bulk_receipt *r=m->bulk_failure_valid?&m->bulk_failure:&m->last_bulk;
  console_printf("wifi-mcu-bulk-observation: version=1 cal_step=%u first_failure=%u sequence=%u reason=%u dci=%u requested=%u flags=%u cc=%u residue=%u elapsed_us=%u polls=%u dispatched=%u event_start=%u event_start_cycle=%u event_end=%u event_end_cycle=%u trb=%x ring=%x endpoint_valid=%u endpoint_state=%u endpoint_dequeue=%x\n",
   (uint64_t)n,(uint64_t)m->bulk_failure_valid,r->sequence,(uint64_t)r->reason,(uint64_t)r->dci,(uint64_t)r->requested,(uint64_t)r->flags,(uint64_t)r->completion,(uint64_t)r->remaining,r->elapsed_us,(uint64_t)r->polls,(uint64_t)r->dispatched,(uint64_t)r->event_start,(uint64_t)r->event_start_cycle,(uint64_t)r->event_end,(uint64_t)r->event_end_cycle,r->trb,r->ring,(uint64_t)r->endpoint_valid,(uint64_t)r->endpoint_state,r->endpoint_dequeue);
  wifi_mcu_bulk_details(r);
 }
 if(!ok)wifi_cal5.failed=true;
 return ok;
}
static bool wifi_cal5_cleanup_fail(void){wifi_cal5.cleanup_failed=wifi_cal5.failed=true;return false;}
static bool wifi_cal5_cleanup_event(struct xhci_device *d,const struct xhci_trb *e){
 if(!wifi_cal5.cleanup_enabled || !wifi_cal5.configured)return false;
 uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32),ring=wifi_cal5.plan.ring;
 unsigned dci=(e->control>>16)&31,slot=e->control>>24;
 bool retained=ptr>=ring && ptr<ring+4096;
 if(wifi_cal5.stopped){
  if(!retained)return false;
  if(!wifi_cal5.cleanup_fault_valid){wifi_cal5.cleanup_fault_event=*e;wifi_cal5.cleanup_fault_valid=true;}
  wifi_cal5_cleanup_fail();return true;
 }
 if(!wifi_cal5.cleanup_started){
  if(ptr<ring+8*16 || ptr>=ring+4096)return false;
  /* Retain one exact empty-ring Stop notification while our published Stop
   * is in flight. Output context cessation is proved only AFTER that command
   * completes; this event is neither an unpublished payload nor a packet. */
  if(wifi_cal5.stopping && wifi_cal5.stop_attempted && xhci.command_pending && firmware_command_exact &&
   !wifi_cal5.cleanup_stop_seen && d==wifi_cal5.d && wifi_cal5_owner(&wifi_cal5,true,true) && slot==d->slot_id && dci==9 &&
   wifi_cal5.posts==8 && !wifi_cal5.pending && wifi_cal5.window.polled && wifi_cal5.window.poll_ok &&
   wifi_cal5.window.attempts==8 && wifi_cal5.window.completions==8 && e->status==(27u<<24) &&
   ((e->control>>10)&63)==32 && !(e->control&~(1u|(63u<<10)|(31u<<16)|(255u<<24))) && ptr==ring+8*16){
   wifi_cal5.cleanup_stop_seen=1;wifi_cal5.cleanup_stop_event=*e;return true;
  }
  if(!wifi_cal5.cleanup_fault_valid){wifi_cal5.cleanup_fault_event=*e;wifi_cal5.cleanup_fault_valid=true;}
  wifi_cal5_cleanup_fail();return true;
 }
 if(!(ptr>=ring && ptr<ring+4096) && !(d==wifi_cal5.d && dci==9))return false;
 const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 bool good=d==wifi_cal5.d && slot==wifi_cal5.d->slot_id && dci==9 && ((e->control>>10)&63)==32 && !(e->control&~allowed) && wifi_cal5_owner(&wifi_cal5,true,true);
 unsigned cc=e->status>>24,res=e->status&0xffffff,n=wifi_cal5.cleanup_posts;
 uint64_t expected=ring+16*(8+n-(wifi_cal5.cleanup_pending?1:0));
 if(good && cc>=26 && cc<=28){
  good=wifi_cal5.stopping && firmware_command_exact && !wifi_cal5.cleanup_stop_seen && ptr==expected &&
   ((cc==27 && !res && !wifi_cal5.cleanup_pending) || (cc!=27 && res<=4096 && wifi_cal5.cleanup_pending));
  if(good){wifi_cal5.cleanup_stop_seen=1;wifi_cal5.cleanup_stop_pending=wifi_cal5.cleanup_pending;wifi_cal5.cleanup_stop_event=*e;wifi_cal5.cleanup_pending=false;}
 }else if(good){
  good=wifi_cal5.cleanup_pending && n && wifi_cal5.cleanup_completed==n-1 && ptr==expected && (cc==1 || cc==13) && res<=4096 &&
   ((context_read32(d->output_context,9,0)&7)==1 || (wifi_cal5.stopping && (context_read32(d->output_context,9,0)&7)==3));
  if(good){wifi_cal5.cleanup_events[n-1]=*e;wifi_cal5.cleanup_completed++;wifi_cal5.cleanup_pending=false;}
 }
 if(!good){if(!wifi_cal5.cleanup_fault_valid){wifi_cal5.cleanup_fault_event=*e;wifi_cal5.cleanup_fault_valid=true;}wifi_cal5_cleanup_fail();}
 return true;
}
static bool wifi_cal5_cleanup_service(void){
 if(!wifi_cal5.cleanup_enabled || !wifi_cal5.mac.mac_disabled || wifi_cal5.stopped)return true;
 if(wifi_cal5.cleanup_failed || !wifi_cal5_owned(&wifi_cal5,true))return wifi_cal5_cleanup_fail();
 uint64_t flags=event_irq_save();struct xhci_trb event;bool good=true;
#ifdef SRVROS_XHCI_CAL5_DRAIN_TEST
 cal5_drain_test_progress();
#endif
 for(unsigned i=0;i<32 && poll_event(&event);i++){
  unsigned type=(event.control>>10)&63;if(type!=32 && type!=33){good=false;break;}
  dispatch_transfer_event(&event);if(wifi_cal5.cleanup_failed || wifi_cal5.failed){good=false;break;}
 }
 if(good && wifi_cal5.cleanup_needed && !wifi_cal5.pending && !wifi_cal5.cleanup_pending && wifi_cal5.cleanup_posts<8){
  good=wifi_cal5.posts==8 && wifi_cal5.window.polled && wifi_cal5.window.poll_ok && wifi_cal5.done && wifi_cal5.events==1 && (wifi_cal5.completion==1 || wifi_cal5.completion==13);
  if(good){unsigned i=wifi_cal5.cleanup_posts;uint64_t physical=wifi_cal5.cleanup_physical[i];
   wifi_cal5.cleanup_started=true;wifi_cal5.cleanup_pending=true;wifi_cal5.cleanup_posts++;
   struct xhci_trb td={.parameter_low=(uint32_t)physical,.parameter_high=physical>>32,.status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
   uint64_t got=ring_push(wifi_cal5.ep.ring,wifi_cal5.ep.ring_phys,&wifi_cal5.ep.enqueue,&wifi_cal5.ep.cycle,&td);
   good=got==wifi_cal5.plan.ring+16*(8+i);if(good)db_write32(wifi_cal5.d->slot_id,9);
  }
 }
 good=good && wifi_cal5_owned(&wifi_cal5,true);event_irq_restore(flags);
 return good?true:wifi_cal5_cleanup_fail();
}
static void wifi_cal5_cleanup_emit(void){
 if(!wifi_cal5.cleanup_enabled)return;
 uint64_t codes=0,residues[2]={0};
 for(unsigned i=0;i<wifi_cal5.cleanup_completed && i<8;i++){const struct xhci_trb *e=&wifi_cal5.cleanup_events[i];codes|=(uint64_t)(e->status>>24)<<(8*i);residues[i/4]|=(uint64_t)(e->status&0xffffff)<<(16*(i%4));}
 if(wifi_cal5.cleanup_fault_valid){const struct xhci_trb *f=&wifi_cal5.cleanup_fault_event;console_printf("wifi-cal5-cleanup-fault: p=%x s=%u c=%u\n",f->parameter_low|((uint64_t)f->parameter_high<<32),(uint64_t)f->status,(uint64_t)f->control);}
 const struct xhci_trb *e=&wifi_cal5.cleanup_stop_event;uint64_t pointer=e->parameter_low|((uint64_t)e->parameter_high<<32);
 console_printf("wifi-cal5-cleanup: posted=%u completed=%u codes=%x residues0=%x residues1=%x stop_p=%x stop_s=%u stop_c=%u stop_q=%x held=%u failed=%u\n",(uint64_t)wifi_cal5.cleanup_posts,(uint64_t)wifi_cal5.cleanup_completed,codes,residues[0],residues[1],pointer,(uint64_t)e->status,(uint64_t)e->control,wifi_cal5.cleanup_stop_seen?wifi_cal5.stop_dequeue:0,(uint64_t)wifi_cal5_cleanup_held(),(uint64_t)wifi_cal5.cleanup_failed);
}
static bool wifi_cal5_event(struct xhci_device *d,const struct xhci_trb *e){
 if(wifi_cal5_cleanup_event(d,e))return true;
 if(d!=wifi_cal5.d || !wifi_cal5.configured || !wifi_cal5.posts || (e->control>>24)!=d->slot_id || ((e->control>>16)&31)!=9)return false;
 uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32),want=wifi_cal5.plan.ring+16*(wifi_cal5.posts-1);
 if(ptr!=want)return false;
 if(!wifi_cal5_layout()){wifi_cal5.failed=true;return true;}
 if(wifi_cal5.stopped || !wifi_cal5.pending){wifi_cal5.late++;return true;}
 wifi_cal5.pending=false;wifi_cal5.done=true;wifi_cal5.events++;wifi_cal5.completion=e->status>>24;wifi_cal5.remaining=e->status&0xffffff;
 bool normal=(wifi_cal5.completion==1 || wifi_cal5.completion==13) && wifi_cal5.remaining<=4096;
 bool stopped=wifi_cal5.stopping && wifi_cal5.completion>=26 && wifi_cal5.completion<=28;
 if((e->control&4) || (!normal && !stopped))wifi_cal5.failed=true;
 return true;
}
static bool wifi_cal5_post(void *c,unsigned i,uint64_t trb,uint64_t physical){
 if(!wifi_cal5.receiving || !wifi_cal5_owned(c,false) || i>=8 || i!=wifi_cal5.posts || wifi_cal5.pending ||
    physical!=wifi_cal5.plan.physical[i] || trb!=wifi_cal5.plan.ring+16*i)return false;
 uint64_t flags=event_irq_save();wifi_cal5.posts++;wifi_cal5.pending=true;wifi_cal5.done=wifi_cal5.delivered=false;wifi_cal5.events=0;
 struct xhci_trb td={.parameter_low=(uint32_t)physical,.parameter_high=physical>>32,.status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
 uint64_t got=ring_push(wifi_cal5.ep.ring,wifi_cal5.ep.ring_phys,&wifi_cal5.ep.enqueue,&wifi_cal5.ep.cycle,&td);
 bool ok=got==trb;if(ok)db_write32(wifi_cal5.d->slot_id,9);event_irq_restore(flags);
 console_printf("wifi-cal5-post: index=%u ok=%u bytes=4096 retained=1 reused=0\n",(uint64_t)i,(uint64_t)ok);return ok;
}
static bool wifi_cal5_poll(void *c,struct mt76_rx_window_event *out,bool *present){
 if(!out || !present || !wifi_cal5.receiving || !wifi_cal5_owned(c,false))return false;
 uint64_t flags=event_irq_save();struct xhci_trb e;if(poll_event(&e))dispatch_transfer_event(&e);
 *present=wifi_cal5.done && !wifi_cal5.delivered;
 if(*present){if(wifi_cal5.events!=1 || wifi_cal5.failed){event_irq_restore(flags);return false;}
 *out=(struct mt76_rx_window_event){.trb=wifi_cal5.plan.ring+16*(wifi_cal5.posts-1),.slot=wifi_cal5.d->slot_id,.dci=9,.completion=wifi_cal5.completion,.remaining=wifi_cal5.remaining};wifi_cal5.delivered=true;}
 event_irq_restore(flags);return !wifi_cal5.failed;
}
static bool wifi_cal5_inspection(void *c,uint32_t tx,uint32_t ibi){
 if(tx!=0x2f2f1b1b || ibi!=UINT32_MAX || !wifi_cal5_owned(c,false))return false;
 wifi_cal5.probing=true;const struct mt76_calibration_io probe={c,wifi_cal5_cal_owned,wifi_cal5_cal_read,wifi_cal5_cal_write,NULL,wifi_cal5_cal_delay,wifi_cal5_restore};
 enum mt76_calibration_status p=mt76_calibration_probe_run(&wifi_cal5.probe,&probe);wifi_cal5.probing=false;
 bool good=p==MT76_CAL_OK && wifi_cal5.probe.complete && wifi_cal5.probe.ibi_write_matched && wifi_cal5.probe.ibi_changed && wifi_cal5.probe.raw_restore_mask==3;
 console_printf("wifi-cal5-probe: status=%u complete=%u matched=%u changed=%u restored=%u alc=%x ibi=%x observed=%x ok=%u\n",(uint64_t)p,(uint64_t)wifi_cal5.probe.complete,(uint64_t)wifi_cal5.probe.ibi_write_matched,(uint64_t)wifi_cal5.probe.ibi_changed,(uint64_t)wifi_cal5.probe.raw_restore_mask,(uint64_t)wifi_cal5.probe.saved[0],(uint64_t)wifi_cal5.probe.saved[1],(uint64_t)wifi_cal5.probe.observed_ibi,(uint64_t)good);
 if(!good)return false;
 wifi_cal5.calibrating=true;const struct mt76_poweron_io cal={c,wifi_cal5_cal_owned,wifi_cal5_cal_read,wifi_cal5_cal_write,wifi_cal5_command,wifi_cal5_cal_delay,wifi_cal5_restore};
 enum mt76_calibration_status status=mt76_poweron5_calibration_run(&wifi_cal5.cal,&cal,&wifi_cal5.d->wifi_identity,&wifi_cal5.d->firmware.mcu.ant_fields,161,20);wifi_cal5.calibrating=false;
 good=status==MT76_CAL_OK && wifi_cal5.cal.complete && wifi_cal5.cal.cleanup_complete && wifi_cal5.cal.restored_mask==3 && wifi_cal5.commands==5;
 console_printf("wifi-cal5-calibration: status=%u complete=%u restored=%u commands=%u alc=%x ibi=%x alc_restored=%x ibi_restored=%x ok=%u\n",(uint64_t)status,(uint64_t)wifi_cal5.cal.complete,(uint64_t)wifi_cal5.cal.restored_mask,(uint64_t)wifi_cal5.commands,(uint64_t)wifi_cal5.cal.saved[0],(uint64_t)wifi_cal5.cal.saved[1],(uint64_t)wifi_cal5.cal.restored[0],(uint64_t)wifi_cal5.cal.restored[1],(uint64_t)good);
 if(!good)return false;
 wifi_cal5.receiving=true;const struct mt76_rx_window_io window={c,wifi_cal5_owned,wifi_cal5_post,wifi_cal5_poll,NULL,wifi_cal5_now};
 if(wifi_cal5.cleanup_enabled)console_write("wifi-cal5-cleanup-plan: forward=8 reserved=8\n");
 console_write("wifi-cal5-progress: calibration replies verified;passive161/20MHz window up to10seconds;no network OUT\n");
 good=mt76_rx5_window_poll(&wifi_cal5.window,&wifi_cal5.plan,&window);wifi_cal5.receiving=false;return good;
}
static bool wifi_cal5_configure(void){
 struct xhci_device *d=wifi_cal5.d;void *p;
 if(!d || !d->input_context || !d->output_context || !xhci.command_ring || context_read32(d->output_context,9,0)&7)return false;
 if(!alloc_frame_zero(&wifi_cal5.ep.ring_phys,&p))return false;
 wifi_cal5.ep.ring=p;wifi_cal5.pin_ring=wifi_cal5.ep.ring_phys;wifi_cal5.pin_rptr=(uintptr_t)p;wifi_cal5.ep.cycle=true;wifi_cal5.ep.dci=9;wifi_cal5.ep.packet=512;write_link_trb(wifi_cal5.ep.ring,wifi_cal5.ep.ring_phys,true);
 wifi_cal5.plan=(struct mt76_rx_window_plan){.budget_us=10000000,.ring=wifi_cal5.ep.ring_phys,.slot=d->slot_id,.dci=9,.country={'U','S'},.channel=161};
 for(unsigned i=0;i<8;i++){if(!alloc_frame_zero(&wifi_cal5.plan.physical[i],&p))return false;wifi_cal5.plan.buffer[i]=p;wifi_cal5.pin_pages[i]=wifi_cal5.plan.physical[i];wifi_cal5.pin_v[i]=(uintptr_t)p;}
 if(!mt76_rx5_window_plan_valid(&wifi_cal5.plan))return false;
 if(wifi_cal5.cleanup_enabled)for(unsigned i=0;i<8;i++){
  uint64_t physical;if(!alloc_frame_zero(&physical,&p))return false;
  const uint64_t used[]={wifi_cal5.plan.ring,d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys,xhci.event_ring_phys};
  for(unsigned j=0;j<sizeof used/sizeof used[0];j++)if(physical==used[j])return false;
  for(unsigned j=0;j<8;j++)if(physical==wifi_cal5.plan.physical[j] || (j<i && physical==wifi_cal5.cleanup_physical[j]))return false;
  wifi_cal5.cleanup_physical[i]=physical;wifi_cal5.cleanup_buffer[i]=p;wifi_cal5.cleanup_pin[i]=(uintptr_t)p;
 }
 zero_bytes(d->input_context,PMM_FRAME_SIZE);context_write32(d->input_context,0,1,1u|(1u<<9));
 context_write32(d->input_context,1,0,(context_read32(d->output_context,0,0)&~(31u<<27))|(16u<<27));
 for(unsigned i=1;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
 context_write32(d->input_context,10,1,(3u<<1)|(6u<<3)|(512u<<16));context_write32(d->input_context,10,2,(uint32_t)(wifi_cal5.ep.ring_phys|1));context_write32(d->input_context,10,3,wifi_cal5.ep.ring_phys>>32);context_write32(d->input_context,10,4,4096);
 struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,.parameter_high=d->input_context_phys>>32,.control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;bool ok=submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&slot) && slot==d->slot_id;firmware_command_exact=false;if(!ok)firmware_command_failed=true;event_irq_restore(flags);
 wifi_cal5.configured=ok;return ok && wifi_cal5_layout();
}
static bool wifi_cal5_stop(void){
 if(!wifi_cal5_owned(&wifi_cal5,true) || wifi_cal5.stop_attempted)return false;
 struct xhci_device *d=wifi_cal5.d;wifi_cal5.stop_attempted=wifi_cal5.stopping=true;
 struct xhci_trb cmd={.control=(15u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 wifi_cal5.cleanup_stop_index=xhci.command_enqueue;wifi_cal5.cleanup_stop_command=cmd;wifi_cal5.cleanup_stop_command.control|=xhci.command_cycle?1u:0u;
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;bool ok=
#ifdef SRVROS_XHCI_CAL5_DRAIN_TEST
 cal5_drain_test_command(&cmd,15,&slot) &&
#else
 submit_command_trb(&cmd,15,&slot) &&
#endif
 slot==d->slot_id;firmware_command_exact=false;wifi_cal5.stopping=false;
 if(!ok)firmware_command_failed=true;
 if(ok){__atomic_thread_fence(__ATOMIC_ACQUIRE);wifi_cal5.stop_state=context_read32(d->output_context,9,0)&7;wifi_cal5.stop_dequeue=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);
 uint64_t deq=wifi_cal5.stop_dequeue&~15ull;ok=wifi_cal5.stop_state==3 && (wifi_cal5.stop_dequeue&15)==1 && deq>=wifi_cal5.plan.ring && deq<=wifi_cal5.plan.ring+16*(wifi_cal5.posts+wifi_cal5.cleanup_posts);}
 if(ok && wifi_cal5.cleanup_enabled){
  ok=!wifi_cal5.cleanup_failed && wifi_cal5.cleanup_stop_seen==1 && !wifi_cal5.cleanup_pending;
  if(ok){
   const struct xhci_trb *e=&wifi_cal5.cleanup_stop_event;
   uint64_t pointer=e->parameter_low|((uint64_t)e->parameter_high<<32);
   uint64_t expected=wifi_cal5.plan.ring+16*(8+wifi_cal5.cleanup_posts-(wifi_cal5.cleanup_stop_pending?1:0));
   ok=pointer==expected && wifi_cal5.stop_dequeue==(pointer|1);
  }
  if(ok && !wifi_cal5.cleanup_posts){
   const struct xhci_trb *e=&wifi_cal5.cleanup_stop_event;
   uint64_t pointer=e->parameter_low|((uint64_t)e->parameter_high<<32);
   ok=wifi_cal5.posts==8 && !wifi_cal5.pending && e->status==(27u<<24) && pointer==wifi_cal5.plan.ring+8*16 && wifi_cal5.stop_dequeue==(pointer|1);
  }
  if(ok){for(unsigned i=0;i<8;i++)copy_bytes(wifi_cal5.cleanup_snapshot[i],wifi_cal5.cleanup_buffer[i],4096);wifi_cal5.cleanup_frozen=true;}
 }
 if(ok){wifi_cal5.pending=false;wifi_cal5.stopped=true;}event_irq_restore(flags);
 if(wifi_cal5.cleanup_enabled)console_printf("wifi-cal5-stop: ok=%u posts=%u state=%u dequeue=%x ring=%x retained=1 cleanup=%u\n",(uint64_t)ok,(uint64_t)wifi_cal5.posts,(uint64_t)wifi_cal5.stop_state,wifi_cal5.stop_dequeue,wifi_cal5.plan.ring,(uint64_t)wifi_cal5.cleanup_posts);
 else console_printf("wifi-cal5-stop: ok=%u posts=%u state=%u dequeue=%x ring=%x retained=1\n",(uint64_t)ok,(uint64_t)wifi_cal5.posts,(uint64_t)wifi_cal5.stop_state,wifi_cal5.stop_dequeue,wifi_cal5.plan.ring);
 return ok;
}
/* Selection inspects immutable retained buffers only after MAC cleanup and
 * exact endpoint cessation, while the same selected RF owner is still live. */
static bool wifi_discovery5_requested;
static bool wifi_discovery5_owned(void *c,const struct mt76_rx_window *window){
 return c==&wifi_cal5 && wifi_discovery5_requested && window==&wifi_cal5.window &&
 wifi_cal5_owned(c,true) && wifi_channel_vco5_owned(&wifi_channel_vco5) &&
 mt76_channel_vco5_selected(&wifi_channel_vco5.state) && mt76_cvco5_valid(&wifi_channel_vco5.state.vco) &&
 wifi_cal5_fields(wifi_cal5.d) && wifi_cal5_cleanup_held() && !wifi_cal5.failed && wifi_cal5.stopped && !wifi_cal5.pending &&
 wifi_cal5.cal.complete && wifi_cal5.probe.complete && wifi_cal5.mac.complete &&
 wifi_cal5.mac.mac_disabled && wifi_cal5.mac.rx_drained && wifi_cal5.mac.tx_drained &&
 wifi_cal5.mac.filter_restored && wifi_cal5.mac.txop_restored &&
 window->complete && window->stopped && !window->failed;
}
static bool wifi_discovery5_inspect(void){
 const struct mt76_discovery_io io={&wifi_cal5,wifi_discovery5_owned};
 bool complete=mt76_discovery5_collect(&wifi_cal5.discovery,&wifi_cal5.window,&io);
 enum mt76_discovery5_status status=MT76_DISC5_OWNERSHIP;
 if(complete)status=mt76_discovery5_choose(&wifi_cal5.choice,&wifi_cal5.discovery,&wifi_cal5.window,&io,1,(const uint8_t *)"Demers5",7);
 /* No identifiers,other SSIDs,raw advertisements or secrets in logs. This
  * is an explicitly requested target candidate,not authentication authority. */
 console_printf("wifi-discovery5: complete=%u inputs=%u parsed=%u rows=%u matches=%u status=%u selected=%u channel=%u rsn_bytes=%u generation=%u retained=1 network_out=0 authenticated=0\n",
 (uint64_t)complete,(uint64_t)wifi_cal5.discovery.inputs,(uint64_t)wifi_cal5.discovery.parsed,(uint64_t)wifi_cal5.discovery.count,
 (uint64_t)wifi_cal5.choice.matches,(uint64_t)status,(uint64_t)wifi_cal5.choice.selected,(uint64_t)wifi_cal5.choice.row.channel,
 (uint64_t)wifi_cal5.choice.rsn_size,wifi_cal5.choice.generation);
 /* Absence/ambiguous/security refusal still permits inverse RF cleanup.
  * The strict saved trial checker separately requires successful selection. */
 return complete && status!=MT76_DISC5_OWNERSHIP;
}
#include "xhci_wifi_power5.inc"
#include "xhci_wifi_handoff5.inc"
static bool wifi_join5_requested;
static bool wifi_join5_inspect(void);
#include "xhci_wifi_peer5.inc"
#include "xhci_wifi_tx5.inc"
#include "xhci_wifi_join5.inc"
#include "xhci_wifi_wpa5.inc"
static bool wifi_cal5_inspect(void *context){
 if(context!=&wifi_channel_vco5 || wifi_cal5.attempted || !wifi_cal5_requested || !wifi_channel_vco5_owned(context) || !mt76_channel_vco5_selected(&wifi_channel_vco5.state) || !mt76_cvco5_valid(&wifi_channel_vco5.state.vco))return false;
 struct xhci_device *d=wifi_channel_vco5.d;wifi_cal5.d=d;wifi_cal5.attempted=wifi_cal5.active=true;wifi_cal5.command_ring=xhci.command_ring_phys;wifi_cal5.output=d->output_context_phys;wifi_cal5.input=d->input_context_phys;wifi_cal5.ep0=d->ep0_ring_phys;wifi_cal5.descriptor=d->descriptor_buffer_phys;
 console_write("wifi-cal5-progress: hold selected161/trim44;configure retained RX;MAC start;fresh IBI probe;R0 VCO161 FULL901 LC1 RXDCOC1;controlled RX;drain disable stop;inverse originals\n");
 bool ok=wifi_cal5_configure();
 console_printf("wifi-cal5-admission: configured=%u parent=%u selected=%u vco=%u fields=%u context=%u owner=%u\n",(uint64_t)ok,(uint64_t)wifi_channel_vco5_owned(&wifi_channel_vco5),(uint64_t)mt76_channel_vco5_selected(&wifi_channel_vco5.state),(uint64_t)mt76_cvco5_valid(&wifi_channel_vco5.state.vco),(uint64_t)wifi_cal5_fields(d),(uint64_t)(context_read32(d->output_context,9,0)&7),(uint64_t)wifi_cal5_owned(&wifi_cal5,false));
 if(ok){const struct mt76_mac_cycle_io io={&wifi_cal5,wifi_cal5_owned,wifi_cal5_read,wifi_cal5_write,wifi_cal5_delay,wifi_cal5_now,wifi_cal5_inspection};ok=mt76_mac_cycle_run(&wifi_cal5.mac,&io);}
 bool stopped=!wifi_cal5.configured || wifi_cal5_stop();
 wifi_cal5_cleanup_emit();
 if(wifi_cal5.window.attempted){const struct mt76_rx_window_io io={.context=&wifi_cal5,.owned=wifi_cal5_finish_owned};const struct mt76_rx_window_stop stop={.dequeue=wifi_cal5.stop_dequeue,.slot=d->slot_id,.dci=9,.state=wifi_cal5.stop_state,.cleanup_posts=wifi_cal5.cleanup_posts};bool finished=mt76_rx5_window_finish(&wifi_cal5.window,&io,stopped && wifi_cal5.mac.complete?&stop:NULL);ok=finished && ok;}
 for(unsigned i=0;i<wifi_cal5.window.attempts && i<8;i++){const struct mt76_rx_window_item *v=&wifi_cal5.window.item[i];console_printf("wifi-cal5-rx: index=%u submitted=%u completed=%u cc=%u remaining=%u actual=%u inspected=%u view=%u beacon_status=%u beacon=%u mpdu=%u interval=%u ssid_size=%u privacy=%u rsn=%u wpa=%u\n",(uint64_t)i,(uint64_t)v->submitted,(uint64_t)v->completed,(uint64_t)v->completion,(uint64_t)v->remaining,(uint64_t)v->actual,(uint64_t)v->inspected,(uint64_t)v->view_status,(uint64_t)v->beacon_status,(uint64_t)v->beacon,(uint64_t)v->mpdu,(uint64_t)v->interval,(uint64_t)v->ssid_size,(uint64_t)v->privacy,(uint64_t)v->rsn,(uint64_t)v->wpa);}
 ok=ok && stopped && !wifi_cal5.failed && wifi_cal5.cal.complete && wifi_cal5.mac.complete && wifi_cal5.mac.mac_disabled && wifi_cal5.mac.rx_drained && wifi_cal5.mac.tx_drained && wifi_cal5.window.complete;
 console_printf("wifi-cal5: ok=%u operations=%u commands=%u probe=%u calibration=%u mac=%u disabled=%u tx_drained=%u rx_drained=%u filter_restored=%u txop_restored=%u stopped=%u window=%u attempts=%u completions=%u beacons=%u elapsed_us=%u retained=1 network_out=0\n",(uint64_t)ok,(uint64_t)wifi_cal5.operations,(uint64_t)wifi_cal5.commands,(uint64_t)wifi_cal5.probe.complete,(uint64_t)wifi_cal5.cal.complete,(uint64_t)wifi_cal5.mac.complete,(uint64_t)wifi_cal5.mac.mac_disabled,(uint64_t)wifi_cal5.mac.tx_drained,(uint64_t)wifi_cal5.mac.rx_drained,(uint64_t)wifi_cal5.mac.filter_restored,(uint64_t)wifi_cal5.mac.txop_restored,(uint64_t)wifi_cal5.stopped,(uint64_t)wifi_cal5.window.complete,(uint64_t)wifi_cal5.window.attempts,(uint64_t)wifi_cal5.window.completions,(uint64_t)wifi_cal5.window.beacons,wifi_cal5.window.elapsed_us);
 if(wifi_discovery5_requested && ok)ok=wifi_discovery5_inspect();
 if(wifi_power5_requested && ok)ok=wifi_power5_inspect();
 wifi_cal5.active=false;return ok;
}
bool xhci_wifi_capture_cal5(void){if(wifi_cal5_requested || wifi_cal5.attempted)return false;wifi_cal5_requested=true;bool ok=xhci_wifi_capture_channel_vco5();wifi_cal5_requested=false;return ok;}

bool xhci_wifi_capture_discovery5(void){
 if(wifi_discovery5_requested || wifi_cal5_requested || wifi_cal5.attempted)return false;
 wifi_discovery5_requested=true;bool ok=xhci_wifi_capture_cal5();wifi_discovery5_requested=false;return ok;
}
