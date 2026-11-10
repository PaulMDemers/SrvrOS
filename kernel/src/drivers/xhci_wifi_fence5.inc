/* Dormant bounded native binding. No boot flag or caller enables this owner.
 * CPU0/IRQ serialized;the separately verified caller freezes BOTH ledger
 * directions between begin/step calls and closes publication on every false.
 * No key copies,ordinary ring wrap authority or hardware retirement booleans. */
#define WIFI_FENCE5_CLEANUP_COMMANDS 6u
#define WIFI_FENCE5_COMMANDS 62u
struct wifi_fence5_native_command {
 struct mt76_fence5_command id;struct xhci_trb shadow,event,stop;
 uint32_t stop_context[8];uint64_t stop_dequeue;
 bool latched,consumed,stop_event;unsigned cc;
};
struct wifi_fence5_native_state {
 bool bound,round,failed,retired,busy,noop,fault_event_valid,opposite_valid;unsigned direction;struct xhci_trb fault_event;unsigned count,first,slot,positions;
 bool cycle;uint64_t epoch,last_us,frontier;
 struct xhci_device *device;struct mt76_dma5 *dma;
 void *owner;bool (*owned)(void *,uint64_t,unsigned);
 uint64_t command_phys;struct xhci_trb *command_ring;
 uint8_t *output,*input;uint64_t output_phys,input_phys;
 uint32_t normal_parent[3][8],normal_config[2][8],opposite[8];
 uint64_t rings[2],pages[2][4];uint32_t unrelated[4][8];
 struct mt76_fence5 engine;
 struct wifi_fence5_native_command commands[WIFI_FENCE5_COMMANDS];
};
/* Each connection gets one never-reinitialized fence engine. The first owner
 * remains a retained tombstone after the explicit one-time transition. */
