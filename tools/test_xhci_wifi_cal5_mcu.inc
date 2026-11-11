/* The fixed RF parent and register values are modeled. The actual5GHz
 * calibration engine/command callback and xHCI MCU publication,poll,dispatch,
 * completion and quarantine are unrenamed production functions. No hardware
 * acceptance,whole-adapter closure or retry is inferred from these cases. */
static bool cal5_mcu_parent;
static unsigned cal5_mcu_fault,cal5_mcu_target,cal5_mcu_calls,cal5_mcu_clock_calls;
static uint32_t cal5_mcu_alc,cal5_mcu_ibi;
static uint64_t cal5_mcu_clock_start;
static bool cal5_mcu_lifetime_active;
static void cal5_mcu_lifetime_bulk(unsigned);
static unsigned cal5_mcu_diagnostic_mode;
static void cal5_mcu_nested_observation(void);
static bool cal5_drain_test_parent(void){return cal5_mcu_parent;}
static void cal5_drain_test_progress(void){assert(!"calibration MCU fixture has no cleanup RX publications");}
static bool cal5_drain_test_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==cal5_mcu_test_device && v);
 if(early5_test_active)return wifi_mac_private_usb(d,write,a,v);
 if(a==0x13b0){if(write)cal5_mcu_alc=*v;else *v=cal5_mcu_alc;return true;}
 if(a==0x2124){if(write)cal5_mcu_ibi=*v;else *v=cal5_mcu_ibi;return true;}
 if(write)return false;
 if(a==0x1004)*v=12;
 else if(a==0x2000)*v=0xf000f200;
 else if(a==0x208)*v=0x40000050;
 else return false;
 return true;
}
static bool cal5_drain_test_delay(unsigned us){test_time+=us;return true;}
static bool cal5_drain_test_command(const struct xhci_trb *cmd,unsigned type,uint64_t *slot){
 (void)cmd;(void)type;(void)slot;assert(!"calibration MCU fixture must not publish a Stop command");return false;
}
static void cal5_mcu_clock(void){
 if(cal5_mcu_calls!=cal5_mcu_target || !cal5_mcu_test_device || !cal5_mcu_test_device->firmware.mcu.pending)return;
 if(++cal5_mcu_clock_calls==1)cal5_mcu_clock_start=test_time;
 else if(cal5_mcu_clock_calls==2)test_time=cal5_mcu_clock_start-1;
}
static void cal5_mcu_test_bulk(unsigned dci){
 if(cal5_mcu_lifetime_active){cal5_mcu_lifetime_bulk(dci);return;}
 struct xhci_device *d=cal5_mcu_test_device;struct xhci_wifi_mcu *m=&d->firmware.mcu;
 assert(d && dci==m->dci && wifi_cal5.commands && wifi_cal5.commands<=5);
 cal5_mcu_calls++;
 if(cal5_mcu_diagnostic_mode && cal5_mcu_calls==cal5_mcu_target)cal5_mcu_nested_observation();
 unsigned step=wifi_cal5.commands-1;bool fault=cal5_mcu_fault && cal5_mcu_calls==cal5_mcu_target;
 uint8_t expected[16];assert(mt76_poweron5_packet(step,expected,sizeof expected)==16);
 if(dci==16)assert(!memcmp(d->firmware.buffer,expected,16));
 if(fault && cal5_mcu_fault>=13){
  assert(cal5_mcu_fault<=15 && dci==11 && cal5_mcu_target==10);
  if(cal5_mcu_fault==15){m->response.enqueue=63;return;}
  unsigned index=xhci.event_dequeue;bool cycle=xhci.event_cycle;
  unsigned count=cal5_mcu_fault==13?6:5;
  for(unsigned n=0;n<count;n++){
   uint64_t p=m->response.ring_phys+0x800+16*n;
   struct xhci_trb e={.parameter_low=(uint32_t)p,.parameter_high=p>>32,
    .status=(13u<<24)|(MT76_MCU_RESPONSE_MAX-16),
    .control=(32u<<10)|(11u<<16)|((uint32_t)d->slot_id<<24)|(cycle?1u:0u)};
   if(cal5_mcu_fault==14 && n<4){
    /* Model dispatch from another consumer before this caller starts polling.
     * These first four raw samples are genuine calls into the native hook. */
    dispatch_transfer_event(&e);
    if(n==3){
     struct xhci_wifi_mcu_bulk_receipt *r=wifi_mcu_observer.receipt;
     assert(r && r->events.samples==4 && !r->events.overflow);
     /* Explicit HOST metadata seeding exercises finite counter overflow; it
      * does not model UINT32_MAX physical completions or traffic volume. */
     r->events.seen=r->events.transfers=r->events.relevant=r->events.exact=r->events.dropped=UINT32_MAX;
    }
   }else{
    assert((xhci.event_ring[index].control&1)!=(unsigned)cycle);
    xhci.event_ring[index]=e;
    if(++index==64){index=0;cycle=!cycle;}
   }
  }
  return; /* Withhold the target completion; preserve the original deadline. */
 }
 if(fault && (cal5_mcu_fault==1 || cal5_mcu_fault==7 || cal5_mcu_fault==8)){
  if(cal5_mcu_fault==7)test_time_step=0;
  if(cal5_mcu_fault==8){cal5_mcu_clock_calls=0;native_boundary_clock_hook=cal5_mcu_clock;}
  return;
 }
 if(dci==11){
  const uint32_t ids[5]={1,12,255,3,2},args[5]={0,161,0x901,1,1};
  uint32_t words[4]={0x4a000008u|((step+1)<<16),ids[step],args[step],0x4a000008u|((step+1)<<16)};
  for(unsigned i=0;i<16;i++)m->buffer[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
  if(fault && cal5_mcu_fault==11)m->buffer[4]^=1; /* valid USB,wrong firmware reply */
 }
 struct xhci_trb e={.parameter_low=(uint32_t)m->trb,.parameter_high=(uint32_t)(m->trb>>32),
  .status=dci==16?1u<<24:(13u<<24)|(MT76_MCU_RESPONSE_MAX-16),
  .control=(32u<<10)|(dci<<16)|((uint32_t)d->slot_id<<24)|(xhci.event_cycle?1u:0u)};
 if(fault){
  if(cal5_mcu_fault==2)e.status=0; /* invalidCC0 is NOT an absent completion */
  if(cal5_mcu_fault==3)e.parameter_low+=16;
  if(cal5_mcu_fault==4)e.control=(e.control&~(31u<<16))|(9u<<16);
  if(cal5_mcu_fault==5)e.status=6u<<24;
  if(cal5_mcu_fault==6)e.status=(13u<<24)|(MT76_MCU_RESPONSE_MAX+1);
  if(cal5_mcu_fault==9)e.control=(e.control&0xffffffu)|(2u<<24);
  if(cal5_mcu_fault==10)e.control|=4;
 }
 if(!fault || cal5_mcu_fault==11){
  context_write32(d->output_context,dci,0,1);
  context_write32(d->output_context,dci,2,(m->trb+16)|1);
  context_write32(d->output_context,dci,3,(m->trb+16)>>32);
 }
 xhci.event_ring[xhci.event_dequeue]=e;
 if(fault && cal5_mcu_fault==12)cal5_mcu_parent=false;
}
static struct xhci_device *cal5_mcu_setup(unsigned fault,unsigned target){
 assert(!wifi_mcu_observer.receipt && !wifi_mcu_observer.device);
 reset_test();memset(&wifi_cal5,0,sizeof wifi_cal5);memset(&wifi_stream5,0,sizeof wifi_stream5);
 memset(&wifi_join5,0,sizeof wifi_join5);memset(&wifi_wpa5,0,sizeof wifi_wpa5);memset(&wifi_fence5_native,0,sizeof wifi_fence5_native);
 test_time=1000;test_time_step=100;complete_control=false;complete_device=NULL;
 cal5_mcu_fault=fault;cal5_mcu_target=target;cal5_mcu_calls=cal5_mcu_clock_calls=0;
 cal5_mcu_diagnostic_mode=0;
 cal5_mcu_parent=true;cal5_mcu_alc=0x2f2f1b1b;cal5_mcu_ibi=UINT32_MAX;
 struct xhci_device *d=&xhci.devices[0];cal5_mcu_test_device=d;
 *d=(struct xhci_device){.present=true,.slot_id=1,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100,
  .wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000}};
 struct xhci_wifi_firmware *f=&d->firmware;f->active=f->configured=f->ready=true;
 void *v;
 assert(alloc_frame_zero(&d->input_context_phys,&v));d->input_context=v;
 assert(alloc_frame_zero(&d->output_context_phys,&v));d->output_context=v;
 assert(alloc_frame_zero(&d->ep0_ring_phys,&v));d->ep0_ring=v;
 assert(alloc_frame_zero(&d->descriptor_buffer_phys,&v));d->descriptor_buffer=v;
 assert(alloc_frame_zero(&f->buffer_phys,&v));f->buffer=v;
 assert(alloc_frame_zero(&f->ep.ring_phys,&v));f->ep.ring=v;f->ep.cycle=true;f->ep.dci=16;f->ep.packet=512;write_link_trb(v,f->ep.ring_phys,true);
 struct xhci_wifi_mcu *m=&f->mcu;m->configured=true;
 assert(alloc_frame_zero(&m->buffer_phys,&v));m->buffer=v;
 assert(alloc_frame_zero(&m->response.ring_phys,&v));m->response.ring=v;m->response.cycle=true;m->response.dci=11;m->response.packet=512;write_link_trb(v,m->response.ring_phys,true);
 m->ant_fields=(struct mt76_ant_fields){.attempted=true,.complete=true,.valid=true,.reads=24,.writes=2,.polls=2,.blocks=2,.antenna=0x17ff,.cfg1=0xffef,.nic0=0xfd11,.nic1=0x1004};
 assert(alloc_frame_zero(&xhci.command_ring_phys,&v));xhci.command_ring=v;xhci.command_cycle=true;
 assert(alloc_frame_zero(&xhci.event_ring_phys,&v));xhci.event_ring=v;xhci.event_cycle=true;
 xhci.present=xhci.operational=xhci.controller_owned=true;xhci.op_base=0x100;xhci.db_base=0x4000;xhci.rt_base=0x3000;
 wifi_cal5_requested=true;wifi_cal5.d=d;wifi_cal5.active=wifi_cal5.attempted=wifi_cal5.configured=true;
 wifi_cal5.command_ring=xhci.command_ring_phys;wifi_cal5.input=d->input_context_phys;wifi_cal5.output=d->output_context_phys;
 wifi_cal5.ep0=d->ep0_ring_phys;wifi_cal5.descriptor=d->descriptor_buffer_phys;
 assert(alloc_frame_zero(&wifi_cal5.plan.ring,&v));wifi_cal5.ep.ring=v;wifi_cal5.ep.ring_phys=wifi_cal5.pin_ring=wifi_cal5.plan.ring;
 wifi_cal5.pin_rptr=(uintptr_t)v;wifi_cal5.ep.cycle=true;wifi_cal5.ep.dci=9;wifi_cal5.ep.packet=512;write_link_trb(v,wifi_cal5.plan.ring,true);
 wifi_cal5.plan.slot=1;wifi_cal5.plan.dci=9;wifi_cal5.plan.country[0]='U';wifi_cal5.plan.country[1]='S';wifi_cal5.plan.channel=161;wifi_cal5.plan.budget_us=10000000;
 for(unsigned i=0;i<8;i++){assert(alloc_frame_zero(&wifi_cal5.plan.physical[i],&v));wifi_cal5.plan.buffer[i]=v;wifi_cal5.pin_pages[i]=wifi_cal5.plan.physical[i];wifi_cal5.pin_v[i]=(uintptr_t)v;}
 context_write32(d->output_context,9,0,1);context_write32(d->output_context,9,2,wifi_cal5.plan.ring|1);context_write32(d->output_context,9,3,wifi_cal5.plan.ring>>32);
 for(unsigned n=0;n<2;n++){unsigned dci=n?16:11;uint64_t ring=n?f->ep.ring_phys:m->response.ring_phys;context_write32(d->output_context,dci,0,1);context_write32(d->output_context,dci,2,ring|1);context_write32(d->output_context,dci,3,ring>>32);}
 wifi_cal5.mac.started=wifi_cal5.mac.snapshot_complete=true;wifi_cal5.calibrating=true;
 assert(wifi_cal5_cal_owned(&wifi_cal5));return d;
}
static enum mt76_calibration_status cal5_mcu_run(void){
 const struct mt76_poweron_io io={&wifi_cal5,wifi_cal5_cal_owned,wifi_cal5_cal_read,wifi_cal5_cal_write,wifi_cal5_command,wifi_cal5_cal_delay,wifi_cal5_restore};
 return mt76_poweron5_calibration_run(&wifi_cal5.cal,&io,&wifi_cal5.d->wifi_identity,&wifi_cal5.d->firmware.mcu.ant_fields,161,20);
}
static void cal5_mcu_finish(void){
 assert(!wifi_mcu_observer.receipt && !wifi_mcu_observer.device);
    assert(!wifi_mcu_loss.trace && !wifi_mcu_loss.device);
 cal5_mcu_test_device=NULL;cal5_mcu_parent=false;wifi_cal5_requested=false;
 cal5_mcu_lifetime_active=false;
 native_boundary_clock_hook=NULL;test_time_step=1000;
 memset(&wifi_cal5,0,sizeof wifi_cal5);
 memset(&wifi_early5,0,sizeof wifi_early5);
}
/* Refusal and an independently allocated nested device cannot detach the
 * outer diagnostic window, including when the numeric sequence saturates. */
