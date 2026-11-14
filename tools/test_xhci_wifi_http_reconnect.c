/* Actual xHCI reconnect/HTTP supervisor + production net/worker/CCMP/AP crypto.
 * Scheduler turns and child process APIs are modeled; no ELF/VM/native claim. */
#define SRVROS_XHCI_HTTP5_HOST_TEST 1
#include <srvros/dhcp_identity5.h>
#define STACK5_TEST_MAIN http_reconnect_legacy_main
#define scheduler_current_thread_id legacy_thread_id
#define scheduler_yield legacy_yield
#define scheduler_wait_timeout legacy_wait_timeout
#define scheduler_wait legacy_wait
#define scheduler_wake_all legacy_wake_all
#define process_current legacy_current
#include "test_xhci_wifi_stack5.c"
#undef scheduler_current_thread_id
#undef scheduler_yield
#undef scheduler_wait_timeout
#undef scheduler_wait
#undef scheduler_wake_all
#undef process_current
#include <setjmp.h>
#include "../userspace/wifihttpprobe/http_exact.h"
static jmp_buf parked_worker,held_run;
static uint64_t current_thread=1;
static bool in_worker,in_child;
static unsigned workers,child_count,child_done,trial_case;
static struct {scheduler_thread_fn fn;void *arg;uint64_t id;} threads[2];
static struct {
 struct http_child_owner *owner;uint32_t ip;int64_t socket;
 char request[HTTP_EXACT_REQUEST_CAPACITY];size_t length,offset;
 struct http_exact_parser parser;bool go,done,eof;uint64_t status;
} children[2];
static void child_turn(void){
 if(!child_count)return;
 unsigned g=child_count-1;
 if(!children[g].go && !children[g].done && test_time>=children[g].owner->ready_us+60000000){children[g].done=true;children[g].status=24;}
 if(!children[g].go || children[g].done)return;
 in_child=true;
 if(children[g].socket<=0){children[g].socket=net_connect_try(children[g].ip,80);assert(children[g].socket>0);goto done;}
 if(net_error((uint64_t)children[g].socket)==SRV_NETERR_INPROGRESS)goto done;
 assert(!net_error((uint64_t)children[g].socket));
 if(children[g].offset<children[g].length){
  int64_t n=net_respond_try_checked((uint64_t)children[g].socket,children[g].request+children[g].offset,children[g].length-children[g].offset,NULL);
  assert(n>0 || n==SRV_ERR_AGAIN);if(n>0)children[g].offset+=(size_t)n;goto done;
 }
 char buffer[512];int64_t n=net_read((uint64_t)children[g].socket,buffer,sizeof buffer,true);
 assert(n>=0 || n==SRV_ERR_AGAIN);
 if(n>0)assert(http_exact_feed(&children[g].parser,buffer,(size_t)n)==HTTP_EXACT_MORE);
 else if(!n){
  assert(http_exact_eof(&children[g].parser)==HTTP_EXACT_COMPLETE);children[g].eof=true;
  assert(!net_close((uint64_t)children[g].socket));children[g].done=true;child_done++;
 }
 done:in_child=false;
}
uint64_t scheduler_current_thread_id(void){return current_thread;}
struct process *process_current(void){return in_child?(struct process *)&children[child_count-1]:NULL;}
bool timer_idle_wakeup_ready(void){return true;}
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg){
 assert(!strcmp(name,"net-probe") && workers<2 && !in_worker && !in_child);
 if((trial_case==3 && workers==1) || trial_case==37)return false;
 threads[workers].fn=fn;threads[workers].arg=arg;threads[workers].id=2+workers;workers++;return true;
}
static void worker_turn(void){
 assert(workers && !in_worker);unsigned i=workers-1;current_thread=threads[i].id;in_worker=true;
 if(!setjmp(parked_worker))threads[i].fn(threads[i].arg);
 in_worker=false;current_thread=1;
}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg){
 assert(in_worker && !ready(arg));q->waiters=1;longjmp(parked_worker,1);
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t end){
 (void)q;(void)ready;(void)arg;(void)end;
 if(in_worker)longjmp(parked_worker,1);
 test_time+=10000;worker_turn();child_turn();return true;
}
void scheduler_wake_all(struct scheduler_wait_queue *q){q->waiters=0;}
void scheduler_yield(void){assert(!in_worker);test_time+=10000;worker_turn();child_turn();}
bool http_child_prepare(struct http_child_owner *o,bool dry){
 assert(!dry && !o->self && child_count<2 && !in_child && !in_worker);
 unsigned g=child_count++;children[g].owner=o;
 o->self=(uintptr_t)o;o->token=100+g;o->pid=200+g;o->ready_us=timer_monotonic_us();o->prepared=true;
 assert(http_exact_build_request(children[g].request,sizeof children[g].request,&children[g].length) && children[g].length==4159);
 http_exact_init(&children[g].parser);return true;
}
bool http_child_publish(struct http_child_owner *o,uint64_t epoch,uint64_t start,uint64_t end,uint32_t ip){
 unsigned g=child_count-1;assert(children[g].owner==o && !children[g].go && end==start+15000000 && epoch && ip);
 o->published=true;o->epoch=epoch;children[g].go=true;children[g].ip=ip;return true;
}
bool http_child_observe(struct http_child_owner *o,uint64_t *status){
 unsigned g=child_count-1;assert(children[g].owner==o);
 if(!children[g].done || o->reaped)return false;
 o->observed=true;*status=children[g].status?children[g].status:trial_case==4?1:0;return true;
}
bool http_child_cancel(struct http_child_owner *o){
 unsigned g=child_count-1;assert(children[g].owner==o);if(!children[g].go && !children[g].done){children[g].done=true;children[g].status=137;}return true;
}
bool http_child_reap(struct http_child_owner *o){
 struct net_probe_worker_info w={0};assert(net_probe_worker_snapshot(&w) && (!w.prepared || w.finished) && !w.active);
 assert(children[child_count-1].owner==o && children[child_count-1].done);o->reaped=true;return true;
}
bool http_child_retire(struct http_child_owner *o){assert(o->reaped);o->retired=true;return true;}
__attribute__((noreturn)) void wifi_http5_test_hold(void){longjmp(held_run,1);}
bool http5_test_RF_parent(bool (*)(void),bool);
/* Counterfactual reproducer: discard a proved local close as the old caller did. */
static bool discard_closed_failure(void){assert(wifi_join5_after());return false;}
static bool legacy_rf_parent(bool (*callback)(void),bool expected){
 (void)callback;(void)expected;return http5_test_RF_parent(discard_closed_failure,false);
}
static bool handoff_rf_parent(bool (*callback)(void),bool expected){
 (void)expected;return http5_test_RF_parent(callback,false);
}
static bool slow_ready_parent(bool (*callback)(void),bool expected){
 assert(expected && !child_count && !workers && !wifi_http_reconnect5.probe[0].self);
 test_time+=70000000;child_turn();
 assert(!child_count && !workers); /* no pre-GO budget exists during RF setup */
 return http5_test_RF_parent(callback,true);
}
static bool no_target_parent(void){assert(wifi_http_reconnect5_unattempted() && !wifi_stream5.attempted);return true;}
/* Inject only after production HTTP has emitted its proved first close.
 * No shortcut invokes the guard instead of the actual supervisor path. */