static struct wifi_fence5_native_state wifi_fence5_first,wifi_fence5_second;
static struct wifi_fence5_native_state *wifi_fence5_active=&wifi_fence5_first;
static bool wifi_fence5_connection_switched;
static unsigned wifi_fence5_previous_frontier;
static void *wifi_fence5_next_owner;
static uint64_t wifi_fence5_next_epoch;
#define wifi_fence5_native (*wifi_fence5_active)
static void wifi_fence5_bad_event(const struct xhci_trb *e){if(!wifi_fence5_native.fault_event_valid){copy_trb(&wifi_fence5_native.fault_event,e);wifi_fence5_native.fault_event_valid=true;}}
static bool wifi_fence5_fault(void){
 wifi_fence5_native.failed=true;
 if(wifi_fence5_native.bound)mt76_dma5_fault(wifi_fence5_native.dma);
 /* Never clear command_pending or recycle a potentially published command. */
 if(wifi_fence5_native.round)xhci.command_failed=true;
 return false;
}
static bool wifi_fence5_sync_admitted(void){
 if(wifi_fence5_connection_switched &&
    (wifi_fence5_native.failed || wifi_fence5_first.failed ||
     xhci.command_ring!=wifi_fence5_first.command_ring || xhci.command_ring_phys!=wifi_fence5_first.command_phys ||
     xhci.command_cycle!=wifi_fence5_first.cycle || xhci.command_enqueue<wifi_fence5_previous_frontier || xhci.command_enqueue>=63))return false;
 if(!wifi_fence5_native.bound || wifi_fence5_native.retired)return true;
 return !wifi_fence5_native.round && !wifi_fence5_native.failed &&
  xhci.command_ring==wifi_fence5_native.command_ring && xhci.command_ring_phys==wifi_fence5_native.command_phys &&
  xhci.command_cycle==wifi_fence5_native.cycle && xhci.command_enqueue>=wifi_fence5_native.first && xhci.command_enqueue<63;
}
static bool wifi_fence5_geometry(void){
 struct mt76_dma5 *d=wifi_fence5_native.dma;
 if(d->noop_only!=wifi_fence5_native.noop)return false;
 for(unsigned dir=0;dir<2;dir++){
  if(d->ring[dir]!=wifi_fence5_native.rings[dir])return false;
  for(unsigned i=0;i<4;i++)if(d->pages[dir][i].lease.page!=wifi_fence5_native.pages[dir][i])return false;
 }
 if(wifi_fence5_native.noop){const unsigned indices[4]={0,9,11,16};
  for(unsigned i=0;i<4;i++)for(unsigned w=0;w<8;w++)if(context_read32(wifi_fence5_native.output,indices[i],w)!=wifi_fence5_native.unrelated[i][w])return false;
 }else{
  struct xhci_device *device=wifi_fence5_native.device;
  if(!device || device->input_context!=wifi_fence5_native.input || device->input_context_phys!=wifi_fence5_native.input_phys)return false;
  const unsigned indices[3]={0,11,16};
  for(unsigned i=0;i<3;i++)for(unsigned w=0;w<8;w++)if(context_read32(wifi_fence5_native.output,indices[i],w)!=wifi_fence5_native.normal_parent[i][w])return false;
  for(unsigned dir=0;dir<2;dir++){
   uint64_t deq=context_read32(wifi_fence5_native.output,8+dir,2)|((uint64_t)context_read32(wifi_fence5_native.output,8+dir,3)<<32);
   if((deq&14) || (deq&~15ull)<d->ring[dir] || (deq&~15ull)>d->ring[dir]+wifi_fence5_native.positions*16)return false;
  }
  for(unsigned dir=0;dir<2;dir++)for(unsigned w=0;w<8;w++){
   uint32_t value=context_read32(wifi_fence5_native.output,8+dir,w),expected=wifi_fence5_native.normal_config[dir][w];
   if(w==2 || w==3)continue;
   if(w==0){if((value&7)!=1 && (value&7)!=3)return false;value&=~7u;expected&=~7u;}
   if(value!=expected)return false;
  }
  if(wifi_fence5_native.round && wifi_fence5_native.opposite_valid){
   if(wifi_fence5_native.direction>1)return false;
   unsigned other=wifi_fence5_native.direction^1u;
   for(unsigned w=0;w<8;w++)if(context_read32(wifi_fence5_native.output,8+other,w)!=wifi_fence5_native.opposite[w])return false;
  }
 }
 return true;
}
static bool wifi_fence5_base(void){
 struct xhci_device *d=wifi_fence5_native.device;
 bool listed=false;for(unsigned i=0;i<XHCI_MAX_DEVICES;i++)if(d==&xhci.devices[i])listed=true;
 return wifi_fence5_native.bound && !wifi_fence5_native.failed && !wifi_fence5_native.retired && listed && d &&
  xhci.operational && xhci.controller_owned && !xhci.stopping && !xhci.reclaiming && !xhci.reclaimed && !xhci.command_failed &&
  xhci.pci && xhci.pci->vendor_id==0x8086 && xhci.pci->device_id==0x8c31 &&
  d->present && !d->removed && !d->retiring && !d->slot_disabled && !d->ep0_quarantined &&
  d->slot_id==wifi_fence5_native.slot && d->vendor_id==0x7392 && d->product_id==0xa711 && d->device_bcd==0x100 &&
  d->wifi_probe_attempted && d->wifi_probe_ok && !d->control_pending && !strict_control.device &&
  !d->firmware.pending && !d->firmware.mcu.pending && !xhci.sync_users &&
  d->output_context==wifi_fence5_native.output && d->output_context_phys==wifi_fence5_native.output_phys &&
  xhci.command_ring==wifi_fence5_native.command_ring && xhci.command_ring_phys==wifi_fence5_native.command_phys &&
  xhci.command_cycle==wifi_fence5_native.cycle && xhci.command_enqueue>=wifi_fence5_native.first && xhci.command_enqueue<63 &&
  wifi_fence5_native.dma->epoch==wifi_fence5_native.epoch && wifi_fence5_native.dma->positions==wifi_fence5_native.positions && (wifi_fence5_native.positions==63 || wifi_fence5_native.positions==127) && wifi_fence5_geometry();
}
/* Capacity estimate only;begin/publication still independently validate. */
static __attribute__((unused)) unsigned wifi_fence5_commands_available(void){
 if(!wifi_fence5_base() || wifi_fence5_native.round || xhci.command_pending || xhci.command_enqueue+WIFI_FENCE5_CLEANUP_COMMANDS>=63)return 0;
 return (unsigned)(63-xhci.command_enqueue-WIFI_FENCE5_CLEANUP_COMMANDS);
}
static bool wifi_fence5_owned(void *c,uint64_t epoch,unsigned slot){
 if(c!=&wifi_fence5_native || epoch!=wifi_fence5_native.epoch || slot!=wifi_fence5_native.slot || !wifi_fence5_base())return false;
 return wifi_fence5_native.owned(wifi_fence5_native.owner,epoch,slot) && wifi_fence5_base();
}
static bool wifi_fence5_empty(void){
 if(!xhci.event_ring || xhci.event_dequeue>=XHCI_EVENT_TRB_COUNT)return false;
 __atomic_thread_fence(__ATOMIC_ACQUIRE);
 return ((((volatile struct xhci_trb *)xhci.event_ring)[xhci.event_dequeue].control&1u)!=0)!=xhci.event_cycle;
}
/* Never attribute an old command/TD pointer to the newer connection. The
 * command ring does not wrap in this finite two-connection admission. */
