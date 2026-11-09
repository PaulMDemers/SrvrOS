/* Actual cal5 ring/publication/event/Stop/held helpers and MAC drain engine.
 * The already-completed RF/calibration/eight-frame parent and USB register/
 * synchronous command boundaries are modeled. No credentials/native authority. */
#include <stdbool.h>
#include <stdint.h>
struct xhci_device;struct xhci_trb;
static bool cal5_drain_test_parent(void);
static void cal5_drain_test_progress(void);
static bool cal5_drain_test_usb(struct xhci_device *,bool,uint32_t,uint32_t *);
static bool cal5_drain_test_delay(unsigned);
static bool cal5_drain_test_command(const struct xhci_trb *,unsigned,uint64_t *);
#define SRVROS_XHCI_CAL5_DRAIN_TEST
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main legacy_xhci_main
#include "test_xhci.c"
#undef main
#pragma GCC diagnostic pop
static _Alignas(4096) struct xhci_trb cal_commands[64],cal_events[XHCI_EVENT_TRB_COUNT];
static uint8_t forward_copy[8][4096];
static struct {unsigned queue,event_fault,stop_fault,span,delays,reads,empty_stop_mode;bool parent,inject_in_control,in_control;uint32_t mac,filter,txop;} sim;
static bool cal5_drain_test_parent(void){return sim.parent;}
static bool cal5_drain_test_delay(unsigned us){sim.delays++;test_time+=us;return true;}
static void event_queue(struct xhci_trb e){
 e.control=(e.control&~1u)|(xhci.event_cycle?1u:0u);
 cal_events[xhci.event_dequeue]=e;
}
static void cal5_drain_test_progress(void){
 if(!wifi_cal5.cleanup_pending || !sim.queue || wifi_cal5.cleanup_failed || (sim.inject_in_control && !sim.in_control))return;
 unsigned i=wifi_cal5.cleanup_posts-1;uint64_t p=wifi_cal5.plan.ring+16*(8+i);
 memset(wifi_cal5.cleanup_buffer[i],0x31+i,4096);sim.queue--;
 context_write32(wifi_cal5.d->output_context,9,2,(p+16)|1);context_write32(wifi_cal5.d->output_context,9,3,(p+16)>>32);
 struct xhci_trb e={.parameter_low=p,.parameter_high=p>>32,.status=(13u<<24)|4032,.control=(32u<<10)|(9u<<16)|((uint32_t)wifi_cal5.d->slot_id<<24)};
 switch(sim.event_fault){case 1:e.parameter_low-=16;break;case 2:e.control^=1u<<24;break;case 3:e.control^=1u<<16;break;case 4:e.control|=4;break;case 5:e.status=2u<<24;break;case 6:e.status=(13u<<24)|4097;break;case 7:wifi_cal5.ep.ring[8+i].parameter_low^=4096;break;default:break;}
 event_queue(e);
}
static bool cal5_drain_test_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==wifi_cal5.d && v);sim.reads++;
 if(sim.inject_in_control && wifi_cal5.cleanup_pending && sim.queue && !wifi_cal5.cleanup_failed){
  d->control_pending=true;xhci.sync_users=1;strict_control.device=d;strict_control.length=4;strict_control.setup=d->ep0_ring_phys;strict_control.data_in=(wifi_cal5.cleanup_control_setup&0x80)!=0;strict_control.done=false;
  d->ep0_ring[0]=(struct xhci_trb){.parameter_low=wifi_cal5.cleanup_control_setup,.parameter_high=wifi_cal5.cleanup_control_setup>>32,.status=8,.control=(XHCI_TRB_TYPE_SETUP_STAGE<<10)|1};
  if(sim.span==1){strict_control.done=true;strict_control.device=NULL;xhci.sync_users=0;}
  if(sim.span==2)d->ep0_ring[0].parameter_low^=1;
  if(sim.span==3)strict_control.device=&xhci.devices[1];
  if(sim.span==4)xhci.sync_users=2;
  sim.in_control=true;cal5_drain_test_progress();sim.in_control=false;struct xhci_trb e;assert(poll_event(&e));dispatch_transfer_event(&e);
  strict_control.device=NULL;strict_control.done=true;d->control_pending=false;xhci.sync_users=0;
 }
 if(write){if(a==0x1004){}else if(a==0x1400 || a==0x1340){}else assert(!"unexpected write");}
 if(write){if(a==0x1004)sim.mac=*v;else if(a==0x1400)sim.filter=*v;else if(a==0x1340)sim.txop=*v;return true;}
 switch(a){case 0x1004:*v=sim.mac;break;case 0x2000:*v=0xf000f200;break;case 0x208:*v=0x40000050;break;case 0x1400:*v=sim.filter;break;case 0x1340:*v=sim.txop;break;case 0x13b0:*v=0x2f2f1b1b;break;case 0x2124:*v=UINT32_MAX;break;case 0x430:*v=0x22;break;case 0xa30:*v=sim.queue<<8;break;case 0x438:case 0xa34:case 0x1200:*v=0;break;default:assert(!"unexpected read");}
 return true;
}
static bool cal5_drain_test_command(const struct xhci_trb *cmd,unsigned type,uint64_t *slot){
 assert(type==15 && firmware_command_exact && wifi_cal5.stopping);struct xhci_device *d=wifi_cal5.d;unsigned index=xhci.command_enqueue;
 assert(index<63);cal_commands[index]=*cmd;cal_commands[index].control|=xhci.command_cycle?1:0;xhci.command_enqueue++;xhci.command_pending=true;xhci.sync_users=1;
 unsigned total=wifi_cal5.posts+wifi_cal5.cleanup_posts;uint64_t p=wifi_cal5.plan.ring+16*(total-(wifi_cal5.cleanup_pending?1:0));
 context_write32(d->output_context,9,0,3);context_write32(d->output_context,9,2,p|1);context_write32(d->output_context,9,3,p>>32);
 struct xhci_trb e={.parameter_low=p,.parameter_high=p>>32,.status=(wifi_cal5.cleanup_pending?26u:27u)<<24,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
 if(wifi_cal5.cleanup_pending)e.status|=4096;
 switch(sim.stop_fault){case 1:e.parameter_low+=16;break;case 2:e.control^=1u<<24;break;case 3:e.control^=1u<<16;break;case 4:e.status=2u<<24;break;case 5:e.control|=4;break;case 6:e.status=(26u<<24)|4097;break;case 7:context_write32(d->output_context,9,2,p);break;case 8:context_write32(d->output_context,9,0,1);break;case 9:cal_commands[index].control^=1u<<10;break;case 10:xhci.sync_users=2;break;default:break;}
 /* Model Transfer Event before the Stop command finalizes its output context. */
 if(sim.empty_stop_mode==1 || sim.empty_stop_mode==2){
  context_write32(d->output_context,9,0,1);
  context_write32(d->output_context,9,2,(wifi_cal5.plan.ring+7*16)|1);
 }
 e.control|=xhci.event_cycle?1:0;
 if(sim.empty_stop_mode!=4)dispatch_transfer_event(&e);
 if(sim.empty_stop_mode==3)dispatch_transfer_event(&e);
 if(sim.empty_stop_mode==1){context_write32(d->output_context,9,0,3);context_write32(d->output_context,9,2,p|1);}
 if(sim.empty_stop_mode==5)context_write32(d->output_context,9,2,(p-16)|1);
 if(sim.empty_stop_mode==6){xhci.command_pending=false;xhci.sync_users=0;dispatch_transfer_event(&e);}
 if(sim.empty_stop_mode==7){xhci.command_failed=true;xhci.command_pending=false;xhci.sync_users=0;return false;}
 xhci.command_pending=false;xhci.sync_users=0;*slot=d->slot_id;return true;
}
static void setup(bool enabled,unsigned queued){
 reset_test();memset(&wifi_cal5,0,sizeof wifi_cal5);memset(&wifi_stream5,0,sizeof wifi_stream5);memset(&wifi_join5,0,sizeof wifi_join5);memset(&wifi_wpa5,0,sizeof wifi_wpa5);memset(&wifi_fence5_native,0,sizeof wifi_fence5_native);
 memset(&sim,0,sizeof sim);sim.parent=true;sim.queue=queued;sim.filter=0x17f97;sim.txop=0x583f;test_time=0;test_time_step=3;
 memset(cal_commands,0,sizeof cal_commands);memset(cal_events,0,sizeof cal_events);
 xhci.present=xhci.operational=xhci.controller_owned=true;xhci.command_ring=cal_commands;xhci.command_ring_phys=(uint64_t)cal_commands;xhci.command_cycle=true;
 xhci.event_ring=cal_events;xhci.event_ring_phys=(uint64_t)cal_events;xhci.event_cycle=true;xhci.op_base=0x100;xhci.db_base=0x4000;xhci.rt_base=0x3000;
 struct xhci_device *d=&xhci.devices[0];*d=(struct xhci_device){.present=true,.slot_id=1,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100};void *v;
 assert(alloc_frame_zero(&d->input_context_phys,&v));d->input_context=v;assert(alloc_frame_zero(&d->output_context_phys,&v));d->output_context=v;
 assert(alloc_frame_zero(&d->ep0_ring_phys,&v));d->ep0_ring=v;assert(alloc_frame_zero(&d->descriptor_buffer_phys,&v));d->descriptor_buffer=v;
 assert(alloc_frame_zero(&d->firmware.buffer_phys,&v));assert(alloc_frame_zero(&d->firmware.ep.ring_phys,&v));assert(alloc_frame_zero(&d->firmware.mcu.buffer_phys,&v));assert(alloc_frame_zero(&d->firmware.mcu.response.ring_phys,&v));d->firmware.mcu.configured=true;
 if(enabled){assert(xhci_wifi_cal5_cleanup_enable());assert(!xhci_wifi_cal5_cleanup_enable());}
 wifi_cal5_requested=true;wifi_cal5.d=d;wifi_cal5.active=wifi_cal5.attempted=wifi_cal5.configured=true;
 wifi_cal5.command_ring=xhci.command_ring_phys;wifi_cal5.input=d->input_context_phys;wifi_cal5.output=d->output_context_phys;wifi_cal5.ep0=d->ep0_ring_phys;wifi_cal5.descriptor=d->descriptor_buffer_phys;
 assert(alloc_frame_zero(&wifi_cal5.plan.ring,&v));wifi_cal5.ep.ring=v;wifi_cal5.ep.ring_phys=wifi_cal5.pin_ring=wifi_cal5.plan.ring;wifi_cal5.pin_rptr=(uintptr_t)v;wifi_cal5.ep.cycle=true;wifi_cal5.ep.dci=9;wifi_cal5.ep.packet=512;write_link_trb(v,wifi_cal5.plan.ring,true);
 wifi_cal5.plan.slot=1;wifi_cal5.plan.dci=9;wifi_cal5.plan.country[0]='U';wifi_cal5.plan.country[1]='S';wifi_cal5.plan.channel=161;wifi_cal5.plan.budget_us=10000000;
 for(unsigned i=0;i<8;i++){assert(alloc_frame_zero(&wifi_cal5.plan.physical[i],&v));wifi_cal5.plan.buffer[i]=v;wifi_cal5.pin_pages[i]=wifi_cal5.plan.physical[i];wifi_cal5.pin_v[i]=(uintptr_t)v;memset(v,0x61+i,4096);memcpy(forward_copy[i],v,4096);}
 if(enabled)for(unsigned i=0;i<8;i++){assert(alloc_frame_zero(&wifi_cal5.cleanup_physical[i],&v));wifi_cal5.cleanup_buffer[i]=v;wifi_cal5.cleanup_pin[i]=(uintptr_t)v;}
 context_write32(d->output_context,9,0,1);context_write32(d->output_context,9,1,(3u<<1)|(6u<<3)|(512u<<16));context_write32(d->output_context,9,2,wifi_cal5.plan.ring|1);context_write32(d->output_context,9,3,wifi_cal5.plan.ring>>32);context_write32(d->output_context,9,4,4096);
 wifi_cal5.receiving=true;
 for(unsigned i=0;i<8;i++){
  assert(wifi_cal5_post(&wifi_cal5,i,wifi_cal5.plan.ring+16*i,wifi_cal5.plan.physical[i]));
  context_write32(d->output_context,9,2,(wifi_cal5.plan.ring+16*(i+1))|1);context_write32(d->output_context,9,3,(wifi_cal5.plan.ring+16*(i+1))>>32);
  uint64_t p=wifi_cal5.plan.ring+16*i;struct xhci_trb e={.parameter_low=p,.parameter_high=p>>32,.status=(13u<<24)|4032,.control=(32u<<10)|(9u<<16)|(1u<<24)};dispatch_transfer_event(&e);assert(wifi_cal5.done && !wifi_cal5.pending);
 }
 wifi_cal5.receiving=false;wifi_cal5.window.attempted=wifi_cal5.window.polled=wifi_cal5.window.poll_ok=true;wifi_cal5.window.attempts=wifi_cal5.window.completions=8;wifi_cal5.operations=47;
 assert(wifi_cal5_owned(&wifi_cal5,false));assert(!xhci_wifi_cal5_cleanup_enable());
}
static bool inspected(void *context,uint32_t alc,uint32_t ibi){assert(context==&wifi_cal5 && alc==0x2f2f1b1b && ibi==UINT32_MAX);return true;}
static bool run(void){
 const struct mt76_mac_cycle_io io={&wifi_cal5,wifi_cal5_owned,wifi_cal5_read,wifi_cal5_write,wifi_cal5_delay,wifi_cal5_now,inspected};
 bool ok=mt76_mac_cycle_run(&wifi_cal5.mac,&io);bool stopped=wifi_cal5_stop();wifi_cal5_cleanup_emit();
 for(unsigned i=0;i<8;i++)assert(!memcmp(forward_copy[i],wifi_cal5.plan.buffer[i],4096));
 assert(!freed);return ok && stopped && wifi_cal5_cleanup_held();
}
static unsigned finish_calls,finish_fault_at,finish_fault_kind;
static bool finish_guard(void *context,bool cleanup){
 assert(context==&wifi_cal5 && cleanup);
 if(++finish_calls==finish_fault_at){
  if(finish_fault_kind==1)wifi_cal5.cleanup_buffer[0][11]^=1;
  else if(finish_fault_kind==2)wifi_cal5.failed=true;
  else {uint64_t p=wifi_cal5.plan.ring+8*16;struct xhci_trb e={.parameter_low=p,.parameter_high=p>>32,.status=13u<<24,.control=(32u<<10)|(9u<<16)|(1u<<24)};dispatch_transfer_event(&e);}
 }
 return wifi_cal5_finish_owned(context,cleanup);
}
static void finish_capture(void){
 wifi_cal5.window.plan=wifi_cal5.plan;
 for(unsigned i=0;i<8;i++)wifi_cal5.window.item[i]=(struct mt76_rx_window_item){.submitted=true,.completed=true,.completion=13,.remaining=4032,.actual=64};
 const struct mt76_rx_window_io io={.context=&wifi_cal5,.owned=finish_guard};
 const struct mt76_rx_window_stop stop={.dequeue=wifi_cal5.stop_dequeue,.slot=wifi_cal5.d->slot_id,.dci=9,.state=wifi_cal5.stop_state,.cleanup_posts=wifi_cal5.cleanup_posts};
 bool done=mt76_rx5_window_finish(&wifi_cal5.window,&io,&stop);assert(done==!finish_fault_at);
 for(unsigned i=0;i<8;i++)assert(wifi_cal5.window.item[i].inspected==(!finish_fault_at || i+1<finish_fault_at));
 assert(!freed);
}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"--trace-empty")){setup(true,0);sim.empty_stop_mode=1;batch_trace=true;console_write("wifi-cal5-cleanup-plan: forward=8 reserved=8\n");assert(run());return 0;}
 if(argc==2 && !strcmp(argv[1],"--trace")){setup(true,5);batch_trace=true;console_write("wifi-cal5-cleanup-plan: forward=8 reserved=8\n");assert(run());return 0;}
 if(argc==2 && !strcmp(argv[1],"--size-projection")){
  setup(true,8);assert(run());for(unsigned i=0;i<8;i++)wifi_cal5.cleanup_events[i].status=(13u<<24)|4096;
  wifi_cal5.cleanup_stop_event.parameter_low=wifi_cal5.cleanup_stop_event.parameter_high=UINT32_MAX;wifi_cal5.cleanup_stop_event.status=wifi_cal5.cleanup_stop_event.control=UINT32_MAX;wifi_cal5.stop_dequeue=UINT64_MAX;
  batch_trace=true;console_write("wifi-cal5-cleanup-plan: forward=8 reserved=8\n");wifi_cal5_cleanup_emit();return 0;
 }
 assert(argc==1);
 setup(false,5);assert(!run() && !wifi_cal5.mac.rx_drained && wifi_cal5.mac.rx_rounds==200 && !wifi_cal5.cleanup_posts && sim.queue==5);
 for(unsigned n=0;n<=9;n++){setup(true,n);bool ok=run();assert(ok==(n<=8));assert(wifi_cal5.cleanup_posts<=8 && wifi_cal5.cleanup_completed<=8);if(ok){assert(!sim.queue && wifi_cal5.mac.rx_drained && wifi_cal5.mac.rx_rounds>=7 && wifi_cal5.mac.rx_rounds<=200 && wifi_cal5.cleanup_frozen);for(unsigned i=0;i<8;i++)assert(wifi_cal5_cleanup_alias(wifi_cal5.cleanup_physical[i]));}}
 /* Empty Stop events carry no payload. Defer final context proof, never
  * duplicate/omit it or accept failed command/state/frontier outcomes. */
 for(unsigned mode=1;mode<=7;mode++){setup(true,0);sim.empty_stop_mode=mode;assert(run()==(mode==1));if(mode!=1)assert(!wifi_cal5.cleanup_frozen && !wifi_cal5.stopped && !freed);}
 for(unsigned fault=1;fault<=10;fault++){setup(true,0);sim.stop_fault=fault;assert(!run() && !wifi_cal5.cleanup_frozen && !freed);}
 for(unsigned span=0;span<=4;span++){setup(true,5);sim.inject_in_control=true;sim.span=span;bool ok=run();assert(ok==(span<2));if(!ok)assert(wifi_cal5.cleanup_failed && !wifi_cal5.cleanup_frozen);}
 for(unsigned fault=1;fault<=7;fault++){setup(true,5);sim.event_fault=fault;assert(!run() && wifi_cal5.cleanup_failed && !wifi_cal5.cleanup_frozen && !freed);}
 for(unsigned fault=1;fault<=10;fault++){setup(true,1);sim.stop_fault=fault;assert(!run() && !wifi_cal5.cleanup_frozen && !freed);}
 setup(true,1);assert(run());uint64_t ptr=wifi_cal5.plan.ring+8*16;struct xhci_trb e={.parameter_low=ptr,.parameter_high=ptr>>32,.status=13u<<24,.control=(32u<<10)|(9u<<16)|(1u<<24)};dispatch_transfer_event(&e);assert(wifi_cal5.cleanup_failed && !wifi_cal5_cleanup_held());
 setup(true,0);ptr=wifi_cal5.plan.ring+16*16;e=(struct xhci_trb){.parameter_low=ptr,.parameter_high=ptr>>32,.status=13u<<24,.control=(32u<<10)|(9u<<16)|(1u<<24)};dispatch_transfer_event(&e);assert(wifi_cal5.cleanup_failed && !wifi_cal5.cleanup_posts);
 setup(true,3);assert(run());wifi_cal5.cleanup_buffer[7][3]^=1;assert(!wifi_cal5_cleanup_held());
 setup(true,0);wifi_cal5.cleanup_physical[0]=wifi_cal5.plan.physical[0];assert(!wifi_cal5_owned(&wifi_cal5,false) && !wifi_cal5.cleanup_posts);
 /* A fault after Stop must invalidate first8 payload admission at every
  * item boundary,while independent hardware teardown admission stays intact. */
 for(unsigned kind=0;kind<3;kind++)for(unsigned at=1;at<=8;at++){
  setup(true,3);assert(run());finish_calls=0;finish_fault_at=at;finish_fault_kind=kind;finish_capture();
 }
 setup(true,0);assert(run());finish_calls=finish_fault_at=0;finish_capture();
 setup(false,0);assert(run());finish_calls=finish_fault_at=0;finish_capture();
 puts("xhci-cal5-cleanup: PASS actual finite8scratch append/200round7quiet drain,original8 immutable,real eventdispatcher,exactStop/deferred emptyCC27 context proof and ownEP0 completion spans,normal/stopped distinction,queue/reserve exhaustion,misroute/ED/CC/residue/late/alias/held faults;modeled heldparent/USB,native unaccepted");return 0;
}