/* Queue a real production hub GET_STATUS TD; only DMA/arrival is modeled. */
static void handoff_hub(unsigned index,unsigned offset){
 struct xhci_device *d=&xhci.devices[1+index];
 *d=(struct xhci_device){.present=true,.configured=true,.addressed=true,
  .slot_id=2+index,.port_id=8,.device_class=9,.hub_ports=2,.hub_poll_port=1,.hub_status_pending=true};
 assert(allocate_device_runtime(d));
 if(trial_case==28)d->ep0_enqueue=62; /* TD crosses its Link TRB too. */
 struct usb_setup_packet q={.request_type=0xa3,.request=USB_REQ_GET_STATUS,.index=1,.length=4};
 queue_control_transfer(d,&q,d->descriptor_buffer_phys,4,true);
 d->descriptor_buffer[0]=3;
 struct xhci_trb e={.parameter_low=d->control_td[2],.parameter_high=d->control_td[2]>>32,
  .status=1u<<24,.control=(32u<<10)|(1u<<16)|((uint32_t)d->slot_id<<24)};
 stream_enqueue(&e,offset);
}
static unsigned hub_removed_after_consume;
static void handoff_hub_remove(uint64_t offset,uint32_t value){
 (void)value;if(offset!=xhci.rt_base+XHCI_INTR0+XHCI_INTR_ERDP)return;
 native_boundary_mmio_hook=NULL;hub_removed_after_consume++;
 *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS+7*16)=0;
}
static void handoff_hub_refusals(void){
 struct xhci_device *d=&xhci.devices[1];struct xhci_device saved=*d;
 struct xhci_trb original=xhci.event_ring[xhci.event_dequeue];
 struct xhci_trb setup=d->ep0_ring[0],data=d->ep0_ring[1],status=d->ep0_ring[2];
 unsigned head=xhci.event_dequeue,frontier=xhci.command_enqueue;
 for(unsigned fault=0;fault<28;fault++){
  struct xhci_trb *e=&xhci.event_ring[head];
  switch(fault){
  case 0:e->parameter_low+=16;break;
  case 1:e->status=13u<<24;break;
  case 2:e->status|=1;break;
  case 3:e->control|=4;break;
  case 4:d->hub_status_pending=false;break;
  case 5:d->control_pending=false;break;
  case 6:d->control_short=true;break;
  case 7:d->control_failed=true;break;
  case 8:d->ep0_quarantined=true;break;
  case 9:d->control_td_count=4;break;
  case 10:d->control_td[0]=d->ep0_ring_phys+4096;break;
  case 11:d->control_td[0]=d->control_td[2];break;
  case 12:d->ep0_ring[0].parameter_high^=1;break;
  case 13:d->ep0_ring[1].parameter_low^=16;break;
  case 14:d->ep0_ring[2].control^=XHCI_TRB_IOC;break;
  case 15:d->descriptor_buffer[0]=0;xhci.devices[31]=(struct xhci_device){.present=true,.parent_slot_id=2,.hub_port_id=1};break;
  case 16:d->descriptor_buffer[2]=1;break;
  case 17:d->descriptor_buffer[0]=3|16;break;
  case 18:d->tt_cleanup_owner=1;break;
  case 19:strict_control.device=d;break;
  case 20:d->device_class=0;break;
  case 21:d->hub_poll_port=3;break;
  case 22:d->ep0_ring[1].control|=XHCI_TRB_IDT;break;
  case 23:d->ep0_ring[2].parameter_low=1;break;
  case 24:d->ep0_ring[1].status|=1u<<17;break;
  case 25:d->ep0_ring[0].control^=XHCI_TRB_IDT;break;
  case 26:d->control_td[1]+=16;break;
  case 27:d->descriptor_buffer[0]=1;break;
  default:abort();
  }
  assert(!wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,
   &wifi_stream5_slots[1],wifi_stream5.epoch+1,wifi_reconnect5_closed));
  assert(wifi_fence5_handoff.reason==41 && xhci.event_dequeue==head &&
   xhci.command_enqueue==frontier && !wifi_fence5_connection_switched && !wifi_fence5_next_owner);
  memset(&xhci.devices[31],0,sizeof xhci.devices[31]);
  *d=saved;d->descriptor_buffer[0]=3;d->descriptor_buffer[2]=0;
  d->ep0_ring[0]=setup;d->ep0_ring[1]=data;d->ep0_ring[2]=status;
  *e=original;strict_control.device=NULL;memset(&wifi_fence5_handoff,0,sizeof wifi_fence5_handoff);
 }
}
static unsigned handoff_recheck_mode;
static bool handoff_recheck(void *owner,uint64_t epoch,const struct mt76_dma5 *dma){
 assert(wifi_reconnect5_closed(owner,epoch,dma));
 switch(handoff_recheck_mode){
 case 0:xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.control=(34u<<10)|(xhci.event_cycle?1u:0u)};break;
 case 1:wifi_fence5_first.frontier++;break;
 case 2:wifi_stream5.dma.epoch++;break;
 case 3:xhci.command_enqueue++;break;
 case 4:xhci.sync_users++;break;
 case 5:return false;
 case 6:handoff_hub(0,0);break;
 default:abort();
 }
 return true;
}
static unsigned handoff_injections;
static void handoff_boundary(const char *fmt){
 if(strncmp(fmt,"wifi-http-reconnect5-connection:",32) || handoff_injections)return;
 assert(wifi_http_reconnect5.connections==1 && child_done==1 && wifi_reconnect5_closed(
  &wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma));
 handoff_injections++;
 if(trial_case>=27 && trial_case<=31){
  unsigned count=(trial_case==27 || trial_case==30 || trial_case==31)?1:trial_case==28?8:9;
  if(trial_case==28 || trial_case==29){
   xhci.event_dequeue=XHCI_EVENT_TRB_COUNT-4;
   for(unsigned i=0;i<XHCI_EVENT_TRB_COUNT;i++)
    xhci.event_ring[i].control=(i>=xhci.event_dequeue?!xhci.event_cycle:xhci.event_cycle)?1:0;
  }
  for(unsigned i=0;i<count;i++)handoff_hub(i,i);
  if(trial_case==27)handoff_hub_refusals();
  if(trial_case==30)xhci.devices[1].descriptor_buffer[0]=0;
  if(trial_case==31){
   xhci.port_count=8;
   *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS+7*16)=XHCI_PORTSC_CCS|XHCI_PORTSC_PED;
   native_boundary_mmio_hook=handoff_hub_remove;
  }
 }else if(trial_case==26){
  /* Exact saved hardware category: another slot EP0 success at the shared
   * head after local HTTP/Stop/key closure. Do not fabricate DMA ownership. */
  assert(wifi_fence5_empty());
  xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
   .parameter_low=0x400010,.status=1u<<24,
   .control=(32u<<10)|(1u<<16)|(2u<<24)|(xhci.event_cycle?1u:0u)};
 }else if(trial_case==18 || trial_case==19){
  assert(wifi_fence5_empty());
  xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
   .parameter_low=trial_case==18?(1u<<24):(uint32_t)(uintptr_t)wifi_stream5.ring[1],
   .parameter_high=trial_case==18?0u:(uint32_t)((uintptr_t)wifi_stream5.ring[1]>>32),
   .status=1u<<24,.control=(trial_case==18?(34u<<10):((32u<<10)|(9u<<16)|(wifi_cal5.d->slot_id<<24)))|(xhci.event_cycle?1u:0u)};
 }else if(trial_case==20)xhci.command_pending=true;
 else if(trial_case==21)xhci.command_enqueue=56;
 else if(trial_case==22)wifi_fence5_second.bound=true;
 else if(trial_case==23)wifi_stream5.events_empty=false;
}
/* Independent finite MAC status FIFO (16-entry loss stress or80-entry capture), fed by actual doorbell metadata.
 * It is independent of AP DHCP replies and drops oldest entries on overflow. */
