/* Explicit controller-only child. No USB payload,MAC enable,key or data page.
 * Retain every object on uncertainty;backend command tombstones survive until
 * checked controller stop. Historical v126 owner has already finished. */
static struct {
 struct xhci_device *d;struct mt76_dma5 dma;struct mt76_dma5_noop_lease lease;
 struct {struct xhci_trb event;uint64_t pointer;unsigned cycle;} records[64];
 struct {uint64_t pointer,event_pointer,cc,slot,delta;struct xhci_trb shadow;} sync[3];
 uint32_t contexts[5][8];struct xhci_trb final_stop;
 struct xhci_trb *ring,shadow[64];uint64_t physical,pin,enqueue,epoch;uintptr_t pointer;bool cycle;
 uint8_t input[4096],expected[4096],cal[sizeof wifi_cal5],power[sizeof wifi_power5],handoff[sizeof wifi_handoff5],peer[sizeof wifi_peer5],tx[sizeof wifi_tx5];
 uint8_t old_ring[3][4096],old_pages[8][4096];uint64_t old_physical[3];
 uint32_t unrelated[4][8];uint64_t input_phys,output_phys;void *input_pointer,*output_pointer;
 unsigned slot;bool commanding;unsigned phase,commands,controls,posts,events,stop_events,drain_events;
 bool attempted,active,failed,pending,stopping,configured,stopped,disabled,input_restored,child_complete,retired,empty,parent_complete;
} wifi_nop5;
static bool wifi_nop5_fail(void){wifi_nop5.failed=true;mt76_dma5_fault(&wifi_nop5.dma);return wifi_nop5.d?wifi_mcu_fail(wifi_nop5.d):false;}
static bool wifi_nop5_owned(void *c,uint64_t epoch,unsigned slot){
 struct xhci_device *d=wifi_nop5.d;
 if(c!=&wifi_nop5 || !wifi_nop5.active || wifi_nop5.failed || !d || d!=wifi_cal5.d || epoch!=wifi_nop5.epoch || epoch!=wifi_peer5.binding.generation || slot!=wifi_nop5.slot || d->slot_id!=wifi_nop5.slot || !xhci.operational || !xhci.controller_owned || xhci.stopping || xhci.reclaiming || xhci.reclaimed || xhci.command_failed || firmware_command_failed || !d->present || d->removed || d->retiring || d->slot_disabled || d->ep0_quarantined || d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 || d->control_failed || d->control_pending || strict_control.device || d->firmware.pending || d->firmware.mcu.pending ||
 d->input_context!=wifi_nop5.input_pointer || d->output_context!=wifi_nop5.output_pointer || d->input_context_phys!=wifi_nop5.input_phys || d->output_context_phys!=wifi_nop5.output_phys ||
 !wifi_nop5.physical || wifi_nop5.physical!=wifi_nop5.pin || (uintptr_t)wifi_nop5.ring!=wifi_nop5.pointer || wifi_nop5.ring!=pmm_phys_to_virt(wifi_nop5.physical) ||
 !wifi_handoff5_equal(d->input_context,wifi_nop5.expected,4096) || !wifi_handoff5_equal(wifi_nop5.ring,wifi_nop5.shadow,sizeof wifi_nop5.shadow) ||
 !wifi_handoff5_equal(&wifi_cal5,wifi_nop5.cal,sizeof wifi_cal5) || !wifi_handoff5_equal(&wifi_power5,wifi_nop5.power,sizeof wifi_power5) ||
 !wifi_handoff5_equal(&wifi_handoff5,wifi_nop5.handoff,sizeof wifi_handoff5) || !wifi_handoff5_equal(&wifi_peer5,wifi_nop5.peer,sizeof wifi_peer5) || !wifi_handoff5_equal(&wifi_tx5,wifi_nop5.tx,sizeof wifi_tx5))return false;
 unsigned state=context_read32(d->output_context,8,0)&7;
 if((wifi_nop5.phase==0 && state!=0 && !(wifi_nop5.commanding && state==1)) || (wifi_nop5.phase==1 && state!=1 && state!=3) || (wifi_nop5.phase==2 && state!=3 && !(wifi_nop5.commanding && state==0)) || (wifi_nop5.phase==3 && state!=0))return false;
 if(state==1 || state==3){uint64_t deq=context_read32(d->output_context,8,2)|((uint64_t)context_read32(d->output_context,8,3)<<32);
 if(context_read32(d->output_context,8,0)!=state || context_read32(d->output_context,8,1)!=((3u<<1)|(2u<<3)|(512u<<16)) || context_read32(d->output_context,8,4)!=124 || (deq&14) || (deq&~15ull)<wifi_nop5.physical || (deq&~15ull)>wifi_nop5.physical+63*16)return false;
 for(unsigned i=5;i<8;i++)if(context_read32(d->output_context,8,i))return false;}
 const unsigned indices[]={0,9,11,16};for(unsigned i=0;i<4;i++)for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,indices[i],j)!=wifi_nop5.unrelated[i][j])return false;
 for(unsigned i=0;i<3;i++)if(!wifi_handoff5_equal(pmm_phys_to_virt(wifi_nop5.old_physical[i]),wifi_nop5.old_ring[i],4096))return false;
 for(unsigned i=0;i<8;i++)if(!wifi_handoff5_equal(pmm_phys_to_virt(wifi_cal5.plan.physical[i]),wifi_nop5.old_pages[i],4096))return false;
 return true;
}
static bool wifi_nop5_event(struct xhci_device *d,const struct xhci_trb *e){
 if(!wifi_nop5.active)return false;
 uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);unsigned dci=(e->control>>16)&31;
 if(d!=wifi_nop5.d || dci!=8){if(ptr>=wifi_nop5.physical && ptr<wifi_nop5.physical+4096){wifi_nop5_fail();return true;}return false;}
 const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 bool good=wifi_nop5_owned(&wifi_nop5,wifi_nop5.epoch,d->slot_id) && ((e->control>>10)&63)==32 && (e->control>>24)==d->slot_id && !(e->control&~allowed);
 if(wifi_nop5.stopping){uint64_t deq=context_read32(d->output_context,8,2)|((uint64_t)context_read32(d->output_context,8,3)<<32);
  good=good && !wifi_nop5.pending && !wifi_nop5.stop_events && firmware_command_exact && e->status==(27u<<24) && ptr==wifi_nop5.physical+16 && deq==(ptr|0) && (context_read32(d->output_context,8,0)&7)==3;
  if(good){wifi_nop5.stop_events++;wifi_nop5.final_stop=*e;}
 }else{
  unsigned index=(unsigned)((wifi_nop5.lease.trb-wifi_nop5.physical)/16);
  good=good && wifi_nop5.phase==1 && wifi_nop5.pending && index<63 && ptr==wifi_nop5.lease.trb && e->status==(1u<<24) &&
   wifi_nop5.shadow[index].parameter_low==0 && wifi_nop5.shadow[index].parameter_high==0 && !wifi_nop5.shadow[index].status && wifi_nop5.shadow[index].control==((8u<<10)|XHCI_TRB_IOC|wifi_nop5.lease.cycle);
  if(good)good=mt76_dma5_noop_complete(&wifi_nop5.dma,&wifi_nop5.lease,1,0,timer_monotonic_us());
  if(good){unsigned n=wifi_nop5.events;if(n>=64)return wifi_nop5_fail();wifi_nop5.records[n].event=*e;wifi_nop5.records[n].pointer=wifi_nop5.lease.trb;wifi_nop5.records[n].cycle=wifi_nop5.lease.cycle;wifi_nop5.pending=false;wifi_nop5.events++;}
 }
 if(!good)wifi_nop5_fail();
 return true;
}
static bool wifi_nop5_dispatch(void){
 uint64_t flags=event_irq_save();struct xhci_trb e;unsigned n=0;bool good=true;
#ifdef SRVROS_XHCI_NOP5_TEST
 xhci_test_nop5_progress();
#endif
 while(n++<32 && poll_event(&e)){unsigned type=(e.control>>10)&63;if((type!=32 && type!=33) || (type==33 && !wifi_fence5_native.round)){good=false;break;}dispatch_transfer_event(&e);if(wifi_nop5.failed){good=false;break;}}
 event_irq_restore(flags);return good?true:wifi_nop5_fail();
}
static bool wifi_nop5_delay(void){
#ifdef SRVROS_XHCI_NOP5_TEST
 return true;
#else
 return timer_delay_us(100);
#endif
}
static bool wifi_nop5_send(void){
 uint64_t flags=event_irq_save();bool good=wifi_nop5_owned(&wifi_nop5,wifi_nop5.epoch,wifi_nop5.d->slot_id) && !wifi_nop5.pending && wifi_nop5.phase==1 && wifi_nop5.posts<64 &&
 mt76_dma5_noop_prepare(&wifi_nop5.dma,wifi_nop5.posts+1,timer_monotonic_us(),&wifi_nop5.lease);
 if(good){struct xhci_trb td={.control=(8u<<10)|XHCI_TRB_IOC};unsigned index=(unsigned)wifi_nop5.enqueue;
  good=index<63 && wifi_nop5.lease.trb==wifi_nop5.physical+16*index && wifi_nop5.lease.cycle==wifi_nop5.cycle && mt76_dma5_noop_publish(&wifi_nop5.dma,&wifi_nop5.lease,timer_monotonic_us());
  if(good){wifi_nop5.pending=true;wifi_nop5.posts++;uint64_t ptr=ring_push(wifi_nop5.ring,wifi_nop5.physical,&wifi_nop5.enqueue,&wifi_nop5.cycle,&td);
   wifi_nop5.shadow[index]=td;wifi_nop5.shadow[index].control|=wifi_nop5.lease.cycle;
   if(index==62)write_link_trb(wifi_nop5.shadow,wifi_nop5.physical,wifi_nop5.lease.cycle);
   good=ptr==wifi_nop5.lease.trb;if(good)db_write32(wifi_nop5.d->slot_id,8);
  }
 }
 event_irq_restore(flags);if(!good)return wifi_nop5_fail();
 uint64_t start=timer_monotonic_us(),previous=start;
 for(unsigned i=0;i<8192;i++){if(!wifi_nop5_dispatch() || !wifi_nop5_owned(&wifi_nop5,wifi_nop5.epoch,wifi_nop5.d->slot_id))return wifi_nop5_fail();
  if(!wifi_nop5.pending)return true;
  uint64_t now=timer_monotonic_us();if(now<previous || now-start>=750000 || !wifi_nop5_delay())return wifi_nop5_fail();previous=now;
 }
 return wifi_nop5_fail();
}
static bool wifi_nop5_command(unsigned type){
 struct xhci_device *d=wifi_nop5.d;if(!wifi_nop5_owned(&wifi_nop5,wifi_nop5.epoch,d->slot_id))return wifi_nop5_fail();
 struct xhci_trb cmd={.control=(type<<10)|((uint32_t)d->slot_id<<24)};
 if(type==12){cmd.parameter_low=d->input_context_phys;cmd.parameter_high=d->input_context_phys>>32;}else cmd.control|=8u<<16;
 uint64_t flags=event_irq_save(),slot=0,index=xhci.command_enqueue,events=xhci.command_events;
 if(index>=63 || wifi_nop5.commands>=3){event_irq_restore(flags);return wifi_nop5_fail();}
 uint64_t pointer=xhci.command_ring_phys+16*index;struct xhci_trb shadow=cmd;shadow.control|=xhci.command_cycle?1u:0u;firmware_command_exact=true;wifi_nop5.stopping=type==15;wifi_nop5.commanding=true;wifi_nop5.commands++;
 bool good;
#ifdef SRVROS_XHCI_NOP5_TEST
 good=xhci_test_nop5_command(&cmd,type,&slot);
#else
 good=submit_command_trb(&cmd,type,&slot);
#endif
 good=good && slot==d->slot_id && xhci.command_events==events+1 && xhci.last_completion_code==1 && xhci.last_slot_id==slot && xhci.last_command_pointer==pointer && xhci.last_expected_command_pointer==pointer && wifi_handoff5_equal(&xhci.command_ring[index],&shadow,sizeof shadow);
 unsigned n=wifi_nop5.commands-1;wifi_nop5.sync[n].pointer=pointer;wifi_nop5.sync[n].event_pointer=xhci.last_command_pointer;wifi_nop5.sync[n].cc=xhci.last_completion_code;wifi_nop5.sync[n].slot=slot;wifi_nop5.sync[n].delta=xhci.command_events-events;wifi_nop5.sync[n].shadow=shadow;
 firmware_command_exact=false;wifi_nop5.stopping=false;if(type==15)good=good && wifi_nop5.stop_events==1;
 good=good && wifi_nop5_owned(&wifi_nop5,wifi_nop5.epoch,d->slot_id);wifi_nop5.commanding=false;event_irq_restore(flags);return good?true:wifi_nop5_fail();
}
static bool wifi_nop5_parent_owned(void){
#ifdef SRVROS_XHCI_NOP5_TEST
 return xhci_test_nop5_parent_owned();
#else
 return wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window);