static void cal5_mcu_nested_observation(void){
 struct xhci_device *d=cal5_mcu_test_device;
 struct xhci_wifi_mcu_bulk_receipt *active=wifi_mcu_observer.receipt;
 assert(active && wifi_mcu_observer.device==d && active->sequence==d->firmware.mcu.bulk_sequence);
 if(cal5_mcu_diagnostic_mode<=2){
  assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==-1);
  assert(d->firmware.mcu.last_bulk.reason==WIFI_MCU_BULK_REFUSED);
  assert(!d->firmware.mcu.last_bulk.events.valid && !d->firmware.mcu.last_bulk.ring_observation.valid);
 }else{
  struct xhci_device *other=&xhci.devices[1];assert(!other->present);
  if(cal5_mcu_diagnostic_mode==4){
   *other=(struct xhci_device){.present=true,.slot_id=2,.wifi_probe_ok=true,
    .wifi_identity={0x76100002,0x76502000}};
   struct xhci_wifi_firmware *f=&other->firmware;f->active=f->configured=f->ready=true;
   f->mcu.configured=true;void *v;
   assert(alloc_frame_zero(&other->output_context_phys,&v));other->output_context=v;
   assert(alloc_frame_zero(&f->buffer_phys,&v));f->buffer=v;
   assert(alloc_frame_zero(&f->ep.ring_phys,&v));f->ep.ring=v;
   f->ep.cycle=true;f->ep.dci=16;f->ep.packet=512;write_link_trb(v,f->ep.ring_phys,true);
   /* The MMIO model serves only slot1, so this separate slot times out using
    * the real bulk loop without fabricating a nested completion. */
  }
  assert(wifi_mcu_bulk(other,false,16)==-1);
  assert(other->firmware.mcu.last_bulk.reason==(cal5_mcu_diagnostic_mode==4?WIFI_MCU_BULK_ELAPSED:WIFI_MCU_BULK_REFUSED));
  assert(!other->firmware.mcu.last_bulk.events.valid && !other->firmware.mcu.last_bulk.events.seen);
  assert(other->firmware.mcu.last_bulk.ring_observation.valid==(cal5_mcu_diagnostic_mode==4));
 }
 assert(wifi_mcu_observer.device==d && wifi_mcu_observer.receipt==active && !active->events.seen);
}
/* A separate controller cursor consumes the real published Normal/Link TRBs.
 * The original24 callback/branch fixtures above intentionally stay unchanged.
 * This models coherent memory observed at the MMIO doorbell, not asynchronous
 * observation between compiler stores or real firmware calibration behavior. */