static struct {uint32_t ext[DHCP_RADIO5_TX_LIMIT],fifo[DHCP_RADIO5_TX_LIMIT];unsigned head,count,dropped;bool ext_read,seeded[2];} data_status[2];
static unsigned bound_at_doorbell;
static void radio_queue(unsigned slot,unsigned pid,unsigned wcid,bool success){
 unsigned capacity=trial_case>=49?DHCP_RADIO5_TX_LIMIT:16u;
 unsigned at=(data_status[slot].head+data_status[slot].count)%capacity;
 if(data_status[slot].count==capacity){data_status[slot].head=(data_status[slot].head+1)%capacity;data_status[slot].dropped++;}else data_status[slot].count++;
 data_status[slot].ext[at]=(pid<<8)|2;
 data_status[slot].fifo[at]=0x20000081u|(wcid<<8)|(success?32u:0u);
}
static void data_status_doorbell(uint64_t offset,uint32_t value){
 if(!wifi_stream5.active || !wifi_stream5.d || offset!=xhci.db_base+4*wifi_stream5.slot || value!=8 || !wifi_stream5.pending[0])return;
 unsigned slot=wifi_stream5_current==&wifi_stream5_slots[1];const struct mt76_dma5_lease *l=&wifi_stream5.lease[0];
 const uint8_t *b=wifi_stream5.buffer[0][l->index];if(l->length<24)return;
 unsigned pid=b[23];
 if(pid>=3 && pid<=8){
  unsigned i=pid-3-slot*3;assert(i<wifi_stream5.data_radio.count && i<3);
  const struct dhcp_radio5_tx *t=&wifi_stream5.data_radio.tx[i];
  assert(t->pid==pid && t->ticket==l->ticket && t->token==l->token && !t->completed);bound_at_doorbell++;
 }
 if(trial_case==48 && wifi_stream5.tx_kind==WIFI_STREAM5_DATA_TX && pid==128)return; /* selective status delivery */
 unsigned wcid=trial_case==41 && pid==5?254:b[9];
 radio_queue(slot,pid,wcid,trial_case!=39);
 if(trial_case==41 && pid==4)radio_queue(slot,pid,wcid,true);
}
static bool data_status_read(struct xhci_device *d,bool write,uint32_t address,uint32_t *value){
 assert(d==wifi_stream5.d && !write && !in_worker && !in_child && !wifi_stream5.data_active && !wifi_stack5.running);
 unsigned slot=wifi_stream5_current==&wifi_stream5_slots[1],phase=wifi_stream5.data_radio.phase[1].attempted;
 struct dhcp_radio5_phase *p=&wifi_stream5.data_radio.phase[phase];
 if(phase){assert(wifi_stream5.exchange.keys_erased && wifi_stream5.mac.mac_disabled && wifi_stream5.mac.tx_drained && wifi_stream5.mac.rx_drained && wifi_http5_data_radio_ready());}
 else assert(wifi_stream5.posts[0]==4 && wifi_stream5.exchange.keys_activated);
 if(!data_status[slot].seeded[phase]){
  data_status[slot].seeded[phase]=true;
  if(!phase && trial_case==41)radio_queue(slot,3,255,true);
  if(phase && trial_case==50)radio_queue(slot,3+slot*3,255,true); /* late duplicate */
  if(phase && trial_case==52)while(data_status[slot].count<DHCP_RADIO5_TX_LIMIT)radio_queue(slot,128,255,true);
 }
 if(phase && trial_case==52)test_time+=500;
 if(address==0x1798){
  assert(!data_status[slot].ext_read);data_status[slot].ext_read=true;
  if(!phase && trial_case==47)return false;
  *value=(trial_case==42 && !phase) || (trial_case==43 && phase)?0x8000:
    data_status[slot].count?data_status[slot].ext[data_status[slot].head]:0;
  if(phase && trial_case==45)d->removed=true;
  if(phase && trial_case==46)test_time+=DHCP_RADIO5_US;
  if(phase && p->ext_reads==19 && trial_case==54)d->removed=true;
  if(phase && p->ext_reads==19 && trial_case==55)test_time+=DHCP_RADIO5_US;
 }else{
  assert(address==0x1718 && data_status[slot].ext_read);data_status[slot].ext_read=false;
  if(phase && (trial_case==44 || (trial_case==53 && p->fifo_reads==19)))return false;
  if((trial_case==42 && !phase) || (trial_case==43 && phase)){*value=0x2000ffa1;return true;}
  if(data_status[slot].count){*value=data_status[slot].fifo[data_status[slot].head];data_status[slot].head=(data_status[slot].head+1)%(trial_case>=49?DHCP_RADIO5_TX_LIMIT:16u);data_status[slot].count--;}
  else *value=phase && trial_case==51?0x2000ffa1:0; /* no empty after all real tags */
 }
 assert(p->ext_reads<=DHCP_RADIO5_READS && p->fifo_reads<=DHCP_RADIO5_READS);return true;
}
static void radio_native_failure(void){
 const struct http_network_probe *p=&wifi_http_reconnect5.probe[0];const struct dhcp_radio5 *r=&wifi_stream5_slots[0].data_radio;
 unsigned phase=trial_case==47?0:1,error=(trial_case==45 || trial_case==54)?2:(trial_case==46 || trial_case==55)?3:1;
 assert(!wifi_http_reconnect5.connections && !wifi_http_reconnect5.probe[1].self && child_count==1);
 assert(r->phase[phase].attempted && !r->phase[phase].complete && r->phase[phase].error==error);
 assert(r->phase[phase].fifo_reads==(trial_case>=53?19u:0u) && r->phase[phase].ext_reads==(trial_case>=53?20u:trial_case==47?0u:1u));
 assert(wifi_stream5.failed && !wifi_stream5.retired && !wifi_stream5.connection_closed && wifi_stream5.commands==2 && !wifi_fence5_connection_switched);
 if(trial_case>=53)assert(p->result.complete && !wifi_http_reconnect5.parent_complete && !wifi_http_reconnect5.complete && !wifi_stack5.detached);
 if(trial_case!=47)assert(p->result.quiescent && p->result.child_retired && p->result.worker_finished && p->result.go==(trial_case>=53));
 else assert(!p->result.attempted && !r->count && !wifi_stream5.data_sends && !wifi_stack5_net.identity);
 puts("xhci-http-reconnect5: PASS radio read/control/ownership/clock refusal retains native context;no further FIFO read/Stop/reuse/second generation;acceptance=0");
}
int main(int argc,char **argv){
 trial_case=argc==3 && (!strcmp(argv[1],"--case") || !strcmp(argv[1],"--trace-case"))?(unsigned)strtoul(argv[2],NULL,10):0;assert(trial_case<56);
 if(argc==19 && !strcmp(argv[1],"--trace")){
  for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)reconnect_seed[d][i]=(uint32_t)strtoull(argv[2+d*8+i],NULL,0);
  reconnect_slot=(unsigned)strtoul(argv[18],NULL,0);assert(reconnect_slot && reconnect_slot<256);reconnect_seeded=true;
 }
 stream_setup();
 if(trial_case>=38){native_boundary_mmio_hook=data_status_doorbell;stream_test_data_radio_read=data_status_read;}
 if((trial_case>=18 && trial_case<24) || (trial_case>=26 && trial_case<=31))test_console_printf_hook=handoff_boundary;
 if(trial_case==15)stream_test_before_connection=no_target_parent;
 if(trial_case==36)stream_test_outer_capture=slow_ready_parent;
 if(trial_case==37 || (trial_case>=44 && trial_case<=47) || trial_case>=53)stream_test_outer_capture=handoff_rf_parent;
 if(trial_case==16)stream_test_outer_capture=http5_test_RF_parent;
 if(trial_case==17)stream_test_outer_capture=legacy_rf_parent;
 if(trial_case==18 || trial_case==29 || trial_case==31)stream_test_outer_capture=handoff_rf_parent;
 if(trial_case==24 || trial_case==25 || trial_case==27 || trial_case==28 || trial_case==30 || (trial_case>=32 && trial_case<=35))stream_test_outer_capture=http5_test_RF_parent;
 if((trial_case>=38 && trial_case<=43) || (trial_case>=49 && trial_case<=52))stream_test_outer_capture=http5_test_RF_parent;
 if(reconnect_seeded){for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,d?8:9,i,reconnect_seed[d][i]);wifi_cal5.d->slot_id=(uint8_t)reconnect_slot;}
 http_reconnect_peer=true;http_reconnect_peer_no_fin=trial_case==5;reconnect_hook_count=0;reconnect_case_id=trial_case==1?5:0;
 connection_dhcp_fault=(trial_case==2 || trial_case==16 || trial_case==17)?53:trial_case==12?56:trial_case==13?58:trial_case==14?59:trial_case==24?61:trial_case==25?60:trial_case>=32 && trial_case<=35?62+trial_case-32:0;
 if(trial_case==38 || (trial_case>=41 && trial_case<=46))connection_dhcp_fault=62;
 if(trial_case==39)connection_dhcp_fault=65;
 if(trial_case>=6 && trial_case<12)reconnect_case_id=trial_case+13;
 batch_trace=((argc==2 || argc==19) && !strcmp(argv[1],"--trace")) || (argc==3 && !strcmp(argv[1],"--trace-case"));
 if(batch_trace)puts("http-reconnect5-host: modeled_usb=1 modeled_scheduler=1 modeled_child=1 acceptance=0");
 if(setjmp(held_run)){
  struct http_network_probe *p=wifi_http5_active_probe;
  if((trial_case>=44 && trial_case<=47) || trial_case>=53){radio_native_failure();return 0;}
  assert(wifi_http5_held_state.requested && wifi_http5_held_state.prepared &&
   wifi_http5_held_state.capture_attempted && wifi_http5_held_state.callback_attempted);
  assert(!wifi_http5_requested && !wifi_http5_prepared && !wifi_http5_capture_attempted && !wifi_http5_callback_attempted);
  if(trial_case==13){
   assert(wifi_http5_held_state.site==7 && !p->result.quiescent && !p->result.receiver_closed && !p->result.disconnected);
   assert(wifi_stream5.pending[0] && wifi_stack5.tx_token && wifi_stack5_net.pending && wifi_stream5.exchange.handshake.keys_may_live);
   assert(!wifi_stream5.exchange.keys_erased && !p->child.retired && !wifi_stream5.connection_closed && !wifi_stack5.detached);
   puts("xhci-http-reconnect5: PASS uncertain DHCP TX remains held with original child/worker/key/provider/DMA;terminal protocol cleanup refuses pending debt;acceptance=0");return 0;
  }
  if(trial_case==2){
   fprintf(stderr,"Unexpected hold after ordinary DHCP protocol failure: site=%llu phase=%llu reason=%llu quiescent=%u bootstrap_closed=%u protocol_failed=%u worker_finished=%u\n",(unsigned long long)wifi_http5_held_state.site,(unsigned long long)p->result.phase,(unsigned long long)p->result.reason,p->result.quiescent,p->bootstrap.closed,p->bootstrap.protocol_failed,p->result.worker_finished);return 1;
  }
  if(trial_case==5){
   assert(wifi_http5_held_state.site==7 && !wifi_http_reconnect5.connections && p->result.reason==HTTP_NET_CLOCK);
   assert(p->result.handoff && p->result.go && !p->result.child_observed && !p->result.quiescent);
   assert(children[0].offset==4159 && children[0].parser.body_bytes==3072 && !children[0].eof);
   assert(!p->child.reaped && !p->child.retired && !p->result.disconnected);
   assert(wifi_stream5.exchange.handshake.keys_may_live && !wifi_stream5.exchange.keys_erased && !wifi_stack5.detached);
   puts("xhci-http-reconnect5: PASS missing genuine EOF retains child/worker/key/provider/native ownership at original deadline;no second connection;acceptance=0");return 0;
  }
  fprintf(stderr,"HTTP reconnect held: site=%llu gen=%u phase=%llu reason=%llu quiescent=%u workerfailed=%llu elapsed=%llu services=%llu childoffset=%zu eof=%u\n",
   (unsigned long long)wifi_http5_held_state.site,wifi_http_reconnect5.connections,
   (unsigned long long)p->result.phase,(unsigned long long)p->result.reason,(unsigned)p->result.quiescent,
   (unsigned long long)p->worker.failed,(unsigned long long)p->result.elapsed_us,
   (unsigned long long)wifi_stack5.services,children[child_count-1].offset,children[child_count-1].eof);return 1;
 }
 assert(xhci_wifi_http_reconnect5_prepare());
 assert(!child_count && !workers && !wifi_http_reconnect5.probe[0].self && !xhci_wifi_http_reconnect5_prepare());
 assert(!xhci_wifi_capture_reconnect5_probe() && !xhci_wifi_http5_prepare() && !xhci_wifi_capture_stack5_probe());
 bool good=xhci_wifi_capture_http_reconnect5();
 bool complete=xhci_wifi_http_reconnect5_finish_capture(good);
 if((trial_case>=44 && trial_case<=47) || trial_case>=53){radio_native_failure();return 0;}
 if(trial_case>=38 && trial_case<=43){
  const struct dhcp_radio5 *r=&wifi_stream5_slots[0].data_radio;
  assert(r->phase[0].complete && r->phase[1].complete && !r->binding_error && !r->overflow);
  for(unsigned i=0;i<r->count;i++)assert(r->tx[i].completed && r->tx[i].pid==3+i && r->tx[i].message==(i==2 || (trial_case==40 && i==1)?3u:1u));
  if(trial_case==40){assert(good && complete && wifi_http_reconnect5.connections==2 && data_status[0].dropped && data_status[1].dropped);}
  else{
   assert(!good && !complete && !wifi_http_reconnect5.connections && wifi_http_reconnect5.failure_closed && wifi_http_reconnect5.parent_complete && wifi_stream5.exchange.keys_erased && wifi_stream5.retired && wifi_stack5.detached && !wifi_http_reconnect5.probe[1].self);
   assert(r->count==(trial_case==39?2u:3u) && !wifi_http_reconnect5.probe[0].result.go);
  }
  assert(bound_at_doorbell==(trial_case==40?4u:r->count));
  puts("xhci-http-reconnect5: PASS actual tagged native DHCP/USB plus independent bounded MAC FIFO; MAC outcome never grants DHCP/HTTP success;RF3844;disjoint lifetime IDs;acceptance=0");return 0;
 }
 if(trial_case>=49 && trial_case<=52){
  assert(good && complete && wifi_http_reconnect5.connections==2 && wifi_http_reconnect5.parent_complete && bound_at_doorbell==4);
  for(unsigned g=0;g<2;g++){
   const struct dhcp_radio5 *r=&wifi_stream5_slots[g].data_radio;const struct dhcp_radio5_phase *p=&r->phase[1];
   assert(r->count==2 && !r->overflow && !r->binding_error && !data_status[g].dropped);
   assert(r->phase[0].complete && r->phase[0].empty && p->complete && !p->error && p->fifo_reads>17 && p->fifo_reads<=DHCP_RADIO5_READS);
   assert(p->empty==(trial_case!=51) && p->finished-p->started<DHCP_RADIO5_US);
   if(trial_case==50)assert(((p->ext[p->fifo_reads-2]>>8)&255)==3+g*3 && p->fifo_reads-2>=17);
   if(trial_case==51 || trial_case==52)assert(p->fifo_reads==DHCP_RADIO5_READS);
   if(trial_case==52)assert(p->finished-p->started>=81000);
   for(unsigned i=0;i<r->count;i++)assert(r->tx[i].completed && r->tx[i].pid==3+g*3+i);
  }
  puts("xhci-http-reconnect5: PASS production native25-transfer radio capture, late duplicate/no-empty/max81 budget; original HTTP/RF/retirement guards;acceptance=0");
 }
 if(trial_case==48)assert(bound_at_doorbell==4);
 if(trial_case>=32 && trial_case<=34){
  unsigned epochs=trial_case==33?2:1;
  for(unsigned g=0;g<epochs;g++){
   const struct net_dhcp_identity5 *t=&wifi_http_reconnect5.probe[g].bootstrap.dhcp_identity;
   const struct net_dhcp_observation *o=&wifi_http_reconnect5.probe[g].bootstrap.dhcp_observation;
   assert(t->version==1 && t->tx_discover==2 && t->tx_request==1 && !t->tx_invalid &&
    t->tx_request_mask==DHCP_IDENTITY5_ALL && t->rx_client==3 && t->rx_self_discover==2 &&
    t->rx_self_request==(trial_case==34?0u:1u) && t->rx_client_mismatch==(trial_case==34?1u:0u) &&
    t->rx_client_mask==(trial_case==34?(DHCP_IDENTITY5_ALL^4u):DHCP_IDENTITY5_ALL) &&
    t->rx_offer==1 && t->rx_ack==(trial_case==33?1u:0u) && !t->rx_server_other && !t->rx_malformed);
   assert(o->discovers==2 && o->requests==1 && o->accepted_offer==1 && o->rejected==1 && o->first_reject==NET_DHCP_RX_ETHERNET);
  }
  if(trial_case!=33){
   const struct http_network_result *r=&wifi_http_reconnect5.probe[0].result;
   assert(!good && !complete && !wifi_http_reconnect5.connections && child_count==1 && !child_done &&
    wifi_http_reconnect5.parent_complete && r->reason==HTTP_NET_DHCP_FAILED && r->child_status==137 &&
    !r->go && r->quiescent && r->child_retired && r->worker_finished && r->receiver_closed && r->refs_scrubbed &&
    wifi_stream5.exchange.keys_erased && wifi_stream5.retired && wifi_stack5.detached);
   assert(!wifi_http_reconnect5.probe[1].self && !wifi_fence5_connection_switched);
   puts("xhci-http-reconnect5: PASS late Offer and identity-bound/mismatched Request echo without ACK; original deadline; exact native failure cleanup and RF3844;no second owner;acceptance=0");return 0;
  }
 }
 if(trial_case==37){
  const struct http_network_probe *p=&wifi_http_reconnect5.probe[0];
  assert(!good && !complete && child_count==1 && !workers && !wifi_http_reconnect5.connections && !wifi_http_reconnect5.prepared);
  assert(p->result.quiescent && p->result.child_retired && p->child.retired && !p->result.attempted && !p->result.go && !wifi_stream5.attempted && !wifi_stack5_net.identity && !wifi_http_reconnect5.probe[1].self);
  puts("xhci-http-reconnect5: PASS first late preparation worker failure closes child before keys/stream/provider/GO;no next generation;RF refusal retained;acceptance=0");return 0;
 }
 if(trial_case==35){
  const struct net_bounded_probe *b=&wifi_http_reconnect5.probe[0].bootstrap;
  const struct net_dhcp_identity5 *i=&b->dhcp_identity;const struct http_network_result *r=&wifi_http_reconnect5.probe[0].result;
  assert(!good && !complete && !wifi_http_reconnect5.connections && child_count==1 && !child_done && workers==1);
  assert(i->tx_discover==2 && !i->tx_request && !i->tx_invalid && i->rx_self_discover==1 && i->rx_client_mask==1023 && !i->rx_offer && !i->rx_ack && !i->rx_malformed);
  assert(b->dhcp_observation.discovers==2 && !b->dhcp_observation.offers && !b->dhcp_observation.requests && b->dhcp_observation.frames==2 && b->dhcp_observation.rejected==1);
  assert(r->reason==HTTP_NET_DHCP_FAILED && r->child_status==137 && !r->go && r->child_retired && r->worker_finished && r->quiescent && r->receiver_closed && r->refs_scrubbed);
  assert(wifi_http_reconnect5.failure_closed && wifi_http_reconnect5.parent_complete && wifi_stream5.exchange.keys_erased && wifi_stream5.retired && wifi_stack5.detached && !wifi_fence5_handoff.captured && !wifi_http_reconnect5.probe[1].self);
  puts("xhci-http-reconnect5: PASS saved no-Offer/current-Discover reflection pattern;original deadline and full failed-workload RF3844;no Request/GO/second owner;acceptance=0");return 0;
 }
 if(trial_case==36){assert(test_time>70000000 && child_done==2 && children[0].owner->ready_us>70000000 && !children[0].status && !children[1].status);}
 if(trial_case==31){
  assert(!good && !complete && hub_removed_after_consume==1 && child_done==1 && workers==1 &&
   wifi_http_reconnect5.connections==1 && !wifi_http_reconnect5.probe[1].self);
  assert(wifi_fence5_handoff.reason==57 && !wifi_fence5_handoff.event_valid && wifi_fence5_empty() &&
   !wifi_fence5_connection_switched && xhci.devices[1].removed && !wifi_fence5_next_owner);
  puts("xhci-http-reconnect5: PASS hub removed after consumption; empty head never overrides failed dispatch ownership; RF remains incomplete; no second connection;acceptance=0");return 0;
 }
 if(trial_case==29){
  assert(!good && !complete && handoff_injections==1 && child_done==1 && workers==1 &&
   wifi_http_reconnect5.connections==1 && !wifi_http_reconnect5.probe[1].self);
  assert(wifi_fence5_handoff.reason==41 && wifi_fence5_handoff.event_valid &&
   (wifi_fence5_handoff.event_control>>24)==10 && !wifi_fence5_connection_switched);
  for(unsigned i=1;i<=9;i++)assert(xhci.devices[i].hub_status_pending==(i==9));
  puts("xhci-http-reconnect5: PASS eight owned hub completions bounded across wrap; ninth remains retained; no second connection;acceptance=0");return 0;
 }
 if(trial_case==27 || trial_case==28 || trial_case==30){
  assert(handoff_injections==1 && !wifi_fence5_handoff.captured);
  for(unsigned i=1;i<=(trial_case==28?8u:1u);i++)assert(!xhci.devices[i].hub_status_pending &&
   !xhci.devices[i].control_pending && xhci.devices[i].control_completion==1 && xhci.devices[i].hub_poll_port==2);
 }
 if((trial_case>=18 && trial_case<24) || trial_case==26){
  static const unsigned reasons[]={41,41,29,39,42,43};
  assert(!good && !complete && handoff_injections==1 && wifi_http_reconnect5.connections==1);
  assert(workers==1 && child_count==1 && child_done==1 && !wifi_http_reconnect5.probe[1].self);
  assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[0].result.quiescent);
  assert(wifi_fence5_handoff.captured && wifi_fence5_handoff.reason==(trial_case==26?41:reasons[trial_case-18]));
  assert(!wifi_fence5_connection_switched && wifi_fence5_active==&wifi_fence5_first &&
   wifi_stream5_current==&wifi_stream5_slots[0] && !wifi_fence5_next_owner);
  assert(wifi_stream5.exchange.keys_erased && wifi_stream5.retired && wifi_stack5.detached);
  if(trial_case==18 || trial_case==19){
   assert(wifi_fence5_handoff.event_valid && !wifi_fence5_empty());
   assert(((wifi_fence5_handoff.event_control>>10)&63)==(trial_case==18?34u:32u));
   assert(wifi_fence5_handoff.event_owner==(trial_case==18?0u:3u));
  }
  if(trial_case==26){
   assert(wifi_fence5_handoff.event_valid && !wifi_fence5_empty());
   assert(wifi_fence5_handoff.event_owner==0 &&
    (wifi_fence5_handoff.event_control>>24)==2 &&
    ((wifi_fence5_handoff.event_control>>16)&31)==1 &&
    ((wifi_fence5_handoff.event_control>>10)&63)==32 &&
    (wifi_fence5_handoff.event_status>>24)==1);
  }
  if(trial_case==23)assert(wifi_reconnect5_closed_reason==14);
  unsigned reason=wifi_fence5_handoff.reason;wifi_fence5_handoff_capture(999);
  assert(wifi_fence5_handoff.reason==reason); /* later observation cannot replace first cause */
  if(trial_case==18){
   /* After the actual supervisor refusal, exercise the same retained native
    * objects at each post-callback check. Only the modeled fixture rolls back
    * mutations between independent counterexamples; production never retries. */
   test_console_printf_hook=NULL;
   xhci.event_ring[xhci.event_dequeue].control=xhci.event_cycle?0u:1u;
   assert(wifi_reconnect5_closed(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma));
   struct xhci_trb *event_ring=xhci.event_ring;
   xhci.command_pending=true;xhci.event_ring=(struct xhci_trb *)(uintptr_t)1;
   memset(&wifi_fence5_handoff,0,sizeof wifi_fence5_handoff);
   assert(!wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,
    &wifi_stream5_slots[1],wifi_stream5.epoch+1,wifi_reconnect5_closed));
   assert(wifi_fence5_handoff.reason==29 && !wifi_fence5_handoff.event_valid);
   xhci.command_pending=false;xhci.event_ring=event_ring;
   static const unsigned expected[]={56,44,45,51,54,43,56};
   for(handoff_recheck_mode=0;handoff_recheck_mode<7;handoff_recheck_mode++){
    struct wifi_fence5_native_state saved=wifi_fence5_first;
    struct mt76_dma5 dma=wifi_stream5.dma;unsigned frontier=xhci.command_enqueue;uint64_t users=xhci.sync_users;
    memset(&wifi_fence5_handoff,0,sizeof wifi_fence5_handoff);
    assert(!wifi_fence5_connection_next(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,
     &wifi_stream5_slots[1],wifi_stream5.epoch+1,handoff_recheck));
    assert(wifi_fence5_handoff.reason==expected[handoff_recheck_mode] &&
     !wifi_fence5_connection_switched && wifi_fence5_active==&wifi_fence5_first && !wifi_fence5_next_owner);
    wifi_fence5_first=saved;wifi_stream5.dma=dma;xhci.command_enqueue=frontier;xhci.sync_users=users;
    xhci.event_ring[xhci.event_dequeue].control=xhci.event_cycle?0u:1u;
   }
  }
  printf("xhci-http-reconnect5: PASS boundary case%u reason%u; first actual HTTP complete, no second owner/DMA/child; event retained; acceptance=0\n",trial_case,reason);return 0;
 }
 if(trial_case==15){
  const struct http_network_probe *p=&wifi_http_reconnect5.probe[0];
  assert(!good && !complete && wifi_http_reconnect5.parent_complete && !wifi_http_reconnect5.connections);
  assert(!p->self && !p->result.attempted && !p->result.go && !p->bootstrap.identity &&
   !wifi_stream5.attempted && !wifi_stack5_net.identity && !child_count && !workers &&
   !wifi_http_reconnect5.probe[1].self);
  puts("xhci-http-reconnect5: PASS missing target never creates child/worker or consumes pre-GO budget;no live stream/GO or second generation;acceptance=0");return 0;
 }
 if(trial_case==2 || trial_case==12 || trial_case==14 || trial_case==16 || trial_case==17 || trial_case==24){
  const struct http_network_probe *p=&wifi_http_reconnect5.probe[0];const struct http_network_result *r=&p->result;
  assert(!good && !complete && !wifi_http_reconnect5.connections && child_count==1 && !child_done && reconnect_hook_count==1);
  assert(r->reason==(trial_case==14?HTTP_NET_DNS_FAILED:HTTP_NET_DHCP_FAILED) && r->phase==(trial_case==14?HTTP_NET_DNS:HTTP_NET_DHCP) && !r->complete && r->dhcp==(trial_case==14) && !r->handoff && !r->go);
  assert(r->quiescent && r->child_retired && r->worker_finished && r->tcp_empty && r->provider_drained && r->receiver_closed && r->refs_scrubbed && r->disconnected);
  assert(p->bootstrap.failed && p->bootstrap.protocol_failed && p->bootstrap.closed && p->bootstrap.disconnected);
  if(trial_case==24){
   const struct net_dhcp_observation *o=&p->bootstrap.dhcp_observation;
   assert(r->child_status==137);
   assert(o->accepted_offer==1 && o->requests==1 && o->request_queued==1 && o->request_sent && !o->acks && !o->accepted_ack && !o->rejected);
   assert(wifi_stream5.data_sends==2 && ap.dhcp==2 && !ap.ack_noise);
   assert(wifi_stream5.data_received==2 && !wifi_fence5_handoff.captured);
   unsigned client=0,server=0,ignored=0;
   for(unsigned i=0;i<wifi_stream5.record_count;i++){
    const struct wifi_stream5_record *r=&wifi_stream5.records[i];
    if(!r->direction && r->kind==5){assert(r->packet.role==4 && r->packet.message==(!client?1u:3u) && r->sequence==9+client);client++;}
    if(r->direction && r->ethernet){assert((r->packet.role==4 && r->packet.message==1) || (r->packet.role==5 && r->packet.message==2));server++;}
    if(r->direction && r->reason==4)ignored++;
   }
   assert(client==2 && server==2 && ignored==68);
  }
  assert(p->bootstrap.dhcp_observation.end_reason==(trial_case==14?NET_DHCP_SUCCESS:NET_DHCP_TIME_LIMIT) && p->bootstrap.dhcp_observation.finished);
  assert(r->elapsed_us>=(trial_case==14?3000000u:2000000u) && r->elapsed_us<5000000 && !children[0].go);
  assert(wifi_stream5_slots[0].exchange.keys_erased && wifi_stream5_slots[0].exchange.leave_sent && !wifi_stream5_slots[0].exchange.handshake.keys_may_live);
  assert(wifi_stream5_slots[0].retired && wifi_stream5_slots[0].connection_closed && wifi_stream5_slots[0].restored && wifi_stack5.detached && wifi_stack5_net.state==MT76_NET5_DOWN);
  assert(wifi_http_reconnect5.failure_closed && wifi_http_reconnect5.parent_complete==(trial_case!=17));
  assert(wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));
  assert(!wifi_reconnect5_closed(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma));
  wifi_stream5.events_empty=false;assert(!wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));wifi_stream5.events_empty=true;
  wifi_stack5_net.callbacks=1;assert(!wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));wifi_stack5_net.callbacks=0;
  /* Every relaxation is scoped to the same failed, detached lifetime. */
