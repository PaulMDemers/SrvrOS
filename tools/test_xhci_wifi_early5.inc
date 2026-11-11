/* HCD/register values and fixed RF parent are modeled. Configure, publication,
 * poll/dispatch, strict EP0, calibration, Stop and quarantine are native code. */
static unsigned early5_producer,early5_mcu_calls,early5_control_calls,early5_rx_bells;
static bool early5_cycle,early5_timeout,early5_control_inject,early5_stop_race,early5_stop_length_invalid;
static unsigned early5_fault,early5_stop_fault,early5_owner_fault,early5_control_fault;
static bool early5_history_requested,early5_history_active;
static uint32_t early5_mac,early5_filter,early5_txop;
static void early5_queue(struct xhci_trb e){
 assert(((xhci.event_ring[early5_producer].control&1)!=0)!=early5_cycle);
 e.control=(e.control&~1u)|(early5_cycle?1u:0u);
 xhci.event_ring[early5_producer]=e;
 if(++early5_producer==64){early5_producer=0;early5_cycle=!early5_cycle;}
}
static void early5_data(unsigned mutation){
 struct xhci_device *d=cal5_mcu_test_device;
 uint64_t p=wifi_early5.ring+16*wifi_early5.completed;
 struct xhci_trb e={.parameter_low=(uint32_t)p,.parameter_high=p>>32,
  .status=(13u<<24)|4000,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 if(mutation==1)e.control=(e.control&0xffffffu)|(2u<<24);
 if(mutation==2)e.control=(e.control&~(31u<<16))|(11u<<16);
 if(mutation==3)e.parameter_low+=16;
 if(mutation==4)e.control|=4;
 if(mutation==5)e.status=6u<<24;
 if(mutation==6)e.status=(13u<<24)|4097;
 if(mutation==7)e.parameter_low-=16;
 early5_queue(e);
}
static void early5_dispatch_one(void){struct xhci_trb e;assert(poll_event(&e));dispatch_transfer_event(&e);}
static void early5_test_doorbell(uint64_t offset,unsigned value){
 struct xhci_device *d=cal5_mcu_test_device;
 if(offset==xhci.db_base && !value){
  unsigned ci=(xhci.command_enqueue+62)%63;const struct xhci_trb *cmd=&xhci.command_ring[ci];
  unsigned type=(cmd->control>>10)&63;
  if(type==12){
   for(unsigned i=0;i<5;i++)context_write32(d->output_context,9,i,context_read32(d->input_context,10,i));
   context_write32(d->output_context,9,0,1);
  }else{
   assert(type==15 && ((cmd->control>>16)&31)==9);
   unsigned n=wifi_early5.completed;
   if(early5_stop_race && n<32){early5_data(0);n++;}
   context_write32(d->output_context,9,0,3);
   uint64_t ptr=wifi_early5.ring+16*n;
   context_write32(d->output_context,9,2,(uint32_t)(ptr|1));
   context_write32(d->output_context,9,3,ptr>>32);
   if(early5_stop_fault==4)context_write32(d->output_context,9,2,(uint32_t)((ptr+16)|1));
   if(early5_stop_fault==5)context_write32(d->output_context,9,0,1);
   struct xhci_trb stop={.parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,
    .status=n<32?(26u<<24)|4096:27u<<24,
    .control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
   if(early5_stop_fault==6)stop.parameter_low+=16;
   if(early5_stop_fault==7)stop.status=(27u<<24)|1;
   if(early5_stop_length_invalid)stop.status=27u<<24;
   if(early5_stop_fault!=1 && early5_stop_fault!=2)early5_queue(stop);
  }
  uint64_t p=xhci.command_ring_phys+16*ci;
  early5_queue((struct xhci_trb){.parameter_low=(uint32_t)p,.parameter_high=p>>32,
   .status=(early5_stop_fault==3?5u:1u)<<24,.control=(33u<<10)|((uint32_t)d->slot_id<<24)});
  if(type==15 && early5_stop_fault==2){
   uint64_t ptr=wifi_early5.ring+16*wifi_early5.completed;
   early5_queue((struct xhci_trb){.parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,
    .status=(26u<<24)|4096,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)});
  }
  return;
 }
 if(offset!=xhci.db_base+4*d->slot_id)return;
 if(value==9){early5_rx_bells++;assert(wifi_early5_layout());return;}
 if(value==11 || value==16){
  struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned step=wifi_cal5.commands-1;
  assert(wifi_early5.armed && step<5 && m->pending);
  early5_mcu_calls++;
  if(early5_owner_fault){
   struct xhci_wifi_mcu_bulk_receipt *r=wifi_mcu_observer.receipt;assert(r);
   switch(early5_owner_fault){
    case 1:r->published_slot++;break;case 2:r->sequence++;break;
    case 3:r->trb+=16;break;case 4:r->dci=9;break;
    case 5:r->requested--;break;case 6:r->published_ring=NULL;break;
    case 7:d->input_context_phys+=4096;break;case 8:xhci.controller_owned=false;break;
    default:assert(!"unexpected early5 MCU owner mutation");
   }
  }
  if(wifi_early5.completed<32)early5_data(early5_fault);
  if(value==11 && early5_timeout)return;
  if(early5_history_active){
   cal5_mcu_lifetime.event_index=early5_producer;cal5_mcu_lifetime.event_cycle=early5_cycle;
   cal5_mcu_lifetime_bulk(value);
   early5_producer=cal5_mcu_lifetime.event_index;early5_cycle=cal5_mcu_lifetime.event_cycle;
   return;
  }
  if(value==11){
   static const uint32_t ids[5]={1,12,255,3,2},args[5]={0,161,0x901,1,1};
   uint32_t words[4]={0x4a000008u|((step+1)<<16),ids[step],args[step],0x4a000008u|((step+1)<<16)};
   for(unsigned i=0;i<16;i++)m->buffer[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
  }
  early5_queue((struct xhci_trb){.parameter_low=(uint32_t)m->trb,.parameter_high=m->trb>>32,
   .status=value==16?1u<<24:(13u<<24)|1008,
   .control=(32u<<10)|(value<<16)|((uint32_t)d->slot_id<<24)});
  return;
 }
 if(value==1){
  early5_control_calls++;
  uint64_t setup=strict_control.setup;const struct xhci_trb *s=&d->ep0_ring[(setup-d->ep0_ring_phys)/16];
  uint64_t request=s->parameter_low|((uint64_t)s->parameter_high<<32);
  unsigned address=(request>>32)&65535;bool in=(request&0x80)!=0;
  uint32_t *v=address==0x13b0?&cal5_mcu_alc:address==0x2124?&cal5_mcu_ibi:
   address==0x1004?&early5_mac:address==0x1400?&early5_filter:address==0x1340?&early5_txop:NULL;
  uint32_t word=v?*v:address==0x2000?0xf000f200:address==0x208?0x40000050:0;
  if(in)for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=(uint8_t)(word>>(8*i));
  else{
   assert(v);*v=0;for(unsigned i=0;i<4;i++)*v|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
  }
  if(early5_control_inject && early5_control_calls%5==0 && wifi_early5.completed<32){
   if(early5_control_fault==1)wifi_early5.control_active=false;
   if(early5_control_fault==2)wifi_early5.control_setup^=0x100000000ull;
   early5_data(0);
  }
  early5_queue((struct xhci_trb){.parameter_low=(uint32_t)strict_control.status,.parameter_high=strict_control.status>>32,
   .status=1u<<24,.control=(32u<<10)|(1u<<16)|((uint32_t)d->slot_id<<24)});
 }
}
static struct xhci_device *early5_setup_limit(unsigned limit){
 struct xhci_device *d=cal5_mcu_setup(0,0);
 if(early5_history_requested){
  memset(&cal5_mcu_lifetime,0,sizeof cal5_mcu_lifetime);
  cal5_mcu_lifetime.prefix=true;
  cal5_mcu_lifetime.cycle[0]=cal5_mcu_lifetime.cycle[1]=cal5_mcu_lifetime.event_cycle=true;
  cal5_mcu_lifetime_active=true;
  size_t size=mt76_mcu_select_response_queue(d->firmware.buffer,MT76_MCU_PACKET_MAX);assert(size==16);
  for(unsigned i=0;i<3;i++)assert(wifi_mcu_bulk(d,false,size)==16);
  for(unsigned i=0;i<1906;i++){
   assert(mt76_poweron5_packet(0,d->firmware.buffer,16)==16);
   assert(wifi_mcu_bulk(d,false,16)==16);
   assert(wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX)==16);
  }
  assert(cal5_mcu_lifetime.wraps[0]>=29 && cal5_mcu_lifetime.wraps[1]>=29 && cal5_mcu_lifetime.event_wraps>=59);
  assert(d->firmware.mcu.bulk_sequence==3815 && d->firmware.mcu.response.enqueue==16);
  cal5_mcu_lifetime_active=false;cal5_mcu_lifetime.prefix=false;
 }
 memset(&wifi_early5,0,sizeof wifi_early5);wifi_early5.requested=true;
 early5_history_active=early5_history_requested;
 early5_test_active=true;early5_producer=(unsigned)xhci.event_dequeue;
 early5_mcu_calls=early5_control_calls=early5_rx_bells=0;
 early5_cycle=xhci.event_cycle;early5_timeout=early5_control_inject=early5_stop_race=early5_stop_length_invalid=false;
 early5_fault=early5_stop_fault=early5_owner_fault=early5_control_fault=0;
 early5_mac=0;early5_filter=0x17f97;early5_txop=0x583f;
 wifi_cal5.configured=false;wifi_cal5.mac=(struct mt76_mac_cycle){0};wifi_cal5.calibrating=false;
 memset(&wifi_cal5.ep,0,sizeof wifi_cal5.ep);context_write32(d->output_context,9,0,0);
 if(limit<33)fail_after=pool_used/4096+limit;
 bool configured=wifi_cal5_configure();
 if(limit<33){assert(!configured && !wifi_early5.published && !early5_rx_bells && !freed);return d;}
 assert(configured);assert(wifi_early5.attempted && !wifi_early5.published);
 assert(wifi_early5_publish());assert(wifi_early5.published==32 && early5_rx_bells==1);
 assert(!wifi_cal5.mac.started && !wifi_cal5.posts && !wifi_cal5.window.attempted);
 assert(!wifi_early5_publish() && early5_rx_bells==1);
 return d;
}
static struct xhci_device *early5_setup(void){return early5_setup_limit(33);}
static void early5_finish(void){
 assert(!freed);early5_test_active=early5_history_requested=early5_history_active=false;cal5_mcu_finish();
}
static bool early5_full_cycle(void){
 early5_control_inject=true;
 const struct mt76_mac_cycle_io io={&wifi_cal5,wifi_cal5_owned,wifi_cal5_read,
  wifi_cal5_write,wifi_cal5_delay,wifi_cal5_now,wifi_cal5_inspection};
 return mt76_mac_cycle_run(&wifi_cal5.mac,&io);
}
static void early5_whole_native_child(void){
 struct xhci_device *d=cal5_mcu_setup(0,0);
 memset(&wifi_cal5,0,sizeof wifi_cal5);memset(&wifi_early5,0,sizeof wifi_early5);
 wifi_early5.requested=true;wifi_channel_vco5.d=d;context_write32(d->output_context,9,0,0);
 early5_test_active=true;early5_producer=early5_mcu_calls=early5_control_calls=early5_rx_bells=0;
 early5_cycle=true;early5_control_inject=true;
 early5_timeout=early5_stop_race=early5_stop_length_invalid=false;
 early5_fault=early5_stop_fault=early5_owner_fault=early5_control_fault=0;
 early5_mac=0;early5_filter=0x17f97;early5_txop=0x583f;
 assert(wifi_cal5_inspect(&wifi_channel_vco5));
 assert(!wifi_cal5.active && wifi_early5.frozen && wifi_cal5.commands==5);
 assert(early5_rx_bells==1 && early5_mcu_calls==10 && wifi_early5.during_mcu==10);
 assert(!wifi_cal5.window.attempted && !wifi_cal5.posts && !wifi_cal5.cleanup_posts);
 assert(!wifi_cal5_inspect(&wifi_channel_vco5));early5_finish();
}
static void test_wifi_early5(void){
 unsigned cases=0;
 early5_whole_native_child();cases++;
 struct xhci_device *d=early5_setup();
 assert(early5_full_cycle());
 assert(wifi_cal5.cal.complete && wifi_cal5.commands==5 && early5_mcu_calls==10);
 assert(wifi_early5.during_mcu==10 && wifi_early5.during_control>0);
 assert(!wifi_cal5.window.attempted && !wifi_cal5.posts && !wifi_cal5.cleanup_posts);
 early5_stop_race=true;assert(wifi_cal5_stop());assert(wifi_early5_emit(true));
 assert(wifi_early5.frozen && wifi_cal5.stopped && !freed);
 unsigned n=wifi_early5.completed;early5_data(0);early5_dispatch_one();
 assert(wifi_early5.failed && wifi_early5.completed==n && d->ep0_quarantined);
 early5_finish();cases++;

 d=early5_setup();assert(early5_full_cycle());early5_stop_length_invalid=true;
 assert(wifi_cal5_stop() && wifi_early5.frozen && wifi_early5.completed<32);
 assert(wifi_early5.stop_event.status==27u<<24);early5_finish();cases++;

 for(unsigned fault=1;fault<=7;fault++){
  d=early5_setup();
  if(fault==7){early5_data(0);early5_dispatch_one();}
  early5_fault=fault;assert(!early5_full_cycle());
  assert(wifi_early5.failed && d->ep0_quarantined && d->firmware.mcu.failed);
  assert(!wifi_cal5_stop() && !wifi_early5.frozen && early5_rx_bells==1 && early5_mcu_calls==1);
  early5_finish();cases++;
 }
 for(unsigned fault=1;fault<=7;fault++){
  d=early5_setup();assert(early5_full_cycle());early5_stop_fault=fault;
  assert(!wifi_cal5_stop() && wifi_early5.failed && !wifi_early5.frozen);
  assert(d->ep0_quarantined && !freed);early5_finish();cases++;
 }
 for(unsigned fault=1;fault<=8;fault++){
  d=early5_setup();early5_owner_fault=fault;assert(!early5_full_cycle());
  assert(wifi_early5.failed && d->ep0_quarantined && !wifi_cal5_stop());
  early5_finish();cases++;
 }
 for(unsigned fault=1;fault<=2;fault++){
  d=early5_setup();early5_control_fault=fault;assert(!early5_full_cycle());
  assert(wifi_early5.failed && d->ep0_quarantined && !wifi_cal5_stop());
  early5_finish();cases++;
 }
 for(unsigned limit=0;limit<33;limit++){
  d=early5_setup_limit(limit);assert(!wifi_cal5.configured && !wifi_early5.armed);
  assert(!wifi_early5_publish() && !early5_rx_bells && !freed);
  early5_finish();cases++;
 }
 d=early5_setup();early5_timeout=true;
 assert(!early5_full_cycle());
 assert(wifi_cal5.commands==1 && early5_mcu_calls==2 && early5_rx_bells==1);
 assert(d->firmware.mcu.bulk_failure_valid && d->firmware.mcu.bulk_failure.reason==WIFI_MCU_BULK_ELAPSED);
 assert(d->firmware.mcu.loss_trace.late_attempted && !d->firmware.mcu.loss_trace.late_completed);
 assert(d->firmware.mcu.pending && d->ep0_quarantined && !wifi_early5.frozen);
 assert(!wifi_cal5_stop());early5_finish();cases++;

 for(unsigned timeout=0;timeout<2;timeout++){
  early5_history_requested=true;d=early5_setup();early5_timeout=timeout!=0;
  assert(early5_full_cycle()==!timeout);
  if(timeout){
   assert(d->firmware.mcu.bulk_failure.sequence==3817 && d->firmware.mcu.pending);
   assert(d->firmware.mcu.loss_trace.late_attempted && d->ep0_quarantined && !wifi_cal5_stop());
  }else{
   assert(d->firmware.mcu.bulk_sequence==3825 && wifi_early5.during_mcu==10);
   assert(wifi_cal5_stop() && wifi_early5.frozen);
  }
  assert(early5_rx_bells==1 && !freed);early5_finish();cases++;
 }

 d=early5_setup();
 for(unsigned i=0;i<32;i++){early5_data(0);early5_dispatch_one();}
 assert(wifi_early5.completed==32 && !wifi_early5.failed && early5_rx_bells==1);
 assert(early5_full_cycle());assert(wifi_cal5_stop());assert(wifi_early5_emit(true));
 assert(wifi_early5.completed==32 && wifi_early5.stop_event.status==27u<<24);
 early5_finish();cases++;

 d=early5_setup();d->removed=true;early5_data(0);early5_dispatch_one();
 assert(wifi_early5.failed && d->ep0_quarantined && !wifi_cal5_stop());early5_finish();cases++;
 d=early5_setup();wifi_early5.physical[0]=1;early5_data(0);early5_dispatch_one();
 assert(wifi_early5.failed && !wifi_early5.completed && !wifi_cal5_stop());early5_finish();cases++;
 d=early5_setup();context_write32(d->output_context,9,2,0xdeadbeef);
 /* A Running endpoint's dequeue field is undefined, not a progress guard. */
 assert(wifi_cal5_owned(&wifi_cal5,false));early5_data(0);early5_dispatch_one();
 assert(wifi_early5.completed==1 && !wifi_early5.failed);early5_finish();cases++;
 printf("xhci-wifi-early5: %u native-path cases PASS (modeled HCD/RF; no physical acceptance)\n",cases);
}
static void trace_wifi_early5(unsigned mode){
 if(mode==4){early5_whole_native_child();return;}
 early5_setup();
 if(mode==1)early5_timeout=true;
 if(mode==2)for(unsigned i=0;i<32;i++){early5_data(0);early5_dispatch_one();}
 bool ok=early5_full_cycle();assert(ok==(mode!=1));
 if(mode==3)early5_stop_fault=2;
 bool stopped=wifi_cal5_stop();assert(stopped==(mode!=1 && mode!=3));
 assert(wifi_early5_emit(ok && stopped)==(mode!=1 && mode!=3));early5_finish();
}
