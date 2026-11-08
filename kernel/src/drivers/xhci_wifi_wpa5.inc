/* Independent child of the fully retired v128 owner. All DMA retained.
 * Keys are software-only;the credential initramfs store is separate from
 * ephemeral key erasure. Only the explicit data recipe enables the disposable
 * authenticated Ethernet diagnostic;the historical four-way recipe is inert. */
#include <srvros/mt76_wpa5.h>
#include <srvros/wpa2_nonce.h>
#include <srvros/wpa2_crypto.h>
#include <srvros/initramfs.h>
#include <srvros/mt76_data5.h>
#include <srvros/mt76_ip5.h>
#include <srvros/mt76_net5.h>
#include <srvros/mt76_dma5.h>
static bool wifi_wpa5_requested,wifi_wpa5_data_requested;
static bool wifi_provider5_requested,wifi_reuse5_requested;
static struct {struct mt76_dma5 dma;struct mt76_dma5_lease tx,rx,loan;bool initialized,early_refused,events_drained,retired;}wifi_reuse5;
static struct {struct mt76_net5 net;uint64_t token,epoch;bool attempted,registered,attached,selected,closed,early_refused,outer_retired,retired;}wifi_provider5;
static bool wifi_provider5_start(void);
static bool wifi_provider5_owned(void *);
static bool wifi_provider5_complete(void);
static bool wifi_provider5_close(void);
static enum mt76_join5_send wifi_provider5_send(void *,const uint8_t *,size_t);
#define WIFI_WPA5_TX_MAX 11
static unsigned wifi_wpa5_tx_limit(void){return wifi_wpa5_data_requested?WIFI_WPA5_TX_MAX:5;}
static unsigned wifi_wpa5_packet_limit(void){return wifi_wpa5_data_requested?MT76_DATA5_USB_MAX:184;}
static bool wifi_wpa5_inspect(void);
static bool wifi_data5_owned(void *);
#define WIFI_WPA5_RX 48
#define WIFI_DATA5_FORWARD_RX 40
#define WIFI_DATA5_EVENTS (WIFI_DATA5_FORWARD_RX+6)
struct wifi_data5_event {unsigned kind,index,bytes,ethernet,present,view,reason,flags,frame,audit;};
static struct {
 struct xhci_device *d;struct mt76_peer5 peer;struct mt76_wpa5 exchange;struct mt76_mac_cycle mac;
 struct xhci_trb *rx,*tx;uint64_t rx_phys,tx_phys,pages[WIFI_WPA5_RX+WIFI_WPA5_TX_MAX],pins[WIFI_WPA5_RX+WIFI_WPA5_TX_MAX+2];
 uintptr_t pointers[WIFI_WPA5_RX+WIFI_WPA5_TX_MAX+2];uint8_t *buffer[WIFI_WPA5_RX+WIFI_WPA5_TX_MAX];
 uint8_t input[4096],expected[4096],cal[sizeof wifi_cal5],power[sizeof wifi_power5],handoff[sizeof wifi_handoff5],binding[sizeof wifi_peer5],preview[sizeof wifi_tx5];
 uint8_t old_ring[4096],old_pages[8][4096],handoff_ring[4096],preview_ring[4096];
 uint8_t rx_snapshot[WIFI_WPA5_RX][4096];unsigned rx_copied;uint8_t slot;
 uint32_t context[5][8];uint64_t fixed[9],stop_pointer[2];uintptr_t input_pointer,output_pointer;
 unsigned lengths[WIFI_WPA5_TX_MAX],kinds[WIFI_WPA5_TX_MAX];uint8_t packets[WIFI_WPA5_TX_MAX][MT76_DATA5_USB_MAX];
 struct mt76_data5_scratch data_scratch;struct mt76_ip5 ip;uint8_t data_usb[MT76_DATA5_USB_MAX];
 struct wifi_data5_event data_event[WIFI_DATA5_EVENTS];unsigned data_events;bool data_reported,data_unsafe,data_window_started;
 uint64_t data_pn;unsigned data_sends,data_received,forward_completed,drain_completed;bool data_mode,data_active;
 uint8_t parent[sizeof wifi_join5],parent_rings[2][4096],parent_pages[WIFI_JOIN5_RX+3][4096],pmk[32];
 unsigned phase,commands,posts,sends,rx_cc,rx_residue,rx_events,tx_events,stop_events[2],stop_dci,usb_ops;
 uint64_t rx_enqueue,tx_enqueue;bool rx_cycle,tx_cycle;
 bool toggle_attempted,toggle_synced,attempted,active,failed,rx_pending,rx_done,rx_delivered,tx_pending,tx_done,restored;
}wifi_wpa5;
static bool wifi_wpa5_fail(void){if(wifi_reuse5_requested)mt76_dma5_fault(&wifi_reuse5.dma);wifi_wpa5.failed=true;return wifi_wpa5.d?wifi_mcu_fail(wifi_wpa5.d):false;}
#include "xhci_wifi_reuse5.inc"
/* Bounded metadata only. Never print while the IP receive window is active.
 * Preserve append order for a strict deferred transcript after MAC cleanup. */
