#include <srvros/packet_observation.h>
#include <srvros/dhcp_radio5.h>
/* Explicit v135:independent live owner,never widens historical WPA/data bounds.
 * One original handshake/key lifetime;four pages/direction;finite real fences.
 * All DMA/context survives uncertainty. No console inside active MAC window. */
#include <srvros/mt76_stream5_probe.h>
#include <srvros/mt76_net5.h>
static enum mt76_join5_send wifi_stack5_window(void *,struct wpa2_ccmp *,uint64_t,const struct mt76_peer5 *);
static bool wifi_stack5_complete(void);
static void wifi_stack5_emit(void);
static bool wifi_http5_failed_connection_closed(void);
static bool wifi_http5_data_radio_ready(void);
#define WIFI_STREAM5_RX 252u
#define WIFI_STREAM5_TX 80u
_Static_assert(WIFI_STREAM5_TX==DHCP_RADIO5_TX_LIMIT,"radio allowance must track original publication bound");
#define WIFI_STREAM5_FENCES 4u
/* DATA is not a mt76_wpa5_tx member; the RX enum uses a different value. */
#define WIFI_STREAM5_DATA_TX 5u
/* Anonymous scalar observations only; never retain packet or key bytes. */
struct wifi_stream5_record {struct packet_observation packet;unsigned sequence;struct xhci_trb event;uint64_t ticket,token,pn;uint64_t started_us,finished_us;unsigned flags,frame,audit,span,dma,mpdu,trailing,ethertype,rate,ack,wcid;unsigned direction,page,cycle,bytes,ethernet,reason,kind;};
struct wifi_stream5_state {
 struct dhcp_radio5 data_radio;
 struct packet_observation tx_packet;
 struct xhci_device *d;struct mt76_peer5 peer;struct mt76_wpa5 exchange;struct mt76_mac_cycle mac;
 struct mt76_dma5 dma;struct mt76_dma5_lease lease[2],loan;
 struct mt76_data5_scratch scratch;struct mt76_stream5_probe probe;
 struct xhci_trb *ring[2],shadow[2][128];uint64_t physical[2],pages[2][4],enqueue[2];uintptr_t pointers[10];bool cycle[2];
 uint8_t *buffer[2][4],page_shadow[2][4][4096],input[4096],expected[4096],pmk[32],usb[MT76_DATA5_USB_MAX],cached[4096];
 uint8_t parent[sizeof wifi_join5],cal[sizeof wifi_cal5],power[sizeof wifi_power5],handoff[sizeof wifi_handoff5],binding[sizeof wifi_peer5],preview[sizeof wifi_tx5];
 uint8_t parent_ring[2][4096],parent_pages[WIFI_JOIN5_RX+3][4096],old_ring[3][4096],old_pages[8][4096];
 uint32_t context[5][8];uint64_t fixed[9],stop_pointer[2],input_phys,output_phys,epoch,data_pn,last_us;uintptr_t input_pointer,output_pointer;
 struct {uint64_t pointer,event_pointer,dequeue,delta;unsigned type,dci,slot,cc;} sync[6];
 struct xhci_trb fault_event;bool fault_event_valid;unsigned fault_phase;
 struct {unsigned present,peer,ok,invalid,rejected,clocks;uint64_t first,last;bool rollback;} auth_observation;
 /* Read-to-pop radio status is distinct from USB completion. Phase0 drains
  * older traffic before AUTH; phase1 follows AUTH response/timeout. An empty
  * FIFO is not RF failure evidence. No status authorizes protocol progress. */
 struct {uint32_t ext[2][17],fifo[2][17];unsigned count[2];bool attempted[2],empty[2],complete[2];} auth_radio;
 struct xhci_trb stops[2];uint32_t snapshots[10][2][8];
 struct wifi_stream5_record records[WIFI_STREAM5_RX+WIFI_STREAM5_TX];unsigned record_count,rx_record;
 uint64_t posts[2],completions[2];unsigned tx_kind;unsigned slot;unsigned wraps[2],phase,commands,stop_dci,stop_events[2],rx_residue,cached_type,cached_size,data_sends,data_received,data_after_txwrap,data_after_rxwrap,drained;
 bool connection_mode,connection_closed,stack_mode,discover_retry,attempted,active,failed,toggle_synced,pending[2],stop_was_pending[2],ready_rx,cached_present,data_active,closed,restored,retired,events_empty,parent_complete,complete;
};
/* A connection is never reset/reused. The bounded reconnect owner may select
 * one fresh second context after the first connection's exact close proof. */
static struct wifi_stream5_state wifi_stream5_slots[2];
static struct wifi_stream5_state *wifi_stream5_current=&wifi_stream5_slots[0];
#define wifi_stream5 (*wifi_stream5_current)
/* The two-connection recipe retains its two-fence command reserve. With127
 * positions and unchanged TX80/RX252 bounds, TX does not wrap and RX can
 * cross at most one boundary. Excess traffic refuses without command reuse. */
/* Only the two-connection recipe uses half of the existing 4KiB ring page.
 * Historical owners retain Link63. Command-ring geometry is unchanged. */
static unsigned wifi_stream5_positions(void){return wifi_stream5.connection_mode?127u:63u;}
static void wifi_stream5_link(struct xhci_trb *ring,uint64_t phys,bool cycle){
 ring[wifi_stream5_positions()]=(struct xhci_trb){.parameter_low=(uint32_t)phys,.parameter_high=phys>>32,
  .control=(XHCI_TRB_TYPE_LINK<<10)|(1u<<1)|(cycle?1u:0u)};
}
static uint64_t wifi_stream5_push(unsigned dir,const struct xhci_trb *source){
 unsigned index=(unsigned)wifi_stream5.enqueue[dir];
 if(index>=wifi_stream5_positions())return 0;
 struct xhci_trb trb=*source;trb.control=(trb.control&~1u)|(wifi_stream5.cycle[dir]?1u:0u);
 copy_trb(&wifi_stream5.ring[dir][index],&trb);
 wifi_stream5.enqueue[dir]++;
 if(wifi_stream5.enqueue[dir]==wifi_stream5_positions()){
  wifi_stream5_link(wifi_stream5.ring[dir],wifi_stream5.physical[dir],wifi_stream5.cycle[dir]);
  wifi_stream5.enqueue[dir]=0;wifi_stream5.cycle[dir]=!wifi_stream5.cycle[dir];
 }
 return wifi_stream5.physical[dir]+16*index;
}
static unsigned wifi_stream5_fence_limit(void){return wifi_stream5.connection_mode?2u:WIFI_STREAM5_FENCES;}

static bool wifi_stream5_delay_us(unsigned us){
#ifdef SRVROS_XHCI_STREAM5_TEST
 return stream_test_delay(us);
#else
 return timer_delay_us(us);
#endif
}
static bool wifi_stream5_entropy(uint8_t out[32]){
#ifdef SRVROS_XHCI_STREAM5_TEST
 return stream_test_nonce(out);
#else
 return wpa2_nonce_generate(out);
#endif
}
static bool wifi_stream5_parent(void){
#ifdef SRVROS_XHCI_STREAM5_TEST
 return stream_test_parent();
#else
 return wifi_join5.restored && wifi_join5.phase==9 && wifi_join5.active && wifi_eapol5.complete && wifi_join5.exchange.complete && wifi_join5_owned(&wifi_join5,true) && wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) && wifi_tx5_owned();