static bool wifi_fence5_previous_event(const struct xhci_trb *e){
 if(!wifi_fence5_connection_switched)return false;
 unsigned type=(e->control>>10)&63;uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);
 if(type==33 && ptr>=wifi_fence5_first.command_phys && ptr<wifi_fence5_first.command_phys+16*wifi_fence5_previous_frontier)return true;
 if(type==32)for(unsigned dir=0;dir<2;dir++)if(ptr>=wifi_fence5_first.rings[dir] && ptr<wifi_fence5_first.rings[dir]+4096)return true;
 return false;
}
static bool wifi_fence5_command_event(const struct xhci_trb *e,uint64_t synchronous_pointer){
 if(wifi_fence5_previous_event(e)){wifi_fence5_bad_event(e);wifi_fence5_fault();return true;}
 if(!wifi_fence5_native.bound || wifi_fence5_native.retired || ((e->control>>10)&63)!=33)return false;
 uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);
 struct wifi_fence5_native_command *r=NULL;
 for(unsigned i=0;i<wifi_fence5_native.count;i++)if(wifi_fence5_native.commands[i].id.pointer==ptr)r=&wifi_fence5_native.commands[i];
 if(!r){
  /* Only the actual synchronous waiter may delegate its exact published
   * command. Shared consumers have no command-completion authority, even
   * when command_pending/sync_users happen to be nonzero. */
  if(!wifi_fence5_native.round && synchronous_pointer && ptr==synchronous_pointer && xhci.command_pending && xhci.sync_users)return false;
  wifi_fence5_bad_event(e);wifi_fence5_fault();return true;
 }
 unsigned index=(unsigned)((ptr-wifi_fence5_native.command_phys)/16);
 const uint32_t allowed=1u|(63u<<10)|(255u<<24);
 bool good=wifi_fence5_base() && wifi_fence5_native.round && xhci.command_pending && !r->latched && !r->consumed &&
  r==&wifi_fence5_native.commands[wifi_fence5_native.count-1] && index<63 && (r->id.type!=15 || r->stop_event) &&
  !(e->control&~allowed) && !(e->status&0xffffff) && (e->control>>24)==r->id.slot && (e->status>>24)==1 &&
  bytes_equal((const uint8_t *)&xhci.command_ring[index],(const uint8_t *)&r->shadow,sizeof r->shadow);
 /* Stop Transfer Event publication precedes command completion, but the
  * controller's final output-context write is guaranteed only before this
  * completion. Keep the raw event until that synchronization point. */
 if(good && r->id.type==15){
  __atomic_thread_fence(__ATOMIC_ACQUIRE);
  for(unsigned w=0;w<8;w++)r->stop_context[w]=context_read32(wifi_fence5_native.output,r->id.dci,w);
  uint64_t deq=r->stop_context[2]|((uint64_t)r->stop_context[3]<<32);
  uint64_t ptr=r->stop.parameter_low|((uint64_t)r->stop.parameter_high<<32);
  unsigned dir=r->id.dci==8?0:1;uint64_t ring=wifi_fence5_native.rings[dir];
  good=(r->stop_context[0]&7)==3 && !(deq&14) && (deq&~15ull)==ptr &&
   ((ptr==ring+16*wifi_fence5_native.positions && (deq&1)==wifi_fence5_native.dma->cycle[dir]) ||
    (ptr==ring && (deq&1)==(wifi_fence5_native.dma->cycle[dir]^1u)));
  if(good)r->stop_dequeue=deq;
 }
 if(!good){wifi_fence5_bad_event(e);wifi_fence5_fault();return true;}
 copy_trb(&r->event,e);r->cc=1;r->latched=true;xhci.command_events++;
 xhci.last_completion_code=1;xhci.last_slot_id=r->id.slot;xhci.last_command_pointer=ptr;xhci.last_expected_command_pointer=ptr;
 return true;
}
/* No TD is live at this bounded boundary. An observed Stop transfer event
 * is admissible only from the exact stopped output dequeue at Link63 or the
 * following invalid base with the independently applicable consumer cycle. */