#endif
}
static bool wifi_nop5_guards(void){
 const uint32_t address[]={0x1004,0x2000,0x208},expected[]={0,0xf000f200,0x40000050};
 for(unsigned i=0;i<3;i++){uint32_t value=0;bool read;
 if(!wifi_nop5_parent_owned())return false;
#ifdef SRVROS_XHCI_NOP5_TEST
 read=xhci_test_nop5_guard(address[i],&value);
#else
 read=wifi_mac_private_usb(wifi_nop5.d,false,address[i],&value);
#endif
 if(!wifi_nop5_parent_owned() || !read || value!=expected[i] || !wifi_nop5_parent_owned())return false;
 wifi_nop5.controls++;}return true;
}
static bool wifi_nop5_run(void){
 if(wifi_nop5.attempted || !wifi_nop5_parent_owned() || !wifi_tx5.disabled || !wifi_tx5.input_restored || wifi_tx5.active || wifi_join5_requested || wifi_join5.attempted || wifi_wpa5.attempted)return false;
 wifi_nop5.attempted=true;wifi_nop5.d=wifi_cal5.d;struct xhci_device *d=wifi_nop5.d;wifi_nop5.slot=d?d->slot_id:0;
 if(!d || d->slot_id!=wifi_cal5.plan.slot || (context_read32(d->output_context,8,0)&7) || !wifi_nop5_guards() || xhci.command_enqueue+5+WIFI_FENCE5_CLEANUP_COMMANDS>63 || xhci.command_pending || wifi_fence5_native.bound)return wifi_nop5_fail();
 void *p;if(!alloc_frame_zero(&wifi_nop5.physical,&p))return false;wifi_nop5.ring=p;wifi_nop5.pin=wifi_nop5.physical;wifi_nop5.pointer=(uintptr_t)p;wifi_nop5.cycle=true;wifi_nop5.epoch=wifi_peer5.binding.generation;
 const uint64_t used[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys,xhci.event_ring_phys,wifi_cal5.plan.ring,wifi_handoff5.plan.new_ring,wifi_tx5.ring_phys};
 for(unsigned i=0;i<sizeof used/sizeof *used;i++)if(wifi_nop5.physical==used[i])return wifi_nop5_fail();
 for(unsigned i=0;i<8;i++)if(wifi_nop5.physical==wifi_cal5.plan.physical[i])return wifi_nop5_fail();
 if(!mt76_dma5_init_noop_ring(&wifi_nop5.dma,wifi_nop5.epoch,wifi_nop5.physical,63,timer_monotonic_us()))return wifi_nop5_fail();
 write_link_trb(wifi_nop5.ring,wifi_nop5.physical,true);write_link_trb(wifi_nop5.shadow,wifi_nop5.physical,true);
 copy_bytes(wifi_nop5.input,d->input_context,4096);copy_bytes(wifi_nop5.expected,d->input_context,4096);wifi_nop5.input_pointer=d->input_context;wifi_nop5.output_pointer=d->output_context;wifi_nop5.input_phys=d->input_context_phys;wifi_nop5.output_phys=d->output_context_phys;
 copy_bytes(wifi_nop5.cal,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);copy_bytes(wifi_nop5.power,(const uint8_t *)&wifi_power5,sizeof wifi_power5);copy_bytes(wifi_nop5.handoff,(const uint8_t *)&wifi_handoff5,sizeof wifi_handoff5);copy_bytes(wifi_nop5.peer,(const uint8_t *)&wifi_peer5,sizeof wifi_peer5);copy_bytes(wifi_nop5.tx,(const uint8_t *)&wifi_tx5,sizeof wifi_tx5);
 wifi_nop5.old_physical[0]=wifi_cal5.plan.ring;wifi_nop5.old_physical[1]=wifi_handoff5.plan.new_ring;wifi_nop5.old_physical[2]=wifi_tx5.ring_phys;
 for(unsigned i=0;i<3;i++)copy_bytes(wifi_nop5.old_ring[i],pmm_phys_to_virt(wifi_nop5.old_physical[i]),4096);
 for(unsigned i=0;i<8;i++)copy_bytes(wifi_nop5.old_pages[i],pmm_phys_to_virt(wifi_cal5.plan.physical[i]),4096);
 const unsigned indices[]={0,9,11,16};for(unsigned i=0;i<4;i++)for(unsigned j=0;j<8;j++)wifi_nop5.unrelated[i][j]=context_read32(d->output_context,indices[i],j);
 console_write("wifi-nop5-progress: controller-only63+1 bufferless descriptors;MAC disabled;exact events;asynchronous stop/drain/rebase;no keys/payload/RF\n");
 wifi_nop5.active=true;
 zero_bytes(d->input_context,4096);context_write32(d->input_context,0,1,1u|(1u<<8));for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_nop5.unrelated[0][j]);
 context_write32(d->input_context,9,1,(3u<<1)|(2u<<3)|(512u<<16));context_write32(d->input_context,9,2,wifi_nop5.physical|1);context_write32(d->input_context,9,3,wifi_nop5.physical>>32);context_write32(d->input_context,9,4,124);copy_bytes(wifi_nop5.expected,d->input_context,4096);
 if(!wifi_nop5_command(12) || (context_read32(d->output_context,8,0)&7)!=1)return wifi_nop5_fail();
 wifi_nop5.configured=true;wifi_nop5.phase=1;for(unsigned j=0;j<8;j++)wifi_nop5.contexts[0][j]=context_read32(d->output_context,8,j);
 if(!wifi_fence5_bind(d,&wifi_nop5.dma,wifi_nop5.epoch,&wifi_nop5,wifi_nop5_owned))return wifi_nop5_fail();
 for(unsigned i=0;i<63;i++)if(!wifi_nop5_send())return false;
 if(!wifi_fence5_begin(0))return wifi_nop5_fail();
 uint64_t start=timer_monotonic_us(),previous=start;bool finished=false;
 for(unsigned i=0;i<8192;i++){
  if((wifi_fence5_native.engine.state==MT76_FENCE5_STOP_WAIT || wifi_fence5_native.engine.state==MT76_FENCE5_REBASE_WAIT) && !wifi_nop5_dispatch())return false;
  if(wifi_fence5_native.engine.state==MT76_FENCE5_STOP_CONTEXT || wifi_fence5_native.engine.state==MT76_FENCE5_VERIFY){unsigned n=wifi_fence5_native.engine.state==MT76_FENCE5_STOP_CONTEXT?1:2;for(unsigned j=0;j<8;j++)wifi_nop5.contexts[n][j]=context_read32(d->output_context,8,j);}
  if(!wifi_fence5_step())return wifi_nop5_fail();
  if(!wifi_fence5_native.round){finished=true;break;}
  uint64_t now=timer_monotonic_us();if(now<previous || now-start>=750000 || !wifi_nop5_delay())return wifi_nop5_fail();previous=now;
 }
 if(!finished || wifi_nop5.dma.wraps!=1 || wifi_nop5.enqueue || wifi_nop5.cycle || !wifi_nop5_send())return wifi_nop5_fail();
 if(!wifi_nop5_command(15) || (context_read32(d->output_context,8,0)&7)!=3)return wifi_nop5_fail();
 wifi_nop5.stopped=true;wifi_nop5.phase=2;for(unsigned j=0;j<8;j++)wifi_nop5.contexts[3][j]=context_read32(d->output_context,8,j);
 zero_bytes(d->input_context,4096);context_write32(d->input_context,0,0,1u<<8);context_write32(d->input_context,0,1,1);for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_nop5.unrelated[0][j]);copy_bytes(wifi_nop5.expected,d->input_context,4096);
 if(!wifi_nop5_command(12) || (context_read32(d->output_context,8,0)&7)!=0)return wifi_nop5_fail();
 wifi_nop5.disabled=true;wifi_nop5.phase=3;for(unsigned j=0;j<8;j++)wifi_nop5.contexts[4][j]=context_read32(d->output_context,8,j);
 copy_bytes(d->input_context,wifi_nop5.input,4096);copy_bytes(wifi_nop5.expected,wifi_nop5.input,4096);
 wifi_nop5.input_restored=wifi_nop5_owned(&wifi_nop5,wifi_nop5.epoch,d->slot_id);wifi_nop5.active=false;
 wifi_nop5.child_complete=wifi_nop5.input_restored && wifi_nop5_parent_owned() && wifi_nop5_guards() && wifi_nop5.posts==64 && wifi_nop5.events==64 && !wifi_nop5.pending;
 return wifi_nop5.child_complete?true:wifi_nop5_fail();
}
static void wifi_nop5_emit(void){
 console_printf("wifi-nop5-plan: ring=%x epoch=%u slot=%u dci=8 positions=63 first=0 total=64 tx_pages=0 rx_pages=0\n",wifi_nop5.physical,wifi_nop5.epoch,(uint64_t)(wifi_nop5.d?wifi_nop5.d->slot_id:0));
 for(unsigned i=0;i<wifi_nop5.events && i<64;i++){const struct xhci_trb *e=&wifi_nop5.records[i].event;
 console_printf("wifi-nop5-td: index=%u pointer=%x cycle=%u cc=%u residue=%u event_status=%x event_control=%x bytes=0 page=0\n",(uint64_t)i,wifi_nop5.records[i].pointer,(uint64_t)wifi_nop5.records[i].cycle,(uint64_t)(e->status>>24),(uint64_t)(e->status&0xffffff),(uint64_t)e->status,(uint64_t)e->control);}
 for(unsigned i=0;i<5;i++){const uint32_t *w=wifi_nop5.contexts[i];console_printf("wifi-nop5-context: phase=%u w0=%x w1=%x w2=%x w3=%x w4=%x w5=%x w6=%x w7=%x\n",(uint64_t)i,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);}
 for(unsigned i=0;i<wifi_fence5_native.count && i<2;i++){const struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[i];const struct xhci_trb *e=&r->event,*t=&r->stop;
 uint64_t ep=e->parameter_low|((uint64_t)e->parameter_high<<32),sp=t->parameter_low|((uint64_t)t->parameter_high<<32);
 console_printf("wifi-nop5-fence-command: index=%u pointer=%x epoch=%u token=%u slot=%u dci=%u type=%u dequeue=%x control=%x status=%x event_pointer=%x event_status=%x event_control=%x stop_pointer=%x stop_status=%x stop_control=%x stop_dequeue=%x consumed=%u\n",(uint64_t)i,r->id.pointer,r->id.epoch,r->id.token,(uint64_t)r->id.slot,(uint64_t)r->id.dci,(uint64_t)r->id.type,r->id.dequeue,(uint64_t)r->shadow.control,(uint64_t)r->shadow.status,ep,(uint64_t)e->status,(uint64_t)e->control,sp,(uint64_t)t->status,(uint64_t)t->control,r->stop_dequeue,(uint64_t)r->consumed);}
 for(unsigned i=0;i<wifi_nop5.commands && i<3;i++)console_printf("wifi-nop5-sync-command: index=%u pointer=%x event_pointer=%x cc=%u slot=%u delta=%u parameter=%x status=%x control=%x\n",(uint64_t)i,wifi_nop5.sync[i].pointer,wifi_nop5.sync[i].event_pointer,wifi_nop5.sync[i].cc,wifi_nop5.sync[i].slot,wifi_nop5.sync[i].delta,(uint64_t)wifi_nop5.sync[i].shadow.parameter_low|((uint64_t)wifi_nop5.sync[i].shadow.parameter_high<<32),(uint64_t)wifi_nop5.sync[i].shadow.status,(uint64_t)wifi_nop5.sync[i].shadow.control);
 const struct xhci_trb *e=&wifi_nop5.final_stop;uint64_t p=e->parameter_low|((uint64_t)e->parameter_high<<32);
 console_printf("wifi-nop5-final-stop: pointer=%x status=%x control=%x\n",p,(uint64_t)e->status,(uint64_t)e->control);
 console_printf("wifi-nop5-frontier: sequence=%u engine_sequence=%u pending=%u round=%u failed=%u empty=%u drain_events=%u\n",wifi_fence5_native.frontier,wifi_fence5_native.engine.last_frontier,(uint64_t)xhci.command_pending,(uint64_t)wifi_fence5_native.round,(uint64_t)wifi_fence5_native.failed,(uint64_t)wifi_nop5.empty,(uint64_t)wifi_nop5.drain_events);
}
static bool wifi_nop5_finish(bool good){
 if(wifi_nop5.pending || wifi_fence5_native.round || xhci.command_pending)return false;
 wifi_nop5.parent_complete=good;
 if(good && wifi_nop5.child_complete){uint64_t flags=event_irq_save();struct xhci_trb e;bool empty=false;
  for(unsigned i=0;i<32;i++){if(!poll_event(&e)){empty=true;break;}uint64_t ptr=e.parameter_low|((uint64_t)e.parameter_high<<32);if(((e.control>>10)&63)!=32 || (ptr>=wifi_nop5.physical && ptr<wifi_nop5.physical+4096)){good=false;break;}dispatch_transfer_event(&e);wifi_nop5.drain_events++;}
  wifi_nop5.empty=empty;good=good && empty && !xhci.command_pending && !wifi_fence5_native.failed && !wifi_fence5_native.round;event_irq_restore(flags);
  if(good){struct mt76_dma5_retirement proof={.epoch=wifi_nop5.epoch,.keys_erased=wifi_nop5.dma.noop_only && !wifi_join5.attempted && !wifi_wpa5.attempted,.mac_disabled=wifi_nop5.child_complete && wifi_nop5.controls==6,.tx_stopped=wifi_nop5.stopped && wifi_nop5.disabled,.rx_stopped=wifi_nop5.dma.noop_only && !wifi_nop5.dma.ring[1],.events_drained=empty,.callbacks_drained=!wifi_nop5.active && !wifi_nop5.pending && !wifi_fence5_native.busy && !wifi_fence5_native.engine.busy,.parent_restored=wifi_nop5.parent_complete};wifi_nop5.retired=mt76_dma5_close(&wifi_nop5.dma,wifi_nop5.epoch) && mt76_dma5_retire(&wifi_nop5.dma,&proof);good=wifi_nop5.retired;}
 }
 wifi_nop5_emit();
 console_printf("wifi-nop5: complete=%u parent_complete=%u child_complete=%u posted=%u completed=%u boundary=63 after_wrap=1 wraps=%u typed_noop=%u data_prepared=%u data_published=%u data_completed=%u fences=%u fence_commands=%u sync_commands=%u stop_events=%u configured=%u stopped=%u disabled=%u input_restored=%u retired=%u events_empty=%u retained=1 backend_retired=0 network_out=0 keys=0 data_dma=0\n",(uint64_t)good,(uint64_t)wifi_nop5.parent_complete,(uint64_t)wifi_nop5.child_complete,(uint64_t)wifi_nop5.posts,(uint64_t)wifi_nop5.events,wifi_nop5.dma.wraps,wifi_nop5.dma.noop_completed,wifi_nop5.dma.prepared,wifi_nop5.dma.published,wifi_nop5.dma.completed,wifi_fence5_native.engine.completed,(uint64_t)wifi_fence5_native.count,(uint64_t)wifi_nop5.commands,(uint64_t)(wifi_nop5.stop_events+(wifi_fence5_native.count?wifi_fence5_native.commands[0].stop_event:0)),(uint64_t)wifi_nop5.configured,(uint64_t)wifi_nop5.stopped,(uint64_t)wifi_nop5.disabled,(uint64_t)wifi_nop5.input_restored,(uint64_t)wifi_nop5.retired,(uint64_t)wifi_nop5.empty);
 return good;
}

bool xhci_wifi_capture_nop5(void){
 if(wifi_tx5_nop_child || wifi_nop5.attempted || wifi_tx5.attempted)return false;
 wifi_tx5_nop_child=wifi_nop5_run;bool good=xhci_wifi_capture_tx5();wifi_tx5_nop_child=NULL;return wifi_nop5_finish(good);
}
