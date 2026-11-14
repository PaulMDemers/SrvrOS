/* Explicit v132 only. Borrow the original post-M4 owner and immutable DMA
 * budget. Provider callbacks never wait/control-transfer/log. The private
 * six-packet diagnostic may wait outside those callbacks. No ring reuse. */
static bool wifi_provider5_backend_owned(void *c,uint64_t epoch){
 return wifi_provider5_requested && c==&wifi_wpa5 && epoch==wifi_wpa5.peer.generation && wifi_data5_owned(c);
}
static bool wifi_provider5_owned(void *c){
 return wifi_provider5.net.state==MT76_NET5_UP && wifi_provider5.net.attached &&
 wifi_provider5.net.epoch==wifi_wpa5.peer.generation && wifi_provider5_backend_owned(c,wifi_wpa5.peer.generation);
}
static enum mt76_net5_submit wifi_provider5_submit(void *c,uint64_t epoch,uint64_t token,const uint8_t *frame,uint16_t length){
 struct mt76_net5 *p=&wifi_provider5.net;struct wpa2_ccmp *k=&wifi_wpa5.exchange.keys;
 unsigned index=wifi_wpa5.data_sends,n=wifi_wpa5.sends;
 if(!wifi_provider5_backend_owned(c,epoch) || p->state!=MT76_NET5_UP || !p->io_busy || !p->pending || p->pending_token!=token ||
 frame!=p->loan || length!=p->loan_size || index>=6 || n!=4+index || wifi_wpa5.tx_pending || wifi_provider5.token)return MT76_NET5_REFUSED;
 uint64_t before=k->tx_pn;
 size_t bytes=mt76_data5_tx(&wifi_wpa5.data_scratch,k,epoch,frame,length,9+index,wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);
 if(!bytes || k->tx_pn!=before+1){
  zero_bytes(wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);
  return k->tx_pn==before?MT76_NET5_REFUSED:MT76_NET5_UNCERTAIN;
 }
 wifi_wpa5.data_pn=k->tx_pn;
 if(!wifi_provider5_backend_owned(c,epoch)){zero_bytes(wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);return MT76_NET5_UNCERTAIN;}
 if(wifi_reuse5_requested && !wifi_reuse5_prepare(0,n,(unsigned)bytes)){zero_bytes(wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);return MT76_NET5_UNCERTAIN;}
 unsigned page=wifi_wpa5_tx_index(n);copy_bytes(wifi_wpa5.buffer[page],wifi_wpa5.data_usb,bytes);copy_bytes(wifi_wpa5.packets[n],wifi_wpa5.data_usb,bytes);
 wifi_wpa5.lengths[n]=(unsigned)bytes;wifi_wpa5.kinds[n]=5;zero_bytes(wifi_wpa5.data_usb,sizeof wifi_wpa5.data_usb);
 struct xhci_trb td={.parameter_low=wifi_wpa5.pages[page],.parameter_high=wifi_wpa5.pages[page]>>32,.status=(uint32_t)bytes,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC};
 uint64_t flags=event_irq_save();if(wifi_reuse5_requested && !wifi_reuse5_publish(0)){event_irq_restore(flags);return MT76_NET5_UNCERTAIN;}wifi_wpa5.sends++;wifi_wpa5.tx_pending=true;wifi_wpa5.tx_done=false;wifi_wpa5.tx_events=0;
 wifi_provider5.token=token;wifi_provider5.epoch=epoch;
 uint64_t ptr=ring_push(wifi_wpa5.tx,wifi_wpa5.tx_phys,&wifi_wpa5.tx_enqueue,&wifi_wpa5.tx_cycle,&td);
 bool good=ptr==wifi_wpa5.tx_phys+16*n;
 if(good)db_write32(wifi_wpa5.d->slot_id,8);
 event_irq_restore(flags);
 return good && wifi_provider5_backend_owned(c,epoch)?MT76_NET5_QUEUED:MT76_NET5_UNCERTAIN;
}
static bool wifi_provider5_complete(void){
 struct mt76_net5 *p=&wifi_provider5.net;uint64_t token=wifi_provider5.token,epoch=wifi_provider5.epoch;
 if(!token || !wifi_wpa5.data_active || !wifi_wpa5.tx_done || wifi_wpa5.tx_events!=1 ||
 !wifi_provider5_backend_owned(&wifi_wpa5,epoch) || !mt76_net5_tx_complete(p,epoch,token,true))return false;
 unsigned index=wifi_wpa5.data_sends;
 if(index>=6 || wifi_wpa5.sends!=5+index || !wifi_data5_record((struct wifi_data5_event){.kind=0,.index=index,.bytes=wifi_wpa5.lengths[wifi_wpa5.sends-1],.present=1,.reason=(unsigned)wifi_wpa5.data_pn}))return false;
 wifi_wpa5.data_sends++;wifi_provider5.token=wifi_provider5.epoch=0;return true;
}
static bool wifi_provider5_service(void *c,uint64_t epoch){
 if(!wifi_provider5_backend_owned(c,epoch))return false;
 wifi_wpa5_dispatch();return wifi_provider5_backend_owned(c,epoch);
}
static bool wifi_provider5_receive(void *c,uint64_t epoch,uint8_t *out,size_t cap,size_t *n,bool *present){
 return wifi_provider5_backend_owned(c,epoch) && out==wifi_provider5.net.rx && cap==sizeof wifi_provider5.net.rx &&
 wifi_wpa5_poll(c,MT76_WPA5_DATA,out,cap,n,present) && wifi_provider5_backend_owned(c,epoch);
}
static bool wifi_provider5_retired(void *c,uint64_t epoch){
 return c==&wifi_wpa5 && epoch==wifi_wpa5.peer.generation && wifi_provider5.outer_retired && wifi_provider5.closed &&
 !wifi_wpa5.active && !wifi_wpa5.data_active && wifi_wpa5.restored && wifi_wpa5.phase==9 &&
 wifi_wpa5.exchange.keys_erased && wpa2_ccmp_erased(&wifi_wpa5.exchange.keys,epoch) && !wifi_wpa5.exchange.handshake.keys_may_live &&
 wifi_wpa5.mac.mac_disabled && wifi_wpa5.mac.tx_drained && wifi_wpa5.mac.rx_drained && wifi_wpa5.mac.filter_restored && wifi_wpa5.mac.txop_restored && (!wifi_reuse5.initialized || wifi_reuse5.retired);
}
static bool wifi_provider5_start(void){
 if(!wifi_provider5_requested || wifi_provider5.attempted || !wifi_data5_owned(&wifi_wpa5))return false;
 wifi_provider5.attempted=true;
 wifi_provider5.registered=mt76_net5_init(&wifi_provider5.net,wifi_wpa5.peer.peer.station);
 const struct mt76_net5_io io={&wifi_wpa5,wifi_provider5_backend_owned,wifi_provider5_submit,wifi_provider5_service,wifi_provider5_receive,wifi_provider5_retired};
 if(wifi_provider5.registered)wifi_provider5.attached=mt76_net5_attach(&wifi_provider5.net,wifi_wpa5.peer.generation,&io);
 if(wifi_provider5.attached && net_device_select()){
  const char *name=net_device_name();static const char want[]="mt76-ccmp";
  bool match=true;for(unsigned i=0;i<sizeof want;i++)if(name[i]!=want[i]){match=false;break;}
  wifi_provider5.selected=match && net_device_mac() && wifi_handoff5_equal(net_device_mac(),wifi_wpa5.peer.peer.station,6);
 }
 if(wifi_provider5.selected && wifi_reuse5_requested)console_write("wifi-provider5-progress: deferred registry selected;original active CCMP owner;nonblocking USB submit/completion;six strict IP requests;bounded reusable pages;no descriptor wrap\n");
 else if(wifi_provider5.selected)console_write("wifi-provider5-progress: deferred registry selected;original active CCMP owner;nonblocking USB submit/completion;six strict IP requests;no ring reuse\n");
 return wifi_provider5.selected;
}
static enum mt76_join5_send wifi_provider5_send(void *c,const uint8_t *frame,size_t length){
 struct mt76_net5 *p=&wifi_provider5.net;unsigned index=wifi_wpa5.data_sends;uint64_t completed=p->completed;
 if(!wifi_provider5_owned(c) || frame!=wifi_wpa5.ip.tx || length>NET_DEVICE_FRAME_MAX || index>=6 || !net_device_send(frame,(uint16_t)length))return MT76_JOIN5_REFUSED;
 uint64_t start=timer_monotonic_us(),previous=start;
 for(unsigned i=0;i<8192;i++){
  if(!mt76_net5_service(p) || !wifi_provider5_owned(c))return MT76_JOIN5_UNCERTAIN;
  if(p->completed==completed+1 && !p->pending && !p->count && wifi_wpa5.tx_done && wifi_wpa5.data_sends==index+1)return MT76_JOIN5_SENT;
  if(!p->pending && !p->count)return MT76_JOIN5_REFUSED;
  uint64_t now=timer_monotonic_us();if(now<previous || now-start>=750000)return MT76_JOIN5_UNCERTAIN;previous=now;
  if(!wifi_wpa5_delay(c,100))return MT76_JOIN5_UNCERTAIN;
 }
 return MT76_JOIN5_UNCERTAIN;
}
static bool wifi_provider5_close(void){
 struct mt76_net5 *p=&wifi_provider5.net;
 if(!wifi_provider5.registered)return true;
 bool safe=!p->pending && !p->count && !p->faults;
 wifi_provider5.closed=!p->attached || mt76_net5_disconnect(p,p->epoch);
 wifi_provider5.early_refused=!p->attached || !mt76_net5_finish_detach(p,p->epoch);
 if(wifi_reuse5_requested){struct mt76_dma5_retirement p=wifi_reuse5_proof(false);wifi_reuse5.early_refused=!mt76_dma5_retire(&wifi_reuse5.dma,&p);safe=safe && wifi_reuse5.early_refused;}
 return safe && wifi_provider5.closed && wifi_provider5.early_refused;
}
static bool wifi_reuse5_finish(bool outer){
 bool empty=false;unsigned events=0;
 if(outer){
  uint64_t flags=event_irq_save();struct xhci_trb e;
  for(;events<32;events++){
   if(!poll_event(&e)){empty=true;break;}
   uint64_t ptr=e.parameter_low|((uint64_t)e.parameter_high<<32);
   if(((e.control>>10)&63)!=32 || (ptr>=wifi_wpa5.rx_phys && ptr<wifi_wpa5.rx_phys+1024) || (ptr>=wifi_wpa5.tx_phys && ptr<wifi_wpa5.tx_phys+1024))break;
   dispatch_transfer_event(&e);
  }
  event_irq_restore(flags);
 }
 wifi_reuse5.events_drained=empty;
 struct mt76_dma5_retirement proof=wifi_reuse5_proof(outer);
 if(outer && mt76_dma5_close(&wifi_reuse5.dma,wifi_wpa5.peer.generation))wifi_reuse5.retired=mt76_dma5_retire(&wifi_reuse5.dma,&proof);
 const struct mt76_dma5 *s=&wifi_reuse5.dma;
 bool good=outer && wifi_reuse5.retired && wifi_reuse5.early_refused && s->published==wifi_wpa5.sends+wifi_wpa5.posts && s->completed>=wifi_wpa5.sends+wifi_wpa5.rx_copied && s->completed<=s->published && s->borrowed==wifi_wpa5.rx_copied && s->released==s->borrowed && !s->faults && !s->wraps && s->state==MT76_DMA5_RETIRED;
 console_printf("wifi-reuse5: complete=%u initialized=%u tx_pages=4 rx_pages=4 usable_trbs=%u tx_posts=%u rx_posts=%u completed=%u borrowed=%u released=%u tx_reused=%u rx_reused=%u early_release_refused=%u events_drained=%u drain_events=%u retired=%u faults=%u descriptor_wraps=0 retained=1 ordinary_rx=0\n",(uint64_t)good,(uint64_t)wifi_reuse5.initialized,(uint64_t)(XHCI_TRB_COUNT-1),(uint64_t)wifi_wpa5.sends,(uint64_t)wifi_wpa5.posts,s->completed,s->borrowed,s->released,(uint64_t)(wifi_wpa5.sends>4?wifi_wpa5.sends-4:0),(uint64_t)(wifi_wpa5.posts>4?wifi_wpa5.posts-4:0),(uint64_t)wifi_reuse5.early_refused,(uint64_t)wifi_reuse5.events_drained,(uint64_t)events,(uint64_t)wifi_reuse5.retired,s->faults);
 return good;
}
bool xhci_wifi_capture_provider5(void){
 if(wifi_provider5_requested || wifi_provider5.attempted || wifi_wpa5.attempted)return false;
 wifi_provider5_requested=true;bool good=xhci_wifi_capture_data5();
 wifi_provider5.outer_retired=good && wifi_provider5.closed;
 if(wifi_reuse5_requested)good=wifi_reuse5_finish(good);
 if(wifi_provider5.registered)wifi_provider5.retired=mt76_net5_retire(&wifi_provider5.net);
 struct mt76_net5 *p=&wifi_provider5.net;
 bool accepted=good && wifi_provider5.registered && wifi_provider5.attached && wifi_provider5.selected && wifi_provider5.closed &&
 wifi_provider5.retired && wifi_provider5.early_refused && p->state==MT76_NET5_RETIRED && p->enqueued==6 && p->submitted==6 && p->completed==6 && !p->faults && !p->dropped && !p->attached && !p->pending && !p->io.context;
 console_printf("wifi-provider5: complete=%u registered=%u attached=%u selected=%u queued=%u submitted=%u completed=%u closed=%u early_release_refused=%u outer_retired=%u detached=%u registry_retired=%u pending=%u faults=%u dropped=%u retained=1 ring_reuse=%u ordinary_rx=0\n",(uint64_t)accepted,(uint64_t)wifi_provider5.registered,(uint64_t)wifi_provider5.attached,(uint64_t)wifi_provider5.selected,p->enqueued,p->submitted,p->completed,(uint64_t)wifi_provider5.closed,(uint64_t)wifi_provider5.early_refused,(uint64_t)wifi_provider5.outer_retired,(uint64_t)!p->attached,(uint64_t)p->registry_drained,(uint64_t)p->pending,p->faults,p->dropped,(uint64_t)wifi_reuse5_requested);
 wifi_provider5_requested=false;return accepted;
}

bool xhci_wifi_capture_reuse5(void){
 if(wifi_reuse5_requested || wifi_reuse5.initialized || wifi_wpa5.attempted)return false;
 wifi_reuse5_requested=true;bool good=xhci_wifi_capture_provider5();wifi_reuse5_requested=false;return good;
}