static bool wifi_fence5_stop_event(const struct xhci_trb *e){
 if(wifi_fence5_previous_event(e)){wifi_fence5_bad_event(e);wifi_fence5_fault();return true;}
 if(!wifi_fence5_native.bound || wifi_fence5_native.retired || !wifi_fence5_native.round || ((e->control>>10)&63)!=32)return false;
 unsigned dci=(e->control>>16)&31,slot=e->control>>24;
 uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);bool ours=slot==wifi_fence5_native.slot && (dci==8 || (!wifi_fence5_native.noop && dci==9));
 for(unsigned dir=0;dir<(wifi_fence5_native.noop?1u:2u);dir++)if(ptr>=wifi_fence5_native.rings[dir] && ptr<wifi_fence5_native.rings[dir]+4096)ours=true;
 if(!ours)return false;
 bool good=wifi_fence5_base() && wifi_fence5_native.round && xhci.command_pending && wifi_fence5_native.count;
 struct wifi_fence5_native_command *r=good?&wifi_fence5_native.commands[wifi_fence5_native.count-1]:NULL;
 unsigned dir=dci==8?0:1;uint64_t ring=wifi_fence5_native.rings[dir];
 const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 good=good && r->id.type==15 && r->id.slot==slot && r->id.dci==dci && !r->latched && !r->consumed && !r->stop_event &&
  (e->status>>24)==27 && !(e->status&0xffffff) && !(e->control&~allowed) &&
  wifi_fence5_native.dma->cursor[dir]==wifi_fence5_native.positions && (ptr==ring+16*wifi_fence5_native.positions || ptr==ring);
 if(!good){wifi_fence5_bad_event(e);wifi_fence5_fault();}else {
  copy_trb(&r->stop,e);r->stop_event=true;
 }
 return true;
}
static enum mt76_fence5_result wifi_fence5_submit(void *c,const struct mt76_fence5_command *q,struct mt76_fence5_command *actual){
 if(!q || !actual || !wifi_fence5_owned(c,q->epoch,q->slot) || !wifi_fence5_native.round ||
  xhci.command_pending || wifi_fence5_native.count>=WIFI_FENCE5_COMMANDS || q->pointer || !q->token || q->token!=wifi_fence5_native.count+1 ||
  (q->dci!=8 && q->dci!=9) || (wifi_fence5_native.noop && q->dci!=8) || (q->type!=15 && q->type!=16))return MT76_FENCE5_FAILED;
 unsigned n=wifi_fence5_native.count,index=(unsigned)xhci.command_enqueue;
 if((q->type==15 && ((n&1) || q->dequeue || index+2+WIFI_FENCE5_CLEANUP_COMMANDS>63 || (n && !wifi_fence5_native.commands[n-1].consumed))) ||
  (q->type==16 && (!(n&1) || !n || !wifi_fence5_native.commands[n-1].consumed || wifi_fence5_native.commands[n-1].id.type!=15 || wifi_fence5_native.commands[n-1].id.dci!=q->dci || index+1+WIFI_FENCE5_CLEANUP_COMMANDS>63 ||
    q->dequeue!=(wifi_fence5_native.dma->ring[q->dci==8?0:1]|(wifi_fence5_native.dma->cycle[q->dci==8?0:1]^1u)))))return MT76_FENCE5_FAILED;
 for(unsigned i=0;i<n;i++)if(wifi_fence5_native.commands[i].id.pointer==xhci.command_ring_phys+16*index)return MT76_FENCE5_FAILED;
 struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[n];r->id=*q;r->id.pointer=xhci.command_ring_phys+16*index;
 r->shadow=(struct xhci_trb){.parameter_low=(uint32_t)q->dequeue,.parameter_high=q->dequeue>>32,.control=(q->type<<10)|(q->slot<<24)|(q->dci<<16)|(xhci.command_cycle?1u:0u)};
 wifi_fence5_native.count++;xhci.command_pending=true;
 struct xhci_trb *td=&xhci.command_ring[index];
 ((volatile struct xhci_trb *)td)->control=r->shadow.control^1u;
 __atomic_thread_fence(__ATOMIC_RELEASE);
 td->parameter_low=r->shadow.parameter_low;td->parameter_high=r->shadow.parameter_high;td->status=r->shadow.status;
 __atomic_thread_fence(__ATOMIC_RELEASE);
 ((volatile struct xhci_trb *)td)->control=r->shadow.control;xhci.command_enqueue++;
 __atomic_thread_fence(__ATOMIC_RELEASE);*actual=r->id;db_write32(0,0);
 return wifi_fence5_base()?MT76_FENCE5_READY:MT76_FENCE5_FAILED;
}
static enum mt76_fence5_result wifi_fence5_completion(void *c,struct mt76_fence5_completion *out){
 if(!out || !wifi_fence5_owned(c,wifi_fence5_native.epoch,wifi_fence5_native.slot) || !wifi_fence5_native.round || !wifi_fence5_native.count)return MT76_FENCE5_FAILED;
 struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];
 if(!xhci.command_pending || r->consumed)return MT76_FENCE5_FAILED;
 if(!r->latched)return MT76_FENCE5_PENDING;
 *out=(struct mt76_fence5_completion){.command=r->id,.cc=r->cc};r->consumed=true;xhci.command_pending=false;return MT76_FENCE5_READY;
}
static bool wifi_fence5_endpoint(void *c,unsigned slot,unsigned dci,struct mt76_fence5_context *out){
 if(!out || !wifi_fence5_owned(c,wifi_fence5_native.epoch,slot) || (dci!=8 && dci!=9) || xhci.command_pending)return false;
 uint8_t *p=wifi_fence5_native.output;
 __atomic_thread_fence(__ATOMIC_ACQUIRE);
 *out=(struct mt76_fence5_context){.epoch=wifi_fence5_native.epoch,.slot=slot,.dci=dci,.state=context_read32(p,dci,0)&7,.dequeue=context_read32(p,dci,2)|((uint64_t)context_read32(p,dci,3)<<32)};
 return wifi_fence5_base();
}
static bool wifi_fence5_frontier(void *c,struct mt76_fence5_frontier *out){
 if(!out || !wifi_fence5_owned(c,wifi_fence5_native.epoch,wifi_fence5_native.slot) || xhci.command_pending || wifi_fence5_native.frontier==UINT64_MAX)return false;
 struct xhci_trb e;*out=(struct mt76_fence5_frontier){.epoch=wifi_fence5_native.epoch,.sequence=++wifi_fence5_native.frontier};
 if(!poll_event(&e)){out->empty=true;return true;}
 out->scanned=1;uint64_t ptr=e.parameter_low|((uint64_t)e.parameter_high<<32);
 for(unsigned dir=0;dir<(wifi_fence5_native.noop?1u:2u);dir++)if(ptr>=wifi_fence5_native.dma->ring[dir] && ptr<wifi_fence5_native.dma->ring[dir]+4096)out->target_events++;
 if(((e.control>>10)&63)!=32 || ((e.control>>24)==wifi_fence5_native.slot && (((e.control>>16)&31)==8 || ((e.control>>16)&31)==9)))out->target_events++;
 if(out->target_events)return false;
 dispatch_transfer_event(&e);return wifi_fence5_base();
}
static bool wifi_fence5_previous_alias(uint64_t p){
 if(!p)return true;
 for(unsigned dir=0;dir<2;dir++){
  if(p==wifi_fence5_first.rings[dir])return true;
  for(unsigned i=0;i<4;i++)if(p==wifi_fence5_first.pages[dir][i])return true;
 }
 return false;
}
static uint64_t wifi_fence5_state_stamp(const void *p,size_t bytes){
 const uint8_t *v=p;uint64_t h=1469598103934665603ull;
 while(bytes--)h=(h^*v++)*1099511628211ull;
 return h;
}
/* All async command identities remain immutable and still occupy their original
 * command positions. This does not require the now-restored endpoint to retain
 * the former connection's live output context. */
