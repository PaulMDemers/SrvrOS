/* One-shot fixed161 management transport. No key/data provider authority.
 * Independent owner replaces,never weakens,the historical stopped-RX gates.
 * Every DMA page survives failure and terminal restoration until reset. */
#include <srvros/mt76_join5.h>
#include <srvros/mt76_eapol5.h>
static struct {bool requested,active,attempted,complete;unsigned polls,frames,matches;}wifi_eapol5;
static bool (*wifi_join5_after)(void);
#define WIFI_JOIN5_RX 48
static struct {
 bool radio_enabled;
 struct {bool attempted,complete;unsigned count;uint32_t ext[17],fifo[17];}radio[2];
 struct xhci_device *d;struct mt76_peer5 peer;struct mt76_join5 exchange;struct mt76_mac_cycle mac;
 struct xhci_trb *rx,*tx;uint64_t rx_phys,tx_phys,pages[WIFI_JOIN5_RX+3],pins[WIFI_JOIN5_RX+5];
 uintptr_t pointers[WIFI_JOIN5_RX+5];uint8_t *buffer[WIFI_JOIN5_RX+3];
 uint8_t input[4096],expected[4096],cal[sizeof wifi_cal5],power[sizeof wifi_power5],handoff[sizeof wifi_handoff5],binding[sizeof wifi_peer5],preview[sizeof wifi_tx5];
 uint8_t old_ring[4096],old_pages[8][4096],handoff_ring[4096],preview_ring[4096];
 uint8_t rx_snapshot[WIFI_JOIN5_RX][4096];unsigned rx_copied;uint8_t slot;
 uint32_t context[5][8];uint64_t fixed[9],stop_pointer[2];uintptr_t input_pointer,output_pointer;
 unsigned lengths[3],kinds[3];uint8_t packets[3][124];
 unsigned phase,commands,posts,sends,rx_cc,rx_residue,rx_events,tx_events,stop_events[2],stop_dci,usb_ops;
 uint64_t rx_enqueue,tx_enqueue;bool rx_cycle,tx_cycle;
 unsigned cleanup_first,cleanup_entry,cleanup_posts,cleanup_completed,cleanup_forward;
 uint64_t cleanup_control_setup,cleanup_stop_index;struct xhci_trb cleanup_stop_command,cleanup_stop_event[2],cleanup_events[8],cleanup_forward_event,last_rx_event;
 uint32_t cleanup_last[3];unsigned cleanup_reads;
 bool cleanup_enabled,cleanup_started,cleanup_needed,cleanup_frozen,cleanup_control_active;
 bool eapol_required,attempted,active,failed,rx_pending,rx_done,rx_delivered,tx_pending,tx_done,restored;
}wifi_join5;
bool xhci_wifi_join5_radio_enable(void){
 if(wifi_join5.radio_enabled || wifi_join5.attempted)return false;
 wifi_join5.radio_enabled=true;return true;
}
static bool wifi_join5_radio(unsigned);
static bool wifi_join5_fail(void){wifi_join5.failed=true;return wifi_join5.d?wifi_mcu_fail(wifi_join5.d):false;}
static bool wifi_join5_cleanup_service(void);
static bool wifi_join5_cleanup_held(void){
 if(!wifi_join5.cleanup_enabled)return true;
 if(!wifi_join5.cleanup_frozen || wifi_join5.rx_pending || wifi_join5.posts>WIFI_JOIN5_RX || wifi_join5.rx_copied>wifi_join5.posts || wifi_join5.rx_copied!=wifi_join5.posts || wifi_join5.cleanup_posts>8 || wifi_join5.cleanup_first>wifi_join5.posts || wifi_join5.failed)return false;
 for(unsigned i=0;i<wifi_join5.posts;i++){
  uint64_t p=wifi_join5.pages[i];
  if(!p || (p&4095) || wifi_join5.pins[i+2]!=p || !wifi_join5.buffer[i] || wifi_join5.pointers[i+2]!=(uintptr_t)wifi_join5.buffer[i] || wifi_join5.buffer[i]!=pmm_phys_to_virt(p))return false;
 }
 for(unsigned i=0;i<wifi_join5.rx_copied;i++)if(!wifi_handoff5_equal(wifi_join5.buffer[i],wifi_join5.rx_snapshot[i],4096))return false;
 return true;
}
static bool wifi_join5_frozen(void){
 struct xhci_device *d=wifi_join5.d;
 if(wifi_join5.cleanup_enabled!=wifi_cal5.cleanup_enabled ||
 (wifi_join5.cleanup_started && (!wifi_join5.cleanup_enabled || !wifi_join5.mac.mac_disabled || wifi_join5.cleanup_first>wifi_join5.posts || wifi_join5.posts-wifi_join5.cleanup_first!=wifi_join5.cleanup_posts || wifi_join5.cleanup_posts>8 || wifi_join5.cleanup_completed>wifi_join5.cleanup_posts || wifi_join5.cleanup_forward>1 || !wifi_join5.cleanup_first || wifi_join5.cleanup_entry>wifi_join5.cleanup_first || wifi_join5.cleanup_first-wifi_join5.cleanup_entry>1 || wifi_join5.cleanup_entry+wifi_join5.cleanup_forward>wifi_join5.cleanup_first || (wifi_join5.cleanup_posts && wifi_join5.cleanup_entry+wifi_join5.cleanup_forward!=wifi_join5.cleanup_first) || (!wifi_join5.cleanup_frozen && wifi_join5.rx_copied!=wifi_join5.cleanup_entry+wifi_join5.cleanup_forward+wifi_join5.cleanup_completed))) ||
 (wifi_join5.cleanup_frozen && !wifi_join5_cleanup_held()) || wifi_join5.eapol_required!=wifi_eapol5.requested || !wifi_join5_requested || !wifi_join5.active || wifi_join5.failed || !d || d!=wifi_cal5.d || !d->present || d->removed || d->retiring || d->ep0_quarantined || d->control_failed ||
 d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 || !d->slot_id || d->slot_id!=wifi_join5.slot || xhci.stopping || firmware_command_failed ||
 !wifi_channel_vco5_owned(&wifi_channel_vco5) || !wifi_cal5_cleanup_held() || !wifi_handoff5_equal(&wifi_cal5,wifi_join5.cal,sizeof wifi_cal5) ||
 !wifi_handoff5_equal(&wifi_power5,wifi_join5.power,sizeof wifi_power5) || !wifi_handoff5_equal(&wifi_handoff5,wifi_join5.handoff,sizeof wifi_handoff5) ||
 !wifi_handoff5_equal(&wifi_peer5,wifi_join5.binding,sizeof wifi_peer5) || !wifi_handoff5_equal(&wifi_tx5,wifi_join5.preview,sizeof wifi_tx5) ||
 !wifi_handoff5_equal(&wifi_peer5.binding,&wifi_join5.peer,sizeof wifi_join5.peer) || !wifi_handoff5_equal(d->input_context,wifi_join5.expected,4096) ||
 !wifi_handoff5_equal(wifi_cal5.ep.ring,wifi_join5.old_ring,4096) || !wifi_handoff5_equal(wifi_handoff5.ring,wifi_join5.handoff_ring,4096) ||
 !wifi_handoff5_equal(wifi_tx5.ring,wifi_join5.preview_ring,4096) || (uintptr_t)d->input_context!=wifi_join5.input_pointer || (uintptr_t)d->output_context!=wifi_join5.output_pointer)return false;
 const uint64_t fixed[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)if(fixed[i]!=wifi_join5.fixed[i])return false;
 for(unsigned i=0;i<8;i++)if(!wifi_handoff5_equal(wifi_cal5.plan.buffer[i],wifi_join5.old_pages[i],4096))return false;
 for(unsigned i=0;i<WIFI_JOIN5_RX+5;i++){
  uint64_t p=i<2?(i?wifi_join5.tx_phys:wifi_join5.rx_phys):wifi_join5.pages[i-2];
  uintptr_t v=(uintptr_t)(i<2?(void *)(i?wifi_join5.tx:wifi_join5.rx):(void *)wifi_join5.buffer[i-2]);
  if(p!=wifi_join5.pins[i] || v!=wifi_join5.pointers[i] || (void *)v!=pmm_phys_to_virt(p))return false;
 }
 if(wifi_join5.rx_copied>wifi_join5.posts)return false;
 for(unsigned i=0;i<wifi_join5.rx_copied;i++)if(!wifi_handoff5_equal(wifi_join5.buffer[i],wifi_join5.rx_snapshot[i],4096))return false;
 for(unsigned i=wifi_join5.posts;i<WIFI_JOIN5_RX;i++)for(unsigned j=0;j<4096;j++)if(wifi_join5.buffer[i][j])return false;
 for(unsigned i=0;i<3;i++){
  unsigned n=i<wifi_join5.sends?wifi_join5.lengths[i]:0;
  if(n>124 || !wifi_handoff5_equal(wifi_join5.buffer[WIFI_JOIN5_RX+i],wifi_join5.packets[i],n))return false;
  for(unsigned j=n;j<4096;j++)if(wifi_join5.buffer[WIFI_JOIN5_RX+i][j])return false;
 }
 const unsigned dcis[]={0,11,16};
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,dcis[i],j)!=wifi_join5.context[i][j])return false;
 return true;
}
static bool wifi_join5_ring(struct xhci_trb *ring,uint64_t phys,unsigned posts,bool rx){
 for(unsigned i=0;i<XHCI_TRB_COUNT;i++){
  uint64_t p=0;uint32_t status=0,control=0;
  if(i==XHCI_TRB_COUNT-1){p=phys;control=(XHCI_TRB_TYPE_LINK<<10)|3;}
  else if(i<posts){p=wifi_join5.pages[rx?i:WIFI_JOIN5_RX+i];status=rx?4096:wifi_join5.lengths[i];control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|1|(rx?XHCI_TRB_ISP:0);}
  const struct xhci_trb *t=&ring[i];if(t->parameter_low!=(uint32_t)p || t->parameter_high!=(uint32_t)(p>>32) || t->status!=status || t->control!=control)return false;
 }
 return true;
}
static bool wifi_join5_context(void){
 struct xhci_device *d=wifi_join5.d;unsigned phase=wifi_join5.phase;if(phase>9)return false;
 for(unsigned j=0;j<8;j++){
  uint32_t tx=context_read32(d->output_context,8,j),rx=context_read32(d->output_context,9,j);
  if(phase==0){if(tx!=wifi_join5.context[3][j])return false;}
  else if(phase<8){
   uint64_t base=wifi_join5.tx_phys,deq=context_read32(d->output_context,8,2)|((uint64_t)context_read32(d->output_context,8,3)<<32);
   uint32_t want=j==0?(phase<6?1:3):j==1?(3u<<1)|(2u<<3)|(512u<<16):j==4?124:0;
   if(j==2 || j==3){if((deq&15)!=1 || (deq&~15ull)<base || (deq&~15ull)>base+16*wifi_join5.sends || (phase>=6 && deq!=(base+16*wifi_join5.sends+1)))return false;}
   else if(tx!=want)return false;
  }else if(j==0 && (tx&7))return false;
  if(phase<2 || phase>=7){if(rx!=wifi_join5.context[4][j])return false;}
  else {
   uint32_t want=wifi_join5.context[4][j];
   if(j==0)want=(want&~7u)|(phase==2 || phase>=5?3:1);
   if(j==2 || j==3){uint64_t deq=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);if((deq&15)!=1 || (deq&~15ull)<wifi_join5.rx_phys || (deq&~15ull)>wifi_join5.rx_phys+16*wifi_join5.posts || (phase>=5 && deq!=(wifi_join5.stop_pointer[1]|1)))return false;}
   else if(rx!=want)return false;
  }
 }
 return true;
}
static bool wifi_join5_owned(void *c,bool cleanup){
 (void)cleanup;struct xhci_device *d=wifi_join5.d;
 bool good=c==&wifi_join5 && wifi_join5_frozen() && d && !d->control_pending && !d->firmware.pending && !d->firmware.mcu.pending && !xhci.sync_users && !xhci.command_pending && !xhci.command_failed && !strict_control.device &&
 wifi_join5.posts<=WIFI_JOIN5_RX && wifi_join5.sends<=3 && wifi_join5.rx_enqueue==wifi_join5.posts && wifi_join5.tx_enqueue==wifi_join5.sends && wifi_join5.rx_cycle && wifi_join5.tx_cycle &&
 wifi_join5_ring(wifi_join5.rx,wifi_join5.rx_phys,wifi_join5.posts,true) && wifi_join5_ring(wifi_join5.tx,wifi_join5.tx_phys,wifi_join5.sends,false) && wifi_join5_context();
 if(!good && wifi_join5.active)wifi_join5_fail();
 return good;
}
/* Validate the exact already-published owner while consuming events. Never
 * perform EP0 I/O or publish a successor from an event callback. */
