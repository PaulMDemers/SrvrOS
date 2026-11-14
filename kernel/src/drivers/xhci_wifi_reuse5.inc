/* Explicit v133:reuse pages only after real validated completion/CPU release.
 * TD addresses remain unique within the existing48 RX/11 TX budget. No wrap. */
static unsigned wifi_wpa5_rx_index(unsigned i){return wifi_reuse5_requested?i%MT76_DMA5_PAGES:i;}
static unsigned wifi_wpa5_tx_index(unsigned i){return WIFI_WPA5_RX+(wifi_reuse5_requested?i%MT76_DMA5_PAGES:i);}
static bool wifi_reuse5_init(void){
 uint64_t rings[2]={wifi_wpa5.tx_phys,wifi_wpa5.rx_phys},pages[2][MT76_DMA5_PAGES];
 for(unsigned i=0;i<MT76_DMA5_PAGES;i++){pages[0][i]=wifi_wpa5.pages[WIFI_WPA5_RX+i];pages[1][i]=wifi_wpa5.pages[i];}
 wifi_reuse5.initialized=mt76_dma5_init_ring(&wifi_reuse5.dma,wifi_wpa5.peer.generation,rings,pages,XHCI_TRB_COUNT-1,timer_monotonic_us());
 if(wifi_reuse5.initialized)console_write("wifi-reuse5-progress: original owner;four RX/four TX pages;validated native completions;copy/authenticate/release before reuse;no descriptor wrap\n");
 return wifi_reuse5.initialized;
}
static bool wifi_reuse5_frozen(void){
 const struct mt76_dma5 *s=&wifi_reuse5.dma;
 if(wifi_wpa5.posts>WIFI_WPA5_RX || wifi_wpa5.sends>wifi_wpa5_tx_limit() || wifi_wpa5.rx_copied>wifi_wpa5.posts || !wifi_reuse5.initialized || s->identity!=(uintptr_t)s || s->positions!=XHCI_TRB_COUNT-1 || s->state!=MT76_DMA5_RUNNING || s->epoch!=wifi_wpa5.peer.generation || s->ring[0]!=wifi_wpa5.tx_phys || s->ring[1]!=wifi_wpa5.rx_phys ||
 s->cursor[0]!=wifi_wpa5.sends || s->cursor[1]!=wifi_wpa5.posts || s->cycle[0]!=1 || s->cycle[1]!=1 || s->prepared!=s->published || s->published!=wifi_wpa5.sends+wifi_wpa5.posts || s->next_ticket!=s->published || s->faults || s->wraps || s->cancelled || s->last_token[0]!=wifi_wpa5.sends || s->last_token[1]!=wifi_wpa5.posts || s->borrowed!=wifi_wpa5.rx_copied || s->released>s->borrowed || s->borrowed-s->released!=(wifi_reuse5.loan.ticket?1u:0u))return false;
 for(unsigned d=0;d<2;d++)for(unsigned p=0;p<MT76_DMA5_PAGES;p++){
  unsigned base=d?0:WIFI_WPA5_RX;const struct mt76_dma5_page *l=&s->pages[d][p];
  if(l->lease.page!=wifi_wpa5.pages[base+p] || l->state>MT76_DMA5_BORROWED || l->state==MT76_DMA5_PREPARED)return false;
 }
 for(unsigned p=0;p<MT76_DMA5_PAGES;p++){
  bool found=false;unsigned last=0;
  for(unsigned i=p;i<wifi_wpa5.posts;i+=MT76_DMA5_PAGES){found=true;last=i;}
  if(found && last<wifi_wpa5.rx_copied){if(!wifi_handoff5_equal(wifi_wpa5.buffer[p],wifi_wpa5.rx_snapshot[last],4096))return false;}
  else if(!found)for(unsigned j=0;j<4096;j++)if(wifi_wpa5.buffer[p][j])return false;
  found=false;last=0;for(unsigned i=p;i<wifi_wpa5.sends;i+=MT76_DMA5_PAGES){found=true;last=i;}
  unsigned n=found?wifi_wpa5.lengths[last]:0;
  if(n>wifi_wpa5_packet_limit() || (n && !wifi_handoff5_equal(wifi_wpa5.buffer[WIFI_WPA5_RX+p],wifi_wpa5.packets[last],n)))return false;
  for(unsigned j=n;j<4096;j++)if(wifi_wpa5.buffer[WIFI_WPA5_RX+p][j])return false;
 }
 for(unsigned p=MT76_DMA5_PAGES;p<WIFI_WPA5_RX;p++)for(unsigned j=0;j<4096;j++)if(wifi_wpa5.buffer[p][j])return false;
 for(unsigned p=MT76_DMA5_PAGES;p<wifi_wpa5_tx_limit();p++)for(unsigned j=0;j<4096;j++)if(wifi_wpa5.buffer[WIFI_WPA5_RX+p][j])return false;
 if(wifi_wpa5.phase<=3){
  if(wifi_wpa5.sends && s->pages[0][(wifi_wpa5.sends-1)%4].state!=(wifi_wpa5.tx_pending?MT76_DMA5_DEVICE:MT76_DMA5_FREE))return false;
  if(wifi_wpa5.posts){enum mt76_dma5_page_state want=wifi_wpa5.rx_pending?MT76_DMA5_DEVICE:!wifi_wpa5.rx_delivered?(wifi_reuse5.loan.ticket?MT76_DMA5_BORROWED:MT76_DMA5_COMPLETE):MT76_DMA5_FREE;if(s->pages[1][(wifi_wpa5.posts-1)%4].state!=want)return false;}
 }
 return true;
}
static bool wifi_reuse5_prepare(unsigned direction,unsigned position,unsigned length){
 struct mt76_dma5_lease *l=direction?&wifi_reuse5.rx:&wifi_reuse5.tx;
 if(!mt76_dma5_prepare(&wifi_reuse5.dma,direction,position+1,length,timer_monotonic_us(),l) || l->index!=position%4 || l->trb!=(direction?wifi_wpa5.rx_phys:wifi_wpa5.tx_phys)+16*position || l->cycle!=1)return wifi_wpa5_fail();
 zero_bytes(wifi_wpa5.buffer[direction?l->index:WIFI_WPA5_RX+l->index],4096);return true;
}
static bool wifi_reuse5_publish(unsigned direction){return mt76_dma5_publish(&wifi_reuse5.dma,direction?&wifi_reuse5.rx:&wifi_reuse5.tx,timer_monotonic_us())?true:wifi_wpa5_fail();}
static bool wifi_reuse5_complete(unsigned direction,unsigned cc,unsigned residue){return mt76_dma5_complete(&wifi_reuse5.dma,direction?&wifi_reuse5.rx:&wifi_reuse5.tx,cc,residue,timer_monotonic_us())?true:wifi_wpa5_fail();}
static bool wifi_reuse5_acquire(void){return mt76_dma5_acquire(&wifi_reuse5.dma,wifi_wpa5.peer.generation,&wifi_reuse5.loan) && wifi_reuse5.loan.index==wifi_wpa5_rx_index(wifi_wpa5.posts-1) && wifi_reuse5.loan.received==4096-wifi_wpa5.rx_residue?true:wifi_wpa5_fail();}
static bool wifi_reuse5_release(void){bool good=mt76_dma5_release(&wifi_reuse5.dma,&wifi_reuse5.loan);if(good)zero_bytes(&wifi_reuse5.loan,sizeof wifi_reuse5.loan);return good?true:wifi_wpa5_fail();}
static struct mt76_dma5_retirement wifi_reuse5_proof(bool outer){
 return (struct mt76_dma5_retirement){.epoch=wifi_wpa5.peer.generation,
 .keys_erased=wifi_wpa5.exchange.keys_erased && wpa2_ccmp_erased(&wifi_wpa5.exchange.keys,wifi_wpa5.peer.generation) && !wifi_wpa5.exchange.handshake.keys_may_live,
 .mac_disabled=wifi_wpa5.mac.mac_disabled && wifi_wpa5.mac.tx_drained && wifi_wpa5.mac.rx_drained,
 .tx_stopped=wifi_wpa5.phase==9 && wifi_wpa5.commands==6 && wifi_wpa5.stop_events[0]==1 && !wifi_wpa5.tx_pending,
 .rx_stopped=wifi_wpa5.phase==9 && wifi_wpa5.commands==6 && wifi_wpa5.stop_events[1]==1 && !wifi_wpa5.rx_pending,
 .events_drained=wifi_reuse5.events_drained && !xhci.command_pending && !xhci.sync_users && !strict_control.device,
 .callbacks_drained=!wifi_provider5.net.callbacks && !wifi_provider5.net.io_busy && !wifi_provider5.net.polling && !wifi_provider5.net.delivering && !wifi_reuse5.loan.ticket,
 .parent_restored=outer && !wifi_wpa5.active && wifi_wpa5.restored && !wifi_wpa5.data_active && wifi_wpa5.phase==9};
}
