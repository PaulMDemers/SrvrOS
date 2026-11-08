/* Actual dormant native binding/shared dispatcher;mock MMIO,not native proof. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main xhci_legacy_test_main
#include "test_xhci.c"
#undef main
#pragma GCC diagnostic pop
static struct mt76_dma5 fence_dma;
static struct pci_device fence_pci={.vendor_id=0x8086,.device_id=0x8c31};
static _Alignas(4096) struct xhci_trb fence_cmd[256],fence_events[256];
static _Alignas(4096) uint8_t fence_context[4096];
static uint64_t fence_epoch;
static bool fence_owned(void *c,uint64_t epoch,unsigned slot){return c==&fence_epoch && epoch==fence_epoch && slot==7;}
static void fence_setup(void){
 reset_test();memset(&wifi_fence5_native,0,sizeof wifi_fence5_native);memset(&fence_dma,0,sizeof fence_dma);
 memset(fence_cmd,0,sizeof fence_cmd);memset(fence_events,0,sizeof fence_events);memset(fence_context,0,sizeof fence_context);fence_epoch=19;
 xhci.present=xhci.operational=xhci.controller_owned=true;xhci.pci=&fence_pci;xhci.command_ring=fence_cmd;xhci.command_ring_phys=0x100000;xhci.command_cycle=true;xhci.command_enqueue=12;
 xhci.event_ring=fence_events;xhci.event_ring_phys=0x110000;xhci.event_cycle=true;xhci.db_base=0x4000;xhci.rt_base=0x3000;xhci.op_base=0x100;
 struct xhci_device *d=&xhci.devices[0];*d=(struct xhci_device){.present=true,.slot_id=7,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100,.wifi_probe_attempted=true,.wifi_probe_ok=true,.output_context=fence_context,.output_context_phys=0x120000};
 assert(mt76_dma5_init_noop_ring(&fence_dma,fence_epoch,0x130000,63,timer_monotonic_us()));
 context_write32(fence_context,8,0,1);context_write32(fence_context,8,1,(512u<<16)|(2u<<3));context_write32(fence_context,8,2,0x130001);
 for(unsigned i=1;i<=63;i++){
  struct mt76_dma5_noop_lease l;assert(mt76_dma5_noop_prepare(&fence_dma,i,timer_monotonic_us(),&l));assert(mt76_dma5_noop_publish(&fence_dma,&l,timer_monotonic_us()));assert(mt76_dma5_noop_complete(&fence_dma,&l,1,0,timer_monotonic_us()));
 }
 context_write32(fence_context,8,2,0x130000+63*16+1);
}
static void fence_bind(void){assert(wifi_fence5_bind(&xhci.devices[0],&fence_dma,fence_epoch,&fence_epoch,fence_owned));assert(wifi_fence5_begin(0));}
static struct xhci_trb fence_cc(void){
 struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];
 return (struct xhci_trb){.parameter_low=r->id.pointer,.parameter_high=r->id.pointer>>32,.status=1u<<24,.control=(33u<<10)|(7u<<24)|1};
}
static void fence_stop(bool base){
 uint64_t ptr=base?0x130000:0x130000+63*16;context_write32(fence_context,8,0,3);context_write32(fence_context,8,2,ptr|(base?0:1));
 struct xhci_trb stop={.parameter_low=ptr,.status=27u<<24,.control=(32u<<10)|(8u<<16)|(7u<<24)|1};dispatch_transfer_event(&stop);
}
static void fence_complete(bool other_consumer){
 struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];context_write32(fence_context,r->id.dci,0,3);
 if(r->id.type==15 && !r->stop_event)fence_stop(false);
 if(r->id.type==16){context_write32(fence_context,r->id.dci,2,r->id.dequeue);context_write32(fence_context,r->id.dci,3,r->id.dequeue>>32);}
 struct xhci_trb e=fence_cc();
 if(other_consumer){fence_events[xhci.event_dequeue]=e;poll_hid_events_once();}else dispatch_transfer_event(&e);
 assert(wifi_fence5_step());
}
static void fence_finish(void){
 for(unsigned i=0;i<16 && wifi_fence5_native.round;i++){assert(wifi_fence5_step());if(xhci.command_pending)fence_complete(false);}
 assert(!wifi_fence5_native.round && !wifi_fence5_native.failed && fence_dma.wraps==1 && !fence_dma.cursor[0] && !fence_dma.cycle[0]);
}
int main(void){
 fence_setup();fence_bind();assert(wifi_fence5_step());assert(xhci.command_pending && !xhci.sync_users && !wifi_fence5_sync_admitted());
 assert(!submit_command(23,0,NULL));fence_complete(true);fence_finish();assert(wifi_fence5_sync_admitted());
 assert(wifi_fence5_native.commands[0].event.control>>10 && wifi_fence5_native.commands[0].stop_dequeue==0x130000+63*16+1 && (wifi_fence5_native.commands[0].stop_context[0]&7)==3);
 assert(!wifi_fence5_retire());
 struct xhci_trb ordinary={.parameter_low=0x130000,.status=1u<<24,.control=(32u<<10)|(8u<<16)|(7u<<24)|1};assert(!wifi_fence5_stop_event(&ordinary));
 struct xhci_trb duplicate=fence_cc();dispatch_transfer_event(&duplicate);assert(wifi_fence5_native.failed && fence_dma.state==MT76_DMA5_FAULT && !wifi_fence5_reclaim_ready());
 for(unsigned fault=0;fault<6;fault++){
  fence_setup();fence_bind();assert(wifi_fence5_step());if(fault!=5)fence_stop(false);struct xhci_trb e=fence_cc();
  if(fault==0)e.parameter_low+=16;
  if(fault==1)e.control^=1u<<24;
  if(fault==2)e.status=5u<<24;
  if(fault==3)xhci.command_ring[xhci.command_enqueue-1].status=1;
  if(fault==4)fence_epoch++;
  dispatch_transfer_event(&e);if(!wifi_fence5_native.failed)assert(!wifi_fence5_step());
  assert(wifi_fence5_native.failed && xhci.command_pending && !xhci.sync_users && fence_dma.state==MT76_DMA5_FAULT);
 }
 for(unsigned boundary=0;boundary<2;boundary++){
  fence_setup();fence_bind();assert(wifi_fence5_step());fence_stop(boundary);assert(!wifi_fence5_native.failed && wifi_fence5_native.commands[0].stop_event);fence_complete(false);fence_finish();
 }
 fence_setup();xhci.command_enqueue=56;assert(!wifi_fence5_bind(&xhci.devices[0],&fence_dma,fence_epoch,&fence_epoch,fence_owned) && !wifi_fence5_native.bound);
 fence_setup();fence_bind();assert(wifi_fence5_step());assert(wifi_fence5_step());assert(!wifi_fence5_native.commands[0].consumed && xhci.command_pending);
 wifi_fence5_stopping();assert(wifi_fence5_native.failed && !xhci.sync_users && !wifi_fence5_reclaim_ready());
 fence_setup();fence_bind();assert(wifi_fence5_step());xhci.devices[0].slot_id++;assert(!wifi_fence5_step());
 fence_setup();fence_bind();assert(wifi_fence5_step());context_write32(fence_context,9,4,1);assert(!wifi_fence5_step());
 fence_setup();fence_bind();assert(wifi_fence5_step());fence_complete(false);assert(!xhci.command_pending);struct xhci_trb stray={.parameter_low=0x100000,.status=1u<<24,.control=(33u<<10)|(7u<<24)|1};dispatch_transfer_event(&stray);assert(wifi_fence5_native.failed);
 /* Deadline and backward clock preserve pending command ownership. */
 for(unsigned clock=0;clock<2;clock++){
  fence_setup();fence_bind();assert(wifi_fence5_step());
  if(clock)test_time=0;else test_time+=MT76_FENCE5_WAIT_US;
  assert(!wifi_fence5_step() && xhci.command_pending && wifi_fence5_native.failed && !xhci.sync_users);
 }
 /* Target transfer debt and a full32-event frontier are not empty proof. */
 for(unsigned debt=0;debt<2;debt++){
  fence_setup();fence_bind();assert(wifi_fence5_step());fence_complete(false);assert(wifi_fence5_step());
  assert(wifi_fence5_native.engine.state==MT76_FENCE5_DRAIN);
  for(unsigned n=0;n<(debt?32u:1u);n++)fence_events[n]=(struct xhci_trb){.parameter_low=debt?0x800000:0x130000,.status=1u<<24,.control=(32u<<10)|((debt?2u:8u)<<16)|((debt?1u:7u)<<24)|1};
  for(unsigned n=0;n<33 && !wifi_fence5_native.failed;n++)(void)wifi_fence5_step();
  assert(wifi_fence5_native.failed && fence_dma.state==MT76_DMA5_FAULT);
 }
 /* Normal terminal retirement requires both ledger proof and real halt state. */
 fence_setup();fence_bind();assert(wifi_fence5_step());fence_complete(false);fence_finish();
 assert(mt76_dma5_close(&fence_dma,fence_epoch));
 const struct mt76_dma5_retirement proof={.epoch=19,.keys_erased=true,.mac_disabled=true,.tx_stopped=true,.rx_stopped=true,.events_drained=true,.callbacks_drained=true,.parent_restored=true};
 assert(mt76_dma5_retire(&fence_dma,&proof));assert(!wifi_fence5_retire());
 xhci.stopped=true;*(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBCMD)=XHCI_USBCMD_RUN;
 *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBSTS)=XHCI_USBSTS_HCH;assert(!wifi_fence5_retire());
 *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBCMD)=0;assert(wifi_fence5_reclaim_ready() && wifi_fence5_native.retired);
 puts("xhci-fence5: PASS dormant native command publication/shared dispatch,real context/frontier reads,pureTX63-position wrap,typed boundaryStop,global exclusion,cleanup reserve,duplicate/stale/slot/CC/shadow/epoch failure and pinned stop retention;mock MMIO,no hardware acceptance");return 0;
}
