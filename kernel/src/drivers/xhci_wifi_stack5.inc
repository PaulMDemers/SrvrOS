/* Bounded original-key provider. IRQ-off callbacks never delay or wait. */
#include <srvros/mt76_stack5_probe.h>
#define WIFI_STACK5_TOTAL_US 15000000u
#define WIFI_STACK5_POLLS 150000u
struct wifi_stack5_state {
 void *context;bool (*exercise)(void *,struct mt76_net5 *);
 uint64_t base_enqueued,base_submitted,base_completed,base_received;
 uint64_t start,last,boundary_start,tx_token,tx_ticket,tx_completion,services,icmp_count,icmp_pn;
 unsigned icmp_sequence;
 uint64_t terminal_epoch,terminal_ticket,terminal_token,terminal_posts,terminal_completions;
 bool selected,running,exercise_complete,boundary,failed,detached,retired,terminal_handoff,terminal_pending,terminal_ready,terminal_provider_closing;
};
static struct wifi_stack5_state wifi_stack5_slots[2];
static struct wifi_stack5_state *wifi_stack5_current=&wifi_stack5_slots[0];
#define wifi_stack5 (*wifi_stack5_current)
/* Adapter registry identity, lifetime counters and tokens survive reconnect. */
static struct mt76_net5 wifi_stack5_net;
static __attribute__((noreturn)) void wifi_http5_hold(unsigned);
static enum mt76_join5_send wifi_stack5_terminal_window(bool,void *,uint64_t);
static bool wifi_stack5_fault(void){wifi_stack5.failed=true;return wifi_stream5_fail();}
static bool wifi_stack5_owned(void *c,uint64_t epoch){return c==&wifi_stream5 && wifi_stack5.selected && wifi_stack5.running && epoch==wifi_stream5.epoch && wifi_stream5.stack_mode && !wifi_stack5.failed && wifi_stream5_data_owned(c);}
static bool wifi_stack5_clock(void){uint64_t now=timer_monotonic_us();if(now<wifi_stack5.last || now<wifi_stack5.start || now-wifi_stack5.start>=WIFI_STACK5_TOTAL_US || wifi_stack5.services>=WIFI_STACK5_POLLS)return wifi_stack5_fault();wifi_stack5.last=now;return true;}
static bool wifi_stack5_ready(void *c,uint64_t epoch){
 /* The last posted RX is still usable. Freezing TX merely because posting
  * advanced its cursor to63 can prevent the request which would complete it. */
 bool rx_available=wifi_stream5.dma.cursor[1]<63 ||
  (wifi_stream5.dma.cursor[1]==63 && wifi_stream5.pending[1] && !wifi_stream5.ready_rx);
 return !wifi_stack5.terminal_handoff && wifi_stack5_owned(c,epoch) && !wifi_stack5.boundary && !wifi_fence5_native.round && !wifi_stream5.pending[0] && !wifi_stack5.tx_token && wifi_stream5.dma.cursor[0]<63 && rx_available && wifi_stream5.posts[0]<WIFI_STREAM5_TX-1 && wifi_stream5.posts[1]<WIFI_STREAM5_RX-8;
}
static enum mt76_net5_submit wifi_stack5_submit(void *c,uint64_t epoch,uint64_t token,const uint8_t *frame,uint16_t length){
 struct mt76_net5 *p=&wifi_stack5_net;struct wpa2_ccmp *k=&wifi_stream5.exchange.keys;
 if(!wifi_stack5_ready(c,epoch) || !token || !p->pending || p->pending_token!=token || frame!=p->loan || length!=p->loan_size || length<14 || length>MT76_DATA5_ETH_MAX || wifi_stream5.posts[0]!=4+wifi_stream5.data_sends)return MT76_NET5_REFUSED;
 if(!wifi_stack5_clock())return MT76_NET5_UNCERTAIN;
 bool echo=length==74 && frame[12]==8 && !frame[13] && frame[14]==0x45 && !frame[16] && frame[17]==60 && !(frame[20]&0x3f) && !frame[21] && frame[23]==1 && frame[34]==8 && !frame[35];
 unsigned sequence=echo?((unsigned)frame[40]<<8|frame[41]):0;
 uint64_t pn=k->tx_pn;size_t bytes=mt76_data5_tx(&wifi_stream5.scratch,k,epoch,frame,length,9+wifi_stream5.data_sends,wifi_stream5.usb,sizeof wifi_stream5.usb);
 if(!bytes || k->tx_pn!=pn+1){zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);return k->tx_pn==pn?MT76_NET5_REFUSED:MT76_NET5_UNCERTAIN;}
 wifi_stream5.data_pn=k->tx_pn;wifi_stream5.tx_kind=5;
 /* Both cursor guards precede PN reservation;publish cannot enter its legacy
  * blocking fence path. Original key seals once;publication uncertainty pins. */
 bool good=wifi_stream5_publish(0,wifi_stream5.usb,(unsigned)bytes);zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);
 if(!good){wifi_stack5_fault();return MT76_NET5_UNCERTAIN;}
 wifi_stack5.tx_token=token;wifi_stack5.tx_ticket=wifi_stream5.lease[0].ticket;wifi_stack5.tx_completion=wifi_stream5.completions[0];wifi_stream5.data_sends++;if(wifi_stream5.wraps[0])wifi_stream5.data_after_txwrap++;
 if(echo){wifi_stack5.icmp_count++;wifi_stack5.icmp_pn=k->tx_pn;wifi_stack5.icmp_sequence=sequence;}return MT76_NET5_QUEUED;
}
static bool wifi_stack5_service(void *c,uint64_t epoch){
 if(wifi_stack5.terminal_handoff || !wifi_stack5_owned(c,epoch) || !wifi_stack5_clock())return false;
 wifi_stack5.services++;
 /* One bounded dispatch,at most one RX acquisition,one fence step. Both DMA
  * directions stay frozen throughout an active native fence. */
 if(!wifi_stream5_dispatch())return wifi_stack5_fault();
 if(!wifi_fence5_native.round && !wifi_stream5_collect(MT76_WPA5_DATA,false))return wifi_stack5_fault();
 if(wifi_stack5.tx_token && !wifi_stream5.pending[0]){
  if(wifi_stream5.lease[0].ticket!=wifi_stack5.tx_ticket || wifi_stream5.completions[0]!=wifi_stack5.tx_completion+1)return wifi_stack5_fault();
  uint64_t token=wifi_stack5.tx_token;wifi_stack5.tx_token=wifi_stack5.tx_ticket=wifi_stack5.tx_completion=0;
  if(!mt76_net5_tx_complete(&wifi_stack5_net,epoch,token,true))return wifi_stack5_fault();
 }
 /* Posting RX63 does not complete RX63. Keep ordinary TX working until that
  * original receive completes; a terminal HTTP owner may instead hand it to
  * the outer exact Stop cleanup without wrapping an otherwise idle ring. */
 bool boundary=wifi_stream5.dma.cursor[0]==63 ||
  (wifi_stream5.dma.cursor[1]==63 && !wifi_stream5.pending[1]);
 if(boundary && !wifi_stack5.boundary){wifi_stack5.boundary=true;wifi_stack5.boundary_start=wifi_stack5.last;}
 if(wifi_stack5.boundary){
  if(wifi_stack5.last-wifi_stack5.boundary_start>=MT76_DMA5_TIMEOUT_US)return wifi_stack5_fault();
  if(wifi_fence5_native.round){
   unsigned dir=wifi_fence5_native.direction;if(!wifi_fence5_step())return wifi_stack5_fault();
   if(!wifi_fence5_native.round){if(wifi_stream5.enqueue[dir] || wifi_stream5.cycle[dir]!=wifi_stream5.dma.cycle[dir])return wifi_stack5_fault();wifi_stream5.wraps[dir]++;}
   return wifi_stack5_owned(c,epoch);
  }
  /* A quiet pending RX must really complete;timeout pins,no fake cancellation.
   * Authenticated cache is independent of released DMA and may cross a fence. */
  if(wifi_stream5.pending[0] || wifi_stream5.pending[1] || wifi_stream5.ready_rx || wifi_stream5.loan.ticket || wifi_stack5.tx_token)return true;
  for(unsigned dir=0;dir<2;dir++)if(wifi_stream5.dma.cursor[dir]==63){
   if(wifi_stream5.dma.wraps>=wifi_stream5_fence_limit() || wifi_fence5_try_begin(dir)==MT76_FENCE5_FAILED)return wifi_stack5_fault();
   return wifi_stack5_owned(c,epoch);
  }
  wifi_stack5.boundary=false;wifi_stack5.boundary_start=0;
 }
 if(!wifi_stream5.pending[1] && !wifi_stream5.ready_rx && !wifi_stream5.cached_present){if(wifi_stream5.posts[1]>=WIFI_STREAM5_RX-8)return wifi_stack5_fault();if(!wifi_stream5_publish(1,NULL,4096))return wifi_stack5_fault();}
 return wifi_stack5_owned(c,epoch);
}
static bool wifi_stack5_receive(void *c,uint64_t epoch,uint8_t *out,size_t cap,size_t *n,bool *present){
 if(!n || !present)return false;
 *n=0;*present=false;
 if(wifi_stack5.terminal_handoff || !wifi_stack5_owned(c,epoch) || out!=wifi_stack5_net.rx || cap!=sizeof wifi_stack5_net.rx || !wifi_stack5_clock())return false;
 if(wifi_stream5.cached_present){
  if(wifi_stream5.cached_type!=MT76_WPA5_DATA || wifi_stream5.cached_size<14 || wifi_stream5.cached_size>cap || wifi_stream5.loan.ticket || wifi_stream5.ready_rx)return wifi_stack5_fault();
  /* Sole replay commit happened in collect,then DMA was released. */
  copy_bytes(out,wifi_stream5.cached,wifi_stream5.cached_size);*n=wifi_stream5.cached_size;*present=true;zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_size=0;wifi_stream5.cached_present=false;
 }
 return true;
}
static bool wifi_stack5_retirement(void *c,uint64_t epoch){
 return c==&wifi_stream5 && epoch==wifi_stream5.epoch && !wifi_stack5.running && !wifi_stack5.tx_token && !wifi_stream5.active && !wifi_stream5.pending[0] && !wifi_stream5.pending[1] && !wifi_stream5.loan.ticket && wifi_stream5.exchange.keys_erased && wpa2_ccmp_erased(&wifi_stream5.exchange.keys,epoch) && !wifi_stream5.exchange.handshake.keys_may_live && wifi_stream5.restored && (wifi_stream5.parent_complete || (wifi_stream5.connection_mode && wifi_stream5.connection_closed)) && wifi_stream5.events_empty && wifi_stream5.retired && wifi_stream5.dma.state==MT76_DMA5_RETIRED;
}
static enum mt76_join5_send wifi_stack5_window(void *c,struct wpa2_ccmp *keys,uint64_t epoch,const struct mt76_peer5 *peer){
 if(c!=&wifi_stream5 || !wifi_stack5.selected || wifi_stack5.running || !wifi_stack5.exercise || keys!=&wifi_stream5.exchange.keys || epoch!=wifi_stream5.epoch || peer!=&wifi_stream5.peer || wifi_stream5.posts[0]!=4 || wifi_stream5.data_active)return MT76_JOIN5_REFUSED;
 wifi_stream5.data_active=true;wifi_stack5.running=true;wifi_stack5.start=wifi_stack5.last=timer_monotonic_us();
 const struct mt76_net5_io io={c,wifi_stack5_owned,wifi_stack5_submit,wifi_stack5_service,wifi_stack5_receive,wifi_stack5_retirement};
 bool registered=wifi_stream5_data_owned(c) && (wifi_stream5.connection_mode && wifi_stack5_net.identity?
  wifi_stack5_net.state==MT76_NET5_DOWN && !wifi_stack5_net.attached && !wifi_stack5_net.io.context &&
   !wifi_stack5_net.faults && !wifi_stack5_net.dropped && wifi_handoff5_equal(wifi_stack5_net.ops.mac,peer->peer.station,6):
  mt76_net5_init(&wifi_stack5_net,peer->peer.station));
 wifi_stack5.base_enqueued=wifi_stack5_net.enqueued;wifi_stack5.base_submitted=wifi_stack5_net.submitted;
 wifi_stack5.base_completed=wifi_stack5_net.completed;wifi_stack5.base_received=wifi_stack5_net.received;
 bool good=wifi_stream5_data_owned(c) && registered && mt76_net5_attach(&wifi_stack5_net,epoch,&io) && mt76_net5_set_ready(&wifi_stack5_net,epoch,wifi_stack5_ready);
 if(good)good=wifi_stack5.exercise(wifi_stack5.context,&wifi_stack5_net);
 if(wifi_stack5.terminal_handoff)return wifi_stack5_terminal_window(good,c,epoch);
 good=good && wifi_stack5_clock() && wifi_stream5_data_owned(c) && wifi_stack5_net.state==MT76_NET5_UP && !wifi_stack5_net.count && !wifi_stack5_net.pending && !wifi_stack5.tx_token && !wifi_fence5_native.round && !wifi_stream5.pending[0] && !wifi_stream5.ready_rx && !wifi_stream5.loan.ticket;
 wifi_stack5.exercise_complete=good;
 if(wifi_stack5_net.attached)(void)mt76_net5_disconnect(&wifi_stack5_net,epoch);
 bool unsafe=wifi_stack5.failed || wifi_stream5.failed || !wifi_stream5_data_owned(c) || wifi_stack5_net.pending || wifi_stack5.tx_token || wifi_stack5_net.faults;
 if(unsafe && !wifi_stream5.failed)wifi_stack5_fault();
 wifi_stack5.running=false;wifi_stream5.closed=true;wifi_stream5.data_active=false;zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_size=0;wifi_stream5.cached_present=false;
 return unsafe?MT76_JOIN5_UNCERTAIN:good?MT76_JOIN5_SENT:MT76_JOIN5_REFUSED;
}
static bool wifi_stack5_complete(void){return wifi_stack5.selected && wifi_stack5.exercise_complete && !wifi_stack5.failed && !wifi_stack5.running && wifi_stack5_net.state==MT76_NET5_CLOSING && !wifi_stack5_net.count && !wifi_stack5_net.pending && !wifi_stack5.tx_token && !wifi_stack5_net.dropped && wifi_stack5_net.enqueued==wifi_stack5_net.submitted && wifi_stack5_net.submitted==wifi_stack5_net.completed && wifi_stack5_net.completed>=wifi_stack5.base_completed && wifi_stack5_net.completed-wifi_stack5.base_completed==wifi_stream5.data_sends && wifi_stack5_net.received>wifi_stack5.base_received && !wifi_stack5_net.faults;}
static void wifi_stack5_emit(void){
 unsigned selected[8],rx_receipts=wifi_stream5_rx_select(selected);
 console_printf("wifi-stack5: complete=%u parent=%u restored=%u retired=%u empty=%u tx=%u rx=%u tx_wraps=%u rx_wraps=%u data_tx=%u data_rx=%u tx_after=%u rx_after=%u commands=%u fences=%u rx_receipts=%u dropped=%u borrowed=%u released=%u faults=%u services=%u elapsed_us=%u enqueued=%u submitted=%u completed=%u received=%u provider_faults=%u pending=%u retained=1 backend_retired=0\n",(uint64_t)(wifi_stream5.complete && wifi_stream5.parent_complete && wifi_stream5.retired),(uint64_t)wifi_stream5.parent_complete,(uint64_t)wifi_stream5.restored,(uint64_t)wifi_stream5.retired,(uint64_t)wifi_stream5.events_empty,wifi_stream5.posts[0],wifi_stream5.posts[1],(uint64_t)wifi_stream5.wraps[0],(uint64_t)wifi_stream5.wraps[1],(uint64_t)wifi_stream5.data_sends,(uint64_t)wifi_stream5.data_received,(uint64_t)wifi_stream5.data_after_txwrap,(uint64_t)wifi_stream5.data_after_rxwrap,(uint64_t)wifi_stream5.commands,wifi_stream5.dma.wraps,(uint64_t)rx_receipts,wifi_stack5_net.dropped,wifi_stream5.dma.borrowed,wifi_stream5.dma.released,wifi_stream5.dma.faults,wifi_stack5.services,wifi_stack5.last-wifi_stack5.start,wifi_stack5_net.enqueued,wifi_stack5_net.submitted,wifi_stack5_net.completed,wifi_stack5_net.received,wifi_stack5_net.faults,(uint64_t)wifi_stack5_net.pending);
}
/* Caller supplies a bounded production exercise,initializing net.c only after
 * provider attachment. It must return with queue/TX callbacks idle. */
