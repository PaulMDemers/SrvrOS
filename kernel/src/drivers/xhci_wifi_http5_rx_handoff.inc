/* Consuming terminal handoff: software provider authority ends here, while the
 * original raw RX lease/events/pages stay owned by outer MAC drain/exact Stop.
 * No endpoint-stop, DMA-release, key-erasure or command publication is implied. */
static bool wifi_stack5_terminal_raw_rx(void) {
 const struct mt76_dma5 *d=&wifi_stream5.dma;
 if(d->identity!=(uintptr_t)d || d->epoch!=wifi_stream5.epoch || d->state!=MT76_DMA5_RUNNING || d->positions!=63 || d->noop_only ||
    wifi_stream5.pending[0] || wifi_stream5.loan.ticket || wifi_stream5.cached_present || wifi_stream5.cached_size ||
    (wifi_stream5.pending[1] && wifi_stream5.ready_rx) || wifi_stream5.posts[1]>WIFI_STREAM5_RX ||
    wifi_stream5.completions[1]>wifi_stream5.posts[1] || wifi_stream5.posts[1]-wifi_stream5.completions[1]!=(uint64_t)wifi_stream5.pending[1])return false;
 unsigned device=0,complete=0;
 for(unsigned i=0;i<MT76_DMA5_PAGES;i++) {
  if(d->pages[0][i].state!=MT76_DMA5_FREE)return false;
  enum mt76_dma5_page_state state=d->pages[1][i].state;
  if(state==MT76_DMA5_DEVICE)device++;
  else if(state==MT76_DMA5_COMPLETE)complete++;
  else if(state!=MT76_DMA5_FREE)return false;
 }
 if(device!=(unsigned)wifi_stream5.pending[1] || complete!=(unsigned)wifi_stream5.ready_rx)return false;
 if(!device && !complete)return true;
 const struct mt76_dma5_lease *l=&wifi_stream5.lease[1];
 if(l->epoch!=d->epoch || l->direction!=1 || l->index>=MT76_DMA5_PAGES || !l->ticket || !l->token || l->length!=4096 || l->received || l->cycle>1 ||
    l->page!=wifi_stream5.pages[1][l->index] || l->trb<d->ring[1] || l->trb>=d->ring[1]+63*16 || (l->trb-d->ring[1])%16)return false;
 const struct mt76_dma5_page *p=&d->pages[1][l->index];const struct mt76_dma5_lease *q=&p->lease;
 return p->state==(device?MT76_DMA5_DEVICE:MT76_DMA5_COMPLETE) && q->epoch==l->epoch && q->ticket==l->ticket && q->token==l->token && q->page==l->page &&
  q->trb==l->trb && q->direction==l->direction && q->index==l->index && q->length==l->length && q->cycle==l->cycle &&
  (device?!q->received:q->received<=q->length);
}
static bool wifi_stack5_terminal_provider_closing(void *c,uint64_t epoch) {
 const struct mt76_net5 *p=&wifi_stack5.net;const struct mt76_net5_io *io=&p->io,*guard=&p->io_guard;
 return wifi_stack5_owned(c,epoch) && p->state==MT76_NET5_CLOSING && p->attached && p->epoch==epoch && !p->retire_requested &&
  io->context==c && guard->context==c && io->owned==wifi_stack5_owned && guard->owned==wifi_stack5_owned &&
  io->submit==wifi_stack5_submit && guard->submit==wifi_stack5_submit && io->service==wifi_stack5_service && guard->service==wifi_stack5_service &&
  io->receive==wifi_stack5_receive && guard->receive==wifi_stack5_receive && io->retired==wifi_stack5_retirement && guard->retired==wifi_stack5_retirement &&
  p->ready==wifi_stack5_ready && p->ready_guard==wifi_stack5_ready;
}
static bool wifi_stack5_terminal_rx_handoff(void *c,uint64_t epoch) {
 uint64_t flags=event_irq_save();struct mt76_net5 *p=&wifi_stack5.net;
 bool good=!wifi_stack5.terminal_handoff && wifi_stack5_probe_current(c,epoch) && wifi_stack5_clock() &&
  !p->callbacks && !p->io_busy && !p->polling && !p->delivering && !p->count && !p->pending && !p->pending_token && !p->loan_size && !p->faults && !p->dropped &&
  p->enqueued==p->submitted && p->submitted==p->completed && !wifi_stack5.tx_token && !wifi_stack5.boundary && !wifi_fence5_native.round && !xhci.command_pending &&
  wifi_stack5_terminal_raw_rx();
 uint64_t enqueued=p->enqueued,submitted=p->submitted,completed=p->completed,received=p->received;
 if(good)good=mt76_net5_disconnect(p,epoch) && wifi_stack5_terminal_provider_closing(c,epoch) &&
  !p->callbacks && !p->io_busy && !p->polling && !p->delivering && !p->count && !p->pending && !p->pending_token && !p->loan_size && !p->faults && !p->dropped &&
  p->enqueued==enqueued && p->submitted==submitted && p->completed==completed && p->received==received && wifi_stack5_terminal_raw_rx();
 if(good) {
  wifi_stack5.terminal_pending=wifi_stream5.pending[1];wifi_stack5.terminal_ready=wifi_stream5.ready_rx;
  wifi_stack5.terminal_epoch=epoch;wifi_stack5.terminal_provider_closing=p->state==MT76_NET5_CLOSING;
  wifi_stack5.terminal_ticket=(wifi_stream5.pending[1] || wifi_stream5.ready_rx)?wifi_stream5.lease[1].ticket:0;
  wifi_stack5.terminal_token=(wifi_stream5.pending[1] || wifi_stream5.ready_rx)?wifi_stream5.lease[1].token:0;
  wifi_stack5.terminal_posts=wifi_stream5.posts[1];wifi_stack5.terminal_completions=wifi_stream5.completions[1];
  wifi_stack5.terminal_handoff=true;
 }
 event_irq_restore(flags);return good;
}
/* A hardware/event/clock fault may use the old retained-DMA unwind only
 * while the software publication seal is still proven. Lost software ownership
 * must hold before reaching WPA's unconditional local-key erasure. */