static bool wifi_data5_record(struct wifi_data5_event event){
 if(!wifi_wpa5.data_mode || !wifi_wpa5.data_active || wifi_wpa5.data_reported || wifi_wpa5.data_events>=WIFI_DATA5_EVENTS)return wifi_wpa5_fail();
 wifi_wpa5.data_event[wifi_wpa5.data_events++]=event;return true;
}
static bool wifi_wpa5_frozen(void){
 struct xhci_device *d=wifi_wpa5.d;
 if(wifi_wpa5.data_mode!=wifi_wpa5_data_requested || wifi_join5_after!=wifi_wpa5_inspect || !wifi_wpa5_requested || !wifi_wpa5.active || wifi_wpa5.failed || !d || d!=wifi_cal5.d || !d->present || d->removed || d->retiring || d->ep0_quarantined || d->control_failed ||
 d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 || !d->slot_id || d->slot_id!=wifi_wpa5.slot || xhci.stopping || firmware_command_failed ||
 !wifi_channel_vco5_owned(&wifi_channel_vco5) || !wifi_handoff5_equal(&wifi_cal5,wifi_wpa5.cal,sizeof wifi_cal5) ||
 !wifi_handoff5_equal(&wifi_power5,wifi_wpa5.power,sizeof wifi_power5) || !wifi_handoff5_equal(&wifi_handoff5,wifi_wpa5.handoff,sizeof wifi_handoff5) ||
 !wifi_handoff5_equal(&wifi_peer5,wifi_wpa5.binding,sizeof wifi_peer5) || !wifi_handoff5_equal(&wifi_tx5,wifi_wpa5.preview,sizeof wifi_tx5) ||
 !wifi_handoff5_equal(&wifi_peer5.binding,&wifi_wpa5.peer,sizeof wifi_wpa5.peer) || !wifi_handoff5_equal(d->input_context,wifi_wpa5.expected,4096) ||
 !wifi_handoff5_equal(wifi_cal5.ep.ring,wifi_wpa5.old_ring,4096) || !wifi_handoff5_equal(wifi_handoff5.ring,wifi_wpa5.handoff_ring,4096) ||
 !wifi_handoff5_equal(wifi_tx5.ring,wifi_wpa5.preview_ring,4096) || (uintptr_t)d->input_context!=wifi_wpa5.input_pointer || (uintptr_t)d->output_context!=wifi_wpa5.output_pointer)return false;
 const uint64_t fixed[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)if(fixed[i]!=wifi_wpa5.fixed[i])return false;
 for(unsigned i=0;i<8;i++)if(!wifi_handoff5_equal(wifi_cal5.plan.buffer[i],wifi_wpa5.old_pages[i],4096))return false;
 for(unsigned i=0;i<WIFI_WPA5_RX+wifi_wpa5_tx_limit()+2;i++){
  uint64_t p=i<2?(i?wifi_wpa5.tx_phys:wifi_wpa5.rx_phys):wifi_wpa5.pages[i-2];
  uintptr_t v=(uintptr_t)(i<2?(void *)(i?wifi_wpa5.tx:wifi_wpa5.rx):(void *)wifi_wpa5.buffer[i-2]);
  if(p!=wifi_wpa5.pins[i] || v!=wifi_wpa5.pointers[i] || (void *)v!=pmm_phys_to_virt(p))return false;
 }
 if(!wifi_handoff5_equal(&wifi_join5,wifi_wpa5.parent,sizeof wifi_join5) ||
 !wifi_handoff5_equal(wifi_join5.rx,wifi_wpa5.parent_rings[0],4096) || !wifi_handoff5_equal(wifi_join5.tx,wifi_wpa5.parent_rings[1],4096))return false;
 for(unsigned i=0;i<WIFI_JOIN5_RX+3;i++)if(!wifi_handoff5_equal(wifi_join5.buffer[i],wifi_wpa5.parent_pages[i],4096))return false;
 if(wifi_wpa5.phase && (!wifi_wpa5.toggle_attempted || !wifi_wpa5.toggle_synced))return false;
 if(wifi_wpa5.rx_copied>wifi_wpa5.posts)return false;
 if(wifi_reuse5_requested){if(!wifi_reuse5_frozen())return false;}else {
 for(unsigned i=0;i<wifi_wpa5.rx_copied;i++)if(!wifi_handoff5_equal(wifi_wpa5.buffer[i],wifi_wpa5.rx_snapshot[i],4096))return false;
 for(unsigned i=wifi_wpa5.posts;i<WIFI_WPA5_RX;i++)for(unsigned j=0;j<4096;j++)if(wifi_wpa5.buffer[i][j])return false;
 for(unsigned i=0;i<wifi_wpa5_tx_limit();i++){
  unsigned n=i<wifi_wpa5.sends?wifi_wpa5.lengths[i]:0;
  if(n>wifi_wpa5_packet_limit() || !wifi_handoff5_equal(wifi_wpa5.buffer[WIFI_WPA5_RX+i],wifi_wpa5.packets[i],n))return false;
  for(unsigned j=n;j<4096;j++)if(wifi_wpa5.buffer[WIFI_WPA5_RX+i][j])return false;
 }
 }
 const unsigned dcis[]={0,11,16};
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,dcis[i],j)!=wifi_wpa5.context[i][j])return false;
 return true;
}
static bool wifi_wpa5_ring(struct xhci_trb *ring,uint64_t phys,unsigned posts,bool rx){
 for(unsigned i=0;i<XHCI_TRB_COUNT;i++){
  uint64_t p=0;uint32_t status=0,control=0;
  if(i==XHCI_TRB_COUNT-1){p=phys;control=(XHCI_TRB_TYPE_LINK<<10)|3;}
  else if(i<posts){p=wifi_wpa5.pages[rx?wifi_wpa5_rx_index(i):wifi_wpa5_tx_index(i)];status=rx?4096:wifi_wpa5.lengths[i];control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|1|(rx?XHCI_TRB_ISP:0);}
  const struct xhci_trb *t=&ring[i];if(t->parameter_low!=(uint32_t)p || t->parameter_high!=(uint32_t)(p>>32) || t->status!=status || t->control!=control)return false;
 }
 return true;
}
static bool wifi_wpa5_context(void){
 struct xhci_device *d=wifi_wpa5.d;unsigned phase=wifi_wpa5.phase;if(phase>9)return false;
 for(unsigned j=0;j<8;j++){
  uint32_t tx=context_read32(d->output_context,8,j),rx=context_read32(d->output_context,9,j);
  if(phase==0){if(tx!=wifi_wpa5.context[3][j])return false;}
  else if(phase<8){
   uint64_t base=wifi_wpa5.tx_phys,deq=context_read32(d->output_context,8,2)|((uint64_t)context_read32(d->output_context,8,3)<<32);
   uint32_t want=j==0?(phase<6?1:3):j==1?(3u<<1)|(2u<<3)|(512u<<16):j==4?wifi_wpa5_packet_limit():0;
   if(j==2 || j==3){if((deq&15)!=1 || (deq&~15ull)<base || (deq&~15ull)>base+16*wifi_wpa5.sends || (phase>=6 && deq!=(base+16*wifi_wpa5.sends+1)))return false;}
   else if(tx!=want)return false;
  }else if(j==0 && (tx&7))return false;
  if(phase<2 || phase>=7){if(rx!=wifi_wpa5.context[4][j])return false;}
  else {
   uint32_t want=wifi_wpa5.context[4][j];
   if(j==0)want=(want&~7u)|(phase==2 || phase>=5?3:1);
   if(j==2 || j==3){uint64_t deq=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);if((deq&15)!=1 || (deq&~15ull)<wifi_wpa5.rx_phys || (deq&~15ull)>wifi_wpa5.rx_phys+16*wifi_wpa5.posts || (phase>=5 && deq!=(wifi_wpa5.stop_pointer[1]|1)))return false;}
   else if(rx!=want)return false;
  }
 }
 return true;
}
static bool wifi_wpa5_owned(void *c,bool cleanup){
 (void)cleanup;struct xhci_device *d=wifi_wpa5.d;
 bool good=c==&wifi_wpa5 && wifi_wpa5_frozen() && d && !d->control_pending && !d->firmware.pending && !d->firmware.mcu.pending && !xhci.sync_users && !xhci.command_pending && !xhci.command_failed && !strict_control.device &&
 wifi_wpa5.posts<=WIFI_WPA5_RX && wifi_wpa5.sends<=wifi_wpa5_tx_limit() && wifi_wpa5.rx_enqueue==wifi_wpa5.posts && wifi_wpa5.tx_enqueue==wifi_wpa5.sends && wifi_wpa5.rx_cycle && wifi_wpa5.tx_cycle &&
 wifi_wpa5_ring(wifi_wpa5.rx,wifi_wpa5.rx_phys,wifi_wpa5.posts,true) && wifi_wpa5_ring(wifi_wpa5.tx,wifi_wpa5.tx_phys,wifi_wpa5.sends,false) && wifi_wpa5_context();
 if(!good && wifi_wpa5.active)wifi_wpa5_fail();
 return good;
}
static bool wifi_wpa5_event(struct xhci_device *d,const struct xhci_trb *e){
 if(!wifi_wpa5.active)return false;
 unsigned dci=(e->control>>16)&31;uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);
 bool ours=dci==8 || dci==9;
 if(d!=wifi_wpa5.d || !ours){
  if((ptr>=wifi_wpa5.rx_phys && ptr<wifi_wpa5.rx_phys+1024) || (ptr>=wifi_wpa5.tx_phys && ptr<wifi_wpa5.tx_phys+1024)){wifi_wpa5_fail();return true;}
  return false;
 }
 unsigned cc=e->status>>24,residue=e->status&0xffffff;const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 bool good=wifi_wpa5_frozen() && wifi_wpa5_ring(wifi_wpa5.rx,wifi_wpa5.rx_phys,wifi_wpa5.posts,true) && wifi_wpa5_ring(wifi_wpa5.tx,wifi_wpa5.tx_phys,wifi_wpa5.sends,false) && ((e->control>>10)&63)==32 && (e->control>>24)==d->slot_id && !(e->control&~allowed);
 if(good && cc>=26 && cc<=28){
  unsigned n=dci==8?0:1;uint64_t expected=(dci==8?wifi_wpa5.tx_phys:wifi_wpa5.rx_phys)+16*(dci==8?wifi_wpa5.sends:wifi_wpa5.posts);
  bool pending=dci==9 && wifi_wpa5.rx_pending;
  good=firmware_command_exact && wifi_wpa5.phase==(dci==9?4u:5u) && wifi_wpa5.stop_dci==dci && !wifi_wpa5.stop_events[n] &&
   ((cc==27 && !residue && ptr==expected) || (pending && cc!=27 && residue<=4096 && ptr==expected-16));
  wifi_wpa5.stop_events[n]++;
  if(good){wifi_wpa5.stop_pointer[n]=ptr;if(dci==9)wifi_wpa5.rx_pending=false;}
  console_printf("wifi-wpa5-stop-event: dci=%u cc=%u residue=%u pointer=%x ok=%u\n",(uint64_t)dci,(uint64_t)cc,(uint64_t)residue,ptr,(uint64_t)good);
 }else if(good && dci==8){
  good=wifi_wpa5.phase==3 && wifi_wpa5.tx_pending && wifi_wpa5.sends && ptr==wifi_wpa5.tx_phys+16*(wifi_wpa5.sends-1) && cc==1 && !residue && !wifi_wpa5.tx_events;
  if(good){
   if(wifi_reuse5_requested && !wifi_reuse5_complete(0,cc,residue))return true;
   wifi_wpa5.tx_events++;wifi_wpa5.tx_pending=false;wifi_wpa5.tx_done=true;
   if(wifi_provider5_requested && wifi_wpa5.kinds[wifi_wpa5.sends-1]==5)good=wifi_provider5_complete();
  }
 }else if(good){
  good=(wifi_wpa5.phase==3 || (wifi_wpa5.phase==4 && wifi_wpa5.stop_dci==9)) && wifi_wpa5.rx_pending && wifi_wpa5.posts && ptr==wifi_wpa5.rx_phys+16*(wifi_wpa5.posts-1) && (cc==1 || cc==13) && residue<=4096 && !wifi_wpa5.rx_events;
  if(good){if(wifi_reuse5_requested && !wifi_reuse5_complete(1,cc,residue))return true;wifi_wpa5.rx_events++;wifi_wpa5.rx_pending=false;wifi_wpa5.rx_done=true;wifi_wpa5.rx_cc=cc;wifi_wpa5.rx_residue=residue;}
 }
 if(!good)wifi_wpa5_fail();
 return true;
}
static void wifi_wpa5_receipt(unsigned phase){
 for(unsigned dci=8;dci<=9;dci++){
  struct xhci_device *d=wifi_wpa5.d;
  console_printf("wifi-wpa5-context: phase=%u dci=%u w0=%x w1=%x w2=%x w3=%x w4=%x w5=%x w6=%x w7=%x\n",(uint64_t)phase,(uint64_t)dci,(uint64_t)context_read32(d->output_context,dci,0),(uint64_t)context_read32(d->output_context,dci,1),(uint64_t)context_read32(d->output_context,dci,2),(uint64_t)context_read32(d->output_context,dci,3),(uint64_t)context_read32(d->output_context,dci,4),(uint64_t)context_read32(d->output_context,dci,5),(uint64_t)context_read32(d->output_context,dci,6),(uint64_t)context_read32(d->output_context,dci,7));
 }
}
/* USB2 CLEAR_FEATURE(ENDPOINT_HALT) resets the device toggle even when it
 * is not stalled. The following fresh host Configure Endpoint starts DATA0.
 * No Reset Endpoint command on a non-halted context,no retry or extra TD. */