bool xhci_wifi_capture_stack5(bool (*exercise)(void *,struct mt76_net5 *),void *context){
 if(!exercise || wifi_stack5.selected || wifi_stream5.attempted || wifi_stream5.stack_mode)return false;
 wifi_stack5.selected=true;wifi_stack5.context=context;wifi_stack5.exercise=exercise;wifi_stream5.stack_mode=true;
 bool good=wifi_stream5_capture(false);
 if(wifi_stack5_net.attached && wifi_stack5_retirement(&wifi_stream5,wifi_stream5.epoch))wifi_stack5.detached=mt76_net5_finish_detach(&wifi_stack5_net,wifi_stream5.epoch);
 if(wifi_stack5_net.identity)wifi_stack5.retired=mt76_net5_retire(&wifi_stack5_net);
 wifi_stack5.exercise=NULL;wifi_stack5.context=NULL;
 console_printf("wifi-stack5-final: complete=%u detached=%u provider_retired=%u retained=1 backend_retired=0\n",(uint64_t)(good && wifi_stack5.detached && wifi_stack5.retired),(uint64_t)wifi_stack5.detached,(uint64_t)wifi_stack5.retired);
 return good && wifi_stack5.detached && wifi_stack5.retired;
}
static struct mt76_stack5_probe wifi_stack5_probe_result;
static bool wifi_stack5_probe_current(void *c,uint64_t epoch){
 struct mt76_net5 *p=&wifi_stack5_net;const struct mt76_net5_io *io=&p->io,*guard=&p->io_guard;
 /* Pre-init admission precedes registry selection. Once net_init selects a
  * provider, matching MAC/link alone cannot authorize a different adapter. */
 return (!net_device_mac() || net_device_selected_is(&p->ops)) &&
 wifi_stack5_owned(c,epoch) && p->state==MT76_NET5_UP && p->attached && p->epoch==epoch && !p->retire_requested &&
 io->context==c && guard->context==c && io->owned==wifi_stack5_owned && guard->owned==wifi_stack5_owned &&
 io->submit==wifi_stack5_submit && guard->submit==wifi_stack5_submit && io->service==wifi_stack5_service && guard->service==wifi_stack5_service &&
 io->receive==wifi_stack5_receive && guard->receive==wifi_stack5_receive && io->retired==wifi_stack5_retirement && guard->retired==wifi_stack5_retirement && p->ready==wifi_stack5_ready && p->ready_guard==wifi_stack5_ready;
}
static bool wifi_stack5_probe_retry(void *c,uint64_t epoch){
 struct mt76_net5 *p=&wifi_stack5_net;
 return wifi_stack5_probe_current(c,epoch) && !p->count && !p->pending && p->submitted==p->completed && !wifi_stack5.tx_token && !wifi_stream5.pending[0];
}
#include "xhci_wifi_http5_rx_handoff.inc"
static bool wifi_stack5_probe_observe(void *c,uint64_t epoch,struct mt76_stack5_observation *o){
 if(!o || !wifi_stack5_probe_current(c,epoch))return false;
 struct mt76_net5 *p=&wifi_stack5_net;
 *o=(struct mt76_stack5_observation){.data_tx=wifi_stream5.data_sends,.tx_posts=wifi_stream5.posts[0],.rx_posts=wifi_stream5.posts[1],.tx_after=wifi_stream5.data_after_txwrap,.rx_after=wifi_stream5.data_after_rxwrap,.services=wifi_stack5.services,.icmp_count=wifi_stack5.icmp_count,.icmp_pn=wifi_stack5.icmp_pn,.tx_cursor=wifi_stream5.dma.cursor[0],.rx_cursor=wifi_stream5.dma.cursor[1],.tx_wraps=wifi_stream5.wraps[0],.rx_wraps=wifi_stream5.wraps[1],.fences=(unsigned)wifi_stream5.dma.wraps,.icmp_sequence=wifi_stack5.icmp_sequence,
 .drained=!p->count && !p->pending && !wifi_stack5.tx_token && !wifi_stream5.pending[0] && !wifi_stream5.ready_rx && !wifi_stream5.loan.ticket && !wifi_stream5.cached_present && !wifi_stack5.boundary && !wifi_fence5_native.round && !xhci.command_pending};
 return wifi_stack5_probe_current(c,epoch);
}
static bool wifi_stack5_probe_exercise(void *context,struct mt76_net5 *p){
 if(context!=&wifi_stack5_probe_result || p!=&wifi_stack5_net || wifi_stack5.start>UINT64_MAX-WIFI_STACK5_TOTAL_US)return false;
 const struct mt76_stack5_probe_io io={&wifi_stream5,wifi_stack5_probe_current,wifi_stack5_probe_retry,wifi_stack5_probe_observe,wifi_stream5.epoch,wifi_stack5.start+WIFI_STACK5_TOTAL_US};
 return mt76_stack5_probe_run(context,p,&io);
}
bool xhci_wifi_capture_stack5_probe(void){
 bool good=xhci_wifi_capture_stack5(wifi_stack5_probe_exercise,&wifi_stack5_probe_result);
 /* All result emission follows key/endpoint/MAC/parent closure;never live. */
 mt76_stack5_probe_emit(&wifi_stack5_probe_result);return good;
}

#include "xhci_wifi_http5.inc"

#include "xhci_wifi_reconnect5.inc"
