/* Software integration only: replace the private diagnostic IO poll boundary
 * with net_device_poll. Compile mt76_net5.c separately with its delivery symbol
 * renamed to rx5_deliver; the historical fixture's assert(0) sink stays intact.
 * Production native diagnostic routing is unchanged. */
#include <srvros/mt76_ip5.h>
static bool rx5_ip_run(struct mt76_ip5 *,const uint8_t[6],uint32_t,const struct mt76_ip5_io *);
#define mt76_ip5_run rx5_ip_run
#define main wpa5_legacy_test_main
#include "test_xhci_wifi_wpa5.c"
#undef main
#undef mt76_ip5_run
static struct {
 uint8_t ethernet[NET_DEVICE_FRAME_MAX];size_t size;
 unsigned delivered,replays,stale_refused,bad_span_refused;
 bool close_on_delivery,accelerate_quiet;
 uint64_t before_pairwise[17],before_group[17];
} rx5;
void rx5_deliver(const uint8_t *frame,uint16_t length){
 struct mt76_net5 *p=&wifi_provider5.net;
 assert(p->delivering && p->polling && p->callbacks && !p->io_busy);
 assert(frame==p->rx && length>=14 && length<=sizeof rx5.ethernet && !rx5.size);
 assert(p->epoch==wifi_wpa5.peer.generation && wifi_data5_owned(&wifi_wpa5));
 assert(wifi_reuse5.dma.borrowed==wifi_reuse5.dma.released);
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<MT76_DMA5_PAGES;j++)assert(wifi_reuse5.dma.pages[i][j].state!=MT76_DMA5_BORROWED);
 unsigned advances=0;
 for(unsigned i=0;i<17;i++){
  uint64_t a=wifi_wpa5.exchange.keys.rx_pairwise[i],b=wifi_wpa5.exchange.keys.rx_group[i];
  assert(a>=rx5.before_pairwise[i] && b>=rx5.before_group[i]);
  advances+=(a>rx5.before_pairwise[i])+(b>rx5.before_group[i]);
 }
 assert(advances==1);
 memcpy(rx5.ethernet,frame,length);rx5.size=length;rx5.delivered++;
 /* Reprocessing the same raw MPDU cannot publish or commit replay state twice. */
 uint64_t committed_pairwise[17],committed_group[17];
 memcpy(committed_pairwise,wifi_wpa5.exchange.keys.rx_pairwise,sizeof committed_pairwise);
 memcpy(committed_group,wifi_wpa5.exchange.keys.rx_group,sizeof committed_group);
 unsigned i=wifi_wpa5.rx_copied-1;uint8_t duplicate[NET_DEVICE_FRAME_MAX];
 struct mt76_data5_rx_result r=mt76_data5_rx_report(&wifi_wpa5.data_scratch,&wifi_wpa5.exchange.keys,p->epoch,wifi_wpa5.rx_snapshot[i],4096-wifi_wpa5.rx_residue,duplicate,sizeof duplicate);
 assert(!r.bytes && !memcmp(committed_pairwise,wifi_wpa5.exchange.keys.rx_pairwise,sizeof committed_pairwise) && !memcmp(committed_group,wifi_wpa5.exchange.keys.rx_group,sizeof committed_group));rx5.replays++;
 if(rx5.close_on_delivery){
  assert(mt76_net5_disconnect(p,p->epoch));
  assert(!mt76_net5_finish_detach(p,p->epoch) && p->io.context==&wifi_wpa5);
  assert(!net_device_poll(1,false));
 }
}
static bool rx5_poll(void *c,uint8_t *out,size_t cap,size_t *n,bool *present){
 assert(c==&wifi_wpa5 && out==wifi_wpa5.ip.rx && cap==sizeof wifi_wpa5.ip.rx);
 *n=0;*present=false;assert(!rx5.size);
 struct mt76_net5 *p=&wifi_provider5.net;
 memcpy(rx5.before_pairwise,wifi_wpa5.exchange.keys.rx_pairwise,sizeof rx5.before_pairwise);
 memcpy(rx5.before_group,wifi_wpa5.exchange.keys.rx_group,sizeof rx5.before_group);
 size_t unused=0;bool available=false;unsigned posts=wifi_wpa5.posts,copied=wifi_wpa5.rx_copied;
 assert(!wifi_provider5_receive(c,p->epoch+1,p->rx,sizeof p->rx,&unused,&available));rx5.stale_refused++;
 assert(p->epoch && !wifi_provider5_receive(c,p->epoch-1,p->rx,sizeof p->rx,&unused,&available));rx5.stale_refused++;
 assert(!wifi_provider5_receive(c,p->epoch,out,cap,&unused,&available));rx5.bad_span_refused++;
 assert(posts==wifi_wpa5.posts && copied==wifi_wpa5.rx_copied);
 uint64_t count=net_device_poll(1,false);assert(count==(rx5.size?1u:0u));
 if(rx5.size){assert(rx5.size<=cap);memcpy(out,rx5.ethernet,rx5.size);*n=rx5.size;*present=true;memset(rx5.ethernet,0,sizeof rx5.ethernet);rx5.size=0;}
 if(rx5.accelerate_quiet && !*present)fake.time+=100000;
 return wifi_provider5_owned(c);
}
static bool rx5_ip_run(struct mt76_ip5 *s,const uint8_t station[6],uint32_t xid,const struct mt76_ip5_io *io){
 assert(io->poll==wifi_data5_poll && wifi_provider5.selected && wifi_reuse5.initialized);
 struct mt76_ip5_io ordinary=*io;ordinary.poll=rx5_poll;
 return mt76_ip5_run(s,station,xid,&ordinary);
}
static void rx5_begin(void){begin();memset(&rx5,0,sizeof rx5);fake.data_mode=true;}
static bool rx5_run(void){bool good=xhci_wifi_capture_reuse5();assert(!wifi_reuse5_requested && !wifi_provider5_requested);return good;}
int main(void){
 rx5_begin();assert(rx5_run());
 assert(rx5.delivered==6 && rx5.replays==6 && rx5.stale_refused && rx5.bad_span_refused);
 assert(wifi_provider5.net.received==6 && wifi_wpa5.ip.complete && wifi_wpa5.data_received==6);
 assert(wifi_reuse5.retired && wifi_provider5.retired && wifi_wpa5.exchange.keys_erased && wifi_wpa5.restored);
 assert(!fake.data_console_calls && !wifi_provider5.net.io.context);
 rx5_begin();fake.pad=true;fake.data_wapi=true;assert(rx5_run() && rx5.delivered==6 && rx5.replays==6);
 rx5_begin();fake.data_bad_mic=true;rx5.accelerate_quiet=true;assert(!rx5_run() && !rx5.delivered && !wifi_provider5.net.received);
 rx5_begin();rx5.close_on_delivery=true;assert(!rx5_run() && rx5.delivered==1);
 assert(wifi_wpa5.exchange.keys_erased && wifi_provider5.net.received==1 && !net_device_poll(32,false));
 rx5_begin();fake.data_spoof=true;rx5.accelerate_quiet=true;assert(!rx5_run() && !rx5.delivered);
 puts("xhci-rx5: PASS actual encrypted native RX callback through production provider/registry delivery into private IP oracle;single replay commit,duplicate/MIC/peer refusal,borrow release before delivery,epoch/span gates and callback-close retention;test-only consumer routing,mock USB/AP,no native ordinary-stack acceptance");return 0;
}