static bool wifi_wpa5_toggle(void){
 struct xhci_device *d=wifi_wpa5.d;
 if(wifi_wpa5.phase || wifi_wpa5.commands || wifi_wpa5.posts || wifi_wpa5.sends || wifi_wpa5.mac.attempted || wifi_wpa5.toggle_attempted || !wifi_wpa5_owned(&wifi_wpa5,false))return false;
 wifi_wpa5.toggle_attempted=true;
 struct usb_setup_packet setup={.request_type=0x02,.request=1,.value=0,.index=4,.length=0};int actual=-1;
 d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
 bool good=wifi_fw_control_once(d,&setup,0,0,false,&actual) && actual==0 && wifi_wpa5_owned(&wifi_wpa5,false);
 wifi_wpa5.toggle_synced=good;
 console_printf("wifi-wpa5-toggle: type=2 request=1 value=0 index=4 length=0 actual=%u ok=%u\n",(uint64_t)(actual<0?UINT32_MAX:(unsigned)actual),(uint64_t)good);
 return good?true:wifi_wpa5_fail();
}
static bool wifi_wpa5_command(unsigned phase,unsigned type,unsigned dci,uint64_t deq){
 struct xhci_device *d=wifi_wpa5.d;if(!wifi_wpa5.toggle_synced || wifi_wpa5.phase!=phase || !wifi_wpa5_owned(&wifi_wpa5,true))return wifi_wpa5_fail();
 struct xhci_trb cmd={.control=(type<<10)|((uint32_t)d->slot_id<<24)};
 if(type==12){cmd.parameter_low=d->input_context_phys;cmd.parameter_high=d->input_context_phys>>32;}
 else {cmd.control|=dci<<16;cmd.parameter_low=deq;cmd.parameter_high=deq>>32;}
 uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;wifi_wpa5.commands++;wifi_wpa5.stop_dci=type==15?dci:0;
 bool good=submit_command_trb(&cmd,type,&slot) && slot==d->slot_id;
 wifi_wpa5.stop_dci=0;firmware_command_exact=false;
 if(type==15)good=good && wifi_wpa5.stop_events[dci==8?0:1]==1;
 if(!good)firmware_command_failed=true;
 if(good)wifi_wpa5.phase++;
 event_irq_restore(flags);
 console_printf("wifi-wpa5-command: phase=%u type=%u dci=%u slot=%u ok=%u\n",(uint64_t)phase,(uint64_t)type,(uint64_t)dci,slot,(uint64_t)good);
 wifi_wpa5_receipt(wifi_wpa5.phase);
 return good && wifi_wpa5_owned(&wifi_wpa5,true)?true:wifi_wpa5_fail();
}
static bool wifi_wpa5_post(void){
 uint64_t serial=wifi_reuse5_requested?event_irq_save():0;
 if(wifi_wpa5.data_mode && wifi_wpa5.posts>=WIFI_DATA5_FORWARD_RX && (!wifi_wpa5.mac.mac_disabled || wifi_wpa5.data_active || !wifi_wpa5.exchange.keys_erased || wifi_wpa5.tx_pending)){if(wifi_reuse5_requested)event_irq_restore(serial);return false;}
 if(!wifi_wpa5.toggle_synced || !wifi_wpa5_owned(&wifi_wpa5,false) || wifi_wpa5.posts>=WIFI_WPA5_RX || wifi_wpa5.rx_pending || (wifi_wpa5.rx_done && !wifi_wpa5.rx_delivered)){if(wifi_reuse5_requested)event_irq_restore(serial);return false;}
 unsigned i=wifi_wpa5.posts;if(wifi_reuse5_requested && !wifi_reuse5_prepare(1,i,4096)){if(wifi_reuse5_requested)event_irq_restore(serial);return false;}
 unsigned page=wifi_wpa5_rx_index(i);struct xhci_trb td={.parameter_low=wifi_wpa5.pages[page],.parameter_high=wifi_wpa5.pages[page]>>32,.status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
 uint64_t flags=event_irq_save();if(wifi_reuse5_requested && !wifi_reuse5_publish(1)){event_irq_restore(flags);event_irq_restore(serial);return false;}wifi_wpa5.posts++;wifi_wpa5.rx_pending=true;wifi_wpa5.rx_done=wifi_wpa5.rx_delivered=false;wifi_wpa5.rx_events=0;
 uint64_t ptr=ring_push(wifi_wpa5.rx,wifi_wpa5.rx_phys,&wifi_wpa5.rx_enqueue,&wifi_wpa5.rx_cycle,&td);
 bool good=ptr==wifi_wpa5.rx_phys+16*i;if(good){if(wifi_wpa5.phase==2)wifi_wpa5.phase=3;db_write32(wifi_wpa5.d->slot_id,9);}event_irq_restore(flags);if(wifi_reuse5_requested)event_irq_restore(serial);
 if(good && i==0){
  uint64_t start=timer_monotonic_us();bool running=false;
  for(unsigned n=0;n<512;n++){
   if(!wifi_wpa5_frozen())break;
   if((context_read32(wifi_wpa5.d->output_context,9,0)&7)==1){running=true;break;}
   uint64_t now=timer_monotonic_us();if(now<start || now-start>=50000 || !timer_delay_us(100))break;
  }
  good=running && wifi_wpa5_owned(&wifi_wpa5,false);
 }
 return good?true:wifi_wpa5_fail();
}
static void wifi_wpa5_dispatch(void){uint64_t flags=event_irq_save();struct xhci_trb e;if(poll_event(&e))dispatch_transfer_event(&e);event_irq_restore(flags);}
static enum mt76_join5_send wifi_wpa5_publish(void *c,const uint8_t *packet,size_t length,unsigned kind){
 uint64_t serial=wifi_reuse5_requested?event_irq_save():0;
 unsigned n=wifi_wpa5.sends;
 if(!wifi_wpa5_owned(c,false) || wifi_wpa5.phase!=3 || n>=wifi_wpa5_tx_limit() || length>wifi_wpa5_packet_limit()){if(wifi_reuse5_requested)event_irq_restore(serial);return MT76_JOIN5_REFUSED;}
 if(wifi_reuse5_requested && !wifi_reuse5_prepare(0,n,(unsigned)length)){event_irq_restore(serial);return MT76_JOIN5_UNCERTAIN;}
 unsigned page=wifi_wpa5_tx_index(n);copy_bytes(wifi_wpa5.buffer[page],packet,length);copy_bytes(wifi_wpa5.packets[n],packet,length);wifi_wpa5.lengths[n]=length;wifi_wpa5.kinds[n]=kind;
 struct xhci_trb td={.parameter_low=wifi_wpa5.pages[page],.parameter_high=wifi_wpa5.pages[page]>>32,.status=length,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC};
 uint64_t flags=event_irq_save();if(wifi_reuse5_requested && !wifi_reuse5_publish(0)){event_irq_restore(flags);event_irq_restore(serial);return MT76_JOIN5_UNCERTAIN;}wifi_wpa5.sends++;wifi_wpa5.tx_pending=true;wifi_wpa5.tx_done=false;wifi_wpa5.tx_events=0;
 uint64_t ptr=ring_push(wifi_wpa5.tx,wifi_wpa5.tx_phys,&wifi_wpa5.tx_enqueue,&wifi_wpa5.tx_cycle,&td);bool good=ptr==wifi_wpa5.tx_phys+16*n;
 if(good)db_write32(wifi_wpa5.d->slot_id,8);
 event_irq_restore(flags);if(wifi_reuse5_requested)event_irq_restore(serial);
 uint64_t start=timer_monotonic_us(),previous=start;
 for(unsigned i=0;good && !wifi_wpa5.tx_done && i<8192;i++){
  wifi_wpa5_dispatch();good=wifi_wpa5_owned(c,false);uint64_t now=timer_monotonic_us();
  if(now<previous || now-start>=750000){good=false;break;}previous=now;
  if(good && !wifi_wpa5.tx_done)good=timer_delay_us(100);
 }
 good=good && wifi_wpa5.tx_done && wifi_wpa5.tx_events==1;
 if(kind<5)console_printf("wifi-wpa5-send: kind=%u bytes=%u completed=%u replay=0\n",(uint64_t)kind,(uint64_t)length,(uint64_t)good);
 else if(!wifi_data5_record((struct wifi_data5_event){.kind=0,.index=wifi_wpa5.data_sends,.bytes=(unsigned)length,.present=good,.reason=(unsigned)wifi_wpa5.data_pn}))good=false;
 if(!good){wifi_wpa5_fail();return MT76_JOIN5_UNCERTAIN;}return MT76_JOIN5_SENT;
}
static enum mt76_join5_send wifi_wpa5_send(void *c,enum mt76_wpa5_tx kind,const uint8_t *packet,size_t length){
 unsigned n=wifi_wpa5.sends;uint8_t expected[184]={0};size_t want=0;
 struct mt76_wpa5 *s=&wifi_wpa5.exchange;
 bool order=(kind==MT76_WPA5_AUTH && n==0) || (kind==MT76_WPA5_ASSOC && n==1 && s->auth_matched) ||
  (kind==MT76_WPA5_M2 && n==2 && s->m1_matched && s->handshake.phase==WPA2_HS_M2_PENDING) ||
  (kind==MT76_WPA5_M4 && n==3 && s->m3_mic && s->keys_installed && s->keys.phase==WPA2_CCMP_INSTALLED && s->handshake.phase==WPA2_HS_M4_PENDING) ||
  (kind==MT76_WPA5_LEAVE && n>=1 && n<=(wifi_wpa5.data_mode?wifi_wpa5_tx_limit()-1:4) && s->auth_sent && s->keys_erased && s->handshake.phase==WPA2_HS_CLOSED);
 if(!wifi_wpa5.toggle_synced || c!=&wifi_wpa5 || packet!=s->packet || !order || length>184 || wifi_wpa5.phase!=3 || n>=wifi_wpa5_tx_limit() || !wifi_wpa5.mac.started || !wifi_wpa5.mac.snapshot_complete || !wifi_wpa5_owned(c,false))return MT76_JOIN5_REFUSED;
 if(kind==MT76_WPA5_M2 || kind==MT76_WPA5_M4){
  bool m2=kind==MT76_WPA5_M2;unsigned e=m2?121:99;
  struct mt76_eapol5_peer ep={.channel=161,.bandwidth=20};copy_bytes(ep.station,wifi_wpa5.peer.peer.station,6);copy_bytes(ep.bssid,wifi_wpa5.peer.peer.bssid,6);
  if(length==(m2?184u:160u) && wpa2_eapol_mic_verify(s->handshake.ptk,packet+56,e))want=mt76_eapol5_usb_tx(&ep,packet+56,e,(unsigned)kind+4,expected,sizeof expected);
  zero_bytes(&ep,sizeof ep);
 }else{
  enum mt76_mgmt5_kind k=kind==MT76_WPA5_AUTH?MT76_MGMT5_AUTH:kind==MT76_WPA5_ASSOC?MT76_MGMT5_ASSOC:MT76_MGMT5_LEAVE;
  want=mt76_mgmt5_usb_request(&wifi_wpa5.peer.peer,k,(unsigned)kind+4,false,expected,sizeof expected);
 }
 bool match=want && length==want && wifi_handoff5_equal(packet,expected,want);zero_bytes(expected,sizeof expected);if(!match)return MT76_JOIN5_REFUSED;
 return wifi_wpa5_publish(c,packet,length,(unsigned)kind);
}
static bool wifi_wpa5_poll_once(void *c,enum mt76_wpa5_rx type,uint8_t *out,size_t cap,size_t *size,bool *present){
 if(!out || !size || !present || c!=&wifi_wpa5 || !wifi_wpa5_owned(c,false))return false;
 *size=0;*present=false;wifi_wpa5_dispatch();if(!wifi_wpa5_owned(c,false))return false;
 if(wifi_wpa5.rx_done && !wifi_wpa5.rx_delivered){
  unsigned i=wifi_wpa5.posts-1;
  if(i!=wifi_wpa5.rx_copied)return wifi_wpa5_fail();
  if(wifi_reuse5_requested && !wifi_reuse5_acquire())return false;
  unsigned page=wifi_wpa5_rx_index(i);copy_bytes(wifi_wpa5.rx_snapshot[i],wifi_wpa5.buffer[page],4096);wifi_wpa5.rx_copied++;
  size_t bytes=4096-wifi_wpa5.rx_residue;
  unsigned flags=0,frame=0,audit=0;unsigned view=MT76_RX_INVALID;enum mt76_data5_rx_reason reason=MT76_DATA5_ADMISSION;size_t mpdu=0,eapol=0;unsigned fc=0;
  if(type==MT76_WPA5_EAPOL){
   struct mt76_eapol5_peer p={.channel=161,.bandwidth=20};copy_bytes(p.station,wifi_wpa5.peer.peer.station,6);copy_bytes(p.bssid,wifi_wpa5.peer.peer.bssid,6);
   struct mt76_eapol5_view v={0};view=mt76_eapol5_rx(&p,wifi_wpa5.buffer[page],bytes,&v);mpdu=v.mpdu_size;fc=v.frame_control;eapol=v.eapol_size;
   if(view==MT76_RX_OK && eapol<=cap){copy_bytes(out,v.eapol,eapol);*size=eapol;*present=true;}zero_bytes(&p,sizeof p);
  }else if(type==MT76_WPA5_MANAGEMENT){
   struct mt76_rx_view v={0};view=mt76_rx_usb_view(wifi_wpa5.buffer[page],bytes,&v);mpdu=v.mpdu_size;fc=v.frame_control;
   if(view==MT76_RX_OK && mpdu>=24 && mpdu<=cap && v.body_size==mpdu-24){copy_bytes(out,v.header,24);copy_bytes(out+24,v.body,v.body_size);*size=mpdu;*present=true;}
  }else if(type==MT76_WPA5_DATA && wifi_wpa5.data_mode && wifi_wpa5.data_active && wifi_wpa5.exchange.keys_activated && wifi_wpa5.exchange.keys.phase==WPA2_CCMP_ACTIVE){
   if(!wifi_data5_owned(c))return wifi_wpa5_fail();
   struct mt76_data5_rx_result report=mt76_data5_rx_report(&wifi_wpa5.data_scratch,&wifi_wpa5.exchange.keys,wifi_wpa5.peer.generation,wifi_wpa5.buffer[page],bytes,out,cap);
   if(!wifi_data5_owned(c))return wifi_wpa5_fail();
   size_t n=report.bytes;reason=report.reason;flags=report.flags;frame=report.frame;audit=report.audit;if(reason==MT76_DATA5_ADMISSION)return wifi_wpa5_fail();
   view=n?MT76_RX_OK:MT76_RX_INVALID;
   if(n){*size=n;*present=true;wifi_wpa5.data_received++;}
  }else return wifi_wpa5_fail();
  wifi_wpa5.rx_delivered=true;
  if(type==MT76_WPA5_DATA){if(!wifi_data5_record((struct wifi_data5_event){.kind=1,.index=i,.bytes=(unsigned)bytes,.ethernet=(unsigned)*size,.present=*present,.view=view,.reason=reason,.flags=flags,.frame=frame,.audit=audit}))return false;}
  else console_printf("wifi-wpa5-rx: index=%u bytes=%u type=%u view=%u mpdu=%u fc=%x eapol=%u present=%u\n",(uint64_t)i,(uint64_t)bytes,(uint64_t)type,(uint64_t)view,(uint64_t)mpdu,(uint64_t)fc,(uint64_t)eapol,(uint64_t)*present);
  if(wifi_reuse5_requested && !wifi_reuse5_release())return false;
  if(wifi_wpa5.posts<(wifi_wpa5.data_mode?WIFI_DATA5_FORWARD_RX:WIFI_WPA5_RX) && !wifi_wpa5_post())return false;
 }
 return true;
}
static bool wifi_wpa5_poll(void *c,enum mt76_wpa5_rx type,uint8_t *out,size_t cap,size_t *size,bool *present){
 uint64_t flags=wifi_reuse5_requested?event_irq_save():0;
 bool good=wifi_wpa5_poll_once(c,type,out,cap,size,present);
 if(wifi_reuse5_requested)event_irq_restore(flags);
 return good;
}
static bool wifi_wpa5_delay(void *c,unsigned us){return c==&wifi_wpa5 && us==100 && wifi_wpa5_owned(c,false) && timer_delay_us(us);}
static uint64_t wifi_wpa5_now(void *c){(void)c;return timer_monotonic_us();}
/* Forward reception cannot spend the eight reserved immutable entries.
 * Once MAC is disabled and ephemeral keys are erased,drain only into the same
 * retained pages. Never parse/decrypt/publish data or reuse a ring slot here. */
static bool wifi_wpa5_drain_rx_once(void){
 if(!wifi_wpa5.data_mode || !wifi_wpa5.mac.mac_disabled)return true;
 if(wifi_wpa5.phase!=3 || wifi_wpa5.data_active || !wifi_wpa5.exchange.keys_erased || wifi_wpa5.tx_pending ||
    wifi_wpa5.rx_copied!=wifi_wpa5.forward_completed+wifi_wpa5.drain_completed || !wifi_wpa5_owned(&wifi_wpa5,true))return wifi_wpa5_fail();
 if(!wifi_wpa5.rx_pending && (!wifi_wpa5.rx_done || wifi_wpa5.rx_delivered) && wifi_wpa5.posts<WIFI_WPA5_RX && !wifi_wpa5_post())return false;
 wifi_wpa5_dispatch();
 if(!wifi_wpa5_owned(&wifi_wpa5,true))return false;
 if(wifi_wpa5.rx_done && !wifi_wpa5.rx_delivered){
  unsigned i=wifi_wpa5.posts-1;if(i!=wifi_wpa5.rx_copied)return wifi_wpa5_fail();
  if(wifi_reuse5_requested && !wifi_reuse5_acquire())return false;
  unsigned page=wifi_wpa5_rx_index(i);copy_bytes(wifi_wpa5.rx_snapshot[i],wifi_wpa5.buffer[page],4096);wifi_wpa5.rx_copied++;wifi_wpa5.drain_completed++;wifi_wpa5.rx_delivered=true;
  console_printf("wifi-retire5-rx: index=%u bytes=%u\n",(uint64_t)i,(uint64_t)(4096-wifi_wpa5.rx_residue));
  if(wifi_reuse5_requested && !wifi_reuse5_release())return false;
  if(wifi_wpa5.posts<WIFI_WPA5_RX && !wifi_wpa5_post())return false;
 }
 return wifi_wpa5_owned(&wifi_wpa5,true);
}
static bool wifi_wpa5_drain_rx(void){
 uint64_t flags=wifi_reuse5_requested?event_irq_save():0;bool good=wifi_wpa5_drain_rx_once();
 if(wifi_reuse5_requested)event_irq_restore(flags);
 return good;
}
static bool wifi_wpa5_usb(void *c,bool write,uint32_t a,uint32_t *v,bool cleanup){
 if(!v || c!=&wifi_wpa5 || !wifi_wpa5_owned(c,cleanup))return false;
 switch(a){case 0x1004:case 0x2000:case 0x208:case 0x1400:case 0x1340:case 0x13b0:case 0x2124:case 0x438:case 0xa30:case 0xa34:case 0x1200:case 0x430:break;default:return false;}
 if(write && a!=0x1004 && a!=0x1400 && a!=0x1340)return false;
 wifi_wpa5.usb_ops++;return wifi_mac_private_usb(wifi_wpa5.d,write,a,v) && wifi_wpa5_owned(c,cleanup)?true:wifi_wpa5_fail();
}
static bool wifi_wpa5_read(void *c,uint32_t a,uint32_t *v,bool cleanup){if(!v)return false;*v=0;return (!cleanup || wifi_wpa5_drain_rx()) && wifi_wpa5_usb(c,false,a,v,cleanup);}
static bool wifi_wpa5_write(void *c,uint32_t a,uint32_t v,bool cleanup){return wifi_wpa5_usb(c,true,a,&v,cleanup);}
static bool wifi_wpa5_mac_delay(void *c,unsigned us,bool cleanup){return us<=10000 && (!cleanup || wifi_wpa5_drain_rx()) && wifi_wpa5_owned(c,cleanup) && timer_delay_us(us);}
static bool wifi_wpa5_nonce(void *c,uint8_t out[32]){return c==&wifi_wpa5 && out==wifi_wpa5.exchange.snonce && wifi_wpa5_owned(c,false) && wpa2_nonce_generate(out);}
static bool wifi_data5_owned(void *c){
 const struct wpa2_ccmp *k=&wifi_wpa5.exchange.keys;const struct wpa2_handshake *h=&wifi_wpa5.exchange.handshake;const struct mt76_mgmt5_peer *p=&wifi_wpa5.peer.peer;
 return c==&wifi_wpa5 && wifi_wpa5.data_mode && wifi_wpa5.data_active && wifi_wpa5.exchange.keys_activated &&
 k->phase==WPA2_CCMP_ACTIVE && k->generation==wifi_wpa5.peer.generation && k->tx_pn==wifi_wpa5.data_pn && k->tx_pn<=6 &&
 k->peer.rsne_size==p->rsn_size && p->rsn_size<=64 && wifi_handoff5_equal(k->peer.aa,p->bssid,6) && wifi_handoff5_equal(k->peer.spa,p->station,6) && wifi_handoff5_equal(k->peer.rsne,p->rsn,p->rsn_size) &&
 h->phase==WPA2_HS_COMPLETE && h->generation==k->generation && h->key_owner==(uintptr_t)k && h->emissions==2 && h->install_handoffs==1 && h->keys_may_live && !h->unsafe && wifi_wpa5_owned(c,false);
}
static enum mt76_join5_send wifi_data5_send(void *c,const uint8_t *frame,size_t length){
 if(wifi_provider5_requested)return wifi_provider5_send(c,frame,length);
 struct wpa2_ccmp *k=&wifi_wpa5.exchange.keys;
 if(!wifi_data5_owned(c) || frame!=wifi_wpa5.ip.tx || wifi_wpa5.data_sends>=6 || wifi_wpa5.sends!=4+wifi_wpa5.data_sends || k->tx_pn!=wifi_wpa5.data_pn)return MT76_JOIN5_REFUSED;
 size_t n=mt76_data5_tx(&wifi_wpa5.data_scratch,k,wifi_wpa5.peer.generation,frame,length,9+wifi_wpa5.data_sends,wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);
 if(!n || k->tx_pn!=wifi_wpa5.data_pn+1)return MT76_JOIN5_REFUSED;
 wifi_wpa5.data_pn=k->tx_pn;
 enum mt76_join5_send result=wifi_wpa5_publish(c,wifi_wpa5.data_usb,n,5);
 zero_bytes(wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);
 if(result==MT76_JOIN5_SENT)wifi_wpa5.data_sends++;
 return result;
}
static bool wifi_data5_exhausted(void *c){
 return wifi_data5_owned(c) && wifi_wpa5.posts==WIFI_DATA5_FORWARD_RX && !wifi_wpa5.rx_pending && wifi_wpa5.rx_done && wifi_wpa5.rx_delivered;
}
static bool wifi_data5_poll(void *c,uint8_t *out,size_t cap,size_t *n,bool *present){return wifi_data5_owned(c) && out==wifi_wpa5.ip.rx && cap==sizeof wifi_wpa5.ip.rx && wifi_wpa5_poll(c,MT76_WPA5_DATA,out,cap,n,present);}
static enum mt76_join5_send wifi_data5_window(void *c,struct wpa2_ccmp *keys,uint64_t generation,const struct mt76_peer5 *peer){
 if(c!=&wifi_wpa5 || !wifi_wpa5.data_mode || wifi_wpa5.data_active || wifi_wpa5.ip.attempted || keys!=&wifi_wpa5.exchange.keys || generation!=wifi_wpa5.peer.generation || peer!=&wifi_wpa5.peer || wifi_wpa5.sends!=4 || !wifi_wpa5_owned(c,false))return MT76_JOIN5_REFUSED;
 wifi_wpa5.data_active=true;wifi_wpa5.data_window_started=true;
 uint8_t entropy[32]={0};bool good=wifi_data5_owned(c) && wpa2_nonce_generate(entropy) && wifi_data5_owned(c);
 uint32_t xid=(uint32_t)entropy[0]<<24|(uint32_t)entropy[1]<<16|(uint32_t)entropy[2]<<8|entropy[3];zero_bytes(entropy,sizeof entropy);
 if(good && wifi_provider5_requested)good=wifi_provider5_start();
 const struct mt76_ip5_io io={c,wifi_provider5_requested?wifi_provider5_owned:wifi_data5_owned,wifi_data5_send,wifi_data5_poll,wifi_wpa5_delay,wifi_wpa5_now,wifi_data5_exhausted};
 console_write("wifi-data5-progress: authenticated encrypted DHCP,ARP,DNS example.com and256-byte gateway echo;bounded waits;then erase and leave\n");
 if(good)good=mt76_ip5_run(&wifi_wpa5.ip,peer->peer.station,xid,&io);
 bool unsafe=wifi_wpa5.ip.unsafe || !wifi_data5_owned(c);
 if(wifi_provider5_requested && !wifi_provider5_close())unsafe=true;
 wifi_wpa5.data_unsafe=unsafe;wifi_wpa5.data_active=false;
 return unsafe?MT76_JOIN5_UNCERTAIN:good?MT76_JOIN5_SENT:MT76_JOIN5_REFUSED;
}
static void wifi_data5_report(void){
 if(!wifi_wpa5.data_mode || !wifi_wpa5.data_window_started || wifi_wpa5.data_reported)return;
 wifi_wpa5.data_reported=true;
 unsigned count=wifi_wpa5.data_events;
 /* A corrupt count cannot read past the retained transcript or claim success. */
 if(count>WIFI_DATA5_EVENTS){wifi_wpa5_fail();return;}
 console_printf("wifi-data5-report: deferred=1 events=%u\n",(uint64_t)count);
 for(unsigned i=0;i<count;i++){
  const struct wifi_data5_event *e=&wifi_wpa5.data_event[i];
  if(e->kind==0)console_printf("wifi-data5-send: index=%u bytes=%u completed=%u pn=%u\n",(uint64_t)e->index,(uint64_t)e->bytes,(uint64_t)e->present,(uint64_t)e->reason);
  else if(e->kind==1)console_printf("wifi-data5-rx: index=%u bytes=%u ethernet=%u present=%u view=%u reason=%u flags=%u frame=%u audit=%u\n",(uint64_t)e->index,(uint64_t)e->bytes,(uint64_t)e->ethernet,(uint64_t)e->present,(uint64_t)e->view,(uint64_t)e->reason,(uint64_t)e->flags,(uint64_t)e->frame,(uint64_t)e->audit);
  else {wifi_wpa5_fail();return;}
 }
 const struct mt76_ip5 *s=&wifi_wpa5.ip;
 for(unsigned phase=0;phase<2;phase++)console_printf("wifi-data5-dhcp: phase=%u ethernet_span=%u ethertype=%u ethernet_destination=%u ethernet_source=%u ip_span=%u fragment=%u ttl=%u protocol=%u ip_checksum=%u ip_binding=%u udp_span=%u udp_ports=%u udp_checksum=%u bootp_span=%u bootp_header=%u xid=%u station=%u cookie=%u option_span=%u option_duplicate=%u option_value=%u padding=%u required=%u end=%u message=%u address=%u ack=%u destination=%u matched=%u offers=%u acks=%u naks=%u other_types=%u reflected=%u\n",(uint64_t)phase,(uint64_t)s->dhcp_class[phase][0],(uint64_t)s->dhcp_class[phase][1],(uint64_t)s->dhcp_class[phase][2],(uint64_t)s->dhcp_class[phase][3],(uint64_t)s->dhcp_class[phase][4],(uint64_t)s->dhcp_class[phase][5],(uint64_t)s->dhcp_class[phase][6],(uint64_t)s->dhcp_class[phase][7],(uint64_t)s->dhcp_class[phase][8],(uint64_t)s->dhcp_class[phase][9],(uint64_t)s->dhcp_class[phase][10],(uint64_t)s->dhcp_class[phase][11],(uint64_t)s->dhcp_class[phase][12],(uint64_t)s->dhcp_class[phase][13],(uint64_t)s->dhcp_class[phase][14],(uint64_t)s->dhcp_class[phase][15],(uint64_t)s->dhcp_class[phase][16],(uint64_t)s->dhcp_class[phase][17],(uint64_t)s->dhcp_class[phase][18],(uint64_t)s->dhcp_class[phase][19],(uint64_t)s->dhcp_class[phase][20],(uint64_t)s->dhcp_class[phase][21],(uint64_t)s->dhcp_class[phase][22],(uint64_t)s->dhcp_class[phase][23],(uint64_t)s->dhcp_class[phase][24],(uint64_t)s->dhcp_class[phase][25],(uint64_t)s->dhcp_class[phase][26],(uint64_t)s->dhcp_class[phase][27],(uint64_t)s->dhcp_class[phase][28],(uint64_t)s->dhcp_type[phase][0],(uint64_t)s->dhcp_type[phase][1],(uint64_t)s->dhcp_type[phase][2],(uint64_t)s->dhcp_type[phase][3],(uint64_t)s->dhcp_reflected[phase]);
 console_printf("wifi-data5-arp: short=%u other=%u format=%u opcode=%u destination=%u sender=%u target=%u matched=%u request_us=%u\n",(uint64_t)s->arp_class[0],(uint64_t)s->arp_class[1],(uint64_t)s->arp_class[2],(uint64_t)s->arp_class[3],(uint64_t)s->arp_class[4],(uint64_t)s->arp_class[5],(uint64_t)s->arp_class[6],(uint64_t)s->arp_class[7],(uint64_t)s->arp_class[8]);
 console_printf("wifi-data5: complete=%u unsafe=%u phase=%u exhausted=%u discover=%u offer=%u request=%u ack=%u arp_dns=%u dns=%u arp_router=%u echo=%u sends=%u polls=%u ignored=%u received=%u tx_pn=%u echo_bytes=256\n",(uint64_t)s->complete,(uint64_t)wifi_wpa5.data_unsafe,(uint64_t)s->phase,(uint64_t)s->exhausted,(uint64_t)s->discover,(uint64_t)s->offer,(uint64_t)s->request,(uint64_t)s->ack,(uint64_t)s->arp_dns,(uint64_t)s->dns,(uint64_t)s->arp_router,(uint64_t)s->echo,(uint64_t)wifi_wpa5.data_sends,(uint64_t)s->polls,(uint64_t)s->ignored,(uint64_t)wifi_wpa5.data_received,wifi_wpa5.exchange.keys.tx_pn);
}

static bool wifi_wpa5_inspection(void *c,uint32_t alc,uint32_t ibi){
 if(c!=&wifi_wpa5 || alc!=wifi_power5.cycle.chosen[0] || ibi!=UINT32_MAX || !wifi_wpa5_owned(c,false))return false;
 const struct mt76_wpa5_io io={c,wifi_wpa5_owned,wifi_wpa5_nonce,wifi_wpa5_send,wifi_wpa5_poll,wifi_wpa5_delay,wifi_wpa5_now};
 const struct mt76_wpa5_data data={c,wifi_data5_window};
 uint64_t start=timer_monotonic_us();bool complete=wifi_wpa5.data_mode?mt76_wpa5_run_data(&wifi_wpa5.exchange,&wifi_wpa5.peer,wifi_wpa5.pmk,&io,&data):mt76_wpa5_run(&wifi_wpa5.exchange,&wifi_wpa5.peer,wifi_wpa5.pmk,&io);uint64_t end=timer_monotonic_us();
 zero_bytes(wifi_wpa5.pmk,sizeof wifi_wpa5.pmk);
 wifi_wpa5.forward_completed=wifi_wpa5.rx_copied;
 console_printf("wifi-wpa5-exchange: complete=%u clock_valid=%u elapsed_us=%u\n",(uint64_t)complete,(uint64_t)(end>=start),end>=start?end-start:0);
 return complete;
}
static bool wifi_wpa5_path(const char *p){const char *q="/cfg/wifi-wpa5.pmk";while(*p && *p==*q){p++;q++;}return !*p && !*q;}
static bool wifi_wpa5_credential(void){
 const struct initramfs_file *file=initramfs_find("/cfg/wifi-wpa5.pmk");unsigned matches=0;
 for(uint64_t i=0;i<initramfs_file_count();i++){const struct initramfs_file *f=initramfs_file_at(i);if(f && f->path && wifi_wpa5_path(f->path))matches++;}
 if(matches!=1 || !file || !file->data || file->size!=73 || file->mode!=0600)return false;
 const uint8_t *p=file->data;static const uint8_t magic[8]={'S','R','W','P','A','5',0,1};
 if(!wifi_handoff5_equal(p,magic,8) || !p[8] || p[8]>32 || p[8]!=wifi_wpa5.peer.peer.ssid_size || !wifi_handoff5_equal(p+9,wifi_wpa5.peer.peer.ssid,p[8]))return false;
 for(unsigned i=p[8];i<32;i++)if(p[9+i])return false;
 copy_bytes(wifi_wpa5.pmk,p+41,32);return true;
}
static bool wifi_wpa5_inspect(void){
 if(!wifi_wpa5_requested || wifi_wpa5.attempted)return false;
 wifi_wpa5.attempted=true;wifi_wpa5.data_mode=wifi_wpa5_data_requested;wifi_wpa5.d=wifi_cal5.d;
 if(!wifi_join5.restored || wifi_join5.phase!=9 || !wifi_join5.active || !wifi_eapol5.complete || !wifi_join5.exchange.complete || !wifi_join5_owned(&wifi_join5,true))return false;
 struct xhci_device *d=wifi_wpa5.d;wifi_wpa5.slot=d->slot_id;const unsigned dcis[]={0,11,16,8,9};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<8;j++)wifi_wpa5.context[i][j]=context_read32(d->output_context,dcis[i],j);
 wifi_wpa5.peer=wifi_join5.peer;
 if(!wifi_wpa5_credential()){console_write("wifi-wpa5-refused: private credential record unavailable or selected SSID mismatch\n");return false;}
 copy_bytes(wifi_wpa5.parent,(const uint8_t *)&wifi_join5,sizeof wifi_join5);
 copy_bytes(wifi_wpa5.parent_rings[0],(const uint8_t *)wifi_join5.rx,4096);copy_bytes(wifi_wpa5.parent_rings[1],(const uint8_t *)wifi_join5.tx,4096);
 for(unsigned i=0;i<WIFI_JOIN5_RX+3;i++)copy_bytes(wifi_wpa5.parent_pages[i],wifi_join5.buffer[i],4096);
 copy_bytes(wifi_wpa5.input,d->input_context,4096);copy_bytes(wifi_wpa5.expected,d->input_context,4096);
 const uint64_t fixed[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)wifi_wpa5.fixed[i]=fixed[i];
 wifi_wpa5.input_pointer=(uintptr_t)d->input_context;wifi_wpa5.output_pointer=(uintptr_t)d->output_context;
 for(unsigned i=0;i<WIFI_WPA5_RX+wifi_wpa5_tx_limit()+2;i++){
  void *p=0;uint64_t phys=0;if(!alloc_frame_zero(&phys,&p))return false;
  for(unsigned j=0;j<9;j++)if(phys==fixed[j])return false;
  if(phys==wifi_cal5.plan.ring || phys==wifi_handoff5.plan.new_ring || phys==wifi_tx5.ring_phys)return false;
  for(unsigned j=0;j<8;j++)if(phys==wifi_cal5.plan.physical[j])return false;
  for(unsigned j=0;j<WIFI_JOIN5_RX+5;j++)if(phys==wifi_join5.pins[j])return false;
  for(unsigned j=0;j<i;j++)if(phys==wifi_wpa5.pins[j])return false;
  wifi_wpa5.pins[i]=phys;wifi_wpa5.pointers[i]=(uintptr_t)p;
  if(i<2){if(i){wifi_wpa5.tx_phys=phys;wifi_wpa5.tx=p;}else{wifi_wpa5.rx_phys=phys;wifi_wpa5.rx=p;}write_link_trb(p,phys,true);}
  else{wifi_wpa5.pages[i-2]=phys;wifi_wpa5.buffer[i-2]=p;}
 }
 wifi_wpa5.rx_cycle=wifi_wpa5.tx_cycle=true;
 copy_bytes(wifi_wpa5.cal,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);copy_bytes(wifi_wpa5.power,(const uint8_t *)&wifi_power5,sizeof wifi_power5);
 copy_bytes(wifi_wpa5.handoff,(const uint8_t *)&wifi_handoff5,sizeof wifi_handoff5);copy_bytes(wifi_wpa5.binding,(const uint8_t *)&wifi_peer5,sizeof wifi_peer5);copy_bytes(wifi_wpa5.preview,(const uint8_t *)&wifi_tx5,sizeof wifi_tx5);
 copy_bytes(wifi_wpa5.old_ring,(const uint8_t *)wifi_cal5.ep.ring,4096);copy_bytes(wifi_wpa5.handoff_ring,(const uint8_t *)wifi_handoff5.ring,4096);copy_bytes(wifi_wpa5.preview_ring,(const uint8_t *)wifi_tx5.ring,4096);
 for(unsigned i=0;i<8;i++)copy_bytes(wifi_wpa5.old_pages[i],wifi_cal5.plan.buffer[i],4096);
 if(wifi_reuse5_requested && !wifi_reuse5_init())return false;
 wifi_wpa5.active=true;bool good=wifi_wpa5_owned(&wifi_wpa5,false);
 if(wifi_wpa5.data_mode)console_write("wifi-wpa5-progress: frozen parent;independent fresh RX/OUT;strict fresh nonce and bounded four-way and authenticated IP window;software key erase/leave;MAC disable and full restore\n");
 else console_write("wifi-wpa5-progress: frozen parent;independent fresh RX/OUT;strict fresh nonce and bounded four-way;software key erase/leave;MAC disable and full restore;no IP data\n");
 console_printf("wifi-wpa5-plan: rx=%x tx=%x old=%x slot=%u max_rx=48\n",wifi_wpa5.rx_phys,wifi_wpa5.tx_phys,(uint64_t)wifi_wpa5.context[4][2]|((uint64_t)wifi_wpa5.context[4][3]<<32),(uint64_t)d->slot_id);wifi_wpa5_receipt(0);
 if(good)good=wifi_wpa5_toggle();
 if(good){
  zero_bytes(d->input_context,4096);context_write32(d->input_context,0,1,1u|(1u<<8));for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_wpa5.context[0][j]);
  context_write32(d->input_context,9,1,(3u<<1)|(2u<<3)|(512u<<16));context_write32(d->input_context,9,2,wifi_wpa5.tx_phys|1);context_write32(d->input_context,9,3,wifi_wpa5.tx_phys>>32);context_write32(d->input_context,9,4,wifi_wpa5_packet_limit());copy_bytes(wifi_wpa5.expected,d->input_context,4096);
  good=wifi_wpa5_command(0,12,8,0);
 }
 if(good)good=wifi_wpa5_command(1,16,9,wifi_wpa5.rx_phys|1);
 if(good)good=wifi_wpa5_post();
 bool exchanged=false;
 if(good){const struct mt76_mac_cycle_io io={&wifi_wpa5,wifi_wpa5_owned,wifi_wpa5_read,wifi_wpa5_write,wifi_wpa5_mac_delay,wifi_wpa5_now,wifi_wpa5_inspection};exchanged=mt76_mac_cycle_run(&wifi_wpa5.mac,&io);good=!wifi_wpa5.failed && wifi_wpa5.mac.mac_disabled && wifi_wpa5.mac.tx_drained && wifi_wpa5.mac.rx_drained && wifi_wpa5.mac.filter_restored && wifi_wpa5.mac.txop_restored && wifi_wpa5_owned(&wifi_wpa5,true);}
 wifi_data5_report();
 if(wifi_wpa5.mac.attempted){
  const struct mt76_mac_cycle *m=&wifi_wpa5.mac;
  if(wifi_wpa5.data_mode)console_printf("wifi-retire5: limit=40 capacity=48 forward=%u drained=%u posts=%u\n",(uint64_t)wifi_wpa5.forward_completed,(uint64_t)wifi_wpa5.drain_completed,(uint64_t)wifi_wpa5.posts);
  console_printf("wifi-wpa5-mac: complete=%u started=%u snapshot=%u disabled=%u tx_drained=%u rx_drained=%u filter_restored=%u txop_restored=%u operations=%u reads=%u writes=%u waits=%u filter=%x txop=%x alc=%x ibi=%x hold_us=%u\n",(uint64_t)m->complete,(uint64_t)m->started,(uint64_t)m->snapshot_complete,(uint64_t)m->mac_disabled,(uint64_t)m->tx_drained,(uint64_t)m->rx_drained,(uint64_t)m->filter_restored,(uint64_t)m->txop_restored,(uint64_t)m->operations,(uint64_t)m->reads,(uint64_t)m->writes,(uint64_t)m->waits,(uint64_t)m->filter,(uint64_t)m->txop,(uint64_t)m->tx_alc,(uint64_t)m->ibi9,m->hold_us);
 }
 if(good){wifi_wpa5.phase=4;good=wifi_wpa5_command(4,15,9,0);}
 if(good)good=wifi_wpa5_command(5,15,8,0);
 uint64_t old=wifi_wpa5.context[4][2]|((uint64_t)wifi_wpa5.context[4][3]<<32);
 if(good)good=wifi_wpa5_command(6,16,9,old);
 if(good){zero_bytes(d->input_context,4096);context_write32(d->input_context,0,0,1u<<8);context_write32(d->input_context,0,1,1);for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_wpa5.context[0][j]);copy_bytes(wifi_wpa5.expected,d->input_context,4096);good=wifi_wpa5_command(7,12,8,0);}
 if(good){copy_bytes(d->input_context,wifi_wpa5.input,4096);copy_bytes(wifi_wpa5.expected,d->input_context,4096);wifi_wpa5.phase=9;wifi_wpa5.restored=wifi_wpa5_owned(&wifi_wpa5,true) && wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) && wifi_tx5_owned() && wifi_join5_owned(&wifi_join5,true);good=wifi_wpa5.restored;}
 zero_bytes(wifi_wpa5.pmk,sizeof wifi_wpa5.pmk);
 struct mt76_wpa5 *s=&wifi_wpa5.exchange;
 console_printf("wifi-wpa5: ok=%u exchange=%u auth=%u assoc=%u m1=%u m2=%u m3_mic=%u installed=%u m4=%u activated=%u erased=%u leave=%u unsafe=%u sends=%u polls=%u ignored=%u aid=%u status=%u commands=%u posts=%u controls=%u restored=%u phase=%u toggle=%u retained=1 hardware_keys=0 ip_data=%u\n",(uint64_t)(good && exchanged),(uint64_t)s->complete,(uint64_t)s->auth_matched,(uint64_t)s->assoc_matched,(uint64_t)s->m1_matched,(uint64_t)s->m2_sent,(uint64_t)s->m3_mic,(uint64_t)s->keys_installed,(uint64_t)s->m4_sent,(uint64_t)s->keys_activated,(uint64_t)s->keys_erased,(uint64_t)s->leave_sent,(uint64_t)s->unsafe,(uint64_t)wifi_wpa5.sends,(uint64_t)s->polls,(uint64_t)s->ignored,(uint64_t)s->aid,(uint64_t)s->status,(uint64_t)wifi_wpa5.commands,(uint64_t)wifi_wpa5.posts,(uint64_t)wifi_wpa5.usb_ops,(uint64_t)wifi_wpa5.restored,(uint64_t)wifi_wpa5.phase,(uint64_t)wifi_wpa5.toggle_synced,(uint64_t)(wifi_wpa5.data_mode && wifi_wpa5.ip.complete));
 if(!wifi_wpa5.restored && !wifi_wpa5.failed)wifi_wpa5_fail();
 wifi_wpa5.active=false;return good && exchanged;
}

bool xhci_wifi_capture_wpa5(void){
 if(wifi_wpa5_requested || wifi_wpa5.attempted || wifi_join5_after)return false;
 wifi_wpa5_requested=true;wifi_join5_after=wifi_wpa5_inspect;
 bool good=xhci_wifi_capture_eapol5();
 zero_bytes(wifi_wpa5.pmk,sizeof wifi_wpa5.pmk);wifi_join5_after=0;wifi_wpa5_requested=false;return good;
}

bool xhci_wifi_capture_data5(void){
 if(wifi_wpa5_requested || wifi_wpa5_data_requested || wifi_wpa5.attempted || wifi_join5_after)return false;
 wifi_wpa5_data_requested=true;
 bool good=xhci_wifi_capture_wpa5();
 wifi_wpa5_data_requested=false;return good;
}
#include "xhci_wifi_provider5.inc"