#endif
}
static bool wifi_stream5_fail(void){if(!wifi_stream5.failed)wifi_stream5.fault_phase=wifi_stream5.phase;wifi_stream5.failed=true;mt76_dma5_fault(&wifi_stream5.dma);return wifi_stream5.d?wifi_mcu_fail(wifi_stream5.d):false;}
static bool wifi_stream5_base(void){
 struct xhci_device *d=wifi_stream5.d;if(!wifi_stream5.active || wifi_stream5.failed || !d || d!=wifi_cal5.d || !d->present || d->removed || d->retiring || d->slot_disabled || d->ep0_quarantined || d->control_failed || !xhci.operational || !xhci.controller_owned || xhci.stopping || xhci.reclaiming || xhci.reclaimed || xhci.command_failed || firmware_command_failed || d->slot_id!=wifi_stream5.slot || d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 ||
 wifi_stream5.epoch!=wifi_stream5.peer.generation || wifi_stream5.dma.identity!=(uintptr_t)&wifi_stream5.dma || wifi_stream5.dma.epoch!=wifi_stream5.epoch || wifi_stream5.dma.positions!=wifi_stream5_positions() || wifi_stream5.dma.state!=MT76_DMA5_RUNNING || d->input_context_phys!=wifi_stream5.input_phys || d->output_context_phys!=wifi_stream5.output_phys || (uintptr_t)d->input_context!=wifi_stream5.input_pointer || (uintptr_t)d->output_context!=wifi_stream5.output_pointer || !wifi_handoff5_equal(d->input_context,wifi_stream5.expected,4096))return false;
 const uint64_t fixed[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};
 for(unsigned i=0;i<9;i++)if(fixed[i]!=wifi_stream5.fixed[i])return false;
 if(!wifi_cal5_cleanup_held() || !wifi_handoff5_equal(&wifi_join5,wifi_stream5.parent,sizeof wifi_join5) || !wifi_handoff5_equal(&wifi_cal5,wifi_stream5.cal,sizeof wifi_cal5) || !wifi_handoff5_equal(&wifi_power5,wifi_stream5.power,sizeof wifi_power5) || !wifi_handoff5_equal(&wifi_handoff5,wifi_stream5.handoff,sizeof wifi_handoff5) || !wifi_handoff5_equal(&wifi_peer5,wifi_stream5.binding,sizeof wifi_peer5) || !wifi_handoff5_equal(&wifi_tx5,wifi_stream5.preview,sizeof wifi_tx5))return false;
 if(!wifi_handoff5_equal(wifi_join5.rx,wifi_stream5.parent_ring[0],4096) || !wifi_handoff5_equal(wifi_join5.tx,wifi_stream5.parent_ring[1],4096))return false;
 for(unsigned i=0;i<WIFI_JOIN5_RX+3;i++)if(!wifi_handoff5_equal(wifi_join5.buffer[i],wifi_stream5.parent_pages[i],4096))return false;
 const void *old[]={wifi_cal5.ep.ring,wifi_handoff5.ring,wifi_tx5.ring};for(unsigned i=0;i<3;i++)if(!wifi_handoff5_equal(old[i],wifi_stream5.old_ring[i],4096))return false;
 for(unsigned i=0;i<8;i++)if(!wifi_handoff5_equal(wifi_cal5.plan.buffer[i],wifi_stream5.old_pages[i],4096))return false;
 const unsigned dcis[]={0,11,16};for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)if(context_read32(d->output_context,dcis[i],j)!=wifi_stream5.context[i][j])return false;
 for(unsigned dir=0;dir<2;dir++){
  if(!wifi_stream5.physical[dir] || (uintptr_t)wifi_stream5.ring[dir]!=wifi_stream5.pointers[dir] || wifi_stream5.ring[dir]!=pmm_phys_to_virt(wifi_stream5.physical[dir]) || wifi_stream5.dma.ring[dir]!=wifi_stream5.physical[dir] || !wifi_handoff5_equal(wifi_stream5.ring[dir],wifi_stream5.shadow[dir],16*(wifi_stream5_positions()+1)))return false;
  for(unsigned p=0;p<4;p++){const struct mt76_dma5_page *l=&wifi_stream5.dma.pages[dir][p];if(l->lease.page!=wifi_stream5.pages[dir][p] || (uintptr_t)wifi_stream5.buffer[dir][p]!=wifi_stream5.pointers[2+dir*4+p] || wifi_stream5.buffer[dir][p]!=pmm_phys_to_virt(l->lease.page))return false;
   if((!dir || l->state==MT76_DMA5_FREE) && !wifi_handoff5_equal(wifi_stream5.buffer[dir][p],wifi_stream5.page_shadow[dir][p],4096))return false;}
 }
 return true;
}
static bool wifi_stream5_context(void){
 struct xhci_device *d=wifi_stream5.d;
 for(unsigned dir=0;dir<2;dir++){unsigned dci=dir?9:8;bool fresh=dir?(wifi_stream5.phase>=2 && wifi_stream5.phase<6):(wifi_stream5.phase>=1 && wifi_stream5.phase<7);
  for(unsigned j=0;j<8;j++){uint32_t actual=context_read32(d->output_context,dci,j);
   if(!fresh){if(!dir && wifi_stream5.phase>=7){if(!j && (actual&7))return false;}else if(actual!=wifi_stream5.context[dir?4:3][j])return false;continue;}
   if(!j){unsigned state=actual&7;if(state!=1 && state!=3)return false;if((actual&~7u)!=(dir?(wifi_stream5.context[4][0]&~7u):0))return false;}
   else if(j==2 || j==3){uint64_t p=context_read32(d->output_context,dci,2)|((uint64_t)context_read32(d->output_context,dci,3)<<32);if((p&14) || (p&~15ull)<wifi_stream5.physical[dir] || (p&~15ull)>wifi_stream5.physical[dir]+wifi_stream5_positions()*16)return false;}
   else {uint32_t wanted=dir?wifi_stream5.context[4][j]:j==1?((3u<<1)|(2u<<3)|(512u<<16)):j==4?MT76_DATA5_USB_MAX:0;if(actual!=wanted)return false;}
  }
 }
 return true;
}
static bool wifi_stream5_owned(void *c,bool cleanup){
 (void)cleanup;bool good=c==&wifi_stream5 && wifi_stream5_base() && wifi_stream5_context() && !xhci.sync_users && !wifi_stream5.d->control_pending && !wifi_stream5.d->firmware.pending && !wifi_stream5.d->firmware.mcu.pending && !strict_control.device;
 if(good && xhci.command_pending)good=wifi_fence5_native.bound && wifi_fence5_native.round && !wifi_fence5_native.failed && wifi_fence5_native.dma==&wifi_stream5.dma;
 return good;
}
static bool wifi_stream5_fence_owned(void *c,uint64_t epoch,unsigned slot){return c==&wifi_stream5 && epoch==wifi_stream5.epoch && slot==wifi_stream5.slot && wifi_stream5_owned(c,false);}
static bool wifi_stream5_event(struct xhci_device *d,const struct xhci_trb *e){
 if(!wifi_stream5.active)return false;
 unsigned dci=(e->control>>16)&31;uint64_t p=e->parameter_low|((uint64_t)e->parameter_high<<32);bool ours=d==wifi_stream5.d && (dci==8 || dci==9);
 if(!ours){for(unsigned dir=0;dir<2;dir++)if(p>=wifi_stream5.physical[dir] && p<wifi_stream5.physical[dir]+4096){if(!wifi_stream5.fault_event_valid){wifi_stream5.fault_event=*e;wifi_stream5.fault_event_valid=true;}wifi_stream5_fail();return true;}return false;}
 unsigned dir=dci==9,cc=e->status>>24,res=e->status&0xffffff;const uint32_t allowed=1u|(63u<<10)|(31u<<16)|(255u<<24);
 bool good=wifi_stream5_base() && wifi_stream5_context() && ((e->control>>10)&63)==32 && (e->control>>24)==wifi_stream5.slot && !(e->control&~allowed);
 if(good && cc>=26 && cc<=28){uint64_t next=wifi_stream5.physical[dir]+16*wifi_stream5.enqueue[dir];
  /* The Stop transfer event precedes command completion; the final output
   * context need not be visible yet. Latch only exact raw TD identity here. */
  good=firmware_command_exact && wifi_stream5.stop_dci==dci && !wifi_stream5.stop_events[dir] &&
   ((cc==27 && !res && !wifi_stream5.pending[dir] && p==next) || (dir && wifi_stream5.pending[dir] && cc!=27 && p==wifi_stream5.lease[dir].trb && res<=4096));
  if(good){wifi_stream5.stop_events[dir]++;wifi_stream5.stop_was_pending[dir]=wifi_stream5.pending[dir];wifi_stream5.stops[dir]=*e;wifi_stream5.pending[dir]=false;}
 }else if(good){struct mt76_dma5_lease *l=&wifi_stream5.lease[dir];unsigned index=(unsigned)((l->trb-wifi_stream5.physical[dir])/16);
  good=wifi_stream5.phase>=3 && wifi_stream5.phase<=4 && wifi_stream5.pending[dir] && index<wifi_stream5_positions() && p==l->trb &&
   (dir?((cc==1 || cc==13) && res<=4096):(cc==1 && !res)) && wifi_stream5.shadow[dir][index].parameter_low==(uint32_t)l->page && wifi_stream5.shadow[dir][index].parameter_high==(uint32_t)(l->page>>32) && wifi_stream5.shadow[dir][index].status==l->length && wifi_stream5.shadow[dir][index].control==((XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|(dir?XHCI_TRB_ISP:0)|l->cycle);
  uint64_t finished=good?timer_monotonic_us():0;
  if(good)good=mt76_dma5_complete(&wifi_stream5.dma,l,cc,res,finished);
  if(good){uint64_t started=wifi_stream5.dma.pages[dir][l->index].started;unsigned i=wifi_stream5.record_count;if(i>=WIFI_STREAM5_RX+WIFI_STREAM5_TX)good=false;else{
    wifi_stream5.records[i]=(struct wifi_stream5_record){.started_us=started,.finished_us=finished,.event=*e,.ticket=l->ticket,.token=l->token,.pn=wifi_stream5.data_pn,.direction=dir,.kind=dir?0:wifi_stream5.tx_kind,.page=l->index,.cycle=l->cycle,.bytes=l->length-res};wifi_stream5.record_count++;
    if(!dir && wifi_stream5.tx_kind==WIFI_STREAM5_DATA_TX && l->length>=24){
     const uint8_t *b=wifi_stream5.buffer[0][l->index];struct wifi_stream5_record *r=&wifi_stream5.records[i];
     r->rate=b[6]|((unsigned)b[7]<<8);r->ack=b[8]&1;r->wcid=b[9];r->mpdu=b[10]|((unsigned)b[11]<<8);r->packet=wifi_stream5.tx_packet;
     if(l->length>=48)r->sequence=(b[46]|((unsigned)b[47]<<8))>>4;
     dhcp_radio5_complete(&wifi_stream5.data_radio,b[23],r->packet.message,r->sequence,l->ticket,l->token,finished);
    }
    wifi_stream5.pending[dir]=false;wifi_stream5.completions[dir]++;if(dir){wifi_stream5.ready_rx=true;wifi_stream5.rx_residue=res;wifi_stream5.rx_record=i;}else copy_bytes(wifi_stream5.page_shadow[0][l->index],wifi_stream5.buffer[0][l->index],4096);
   }}
 }
 if(!good){if(!wifi_stream5.fault_event_valid){wifi_stream5.fault_event=*e;wifi_stream5.fault_event_valid=true;}wifi_stream5_fail();}
 return true;
}
static bool wifi_stream5_dispatch(void){
#ifdef SRVROS_XHCI_STREAM5_TEST
#ifdef SRVROS_XHCI_STACK5_TEST
 if(wifi_stream5.stack_mode)stack_test_progress();else
#endif
 stream_test_progress();
#endif
 uint64_t f=event_irq_save();struct xhci_trb e;bool good=true;for(unsigned i=0;i<32 && poll_event(&e);i++){unsigned t=(e.control>>10)&63;if(t!=32 && t!=33){good=false;break;}dispatch_transfer_event(&e);if(wifi_stream5.failed || (t==33 && !wifi_fence5_native.round) || wifi_fence5_native.failed){good=false;break;}}event_irq_restore(f);return good?true:wifi_stream5_fail();}
static enum mt76_wpa5_rx wifi_stream5_type(void){return wifi_stream5.data_active?MT76_WPA5_DATA:wifi_stream5.exchange.assoc_matched?MT76_WPA5_EAPOL:MT76_WPA5_MANAGEMENT;}
static bool wifi_stream5_collect(enum mt76_wpa5_rx type,bool drain){
 if(!wifi_stream5.ready_rx)return true;
 if(wifi_stream5.cached_present && !drain)return true;
 uint64_t f=event_irq_save();bool good=mt76_dma5_acquire(&wifi_stream5.dma,wifi_stream5.epoch,&wifi_stream5.loan);struct mt76_dma5_lease *l=&wifi_stream5.loan;
 if(good){unsigned p=l->index;size_t bytes=l->received,n=0;copy_bytes(wifi_stream5.page_shadow[1][p],wifi_stream5.buffer[1][p],4096);
  if(!drain){
   if(type==MT76_WPA5_DATA){if(!wifi_stream5.data_active || wifi_stream5.exchange.keys.phase!=WPA2_CCMP_ACTIVE || wifi_stream5.exchange.keys.generation!=wifi_stream5.epoch)good=false;
    else {struct mt76_data5_rx_result r=mt76_data5_rx_report(&wifi_stream5.scratch,&wifi_stream5.exchange.keys,wifi_stream5.epoch,wifi_stream5.buffer[1][p],bytes,wifi_stream5.cached,MT76_DATA5_ETH_MAX);n=r.bytes;wifi_stream5.records[wifi_stream5.rx_record].reason=r.reason;wifi_stream5.records[wifi_stream5.rx_record].ethernet=(unsigned)n;
     struct wifi_stream5_record *record=&wifi_stream5.records[wifi_stream5.rx_record];
     record->flags=r.flags;record->frame=r.frame;record->audit=r.audit;
     if(n>=14){record->packet=packet_observe(wifi_stream5.cached,n);record->sequence=(wifi_stream5.buffer[1][p][58]|((unsigned)wifi_stream5.buffer[1][p][59]<<8))>>4;}
     if(n>=14)record->ethertype=((unsigned)wifi_stream5.cached[12]<<8)|wifi_stream5.cached[13];
     if(bytes>=36){const uint8_t *b=wifi_stream5.buffer[1][p];unsigned dma=b[0]|((unsigned)b[1]<<8),mpdu=(b[10]|((unsigned)b[11]<<8))&0x3fff;
      if(dma>=32 && !(dma&3) && dma<=bytes-4 && mpdu>=2 && mpdu<=dma-32){record->span=1;record->dma=dma;record->mpdu=mpdu;record->trailing=(unsigned)bytes-4-dma;}
     }

     if(r.reason==MT76_DATA5_ADMISSION)good=false;
     if(n){wifi_stream5.data_received++;if(wifi_stream5.wraps[1])wifi_stream5.data_after_rxwrap++;}}
   }else if(type==MT76_WPA5_EAPOL){struct mt76_eapol5_peer peer={.channel=161,.bandwidth=20};copy_bytes(peer.station,wifi_stream5.peer.peer.station,6);copy_bytes(peer.bssid,wifi_stream5.peer.peer.bssid,6);struct mt76_eapol5_view v={0};if(mt76_eapol5_rx(&peer,wifi_stream5.buffer[1][p],bytes,&v)==MT76_RX_OK && v.eapol_size<=sizeof wifi_stream5.cached){n=v.eapol_size;copy_bytes(wifi_stream5.cached,v.eapol,n);}zero_bytes(&peer,sizeof peer);
   }else{struct mt76_rx_view v={0};if(mt76_rx_usb_view(wifi_stream5.buffer[1][p],bytes,&v)==MT76_RX_OK && v.mpdu_size>=24 && v.mpdu_size<=sizeof wifi_stream5.cached && v.body_size==v.mpdu_size-24){n=v.mpdu_size;copy_bytes(wifi_stream5.cached,v.header,24);copy_bytes(wifi_stream5.cached+24,v.body,v.body_size);}}
  }else wifi_stream5.drained++;
  if(good){wifi_stream5.cached_size=(unsigned)n;wifi_stream5.cached_type=type;wifi_stream5.cached_present=n!=0;good=mt76_dma5_release(&wifi_stream5.dma,l);if(good){zero_bytes(l,sizeof *l);wifi_stream5.ready_rx=false;}}
 }
 event_irq_restore(f);return good?true:wifi_stream5_fail();
}
static bool wifi_stream5_fences(void){
 if(wifi_stream5.dma.cursor[0]!=wifi_stream5_positions() && wifi_stream5.dma.cursor[1]!=wifi_stream5_positions())return true;
 uint64_t start=timer_monotonic_us(),prev=start;
 for(unsigned i=0;i<8192 && (wifi_stream5.pending[0] || wifi_stream5.pending[1]);i++){
  if(!wifi_stream5_dispatch() || !wifi_stream5_collect(wifi_stream5_type(),!wifi_stream5.data_active && wifi_stream5.exchange.keys_erased))return false;
  uint64_t now=timer_monotonic_us();if(now<prev || now-start>=750000 || !wifi_stream5_delay_us(100))return wifi_stream5_fail();prev=now;
 }
 if(wifi_stream5.pending[0] || wifi_stream5.pending[1] || !wifi_stream5_collect(wifi_stream5_type(),!wifi_stream5.data_active && wifi_stream5.exchange.keys_erased) || wifi_stream5.ready_rx || wifi_stream5.loan.ticket)return wifi_stream5_fail();
 for(unsigned dir=0;dir<2;dir++)if(wifi_stream5.dma.cursor[dir]==wifi_stream5_positions()){
  if(wifi_stream5.dma.wraps>=wifi_stream5_fence_limit())return wifi_stream5_fail();
  start=prev=timer_monotonic_us();bool done=false,begun=false;
  for(unsigned i=0;i<8192;i++){
   if(!begun){
    enum mt76_fence5_result r=wifi_fence5_try_begin(dir);
    if(r==MT76_FENCE5_FAILED)return wifi_stream5_fail();
    begun=r==MT76_FENCE5_READY;
    if(!begun && !wifi_stream5_dispatch())return false;
   }
   if(begun){
    if((wifi_fence5_native.engine.state==MT76_FENCE5_STOP_WAIT || wifi_fence5_native.engine.state==MT76_FENCE5_REBASE_WAIT) && !wifi_stream5_dispatch())return false;
    if(!wifi_fence5_step())return wifi_stream5_fail();
    if(!wifi_fence5_native.round){done=true;break;}
   }
   uint64_t now=timer_monotonic_us();if(now<prev || now-start>=750000 || !wifi_stream5_delay_us(100))return wifi_stream5_fail();prev=now;
  }
  if(!done || wifi_stream5.enqueue[dir] || wifi_stream5.cycle[dir]!=wifi_stream5.dma.cycle[dir])return wifi_stream5_fail();
  wifi_stream5.wraps[dir]++;
 }
 return true;
}
static bool wifi_stream5_publish(unsigned dir,const uint8_t *packet,unsigned length){
 if(!wifi_stream5_owned(&wifi_stream5,false) || dir>1 || wifi_stream5.pending[dir] || (wifi_stream5.dma.cursor[dir]==wifi_stream5_positions() && !wifi_stream5_fences()) || (dir && (wifi_stream5.ready_rx || wifi_stream5.cached_present)) || wifi_stream5.posts[dir]>=(dir?WIFI_STREAM5_RX:WIFI_STREAM5_TX))return false;
 uint64_t f=event_irq_save();struct mt76_dma5_lease *l=&wifi_stream5.lease[dir];bool good=mt76_dma5_prepare(&wifi_stream5.dma,dir,wifi_stream5.posts[dir]+1,length,timer_monotonic_us(),l);
 if(good){unsigned p=l->index,index=(unsigned)wifi_stream5.enqueue[dir];zero_bytes(wifi_stream5.buffer[dir][p],4096);if(!dir){copy_bytes(wifi_stream5.buffer[0][p],packet,length);copy_bytes(wifi_stream5.page_shadow[0][p],wifi_stream5.buffer[0][p],4096);}good=index<wifi_stream5_positions() && l->trb==wifi_stream5.physical[dir]+16*index && l->cycle==wifi_stream5.cycle[dir] && mt76_dma5_publish(&wifi_stream5.dma,l,timer_monotonic_us());
  if(good && !dir && wifi_stream5.data_radio.enabled && length>=48){
   unsigned base=3+wifi_stream5.data_radio.slot*DHCP_RADIO5_TAGS,pid=packet[23];
   if(pid>=base && pid<base+wifi_stream5.data_radio.count)
    dhcp_radio5_bind(&wifi_stream5.data_radio,pid-base+1,l->ticket,l->token);
  }
  if(good){struct xhci_trb td={.parameter_low=l->page,.parameter_high=l->page>>32,.status=length,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|(dir?XHCI_TRB_ISP:0)};wifi_stream5.pending[dir]=true;wifi_stream5.posts[dir]++;uint64_t ptr=wifi_stream5_push(dir,&td);wifi_stream5.shadow[dir][index]=td;wifi_stream5.shadow[dir][index].control|=l->cycle;if(index+1==wifi_stream5_positions())wifi_stream5_link(wifi_stream5.shadow[dir],wifi_stream5.physical[dir],l->cycle);good=ptr==l->trb;if(good)db_write32(wifi_stream5.slot,dir?9:8);}
 }
 event_irq_restore(f);return good?true:wifi_stream5_fail();
}
static bool wifi_stream5_post(void){return wifi_stream5.pending[1] || wifi_stream5.ready_rx || wifi_stream5.cached_present?true:wifi_stream5_publish(1,NULL,4096);}
static enum mt76_join5_send wifi_stream5_transmit(const uint8_t *packet,unsigned length){
 if(!wifi_stream5_fences() || !wifi_stream5_post() || !wifi_stream5_publish(0,packet,length))return MT76_JOIN5_UNCERTAIN;
 uint64_t start=timer_monotonic_us(),prev=start;
 for(unsigned i=0;i<8192;i++){if(!wifi_stream5_dispatch() || !wifi_stream5_owned(&wifi_stream5,false))return MT76_JOIN5_UNCERTAIN;if(!wifi_stream5.pending[0])return MT76_JOIN5_SENT;uint64_t now=timer_monotonic_us();if(now<prev || now-start>=750000 || !wifi_stream5_delay_us(100))break;prev=now;}
 wifi_stream5_fail();return MT76_JOIN5_UNCERTAIN;
}
static bool wifi_stream5_poll(void *c,enum mt76_wpa5_rx type,uint8_t *out,size_t cap,size_t *n,bool *present){
 if(c!=&wifi_stream5 || !out || !n || !present || !wifi_stream5_owned(c,false))return false;
 *n=0;*present=false;
 if(!wifi_stream5_dispatch() || !wifi_stream5_collect(type,false) || !wifi_stream5_fences())return false;
 if(!wifi_stream5.cached_present && !wifi_stream5_post())return false;
 if(!wifi_stream5_dispatch() || !wifi_stream5_collect(type,false))return false;
 if(wifi_stream5.cached_present){if(wifi_stream5.cached_type==(unsigned)type){if(wifi_stream5.cached_size>cap)return wifi_stream5_fail();copy_bytes(out,wifi_stream5.cached,wifi_stream5.cached_size);*n=wifi_stream5.cached_size;*present=true;}zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_present=false;wifi_stream5.cached_size=0;}
 /* Observation only:classify the already copied AUTH-phase return. No
  * admission result,packet,address or extra clock read is retained. */
 if(wifi_stream5.stack_mode && type==MT76_WPA5_MANAGEMENT && wifi_stream5.exchange.auth_sent && !wifi_stream5.exchange.auth_matched && !wifi_stream5.exchange.keys_erased && *present){
  wifi_stream5.auth_observation.present++;
  const struct mt76_mgmt5_peer *p=&wifi_stream5.peer.peer;
  if(*n>=24 && (out[0]&0xfc)==0xb0 && wifi_handoff5_equal(out+4,p->station,6) && wifi_handoff5_equal(out+10,p->bssid,6) && wifi_handoff5_equal(out+16,p->bssid,6)){
   struct mt76_mgmt5_response r={0};enum mt76_mgmt5_status status=mt76_mgmt5_response(p,MT76_MGMT5_AUTH,out,*n,&r);
   wifi_stream5.auth_observation.peer++;
   if(status==MT76_MGMT5_OK)wifi_stream5.auth_observation.ok++;
   else if(status==MT76_MGMT5_REJECTED)wifi_stream5.auth_observation.rejected++;
   else wifi_stream5.auth_observation.invalid++;
   zero_bytes(&r,sizeof r);
  }
 }
 return wifi_stream5_owned(c,false);
}
static bool wifi_stream5_auth_radio(unsigned phase);
static enum mt76_join5_send wifi_stream5_send(void *c,enum mt76_wpa5_tx kind,const uint8_t *packet,size_t length){
 struct mt76_wpa5 *s=&wifi_stream5.exchange;uint64_t count=wifi_stream5.posts[0];
 bool good=c==&wifi_stream5 && packet==s->packet && length && length<=sizeof s->packet && wifi_stream5_owned(c,false) &&
 ((kind==MT76_WPA5_AUTH && !count) || (kind==MT76_WPA5_ASSOC && count==1 && s->auth_matched) || (kind==MT76_WPA5_M2 && count==2 && s->m1_matched) || (kind==MT76_WPA5_M4 && count==3 && s->m3_mic && s->keys_installed) || (kind==MT76_WPA5_LEAVE && count==4+wifi_stream5.data_sends && s->keys_erased && s->handshake.phase==WPA2_HS_CLOSED && wifi_stream5.closed));
 if(!good)return MT76_JOIN5_REFUSED;
 /* Finish observing AUTH before any ASSOC/EAPOL publication. */
 if(kind==MT76_WPA5_ASSOC && !wifi_stream5_auth_radio(1))return MT76_JOIN5_UNCERTAIN;
 wifi_stream5.tx_kind=(unsigned)kind;return wifi_stream5_transmit(packet,(unsigned)length);
}
static bool wifi_stream5_delay(void *c,unsigned us){return c==&wifi_stream5 && us==100 && wifi_stream5_owned(c,false) && wifi_stream5_delay_us(us);}
static uint64_t wifi_stream5_now(void *c){
 (void)c;uint64_t now=timer_monotonic_us();
 if(wifi_stream5.stack_mode && wifi_stream5.exchange.auth_sent && !wifi_stream5.exchange.auth_matched && !wifi_stream5.exchange.keys_erased){
  if(!wifi_stream5.auth_observation.clocks)wifi_stream5.auth_observation.first=now;
  else if(now<wifi_stream5.auth_observation.last)wifi_stream5.auth_observation.rollback=true;
  wifi_stream5.auth_observation.last=now;wifi_stream5.auth_observation.clocks++;
 }
 return now;
}
static bool wifi_stream5_nonce(void *c,uint8_t out[32]){return c==&wifi_stream5 && out==wifi_stream5.exchange.snonce && wifi_stream5_owned(c,false) && wifi_stream5_entropy(out);}
static bool wifi_stream5_data_owned(void *c){
 struct mt76_wpa5 *s=&wifi_stream5.exchange;const struct wpa2_ccmp *k=&s->keys;
 return c==&wifi_stream5 && wifi_stream5.data_active && !wifi_stream5.closed && s->keys_activated && k->phase==WPA2_CCMP_ACTIVE && k->generation==wifi_stream5.epoch && k->tx_pn==wifi_stream5.data_pn && s->handshake.phase==WPA2_HS_COMPLETE && s->handshake.generation==wifi_stream5.epoch && s->handshake.key_owner==(uintptr_t)k && s->handshake.emissions==2 && s->handshake.install_handoffs==1 && s->handshake.keys_may_live && !s->handshake.unsafe && k->peer.rsne_size==wifi_stream5.peer.peer.rsn_size && k->peer.rsne_size<=64 && wifi_handoff5_equal(k->peer.aa,wifi_stream5.peer.peer.bssid,6) && wifi_handoff5_equal(k->peer.spa,wifi_stream5.peer.peer.station,6) && wifi_handoff5_equal(k->peer.rsne,wifi_stream5.peer.peer.rsn,k->peer.rsne_size) && wifi_stream5_owned(c,false);
}
static enum mt76_join5_send wifi_stream5_data_send(void *c,const uint8_t *frame,size_t length){
 struct mt76_stream5_probe *p=&wifi_stream5.probe;struct wpa2_ccmp *k=&wifi_stream5.exchange.keys;
 if(!wifi_stream5_data_owned(c) || (frame!=p->setup.tx && frame!=p->tx) || length>1514 || wifi_stream5.data_sends>=70u+(unsigned)wifi_stream5.discover_retry || wifi_stream5.posts[0]!=4+wifi_stream5.data_sends)return MT76_JOIN5_REFUSED;
 uint64_t pn=k->tx_pn;size_t bytes=mt76_data5_tx(&wifi_stream5.scratch,k,wifi_stream5.epoch,frame,length,9+wifi_stream5.data_sends,wifi_stream5.usb,sizeof wifi_stream5.usb);
 if(!bytes || k->tx_pn!=pn+1){zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);return k->tx_pn==pn?MT76_JOIN5_REFUSED:MT76_JOIN5_UNCERTAIN;}wifi_stream5.data_pn=k->tx_pn;
 wifi_stream5.tx_kind=WIFI_STREAM5_DATA_TX;enum mt76_join5_send result=wifi_stream5_transmit(wifi_stream5.usb,(unsigned)bytes);zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);
 if(result==MT76_JOIN5_SENT){wifi_stream5.data_sends++;if(wifi_stream5.wraps[0])wifi_stream5.data_after_txwrap++;}return result;
}
static bool wifi_stream5_data_poll(void *c,uint8_t *out,size_t cap,size_t *n,bool *present){struct mt76_stream5_probe *p=&wifi_stream5.probe;return wifi_stream5_data_owned(c) && (out==p->setup.rx || out==p->rx) && cap==1514 && wifi_stream5_poll(c,MT76_WPA5_DATA,out,cap,n,present);}
static bool wifi_stream5_exhausted(void *c){return wifi_stream5_data_owned(c) && wifi_stream5.posts[1]>=WIFI_STREAM5_RX-8 && !wifi_stream5.pending[1] && !wifi_stream5.ready_rx && !wifi_stream5.cached_present;}
static enum mt76_join5_send wifi_stream5_window(void *c,struct wpa2_ccmp *keys,uint64_t epoch,const struct mt76_peer5 *peer){
 if(wifi_stream5.stack_mode)return wifi_stack5_window(c,keys,epoch,peer);
 if(c!=&wifi_stream5 || keys!=&wifi_stream5.exchange.keys || epoch!=wifi_stream5.epoch || peer!=&wifi_stream5.peer || wifi_stream5.posts[0]!=4 || wifi_stream5.data_active)return MT76_JOIN5_REFUSED;
 wifi_stream5.data_active=true;uint8_t entropy[32]={0};bool good=wifi_stream5_data_owned(c) && wifi_stream5_entropy(entropy);uint32_t xid=(uint32_t)entropy[0]<<24|(uint32_t)entropy[1]<<16|(uint32_t)entropy[2]<<8|entropy[3];zero_bytes(entropy,sizeof entropy);
 const struct mt76_ip5_io io={c,wifi_stream5_data_owned,wifi_stream5_data_send,wifi_stream5_data_poll,wifi_stream5_delay,wifi_stream5_now,wifi_stream5_exhausted};
 if(good)good=wifi_stream5.discover_retry?mt76_stream5_probe_run_discover_retry(&wifi_stream5.probe,peer->peer.station,xid,&io):mt76_stream5_probe_run(&wifi_stream5.probe,peer->peer.station,xid,&io);
 bool unsafe=wifi_stream5.probe.unsafe || !wifi_stream5_data_owned(c);wifi_stream5.closed=true;wifi_stream5.data_active=false;zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);wifi_stream5.cached_present=false;
 return unsafe?MT76_JOIN5_UNCERTAIN:good?MT76_JOIN5_SENT:MT76_JOIN5_REFUSED;
}
static bool wifi_stream5_drain(void){
 if(!wifi_stream5.mac.mac_disabled || !wifi_stream5.exchange.keys_erased)return true;
 if(!wifi_stream5_dispatch() || !wifi_stream5_collect(MT76_WPA5_DATA,true))return false;
 if(!wifi_stream5.pending[1] && !wifi_stream5.ready_rx && !wifi_stream5_post())return false;
 return wifi_stream5_dispatch() && wifi_stream5_collect(MT76_WPA5_DATA,true);
}
static bool wifi_stream5_usb(void *c,bool write,uint32_t a,uint32_t *v,bool cleanup){
 if(c!=&wifi_stream5 || !v || !wifi_stream5_owned(c,cleanup))return false;
 switch(a){case 0x1004:case 0x2000:case 0x208:case 0x1400:case 0x1340:case 0x13b0:case 0x2124:case 0x438:case 0xa30:case 0xa34:case 0x1200:case 0x430:break;default:return false;}
 if(write && a!=0x1004 && a!=0x1400 && a!=0x1340)return false;
 return
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_usb(wifi_stream5.d,write,a,v) &&
#else
 wifi_mac_private_usb(wifi_stream5.d,write,a,v) &&
#endif
 wifi_stream5_owned(c,cleanup)?true:wifi_stream5_fail();
}
static bool wifi_stream5_auth_radio(unsigned phase){
 if(!wifi_stream5.connection_mode || !wifi_stream5.stack_mode)return true;
 if(phase>1 || wifi_stream5.auth_radio.attempted[phase])return phase<2 && wifi_stream5.auth_radio.complete[phase];
 wifi_stream5.auth_radio.attempted[phase]=true;
 if((!phase && wifi_stream5.posts[0]) || (phase && (!wifi_stream5.auth_radio.complete[0] || wifi_stream5.posts[0]!=1 || wifi_stream5.pending[0])))return wifi_stream5_fail();
 /* Linux mt76x02 reads EXT before FIFO and drains16 hardware entries. A
  * seventeenth read proves emptiness or records saturation explicitly. No
  * waiting for radio status, replay, or AUTH deadline extension. */
 for(unsigned i=0;i<17;i++){
  uint32_t words[2]={0};const uint32_t addresses[]={0x1798,0x1718};
  for(unsigned j=0;j<2;j++){
   if(!wifi_stream5_owned(&wifi_stream5,false))return wifi_stream5_fail();
   bool good=
#ifdef SRVROS_XHCI_STREAM5_TEST
    stream_test_usb(wifi_stream5.d,false,addresses[j],&words[j]);
#else
    wifi_mac_private_usb(wifi_stream5.d,false,addresses[j],&words[j]);
#endif
   if(!good || !wifi_stream5_owned(&wifi_stream5,false))return wifi_stream5_fail();
  }
  wifi_stream5.auth_radio.ext[phase][i]=words[0];wifi_stream5.auth_radio.fifo[phase][i]=words[1];wifi_stream5.auth_radio.count[phase]++;
  if(!(words[1]&1)){wifi_stream5.auth_radio.empty[phase]=true;break;}
 }
 wifi_stream5.auth_radio.complete[phase]=true;return true;
}
/* Bounded consuming reads only outside provider callbacks. No status wait,
 * logging, retries or protocol decision. Partial/failed reads remain retained. */