#define REFUSES(field,value) do{__typeof__(field) saved=(field);(field)=(value);assert(!wifi_reconnect5_closed_state(&wifi_stream5,wifi_stream5.epoch,&wifi_stream5.dma,true));(field)=saved;}while(0)
  REFUSES(wifi_stack5.terminal_epoch,wifi_stream5.epoch+1);
  REFUSES(wifi_stack5.detached,false);REFUSES(wifi_stack5.terminal_handoff,false);
  REFUSES(wifi_stream5.exchange.keys_erased,false);REFUSES(wifi_stream5.exchange.unsafe,true);
  REFUSES(wifi_stream5.exchange.handshake.keys_may_live,true);
  REFUSES(wifi_stream5.pending[0],true);REFUSES(wifi_stream5.cached_present,true);
  REFUSES(wifi_stream5.loan.ticket,1);REFUSES(wifi_stream5.dma.borrowed,wifi_stream5.dma.released+1);
  REFUSES(wifi_stream5.stop_events[1],0);REFUSES(wifi_stream5.dma.faults,1);
  REFUSES(wifi_http_reconnect5.probe[0].guard_failed,true);
  REFUSES(wifi_http_reconnect5.probe[0].result.go,true);
  REFUSES(wifi_http_reconnect5.probe[0].worker.ever_active,true);
