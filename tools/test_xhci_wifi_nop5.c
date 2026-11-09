#include <stdbool.h>
#include <stdint.h>
/* Actual child,ledger,fence and backend;mock MMIO/parent/three legacy sync
 * commands. No native acceptance is conferred by this host fixture. */
#define SRVROS_XHCI_NOP5_TEST
struct xhci_trb;
static bool xhci_test_nop5_parent_owned(void);
static bool xhci_test_nop5_guard(uint32_t,uint32_t *);
static bool xhci_test_nop5_command(const struct xhci_trb *,unsigned,uint64_t *);
static void xhci_test_nop5_progress(void);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main legacy_xhci_main
#include "test_xhci.c"
#undef main
#pragma GCC diagnostic pop
static struct pci_device nop_pci={.vendor_id=0x8086,.device_id=0x8c31};
static _Alignas(4096) struct xhci_trb nop_commands[64],nop_events[256];
static unsigned nop_fault,nop_fault_at=1;
static bool nop_parent=true,nop_link;
static bool xhci_test_nop5_parent_owned(void){return nop_parent;}
static bool xhci_test_nop5_guard(uint32_t a,uint32_t *v){*v=a==0x1004?0:a==0x2000?0xf000f200:0x40000050;return nop_fault!=20;}
static void nop_enqueue(const struct xhci_trb *event,unsigned offset){unsigned index=(unsigned)xhci.event_dequeue+offset;bool cycle=xhci.event_cycle;if(index>=XHCI_EVENT_TRB_COUNT){index-=XHCI_EVENT_TRB_COUNT;cycle=!cycle;}xhci.event_ring[index]=*event;xhci.event_ring[index].control=(event->control&~1u)|(cycle?1u:0u);}
static bool xhci_test_nop5_command(const struct xhci_trb *t,unsigned type,uint64_t *slot){
 unsigned i=(unsigned)xhci.command_enqueue;assert(i<63);struct xhci_trb shadow=*t;shadow.control|=xhci.command_cycle?1u:0u;xhci.command_ring[i]=shadow;xhci.command_enqueue++;
 struct xhci_device *d=wifi_nop5.d;*slot=d->slot_id;
 if(type==12){if(context_read32(d->input_context,0,0)&(1u<<8))context_write32(d->output_context,8,0,0);else{for(unsigned j=0;j<8;j++)context_write32(d->output_context,8,j,context_read32(d->input_context,9,j));context_write32(d->output_context,8,0,1);}}
 else{assert(type==15);uint64_t ptr=wifi_nop5.physical+16;context_write32(d->output_context,8,0,3);context_write32(d->output_context,8,2,ptr);context_write32(d->output_context,8,3,ptr>>32);struct xhci_trb stop={.parameter_low=ptr,.parameter_high=ptr>>32,.status=27u<<24,.control=(32u<<10)|(8u<<16)|((uint32_t)d->slot_id<<24)};if(nop_fault==21)stop.status=26u<<24;dispatch_transfer_event(&stop);}
 xhci.command_events++;xhci.last_completion_code=1;xhci.last_slot_id=*slot;xhci.last_command_pointer=xhci.command_ring_phys+16*i;xhci.last_expected_command_pointer=xhci.last_command_pointer;
 if(nop_fault==22)xhci.command_events++;
 return nop_fault!=23;
}
static void xhci_test_nop5_progress(void){
 if(wifi_nop5.pending){struct mt76_dma5_noop_lease *l=&wifi_nop5.lease;struct xhci_trb event={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=1u<<24,.control=(32u<<10)|(8u<<16)|((uint32_t)wifi_nop5.slot<<24)};
  if(wifi_nop5.posts==nop_fault_at)switch(nop_fault){case 1:event.parameter_low+=16;break;case 2:event.control^=1u<<24;break;case 3:event.control^=1u<<16;break;case 4:event.status=13u<<24;break;case 5:event.status|=1;break;case 6:event.control|=4;break;case 7:wifi_nop5.ring[0].status=1;break;case 8:return;case 9:wifi_peer5.binding.generation++;break;case 10:context_write32(wifi_nop5.d->output_context,9,4,1);break;case 11:wifi_cal5.plan.physical[0]^=4096;break;case 12:event.control=(33u<<10)|((uint32_t)wifi_nop5.slot<<24);break;case 13:context_write32(wifi_nop5.d->output_context,8,1,0);break;case 14:wifi_nop5.d->removed=true;break;case 15:wifi_nop5.d->slot_id++;break;case 16:xhci.stopping=true;break;default:break;}
  uint64_t next=wifi_nop5.posts==63?wifi_nop5.physical+63*16:wifi_nop5.physical+16*wifi_nop5.enqueue;
  unsigned cycle=wifi_nop5.posts==63?1u:(unsigned)wifi_nop5.cycle;context_write32(wifi_nop5.d->output_context,8,0,1);context_write32(wifi_nop5.d->output_context,8,2,next|cycle);context_write32(wifi_nop5.d->output_context,8,3,next>>32);nop_enqueue(&event,0);return;
 }
 if(xhci.command_pending && wifi_fence5_native.round){struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];unsigned n=0;
  context_write32(wifi_nop5.d->output_context,8,0,3);
  if(r->id.type==15){uint64_t ptr=wifi_nop5.physical+(nop_link?63*16:0),deq=ptr|(nop_link?1:0);context_write32(wifi_nop5.d->output_context,8,2,deq);context_write32(wifi_nop5.d->output_context,8,3,deq>>32);struct xhci_trb stop={.parameter_low=ptr,.parameter_high=ptr>>32,.status=27u<<24,.control=(32u<<10)|(8u<<16)|((uint32_t)wifi_nop5.slot<<24)};nop_enqueue(&stop,n++);}
  else{context_write32(wifi_nop5.d->output_context,8,2,r->id.dequeue);context_write32(wifi_nop5.d->output_context,8,3,r->id.dequeue>>32);}
  struct xhci_trb cc={.parameter_low=r->id.pointer,.parameter_high=r->id.pointer>>32,.status=1u<<24,.control=(33u<<10)|((uint32_t)wifi_nop5.slot<<24)};if(nop_fault==24)cc.status=5u<<24;nop_enqueue(&cc,n);
 }
}
static void nop_setup(void){
 reset_test();memset(&wifi_nop5,0,sizeof wifi_nop5);memset(&wifi_fence5_native,0,sizeof wifi_fence5_native);memset(&wifi_tx5,0,sizeof wifi_tx5);memset(&wifi_cal5,0,sizeof wifi_cal5);memset(&wifi_peer5,0,sizeof wifi_peer5);memset(&wifi_power5,0,sizeof wifi_power5);memset(&wifi_handoff5,0,sizeof wifi_handoff5);memset(nop_commands,0,sizeof nop_commands);memset(nop_events,0,sizeof nop_events);nop_fault=0;nop_fault_at=1;nop_parent=true;nop_link=false;test_time_step=3;
 xhci.present=xhci.operational=xhci.controller_owned=true;xhci.pci=&nop_pci;xhci.command_ring=nop_commands;xhci.command_ring_phys=(uint64_t)nop_commands;xhci.command_cycle=true;xhci.command_enqueue=12;xhci.event_ring=nop_events;xhci.event_ring_phys=(uint64_t)nop_events;xhci.event_cycle=true;xhci.db_base=0x4000;xhci.rt_base=0x3000;xhci.op_base=0x100;
 struct xhci_device *d=&xhci.devices[0];*d=(struct xhci_device){.present=true,.slot_id=1,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100,.wifi_probe_attempted=true,.wifi_probe_ok=true};
 void *p;assert(alloc_frame_zero(&d->input_context_phys,&p));d->input_context=p;assert(alloc_frame_zero(&d->output_context_phys,&p));d->output_context=p;
 wifi_cal5.d=d;wifi_cal5.plan.slot=d->slot_id;assert(alloc_frame_zero(&wifi_cal5.plan.ring,&p));assert(alloc_frame_zero(&wifi_handoff5.plan.new_ring,&p));assert(alloc_frame_zero(&wifi_tx5.ring_phys,&p));wifi_tx5.ring=p;
 for(unsigned i=0;i<8;i++)assert(alloc_frame_zero(&wifi_cal5.plan.physical[i],&p));
 wifi_tx5.disabled=wifi_tx5.input_restored=true;wifi_tx5.phase=7;wifi_peer5.binding.generation=19;context_write32(d->output_context,0,0,16u<<27);
 for(unsigned i=0;i<4096;i++)((uint8_t *)d->input_context)[i]=(uint8_t)(i+7);
}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"--trace")){nop_setup();wifi_peer5.binding.generation=1;batch_trace=true;assert(wifi_nop5_run());assert(wifi_nop5_finish(true));return 0;}assert(argc==1);
 for(unsigned link=0;link<2;link++){nop_setup();nop_link=link;assert(wifi_nop5_run());assert(wifi_nop5.child_complete && wifi_nop5.posts==64 && wifi_nop5.events==64 && wifi_nop5.commands==3 && wifi_fence5_native.count==2);assert(wifi_nop5_finish(true) && wifi_nop5.retired && !wifi_fence5_native.retired && !wifi_nop5.dma.published);}
 for(unsigned at=1;at<=64;at+=63)for(unsigned i=1;i<=16;i++){nop_setup();nop_fault=i;nop_fault_at=at;if(i==8)test_time_step=100000;assert(!wifi_nop5_run() && wifi_nop5.failed && !wifi_nop5.retired);}
 for(unsigned i=20;i<=24;i++){nop_setup();nop_fault=i;assert(!wifi_nop5_run() && wifi_nop5.failed && !wifi_nop5.retired);}
 nop_setup();nop_fault=1;assert(!wifi_nop5_run() && wifi_nop5.pending && wifi_nop5.fault_event_valid && wifi_nop5.fault_context_valid);assert(!wifi_nop5_finish(false) && wifi_nop5.dma.state==MT76_DMA5_FAULT && !wifi_nop5.retired);
 nop_setup();nop_fault=24;assert(!wifi_nop5_run() && wifi_fence5_native.fault_event_valid && !wifi_fence5_native.retired);assert(!wifi_nop5_finish(false));
 nop_setup();nop_parent=false;assert(!wifi_nop5_run() && !wifi_nop5.attempted);
 nop_setup();xhci.command_enqueue=53;assert(!wifi_nop5_run() && !wifi_nop5.posts);
 nop_setup();assert(wifi_nop5_run());assert(!wifi_nop5_run());assert(!wifi_nop5_finish(false) && !wifi_nop5.retired);
 nop_setup();wifi_wpa5.attempted=true;assert(!wifi_nop5_run() && !wifi_nop5.attempted);wifi_wpa5.attempted=false;
 nop_setup();assert(wifi_nop5_run());wifi_wpa5.attempted=true;assert(!wifi_nop5_finish(true) && !wifi_nop5.retired);wifi_wpa5.attempted=false;
 puts("xhci-nop5: PASS actual63+1 typed NoOps/native asyncfence,Link/baseFSE,exactevents/commandidentity,fullinput/unrelated/parent snapshots,timeout/field/late/ownership failures;mock MMIO/parent,no native acceptance");return 0;
}