static bool wifi_stream5_data_radio(unsigned phase){
 struct dhcp_radio5 *s=&wifi_stream5.data_radio;
 if(!s->enabled)return true;
 if(phase>1 || s->phase[phase].attempted)return false;
 struct dhcp_radio5_phase *p=&s->phase[phase];p->attempted=1;p->started=timer_monotonic_us();
 if(!wifi_stream5.connection_mode || !wifi_stream5.stack_mode || s->epoch!=wifi_stream5.epoch ||
    wifi_stream5.data_active || wifi_stream5.pending[0] || wifi_stream5.loan.ticket ||
    wifi_fence5_native.round || xhci.command_pending ||
    (phase?(!s->phase[0].complete || !wifi_http5_data_radio_ready() || !wifi_stream5.closed ||
      !wifi_stream5.exchange.keys_erased || wifi_stream5.exchange.handshake.keys_may_live ||
      !wifi_stream5.mac.mac_disabled || !wifi_stream5.mac.tx_drained || !wifi_stream5.mac.rx_drained):
     (wifi_stream5.posts[0]!=4 || wifi_stream5.completions[0]!=4 || wifi_stream5.closed ||
      !wifi_stream5.exchange.keys_activated || wifi_stream5.exchange.keys_erased))){p->error=2;return wifi_stream5_fail();}
 uint64_t last=p->started;
 for(unsigned i=0;i<DHCP_RADIO5_READS;i++){
  const uint32_t addresses[2]={0x1798,0x1718};
  for(unsigned j=0;j<2;j++){
   if(!wifi_stream5_owned(&wifi_stream5,phase!=0)){p->error=2;goto failed;}
   uint32_t value=0;bool good=
#ifdef SRVROS_XHCI_STREAM5_TEST
    stream_test_usb(wifi_stream5.d,false,addresses[j],&value);
#else
    wifi_mac_private_usb(wifi_stream5.d,false,addresses[j],&value);
#endif
   if(good){if(!j){p->ext[i]=value;p->ext_reads++;}else{p->fifo[i]=value;p->fifo_reads++;}}
   if(!good){p->error=1;goto failed;}
   if(!wifi_stream5_owned(&wifi_stream5,phase!=0)){p->error=2;goto failed;}
   uint64_t now=timer_monotonic_us();p->finished=now;
   if(now<last || now<p->started || now-p->started>=DHCP_RADIO5_US){p->error=3;goto failed;}
   last=now;
  }
  if(!(p->fifo[i]&1)){p->empty=1;break;}
 }
 p->complete=1;return true;
failed:return wifi_stream5_fail();
}
static bool wifi_stream5_read(void *c,uint32_t a,uint32_t *v,bool cleanup){if(!v)return false;*v=0;return (!cleanup || wifi_stream5_drain()) && wifi_stream5_usb(c,false,a,v,cleanup);}
static bool wifi_stream5_write(void *c,uint32_t a,uint32_t v,bool cleanup){return wifi_stream5_usb(c,true,a,&v,cleanup);}
static bool wifi_stream5_mac_delay(void *c,unsigned us,bool cleanup){return us<=10000 && (!cleanup || wifi_stream5_drain()) && wifi_stream5_owned(c,cleanup) && wifi_stream5_delay_us(us);}
static bool wifi_stream5_inspection(void *c,uint32_t alc,uint32_t ibi){
 if(c!=&wifi_stream5 || alc!=wifi_power5.cycle.chosen[0] || ibi!=UINT32_MAX || !wifi_stream5_owned(c,false))return false;
 const struct mt76_wpa5_io io={c,wifi_stream5_owned,wifi_stream5_nonce,wifi_stream5_send,wifi_stream5_poll,wifi_stream5_delay,wifi_stream5_now};const struct mt76_wpa5_data data={c,wifi_stream5_window};
 bool good=wifi_stream5_auth_radio(0) && mt76_wpa5_run_data(&wifi_stream5.exchange,&wifi_stream5.peer,wifi_stream5.pmk,&io,&data);
 /* A timed-out AUTH never reaches the ASSOC callback. Observe its unchanged
  * unsuccessful transaction before MAC teardown. */
 if(wifi_stream5.exchange.auth_sent && !wifi_stream5.auth_radio.attempted[1])good=wifi_stream5_auth_radio(1) && good;
 zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);return good;
}
static bool wifi_stream5_command(unsigned type,unsigned dci,uint64_t dequeue,unsigned next){
 struct xhci_device *d=wifi_stream5.d;if(!wifi_stream5_owned(&wifi_stream5,true) || xhci.command_pending || xhci.command_enqueue>=63)return wifi_stream5_fail();
 struct xhci_trb t={.control=(type<<10)|((uint32_t)wifi_stream5.slot<<24)};if(type==12){t.parameter_low=d->input_context_phys;t.parameter_high=d->input_context_phys>>32;}else{t.control|=dci<<16;t.parameter_low=dequeue;t.parameter_high=dequeue>>32;}
 unsigned index=(unsigned)xhci.command_enqueue;uint64_t expected=xhci.command_ring_phys+16*index,count=xhci.command_events;struct xhci_trb shadow=t;shadow.control|=xhci.command_cycle?1u:0u;
 uint64_t f=event_irq_save(),slot=0;firmware_command_exact=true;wifi_stream5.commands++;wifi_stream5.stop_dci=type==15?dci:0;
 bool good=
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_command(&t,type,&slot) &&
#else
 submit_command_trb(&t,type,&slot) &&
#endif
 slot==wifi_stream5.slot && xhci.command_events==count+1 && xhci.last_completion_code==1 && xhci.last_slot_id==slot && xhci.last_command_pointer==expected && xhci.last_expected_command_pointer==expected && wifi_handoff5_equal(&xhci.command_ring[index],&shadow,sizeof shadow);
 unsigned record=wifi_stream5.commands-1;if(record>=6)good=false;else{wifi_stream5.sync[record].pointer=expected;wifi_stream5.sync[record].event_pointer=xhci.last_command_pointer;wifi_stream5.sync[record].dequeue=dequeue;wifi_stream5.sync[record].delta=xhci.command_events-count;wifi_stream5.sync[record].type=type;wifi_stream5.sync[record].dci=dci;wifi_stream5.sync[record].slot=(unsigned)slot;wifi_stream5.sync[record].cc=(unsigned)xhci.last_completion_code;}
 firmware_command_exact=false;wifi_stream5.stop_dci=0;
 if(type==15){
  unsigned dir=dci==9;const struct xhci_trb *e=&wifi_stream5.stops[dir];
  uint64_t ptr=e->parameter_low|((uint64_t)e->parameter_high<<32);
  __atomic_thread_fence(__ATOMIC_ACQUIRE);
  uint64_t deq=context_read32(d->output_context,dci,2)|((uint64_t)context_read32(d->output_context,dci,3)<<32);
  unsigned cycle=wifi_stream5.stop_was_pending[dir]?wifi_stream5.lease[dir].cycle:(unsigned)wifi_stream5.cycle[dir];
  good=good && wifi_stream5.stop_events[dir]==1 && (context_read32(d->output_context,dci,0)&7)==3 && !(deq&14) && (deq&~15ull)==ptr && (deq&1)==cycle;
  if(good)wifi_stream5.stop_pointer[dir]=deq;
 }
 if(good){wifi_stream5.phase=next;for(unsigned dir=0;dir<2;dir++)for(unsigned j=0;j<8;j++)wifi_stream5.snapshots[next][dir][j]=context_read32(d->output_context,dir?9:8,j);}
 event_irq_restore(f);return good && wifi_stream5_owned(&wifi_stream5,true)?true:wifi_stream5_fail();
}
static bool wifi_stream5_credentials(void){
 const struct initramfs_file *file;unsigned matches=0;
#ifdef SRVROS_XHCI_STREAM5_TEST
 file=stream_test_credential();matches=file?1:0;
#else
 file=initramfs_find("/cfg/wifi-wpa5.pmk");for(uint64_t i=0;i<initramfs_file_count();i++){const struct initramfs_file *f=initramfs_file_at(i);if(f && f->path && wifi_wpa5_path(f->path))matches++;}
#endif
 if(matches!=1 || !file || !file->data || file->size!=73 || file->mode!=0600)return false;
 const uint8_t *p=file->data;const uint8_t magic[8]={'S','R','W','P','A','5',0,1};if(!wifi_handoff5_equal(p,magic,8) || !p[8] || p[8]>32 || p[8]!=wifi_stream5.peer.peer.ssid_size || !wifi_handoff5_equal(p+9,wifi_stream5.peer.peer.ssid,p[8]))return false;
 for(unsigned i=p[8];i<32;i++)if(p[9+i])return false;
 copy_bytes(wifi_stream5.pmk,p+41,32);return true;
}
static bool wifi_stream5_inspect(void){
 if(wifi_stream5.attempted || !wifi_stream5_parent())return false;
 wifi_stream5.attempted=true;wifi_stream5.d=wifi_cal5.d;struct xhci_device *d=wifi_stream5.d;wifi_stream5.slot=d->slot_id;wifi_stream5.peer=wifi_join5.peer;wifi_stream5.epoch=wifi_stream5.peer.generation;
 if(wifi_stream5.connection_mode && wifi_stream5_current==&wifi_stream5_slots[1]){
  if(!wifi_stream5_slots[0].epoch || wifi_stream5_slots[0].epoch==UINT64_MAX)return false;
  wifi_stream5.epoch=wifi_stream5_slots[0].epoch+1;wifi_stream5.peer.generation=wifi_stream5.epoch;
 }
 if(wifi_fence5_native.bound || xhci.command_enqueue+2+2*wifi_stream5_fence_limit()+6>63 || !wifi_stream5_credentials())return false;
 const unsigned dcis[]={0,11,16,8,9};for(unsigned i=0;i<5;i++)for(unsigned j=0;j<8;j++)wifi_stream5.context[i][j]=context_read32(d->output_context,dcis[i],j);
 for(unsigned dir=0;dir<2;dir++)for(unsigned j=0;j<8;j++)wifi_stream5.snapshots[0][dir][j]=wifi_stream5.context[dir?4:3][j];
 copy_bytes(wifi_stream5.parent,(const uint8_t *)&wifi_join5,sizeof wifi_join5);copy_bytes(wifi_stream5.parent_ring[0],(const uint8_t *)wifi_join5.rx,4096);copy_bytes(wifi_stream5.parent_ring[1],(const uint8_t *)wifi_join5.tx,4096);
 for(unsigned i=0;i<WIFI_JOIN5_RX+3;i++)copy_bytes(wifi_stream5.parent_pages[i],wifi_join5.buffer[i],4096);
 copy_bytes(wifi_stream5.cal,(const uint8_t *)&wifi_cal5,sizeof wifi_cal5);copy_bytes(wifi_stream5.power,(const uint8_t *)&wifi_power5,sizeof wifi_power5);copy_bytes(wifi_stream5.handoff,(const uint8_t *)&wifi_handoff5,sizeof wifi_handoff5);copy_bytes(wifi_stream5.binding,(const uint8_t *)&wifi_peer5,sizeof wifi_peer5);copy_bytes(wifi_stream5.preview,(const uint8_t *)&wifi_tx5,sizeof wifi_tx5);
 const void *old[]={wifi_cal5.ep.ring,wifi_handoff5.ring,wifi_tx5.ring};for(unsigned i=0;i<3;i++)copy_bytes(wifi_stream5.old_ring[i],old[i],4096);for(unsigned i=0;i<8;i++)copy_bytes(wifi_stream5.old_pages[i],wifi_cal5.plan.buffer[i],4096);
 copy_bytes(wifi_stream5.input,d->input_context,4096);copy_bytes(wifi_stream5.expected,d->input_context,4096);wifi_stream5.input_phys=d->input_context_phys;wifi_stream5.output_phys=d->output_context_phys;wifi_stream5.input_pointer=(uintptr_t)d->input_context;wifi_stream5.output_pointer=(uintptr_t)d->output_context;
 const uint64_t fixed[]={d->ep0_ring_phys,d->descriptor_buffer_phys,d->input_context_phys,d->output_context_phys,d->firmware.buffer_phys,d->firmware.ep.ring_phys,d->firmware.mcu.buffer_phys,d->firmware.mcu.response.ring_phys,xhci.command_ring_phys};for(unsigned i=0;i<9;i++)wifi_stream5.fixed[i]=fixed[i];
 uint64_t allocated[10]={0};for(unsigned i=0;i<10;i++){void *p;uint64_t phys;
  if(wifi_stream5.connection_mode){
   phys=pmm_alloc_frame_tagged(PMM_FRAME_DMA);if(!phys || !(p=pmm_phys_to_virt(phys)))return false;
  }else if(!alloc_frame_zero(&phys,&p))return false;
  for(unsigned j=0;j<9;j++)if(phys==fixed[j])return false;
  for(unsigned j=0;j<i;j++)if(phys==allocated[j])return false;
  for(unsigned j=0;j<WIFI_JOIN5_RX+5;j++)if(phys==wifi_join5.pins[j])return false;
  if(phys==xhci.event_ring_phys || phys==wifi_cal5.plan.ring || phys==wifi_handoff5.plan.new_ring || phys==wifi_tx5.ring_phys)return false;
  if(wifi_cal5_cleanup_alias(phys))return false;
  for(unsigned j=0;j<8;j++)if(phys==wifi_cal5.plan.physical[j])return false;
  if(wifi_stream5_current==&wifi_stream5_slots[1])for(unsigned j=0;j<10;j++){
   uint64_t old=j<2?wifi_stream5_slots[0].physical[j]:wifi_stream5_slots[0].pages[(j-2)/4][(j-2)%4];
   if(phys==old)return false;
  }
  if(wifi_stream5.connection_mode)zero_bytes(p,PMM_FRAME_SIZE);
  allocated[i]=phys;wifi_stream5.pointers[i]=(uintptr_t)p;
  if(i<2){wifi_stream5.physical[i]=phys;wifi_stream5.ring[i]=p;wifi_stream5.cycle[i]=true;wifi_stream5_link(p,phys,true);wifi_stream5_link(wifi_stream5.shadow[i],phys,true);}else{unsigned dir=(i-2)/4,page=(i-2)%4;wifi_stream5.pages[dir][page]=phys;wifi_stream5.buffer[dir][page]=p;}
 }
 if(!mt76_dma5_init_ring(&wifi_stream5.dma,wifi_stream5.epoch,wifi_stream5.physical,wifi_stream5.pages,wifi_stream5_positions(),timer_monotonic_us()))return false;
 if(wifi_stream5.stack_mode)console_write("wifi-stack5-progress: original four-way key owner;bounded asynchronous production provider;four pages/direction;real RX/TX fences;erase/leave and full restore\n");
 else if(wifi_stream5.discover_retry)console_write("wifi-stream5-progress: original four-way key owner;four pages/direction;up to71 encrypted requests;64 exact echoes;one bounded Discover retry;real RX/TX fences;erase/leave and full restore\n");
 else console_write("wifi-stream5-progress: original four-way key owner;four pages/direction;70 encrypted requests;64 exact echoes;real RX/TX fences;erase/leave and full restore\n");
 wifi_stream5.active=true;if(!wifi_stream5_owned(&wifi_stream5,false))return wifi_stream5_fail();
 struct usb_setup_packet setup={.request_type=2,.request=1,.value=0,.index=4,.length=0};int actual=-1;
 wifi_stream5.toggle_synced=
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_toggle(d,&setup,&actual) &&
#else
 wifi_fw_control_once(d,&setup,0,0,false,&actual) &&
#endif
 actual==0 && wifi_stream5_owned(&wifi_stream5,false);if(!wifi_stream5.toggle_synced)return wifi_stream5_fail();
 zero_bytes(d->input_context,4096);context_write32(d->input_context,0,1,1u|(1u<<8));for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_stream5.context[0][j]);context_write32(d->input_context,9,1,(3u<<1)|(2u<<3)|(512u<<16));context_write32(d->input_context,9,2,wifi_stream5.physical[0]|1);context_write32(d->input_context,9,3,wifi_stream5.physical[0]>>32);context_write32(d->input_context,9,4,MT76_DATA5_USB_MAX);copy_bytes(wifi_stream5.expected,d->input_context,4096);
 if(!wifi_stream5_command(12,8,0,1) || !wifi_stream5_command(16,9,wifi_stream5.physical[1]|1,2))return false;
 if(!wifi_fence5_bind(d,&wifi_stream5.dma,wifi_stream5.epoch,&wifi_stream5,wifi_stream5_fence_owned))return wifi_stream5_fail();
 wifi_stream5.phase=3;
 if(!wifi_stream5_post())return false;
 const struct mt76_mac_cycle_io io={&wifi_stream5,wifi_stream5_owned,wifi_stream5_read,wifi_stream5_write,wifi_stream5_mac_delay,wifi_stream5_now,wifi_stream5_inspection};bool exchanged=mt76_mac_cycle_run(&wifi_stream5.mac,&io);
 bool good=!wifi_stream5.failed && wifi_stream5.exchange.keys_erased && wifi_stream5.mac.mac_disabled && wifi_stream5.mac.tx_drained && wifi_stream5.mac.rx_drained && wifi_stream5.mac.filter_restored && wifi_stream5.mac.txop_restored && wifi_stream5_owned(&wifi_stream5,true);
 if(good)good=wifi_stream5_data_radio(1);
 if(good)good=wifi_stream5_command(15,9,0,4);
 if(good)good=wifi_stream5_command(15,8,0,5);
 uint64_t old_dequeue=wifi_stream5.context[4][2]|((uint64_t)wifi_stream5.context[4][3]<<32);if(good)good=wifi_stream5_command(16,9,old_dequeue,6);
 if(good){zero_bytes(d->input_context,4096);context_write32(d->input_context,0,0,1u<<8);context_write32(d->input_context,0,1,1);for(unsigned j=0;j<3;j++)context_write32(d->input_context,1,j,wifi_stream5.context[0][j]);copy_bytes(wifi_stream5.expected,d->input_context,4096);good=wifi_stream5_command(12,8,0,7);}
 if(good){copy_bytes(d->input_context,wifi_stream5.input,4096);copy_bytes(wifi_stream5.expected,wifi_stream5.input,4096);wifi_stream5.phase=9;for(unsigned dir=0;dir<2;dir++)for(unsigned j=0;j<8;j++)wifi_stream5.snapshots[9][dir][j]=context_read32(d->output_context,dir?9:8,j);wifi_stream5.restored=wifi_stream5_owned(&wifi_stream5,true) && wifi_stream5_parent();good=wifi_stream5.restored;}
 zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);if(good)wifi_stream5.active=false;
 wifi_stream5.complete=good && exchanged && wifi_stream5.exchange.complete && (wifi_stream5.stack_mode?wifi_stack5_complete():wifi_stream5.probe.complete && wifi_stream5.probe.echoes_matched==64 && wifi_stream5.probe.setup.discover_retries<=(unsigned)wifi_stream5.discover_retry && wifi_stream5.data_sends==70u+wifi_stream5.probe.setup.discover_retries && wifi_stream5.wraps[0]>=1 && wifi_stream5.wraps[1]>=1 && wifi_stream5.data_after_txwrap && wifi_stream5.data_after_rxwrap);
 return wifi_stream5.complete;
}
static bool wifi_stream5_zero(const void *p,size_t bytes){const uint8_t *v=p;unsigned n=0;while(bytes--)n|=*v++;return n==0;}
static bool wifi_stream5_probe_erased(void){const struct mt76_stream5_probe *p=&wifi_stream5.probe;const struct mt76_ip5 *a=&p->setup;
 return !p->xid && !p->address && !p->router && !a->xid && !a->address && !a->server && !a->mask && !a->router && !a->resolver && !a->answer && !a->request_size && wifi_stream5_zero(p->tx,sizeof p->tx) && wifi_stream5_zero(p->rx,sizeof p->rx) && wifi_stream5_zero(p->station,sizeof p->station) && wifi_stream5_zero(p->router_mac,sizeof p->router_mac) && wifi_stream5_zero(a->tx,sizeof a->tx) && wifi_stream5_zero(a->rx,sizeof a->rx) && wifi_stream5_zero(a->request_digest,sizeof a->request_digest) && wifi_stream5_zero(a->station,sizeof a->station) && wifi_stream5_zero(a->dns_mac,sizeof a->dns_mac) && wifi_stream5_zero(a->router_mac,sizeof a->router_mac);
}
/* Actual first/last Ethernet plus nearest admitted frame before/after each
 * completed RX fence, de-duplicated and bounded to eight retained receipts. */