static bool wifi_fence5_closed_records(const struct wifi_fence5_native_state *s){
 const struct mt76_fence5_io *io=&s->engine.io,*guard=&s->engine.io_guard;
 if(io->context!=s || guard->context!=s || io->owned!=wifi_fence5_owned || guard->owned!=wifi_fence5_owned ||
    io->submit!=wifi_fence5_submit || guard->submit!=wifi_fence5_submit ||
    io->completion!=wifi_fence5_completion || guard->completion!=wifi_fence5_completion ||
    io->endpoint!=wifi_fence5_endpoint || guard->endpoint!=wifi_fence5_endpoint ||
    io->frontier!=wifi_fence5_frontier || guard->frontier!=wifi_fence5_frontier)return false;
 if(s->count>WIFI_FENCE5_COMMANDS || (s->count&1) || s->engine.identity!=(uintptr_t)&s->engine ||
    s->engine.state!=MT76_FENCE5_IDLE || s->engine.busy || s->engine.dma!=s->dma || s->engine.epoch!=s->epoch ||
    s->engine.pointer_count!=s->count || s->engine.next_token!=s->count || s->engine.completed!=s->count/2)return false;
 for(unsigned i=0;i<s->count;i++){
  const struct wifi_fence5_native_command *r=&s->commands[i];uint64_t p=r->id.pointer;
  if(!r->latched || !r->consumed || r->cc!=1 || r->id.epoch!=s->epoch || r->id.token!=i+1 || r->id.slot!=s->slot ||
     (r->id.dci!=8 && r->id.dci!=9) || r->id.type!=((i&1)?16u:15u) ||
     p<s->command_phys || p>=s->command_phys+16*xhci.command_enqueue || (p-s->command_phys)%16 ||
     p!=s->engine.command_pointers[i] || (i && p<=s->commands[i-1].id.pointer))return false;
  unsigned index=(unsigned)((p-s->command_phys)/16);
  uint32_t control=(r->id.type<<10)|(r->id.slot<<24)|(r->id.dci<<16)|(s->cycle?1u:0u);
  uint64_t event=r->event.parameter_low|((uint64_t)r->event.parameter_high<<32);
  if(index>=63 || r->shadow.parameter_low!=(uint32_t)r->id.dequeue || r->shadow.parameter_high!=(uint32_t)(r->id.dequeue>>32) ||
     r->shadow.status || r->shadow.control!=control || event!=p || r->event.status!=(1u<<24) ||
     (r->event.control&~1u)!=((33u<<10)|(s->slot<<24)) ||
     !bytes_equal((const uint8_t *)&xhci.command_ring[index],(const uint8_t *)&r->shadow,sizeof r->shadow))return false;
  if(!(i&1)){
   uint64_t stop=r->stop.parameter_low|((uint64_t)r->stop.parameter_high<<32),dequeue=r->stop_context[2]|((uint64_t)r->stop_context[3]<<32);
   uint64_t ring=s->rings[r->id.dci==9];
   if(r->id.dequeue || !r->stop_event || r->stop.status!=(27u<<24) ||
      (r->stop.control&~1u)!=((32u<<10)|(r->id.dci<<16)|(s->slot<<24)) ||
      (r->stop_context[0]&7)!=3 || (dequeue&14) || r->stop_dequeue!=dequeue || (dequeue&~15ull)!=stop ||
      (stop!=ring && stop!=ring+16*s->positions))return false;
  }else if(r->id.dci!=s->commands[i-1].id.dci || (r->id.dequeue&14) ||
           (r->id.dequeue&~15ull)!=s->rings[r->id.dci==9])return false;
 }
 return true;
}
/* Root's native connection-close callback must independently prove exact Stop,
 * restored contexts/parent, erased keys and drained software borrowers. This
 * operation allocates nothing, never clears an old engine, and grants no DMA
 * page reuse or whole-backend retirement. Caller retains both context objects. */
