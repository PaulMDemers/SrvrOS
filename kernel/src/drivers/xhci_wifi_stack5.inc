/* Bounded original-key provider. IRQ-off callbacks never delay or wait. */
#define WIFI_STACK5_TOTAL_US 15000000u
#define WIFI_STACK5_POLLS 150000u
static struct {
 struct mt76_net5 net;void *context;bool (*exercise)(void *,struct mt76_net5 *);
 uint64_t start,last,boundary_start,tx_token,tx_ticket,tx_completion,services;
 bool selected,running,exercise_complete,boundary,failed,detached,retired;
} wifi_stack5;
static bool wifi_stack5_fault(void){wifi_stack5.failed=true;return wifi_stream5_fail();}
static bool wifi_stack5_owned(void *c,uint64_t epoch){return c==&wifi_stream5 && wifi_stack5.selected && wifi_stack5.running && epoch==wifi_stream5.epoch && wifi_stream5.stack_mode && !wifi_stack5.failed && wifi_stream5_data_owned(c);}
static bool wifi_stack5_clock(void){uint64_t now=timer_monotonic_us();if(now<wifi_stack5.last || now<wifi_stack5.start || now-wifi_stack5.start>=WIFI_STACK5_TOTAL_US || wifi_stack5.services>=WIFI_STACK5_POLLS)return wifi_stack5_fault();wifi_stack5.last=now;return true;}
static bool wifi_stack5_ready(void *c,uint64_t epoch){return wifi_stack5_owned(c,epoch) && !wifi_stack5.boundary && !wifi_fence5_native.round && !wifi_stream5.pending[0] && !wifi_stack5.tx_token && wifi_stream5.dma.cursor[0]<63 && wifi_stream5.dma.cursor[1]<63 && wifi_stream5.posts[0]<WIFI_STREAM5_TX-1 && wifi_stream5.posts[1]<WIFI_STREAM5_RX-8;}
static enum mt76_net5_submit wifi_stack5_submit(void *c,uint64_t epoch,uint64_t token,const uint8_t *frame,uint16_t length){
 struct mt76_net5 *p=&wifi_stack5.net;struct wpa2_ccmp *k=&wifi_stream5.exchange.keys;
 if(!wifi_stack5_ready(c,epoch) || !token || !p->pending || p->pending_token!=token || frame!=p->loan || length!=p->loan_size || length<14 || length>MT76_DATA5_ETH_MAX || wifi_stream5.posts[0]!=4+wifi_stream5.data_sends)return MT76_NET5_REFUSED;
 if(!wifi_stack5_clock())return MT76_NET5_UNCERTAIN;
 uint64_t pn=k->tx_pn;size_t bytes=mt76_data5_tx(&wifi_stream5.scratch,k,epoch,frame,length,9+wifi_stream5.data_sends,wifi_stream5.usb,sizeof wifi_stream5.usb);
 if(!bytes || k->tx_pn!=pn+1){zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);return k->tx_pn==pn?MT76_NET5_REFUSED:MT76_NET5_UNCERTAIN;}
 wifi_stream5.data_pn=k->tx_pn;wifi_stream5.tx_kind=5;
 /* Both cursor guards precede PN reservation;publish cannot enter its legacy
  * blocking fence path. Original key seals once;publication uncertainty pins. */
 bool good=wifi_stream5_publish(0,wifi_stream5.usb,(unsigned)bytes);zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);
 if(!good){wifi_stack5_fault();return MT76_NET5_UNCERTAIN;}
 wifi_stack5.tx_token=token;wifi_stack5.tx_ticket=wifi_stream5.lease[0].ticket;wifi_stack5.tx_completion=wifi_stream5.completions[0];wifi_stream5.data_sends++;if(wifi_stream5.wraps[0])wifi_stream5.data_after_txwrap++;return MT76_NET5_QUEUED;
}
static bool wifi_stack5_service(void *c,uint64_t epoch){
 if(!wifi_stack5_owned(c,epoch) || !wifi_stack5_clock())return false;
 wifi_stack5.services++;
 /* One bounded dispatch,at most one RX acquisition,one fence step. Both DMA
  * directions stay frozen throughout an active native fence. */
 if(!wifi_stream5_dispatch())return wifi_stack5_fault();
 if(!wifi_fence5_native.round && !wifi_stream5_collect(MT76_WPA5_DATA,false))return wifi_stack5_fault();
 if(wifi_stack5.tx_token && !wifi_stream5.pending[0]){
  if(wifi_stream5.lease[0].ticket!=wifi_stack5.tx_ticket || wifi_stream5.completions[0]!=wifi_stack5.tx_completion+1)return wifi_stack5_fault();
  uint64_t token=wifi_stack5.tx_token;wifi_stack5.tx_token=wifi_stack5.tx_ticket=wifi_stack5.tx_completion=0;
  if(!mt76_net5_tx_complete(&wifi_stack5.net,epoch,token,true))return wifi_stack5_fault();
 }
 bool boundary=wifi_stream5.dma.cursor[0]==63 || wifi_stream5.dma.cursor[1]==63;
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
  for(unsigned dir=0;dir<2;dir++)if(wifi_stream5.dma.cursor[dir]==63){if(wifi_stream5.dma.wraps>=WIFI_STREAM5_FENCES || !wifi_fence5_begin(dir))return wifi_stack5_fault();return wifi_stack5_owned(c,epoch);}
  wifi_stack5.boundary=false;wifi_stack5.boundary_start=0;
 }
 if(!wifi_stream5.pending[1] && !wifi_stream5.ready_rx && !wifi_stream5.cached_present){if(wifi_stream5.posts[1]>=WIFI_STREAM5_RX-8)return wifi_stack5_fault();if(!wifi_stream5_publish(1,NULL,4096))return wifi_stack5_fault();}
 return wifi_stack5_owned(c,epoch);
}
static bool wifi_stack5_receive(void *c,uint64_t epoch,uint8_t *out,size_t cap,size_t *n,bool *present){
 if(!n || !present)return false;
 *n=0;*present=false;
 if(!wifi_stack5_owned(c,epoch) || out!=wifi_stack5.net.rx || cap!=sizeof wifi_stack5.net.rx || !wifi_stack5_clock())return false;
 if(wifi_stream5.cached_present){
  if(wifi_stream5.cached_type!=MT76_WPA5_DATA || wifi_stream5.cached_size<14 || wifi_stream5.cached_size>cap || wifi_stream5.loan.ticket || wifi_stream5.ready_rx)return wifi_stack5_fault();
  /* Sole replay commit happened in collect,then DMA was released. */
  copy_bytes(out,wifi_stream5.cached,wifi_stream5.cached_size);*n=wifi_stream5.cached_size;*present=true;zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_size=0;wifi_stream5.cached_present=false;
 }
 return true;
}
static bool wifi_stack5_retirement(void *c,uint64_t epoch){
 return c==&wifi_stream5 && epoch==wifi_stream5.epoch && !wifi_stack5.running && !wifi_stack5.tx_token && !wifi_stream5.active && !wifi_stream5.pending[0] && !wifi_stream5.pending[1] && !wifi_stream5.loan.ticket && wifi_stream5.exchange.keys_erased && wpa2_ccmp_erased(&wifi_stream5.exchange.keys,epoch) && !wifi_stream5.exchange.handshake.keys_may_live && wifi_stream5.restored && wifi_stream5.parent_complete && wifi_stream5.events_empty && wifi_stream5.retired && wifi_stream5.dma.state==MT76_DMA5_RETIRED;
}
static enum mt76_join5_send wifi_stack5_window(void *c,struct wpa2_ccmp *keys,uint64_t epoch,const struct mt76_peer5 *peer){
 if(c!=&wifi_stream5 || !wifi_stack5.selected || wifi_stack5.running || !wifi_stack5.exercise || keys!=&wifi_stream5.exchange.keys || epoch!=wifi_stream5.epoch || peer!=&wifi_stream5.peer || wifi_stream5.posts[0]!=4 || wifi_stream5.data_active)return MT76_JOIN5_REFUSED;
 wifi_stream5.data_active=true;wifi_stack5.running=true;wifi_stack5.start=wifi_stack5.last=timer_monotonic_us();
 const struct mt76_net5_io io={c,wifi_stack5_owned,wifi_stack5_submit,wifi_stack5_service,wifi_stack5_receive,wifi_stack5_retirement};
 bool good=wifi_stream5_data_owned(c) && mt76_net5_init(&wifi_stack5.net,peer->peer.station) && mt76_net5_attach(&wifi_stack5.net,epoch,&io) && mt76_net5_set_ready(&wifi_stack5.net,epoch,wifi_stack5_ready);
 if(good)good=wifi_stack5.exercise(wifi_stack5.context,&wifi_stack5.net);
 good=good && wifi_stack5_clock() && wifi_stream5_data_owned(c) && wifi_stack5.net.state==MT76_NET5_UP && !wifi_stack5.net.count && !wifi_stack5.net.pending && !wifi_stack5.tx_token && !wifi_fence5_native.round && !wifi_stream5.pending[0] && !wifi_stream5.ready_rx && !wifi_stream5.loan.ticket;
 wifi_stack5.exercise_complete=good;
 if(wifi_stack5.net.attached)(void)mt76_net5_disconnect(&wifi_stack5.net,epoch);
 bool unsafe=wifi_stack5.failed || wifi_stream5.failed || !wifi_stream5_data_owned(c) || wifi_stack5.net.pending || wifi_stack5.tx_token || wifi_stack5.net.faults;
 if(unsafe && !wifi_stream5.failed)wifi_stack5_fault();
 wifi_stack5.running=false;wifi_stream5.closed=true;wifi_stream5.data_active=false;zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_size=0;wifi_stream5.cached_present=false;
 return unsafe?MT76_JOIN5_UNCERTAIN:good?MT76_JOIN5_SENT:MT76_JOIN5_REFUSED;
}
static bool wifi_stack5_complete(void){return wifi_stack5.selected && wifi_stack5.exercise_complete && !wifi_stack5.failed && !wifi_stack5.running && wifi_stack5.net.state==MT76_NET5_CLOSING && !wifi_stack5.net.count && !wifi_stack5.net.pending && !wifi_stack5.tx_token && !wifi_stack5.net.dropped && wifi_stack5.net.enqueued==wifi_stack5.net.submitted && wifi_stack5.net.submitted==wifi_stack5.net.completed && wifi_stack5.net.completed==wifi_stream5.data_sends && wifi_stack5.net.received && !wifi_stack5.net.faults;}
static void wifi_stack5_emit(void){
 console_printf("wifi-stack5: complete=%u parent=%u restored=%u retired=%u empty=%u tx=%u rx=%u tx_wraps=%u rx_wraps=%u data_tx=%u data_rx=%u borrowed=%u released=%u faults=%u services=%u elapsed_us=%u enqueued=%u submitted=%u completed=%u received=%u provider_faults=%u pending=%u retained=1 backend_retired=0\n",(uint64_t)(wifi_stream5.complete && wifi_stream5.parent_complete && wifi_stream5.retired),(uint64_t)wifi_stream5.parent_complete,(uint64_t)wifi_stream5.restored,(uint64_t)wifi_stream5.retired,(uint64_t)wifi_stream5.events_empty,wifi_stream5.posts[0],wifi_stream5.posts[1],(uint64_t)wifi_stream5.wraps[0],(uint64_t)wifi_stream5.wraps[1],(uint64_t)wifi_stream5.data_sends,(uint64_t)wifi_stream5.data_received,wifi_stream5.dma.borrowed,wifi_stream5.dma.released,wifi_stream5.dma.faults,wifi_stack5.services,wifi_stack5.last-wifi_stack5.start,wifi_stack5.net.enqueued,wifi_stack5.net.submitted,wifi_stack5.net.completed,wifi_stack5.net.received,wifi_stack5.net.faults,(uint64_t)wifi_stack5.net.pending);
}
/* Caller supplies a bounded production exercise,initializing net.c only after
 * provider attachment. It must return with queue/TX callbacks idle. */
bool xhci_wifi_capture_stack5(bool (*exercise)(void *,struct mt76_net5 *),void *context){
 if(!exercise || wifi_stack5.selected || wifi_stream5.attempted || wifi_stream5.stack_mode)return false;
 wifi_stack5.selected=true;wifi_stack5.context=context;wifi_stack5.exercise=exercise;wifi_stream5.stack_mode=true;
 bool good=wifi_stream5_capture(false);
 if(wifi_stack5.net.attached && wifi_stack5_retirement(&wifi_stream5,wifi_stream5.epoch))wifi_stack5.detached=mt76_net5_finish_detach(&wifi_stack5.net,wifi_stream5.epoch);
 if(wifi_stack5.net.identity)wifi_stack5.retired=mt76_net5_retire(&wifi_stack5.net);
 wifi_stack5.exercise=NULL;wifi_stack5.context=NULL;
 console_printf("wifi-stack5-final: complete=%u detached=%u provider_retired=%u retained=1 backend_retired=0\n",(uint64_t)(good && wifi_stack5.detached && wifi_stack5.retired),(uint64_t)wifi_stack5.detached,(uint64_t)wifi_stack5.retired);
 return good && wifi_stack5.detached && wifi_stack5.retired;
}