static unsigned wifi_stream5_rx_select(unsigned selected[8]){
 unsigned first=UINT32_MAX,last=UINT32_MAX,before[3]={UINT32_MAX,UINT32_MAX,UINT32_MAX},after[3]={UINT32_MAX,UINT32_MAX,UINT32_MAX};
 for(unsigned i=0;i<wifi_stream5.record_count && i<WIFI_STREAM5_TX+WIFI_STREAM5_RX;i++){
  const struct wifi_stream5_record *r=&wifi_stream5.records[i];if(!r->direction || !r->ethernet)continue;
  if(first==UINT32_MAX)first=i;
  last=i;
  for(unsigned n=0;n<wifi_stream5.wraps[1] && n<3;n++){uint64_t boundary=wifi_stream5_positions()*(n+1);if(r->token<=boundary)before[n]=i;else if(after[n]==UINT32_MAX)after[n]=i;}
 }
 unsigned count=0;for(unsigned i=0;i<wifi_stream5.record_count && i<WIFI_STREAM5_TX+WIFI_STREAM5_RX;i++){
  bool keep=i==first || i==last;for(unsigned n=0;n<3;n++)keep=keep || i==before[n] || i==after[n];if(keep && count<8)selected[count++]=i;
 }return count;
}
static __attribute__((unused)) void wifi_stream5_packet_emit(const struct wifi_stream5_state *s){
 /* Format only after the HTTP supervisor has checked all child/worker
  * quiescence and the adapter capture/guard returned. No extra console work
  * occurs between connection close and next-owner admission. */
 if(s->connection_mode && s->stack_mode && s->closed){
  unsigned count=0;for(unsigned i=0;i<s->record_count;i++){
   const struct wifi_stream5_record *r=&s->records[i];
   if((!r->direction && r->kind==WIFI_STREAM5_DATA_TX) || (r->direction && r->ethernet))count++;
  }
  console_printf("wifi-packet5: version=1 epoch=%u records=%u observation_only=1\n",s->epoch,(uint64_t)count);
  for(unsigned i=0;i<s->record_count;i++){
   const struct wifi_stream5_record *r=&s->records[i];
   if((!r->direction && r->kind==WIFI_STREAM5_DATA_TX) || (r->direction && r->ethernet))
    console_printf("wifi-packet5-frame: index=%u ticket=%u token=%u direction=%u ethernet=%u role=%u message=%u sequence=%u started_us=%u finished_us=%u radio_delivery_proved=0\n",
     (uint64_t)i,r->ticket,r->token,(uint64_t)r->direction,(uint64_t)r->packet.bytes,(uint64_t)r->packet.role,(uint64_t)r->packet.message,(uint64_t)r->sequence,r->started_us,r->finished_us);
  }
 }
}
/* Scalar-only emission after the outer capture/guard; never live I/O. */
static void wifi_stream5_data_radio_record_emit(const struct dhcp_radio5 *s,bool held){
 if(!s->enabled)return;
 console_printf("wifi-dhcp-radio5: version=2 epoch=%u slot=%u tags=%u overflow=%u binding_error=%u held=%u observation_only=1\n",s->epoch,(uint64_t)s->slot,(uint64_t)s->count,(uint64_t)s->overflow,(uint64_t)s->binding_error,(uint64_t)held);
 for(unsigned i=0;i<s->count && i<DHCP_RADIO5_TAGS;i++){
  const struct dhcp_radio5_tx *t=&s->tx[i];
  console_printf("wifi-dhcp-radio5-tx: i=%u pid=%u message=%u sequence=%u ticket=%u token=%u completed=%u finished_us=%u\n",(uint64_t)i,(uint64_t)t->pid,(uint64_t)t->message,(uint64_t)t->sequence,t->ticket,t->token,(uint64_t)t->completed,t->finished);
 }
 for(unsigned phase=0;phase<2;phase++){
  const struct dhcp_radio5_phase *p=&s->phase[phase];
  console_printf("wifi-dhcp-radio5-phase: phase=%u attempted=%u complete=%u ext_reads=%u fifo_reads=%u empty=%u error=%u started_us=%u finished_us=%u\n",(uint64_t)phase,(uint64_t)p->attempted,(uint64_t)p->complete,(uint64_t)p->ext_reads,(uint64_t)p->fifo_reads,(uint64_t)p->empty,(uint64_t)p->error,p->started,p->finished);
  for(unsigned i=0;i<p->ext_reads && i<DHCP_RADIO5_READS;i++)
   console_printf("wifi-dhcp-radio5-entry: phase=%u i=%u ext=%u fifo_present=%u fifo=%u\n",(uint64_t)phase,(uint64_t)i,(uint64_t)p->ext[i],(uint64_t)(i<p->fifo_reads),(uint64_t)p->fifo[i]);
 }
}
static unsigned wifi_stream5_emit(void){
 unsigned selected[8];unsigned rx_receipts=wifi_stream5_rx_select(selected);
 if(wifi_stream5.connection_mode && wifi_stream5.stack_mode){
  unsigned tx=0,rx=0;for(unsigned i=0;i<wifi_stream5.record_count;i++){
   const struct wifi_stream5_record *r=&wifi_stream5.records[i];
   tx+=!r->direction && r->kind==WIFI_STREAM5_DATA_TX;rx+=r->direction && r->reason;
  }
  console_printf("wifi-encrypted5: version=1 epoch=%u tx=%u rx=%u records=%u keys_erased=%u retired=%u observation_only=1\n",wifi_stream5.epoch,(uint64_t)tx,(uint64_t)rx,(uint64_t)wifi_stream5.record_count,(uint64_t)wifi_stream5.exchange.keys_erased,(uint64_t)wifi_stream5.retired);
 }
 if(wifi_stream5.connection_mode && wifi_stream5.stack_mode)for(unsigned i=0;i<wifi_stream5.record_count;i++){
  const struct wifi_stream5_record *r=&wifi_stream5.records[i];
  if(!r->direction && r->kind==WIFI_STREAM5_DATA_TX)console_printf("wifi-encrypted5-tx: index=%u ticket=%u token=%u pn=%u bytes=%u mpdu=%u rate=%u ack=%u wcid=%u cc=%u residue=%u started_us=%u finished_us=%u radio_delivery_proved=0\n",(uint64_t)i,r->ticket,r->token,r->pn,(uint64_t)r->bytes,(uint64_t)r->mpdu,(uint64_t)r->rate,(uint64_t)r->ack,(uint64_t)r->wcid,(uint64_t)(r->event.status>>24),(uint64_t)(r->event.status&0xffffff),r->started_us,r->finished_us);
  if(r->direction && r->reason)console_printf("wifi-encrypted5-rx: index=%u ticket=%u token=%u bytes=%u reason=%u flags=%u frame=%u audit=%u span=%u dma=%u mpdu=%u trailing=%u ethernet=%u ethertype=%u\n",(uint64_t)i,r->ticket,r->token,(uint64_t)r->bytes,(uint64_t)r->reason,(uint64_t)r->flags,(uint64_t)r->frame,(uint64_t)r->audit,(uint64_t)r->span,(uint64_t)r->dma,(uint64_t)r->mpdu,(uint64_t)r->trailing,(uint64_t)r->ethernet,(uint64_t)r->ethertype);
 }

 if(wifi_stream5.connection_mode && wifi_stream5.stack_mode)for(unsigned p=0;p<2;p++){
  console_printf("wifi-stream5-radio: version=1 phase=%u attempted=%u complete=%u reads=%u empty=%u saturated=%u observation_only=1\n",(uint64_t)p,(uint64_t)wifi_stream5.auth_radio.attempted[p],(uint64_t)wifi_stream5.auth_radio.complete[p],(uint64_t)wifi_stream5.auth_radio.count[p],(uint64_t)wifi_stream5.auth_radio.empty[p],(uint64_t)(wifi_stream5.auth_radio.complete[p] && !wifi_stream5.auth_radio.empty[p]));
  for(unsigned i=0;i<wifi_stream5.auth_radio.count[p] && i<17;i++)console_printf("wifi-stream5-radio-entry: phase=%u i=%u ext=%x fifo=%x\n",(uint64_t)p,(uint64_t)i,(uint64_t)wifi_stream5.auth_radio.ext[p][i],(uint64_t)wifi_stream5.auth_radio.fifo[p][i]);
 }
 console_printf("wifi-stream5-plan: tx=%x rx=%x epoch=%u slot=%u pages=4 positions=%u max_tx=80 max_rx=252 max_fences=%u\n",wifi_stream5.physical[0],wifi_stream5.physical[1],wifi_stream5.epoch,(uint64_t)wifi_stream5.slot,(uint64_t)wifi_stream5_positions(),(uint64_t)wifi_stream5_fence_limit());
 for(unsigned i=0;i<wifi_stream5.record_count && i<WIFI_STREAM5_TX+WIFI_STREAM5_RX;i++){const struct wifi_stream5_record *r=&wifi_stream5.records[i];uint64_t ptr=r->event.parameter_low|((uint64_t)r->event.parameter_high<<32);console_printf("wifi-stream5-td: %u %u %x %u %u\n",(uint64_t)i,(uint64_t)r->direction,ptr,(uint64_t)r->event.status,(uint64_t)r->event.control);}
 for(unsigned i=0;i<wifi_stream5.record_count && i<WIFI_STREAM5_TX+WIFI_STREAM5_RX;i++){const struct wifi_stream5_record *r=&wifi_stream5.records[i];
 if(!r->direction && r->kind==WIFI_STREAM5_DATA_TX)console_printf("wifi-stream5-data: %u %u %u %u %u %u %u\n",(uint64_t)i,r->ticket,r->token,(uint64_t)r->page,(uint64_t)r->cycle,r->pn,(uint64_t)r->bytes);
 bool selected_rx=false;for(unsigned n=0;n<rx_receipts;n++)selected_rx=selected_rx || selected[n]==i;
 if(selected_rx)console_printf("wifi-stream5-rx: %u %u %u %u %u %u %u\n",(uint64_t)i,r->ticket,r->token,(uint64_t)r->page,(uint64_t)r->cycle,(uint64_t)r->ethernet,(uint64_t)r->reason);
 }
 for(unsigned i=0;i<wifi_fence5_native.count && i<8;i++){const struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[i];const struct xhci_trb *e=&r->event,*t=&r->stop;uint64_t ep=e->parameter_low|((uint64_t)e->parameter_high<<32),sp=t->parameter_low|((uint64_t)t->parameter_high<<32);
 console_printf("wifi-stream5-fence: i=%u p=%x t=%u d=%u q=%x e=%x s=%u c=%u sp=%x ss=%u sc=%u sq=%x used=%u\n",(uint64_t)i,r->id.pointer,(uint64_t)r->id.type,(uint64_t)r->id.dci,r->id.dequeue,ep,(uint64_t)e->status,(uint64_t)e->control,sp,(uint64_t)t->status,(uint64_t)t->control,r->stop_dequeue,(uint64_t)r->consumed);}
 for(unsigned i=0;i<wifi_stream5.commands && i<6;i++)console_printf("wifi-stream5-sync: i=%u p=%x e=%x t=%u d=%u q=%x slot=%u cc=%u delta=%u\n",(uint64_t)i,wifi_stream5.sync[i].pointer,wifi_stream5.sync[i].event_pointer,(uint64_t)wifi_stream5.sync[i].type,(uint64_t)wifi_stream5.sync[i].dci,wifi_stream5.sync[i].dequeue,(uint64_t)wifi_stream5.sync[i].slot,(uint64_t)wifi_stream5.sync[i].cc,wifi_stream5.sync[i].delta);
 const unsigned phases[]={0,2,5,9};for(unsigned n=0;n<4;n++)for(unsigned d=0;d<2;d++){const uint32_t *w=wifi_stream5.snapshots[phases[n]][d];console_printf("wifi-stream5-context: p=%u d=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u\n",(uint64_t)phases[n],(uint64_t)d,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);}
 for(unsigned d=0;d<2;d++){const struct xhci_trb *e=&wifi_stream5.stops[d];uint64_t p=e->parameter_low|((uint64_t)e->parameter_high<<32);console_printf("wifi-stream5-stop: d=%u p=%x s=%u c=%u q=%x\n",(uint64_t)d,p,(uint64_t)e->status,(uint64_t)e->control,wifi_stream5.stop_pointer[d]);}
 const struct xhci_trb *bad=&wifi_stream5.fault_event;uint64_t bad_pointer=bad->parameter_low|((uint64_t)bad->parameter_high<<32);
 console_printf("wifi-stream5-state: failed=%u phase=%u first_phase=%u pending_tx=%u pending_rx=%u round=%u command_pending=%u fault_event=%u p=%x s=%u c=%u\n",(uint64_t)wifi_stream5.failed,(uint64_t)wifi_stream5.phase,(uint64_t)wifi_stream5.fault_phase,(uint64_t)wifi_stream5.pending[0],(uint64_t)wifi_stream5.pending[1],(uint64_t)wifi_fence5_native.round,(uint64_t)xhci.command_pending,(uint64_t)wifi_stream5.fault_event_valid,bad_pointer,(uint64_t)bad->status,(uint64_t)bad->control);
 const struct mt76_wpa5 *s=&wifi_stream5.exchange;console_printf("wifi-stream5-keys: auth=%u assoc=%u m1=%u m2=%u m3=%u m4=%u activated=%u erased=%u leave=%u unsafe=%u installs=%u pn=%u\n",(uint64_t)s->auth_matched,(uint64_t)s->assoc_matched,(uint64_t)s->m1_matched,(uint64_t)s->m2_sent,(uint64_t)s->m3_mic,(uint64_t)s->m4_sent,(uint64_t)s->keys_activated,(uint64_t)s->keys_erased,(uint64_t)s->leave_sent,(uint64_t)s->unsafe,s->handshake.install_handoffs,wifi_stream5.data_pn);
 if(s->attempted){
  unsigned count=0;for(unsigned i=0;i<4;i++)if(s->responses[i].result)count++;
  console_printf("wifi-handshake5: version=1 epoch=%u responses=%u observation_only=1\n",wifi_stream5.epoch,(uint64_t)count);
  for(unsigned i=0;i<4;i++){const struct mt76_wpa5_response *r=&s->responses[i];if(r->result)
   console_printf("wifi-handshake5-response: stage=%u result=%u polls=%u present=%u ignored=%u kind=%u reject=%u start_us=%u last_us=%u\n",(uint64_t)i,(uint64_t)r->result,(uint64_t)r->polls,(uint64_t)r->present,(uint64_t)r->ignored,(uint64_t)r->kind,(uint64_t)r->reject,r->start,r->last);
  }
 }
 if(wifi_stream5.stack_mode)console_printf("wifi-stack5-auth: attempted=%u auth_sent=%u assoc_sent=%u sends=%u polls=%u ignored=%u status=%u aid=%u present=%u peer=%u ok=%u invalid=%u rejected=%u clocks=%u elapsed_us=%u rollback=%u\n",(uint64_t)s->attempted,(uint64_t)s->auth_sent,(uint64_t)s->assoc_sent,(uint64_t)s->sends,(uint64_t)s->polls,(uint64_t)s->ignored,(uint64_t)s->status,(uint64_t)s->aid,(uint64_t)wifi_stream5.auth_observation.present,(uint64_t)wifi_stream5.auth_observation.peer,(uint64_t)wifi_stream5.auth_observation.ok,(uint64_t)wifi_stream5.auth_observation.invalid,(uint64_t)wifi_stream5.auth_observation.rejected,(uint64_t)wifi_stream5.auth_observation.clocks,wifi_stream5.auth_observation.last>=wifi_stream5.auth_observation.first?wifi_stream5.auth_observation.last-wifi_stream5.auth_observation.first:0,(uint64_t)wifi_stream5.auth_observation.rollback);
 const struct mt76_stream5_probe *p=&wifi_stream5.probe;console_printf("wifi-stream5-probe: complete=%u unsafe=%u setup=%u setup_sends=%u dhcp=%u dns=%u echo=%u sent=%u matched=%u elapsed_us=%u private_erased=%u timed_out=%u exhausted=%u clock_fault=%u polls=%u ignored=%u total_polls=%u\n",(uint64_t)p->complete,(uint64_t)p->unsafe,(uint64_t)p->setup.complete,(uint64_t)p->setup.sends,(uint64_t)(p->setup.discover && p->setup.offer && p->setup.request && p->setup.ack),(uint64_t)p->setup.dns,(uint64_t)p->setup.echo,(uint64_t)p->echoes_sent,(uint64_t)p->echoes_matched,p->elapsed_us,(uint64_t)wifi_stream5_probe_erased(),(uint64_t)p->timed_out,(uint64_t)p->exhausted,(uint64_t)p->clock_fault,(uint64_t)p->polls,(uint64_t)p->ignored,(uint64_t)p->total_polls);
 if(wifi_stream5.discover_retry){const struct mt76_ip5 *a=&p->setup;for(unsigned phase=0;phase<2;phase++){
  console_printf("wifi-stream5-dhcp: %u %u %u %u %u %u %u %u %u %u",(uint64_t)phase,(uint64_t)a->discover_retries,(uint64_t)a->sends,(uint64_t)a->polls,(uint64_t)a->ignored,(uint64_t)a->dhcp_reflected[phase],(uint64_t)a->dhcp_type[phase][0],(uint64_t)a->dhcp_type[phase][1],(uint64_t)a->dhcp_type[phase][2],(uint64_t)a->dhcp_type[phase][3]);
  for(unsigned reason=0;reason<MT76_IP5_DHCP_REASONS;reason++)console_printf(" %u",(uint64_t)a->dhcp_class[phase][reason]);
  console_write("\n");
 }}
 return rx_receipts;
}
static bool wifi_stream5_close(bool good,bool adapter_complete){
 bool accepted=good;
 /* Semantic failure is never success. Only the independently bound HTTP
  * pre-handoff abort may request ordinary exact physical retirement afterward. */
 bool failed_http=!good && !adapter_complete && wifi_stream5.connection_mode &&
  wifi_stream5.stack_mode && wifi_http5_failed_connection_closed();
 good=good || failed_http;
 /* Connection close proves restoration to the still-owned adapter window;
  * only the outer capture return may claim full adapter-parent restoration. */
 wifi_stream5.parent_complete=good && adapter_complete;
 if(!adapter_complete && (!wifi_stream5.connection_mode || !wifi_stream5_parent() ||
    !wifi_stream5.restored || wifi_stream5.active || wifi_stream5.failed ||
    (!wifi_stream5.exchange.complete && !failed_http) || !wifi_stream5.exchange.keys_erased ||
    !wifi_stream5.exchange.leave_sent))good=false;
 if(good && !wifi_stream5.pending[0] && !wifi_fence5_native.round && !xhci.command_pending){uint64_t f=event_irq_save();struct xhci_trb e;bool empty=false;
  for(unsigned i=0;i<32;i++){if(!poll_event(&e)){empty=true;break;}uint64_t p=e.parameter_low|((uint64_t)e.parameter_high<<32);bool ours=false;for(unsigned d=0;d<2;d++)if(p>=wifi_stream5.physical[d] && p<wifi_stream5.physical[d]+4096)ours=true;if(((e.control>>10)&63)!=32 || ours){good=false;break;}dispatch_transfer_event(&e);}
  wifi_stream5.events_empty=empty;good=good && empty && !wifi_fence5_native.failed;event_irq_restore(f);
  if(good){struct mt76_dma5_retirement proof={.epoch=wifi_stream5.epoch,.keys_erased=wifi_stream5.exchange.keys_erased && wpa2_ccmp_erased(&wifi_stream5.exchange.keys,wifi_stream5.epoch) && !wifi_stream5.exchange.handshake.keys_may_live,.mac_disabled=wifi_stream5.mac.mac_disabled && wifi_stream5.mac.tx_drained && wifi_stream5.mac.rx_drained,.tx_stopped=wifi_stream5.stop_events[0]==1 && wifi_stream5.restored,.rx_stopped=wifi_stream5.stop_events[1]==1 && wifi_stream5.restored,.events_drained=empty,.callbacks_drained=!wifi_stream5.active && !wifi_stream5.loan.ticket && wifi_stream5.closed,.parent_restored=adapter_complete?wifi_stream5.parent_complete:good && wifi_stream5_parent()};wifi_stream5.retired=mt76_dma5_close(&wifi_stream5.dma,wifi_stream5.epoch) && mt76_dma5_retire(&wifi_stream5.dma,&proof);good=wifi_stream5.retired;}
 }
 if(wifi_stream5.pending[0] || wifi_fence5_native.round || xhci.command_pending)good=false;
 wifi_stream5.connection_closed=good && !adapter_complete;
 wifi_stream5.closed=true;wifi_stream5.data_active=false;uint64_t emit_flags=event_irq_save();
 unsigned rx_receipts=wifi_stream5_emit();if(wifi_stream5.stack_mode)wifi_stack5_emit();else console_printf("wifi-stream5: complete=%u parent=%u restored=%u retired=%u empty=%u tx=%u rx=%u tx_wraps=%u rx_wraps=%u data_tx=%u data_rx=%u tx_after=%u rx_after=%u borrowed=%u released=%u faults=%u commands=%u fences=%u rx_receipts=%u retained=1 ordinary_rx=0 tcp=0 http=0 backend_retired=0\n",(uint64_t)good,(uint64_t)wifi_stream5.parent_complete,(uint64_t)wifi_stream5.restored,(uint64_t)wifi_stream5.retired,(uint64_t)wifi_stream5.events_empty,wifi_stream5.posts[0],wifi_stream5.posts[1],(uint64_t)wifi_stream5.wraps[0],(uint64_t)wifi_stream5.wraps[1],(uint64_t)wifi_stream5.data_sends,(uint64_t)wifi_stream5.data_received,(uint64_t)wifi_stream5.data_after_txwrap,(uint64_t)wifi_stream5.data_after_rxwrap,wifi_stream5.dma.borrowed,wifi_stream5.dma.released,wifi_stream5.dma.faults,(uint64_t)wifi_stream5.commands,wifi_stream5.dma.wraps,(uint64_t)rx_receipts);
 event_irq_restore(emit_flags);return accepted && good;
}