static __attribute__((unused)) bool wifi_fence5_connection_next(void *old_owner,uint64_t old_epoch,struct mt76_dma5 *old_dma,
 void *next_owner,uint64_t next_epoch,bool (*closed)(void *,uint64_t,const struct mt76_dma5 *)){
 uint64_t f=event_irq_save();bool ok=false;struct wifi_fence5_native_state *s=&wifi_fence5_first;
 if(wifi_fence5_active!=s || wifi_fence5_connection_switched || !s->bound || s->retired || s->failed || s->round || s->busy || s->noop ||
    !old_owner || s->owner!=old_owner || s->epoch!=old_epoch || !old_epoch || !old_dma || s->dma!=old_dma ||
    old_dma->identity!=(uintptr_t)old_dma || old_dma->epoch!=old_epoch || old_dma->state!=MT76_DMA5_RETIRED || old_dma->positions!=s->positions ||
    !next_owner || next_owner==old_owner || next_epoch<=old_epoch || !closed ||
    !xhci.operational || !xhci.controller_owned || xhci.stopping || xhci.stopped || xhci.reclaiming || xhci.reclaimed ||
    xhci.command_pending || xhci.command_failed || xhci.sync_users || xhci.poll_users ||
    xhci.command_ring!=s->command_ring || xhci.command_ring_phys!=s->command_phys || xhci.command_cycle!=s->cycle ||
    s->first>=63 || xhci.command_enqueue>=63 || xhci.command_enqueue<s->first || xhci.command_enqueue+2+WIFI_FENCE5_CLEANUP_COMMANDS>63 ||
    !wifi_fence5_closed_records(s) || !wifi_fence5_empty())goto done;
 for(size_t i=0;i<sizeof wifi_fence5_second;i++)if(((const uint8_t *)&wifi_fence5_second)[i])goto done;
 uint64_t stamp=wifi_fence5_state_stamp(s,sizeof *s),dma_stamp=wifi_fence5_state_stamp(old_dma,sizeof *old_dma),frontier=xhci.command_enqueue;
 if(!closed(old_owner,old_epoch,old_dma) || stamp!=wifi_fence5_state_stamp(s,sizeof *s) ||
    dma_stamp!=wifi_fence5_state_stamp(old_dma,sizeof *old_dma) || wifi_fence5_active!=s || wifi_fence5_connection_switched ||
    xhci.command_ring!=s->command_ring || xhci.command_ring_phys!=s->command_phys || xhci.command_cycle!=s->cycle ||
    xhci.command_enqueue!=frontier || xhci.command_pending || xhci.command_failed || xhci.sync_users || xhci.poll_users || !wifi_fence5_empty())goto done;
 wifi_fence5_previous_frontier=(unsigned)frontier;wifi_fence5_next_owner=next_owner;wifi_fence5_next_epoch=next_epoch;
 wifi_fence5_connection_switched=true;wifi_fence5_active=&wifi_fence5_second;ok=true;
 done:event_irq_restore(f);return ok;
}
static __attribute__((unused)) bool wifi_fence5_bind(struct xhci_device *d,struct mt76_dma5 *dma,uint64_t epoch,void *owner,bool (*owned)(void *,uint64_t,unsigned)){
 uint64_t f=event_irq_save();bool ok=false;
 if(wifi_fence5_native.bound || wifi_fence5_native.failed ||
  (wifi_fence5_connection_switched && (owner!=wifi_fence5_next_owner || epoch!=wifi_fence5_next_epoch || d!=wifi_fence5_first.device || dma==wifi_fence5_first.dma || !wifi_fence5_sync_admitted())) ||
  !d || !dma || !owner || !owned || !epoch || dma->identity!=(uintptr_t)dma || dma->epoch!=epoch || dma->state!=MT76_DMA5_RUNNING || (dma->positions!=63 && (dma->positions!=127 || dma->noop_only)) ||
  xhci.command_pending || xhci.command_failed || xhci.sync_users || !xhci.command_ring || !xhci.command_ring_phys || xhci.command_enqueue>=63 || (xhci.command_ring_phys&4095) || xhci.command_enqueue+2+WIFI_FENCE5_CLEANUP_COMMANDS>63 || !wifi_fence5_empty())goto done;
 bool listed=false;for(unsigned i=0;i<XHCI_MAX_DEVICES;i++)if(d==&xhci.devices[i])listed=true;if(!listed || !d->output_context || !d->output_context_phys || (d->output_context_phys&4095))goto done;
 for(unsigned dir=0;dir<2;dir++){
  uint64_t ring=dma->ring[dir];unsigned dci=dir?9:8;
  if(dma->noop_only && dir){if(ring)goto done;continue;}
  if(!dma->noop_only && dma->ring[0]==dma->ring[1])goto done;
  if((context_read32(d->output_context,dci,0)&7)!=1 && (context_read32(d->output_context,dci,0)&7)!=3)goto done;
  if(wifi_fence5_connection_switched && wifi_fence5_previous_alias(ring))goto done;
  if(!ring || (ring&4095) || ring==xhci.command_ring_phys || ring==xhci.event_ring_phys || ring==d->input_context_phys || ring==d->output_context_phys ||
   ((context_read32(d->output_context,dci,1)>>3)&7)!=(dir?6u:2u) || (context_read32(d->output_context,dci,1)>>16)!=512)goto done;
  uint64_t deq=context_read32(d->output_context,dci,2)|((uint64_t)context_read32(d->output_context,dci,3)<<32);
  if((deq&14) || (deq&~15ull)<ring || (deq&~15ull)>ring+dma->positions*16)goto done;
  for(unsigned i=0;i<4;i++)if(dma->pages[dir][i].state!=MT76_DMA5_FREE)goto done;
 }
 for(unsigned dir=0;dir<2;dir++)for(unsigned i=0;i<4;i++){
  uint64_t page=dma->pages[dir][i].lease.page;
  if(dma->pages[dir][i].state!=MT76_DMA5_FREE)goto done;
  if(dma->noop_only){if(page)goto done;continue;}
  if(wifi_fence5_connection_switched && wifi_fence5_previous_alias(page))goto done;
  if(!page || (page&4095) || page==dma->ring[0] || page==dma->ring[1] || page==xhci.command_ring_phys || page==xhci.event_ring_phys || page==d->input_context_phys || page==d->output_context_phys)goto done;
  for(unsigned prior=0;prior<dir*4+i;prior++)if(page==dma->pages[prior/4][prior%4].lease.page)goto done;
 }
 wifi_fence5_native.noop=dma->noop_only;wifi_fence5_native.positions=dma->positions;
 for(unsigned dir=0;dir<2;dir++){wifi_fence5_native.rings[dir]=dma->ring[dir];for(unsigned i=0;i<4;i++)wifi_fence5_native.pages[dir][i]=dma->pages[dir][i].lease.page;}
 if(dma->noop_only){const unsigned indices[4]={0,9,11,16};for(unsigned i=0;i<4;i++)for(unsigned w=0;w<8;w++)wifi_fence5_native.unrelated[i][w]=context_read32(d->output_context,indices[i],w);}else{
  if(!d->input_context || !d->input_context_phys || (d->input_context_phys&4095))goto done;
  wifi_fence5_native.input=d->input_context;wifi_fence5_native.input_phys=d->input_context_phys;
  const unsigned indices[3]={0,11,16};for(unsigned i=0;i<3;i++)for(unsigned w=0;w<8;w++)wifi_fence5_native.normal_parent[i][w]=context_read32(d->output_context,indices[i],w);
  for(unsigned dir=0;dir<2;dir++)for(unsigned w=0;w<8;w++)wifi_fence5_native.normal_config[dir][w]=context_read32(d->output_context,8+dir,w);
 }
 wifi_fence5_native.bound=true;wifi_fence5_native.device=d;wifi_fence5_native.dma=dma;wifi_fence5_native.epoch=epoch;wifi_fence5_native.slot=d->slot_id;
 wifi_fence5_native.owner=owner;wifi_fence5_native.owned=owned;wifi_fence5_native.command_ring=xhci.command_ring;wifi_fence5_native.command_phys=xhci.command_ring_phys;
 wifi_fence5_native.cycle=xhci.command_cycle;wifi_fence5_native.first=(unsigned)xhci.command_enqueue;wifi_fence5_native.output=d->output_context;wifi_fence5_native.output_phys=d->output_context_phys;
 const unsigned dci[2]={8,9};const struct mt76_fence5_io io={&wifi_fence5_native,wifi_fence5_owned,wifi_fence5_submit,wifi_fence5_completion,wifi_fence5_endpoint,wifi_fence5_frontier};
 uint64_t now=timer_monotonic_us();wifi_fence5_native.last_us=now;
 ok=wifi_fence5_owned(&wifi_fence5_native,epoch,d->slot_id) && mt76_fence5_init(&wifi_fence5_native.engine,dma,epoch,d->slot_id,dci,&io,now);
 if(!ok)wifi_fence5_fault();
 done:event_irq_restore(f);return ok;
}
/* An unrelated completion can arrive after the caller's bounded dispatch,
 * including while CPU IRQs are masked. Before any fence command is published,
 * a nonempty shared event frontier is backpressure, not lost DMA ownership.
 * The caller dispatches and retries inside its ORIGINAL bounded window. */
