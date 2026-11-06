/* Dedicated OUT04/DCI8 ownership preview. Empty ring only,no TD/doorbell or
 * MAC start. Configure -> stopped -> disabled;shared input bytes restored.
 * Disabled endpoint reserved fields are not software-cleared or presumed zero. */
static struct {
 struct xhci_device *d;
 uint64_t ring_phys,pin_phys;uintptr_t pin_pointer;struct xhci_trb *ring;
 uint8_t input[4096],expected[4096];struct mt76_peer5 peer;
 uint32_t other[4][8];
 unsigned phase,commands,controls,stop_events;
 bool attempted,active,failed,semantic,configured,stopped,disabled,input_restored,stopping;
}wifi_tx5;
static bool wifi_tx5_fail(void){wifi_tx5.failed=true;return wifi_tx5.d?wifi_mcu_fail(wifi_tx5.d):false;}
static bool wifi_tx5_owned(void){
 struct xhci_device *d=wifi_tx5.d;
 if(!wifi_tx5_requested || !wifi_tx5.active || wifi_tx5.failed || !d || d!=wifi_cal5.d || wifi_tx5.phase>7 ||
 !wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) || !wifi_peer5.binding.complete ||
 !wifi_handoff5_equal(&wifi_peer5.binding,&wifi_tx5.peer,sizeof wifi_tx5.peer) || !wifi_tx5.ring_phys ||
 wifi_tx5.ring_phys!=wifi_tx5.pin_phys || (uintptr_t)wifi_tx5.ring!=wifi_tx5.pin_pointer || wifi_tx5.ring!=pmm_phys_to_virt(wifi_tx5.ring_phys) || !wifi_handoff5_equal(d->input_context,wifi_tx5.expected,4096))return false;
 const unsigned dci[]={0,9,11,16};
 for(unsigned n=0;n<4;n++)for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,dci[n],j)!=wifi_tx5.other[n][j])return false;
 for(unsigned i=0;i<XHCI_TRB_COUNT;i++){
  const struct xhci_trb *t=&wifi_tx5.ring[i];bool link=i==XHCI_TRB_COUNT-1;
  if(t->parameter_low!=(link?(uint32_t)wifi_tx5.ring_phys:0) || t->parameter_high!=(link?(uint32_t)(wifi_tx5.ring_phys>>32):0) || t->status ||
     t->control!=(link?((XHCI_TRB_TYPE_LINK<<10)|3):0))return false;
 }
 if(wifi_tx5.phase>=2 && wifi_tx5.phase<6){
  const uint32_t expected[]={wifi_tx5.phase<4?1:3,(3u<<1)|(2u<<3)|(512u<<16),(uint32_t)(wifi_tx5.ring_phys|1),(uint32_t)(wifi_tx5.ring_phys>>32),124,0,0,0};
  for(unsigned i=0;i<8;i++)if(context_read32(d->output_context,8,i)!=expected[i])return false;
 }
 unsigned state=context_read32(d->output_context,8,0)&7;
 return state==(wifi_tx5.phase<2?0:wifi_tx5.phase<4?1:wifi_tx5.phase<6?3:0);
}
static bool wifi_tx5_event(struct xhci_device *d,const struct xhci_trb *e){
 if(!wifi_tx5.active || d!=wifi_tx5.d || ((e->control>>16)&31)!=8)return false;
 uint64_t pointer=e->parameter_low|((uint64_t)e->parameter_high<<32);
 const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 bool ok=wifi_tx5.stopping && wifi_tx5.phase==3 && wifi_tx5.commands==2 && !wifi_tx5.stop_events &&
 firmware_command_exact && !firmware_command_failed && !wifi_tx5.failed &&
 ((e->control>>10)&63)==32 && (e->control>>24)==d->slot_id && !(e->control&~allowed) &&
 e->status==(27u<<24) && pointer==wifi_tx5.ring_phys && wifi_tx5.ring_phys==wifi_tx5.pin_phys &&
 (uintptr_t)wifi_tx5.ring==wifi_tx5.pin_pointer && wifi_handoff5_equal(d->input_context,wifi_tx5.expected,4096) &&
 wifi_handoff5_equal(&wifi_peer5.binding,&wifi_tx5.peer,sizeof wifi_tx5.peer);
 wifi_tx5.stop_events++;
 console_printf("wifi-tx5-stop-event: phase=%u slot=%u dci=8 pointer=%x cc=%u residue=%u ed=%u ok=%u\n",(uint64_t)wifi_tx5.phase,(uint64_t)(e->control>>24),pointer,(uint64_t)(e->status>>24),(uint64_t)(e->status&0xffffff),(uint64_t)((e->control>>2)&1),(uint64_t)ok);
 if(!ok)wifi_tx5_fail();
 return true;
}
static void wifi_tx5_context(unsigned op){
 struct xhci_device *d=wifi_tx5.d;
 console_printf("wifi-tx5-context: op=%u w0=%x w1=%x w2=%x w3=%x w4=%x w5=%x w6=%x w7=%x\n",(uint64_t)op,(uint64_t)context_read32(d->output_context,8,0),(uint64_t)context_read32(d->output_context,8,1),(uint64_t)context_read32(d->output_context,8,2),(uint64_t)context_read32(d->output_context,8,3),(uint64_t)context_read32(d->output_context,8,4),(uint64_t)context_read32(d->output_context,8,5),(uint64_t)context_read32(d->output_context,8,6),(uint64_t)context_read32(d->output_context,8,7));
}
/* Failure-only read-only receipt. It never changes admission or issues a
 * recovery command;retain the first failed owner even if a later read differs. */
