/* Actual finite native handoff include + actual DMA/provider/registry. MMIO,
 * original-key guard and xHCI event dispatch are modeled; no native acceptance. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/mt76_dma5.h>
#include <srvros/mt76_net5.h>
#include <srvros/mt76_join5.h>
#define SRVROS_NET_DEVICE_HOST_TEST
#include "../kernel/src/net_device.c"
#define WIFI_STREAM5_RX 252u
static uint64_t now=1000,irq=512;
static unsigned notifications,submits,services,receives;
static bool key_live,metadata_ok;
uint64_t timer_monotonic_us(void){return now;}
void net_notify_rx(void){notifications++;}
void net_handle_ethernet_frame(const uint8_t *p,uint16_t n){(void)p;(void)n;assert(!"unexpected delivery");}
static uint64_t event_irq_save(void){uint64_t f=irq;irq=0;return f;}
static void event_irq_restore(uint64_t f){assert(!irq);irq=f;}
static void zero_bytes(void *p,size_t n){memset(p,0,n);}
static struct {
 struct mt76_net5 net;uint64_t tx_token,last,terminal_epoch,terminal_ticket,terminal_token,terminal_posts,terminal_completions;
 bool selected,running,exercise_complete,boundary,failed,terminal_handoff,terminal_pending,terminal_ready,terminal_provider_closing;
} wifi_stack5;
static struct {
 struct mt76_dma5 dma;struct mt76_dma5_lease lease[2],loan;
 uint64_t epoch,pages[2][4],posts[2],completions[2];uint8_t cached[4096];unsigned cached_size;
 bool pending[2],ready_rx,cached_present,failed,closed,data_active;
} wifi_stream5;
static struct {unsigned round;} wifi_fence5_native;
static struct {bool command_pending;} xhci;
static bool wifi_stream5_data_owned(void *c){return c==&wifi_stream5 && key_live && metadata_ok && wifi_stream5.data_active && !wifi_stream5.closed;}
static bool wifi_stack5_owned(void *c,uint64_t epoch){return wifi_stack5.running && !wifi_stack5.failed && epoch==wifi_stream5.epoch && wifi_stream5_data_owned(c);}
static bool wifi_stack5_clock(void){if(now<wifi_stack5.last || now>=15000000)return false;wifi_stack5.last=now;return true;}
static bool wifi_stack5_fault(void){wifi_stack5.failed=wifi_stream5.failed=true;return false;}
static bool wifi_stack5_ready(void *c,uint64_t e){return !wifi_stack5.terminal_handoff && wifi_stack5_owned(c,e);}
static enum mt76_net5_submit wifi_stack5_submit(void *c,uint64_t e,uint64_t token,const uint8_t *p,uint16_t n){(void)c;(void)e;(void)token;(void)p;(void)n;submits++;return MT76_NET5_QUEUED;}
static bool wifi_stack5_service(void *c,uint64_t e){(void)c;(void)e;services++;return true;}
static bool wifi_stack5_receive(void *c,uint64_t e,uint8_t *p,size_t cap,size_t *n,bool *present){(void)c;(void)e;(void)p;(void)cap;receives++;*n=0;*present=false;return true;}
static bool wifi_stack5_retirement(void *c,uint64_t e){(void)c;(void)e;return false;}
static bool wifi_stack5_probe_current(void *c,uint64_t e){return wifi_stack5_owned(c,e) && wifi_stack5.net.state==MT76_NET5_UP && wifi_stack5.net.attached && wifi_stack5.net.epoch==e;}
static jmp_buf held;static unsigned held_site;
static __attribute__((noreturn)) void wifi_http5_hold(unsigned site){assert(key_live && irq==512);held_site=site;longjmp(held,1);}
#include "../kernel/src/drivers/xhci_wifi_http5_rx_handoff.inc"
static void setup(unsigned raw) {
 memset(providers,0,sizeof providers);memset(identities,0,sizeof identities);memset(names,0,sizeof names);memset(references,0,sizeof references);memset(retiring,0,sizeof retiring);provider_count=0;selected=NULL;sealed=false;
 memset(&wifi_stack5,0,sizeof wifi_stack5);memset(&wifi_stream5,0,sizeof wifi_stream5);memset(&wifi_fence5_native,0,sizeof wifi_fence5_native);memset(&xhci,0,sizeof xhci);
 now=1000;irq=512;key_live=metadata_ok=true;submits=services=receives=notifications=0;
 wifi_stack5.selected=wifi_stack5.running=wifi_stream5.data_active=true;wifi_stream5.epoch=31;
 const uint8_t mac[6]={2,1,2,3,4,5};assert(mt76_net5_init(&wifi_stack5.net,mac));assert(net_device_select());
 struct mt76_net5_io io={&wifi_stream5,wifi_stack5_owned,wifi_stack5_submit,wifi_stack5_service,wifi_stack5_receive,wifi_stack5_retirement};
 assert(mt76_net5_attach(&wifi_stack5.net,31,&io));assert(mt76_net5_set_ready(&wifi_stack5.net,31,wifi_stack5_ready));
 uint64_t rings[2]={0x10000,0x20000},pages[2][4];for(unsigned d=0;d<2;d++)for(unsigned i=0;i<4;i++)pages[d][i]=0x30000+(d*4+i)*4096;
 memcpy(wifi_stream5.pages,pages,sizeof pages);assert(mt76_dma5_init_ring(&wifi_stream5.dma,31,rings,pages,63,now));
 if(raw) {
  assert(mt76_dma5_prepare(&wifi_stream5.dma,1,1,4096,now,&wifi_stream5.lease[1]));assert(mt76_dma5_publish(&wifi_stream5.dma,&wifi_stream5.lease[1],now));wifi_stream5.pending[1]=true;wifi_stream5.posts[1]=1;
  if(raw==2){assert(mt76_dma5_complete(&wifi_stream5.dma,&wifi_stream5.lease[1],13,3728,now));wifi_stream5.pending[1]=false;wifi_stream5.ready_rx=true;wifi_stream5.completions[1]=1;}
 }
}
static void callbacks_closed(void) {
 uint8_t frame[60]={0};memset(frame,255,6);memcpy(frame+6,wifi_stack5.net.ops.mac,6);frame[12]=8;
 unsigned a=submits,b=services,c=receives;uint64_t tx=wifi_stack5.net.enqueued;
 assert(!net_device_send(frame,sizeof frame));assert(!net_device_poll(32,false));assert(!mt76_net5_service(&wifi_stack5.net));
 assert(submits==a && services==b && receives==c && wifi_stack5.net.enqueued==tx && !wifi_stack5.net.dropped);
}
static void positive(unsigned raw,bool late) {
 setup(raw);struct mt76_dma5 before=wifi_stream5.dma;
 assert(wifi_stack5_terminal_rx_handoff(&wifi_stream5,31));assert(irq==512 && key_live);
 assert(wifi_stack5.terminal_handoff && wifi_stack5.terminal_pending==(raw==1) && wifi_stack5.terminal_ready==(raw==2));
 assert(wifi_stack5.terminal_posts==(raw?1u:0u) && wifi_stack5.terminal_completions==(raw==2?1u:0u) && wifi_stack5.terminal_ticket==(raw?wifi_stream5.lease[1].ticket:0));
 assert(wifi_stack5.net.state==MT76_NET5_CLOSING && wifi_stack5.net.attached && !memcmp(&before,&wifi_stream5.dma,sizeof before));
 callbacks_closed();assert(!wifi_stack5_terminal_rx_handoff(&wifi_stream5,31));
 if(late){assert(raw==1);assert(mt76_dma5_complete(&wifi_stream5.dma,&wifi_stream5.lease[1],13,3728,now));wifi_stream5.pending[1]=false;wifi_stream5.ready_rx=true;wifi_stream5.completions[1]++;assert(wifi_stack5_terminal_raw_rx());}
 assert(wifi_stack5_terminal_window(true,&wifi_stream5,31)==MT76_JOIN5_SENT);assert(irq==512 && key_live && wifi_stream5.dma.state==MT76_DMA5_RUNNING);
 assert(!wifi_stack5.running && wifi_stream5.closed && !wifi_stream5.data_active && !wifi_stream5.loan.ticket);
 /* Hardware/key/ledger retirement is still unproved and remains mandatory. */
 assert(!mt76_net5_finish_detach(&wifi_stack5.net,31));
}
static void negative(unsigned which) {
 setup(1);struct mt76_net5 *p=&wifi_stack5.net;
 switch(which){
 case 0:p->callbacks=1;break;case 1:p->io_busy=true;break;case 2:p->polling=true;break;case 3:p->delivering=true;break;
 case 4:p->count=1;p->length[0]=60;memset(p->queue[0],0x51,60);break;
 case 5:p->pending=true;p->pending_token=1;p->next_token=1;break;
 case 6:p->loan_size=1;break;case 7:p->faults=1;break;case 8:p->dropped=1;break;case 9:p->enqueued=1;break;
 case 10:wifi_stack5.tx_token=1;break;case 11:wifi_stack5.boundary=true;break;case 12:wifi_fence5_native.round=1;break;case 13:xhci.command_pending=true;break;
 case 14:wifi_stream5.cached_present=true;break;case 15:wifi_stream5.cached_size=14;break;case 16:wifi_stream5.loan.ticket=1;break;
 case 17:wifi_stream5.pending[0]=true;break;case 18:wifi_stream5.ready_rx=true;break;
 case 19:wifi_stream5.lease[1].ticket++;break;case 20:wifi_stream5.lease[1].trb+=8;break;case 21:wifi_stream5.lease[1].epoch++;break;
 case 22:wifi_stream5.dma.pages[1][wifi_stream5.lease[1].index].state=MT76_DMA5_BORROWED;break;
 case 23:wifi_stream5.dma.pages[0][0].state=MT76_DMA5_DEVICE;break;case 24:metadata_ok=false;break;case 25:wifi_stack5.terminal_handoff=true;break;
 case 26:wifi_stream5.lease[1].received=1;break;case 27:wifi_stream5.lease[1].length=2048;break;case 28:wifi_stream5.dma.pages[1][1].state=MT76_DMA5_COMPLETE;break;case 29:wifi_stream5.completions[1]++;break;case 30:wifi_stream5.posts[1]++;break;case 31:wifi_stream5.posts[1]=253;break;
 default:assert(0);
 }
 struct mt76_net5 saved=*p;struct mt76_dma5 dma=wifi_stream5.dma;unsigned n=notifications;
 assert(!wifi_stack5_terminal_rx_handoff(&wifi_stream5,31));assert(irq==512 && key_live && notifications==n);
 assert(!memcmp(&saved,p,sizeof saved) && !memcmp(&dma,&wifi_stream5.dma,sizeof dma));
}
static void lost_after_handoff(unsigned which) {
 setup(1);assert(wifi_stack5_terminal_rx_handoff(&wifi_stream5,31));
 switch(which){
 case 0:wifi_stream5.cached_present=true;break;case 1:wifi_stream5.cached_size=14;break;case 2:wifi_stream5.loan.ticket=1;break;
 case 3:wifi_stack5.net.callbacks=1;break;case 4:wifi_stack5.net.polling=true;break;case 5:wifi_stack5.net.io_busy=true;break;
 case 6:wifi_stack5.net.state=MT76_NET5_UP;break;case 7:wifi_stack5.net.epoch++;break;case 8:wifi_stream5.epoch++;break;
 case 9:wifi_stack5.net.io.context=NULL;break;case 10:wifi_stack5.net.io_guard.receive=NULL;break;case 11:wifi_stack5.net.ready=NULL;break;
 case 12:wifi_stack5.net.count=1;break;case 13:wifi_stream5.dma.pages[1][0].state=MT76_DMA5_BORROWED;break;
 default:assert(0);
 }
 held_site=0;
 if(!setjmp(held)){(void)wifi_stack5_terminal_window(true,&wifi_stream5,31);assert(!"software seal loss returned to legacy key erase");}
 assert(held_site==5 && key_live && wifi_stream5.dma.state==MT76_DMA5_RUNNING);irq=512;
}
int main(void){positive(0,false);positive(1,false);positive(2,false);positive(1,true);for(unsigned i=0;i<32;i++)negative(i);for(unsigned i=0;i<14;i++)lost_after_handoff(i);puts("http5 native RX handoff: PASS real registry/provider/DMA with modeled native metadata;4positive32refusal14posthandoffholds; no hardware/key/endpoint retirement acceptance");return 0;}