static __attribute__((unused)) enum mt76_fence5_result wifi_fence5_try_begin(unsigned direction){
 uint64_t f=event_irq_save(),now=timer_monotonic_us();bool ok=false;
 enum mt76_fence5_result result=MT76_FENCE5_FAILED;
 if(direction>1 || (wifi_fence5_native.noop && direction) || wifi_fence5_native.round || !wifi_fence5_owned(&wifi_fence5_native,wifi_fence5_native.epoch,wifi_fence5_native.slot) || xhci.command_pending ||
  xhci.command_enqueue+2+WIFI_FENCE5_CLEANUP_COMMANDS>63)goto done;
 if(!wifi_fence5_native.noop){
  if(direction>1)goto done;
  for(unsigned dir=0;dir<2;dir++)for(unsigned i=0;i<4;i++)if(wifi_fence5_native.dma->pages[dir][i].state!=MT76_DMA5_FREE)goto done;
 }
 if(!wifi_fence5_empty()){result=MT76_FENCE5_PENDING;goto done;}
 if(!wifi_fence5_native.noop){
  unsigned other=direction^1u;
  __atomic_thread_fence(__ATOMIC_ACQUIRE);
  for(unsigned w=0;w<8;w++)wifi_fence5_native.opposite[w]=context_read32(wifi_fence5_native.output,8+other,w);
  wifi_fence5_native.direction=direction;wifi_fence5_native.opposite_valid=true;
 }
 ok=mt76_fence5_begin(&wifi_fence5_native.engine,direction,now);
 if(ok){wifi_fence5_native.round=true;wifi_fence5_native.last_us=now;}
 result=ok?MT76_FENCE5_READY:MT76_FENCE5_FAILED;
 done:if(result==MT76_FENCE5_FAILED && wifi_fence5_native.bound)wifi_fence5_fault();
 event_irq_restore(f);return result;
}
static __attribute__((unused)) bool wifi_fence5_begin(unsigned direction){return wifi_fence5_try_begin(direction)==MT76_FENCE5_READY;}
static __attribute__((unused)) bool wifi_fence5_step(void){
 uint64_t f=event_irq_save(),now=timer_monotonic_us();bool ok=wifi_fence5_native.round && !wifi_fence5_native.busy;
 if(ok){wifi_fence5_native.busy=true;ok=now>=wifi_fence5_native.last_us && mt76_fence5_step(&wifi_fence5_native.engine,now);wifi_fence5_native.last_us=now;wifi_fence5_native.busy=false;}
 if(ok && wifi_fence5_native.engine.state==MT76_FENCE5_IDLE)wifi_fence5_native.round=false;
 if(!ok && wifi_fence5_native.bound)wifi_fence5_fault();
 event_irq_restore(f);return ok;
}
static void wifi_fence5_stopping(void){
 const struct wifi_fence5_native_state *s=wifi_fence5_connection_switched?&wifi_fence5_first:NULL;
 if((wifi_fence5_native.bound && !wifi_fence5_native.retired && (wifi_fence5_native.round || xhci.command_pending || wifi_fence5_native.dma->state!=MT76_DMA5_RETIRED)) ||
    (s && s->bound && !s->retired && (s->round || s->busy || xhci.command_pending || s->dma->state!=MT76_DMA5_RETIRED)))wifi_fence5_fault();
}
static bool wifi_fence5_retire(void);
static bool wifi_fence5_reclaim_ready(void){
 bool current=!wifi_fence5_native.bound || wifi_fence5_native.retired;
 bool previous=!wifi_fence5_connection_switched || !wifi_fence5_first.bound || wifi_fence5_first.retired;
 return (current && previous) || wifi_fence5_retire();
}
static bool wifi_fence5_retirement_ready(const struct wifi_fence5_native_state *s){
 return !s->bound || s->retired || (!s->failed && !s->busy && !s->round && s->dma->state==MT76_DMA5_RETIRED);
}
static __attribute__((unused)) bool wifi_fence5_retire(void){
 uint64_t f=event_irq_save();bool any=wifi_fence5_native.bound || (wifi_fence5_connection_switched && wifi_fence5_first.bound);
 bool ok=any && wifi_fence5_retirement_ready(wifi_fence5_active) &&
  (!wifi_fence5_connection_switched || wifi_fence5_retirement_ready(&wifi_fence5_first)) &&
  xhci.stopped && !xhci.sync_users && !xhci.poll_users &&
  !(op_read32(XHCI_OP_USBCMD)&(XHCI_USBCMD_RUN|XHCI_USBCMD_INTE|XHCI_USBCMD_HCRST)) &&
  (op_read32(XHCI_OP_USBSTS)&(XHCI_USBSTS_HCH|XHCI_USBSTS_CNR))==XHCI_USBSTS_HCH && wifi_fence5_empty();
 if(ok){if(wifi_fence5_native.bound)wifi_fence5_native.retired=true;if(wifi_fence5_connection_switched && wifi_fence5_first.bound)wifi_fence5_first.retired=true;}
 event_irq_restore(f);return ok;
}