static bool wifi_stack5_terminal_software_closed(void *c,uint64_t epoch) {
 const struct mt76_net5 *p=&wifi_stack5.net;const struct mt76_net5_io *io=&p->io,*guard=&p->io_guard;
 if(!wifi_stack5.terminal_handoff || !wifi_stack5.terminal_provider_closing || !wifi_stack5.terminal_epoch ||
    c!=&wifi_stream5 || epoch!=wifi_stack5.terminal_epoch || wifi_stream5.epoch!=epoch ||
    p->identity!=(uintptr_t)p || p->state!=MT76_NET5_CLOSING || !p->attached || p->epoch!=epoch || p->retire_requested ||
    p->callbacks || p->io_busy || p->polling || p->delivering || p->count || p->pending || p->pending_token || p->loan_size ||
    wifi_stream5.cached_present || wifi_stream5.cached_size || wifi_stream5.loan.ticket || wifi_stack5.tx_token ||
    io->context!=c || guard->context!=c || io->owned!=wifi_stack5_owned || guard->owned!=wifi_stack5_owned ||
    io->submit!=wifi_stack5_submit || guard->submit!=wifi_stack5_submit || io->service!=wifi_stack5_service || guard->service!=wifi_stack5_service ||
    io->receive!=wifi_stack5_receive || guard->receive!=wifi_stack5_receive || io->retired!=wifi_stack5_retirement || guard->retired!=wifi_stack5_retirement ||
    p->ready!=wifi_stack5_ready || p->ready_guard!=wifi_stack5_ready)return false;
 for(unsigned dir=0;dir<2;dir++)for(unsigned i=0;i<MT76_DMA5_PAGES;i++)if(wifi_stream5.dma.pages[dir][i].state==MT76_DMA5_BORROWED)return false;
 return true;
}
/* Only the consuming HTTP path may accept CLOSING with raw RX retained. The
 * original UP/empty-cache path remains byte-for-byte outside this branch. */
static enum mt76_join5_send wifi_stack5_terminal_window(bool good,void *c,uint64_t epoch) {
 uint64_t flags=event_irq_save();struct mt76_net5 *p=&wifi_stack5.net;
 if(!wifi_stack5_terminal_software_closed(c,epoch)){event_irq_restore(flags);wifi_http5_hold(5);}
 bool safe=wifi_stack5.terminal_handoff && c==&wifi_stream5 && epoch==wifi_stream5.epoch &&
  wifi_stack5_terminal_provider_closing(c,epoch) && !p->callbacks && !p->io_busy && !p->polling && !p->delivering &&
  !p->count && !p->pending && !p->pending_token && !p->loan_size && !p->faults && !p->dropped &&
  !wifi_stack5.tx_token && !wifi_stack5.boundary && !wifi_fence5_native.round && !xhci.command_pending && wifi_stack5_terminal_raw_rx();
 good=good && safe && wifi_stack5_clock() && wifi_stream5_data_owned(c);
 wifi_stack5.exercise_complete=good;
 bool unsafe=!safe || wifi_stack5.failed || wifi_stream5.failed || !wifi_stream5_data_owned(c) || p->pending || wifi_stack5.tx_token || p->faults;
 if(unsafe && !wifi_stream5.failed)wifi_stack5_fault();
 wifi_stack5.running=false;wifi_stream5.closed=true;wifi_stream5.data_active=false;
 zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_size=0;wifi_stream5.cached_present=false;
 event_irq_restore(flags);
 return unsafe?MT76_JOIN5_UNCERTAIN:good?MT76_JOIN5_SENT:MT76_JOIN5_REFUSED;
}
