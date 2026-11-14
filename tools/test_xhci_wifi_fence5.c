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
/* Normal mode has real TX/RX pages and descriptor geometry. This fixture's
 * transport completions are independently validated raw mock events;CCMP is
 * exercised separately by the stream fixture,not invented here. */
static _Alignas(4096) uint8_t normal_input[4096],normal_page[2][4][4096];
static _Alignas(4096) struct xhci_trb normal_ring[2][256];
static uint64_t normal_token[2];
static void normal_setup(unsigned command_start){
 fence_setup();memset(&fence_dma,0,sizeof fence_dma);memset(normal_input,0,sizeof normal_input);memset(normal_page,0,sizeof normal_page);memset(normal_ring,0,sizeof normal_ring);memset(normal_token,0,sizeof normal_token);
 struct xhci_device *d=&xhci.devices[0];d->input_context=normal_input;d->input_context_phys=0x180000;
 const uint64_t rings[2]={0x130000,0x140000},pages[2][4]={{0x150000,0x151000,0x152000,0x153000},{0x154000,0x155000,0x156000,0x157000}};
 assert(mt76_dma5_init_ring(&fence_dma,fence_epoch,rings,pages,63,timer_monotonic_us()));xhci.command_enqueue=command_start;
 for(unsigned dir=0;dir<2;dir++){
  for(unsigned w=0;w<8;w++)context_write32(fence_context,8+dir,w,0);
  context_write32(fence_context,8+dir,0,1);context_write32(fence_context,8+dir,1,(512u<<16)|((dir?6u:2u)<<3));context_write32(fence_context,8+dir,2,rings[dir]|1);context_write32(fence_context,8+dir,4,dir?4096:124);write_link_trb(normal_ring[dir],rings[dir],true);
 }
 assert(wifi_fence5_bind(d,&fence_dma,fence_epoch,&fence_epoch,fence_owned));
}
static struct mt76_dma5_lease normal_post(unsigned dir){
 struct mt76_dma5_lease l;unsigned length=dir?4096:64;
 assert(mt76_dma5_prepare(&fence_dma,dir,++normal_token[dir],length,timer_monotonic_us(),&l));
 unsigned index=(unsigned)((l.trb-fence_dma.ring[dir])/16);assert(index<63 && l.cycle==fence_dma.cycle[dir]);
 memset(normal_page[dir][l.index],(int)(normal_token[dir]&255),length);
 normal_ring[dir][index]=(struct xhci_trb){.parameter_low=l.page,.parameter_high=l.page>>32,.status=length,.control=(1u<<10)|XHCI_TRB_IOC|l.cycle|(dir?XHCI_TRB_ISP:0)};
 if(index==62)write_link_trb(normal_ring[dir],fence_dma.ring[dir],l.cycle);
 assert(mt76_dma5_publish(&fence_dma,&l,timer_monotonic_us()));context_write32(fence_context,8+dir,0,1);return l;
}
static void normal_event(unsigned dir,const struct mt76_dma5_lease *l,bool release){
 unsigned residue=dir?4096-64:0,cc=dir?13:1;
 struct xhci_trb e={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=(cc<<24)|residue,.control=(32u<<10)|((8+dir)<<16)|(7u<<24)|1};
 assert(!wifi_fence5_stop_event(&e));
 unsigned index=(unsigned)((l->trb-fence_dma.ring[dir])/16);struct xhci_trb *td=&normal_ring[dir][index];
 assert((e.parameter_low|((uint64_t)e.parameter_high<<32))==l->trb && (e.control>>24)==7 && ((e.control>>16)&31)==8+dir && !(e.control&4));
 assert((td->parameter_low|((uint64_t)td->parameter_high<<32))==l->page && td->status==l->length && (td->control&1)==l->cycle && ((td->control>>10)&63)==1);
 assert(mt76_dma5_complete(&fence_dma,l,cc,residue,timer_monotonic_us()));context_write32(fence_context,8+dir,2,(l->trb+16)|l->cycle);
 if(dir && release){struct mt76_dma5_lease loan;assert(mt76_dma5_acquire(&fence_dma,fence_epoch,&loan));assert(loan.received==64 && loan.ticket==l->ticket && normal_page[dir][loan.index][0]==(normal_token[dir]&255));assert(mt76_dma5_release(&fence_dma,&loan));}
}
static void normal_advance(unsigned dir,unsigned count){for(unsigned i=0;i<count;i++){struct mt76_dma5_lease l=normal_post(dir);normal_event(dir,&l,true);}}
static void normal_round(unsigned dir,bool at_base){
 unsigned old=fence_dma.cycle[dir],other_cursor=fence_dma.cursor[dir^1],other_cycle=fence_dma.cycle[dir^1];uint64_t wraps=fence_dma.wraps;
 assert(wifi_fence5_commands_available()>=2 && wifi_fence5_begin(dir));
 for(unsigned step=0;step<24 && wifi_fence5_native.round;step++){
  assert(wifi_fence5_step());if(!xhci.command_pending)continue;
  struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];if(r->latched)continue;
  unsigned dci=8+dir;
  if(r->id.type==15){
   uint64_t ptr=fence_dma.ring[dir]+(at_base?0:63*16);
   if(!at_base){context_write32(fence_context,dci,0,3);context_write32(fence_context,dci,2,ptr|old);}
   struct xhci_trb e={.parameter_low=ptr,.parameter_high=ptr>>32,.status=27u<<24,.control=(32u<<10)|(dci<<16)|(7u<<24)|1};dispatch_transfer_event(&e);
   assert(r->stop_event && !r->latched && !r->stop_dequeue);
   context_write32(fence_context,dci,0,3);context_write32(fence_context,dci,2,ptr|(at_base?(old^1u):old));
  }else{assert(r->id.type==16 && r->id.dci==dci);context_write32(fence_context,dci,0,3);context_write32(fence_context,dci,2,r->id.dequeue);context_write32(fence_context,dci,3,r->id.dequeue>>32);}
  struct xhci_trb command=fence_cc();dispatch_transfer_event(&command);
 }
 assert(!wifi_fence5_native.round && !wifi_fence5_native.failed && fence_dma.wraps==wraps+1 && fence_dma.cursor[dir]==0 && fence_dma.cycle[dir]==(old^1u));
 assert(fence_dma.cursor[dir^1]==other_cursor && fence_dma.cycle[dir^1]==other_cycle);
}
static void normal_tests(void){
 normal_setup(12);
 for(unsigned cycle=0;cycle<2;cycle++)for(unsigned dir=0;dir<2;dir++){normal_advance(dir,63);normal_round(dir,(cycle+dir)&1);}
 assert(fence_dma.wraps==4 && wifi_fence5_native.count==8 && fence_dma.published==252 && fence_dma.completed==252 && fence_dma.borrowed==126 && fence_dma.released==126);
 for(unsigned dir=0;dir<2;dir++){struct mt76_dma5_lease l=normal_post(dir);assert(l.trb==fence_dma.ring[dir] && l.cycle==1);normal_event(dir,&l,true);}
 for(unsigned mutation=0;mutation<9;mutation++){
  normal_setup(12);normal_advance(0,63);
  if(mutation<3)context_write32(fence_context,(unsigned[]){0,11,16}[mutation],4,1);
  if(mutation==3)xhci.devices[0].input_context=fence_context;
  if(mutation==4)xhci.devices[0].input_context_phys++;
  if(mutation==5)context_write32(fence_context,8,1,context_read32(fence_context,8,1)^2);
  if(mutation==6)context_write32(fence_context,9,4,1);
  if(mutation==7)context_write32(fence_context,9,2,0x999001);
  if(mutation==8)xhci.devices[0].output_context_phys++;
  assert(!wifi_fence5_begin(0) && wifi_fence5_native.failed && !wifi_fence5_native.count && xhci.command_enqueue==12);
 }
 normal_setup(12);normal_advance(0,63);assert(wifi_fence5_begin(0));context_write32(fence_context,9,2,0x140011);assert(!wifi_fence5_step() && !wifi_fence5_native.count);
 for(unsigned loan=0;loan<3;loan++){
  normal_setup(12);normal_advance(0,63);struct mt76_dma5_lease l=normal_post(1),borrowed;
  if(loan){normal_event(1,&l,false);if(loan==2)assert(mt76_dma5_acquire(&fence_dma,fence_epoch,&borrowed));}
  enum mt76_dma5_page_state held=fence_dma.pages[1][l.index].state;
  assert(!wifi_fence5_begin(0) && !wifi_fence5_native.count && fence_dma.pages[1][l.index].state==held);
  if(loan==2)assert(mt76_dma5_release(&fence_dma,&borrowed));
 }
 normal_setup(55);normal_advance(1,63);normal_round(1,true);assert(!wifi_fence5_commands_available());normal_advance(1,63);assert(!wifi_fence5_begin(1) && wifi_fence5_native.count==2);
 normal_setup(12);normal_advance(1,63);assert(wifi_fence5_begin(1));assert(wifi_fence5_step());
 struct xhci_trb debt={.parameter_low=0x130000,.status=1u<<24,.control=(32u<<10)|(8u<<16)|(7u<<24)|1};dispatch_transfer_event(&debt);assert(wifi_fence5_native.failed && fence_dma.state==MT76_DMA5_FAULT);
}
static void preadmission_tests(void){
 /* No command is outstanding: a shared consumer must retain unexpected
  * command debt instead of consuming it as harmless frontier backpressure. */
 for(unsigned consumer=0;consumer<3;consumer++){
  fence_setup();assert(wifi_fence5_bind(&xhci.devices[0],&fence_dma,fence_epoch,&fence_epoch,fence_owned));
  struct xhci_trb e={.parameter_low=0x100000+12*16,.status=1u<<24,.control=(33u<<10)|(7u<<24)|1};
  if(consumer==2){xhci.command_pending=true;xhci.sync_users=1;}
  if(consumer){fence_events[0]=e;poll_hid_events_once();}else dispatch_transfer_event(&e);
  assert(wifi_fence5_native.failed && wifi_fence5_native.fault_event_valid && fence_dma.state==MT76_DMA5_FAULT);
  assert(!wifi_fence5_native.round && !wifi_fence5_native.count && xhci.command_enqueue==12 && !xhci.command_events && !fence_dma.wraps);
  assert(!wifi_fence5_begin(0));
 }
 /* Only the actual synchronous waiter may delegate its exact pointer. */
 for(unsigned wrong=0;wrong<2;wrong++){
  fence_setup();assert(wifi_fence5_bind(&xhci.devices[0],&fence_dma,fence_epoch,&fence_epoch,fence_owned));
  fence_events[0]=(struct xhci_trb){.parameter_low=0x100000+(12+wrong)*16,.status=1u<<24,.control=(33u<<10)|(7u<<24)|1};
  bool ok=submit_command(23,0,NULL);
  if(wrong)assert(!ok && wifi_fence5_native.failed && xhci.command_pending && xhci.command_failed && fence_dma.state==MT76_DMA5_FAULT);
  else assert(ok && !wifi_fence5_native.failed && !xhci.command_pending && !xhci.command_failed && xhci.command_events==1 && wifi_fence5_sync_admitted());
 }
 /* A busy frontier cannot hide invalid ownership/geometry. */
 normal_setup(12);normal_advance(0,63);
 fence_events[0]=(struct xhci_trb){.parameter_low=0x800000,.status=1u<<24,.control=(32u<<10)|(3u<<16)|(8u<<24)|1};
 context_write32(fence_context,9,4,1);
 assert(wifi_fence5_try_begin(0)==MT76_FENCE5_FAILED && wifi_fence5_native.failed && !wifi_fence5_native.count && xhci.command_enqueue==12);
}
int main(void){
 preadmission_tests();
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
 normal_tests();
 puts("xhci-fence5-normal: PASS actual backend TX8/RX9 normal-page geometry,4 fences across both cycles,validated mock TD completions/RX loans,63/64 reuse,common/config/input/opposite snapshots,held-page refusal,budget and debt;mock MMIO/data,no encrypted or hardware acceptance");
 puts("xhci-fence5: PASS dormant native command publication/shared dispatch,real context/frontier reads,pureTX63-position wrap,typed boundaryStop,global exclusion,cleanup reserve,duplicate/stale/slot/CC/shadow/epoch failure and pinned stop retention;mock MMIO,no hardware acceptance");return 0;
}