static bool wifi_join5_cleanup_event_span(struct xhci_device *d){
 if(!d || d!=wifi_join5.d)return false;
 if(xhci.command_pending){
  uint64_t i=wifi_join5.cleanup_stop_index;const struct xhci_trb *w=&wifi_join5.cleanup_stop_command;
  if(!firmware_command_exact || (wifi_join5.phase!=4 && wifi_join5.phase!=5) || !wifi_join5.stop_dci || xhci.sync_users!=1 || d->control_pending || strict_control.device || i>=63 || !xhci.command_ring)return false;
  const struct xhci_trb *h=&xhci.command_ring[i];
  return h->parameter_low==w->parameter_low && h->parameter_high==w->parameter_high && h->status==w->status && h->control==w->control && ((w->control>>10)&63)==15 && ((w->control>>16)&31)==wifi_join5.stop_dci && (w->control>>24)==d->slot_id && xhci.command_enqueue==(i==62?0:i+1);
 }
 if(d->control_pending || strict_control.device){
  if(!wifi_join5.cleanup_control_active || xhci.sync_users>1 || !d->control_pending || (strict_control.device!=d && (strict_control.device || !strict_control.done)) || strict_control.length!=4 || !d->ep0_ring)return false;
  uint64_t p=strict_control.setup;if(p<d->ep0_ring_phys || p>=d->ep0_ring_phys+63*16 || (p&15))return false;
  const struct xhci_trb *t=&d->ep0_ring[(p-d->ep0_ring_phys)/16];
  return (t->parameter_low|((uint64_t)t->parameter_high<<32))==wifi_join5.cleanup_control_setup && ((t->control>>10)&63)==XHCI_TRB_TYPE_SETUP_STAGE && t->status==8 && strict_control.data_in==((wifi_join5.cleanup_control_setup&0x80)!=0);
 }
 return xhci.sync_users==0;
}
static bool wifi_join5_event(struct xhci_device *d,const struct xhci_trb *e){
 unsigned dci=(e->control>>16)&31;uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);
 if(wifi_join5.cleanup_frozen && ((ptr>=wifi_join5.rx_phys && ptr<wifi_join5.rx_phys+4096) || (ptr>=wifi_join5.tx_phys && ptr<wifi_join5.tx_phys+4096))){wifi_join5_fail();return true;}
 if(!wifi_join5.active)return false;
 bool ours=dci==8 || dci==9;
 if(d!=wifi_join5.d || !ours){
  if((ptr>=wifi_join5.rx_phys && ptr<wifi_join5.rx_phys+1024) || (ptr>=wifi_join5.tx_phys && ptr<wifi_join5.tx_phys+1024)){wifi_join5_fail();return true;}
  return false;
 }
 unsigned cc=e->status>>24,residue=e->status&0xffffff;const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 bool good=(!wifi_join5.cleanup_enabled || wifi_join5_cleanup_event_span(d)) && wifi_join5_frozen() && wifi_join5_ring(wifi_join5.rx,wifi_join5.rx_phys,wifi_join5.posts,true) && wifi_join5_ring(wifi_join5.tx,wifi_join5.tx_phys,wifi_join5.sends,false) && ((e->control>>10)&63)==32 && (e->control>>24)==d->slot_id && !(e->control&~allowed);
 if(good && cc>=26 && cc<=28){
  unsigned n=dci==8?0:1;uint64_t expected=(dci==8?wifi_join5.tx_phys:wifi_join5.rx_phys)+16*(dci==8?wifi_join5.sends:wifi_join5.posts);
  bool pending=dci==9 && wifi_join5.rx_pending;
  good=firmware_command_exact && wifi_join5.phase==(dci==9?4u:5u) && wifi_join5.stop_dci==dci && !wifi_join5.stop_events[n] &&
   ((cc==27 && !residue && ptr==expected && (!wifi_join5.cleanup_enabled || !pending)) || (pending && cc!=27 && residue<=4096 && ptr==expected-16));
  wifi_join5.stop_events[n]++;
  if(good){wifi_join5.stop_pointer[n]=ptr;if(wifi_join5.cleanup_enabled)wifi_join5.cleanup_stop_event[n]=*e;if(dci==9)wifi_join5.rx_pending=false;}
  console_printf("wifi-join5-stop-event: dci=%u cc=%u residue=%u pointer=%x ok=%u\n",(uint64_t)dci,(uint64_t)cc,(uint64_t)residue,ptr,(uint64_t)good);
 }else if(good && dci==8){
  good=wifi_join5.phase==3 && wifi_join5.tx_pending && wifi_join5.sends && ptr==wifi_join5.tx_phys+16*(wifi_join5.sends-1) && cc==1 && !residue && !wifi_join5.tx_events;
  if(good){wifi_join5.tx_events++;wifi_join5.tx_pending=false;wifi_join5.tx_done=true;}
 }else if(good){
  good=(wifi_join5.phase==3 || (wifi_join5.phase==4 && wifi_join5.stop_dci==9)) && wifi_join5.rx_pending && wifi_join5.posts && ptr==wifi_join5.rx_phys+16*(wifi_join5.posts-1) && (cc==1 || cc==13) && residue<=4096 && !wifi_join5.rx_events;
  if(good){wifi_join5.rx_events++;wifi_join5.rx_pending=false;wifi_join5.rx_done=true;wifi_join5.rx_cc=cc;wifi_join5.rx_residue=residue;if(wifi_join5.cleanup_enabled)wifi_join5.last_rx_event=*e;if(wifi_join5.cleanup_started && wifi_join5.posts>wifi_join5.cleanup_first){unsigned n=wifi_join5.posts-wifi_join5.cleanup_first;if(n>8)return wifi_join5_fail(),true;wifi_join5.cleanup_events[n-1]=*e;}}
 }
 if(!good)wifi_join5_fail();
 return true;
}
static void wifi_join5_receipt(unsigned phase){
 for(unsigned dci=8;dci<=9;dci++){
  struct xhci_device *d=wifi_join5.d;
  console_printf("wifi-join5-context: phase=%u dci=%u w0=%x w1=%x w2=%x w3=%x w4=%x w5=%x w6=%x w7=%x\n",(uint64_t)phase,(uint64_t)dci,(uint64_t)context_read32(d->output_context,dci,0),(uint64_t)context_read32(d->output_context,dci,1),(uint64_t)context_read32(d->output_context,dci,2),(uint64_t)context_read32(d->output_context,dci,3),(uint64_t)context_read32(d->output_context,dci,4),(uint64_t)context_read32(d->output_context,dci,5),(uint64_t)context_read32(d->output_context,dci,6),(uint64_t)context_read32(d->output_context,dci,7));
 }
}
static bool wifi_join5_command(unsigned phase,unsigned type,unsigned dci,uint64_t deq){
 struct xhci_device *d=wifi_join5.d;if(wifi_join5.phase!=phase || !wifi_join5_owned(&wifi_join5,true))return wifi_join5_fail();
 struct xhci_trb cmd={.control=(type<<10)|((uint32_t)d->slot_id<<24)};
 if(type==12){cmd.parameter_low=d->input_context_phys;cmd.parameter_high=d->input_context_phys>>32;}
 else {cmd.control|=dci<<16;cmd.parameter_low=deq;cmd.parameter_high=deq>>32;}
 if(type==15 && wifi_join5.cleanup_enabled){wifi_join5.cleanup_stop_index=xhci.command_enqueue;wifi_join5.cleanup_stop_command=cmd;wifi_join5.cleanup_stop_command.control|=xhci.command_cycle?1u:0u;}
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;wifi_join5.commands++;wifi_join5.stop_dci=type==15?dci:0;
 bool good=submit_command_trb(&cmd,type,&slot) && slot==d->slot_id;
 wifi_join5.stop_dci=0;firmware_command_exact=false;
 if(type==15)good=good && wifi_join5.stop_events[dci==8?0:1]==1;
 if(!good)firmware_command_failed=true;
 if(good)wifi_join5.phase++;
 event_irq_restore(flags);
 console_printf("wifi-join5-command: phase=%u type=%u dci=%u slot=%u ok=%u\n",(uint64_t)phase,(uint64_t)type,(uint64_t)dci,slot,(uint64_t)good);
 wifi_join5_receipt(wifi_join5.phase);
 return good && wifi_join5_owned(&wifi_join5,true)?true:wifi_join5_fail();
}
static bool wifi_join5_post(void){
 if(!wifi_join5_owned(&wifi_join5,false) || wifi_join5.posts>=WIFI_JOIN5_RX || wifi_join5.rx_pending || (wifi_join5.rx_done && !wifi_join5.rx_delivered))return false;
 unsigned i=wifi_join5.posts;struct xhci_trb td={.parameter_low=wifi_join5.pages[i],.parameter_high=wifi_join5.pages[i]>>32,.status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
 uint64_t flags=event_irq_save();wifi_join5.posts++;wifi_join5.rx_pending=true;wifi_join5.rx_done=wifi_join5.rx_delivered=false;wifi_join5.rx_events=0;
 uint64_t ptr=ring_push(wifi_join5.rx,wifi_join5.rx_phys,&wifi_join5.rx_enqueue,&wifi_join5.rx_cycle,&td);
 bool good=ptr==wifi_join5.rx_phys+16*i;if(good){if(wifi_join5.phase==2)wifi_join5.phase=3;db_write32(wifi_join5.d->slot_id,9);}event_irq_restore(flags);
 if(good && i==0){
  uint64_t start=timer_monotonic_us();bool running=false;
  for(unsigned n=0;n<512;n++){
   if(!wifi_join5_frozen())break;
   if((context_read32(wifi_join5.d->output_context,9,0)&7)==1){running=true;break;}
   uint64_t now=timer_monotonic_us();if(now<start || now-start>=50000 || !timer_delay_us(100))break;
  }
  good=running && wifi_join5_owned(&wifi_join5,false);
 }
 return good?true:wifi_join5_fail();
}
static void wifi_join5_dispatch(void){uint64_t flags=event_irq_save();struct xhci_trb e;if(poll_event(&e))dispatch_transfer_event(&e);event_irq_restore(flags);}
static enum mt76_join5_send wifi_join5_send(void *c,enum mt76_mgmt5_kind kind,const uint8_t *packet,size_t length){
 unsigned n=wifi_join5.sends;uint8_t expected[124]={0};size_t want=mt76_mgmt5_usb_request(&wifi_join5.peer.peer,kind,(unsigned)kind+1,false,expected,sizeof expected);
 bool order=(kind==MT76_MGMT5_AUTH && n==0) || (kind==MT76_MGMT5_ASSOC && n==1 && wifi_join5.exchange.auth_matched) || (kind==MT76_MGMT5_LEAVE && n>=1 && n<=2 && wifi_join5.exchange.auth_sent);
 if(c!=&wifi_join5 || !packet || !want || want>124 || wifi_join5.phase!=3 || n>=3 || !order || !wifi_join5.mac.started || !wifi_join5.mac.snapshot_complete || !wifi_join5_owned(c,false) || length!=want || !wifi_handoff5_equal(packet,expected,want)){zero_bytes(expected,sizeof expected);return MT76_JOIN5_REFUSED;}
 if(wifi_join5.radio_enabled && !wifi_join5_radio(kind==MT76_MGMT5_AUTH?0:1))return MT76_JOIN5_UNCERTAIN;
 copy_bytes(wifi_join5.buffer[WIFI_JOIN5_RX+n],packet,length);copy_bytes(wifi_join5.packets[n],packet,length);wifi_join5.lengths[n]=length;wifi_join5.kinds[n]=kind;zero_bytes(expected,sizeof expected);
 struct xhci_trb td={.parameter_low=wifi_join5.pages[WIFI_JOIN5_RX+n],.parameter_high=wifi_join5.pages[WIFI_JOIN5_RX+n]>>32,.status=length,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC};
 uint64_t flags=event_irq_save();wifi_join5.sends++;wifi_join5.tx_pending=true;wifi_join5.tx_done=false;wifi_join5.tx_events=0;
 uint64_t ptr=ring_push(wifi_join5.tx,wifi_join5.tx_phys,&wifi_join5.tx_enqueue,&wifi_join5.tx_cycle,&td);bool good=ptr==wifi_join5.tx_phys+16*n;
 if(good)db_write32(wifi_join5.d->slot_id,8);
 event_irq_restore(flags);
 uint64_t start=timer_monotonic_us(),previous=start;
 for(unsigned i=0;good && !wifi_join5.tx_done && i<8192;i++){
  wifi_join5_dispatch();good=wifi_join5_owned(c,false);uint64_t now=timer_monotonic_us();
  if(now<previous || now-start>=750000){good=false;break;}previous=now;
  if(good && !wifi_join5.tx_done)good=timer_delay_us(100);
 }
 good=good && wifi_join5.tx_done && wifi_join5.tx_events==1;
 console_printf("wifi-join5-send: kind=%u bytes=%u completed=%u replay=0\n",(uint64_t)kind,(uint64_t)length,(uint64_t)good);
 if(!good){wifi_join5_fail();return MT76_JOIN5_UNCERTAIN;}return MT76_JOIN5_SENT;
}
static bool wifi_join5_poll(void *c,uint8_t *out,size_t cap,size_t *size,bool *present){
 if(!out || !size || !present || c!=&wifi_join5 || wifi_join5.cleanup_started || !wifi_join5_owned(c,false))return false;
 *size=0;*present=false;wifi_join5_dispatch();if(!wifi_join5_owned(c,false))return false;
 if(wifi_join5.rx_done && !wifi_join5.rx_delivered){
  unsigned i=wifi_join5.posts-1;
  if(i!=wifi_join5.rx_copied)return wifi_join5_fail();
  copy_bytes(wifi_join5.rx_snapshot[i],wifi_join5.buffer[i],4096);wifi_join5.rx_copied++;
  size_t bytes=4096-wifi_join5.rx_residue;
  if(wifi_eapol5.active){
   struct mt76_eapol5_peer p={.channel=161,.bandwidth=20};
   copy_bytes(p.station,wifi_join5.peer.peer.station,6);copy_bytes(p.bssid,wifi_join5.peer.peer.bssid,6);
   struct mt76_eapol5_view v={0};enum mt76_rx_status status=mt76_eapol5_rx(&p,wifi_join5.buffer[i],bytes,&v);
   wifi_join5.rx_delivered=true;wifi_eapol5.frames++;
   uint32_t rxinfo=0;if(bytes>=8)for(unsigned k=0;k<4;k++)rxinfo|=(uint32_t)wifi_join5.buffer[i][4+k]<<(8*k);
   bool shape=status==MT76_RX_OK && mt76_eapol5_m1(v.eapol,v.eapol_size);
   if(status==MT76_RX_OK && v.eapol_size<=cap){copy_bytes(out,v.eapol,v.eapol_size);*size=v.eapol_size;*present=true;}
   console_printf("wifi-eapol5-rx: index=%u bytes=%u view=%u rxinfo=%x mpdu=%u fc=%x eapol=%u m1=%u\n",(uint64_t)i,(uint64_t)bytes,(uint64_t)status,(uint64_t)rxinfo,(uint64_t)v.mpdu_size,(uint64_t)v.frame_control,(uint64_t)v.eapol_size,(uint64_t)shape);
   zero_bytes(&p,sizeof p);
  }else{
  struct mt76_rx_view v={0};
  enum mt76_rx_status status=mt76_rx_usb_view(wifi_join5.buffer[i],bytes,&v);
  if(wifi_join5.radio_enabled && bytes>=40){
   const uint8_t *b=wifi_join5.buffer[i];uint32_t info=0;for(unsigned k=0;k<4;k++)info|=(uint32_t)b[4+k]<<(8*k);
   console_printf("wifi-join5-framing: index=%u bytes=%u dma=%u rxinfo=%x raw_fc=%u view=%u consumed=%u\n",(uint64_t)i,(uint64_t)bytes,(uint64_t)(b[0]|((unsigned)b[1]<<8)),(uint64_t)info,(uint64_t)(b[36]|((unsigned)b[37]<<8)),(uint64_t)status,(uint64_t)v.consumed);
  }
  wifi_join5.rx_delivered=true;
  if(status==MT76_RX_OK && v.mpdu_size>=24 && v.mpdu_size<=cap && v.body_size==v.mpdu_size-24){copy_bytes(out,v.header,24);copy_bytes(out+24,v.body,v.body_size);*size=v.mpdu_size;*present=true;}
  /* Read-only bounded diagnostic;no address,SSID,IE or packet bytes. The
   * actual exchange still applies its unchanged parser and deadline. */
  bool peer=*present && wifi_handoff5_equal(v.header+4,wifi_join5.peer.peer.station,6) && wifi_handoff5_equal(v.header+10,wifi_join5.peer.peer.bssid,6) && wifi_handoff5_equal(v.header+16,wifi_join5.peer.peer.bssid,6);
  unsigned type=v.frame_control&0xfc,algorithm=0,transaction=0,code=0,parser=3;
  if(*present && type==0xb0 && v.body_size>=6){algorithm=v.body[0]|((unsigned)v.body[1]<<8);transaction=v.body[2]|((unsigned)v.body[3]<<8);code=v.body[4]|((unsigned)v.body[5]<<8);}
  else if(*present && type==0x10 && v.body_size>=4)code=v.body[2]|((unsigned)v.body[3]<<8);
  if(peer && (type==0xb0 || type==0x10)){struct mt76_mgmt5_response r={0};parser=mt76_mgmt5_response(&wifi_join5.peer.peer,type==0xb0?MT76_MGMT5_AUTH:MT76_MGMT5_ASSOC,out,*size,&r);}
  console_printf("wifi-join5-rx: index=%u bytes=%u view=%u mpdu=%u fc=%x body=%u peer=%u algorithm=%u transaction=%u code=%u parser=%u\n",(uint64_t)i,(uint64_t)bytes,(uint64_t)status,(uint64_t)v.mpdu_size,(uint64_t)v.frame_control,(uint64_t)v.body_size,(uint64_t)peer,(uint64_t)algorithm,(uint64_t)transaction,(uint64_t)code,(uint64_t)parser);
  }
  if(wifi_join5.posts<WIFI_JOIN5_RX && !wifi_join5_post())return false;
 }
 return true;
}
static bool wifi_join5_delay(void *c,unsigned us){return c==&wifi_join5 && us==100 && wifi_join5_owned(c,false) && timer_delay_us(us);}
static uint64_t wifi_join5_now(void *c){(void)c;return timer_monotonic_us();}
static bool wifi_join5_usb(void *c,bool write,uint32_t a,uint32_t *v,bool cleanup){
 if(!v || c!=&wifi_join5 || !wifi_join5_owned(c,cleanup))return false;
 switch(a){case 0x1798:case 0x1718:if(!wifi_join5.radio_enabled || write)return false;break;case 0x1004:case 0x2000:case 0x208:case 0x1400:case 0x1340:case 0x13b0:case 0x2124:case 0x438:case 0xa30:case 0xa34:case 0x1200:case 0x430:break;default:return false;}
 if(write && a!=0x1004 && a!=0x1400 && a!=0x1340)return false;
 wifi_join5.cleanup_control_setup=(uint64_t)(write?0x40:0xc0)|((uint64_t)(write?6:7)<<8)|((uint64_t)a<<32)|(4ull<<48);
 wifi_join5.cleanup_control_active=wifi_join5.cleanup_enabled;
 wifi_join5.usb_ops++;bool ok=wifi_mac_private_usb(wifi_join5.d,write,a,v);
 wifi_join5.cleanup_control_active=false;
 return ok && wifi_join5_owned(c,cleanup)?true:wifi_join5_fail();
}
/* Explicit diagnostic only. Read EXT before the consuming FIFO; never wait,
 * retransmit, treat an ACK as a peer reply, or read after the first empty entry.
 * Phase1 precedes ASSOC/leave so their status cannot contaminate AUTH evidence. */
static bool wifi_join5_radio(unsigned phase){
 if(phase>1)return wifi_join5_fail();
 if(wifi_join5.radio[phase].attempted)return wifi_join5.radio[phase].complete;
 wifi_join5.radio[phase].attempted=true;
 if((!phase && wifi_join5.sends) || (phase && (!wifi_join5.radio[0].complete || wifi_join5.sends!=1 || wifi_join5.tx_pending)))return wifi_join5_fail();
 for(unsigned i=0;i<17;i++){
  uint32_t ext=0,fifo=0;
  if(!wifi_join5_usb(&wifi_join5,false,0x1798,&ext,false) ||
     !wifi_join5_usb(&wifi_join5,false,0x1718,&fifo,false))return wifi_join5_fail();
  wifi_join5.radio[phase].ext[i]=ext;wifi_join5.radio[phase].fifo[i]=fifo;
  wifi_join5.radio[phase].count++;
  if(!(fifo&1))break;
 }
 wifi_join5.radio[phase].complete=true;return true;
}
static bool wifi_join5_read(void *c,uint32_t a,uint32_t *v,bool cleanup){
 if(!v || (cleanup && !wifi_join5_cleanup_service()))return false;
 *v=0;
 bool ok=wifi_join5_usb(c,false,a,v,cleanup);
 if(ok && cleanup && wifi_join5.cleanup_started && (a==0x430 || a==0xa30 || a==0xa34)){
  unsigned i=a==0x430?0:a==0xa30?1:2;wifi_join5.cleanup_last[i]=*v;wifi_join5.cleanup_reads++;
  if((i==0?(*v&0xff0000):*v))wifi_join5.cleanup_needed=true;
 }
 return ok;
}
static bool wifi_join5_write(void *c,uint32_t a,uint32_t v,bool cleanup){return wifi_join5_usb(c,true,a,&v,cleanup);}
static bool wifi_join5_mac_delay(void *c,unsigned us,bool cleanup){return us<=10000 && (!cleanup || wifi_join5_cleanup_service()) && wifi_join5_owned(c,cleanup) && timer_delay_us(us);}
/* Retained, never-reused RX pages only. Completed cleanup bytes are frozen but
 * never parsed, exposed as management replies, or given key/data authority. */
static bool wifi_join5_cleanup_snapshot(void){
 if(!wifi_join5.rx_done || wifi_join5.rx_delivered)return true;
 unsigned i=wifi_join5.posts-1;
 if(wifi_join5.rx_copied!=i || wifi_join5.rx_events!=1 || (wifi_join5.rx_cc!=1 && wifi_join5.rx_cc!=13) || wifi_join5.rx_residue>4096)return wifi_join5_fail();
 copy_bytes(wifi_join5.rx_snapshot[i],wifi_join5.buffer[i],4096);wifi_join5.rx_copied++;wifi_join5.rx_delivered=true;
 if(i<wifi_join5.cleanup_first){wifi_join5.cleanup_forward++;wifi_join5.cleanup_forward_event=wifi_join5.last_rx_event;}else wifi_join5.cleanup_completed++;
 return true;
}
static bool wifi_join5_cleanup_service(void){
 if(!wifi_join5.cleanup_enabled || !wifi_join5.mac.mac_disabled || wifi_join5.cleanup_frozen)return true;
 if(!wifi_join5_owned(&wifi_join5,true) || wifi_join5.phase!=3)return wifi_join5_fail();
 if(!wifi_join5.cleanup_started){wifi_join5.cleanup_started=true;wifi_join5.cleanup_first=wifi_join5.posts;wifi_join5.cleanup_entry=wifi_join5.rx_copied;}
 uint64_t flags=event_irq_save();struct xhci_trb e;bool good=true;
 for(unsigned i=0;i<32 && poll_event(&e);i++){
  unsigned type=(e.control>>10)&63;if(type!=32 && type!=33){good=false;break;}
  dispatch_transfer_event(&e);if(wifi_join5.failed){good=false;break;}
 }
 if(good)good=wifi_join5_cleanup_snapshot();
 if(good && wifi_join5.cleanup_needed && !wifi_join5.rx_pending){
  good=wifi_join5.cleanup_posts<8 && wifi_join5.posts<WIFI_JOIN5_RX && wifi_join5.rx_copied==wifi_join5.posts && wifi_join5.rx_done && wifi_join5.rx_delivered;
  if(good){unsigned i=wifi_join5.posts;struct xhci_trb td={.parameter_low=wifi_join5.pages[i],.parameter_high=wifi_join5.pages[i]>>32,.status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
   wifi_join5.cleanup_posts++;wifi_join5.posts++;wifi_join5.rx_pending=true;wifi_join5.rx_done=wifi_join5.rx_delivered=false;wifi_join5.rx_events=0;wifi_join5.cleanup_needed=false;
   uint64_t ptr=ring_push(wifi_join5.rx,wifi_join5.rx_phys,&wifi_join5.rx_enqueue,&wifi_join5.rx_cycle,&td);
   good=ptr==wifi_join5.rx_phys+16*i;if(good)db_write32(wifi_join5.d->slot_id,9);
  }
 }
 good=good && wifi_join5_owned(&wifi_join5,true);event_irq_restore(flags);
 return good?true:wifi_join5_fail();
}
static bool wifi_join5_cleanup_freeze(void){
 if(!wifi_join5.cleanup_enabled)return true;
 if(wifi_join5.phase!=6 || !wifi_join5.mac.rx_drained || !wifi_join5.mac.mac_disabled || wifi_join5.stop_events[0]!=1 || wifi_join5.stop_events[1]!=1 || wifi_join5.rx_pending || !wifi_join5_owned(&wifi_join5,true))return wifi_join5_fail();
 if(!wifi_join5_cleanup_snapshot())return false;
 /* A terminal Stop residue is never a normal receive or packet. Freeze any
  * last held page only after successful command/final-context validation. */
 if(wifi_join5.rx_copied<wifi_join5.posts){
  if(wifi_join5.rx_copied+1!=wifi_join5.posts)return wifi_join5_fail();
  unsigned i=wifi_join5.rx_copied;copy_bytes(wifi_join5.rx_snapshot[i],wifi_join5.buffer[i],4096);wifi_join5.rx_copied++;
 }
 wifi_join5.cleanup_frozen=true;return wifi_join5_cleanup_held();
}
static void wifi_join5_cleanup_emit(void){
 if(!wifi_join5.cleanup_enabled)return;
 uint64_t codes=0,res[2]={0};
 for(unsigned i=0;i<wifi_join5.cleanup_posts;i++){const struct xhci_trb *e=&wifi_join5.cleanup_events[i];codes|=(uint64_t)(e->status>>24)<<(8*i);res[i/4]|=(uint64_t)(e->status&0xffffff)<<(16*(i%4));}
 const struct xhci_trb *e=&wifi_join5.cleanup_stop_event[1];
 console_printf("wifi-join5-cleanup: version=1 first=%u entry=%u posted=%u completed=%u forward=%u forward_p=%x forward_s=%u forward_c=%u copied=%u codes=%x residues0=%x residues1=%x stop_p=%x stop_s=%u stop_c=%u stop_q=%x rounds=%u reads=%u busy0=%x busy1=%x busy2=%x held=%u failed=%u\n",(uint64_t)wifi_join5.cleanup_first,(uint64_t)wifi_join5.cleanup_entry,(uint64_t)wifi_join5.cleanup_posts,(uint64_t)wifi_join5.cleanup_completed,(uint64_t)wifi_join5.cleanup_forward,wifi_join5.cleanup_forward_event.parameter_low|((uint64_t)wifi_join5.cleanup_forward_event.parameter_high<<32),(uint64_t)wifi_join5.cleanup_forward_event.status,(uint64_t)wifi_join5.cleanup_forward_event.control,(uint64_t)wifi_join5.rx_copied,codes,res[0],res[1],e->parameter_low|((uint64_t)e->parameter_high<<32),(uint64_t)e->status,(uint64_t)e->control,wifi_join5.stop_pointer[1]|(wifi_join5.stop_events[1]?1:0),(uint64_t)wifi_join5.mac.rx_rounds,(uint64_t)wifi_join5.cleanup_reads,(uint64_t)wifi_join5.cleanup_last[0],(uint64_t)wifi_join5.cleanup_last[1],(uint64_t)wifi_join5.cleanup_last[2],(uint64_t)wifi_join5_cleanup_held(),(uint64_t)wifi_join5.failed);
}
static bool wifi_eapol5_capture(void *c,bool *unsafe){
 if(c!=&wifi_join5 || !unsafe || wifi_eapol5.attempted || !wifi_eapol5.requested || !wifi_join5_owned(c,false))return false;
 wifi_eapol5.attempted=wifi_eapol5.active=true;
 console_write("wifi-eapol5-progress: bounded post-association M1 receive;no keys or EAPOL TX;leave and full retirement follow\n");
 uint64_t start=timer_monotonic_us(),previous=start;bool clock=true;
 for(unsigned n=0;n<20000;n++){
  size_t size=0;bool present=false;wifi_eapol5.polls++;
  if(!wifi_join5_poll(c,wifi_join5.exchange.reply,sizeof wifi_join5.exchange.reply,&size,&present) || !wifi_join5_owned(c,false)){*unsafe=true;break;}
  bool match=present && mt76_eapol5_m1(wifi_join5.exchange.reply,size);
  zero_bytes(wifi_join5.exchange.reply,sizeof wifi_join5.exchange.reply);
  uint64_t now=timer_monotonic_us();if(now<previous){clock=false;break;}previous=now;
  if(match && now-start<1500000){wifi_eapol5.matches++;wifi_eapol5.complete=true;break;}
  if(now-start>=1500000 || (wifi_join5.posts==WIFI_JOIN5_RX && !wifi_join5.rx_pending))break;
  if(!wifi_join5_delay(c,100)){*unsafe=true;break;}
 }
 zero_bytes(wifi_join5.exchange.reply,sizeof wifi_join5.exchange.reply);wifi_eapol5.active=false;
 uint64_t end=timer_monotonic_us();clock=clock && end>=start;
 wifi_eapol5.complete=wifi_eapol5.complete && clock && !*unsafe && wifi_join5_owned(c,false);
 console_printf("wifi-eapol5: complete=%u polls=%u frames=%u matched=%u clock_valid=%u elapsed_us=%u eapol_tx=0 keys=0 authenticated=0\n",(uint64_t)wifi_eapol5.complete,(uint64_t)wifi_eapol5.polls,(uint64_t)wifi_eapol5.frames,(uint64_t)wifi_eapol5.matches,(uint64_t)clock,clock?end-start:0);
 return wifi_eapol5.complete;
}
static bool wifi_join5_inspection(void *c,uint32_t alc,uint32_t ibi){
 if(c!=&wifi_join5 || alc!=wifi_power5.cycle.chosen[0] || ibi!=UINT32_MAX || !wifi_join5_owned(c,false))return false;
 const struct mt76_join5_io io={c,wifi_join5_owned,wifi_join5_send,wifi_join5_poll,wifi_join5_delay,wifi_join5_now};
 uint64_t start=timer_monotonic_us();bool complete=mt76_join5_run_after(&wifi_join5.exchange,&wifi_join5.peer,&io,wifi_eapol5.requested?wifi_eapol5_capture:0);uint64_t end=timer_monotonic_us();
 console_printf("wifi-join5-exchange: complete=%u clock_valid=%u elapsed_us=%u\n",(uint64_t)complete,(uint64_t)(end>=start),end>=start?end-start:0);
 return complete;
}
static void wifi_join5_emit_responses(void){
 if(wifi_join5.radio_enabled)for(unsigned p=0;p<2;p++){
  console_printf("wifi-join5-radio: phase=%u attempted=%u complete=%u reads=%u observation_only=1\n",(uint64_t)p,(uint64_t)wifi_join5.radio[p].attempted,(uint64_t)wifi_join5.radio[p].complete,(uint64_t)wifi_join5.radio[p].count);
  for(unsigned i=0;i<wifi_join5.radio[p].count;i++)console_printf("wifi-join5-radio-entry: phase=%u i=%u ext=%x fifo=%x\n",(uint64_t)p,(uint64_t)i,(uint64_t)wifi_join5.radio[p].ext[i],(uint64_t)wifi_join5.radio[p].fifo[i]);
 }

 for(unsigned i=0;i<2;i++){
  const struct mt76_join5_response_observation *o=&wifi_join5.exchange.response[i];
  console_printf("wifi-join5-response: phase=%u attempted=%u reason=%u polls=%u present=%u ignored=%u peer=%u clocks=%u span_us=%u\n",(uint64_t)i,(uint64_t)o->attempted,(uint64_t)o->reason,(uint64_t)o->polls,(uint64_t)o->present,(uint64_t)o->ignored,(uint64_t)o->peer,(uint64_t)o->clocks,o->span_us);
 }
}
static bool wifi_join5_inspect(void){
 if(!wifi_join5_requested || wifi_join5.attempted)return false;
 wifi_join5.attempted=true;wifi_join5.cleanup_enabled=wifi_cal5.cleanup_enabled;wifi_join5.eapol_required=wifi_eapol5.requested;wifi_join5.d=wifi_cal5.d;
 if(!wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) || !wifi_tx5.input_restored || !wifi_tx5.disabled || wifi_tx5.phase!=7 || !wifi_tx5_owned())return false;
 struct xhci_device *d=wifi_join5.d;wifi_join5.slot=d->slot_id;const unsigned dcis[]={0,11,16,8,9};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<8;j++)wifi_join5.context[i][j]=context_read32(d->output_context,dcis[i],j);
 wifi_join5.peer=wifi_peer5.binding;copy_bytes(wifi_join5.input,d->input_context,4096);copy_bytes(wifi_join5.expected,d->input_context,4096);
 const uint64_t fixed[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)wifi_join5.fixed[i]=fixed[i];
 wifi_join5.input_pointer=(uintptr_t)d->input_context;wifi_join5.output_pointer=(uintptr_t)d->output_context;
 for(unsigned i=0;i<WIFI_JOIN5_RX+5;i++){
  void *p=0;uint64_t phys=0;if(!alloc_frame_zero(&phys,&p))return false;
  for(unsigned j=0;j<9;j++)if(phys==fixed[j])return false;
  if(phys==wifi_cal5.plan.ring || phys==wifi_handoff5.plan.new_ring || phys==wifi_tx5.ring_phys)return false;
  if(wifi_cal5_cleanup_alias(phys))return false;
  for(unsigned j=0;j<8;j++)if(phys==wifi_cal5.plan.physical[j])return false;
  for(unsigned j=0;j<i;j++)if(phys==wifi_join5.pins[j])return false;
  wifi_join5.pins[i]=phys;wifi_join5.pointers[i]=(uintptr_t)p;
  if(i<2){if(i){wifi_join5.tx_phys=phys;wifi_join5.tx=p;}else{wifi_join5.rx_phys=phys;wifi_join5.rx=p;}write_link_trb(p,phys,true);}
  else{wifi_join5.pages[i-2]=phys;wifi_join5.buffer[i-2]=p;}
 }
 wifi_join5.rx_cycle=wifi_join5.tx_cycle=true;
 copy_bytes(wifi_join5.cal,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);copy_bytes(wifi_join5.power,(const uint8_t *)&wifi_power5,sizeof wifi_power5);
 copy_bytes(wifi_join5.handoff,(const uint8_t *)&wifi_handoff5,sizeof wifi_handoff5);copy_bytes(wifi_join5.binding,(const uint8_t *)&wifi_peer5,sizeof wifi_peer5);copy_bytes(wifi_join5.preview,(const uint8_t *)&wifi_tx5,sizeof wifi_tx5);
 copy_bytes(wifi_join5.old_ring,(const uint8_t *)wifi_cal5.ep.ring,4096);copy_bytes(wifi_join5.handoff_ring,(const uint8_t *)wifi_handoff5.ring,4096);copy_bytes(wifi_join5.preview_ring,(const uint8_t *)wifi_tx5.ring,4096);
 for(unsigned i=0;i<8;i++)copy_bytes(wifi_join5.old_pages[i],wifi_cal5.plan.buffer[i],4096);
 wifi_join5.active=true;bool good=wifi_join5_owned(&wifi_join5,false);
 console_write("wifi-join5-progress: frozen parent;fresh live RX/data OUT;matched AUTH ASSOC leave;MAC disable;stop/drop/restore;no keys or IP data\n");
 console_printf("wifi-join5-plan: rx=%x tx=%x old=%x slot=%u max_rx=48\n",wifi_join5.rx_phys,wifi_join5.tx_phys,(uint64_t)wifi_join5.context[4][2]|((uint64_t)wifi_join5.context[4][3]<<32),(uint64_t)d->slot_id);wifi_join5_receipt(0);
 if(good){
  zero_bytes(d->input_context,4096);context_write32(d->input_context,0,1,1u|(1u<<8));for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_join5.context[0][j]);
  context_write32(d->input_context,9,1,(3u<<1)|(2u<<3)|(512u<<16));context_write32(d->input_context,9,2,wifi_join5.tx_phys|1);context_write32(d->input_context,9,3,wifi_join5.tx_phys>>32);context_write32(d->input_context,9,4,124);copy_bytes(wifi_join5.expected,d->input_context,4096);
  good=wifi_join5_command(0,12,8,0);
 }
 if(good)good=wifi_join5_command(1,16,9,wifi_join5.rx_phys|1);
 if(good)good=wifi_join5_post();
 bool exchanged=false;
 if(good){const struct mt76_mac_cycle_io io={&wifi_join5,wifi_join5_owned,wifi_join5_read,wifi_join5_write,wifi_join5_mac_delay,wifi_join5_now,wifi_join5_inspection};exchanged=mt76_mac_cycle_run(&wifi_join5.mac,&io);good=!wifi_join5.failed && wifi_join5.mac.mac_disabled && wifi_join5.mac.tx_drained && wifi_join5.mac.rx_drained && wifi_join5.mac.filter_restored && wifi_join5.mac.txop_restored && wifi_join5_owned(&wifi_join5,true);}
 if(wifi_join5.mac.attempted){
  const struct mt76_mac_cycle *m=&wifi_join5.mac;
  console_printf("wifi-join5-mac: complete=%u started=%u snapshot=%u disabled=%u tx_drained=%u rx_drained=%u filter_restored=%u txop_restored=%u operations=%u reads=%u writes=%u waits=%u filter=%x txop=%x alc=%x ibi=%x hold_us=%u\n",(uint64_t)m->complete,(uint64_t)m->started,(uint64_t)m->snapshot_complete,(uint64_t)m->mac_disabled,(uint64_t)m->tx_drained,(uint64_t)m->rx_drained,(uint64_t)m->filter_restored,(uint64_t)m->txop_restored,(uint64_t)m->operations,(uint64_t)m->reads,(uint64_t)m->writes,(uint64_t)m->waits,(uint64_t)m->filter,(uint64_t)m->txop,(uint64_t)m->tx_alc,(uint64_t)m->ibi9,m->hold_us);
 }
 if(good){wifi_join5.phase=4;good=wifi_join5_command(4,15,9,0);}
 if(good)good=wifi_join5_command(5,15,8,0);
 if(good)good=wifi_join5_cleanup_freeze();
 uint64_t old=wifi_join5.context[4][2]|((uint64_t)wifi_join5.context[4][3]<<32);
 if(good)good=wifi_join5_command(6,16,9,old);
 if(good){zero_bytes(d->input_context,4096);context_write32(d->input_context,0,0,1u<<8);context_write32(d->input_context,0,1,1);for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_join5.context[0][j]);copy_bytes(wifi_join5.expected,d->input_context,4096);good=wifi_join5_command(7,12,8,0);}
 if(good){copy_bytes(d->input_context,wifi_join5.input,4096);copy_bytes(wifi_join5.expected,d->input_context,4096);wifi_join5.phase=9;wifi_join5.restored=wifi_join5_owned(&wifi_join5,true) && wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) && wifi_tx5_owned();good=wifi_join5.restored;}
 struct mt76_join5 *s=&wifi_join5.exchange;
 console_printf("wifi-join5: ok=%u exchange=%u auth=%u assoc=%u leave=%u unsafe=%u sends=%u polls=%u ignored=%u aid=%u status=%u commands=%u posts=%u controls=%u restored=%u phase=%u retained=1 authenticated=0 ip_data=0\n",(uint64_t)(good && exchanged),(uint64_t)s->complete,(uint64_t)s->auth_matched,(uint64_t)s->assoc_matched,(uint64_t)s->leave_sent,(uint64_t)s->unsafe,(uint64_t)wifi_join5.sends,(uint64_t)s->polls,(uint64_t)s->ignored,(uint64_t)s->aid,(uint64_t)s->status,(uint64_t)wifi_join5.commands,(uint64_t)wifi_join5.posts,(uint64_t)wifi_join5.usb_ops,(uint64_t)wifi_join5.restored,(uint64_t)wifi_join5.phase);
 /* Result-only metadata is emitted after the existing cleanup/summary. */
 wifi_join5_emit_responses();wifi_join5_cleanup_emit();
 if(!wifi_join5.restored && !wifi_join5.failed)wifi_join5_fail();
 if(good && exchanged && wifi_join5_after)good=wifi_join5_after();
 if(wifi_join5.cleanup_enabled && (!wifi_join5_cleanup_held() || !wifi_join5_owned(&wifi_join5,true)))good=wifi_join5_fail();
 wifi_join5.active=false;return good && exchanged;
}
bool xhci_wifi_capture_join5(void){if(wifi_join5_requested || wifi_join5.attempted)return false;wifi_join5_requested=true;bool good=xhci_wifi_capture_tx5();wifi_join5_requested=false;return good;}

bool xhci_wifi_capture_eapol5(void){if(wifi_eapol5.requested || wifi_eapol5.attempted)return false;wifi_eapol5.requested=true;bool good=xhci_wifi_capture_join5();wifi_eapol5.requested=false;return good;}
