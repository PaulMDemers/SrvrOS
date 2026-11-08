#define main wpa5_legacy_test_main
#include "test_xhci_wifi_wpa5.c"
#undef main
static bool run_reuse(void){
 bool good=xhci_wifi_capture_reuse5();
 for(unsigned i=0;i<32;i++)assert(!wifi_wpa5.pmk[i]);
 assert(!wifi_reuse5_requested && !wifi_provider5_requested && !wifi_wpa5_requested && !wifi_wpa5_data_requested && !wifi_join5_after);return good;
}
int main(int argc,char **argv){
 begin();fake.data_mode=true;
 if((argc==2 || argc==11) && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--trace-audit"))){
  if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}
  fake.auth_extra=true;fake.trace=true;fake.data_wapi=!strcmp(argv[1],"--trace-audit");assert(run_reuse());return 0;
 }
 assert(argc==1 && run_reuse());
 assert(wifi_reuse5.retired && wifi_reuse5.early_refused && wifi_reuse5.events_drained && wifi_reuse5.dma.state==MT76_DMA5_RETIRED && !wifi_reuse5.dma.faults && !wifi_reuse5.dma.wraps);
 assert(wifi_reuse5.dma.published==wifi_wpa5.sends+wifi_wpa5.posts && wifi_reuse5.dma.borrowed==wifi_wpa5.rx_copied && wifi_reuse5.dma.released==wifi_wpa5.rx_copied);
 assert(wifi_wpa5.ip.complete && wifi_wpa5.exchange.keys_erased && wifi_provider5.retired && wifi_wpa5.restored && !fake.quarantine && !fake.data_console_calls);
 for(unsigned i=0;i<wifi_wpa5.posts;i++)assert(wifi_wpa5.rx[i].parameter_low==wifi_wpa5.pages[i%4]);
 for(unsigned i=0;i<wifi_wpa5.sends;i++)assert(wifi_wpa5.tx[i].parameter_low==wifi_wpa5.pages[WIFI_WPA5_RX+i%4]);
 begin();fake.data_mode=true;fake.data_wapi=true;fake.drain_queue=3;assert(run_reuse() && wifi_wpa5.drain_completed==3 && !fake.data_console_calls);
 begin();fake.data_mode=true;fake.data_flood=true;fake.drain_queue=3;assert(!run_reuse() && wifi_wpa5.restored && wifi_wpa5.ip.exhausted && !wifi_reuse5.retired && wifi_provider5.net.attached);
 begin();fake.data_mode=true;fake.data_bad_mic=true;assert(!run_reuse() && wifi_wpa5.exchange.keys_erased && !wifi_reuse5.retired && wifi_provider5.net.attached);
 begin();fake.data_mode=true;fake.reuse_tail=1;assert(!run_reuse() && !wifi_reuse5.retired && !wifi_provider5.retired && !wifi_reuse5.events_drained);
 for(unsigned kind=1;kind<=6;kind++){
  begin();fake.data_mode=true;fake.reuse_fault=kind;assert(!run_reuse() && !wifi_reuse5.retired && !wifi_provider5.retired);
 }
 for(unsigned send=8;send<=13;send++){
  begin();fake.data_mode=true;fake.fail_send=send;assert(!run_reuse() && wifi_wpa5.exchange.keys_erased && !wifi_reuse5.retired && wifi_provider5.net.attached);
 }
 puts("xhci-reuse5: PASS actual native owner/real registry/independent encrypted AP,page reuse through real descriptor/event paths,CCMP PN1..6,borrow/copy/release,cleanup reserve and full late ledger/provider retirement;mock USB/RF,no physical reuse acceptance");return 0;
}