static struct {
 unsigned index[2],wraps[2],transfers[2],event_index,event_wraps,events,calls,mode;
 bool cycle[2],event_cycle,prefix,delayed_pending;
 uint64_t delayed_at;struct xhci_trb delayed;
} cal5_mcu_lifetime;
static void cal5_mcu_lifetime_event(struct xhci_trb event){
 unsigned n=cal5_mcu_lifetime.event_index;
 assert((xhci.event_ring[n].control&1)!=(unsigned)cal5_mcu_lifetime.event_cycle);
 event.control=(event.control&~1u)|(cal5_mcu_lifetime.event_cycle?1u:0u);
 xhci.event_ring[n]=event;cal5_mcu_lifetime.events++;
 if(++cal5_mcu_lifetime.event_index==64){
  cal5_mcu_lifetime.event_index=0;cal5_mcu_lifetime.event_cycle=!cal5_mcu_lifetime.event_cycle;
  cal5_mcu_lifetime.event_wraps++;
 }
}
static void cal5_mcu_lifetime_clock(void){
 if(cal5_mcu_lifetime.delayed_pending && test_time>=cal5_mcu_lifetime.delayed_at){
  cal5_mcu_lifetime.delayed_pending=false;
  unsigned mode=cal5_mcu_lifetime.mode;
  struct xhci_wifi_mcu *m=&cal5_mcu_test_device->firmware.mcu;
  if(mode==13){m->buffer=(uint8_t *)(uintptr_t)1;native_boundary_clock_hook=NULL;return;}
  if(mode==14){test_time_step=0;native_boundary_clock_hook=NULL;return;}
  if(mode==9){
   const uint32_t words[4]={0x4a010008,1,0,0x4a010008};
   for(unsigned i=0;i<16;i++)m->buffer[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
  }
  cal5_mcu_lifetime_event(cal5_mcu_lifetime.delayed);
  if(mode==11 || mode==12){
   struct xhci_trb event;assert(poll_event(&event));
   /* Deliberately faulty modeled consumer/metadata, not an assertion that
    * production drops events. Use the real poll/dispatch helpers to prove
    * that the new trace separates consumption,dispatch and claim. */
   if(mode==12){m->pending=false;dispatch_transfer_event(&event);m->pending=true;}
  }
  native_boundary_clock_hook=NULL;
 }
}
static void cal5_mcu_lifetime_bulk(unsigned dci){
 struct xhci_device *d=cal5_mcu_test_device;
 struct xhci_wifi_mcu *m=&d->firmware.mcu;
 unsigned dir=dci==11?1:0,step=wifi_cal5.commands?wifi_cal5.commands-1:0;
 struct xhci_bulk_endpoint *ep=dir?&m->response:&d->firmware.ep;
 bool target=!cal5_mcu_lifetime.prefix && step==(cal5_mcu_lifetime.mode>=6?0u:2u) && dir;
 assert(dci==11 || dci==16);cal5_mcu_lifetime.calls++;
 if(cal5_mcu_lifetime.mode==6 && !cal5_mcu_lifetime.prefix && !step && !dir){
  /* Rebase only the independent HOST event fixture, with no pending event.
   * The R0 OUT completion consumes61/C1; the actual IN receipt then starts
   * at62/C1, before the modeled foreign transfers wrap to23/C0. */
  xhci.event_dequeue=cal5_mcu_lifetime.event_index=61;
  xhci.event_cycle=cal5_mcu_lifetime.event_cycle=true;
  for(unsigned i=0;i<64;i++)xhci.event_ring[i]=(struct xhci_trb){.control=i<61?1u:0u};
 }
 /* Negative cases corrupt hardware-visible ownership, not m->trb or the
  * independently maintained controller cursor, so a fabricated completion
  * cannot accidentally make a rejected publication pass. */
 if(target && cal5_mcu_lifetime.mode==3)
  ep->ring[cal5_mcu_lifetime.index[dir]].control^=1;
 if(target && cal5_mcu_lifetime.mode==5){
  assert(cal5_mcu_lifetime.index[dir]==63);ep->ring[63].control^=1;
 }
 if(cal5_mcu_lifetime.index[dir]==63){
  const struct xhci_trb *link=&ep->ring[63];
  if((link->control&1)!=(unsigned)cal5_mcu_lifetime.cycle[dir])return;
  assert((link->parameter_low|((uint64_t)link->parameter_high<<32))==ep->ring_phys);
  assert(link->status==0 && link->control==((6u<<10)|2u|(cal5_mcu_lifetime.cycle[dir]?1u:0u)));
  cal5_mcu_lifetime.index[dir]=0;cal5_mcu_lifetime.cycle[dir]=!cal5_mcu_lifetime.cycle[dir];
  cal5_mcu_lifetime.wraps[dir]++;
 }
 unsigned index=cal5_mcu_lifetime.index[dir];const struct xhci_trb *td=&ep->ring[index];
 if((td->control&1)!=(unsigned)cal5_mcu_lifetime.cycle[dir])return;
 uint64_t pointer=ep->ring_phys+16*index;
 uint64_t buffer=td->parameter_low|((uint64_t)td->parameter_high<<32);
 assert(td->control==((1u<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP|(cal5_mcu_lifetime.cycle[dir]?1u:0u)));
 assert(td->status==(dir?MT76_MCU_RESPONSE_MAX:16u));
 assert(buffer==(dir?m->buffer_phys:d->firmware.buffer_phys));
 assert(pointer==m->trb && dci==m->dci);
 if(!dir && !cal5_mcu_lifetime.prefix){
  uint8_t expected[16];assert(mt76_poweron5_packet(step,expected,sizeof expected)==16);
  assert(!memcmp(d->firmware.buffer,expected,16));
 }
 if(target && (cal5_mcu_lifetime.mode==2 || cal5_mcu_lifetime.mode==6)){
  /* No USB response/completion is produced. Twenty-five foreign events cross
   * the event-ring boundary exactly as the saved failure's categorical rows.
   * The modeled live endpoint context intentionally retains its original
   * dequeue, demonstrating why that snapshot alone has no stuck-ring proof. */
  if(cal5_mcu_lifetime.mode==2){
   assert(index==18 && m->bulk_sequence==3821);
   assert(xhci.event_dequeue==47 && !xhci.event_cycle);
   for(unsigned n=0;n<25;n++)cal5_mcu_lifetime_event((struct xhci_trb){.control=39u<<10});
  }else{
   assert(index==16 && m->bulk_sequence==3817);
   assert(xhci.event_dequeue==62 && xhci.event_cycle);
   /* Unrelated Transfer Events, not a claimed MCU completion or a model
    * of this machine's HID device. The production dispatcher observes all. */
   for(unsigned n=0;n<25;n++)cal5_mcu_lifetime_event((struct xhci_trb){
    .parameter_low=0x1000,.status=1u<<24,.control=(32u<<10)|(7u<<16)|(6u<<24)});
  }
  return;
 }
 cal5_mcu_lifetime.index[dir]++;cal5_mcu_lifetime.transfers[dir]++;
 if(cal5_mcu_lifetime.mode!=2 && cal5_mcu_lifetime.mode!=6){
  uint64_t next=ep->ring_phys+16*cal5_mcu_lifetime.index[dir];
  context_write32(d->output_context,dci,2,(uint32_t)next|(cal5_mcu_lifetime.cycle[dir]?1u:0u));
  context_write32(d->output_context,dci,3,next>>32);
 }
 if(dir && !(target && cal5_mcu_lifetime.mode==9)){
  const uint32_t ids[5]={1,12,255,3,2},args[5]={0,161,0x901,1,1};
  uint32_t words[4]={0x4a000008u|((step+1)<<16),ids[step],args[step],0x4a000008u|((step+1)<<16)};
  uint8_t *reply=pmm_phys_to_virt(buffer);
  for(unsigned i=0;i<16;i++)reply[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
  if(target && cal5_mcu_lifetime.mode==4)reply[4]^=1;
 }
 struct xhci_trb event={.parameter_low=(uint32_t)pointer,.parameter_high=pointer>>32,
  .status=dir?(13u<<24)|(MT76_MCU_RESPONSE_MAX-16):1u<<24,
  .control=(32u<<10)|(dci<<16)|((uint32_t)d->slot_id<<24)};
 if(target && cal5_mcu_lifetime.mode==10)return; /* DMA reply, no event */
 if(target && (cal5_mcu_lifetime.mode==1 || cal5_mcu_lifetime.mode==7 || cal5_mcu_lifetime.mode>=8)){
  cal5_mcu_lifetime.delayed=event;cal5_mcu_lifetime.delayed_pending=true;
  unsigned mode=cal5_mcu_lifetime.mode;
  cal5_mcu_lifetime.delayed_at=test_time+(mode==8 || mode==9?650000:mode==13?550000:mode==14?500200:400000);
  native_boundary_clock_hook=cal5_mcu_lifetime_clock;
 }else cal5_mcu_lifetime_event(event);
}
static void test_wifi_cal5_mcu_lifetime(void){
 for(unsigned mode=0;mode<15;mode++){
  struct xhci_device *d=cal5_mcu_setup(0,0);struct xhci_wifi_mcu *m=&d->firmware.mcu;
  memset(&cal5_mcu_lifetime,0,sizeof cal5_mcu_lifetime);
  cal5_mcu_lifetime.mode=mode;cal5_mcu_lifetime.prefix=true;
  cal5_mcu_lifetime.cycle[0]=cal5_mcu_lifetime.cycle[1]=cal5_mcu_lifetime.event_cycle=true;
  cal5_mcu_lifetime.event_index=xhci.event_dequeue=3;cal5_mcu_lifetime_active=true;
  /* Three unrelated events were consumed before this transport owner began.
   * Their old cycle must remain unavailable when the event producer wraps. */
  for(unsigned n=0;n<3;n++)xhci.event_ring[n]=(struct xhci_trb){.control=(39u<<10)|1u};
  /* Three no-reply commands followed by1906 request/response pairs make the
   * actual FULL-calibration IN bulk3821,index18. The prefix models transport
   * volume only; it is not a replacement for the real initialization recipe.
   * The Link-corruption variant chooses the next segment boundary instead. */
  size_t size=mt76_mcu_select_response_queue(d->firmware.buffer,MT76_MCU_PACKET_MAX);assert(size==16);
  for(unsigned n=0;n<3;n++)assert(wifi_mcu_bulk(d,false,size)==16);
  unsigned prefix=mode==5?1888:1906;
  for(unsigned n=0;n<prefix;n++){
   assert(mt76_poweron5_packet(0,d->firmware.buffer,16)==16);
   assert(wifi_mcu_bulk(d,false,16)==16);
   assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==16);
  }
  assert(cal5_mcu_lifetime.wraps[0]>=29 && cal5_mcu_lifetime.wraps[1]>=29 && cal5_mcu_lifetime.event_wraps>=59);
  cal5_mcu_lifetime.prefix=false;
  enum mt76_calibration_status status=cal5_mcu_run();
  assert((status==MT76_CAL_OK)==(mode<2 || mode==7));
  assert(cal5_mcu_lifetime.calls==m->bulk_sequence && !freed);
  if(mode<2 || mode==7){
   assert(wifi_cal5.cal.complete && wifi_cal5.commands==5 && !m->bulk_failure_valid && !m->pending);
   assert(m->bulk_sequence==3825 && cal5_mcu_lifetime.transfers[1]==1911);
   assert(!d->ep0_quarantined && !cal5_mcu_lifetime.delayed_pending && !native_boundary_clock_hook);
   for(unsigned dir=0;dir<2;dir++){
    struct xhci_bulk_endpoint *ep=dir?&m->response:&d->firmware.ep;
    unsigned index=cal5_mcu_lifetime.index[dir];bool cycle=cal5_mcu_lifetime.cycle[dir];
    if(index==63){index=0;cycle=!cycle;}
    assert(ep->enqueue==index && ep->cycle==cycle);
   }
  }else if(mode==4){
   assert(!wifi_cal5.cal.complete && wifi_cal5.commands==3 && !m->bulk_failure_valid);
   assert(m->last_bulk.reason==WIFI_MCU_BULK_SUCCESS && !m->pending && !m->failed && !d->ep0_quarantined);
  }else if(mode>=8){
   const struct xhci_wifi_mcu_loss_trace *t=&m->loss_trace;
   assert(!wifi_cal5.cal.complete && wifi_cal5.commands==1 && m->bulk_failure_valid);
   assert(m->bulk_failure.reason==WIFI_MCU_BULK_ELAPSED && m->bulk_failure.flags==29);
   assert(m->bulk_failure.elapsed_us>=500000 && m->bulk_failure.elapsed_us<501000);
   assert(t->published && t->sequence==3817 && t->original_reason==WIFI_MCU_BULK_ELAPSED && t->original_flags==29 && !t->overflow);
   assert(t->original.valid && t->original.stable && t->original.reply_match==(mode!=9));
   assert(m->failed && d->firmware.failed && d->ep0_quarantined && t->late_attempted);
   assert(m->bulk_sequence==3817 && m->response.enqueue==17 && !freed);
   if(mode==8 || mode==9){
    assert(t->late_completed && t->late_reason==1 && t->elapsed_us>=650000 && t->elapsed_us<652000);
    assert(t->consumed_exact==1 && t->dispatched_exact==1 && t->claimed==1);
    assert(!m->pending && m->done && t->final_flags==30 && t->final.reply_match);
   }else{
    assert(!t->late_completed && m->pending && !m->done && t->final_flags==29);
    assert(t->late_reason==(mode==13?4u:mode==14?5u:2u));
    if(mode!=13 && mode!=14)assert(t->elapsed_us>=1500000 && t->elapsed_us<1501000);
    assert(t->claimed==0 && t->consumed_exact==(mode==11 || mode==12?1u:0u));
    assert(t->dispatched_exact==(mode==12?1u:0u));
    assert(t->final.valid==(mode!=13));
   }
   struct xhci_wifi_mcu_bulk_receipt retained=m->bulk_failure;
   unsigned calls=cal5_mcu_lifetime.calls;uint64_t enqueue=m->response.enqueue;
   assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==-1);
   assert(cal5_mcu_lifetime.calls==calls && m->response.enqueue==enqueue && !freed);
   assert(!memcmp(&retained,&m->bulk_failure,sizeof retained));
  }else{
   assert(!wifi_cal5.cal.complete && wifi_cal5.commands==(mode==6?1u:3u) && m->bulk_failure_valid);
   assert(m->bulk_failure.reason==WIFI_MCU_BULK_ELAPSED && m->bulk_failure.flags==29);
   assert(m->pending && !m->done && m->failed && d->ep0_quarantined);
   assert(m->bulk_failure.dci==11 && m->bulk_failure.elapsed_us>=500000);
   if(mode==2){
    const struct xhci_wifi_mcu_bulk_receipt *r=&m->bulk_failure;
    assert(r->sequence==3821 && r->trb==m->response.ring_phys+18*16);
    assert(r->dispatched==25 && r->event_start==47 && !r->event_start_cycle && r->event_end==8 && r->event_end_cycle);
    assert(r->endpoint_valid && r->endpoint_state==1 && r->endpoint_dequeue==(m->response.ring_phys|1));
    assert(r->ring_observation.valid && r->ring_observation.index==18 && r->ring_observation.producer_index==19);
    assert(r->ring_observation.expected_parameter==m->buffer_phys && r->ring_observation.expected_cycle==1);
    assert(r->ring_observation.submitted_parameter==m->buffer_phys && r->ring_observation.submitted_status==1024);
    assert(r->events.valid && r->events.slot==1 && r->events.seen==25 && !r->events.transfers && !r->events.samples);
   }
   if(mode==3)assert(m->bulk_failure.ring_observation.valid &&
       (m->bulk_failure.ring_observation.submitted_control&1)!=m->bulk_failure.ring_observation.expected_cycle);
   if(mode==5)assert(m->bulk_failure.ring_observation.valid &&
       m->bulk_failure.ring_observation.index==0 && (m->bulk_failure.ring_observation.link_control&1)==m->bulk_failure.ring_observation.expected_cycle);
   if(mode==6){
    const struct xhci_wifi_mcu_bulk_receipt *r=&m->bulk_failure;
    assert(r->sequence==3817 && r->trb==m->response.ring_phys+16*16);
    assert(r->dispatched==25 && r->event_start==62 && r->event_start_cycle && r->event_end==23 && !r->event_end_cycle);
    assert(r->endpoint_valid && r->endpoint_state==1 && r->endpoint_dequeue==(m->response.ring_phys|1));
    assert(r->ring_observation.valid && r->ring_observation.index==16 && r->ring_observation.producer_index==17 && r->ring_observation.producer_cycle==1);
    assert(r->ring_observation.expected_parameter==m->buffer_phys && r->ring_observation.expected_cycle==1);
    assert(r->ring_observation.submitted_parameter==m->buffer_phys && r->ring_observation.submitted_status==1024 && r->ring_observation.submitted_control==0x425);
    assert(r->events.valid && r->events.seen==25 && r->events.transfers==25 && !r->events.relevant && !r->events.exact && !r->events.samples && !r->events.dropped && !r->events.overflow);
   }
   struct xhci_wifi_mcu_bulk_receipt retained=m->bulk_failure;
   unsigned calls=cal5_mcu_lifetime.calls,transfers=cal5_mcu_lifetime.transfers[1];uint64_t enqueue=m->response.enqueue;
   assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==-1);
   assert(cal5_mcu_lifetime.calls==calls && cal5_mcu_lifetime.transfers[1]==transfers && m->response.enqueue==enqueue && !freed);
   assert(!memcmp(&retained,&m->bulk_failure,sizeof retained) && m->last_bulk.reason==WIFI_MCU_BULK_REFUSED);
  }
  cal5_mcu_finish();
 }
 puts("xhci-cal5-mcu-lifetime: PASS15 realTRB/Link63 after1906INprefix;original8 controls plus earlyDMA/lateDMA650ms,noevent,consumed-not-dispatched,dispatched-not-claimed,pinloss,stalledclock;original500msfailure and quarantine retained;modeledcontroller/firmware,physicalcauseunproved");
}
static void test_wifi_cal5_mcu_observations(void){
 for(unsigned mode=1;mode<=4;mode++){
  struct xhci_device *d=cal5_mcu_setup(0,10);cal5_mcu_diagnostic_mode=mode;
  if(mode==2)d->firmware.mcu.bulk_sequence=UINT64_MAX;
  assert(cal5_mcu_run()==MT76_CAL_OK && !freed);
  struct xhci_wifi_mcu *m=&d->firmware.mcu;
  assert(m->last_bulk.reason==WIFI_MCU_BULK_SUCCESS && m->last_bulk.events.valid);
  assert(m->last_bulk.events.seen==1 && m->last_bulk.events.transfers==1 && m->last_bulk.events.exact==1);
  assert(!m->last_bulk.ring_observation.valid && !wifi_mcu_observer.receipt);
  if(mode<=2)assert(m->bulk_failure_valid && m->bulk_failure.reason==WIFI_MCU_BULK_REFUSED &&
      !m->bulk_failure.events.valid && m->last_bulk.sequence==(mode==2?UINT64_MAX:10));
  else assert(!m->bulk_failure_valid);
  cal5_mcu_finish();
 }
 /* A changed owner, invalid index/alignment/producer or retiring device must
  * not authorize any new retained-DMA read, even if the pointer is unusable. */
 struct xhci_device *d=cal5_mcu_setup(0,0);struct xhci_wifi_mcu *m=&d->firmware.mcu;
 struct xhci_wifi_mcu_bulk_receipt r={.sequence=1,.dci=11,.ring=m->response.ring_phys,
  .trb=m->response.ring_phys,.published_ring=m->response.ring,
  .published_parameter=m->buffer_phys,.published_slot=1,.published_cycle=1};
 struct xhci_trb *saved=m->response.ring;
 m->response.ring=(struct xhci_trb *)(uintptr_t)1;wifi_mcu_observe_ring(d,&r);
 assert(!r.ring_observation.valid && !r.ring_observation.submitted_parameter && !r.ring_observation.link_control);
 m->response.ring=saved;
 r.trb=r.ring+63*16;wifi_mcu_observe_ring(d,&r);assert(!r.ring_observation.valid);
 r.trb=r.ring+1;wifi_mcu_observe_ring(d,&r);assert(!r.ring_observation.valid);
 r.trb=r.ring;m->response.enqueue=63;wifi_mcu_observe_ring(d,&r);assert(!r.ring_observation.valid);
 m->response.enqueue=0;d->retiring=true;wifi_mcu_observe_ring(d,&r);assert(!r.ring_observation.valid);
 d->retiring=false;
 assert(!r.ring_observation.expected_parameter && !r.ring_observation.producer_index && !r.ring_observation.submitted_control);
 cal5_mcu_finish();
 /* Exercise the real dispatch hook before ordinary handlers, with four
  * retained raw samples, explicit drop accounting and saturating counters. */
 d=cal5_mcu_setup(0,0);m=&d->firmware.mcu;
 r=(struct xhci_wifi_mcu_bulk_receipt){.sequence=1,.dci=11,.ring=m->response.ring_phys,
  .trb=m->response.ring_phys,.published_slot=1};
 r.events.valid=true;r.events.slot=1;wifi_mcu_observer.device=d;wifi_mcu_observer.receipt=&r;
 for(unsigned n=0;n<6;n++){
  uint64_t p=r.ring+0x800+16*n;
  struct xhci_trb e={.parameter_low=(uint32_t)p,.parameter_high=p>>32,
   .status=(13u<<24)|n,.control=(32u<<10)|(11u<<16)|(1u<<24)};
  dispatch_transfer_event(&e);
 }
 assert(r.events.seen==6 && r.events.transfers==6 && r.events.relevant==6 && !r.events.exact);
 assert(r.events.samples==4 && r.events.dropped==2 && !r.events.overflow && !m->done && !m->pending);
 for(unsigned n=0;n<4;n++)assert(r.events.sample[n].parameter_low==(uint32_t)(r.ring+0x800+16*n) &&
    r.events.sample[n].status==((13u<<24)|n));
 struct xhci_trb retained[4];memcpy(retained,r.events.sample,sizeof retained);
 r.events.seen=r.events.transfers=r.events.relevant=r.events.exact=r.events.dropped=UINT32_MAX;
 struct xhci_trb exact={.parameter_low=(uint32_t)r.trb,.parameter_high=r.trb>>32,
  .status=0,.control=(32u<<10)|(11u<<16)|(1u<<24)|4};
 dispatch_transfer_event(&exact);
 assert(r.events.overflow && r.events.seen==UINT32_MAX && r.events.transfers==UINT32_MAX &&
     r.events.relevant==UINT32_MAX && r.events.exact==UINT32_MAX && r.events.dropped==UINT32_MAX);
 assert(r.events.samples==4 && !memcmp(retained,r.events.sample,sizeof retained) && !m->done);
 wifi_mcu_bulk_record(d,&r,WIFI_MCU_BULK_SUCCESS,0);
 assert(!wifi_mcu_observer.receipt && !wifi_mcu_observer.device && m->last_bulk.events.overflow);
 struct xhci_wifi_mcu_bulk_receipt frozen=m->last_bulk;dispatch_transfer_event(&exact);
 assert(!memcmp(&frozen,&m->last_bulk,sizeof frozen));
 cal5_mcu_finish();
 /* These additional callback-driven traces use the real calibration failure
  * formatter, allowing strict Python consumers to check the emitted contracts. */
 for(unsigned fault=13;fault<=15;fault++){
  d=cal5_mcu_setup(fault,10);m=&d->firmware.mcu;
  assert(cal5_mcu_run()!=MT76_CAL_OK && m->bulk_failure_valid && !freed);
  const struct xhci_wifi_mcu_bulk_receipt *f=&m->bulk_failure;
  assert(f->sequence==10 && f->reason==WIFI_MCU_BULK_ELAPSED && f->events.valid);
  assert(m->pending && m->failed && d->ep0_quarantined);
  if(fault==13)assert(f->ring_observation.valid && f->events.seen==6 &&
      f->events.samples==4 && f->events.dropped==2 && !f->events.exact && !f->events.overflow);
  if(fault==14)assert(f->ring_observation.valid && f->events.samples==4 && f->events.overflow &&
      f->events.seen==UINT32_MAX && f->events.transfers==UINT32_MAX && f->events.relevant==UINT32_MAX &&
      f->events.exact==UINT32_MAX && f->events.dropped==UINT32_MAX && f->dispatched==1);
  if(fault==15)assert(!f->ring_observation.valid && !f->ring_observation.submitted_control &&
      !f->ring_observation.expected_parameter && !f->events.seen && !f->events.samples);
  struct xhci_wifi_mcu_bulk_receipt retained=*f;
  unsigned calls=cal5_mcu_calls;assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==-1);
  assert(cal5_mcu_calls==calls && !memcmp(&retained,&m->bulk_failure,sizeof retained) && !freed);
  cal5_mcu_finish();
 }
 puts("xhci-cal5-mcu-observations: PASS9 scenario groups;nested same/MAX/refused/admitted other owner,invalidRAMguard,bounded rawevent/drop/saturation/lateimmutability,actual emitted drop/seededoverflow/invalidproducer;diagnostics only,no native acceptance");
}
static void test_wifi_cal5_mcu(void){
 struct xhci_device *d=cal5_mcu_setup(0,0);assert(cal5_mcu_run()==MT76_CAL_OK);
 struct xhci_wifi_mcu *m=&d->firmware.mcu;
 assert(wifi_cal5.cal.complete && wifi_cal5.cal.restored_mask==3 && wifi_cal5.commands==5 && cal5_mcu_calls==10 && !freed && !d->ep0_quarantined);
 assert(cal5_mcu_alc==0x2f2f1b1b && cal5_mcu_ibi==UINT32_MAX);
 assert(m->bulk_sequence==10 && !m->bulk_failure_valid && m->last_bulk.reason==WIFI_MCU_BULK_SUCCESS);
 assert(m->last_bulk.sequence==10 && m->last_bulk.dci==11 && m->last_bulk.requested==MT76_MCU_RESPONSE_MAX);
 assert(m->last_bulk.completion==13 && m->last_bulk.remaining==MT76_MCU_RESPONSE_MAX-16 && m->last_bulk.flags==2 && !m->last_bulk.endpoint_valid);
 cal5_mcu_finish();
 for(unsigned f=1;f<=12;f++){
  unsigned target=f==12?9:10;d=cal5_mcu_setup(f,target);assert(cal5_mcu_run()!=MT76_CAL_OK);m=&d->firmware.mcu;
  assert(!wifi_cal5.cal.complete && wifi_cal5.commands==5 && cal5_mcu_calls==target && !freed);
  assert(cal5_mcu_alc==0x2f2f1b1b && cal5_mcu_ibi==UINT32_MAX);
  if(f<=10){
   const struct xhci_wifi_mcu_bulk_receipt *r=&m->bulk_failure;
   assert(m->bulk_failure_valid && m->failed && d->firmware.failed && d->ep0_quarantined);
   assert(r->sequence==10 && r->dci==11 && r->requested==MT76_MCU_RESPONSE_MAX && r->ring==m->response.ring_phys);
   assert(r->trb==m->response.ring_phys+4*16 && r->endpoint_valid && r->endpoint_state==1);
   assert(r->ring_observation.valid && r->ring_observation.index==4 && r->ring_observation.producer_index==5);
   assert(r->ring_observation.expected_parameter==m->buffer_phys && r->ring_observation.expected_cycle==1);
   assert(r->ring_observation.submitted_parameter==m->buffer_phys && r->ring_observation.submitted_status==1024);
   assert(r->ring_observation.link_parameter==m->response.ring_phys && r->ring_observation.link_control==((6u<<10)|3u));
   assert(r->events.valid && r->events.slot==1 && !r->events.overflow && !r->events.dropped);
   assert(r->events.seen==(f==1 || f==7 || f==8?0u:1u) && r->events.transfers==r->events.seen && r->events.samples==r->events.seen);
   assert(r->events.exact==(f==2 || f==5 || f==6 || f==10?1u:0u));
   if(r->events.samples){
    assert(r->events.sample[0].parameter_low==(uint32_t)(r->trb+(f==3?16:0)));
    if(f==2)assert(!r->events.sample[0].status);
    if(f==9)assert((r->events.sample[0].control>>24)==2);
   }
   assert(r->event_start==9 && r->event_start_cycle==1 && r->event_end==(f==1 || f==7 || f==8?9u:10u) && r->event_end_cycle==1);
   unsigned reason=(f==2 || f==5 || f==6 || f==10)?WIFI_MCU_BULK_COMPLETION_FAILED:f==7?WIFI_MCU_BULK_STALLED:f==8?WIFI_MCU_BULK_CLOCK_REVERSED:WIFI_MCU_BULK_ELAPSED;
   assert(r->reason==reason && r->polls && r->dispatched==(f==1 || f==7 || f==8?0u:1u));
   if(reason==WIFI_MCU_BULK_COMPLETION_FAILED){assert(r->flags==30 && !m->pending && m->done);if(f==2)assert(!r->completion);}
   else {assert(r->flags==29 && m->pending && !m->done && !r->completion && !r->remaining);}
   if(reason==WIFI_MCU_BULK_ELAPSED)assert(r->elapsed_us>=500000 && r->elapsed_us<500200);
   if(reason==WIFI_MCU_BULK_STALLED)assert(!r->elapsed_us && r->polls==XHCI_TRANSFER_TIMEOUT);
   struct xhci_wifi_mcu_bulk_receipt frozen=*r;
   uint64_t in_count=m->response.enqueue,out_count=d->firmware.ep.enqueue;
   assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==-1);
   assert(m->bulk_sequence==11 && m->last_bulk.reason==WIFI_MCU_BULK_REFUSED && !m->last_bulk.polls && !m->last_bulk.dispatched && !m->last_bulk.trb && !m->last_bulk.ring && !m->last_bulk.endpoint_valid);
   assert(!memcmp(&frozen,&m->bulk_failure,sizeof frozen) && m->response.enqueue==in_count && d->firmware.ep.enqueue==out_count && cal5_mcu_calls==target);
   if(m->pending){
    struct xhci_trb late={.parameter_low=(uint32_t)m->trb,.parameter_high=m->trb>>32,.status=(13u<<24)|(MT76_MCU_RESPONSE_MAX-16),.control=(32u<<10)|(11u<<16)|((uint32_t)d->slot_id<<24)};
    dispatch_transfer_event(&late);assert(!m->pending && m->done && m->failed && d->ep0_quarantined);
    assert(!wifi_mcu_event(d,&late) && !memcmp(&frozen,&m->bulk_failure,sizeof frozen));
   }
  }else{
   /* A completed USB transfer with bad firmware content,or loss of the RF
    * parent after OUT,must NOT fabricate a USB timeout or admit the next IN. */
   assert(!m->bulk_failure_valid && !m->failed && !d->ep0_quarantined && m->last_bulk.reason==WIFI_MCU_BULK_SUCCESS);
   assert(m->last_bulk.sequence==target && m->last_bulk.dci==(f==12?16u:11u) && !m->last_bulk.endpoint_valid);
   assert(!m->last_bulk.ring_observation.valid && m->last_bulk.events.valid && m->last_bulk.events.exact==1);
  }
  assert(!wifi_cal5_command(&wifi_cal5,MT76_POWERON_RXDCOC,1) && cal5_mcu_calls==target);
  cal5_mcu_finish();
 }
 /* Every command has separate OUT/IN timeout coverage through the same real
  * callback; a failed early command cannot leak into later fixed commands. */
 for(unsigned target=1;target<=10;target++){
  d=cal5_mcu_setup(1,target);assert(cal5_mcu_run()!=MT76_CAL_OK);m=&d->firmware.mcu;
  assert(m->bulk_failure_valid && m->bulk_failure.reason==WIFI_MCU_BULK_ELAPSED && m->bulk_failure.sequence==target);
  assert(cal5_mcu_calls==target && wifi_cal5.commands==(target+1)/2 && !freed && d->ep0_quarantined);
  cal5_mcu_finish();
 }
 d=cal5_mcu_setup(0,0);m=&d->firmware.mcu;d->firmware.ready=false;
 assert(wifi_mcu_bulk(d,true,32)==-1 && !cal5_mcu_calls && !m->pending && !m->failed && !d->ep0_quarantined && !freed);
 assert(m->bulk_failure_valid && m->bulk_sequence==1 && m->last_bulk.reason==WIFI_MCU_BULK_REFUSED && m->last_bulk.requested==32);
 assert(!m->last_bulk.trb && !m->last_bulk.ring && !m->last_bulk.endpoint_valid && !m->last_bulk.polls && !m->last_bulk.dispatched);
 assert(!m->last_bulk.ring_observation.valid && !m->last_bulk.events.valid && !m->last_bulk.events.seen);
 assert(!m->response.enqueue && !d->firmware.ep.enqueue);
 cal5_mcu_finish();
 puts("xhci-cal5-mcu: PASS24 actual5GHz engine/command/bulk/poll/event cases;explicit timeout-vs-invalidCC0,wrong event,clock/gate/refusal,firstfailure immutability,retained DMA/no retry;RFparent/registers modeled,hardware acceptance=0");
 test_wifi_cal5_mcu_lifetime();
 test_wifi_cal5_mcu_observations();
}

static void trace_wifi_cal5_mcu(unsigned fault,unsigned target){
 assert(fault<=15 && ((!fault && !target) || (fault && target>=1 && target<=10)));
 assert(fault<=12 || target==10);
 cal5_mcu_setup(fault,target);
 enum mt76_calibration_status status=cal5_mcu_run();
 assert((status==MT76_CAL_OK)==!fault);
 cal5_mcu_finish();
}
