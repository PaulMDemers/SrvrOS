#define main wpa5_legacy_test_main
#include "test_xhci_wifi_wpa5.c"
#undef main
static bool run_provider(void){
 bool good=xhci_wifi_capture_provider5();
 for(unsigned i=0;i<32;i++)assert(!wifi_wpa5.pmk[i]);
 assert(!wifi_provider5_requested && !wifi_wpa5_requested && !wifi_wpa5_data_requested && !wifi_join5_after);return good;
}
int main(int argc,char **argv){
 begin();fake.data_mode=true;
 if((argc==2 || argc==11) && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--trace-audit"))){
  if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}
  fake.auth_extra=true;fake.trace=true;fake.data_wapi=!strcmp(argv[1],"--trace-audit");assert(run_provider());return 0;
 }
 assert(argc==1 && run_provider());
 assert(wifi_provider5.registered && wifi_provider5.attached && wifi_provider5.selected && wifi_provider5.closed && wifi_provider5.outer_retired && wifi_provider5.retired);
 struct mt76_net5 *p=&wifi_provider5.net;
 assert(p->state==MT76_NET5_RETIRED && p->enqueued==6 && p->submitted==6 && p->completed==6 && !p->attached && !p->io.context && !p->pending && !p->count && !p->faults && !p->dropped);
 assert(wifi_wpa5.ip.complete && wifi_wpa5.exchange.keys_erased && wifi_wpa5.restored && !fake.quarantine && fake.sends==14 && fake.commands==12 && !fake.data_console_calls);
 uint8_t eth[64]={0};assert(!net_device_send(eth,sizeof eth) && !run_provider());
 begin();fake.data_mode=true;fake.data_wapi=true;fake.slow_console=100000;assert(run_provider() && !fake.data_console_calls);
 begin();fake.data_mode=true;assert(!net_device_select());assert(!run_provider());
 assert(!wifi_provider5.registered && !wifi_wpa5.ip.attempted && wifi_wpa5.exchange.keys_erased && wifi_wpa5.restored);
 for(unsigned kind=1;kind<=3;kind++){
  begin();fake.data_mode=true;fake.provider_fault=kind;assert(!run_provider());
  assert(wifi_wpa5.exchange.keys_erased && !wifi_provider5.retired && wifi_provider5.net.attached && wifi_provider5.net.io.context);
 }
 for(unsigned send=8;send<=13;send++){
  begin();fake.data_mode=true;fake.fail_send=send;assert(!run_provider());
  assert(wifi_wpa5.exchange.keys_erased && !wifi_provider5.retired && wifi_provider5.net.attached && wifi_provider5.net.pending && wifi_provider5.net.registry_drained && !fake.data_console_calls);
 }
 begin();fake.data_mode=true;fake.data_bad_mic=true;assert(!run_provider());
 assert(wifi_wpa5.restored && wifi_wpa5.exchange.keys_erased && wifi_provider5.closed && !wifi_provider5.outer_retired && !wifi_provider5.retired && wifi_provider5.net.attached && wifi_provider5.net.io.context);
 begin();fake.data_mode=true;fake.data_nonce_fail=true;assert(!run_provider() && !wifi_provider5.registered && wifi_wpa5.exchange.keys_erased);
 puts("xhci-provider5: PASS actual native owner/real registry/copied provider queue,independent encrypted six-packet AP fixture,nonblocking submit and mapped completions,same key owner/PN1..6,late full-parent retirement gate,wrong tokens/epochs,all six USB stalls,MIC/entropy/late registration refusals;mock USB/RF,no native provider acceptance");return 0;
}