static bool wifi_stream5_finish(bool good){return wifi_stream5_close(good,true);}

static bool wifi_stream5_capture(bool discover_retry){
 if(wifi_stream5.attempted || wifi_join5_after || wifi_wpa5_requested || wifi_wpa5_data_requested || wifi_provider5_requested || wifi_reuse5_requested || wifi_tx5_nop_child)return false;
 wifi_stream5.discover_retry=discover_retry;wifi_join5_after=wifi_stream5_inspect;bool good=
#ifdef SRVROS_XHCI_STREAM5_TEST
 stream_test_capture_parent();
#else
 xhci_wifi_capture_eapol5();
#endif
wifi_join5_after=NULL;zero_bytes(wifi_stream5.pmk,sizeof wifi_stream5.pmk);zero_bytes(wifi_stream5.usb,sizeof wifi_stream5.usb);zero_bytes(wifi_stream5.cached,sizeof wifi_stream5.cached);zero_bytes(&wifi_stream5.scratch,sizeof wifi_stream5.scratch);return wifi_stream5_finish(good);
}

bool xhci_wifi_capture_stream5(void){return wifi_stream5_capture(false);}
bool xhci_wifi_capture_stream5_dhcp(void){return wifi_stream5_capture(true);}

#include "xhci_wifi_stack5.inc"
