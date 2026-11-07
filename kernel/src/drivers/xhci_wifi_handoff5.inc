#include <srvros/mt76_handoff5.h>
static bool wifi_peer5_requested;
static bool wifi_peer5_inspect(void);
/* Empty stopped-ring handoff only. No TD publication,MAC enable or network
 * OUT. Old parent predicates remain unchanged and refuse the fresh context.
 * Static snapshots/frames are retained on every outcome,never recycled. */
static struct {
 struct xhci_device *d;struct mt76_handoff5_plan plan;struct mt76_handoff5 result;
 struct xhci_trb *ring;uint32_t endpoint[8];unsigned op,controls;
 uint64_t command_ring,input,output,ep0,descriptor,new_physical;uintptr_t new_pointer;
 uint8_t cal[sizeof wifi_cal5],power[sizeof wifi_power5],old_ring[4096],old_pages[8][4096];
 bool attempted,active,frozen,failed,inspection_ok;
} wifi_handoff5;
static bool wifi_handoff5_equal(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return false;return true;}
static bool wifi_handoff5_fail(void){wifi_handoff5.failed=true;return wifi_handoff5.d?wifi_mcu_fail(wifi_handoff5.d):false;}
static bool wifi_handoff5_context(void){
 struct xhci_device *d=wifi_handoff5.d;if(!d || !d->output_context)return false;
 uint64_t expected=wifi_handoff5.op<2 || wifi_handoff5.op>=6?wifi_handoff5.plan.old_dequeue:wifi_handoff5.plan.new_ring|1;
 for(unsigned i=0;i<8;i++){
  uint32_t value=i==2?(uint32_t)expected:i==3?(uint32_t)(expected>>32):wifi_handoff5.endpoint[i];
  if(context_read32(d->output_context,9,i)!=value)return false;
 }
 return (context_read32(d->output_context,9,0)&7)==3;
}
static bool wifi_handoff5_owned(void *c,bool cleanup){
 (void)cleanup;struct xhci_device *d=wifi_handoff5.d;
 if(c!=&wifi_handoff5 || !wifi_handoff5_requested || !wifi_handoff5.attempted || !wifi_handoff5.active || !wifi_handoff5.frozen || wifi_handoff5.failed ||
 !d || d!=wifi_cal5.d || d!=wifi_power5.d || !d->present || d->removed || d->retiring || d->ep0_quarantined || d->control_pending || d->control_failed ||
 d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 || !d->slot_id || d->slot_id!=wifi_handoff5.plan.slot ||
 d->firmware.pending || d->firmware.mcu.pending || xhci.stopping || xhci.sync_users || xhci.command_pending || xhci.command_failed || firmware_command_failed || strict_control.device ||
 wifi_handoff5.command_ring!=xhci.command_ring_phys || wifi_handoff5.input!=d->input_context_phys || wifi_handoff5.output!=d->output_context_phys ||
 wifi_handoff5.ep0!=d->ep0_ring_phys || wifi_handoff5.descriptor!=d->descriptor_buffer_phys || wifi_handoff5.op>7 ||
 !mt76_handoff5_plan_valid(&wifi_handoff5.plan) || wifi_handoff5.plan.generation!=wifi_power5.plan.generation ||
 wifi_handoff5.plan.old_ring!=wifi_cal5.plan.ring || wifi_handoff5.plan.old_dequeue!=wifi_cal5.stop_dequeue || wifi_handoff5.plan.old_posts!=wifi_cal5.posts || wifi_handoff5.plan.protected_count!=17 ||
 wifi_handoff5.plan.new_ring!=wifi_handoff5.new_physical || (uintptr_t)wifi_handoff5.ring!=wifi_handoff5.new_pointer ||
 wifi_handoff5.ring!=pmm_phys_to_virt(wifi_handoff5.new_physical) || !wifi_channel_vco5_owned(&wifi_channel_vco5) ||
 !wifi_handoff5_equal(&wifi_cal5,wifi_handoff5.cal,sizeof wifi_cal5) || !wifi_handoff5_equal(&wifi_power5,wifi_handoff5.power,sizeof wifi_power5) ||
 !wifi_handoff5_equal(wifi_cal5.ep.ring,wifi_handoff5.old_ring,4096) || !wifi_handoff5_context())return false;
 const uint64_t protected[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)if(wifi_handoff5.plan.protected_pages[i]!=protected[i])return false;
 for(unsigned i=0;i<8;i++)if(wifi_handoff5.plan.protected_pages[i+9]!=wifi_cal5.plan.physical[i] || !wifi_handoff5_equal(wifi_cal5.plan.buffer[i],wifi_handoff5.old_pages[i],4096))return false;
 /* No producer ever touches the fresh ring in this recipe. Even its link
  * TRB must remain the exact initialized private image. */
 for(unsigned i=0;i<XHCI_TRB_COUNT;i++){
  const struct xhci_trb *t=&wifi_handoff5.ring[i];bool link=i==XHCI_TRB_COUNT-1;
  if(t->parameter_low!=(link?(uint32_t)wifi_handoff5.plan.new_ring:0) || t->parameter_high!=(link?(uint32_t)(wifi_handoff5.plan.new_ring>>32):0) ||
  t->status || t->control!=(link?((XHCI_TRB_TYPE_LINK<<10)|(1u<<1)|1):0))return false;
 }
 return true;
}
static bool wifi_handoff5_read_context(void *c,uint32_t *out){
 unsigned op=wifi_handoff5.op;
 if(!out || !wifi_handoff5_owned(c,op>=4) || !(op==0 || op==2 || op==4 || op==6))return wifi_handoff5_fail();
 for(unsigned i=0;i<8;i++)out[i]=context_read32(wifi_handoff5.d->output_context,9,i);
 console_printf("wifi-handoff5-context: op=%u w0=%x w1=%x w2=%x w3=%x w4=%x w5=%x w6=%x w7=%x\n",(uint64_t)op,(uint64_t)out[0],(uint64_t)out[1],(uint64_t)out[2],(uint64_t)out[3],(uint64_t)out[4],(uint64_t)out[5],(uint64_t)out[6],(uint64_t)out[7]);
 wifi_handoff5.op++;return true;
}
static bool wifi_handoff5_set_dequeue(void *c,uint64_t value){
 unsigned op=wifi_handoff5.op;uint64_t expected=op==1?wifi_handoff5.plan.new_ring|1:wifi_handoff5.plan.old_dequeue;
 if((op!=1 && op!=5) || value!=expected || !wifi_handoff5_owned(c,op==5))return wifi_handoff5_fail();
 struct xhci_device *d=wifi_handoff5.d;struct xhci_trb cmd={.parameter_low=(uint32_t)value,.parameter_high=value>>32,.control=(16u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;
 bool ok=submit_command_trb(&cmd,16,&slot) && slot==d->slot_id;firmware_command_exact=false;
 if(!ok)firmware_command_failed=true;
 if(ok){wifi_handoff5.op++;ok=wifi_handoff5_context();}
 event_irq_restore(flags);
 console_printf("wifi-handoff5-command: op=%u dequeue=%x slot=%u dci=9 ok=%u\n",(uint64_t)op,value,slot,(uint64_t)ok);
 return ok?true:wifi_handoff5_fail();
}
static bool wifi_handoff5_inspection(void *c){
 if(wifi_handoff5.op!=3 || wifi_handoff5.controls || !wifi_handoff5_owned(c,false))return wifi_handoff5_fail();
 bool selected=true;
 for(unsigned i=0;i<15;i++){
  unsigned g=i<3?i:i>=12?i-12:3;
  uint32_t a=g==0?0x1004:g==1?0x2000:g==2?0x208:wifi_power5.plan.write[i-3].address;
  uint32_t expected=g==0?0:g==1?0xf000f200:g==2?0x40000050:wifi_power5.cycle.chosen[i-3],v=0;
  if(wifi_handoff5.controls!=i || !wifi_handoff5_owned(c,false))return wifi_handoff5_fail();
  wifi_handoff5.controls++;bool ok=wifi_mac_private_usb(wifi_handoff5.d,false,a,&v);
  console_printf("wifi-handoff5-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)i,(uint64_t)a,(uint64_t)v,(uint64_t)(ok && v==expected));
  if(!ok || !wifi_handoff5_owned(c,false) || (g<3 && v!=expected))return wifi_handoff5_fail();
  if(v!=expected)selected=false;
 }
 wifi_handoff5.inspection_ok=selected;wifi_handoff5.op++;return selected;
}
static bool wifi_handoff5_inspect(void){
 if(!wifi_handoff5_requested || wifi_handoff5.attempted)return false;
 wifi_handoff5.attempted=true;wifi_handoff5.d=wifi_cal5.d;
 if(!wifi_power5_owned(&wifi_power5,false) || wifi_power5.step!=84 || !wifi_power5.inspected || !wifi_power5.cycle.selected ||
 wifi_power5.cycle.verified_mask!=511 || wifi_power5.cycle.modified_mask!=511 || wifi_cal5.posts!=8 || wifi_cal5.plan.ring!=wifi_cal5.pin_ring)return false;
 struct xhci_device *d=wifi_handoff5.d;void *ring=0;
 wifi_handoff5.plan=(struct mt76_handoff5_plan){.generation=wifi_power5.plan.generation,.old_ring=wifi_cal5.plan.ring,.old_dequeue=wifi_cal5.stop_dequeue,.old_posts=wifi_cal5.posts,.new_slots=XHCI_TRB_COUNT-1,.slot=d->slot_id};
 for(unsigned i=0;i<8;i++)wifi_handoff5.endpoint[i]=context_read32(d->output_context,9,i);
 if((wifi_handoff5.endpoint[0]&7)!=3 || (wifi_handoff5.endpoint[2]|((uint64_t)wifi_handoff5.endpoint[3]<<32))!=wifi_cal5.stop_dequeue)return false;
 if(!alloc_frame_zero(&wifi_handoff5.plan.new_ring,&ring))return false;
 wifi_handoff5.ring=ring;wifi_handoff5.new_physical=wifi_handoff5.plan.new_ring;wifi_handoff5.new_pointer=(uintptr_t)ring;write_link_trb(ring,wifi_handoff5.plan.new_ring,true);
 const uint64_t protected[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)wifi_handoff5.plan.protected_pages[wifi_handoff5.plan.protected_count++]=protected[i];
 for(unsigned i=0;i<8;i++)wifi_handoff5.plan.protected_pages[wifi_handoff5.plan.protected_count++]=wifi_cal5.plan.physical[i];
 if(!mt76_handoff5_plan_valid(&wifi_handoff5.plan) || !wifi_power5_owned(&wifi_power5,false))return false;
 wifi_handoff5.command_ring=xhci.command_ring_phys;wifi_handoff5.input=d->input_context_phys;wifi_handoff5.output=d->output_context_phys;wifi_handoff5.ep0=d->ep0_ring_phys;wifi_handoff5.descriptor=d->descriptor_buffer_phys;
 copy_bytes(wifi_handoff5.cal,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);copy_bytes(wifi_handoff5.power,(const uint8_t *)&wifi_power5,sizeof wifi_power5);
 copy_bytes(wifi_handoff5.old_ring,(const uint8_t *)wifi_cal5.ep.ring,4096);
 for(unsigned i=0;i<8;i++)copy_bytes(wifi_handoff5.old_pages[i],wifi_cal5.plan.buffer[i],4096);
 wifi_handoff5.active=wifi_handoff5.frozen=true;
 console_write("wifi-handoff5-progress: frozen stopped parent;fresh empty RX ring;selected power readbacks;exact old context restoration;no TD/MAC start/network OUT\n");
 const struct mt76_handoff5_io io={&wifi_handoff5,wifi_handoff5_owned,wifi_handoff5_read_context,wifi_handoff5_set_dequeue,wifi_handoff5_inspection};
 bool ok=mt76_handoff5_run(&wifi_handoff5.result,&wifi_handoff5.plan,&io);
 ok=ok && wifi_handoff5.op==7 && wifi_handoff5.controls==15 && wifi_handoff5.inspection_ok && wifi_handoff5_owned(&wifi_handoff5,false) && wifi_power5_owned(&wifi_power5,false);
 struct mt76_handoff5 *s=&wifi_handoff5.result;
 if(!ok && s->unsafe && !wifi_handoff5.failed)wifi_handoff5_fail();
 console_printf("wifi-handoff5: ok=%u complete=%u retargeted=%u inspected=%u restored=%u unsafe=%u reads=%u commands=%u controls=%u steps=%u generation=%u old_ring=%x old_dequeue=%x new_ring=%x protected=%u retained=1 posted=0 network_out=0\n",(uint64_t)ok,(uint64_t)s->complete,(uint64_t)s->retargeted,(uint64_t)s->inspected,(uint64_t)s->restored,(uint64_t)s->unsafe,(uint64_t)s->reads,(uint64_t)s->commands,(uint64_t)wifi_handoff5.controls,(uint64_t)wifi_handoff5.op,s->generation,wifi_handoff5.plan.old_ring,wifi_handoff5.plan.old_dequeue,wifi_handoff5.plan.new_ring,(uint64_t)wifi_handoff5.plan.protected_count);
 if(ok && wifi_peer5_requested)ok=wifi_peer5_inspect();
 wifi_handoff5.active=false;return ok;
}
bool xhci_wifi_capture_handoff5(void){
 if(wifi_handoff5_requested || wifi_handoff5.attempted)return false;
 wifi_handoff5_requested=true;bool ok=xhci_wifi_capture_power5();wifi_handoff5_requested=false;return ok;
}