#undef REFUSES
  assert(!xhci_wifi_capture_http_reconnect5());
  assert(!wifi_http5_held_state.frozen && !net_prepare_connection_probe_worker());
  printf("xhci-http-reconnect5: PASS case%u actual native bootstrap failure, unused child/worker and protocol closure, consuming raw RX handoff, key erase/leave/exact Stop/DMA retirement/provider detach;original deadlines;no second generation;acceptance=0\n",trial_case);return 0;
 }
 if(trial_case>=6 && trial_case<12){
  const struct wifi_stream5_state *s=&wifi_stream5_slots[1];
  assert(!good && !complete && wifi_http_reconnect5.connections==1 && child_done==1);
  assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[1].result.quiescent && !wifi_http_reconnect5.probe[1].result.attempted);
  assert(wifi_http_reconnect5.probe[1].result.child_retired && wifi_http_reconnect5.probe[1].result.worker_finished);
  assert(s->posts[0]==1 && s->completions[0]==1 && !s->fault_event_valid);
  assert(s->exchange.auth_sent && !s->exchange.auth_matched && !s->exchange.assoc_sent && !s->exchange.keys_activated && s->exchange.keys_erased && !s->exchange.leave_sent);
  assert(s->auth_observation.present==25 && !s->auth_observation.peer && !s->auth_observation.invalid && !s->auth_observation.rejected);
  assert(s->auth_observation.last-s->auth_observation.first>=750000 && s->auth_observation.last-s->auth_observation.first<755000);
  assert(s->auth_radio.complete[0] && s->auth_radio.empty[0]==(trial_case!=9));
  if(trial_case==10){assert(s->failed && !s->auth_radio.complete[1] && !s->restored && !s->stop_events[0] && !s->stop_events[1]);}
  else{assert(!s->failed && s->phase==9 && s->auth_radio.complete[1] && s->auth_radio.empty[1]);}
  if(trial_case>=7){
   assert(!radio_sim.ext_read && radio_sim.pop[0]==(trial_case==9?17u:1u));
   if(trial_case!=10){assert(radio_sim.pop[1]==(trial_case==11?1u:2u));}
   if(trial_case==7 || trial_case==9)assert(s->auth_radio.fifo[1][0]==0x2000ffa1);
   if(trial_case==8)assert(s->auth_radio.fifo[1][0]==0x2000ff81);
  }
  assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream));
  printf("xhci-http-reconnect5: PASS case%u first HTTP success/second AUTH miss with25 unrelated frames; original750ms budget; independent radio ACK/no-ACK/absent/saturated/control-failure outcomes never authorize IP;unused child/worker retired;acceptance=0\n",trial_case);return 0;
 }
 if(trial_case==1 || trial_case==3){
  assert(!good && !complete && wifi_http_reconnect5.connections==1 && child_done==1);
  assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[1].result.quiescent && !wifi_http_reconnect5.probe[1].result.attempted);
  assert(wifi_http_reconnect5.probe[1].result.child_retired && !wifi_stream5_slots[1].exchange.handshake.keys_may_live);
  assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream) || trial_case==3);
  printf("xhci-http-reconnect5: PASS case%u safe second preparation/handshake refusal, first actual HTTP complete;unused child/worker retired;acceptance=0\n",trial_case);return 0;
 }
 if(trial_case==4){
  const struct http_network_result *r=&wifi_http_reconnect5.probe[0].result;
  assert(!good && !complete && !wifi_http_reconnect5.connections && child_done==1);
  assert(r->reason==HTTP_NET_CHILD_FAILED && r->child_status==1 && !r->complete);
  assert(r->disconnected && r->quiescent && r->child_retired && r->worker_finished && r->tcp_empty && r->provider_drained && r->receiver_closed && r->refs_scrubbed);
  assert(wifi_stream5_slots[0].exchange.keys_erased && !wifi_http_reconnect5.probe[1].self);
  puts("xhci-http-reconnect5: PASS modeled nonzero child exit after real TCP/EOF safely retires first lifetime;no second connection;acceptance=0");return 0;
 }
 if(trial_case==25){
  for(unsigned g=0;g<2;g++){
   const struct wifi_stream5_state *s=&wifi_stream5_slots[g];unsigned offers=0,acks=0,reflected=0;
   uint64_t request=0,received=0;
   for(unsigned i=0;i<s->record_count;i++){
    const struct wifi_stream5_record *r=&s->records[i];
    if(!r->direction && r->packet.message==3)request=r->finished_us;
    if(r->direction && r->packet.role==4 && r->packet.message==1)reflected++;
    if(r->direction && r->packet.role==5){offers+=r->packet.message==2;if(r->packet.message==5){acks++;received=r->finished_us;}}
   }
   assert(offers==1 && acks==1 && reflected==1 && request && received>request && received-request>=300000);
  }
 }
 assert(complete);
 assert(workers==2 && child_count==2 && child_done==2 && reconnect_hook_count==2);
 for(unsigned g=0;g<2;g++){
  const struct http_network_result *r=&wifi_http_reconnect5.probe[g].result;
  assert(r->complete && r->disconnected && r->child_retired && r->worker_finished && r->receiver_closed && r->refs_scrubbed);
  assert(children[g].offset==4159 && children[g].eof && children[g].parser.body_bytes==3072);
  assert(wifi_stream5_slots[g].exchange.keys_erased && wifi_stream5_slots[g].retired && wifi_stream5_slots[g].connection_closed);
 }
 assert(wifi_stack5_net.state==MT76_NET5_RETIRED && wifi_stack5_net.registry_drained);
 assert(reconnect_keys_seen[0] && reconnect_keys_seen[1]);
 assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream));
 assert(!memcmp(&reconnect_old_fence,&wifi_fence5_first,sizeof reconnect_old_fence));
 for(unsigned i=0;i<10;i++)assert(!memcmp(reconnect_old_physical[i],i<2?(void *)wifi_stream5_slots[0].ring[i]:(void *)wifi_stream5_slots[0].buffer[(i-2)/4][(i-2)%4],4096));
 assert(wifi_http_reconnect5.epochs[1]>wifi_http_reconnect5.epochs[0] && wifi_http_reconnect5.tokens[1]>wifi_http_reconnect5.tokens[0]);
 assert(!xhci_wifi_capture_http_reconnect5() && !xhci_wifi_http_reconnect5_prepare());
 puts("xhci-http-reconnect5: PASS two fresh native USB/HTTP connection paths, actual production net/TCP/worker/CCMP, two4159-byte requests/exact3072-byte bodies/genuine EOF and complete native cleanup;modeled scheduler+child+USB;acceptance=0");
 return 0;
}