static void wifi_tx5_diagnostic(unsigned op){
 struct xhci_device *d=wifi_tx5.d;
 if(!d || !d->input_context || !d->output_context){console_write("wifi-tx5-diagnostic: context_unavailable=1\n");return;}
 wifi_tx5_context(wifi_tx5.phase);
 bool parent=wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window);
 bool peer=wifi_handoff5_equal(&wifi_peer5.binding,&wifi_tx5.peer,sizeof wifi_tx5.peer);
 bool input=wifi_handoff5_equal(d->input_context,wifi_tx5.expected,4096);
 console_printf("wifi-tx5-diagnostic: op=%u phase=%u parent=%u peer=%u input=%u failed=%u command_failed=%u phys=%x pin=%x pointer=%u\n",(uint64_t)op,(uint64_t)wifi_tx5.phase,(uint64_t)parent,(uint64_t)peer,(uint64_t)input,(uint64_t)wifi_tx5.failed,(uint64_t)firmware_command_failed,wifi_tx5.ring_phys,wifi_tx5.pin_phys,(uint64_t)((uintptr_t)wifi_tx5.ring==wifi_tx5.pin_pointer));
 const unsigned dci[]={0,9,11,16};
 for(unsigned n=0;n<4;n++)for(unsigned j=0;j<8;j++){
  uint32_t value=context_read32(d->output_context,dci[n],j);
  if(value!=wifi_tx5.other[n][j])console_printf("wifi-tx5-other-diff: dci=%u word=%u expected=%x observed=%x\n",(uint64_t)dci[n],(uint64_t)j,(uint64_t)wifi_tx5.other[n][j],(uint64_t)value);
 }
}
static bool wifi_tx5_command(unsigned op){
 struct xhci_device *d=wifi_tx5.d;
 if(op!=wifi_tx5.phase || !(op==1 || op==3 || op==5) || !wifi_tx5_owned())return wifi_tx5_fail();
 unsigned type=op==3?15:XHCI_TRB_TYPE_CONFIGURE_ENDPOINT;
 struct xhci_trb command={.control=(type<<10)|((uint32_t)d->slot_id<<24)};
 if(op==3)command.control|=8u<<16;
 else {command.parameter_low=d->input_context_phys;command.parameter_high=d->input_context_phys>>32;}
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;wifi_tx5.commands++;
 wifi_tx5.stopping=op==3;
 bool ok=submit_command_trb(&command,type,&slot) && slot==d->slot_id;wifi_tx5.stopping=false;firmware_command_exact=false;
 if(op==3)ok=ok && wifi_tx5.stop_events==1 && !wifi_tx5.failed;
 if(!ok)firmware_command_failed=true;
 if(ok)wifi_tx5.phase++;
 event_irq_restore(flags);
 console_printf("wifi-tx5-command: op=%u type=%u slot=%u dci=8 ok=%u\n",(uint64_t)op,(uint64_t)type,slot,(uint64_t)ok);
 bool owned=ok && wifi_tx5_owned();
 if(ok && !owned)wifi_tx5_diagnostic(op);
 return owned?true:wifi_tx5_fail();
}
static bool wifi_tx5_inspect(void){
 if(!wifi_tx5_requested || wifi_tx5.attempted)return false;
 wifi_tx5.attempted=true;wifi_tx5.d=wifi_cal5.d;struct xhci_device *d=wifi_tx5.d;
 if(!wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) || !wifi_peer5.binding.complete || context_read32(d->output_context,8,0)&7)return false;
 const unsigned dci[]={0,9,11,16};
 for(unsigned n=0;n<4;n++)for(unsigned j=0;j<8;j++)wifi_tx5.other[n][j]=context_read32(d->output_context,dci[n],j);
 if((wifi_tx5.other[0][0]>>27)!=16)return false;
 for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,8,j))return false;
 void *p;if(!alloc_frame_zero(&wifi_tx5.ring_phys,&p))return false;wifi_tx5.ring=p;wifi_tx5.pin_phys=wifi_tx5.ring_phys;wifi_tx5.pin_pointer=(uintptr_t)p;write_link_trb(p,wifi_tx5.ring_phys,true);
 const uint64_t used[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys,wifi_cal5.plan.ring,wifi_handoff5.plan.new_ring};
 for(unsigned i=0;i<sizeof used/sizeof used[0];i++)if(wifi_tx5.ring_phys==used[i])return false;
 for(unsigned i=0;i<8;i++)if(wifi_tx5.ring_phys==wifi_cal5.plan.physical[i])return false;
 copy_bytes(wifi_tx5.input,d->input_context,4096);copy_bytes(wifi_tx5.expected,d->input_context,4096);copy_bytes((uint8_t *)&wifi_tx5.peer,(const uint8_t *)&wifi_peer5.binding,sizeof wifi_tx5.peer);
 wifi_tx5.active=true;bool ok=wifi_tx5_owned();
 console_write("wifi-tx5-progress: dedicated OUT04 empty ring;configure stop disable;preserve RX/MCU/slot and restore input;no TD/doorbell/MAC/network OUT\n");wifi_tx5_context(0);
 if(ok){
  zero_bytes(d->input_context,4096);context_write32(d->input_context,0,1,1u|(1u<<8));
  for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_tx5.other[0][j]);
  context_write32(d->input_context,9,1,(3u<<1)|(2u<<3)|(512u<<16));context_write32(d->input_context,9,2,wifi_tx5.ring_phys|1);context_write32(d->input_context,9,3,wifi_tx5.ring_phys>>32);context_write32(d->input_context,9,4,124);
  copy_bytes(wifi_tx5.expected,d->input_context,4096);wifi_tx5.phase=1;ok=wifi_tx5_command(1);wifi_tx5.configured=ok;
 }
 if(ok){
  wifi_tx5_context(2);
  const uint32_t expected[]={1,(3u<<1)|(2u<<3)|(512u<<16),(uint32_t)(wifi_tx5.ring_phys|1),(uint32_t)(wifi_tx5.ring_phys>>32),124,0,0,0};
  for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,8,j)!=expected[j])ok=wifi_tx5_fail();
 }
 if(ok)for(unsigned i=0;i<15;i++){
  unsigned g=i<3?i:i>=12?i-12:3;uint32_t a=g==0?0x1004:g==1?0x2000:g==2?0x208:wifi_power5.plan.write[i-3].address,v=0;
  uint32_t wanted=g==0?0:g==1?0xf000f200:g==2?0x40000050:wifi_power5.cycle.chosen[i-3];
  bool read=wifi_tx5_owned() && wifi_mac_private_usb(d,false,a,&v);wifi_tx5.controls++;
  console_printf("wifi-tx5-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)i,(uint64_t)a,(uint64_t)v,(uint64_t)(read && v==wanted));
  if(!read || !wifi_tx5_owned() || (g<3 && v!=wanted)){ok=wifi_tx5_fail();break;}
  if(v!=wanted)wifi_tx5.semantic=true;
 }
 if(ok){wifi_tx5.phase=3;ok=wifi_tx5_command(3);wifi_tx5.stopped=ok;if(ok)wifi_tx5_context(4);}
 if(ok){
  zero_bytes(d->input_context,4096);context_write32(d->input_context,0,0,1u<<8);context_write32(d->input_context,0,1,1);
  for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_tx5.other[0][j]);
  copy_bytes(wifi_tx5.expected,d->input_context,4096);wifi_tx5.phase=5;ok=wifi_tx5_command(5);wifi_tx5.disabled=ok;if(ok)wifi_tx5_context(6);
 }
 if(ok){copy_bytes(d->input_context,wifi_tx5.input,4096);copy_bytes(wifi_tx5.expected,wifi_tx5.input,4096);wifi_tx5.phase=7;wifi_tx5.input_restored=wifi_tx5_owned();ok=wifi_tx5.input_restored && !wifi_tx5.semantic;}
 console_printf("wifi-tx5: ok=%u configured=%u stopped=%u disabled=%u input_restored=%u semantic=%u commands=%u controls=%u phase=%u ring=%x stop_events=%u others=4 retained=1 posted=0 doorbells=0 network_out=0\n",(uint64_t)ok,(uint64_t)wifi_tx5.configured,(uint64_t)wifi_tx5.stopped,(uint64_t)wifi_tx5.disabled,(uint64_t)wifi_tx5.input_restored,(uint64_t)wifi_tx5.semantic,(uint64_t)wifi_tx5.commands,(uint64_t)wifi_tx5.controls,(uint64_t)wifi_tx5.phase,wifi_tx5.ring_phys,(uint64_t)wifi_tx5.stop_events);
 wifi_tx5.active=false;return ok;
}
bool xhci_wifi_capture_tx5(void){if(wifi_tx5_requested || wifi_tx5.attempted)return false;wifi_tx5_requested=true;bool ok=xhci_wifi_capture_peer5();wifi_tx5_requested=false;return ok;}
