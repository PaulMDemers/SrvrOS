/* Exercise the same actual native owner and historical parent fixture. */
#define main wpa5_legacy_test_main
#include "test_xhci_wifi_wpa5.c"
#undef main
int main(int argc,char **argv){
 begin();fake.data_mode=true;
 if((argc==2 || argc==11) && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--trace-drain") || !strcmp(argv[1],"--trace-audit"))){
  if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}
  if(!strcmp(argv[1],"--trace-drain"))fake.drain_queue=3;
  if(!strcmp(argv[1],"--trace-audit"))fake.data_wapi_once=true;
  fake.auth_extra=true;fake.trace=true;assert(run());return 0;
 }
 assert(argc==1 && run() && wifi_wpa5.ip.complete && wifi_wpa5.ip.echo && wifi_wpa5.ip.sends==6 && wifi_wpa5.data_sends==6 && wifi_wpa5.data_received==6 && wifi_wpa5.exchange.keys.tx_pn==6);
 assert(wifi_wpa5.restored && wifi_wpa5.exchange.keys_erased && fake.sends==14 && fake.commands==12 && !fake.quarantine && !fake.mac);
 assert(wifi_wpa5.data_reported && wifi_wpa5.data_events==12 && !fake.data_console_calls);
 unsigned checks=fake.ownership,first=fake.child_first;
 /* Deliberately expensive console writes may occur around the exchange,but
  * neither data send/poll nor the IP summary can charge its active deadline. */
 begin();fake.data_mode=true;fake.slow_console=100000;assert(run() && wifi_wpa5.ip.complete && !fake.data_console_calls && wifi_wpa5.data_reported);
 begin();fake.data_mode=true;fake.data_flood=true;fake.drain_queue=3;fake.slow_console=100000;
 assert(!run() && wifi_wpa5.restored && wifi_wpa5.ip.exhausted && !fake.data_console_calls && wifi_wpa5.data_reported);
 begin();fake.data_mode=true;fake.data_bad_mic=true;fake.slow_console=100000;
 assert(!run() && wifi_wpa5.restored && !wifi_wpa5.ip.offer && !fake.data_console_calls && wifi_wpa5.data_reported);
 begin();fake.data_mode=true;assert(run());
 for(unsigned i=0;i<sizeof wifi_wpa5.ip.tx;i++)assert(!wifi_wpa5.ip.tx[i] && !wifi_wpa5.ip.rx[i]);
 for(unsigned i=0;i<sizeof wifi_wpa5.data_scratch;i++)assert(!((uint8_t *)&wifi_wpa5.data_scratch)[i]);
 for(unsigned i=0;i<sizeof wifi_wpa5.data_usb;i++)assert(!wifi_wpa5.data_usb[i]);
 begin();fake.data_mode=true;fake.data_wapi_once=true;
 assert(run() && wifi_wpa5.ip.complete && wifi_wpa5.data_received==6 && wifi_wpa5.data_events==13 && wifi_wpa5.data_event[1].audit==4 && wifi_wpa5.data_event[2].reason==9 && wifi_wpa5.exchange.keys.tx_pn==6);
 begin();fake.data_mode=true;fake.data_wapi=true;
 assert(!run() && wifi_wpa5.restored && !fake.quarantine && !wifi_wpa5.ip.offer && !wifi_wpa5.data_received && wifi_wpa5.exchange.keys_erased && wifi_wpa5.exchange.leave_sent);
 assert(wifi_wpa5.data_events==2 && wifi_wpa5.data_event[1].kind==1 && wifi_wpa5.data_event[1].reason==2 && wifi_wpa5.data_event[1].flags==1024 && wifi_wpa5.data_event[1].audit==4);
 begin();fake.data_mode=true;fake.pad=true;assert(run() && wifi_wpa5.ip.complete);
 /* All forward pages can fill with irrelevant traffic. The same48 slots
  * retain eight cleanup entries;no page/TD reuse or crypto parsing in drain. */
 begin();fake.data_mode=true;fake.data_flood=true;fake.drain_queue=3;
 assert(!run() && wifi_wpa5.restored && !fake.quarantine && wifi_wpa5.exchange.keys_erased && wifi_wpa5.exchange.leave_sent);
 assert(wifi_wpa5.ip.exhausted && !wifi_wpa5.ip.unsafe && wifi_wpa5.ip.polls==36 && wifi_wpa5.forward_completed==40 && wifi_wpa5.drain_completed==3 && wifi_wpa5.rx_copied==43 && wifi_wpa5.posts==44 && !fake.drain_queue && wifi_wpa5.rx_enqueue==44 && wifi_wpa5.rx_cycle);
 begin();fake.data_mode=true;fake.data_flood=true;fake.drain_queue=9;
 assert(!run() && !wifi_wpa5.restored && fake.quarantine && wifi_wpa5.exchange.keys_erased && wifi_wpa5.exchange.leave_sent);
 assert(wifi_wpa5.ip.exhausted && !wifi_wpa5.ip.unsafe && wifi_wpa5.ip.polls==36 && wifi_wpa5.forward_completed==40 && wifi_wpa5.drain_completed==8 && wifi_wpa5.rx_copied==48 && wifi_wpa5.posts==48 && fake.drain_queue==1 && wifi_wpa5.rx_enqueue==48 && wifi_wpa5.rx_cycle);
 begin();fake.data_mode=true;fake.data_flood=true;fake.drain_queue=3;fake.drain_fault=1;
 assert(!run() && !wifi_wpa5.restored && fake.quarantine && wifi_wpa5.exchange.keys_erased && wifi_wpa5.posts<=41);
 for(unsigned i=8;i<=14;i++){
  begin();fake.data_mode=true;fake.fail_send=i;assert(!run() && fake.quarantine && fake.sends==i && wifi_wpa5.exchange.keys_erased);
 }
 for(unsigned i=54;i<=114;i++){
  begin();fake.data_mode=true;fake.fail_alloc=i;assert(!run() && fake.commands==6);
  begin();fake.data_mode=true;fake.alias_alloc=i;assert(!run() && fake.commands==6);
 }
 begin();fake.data_mode=true;fake.data_bad_mic=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && !wifi_wpa5.ip.offer && wifi_wpa5.exchange.leave_sent && wifi_wpa5.exchange.keys_erased);
 begin();fake.data_mode=true;fake.data_spoof=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && !wifi_wpa5.ip.offer && wifi_wpa5.exchange.leave_sent);
 begin();fake.data_mode=true;fake.data_empty=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && wifi_wpa5.exchange.leave_sent);
 begin();fake.data_mode=true;fake.data_nonce_fail=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && wifi_wpa5.exchange.leave_sent && !wifi_wpa5.ip.attempted);
 for(unsigned i=1;i<=9;i++){
  begin();fake.data_mode=true;fake.data_mutate=i;assert(!run() && wifi_wpa5.exchange.unsafe && !wifi_wpa5.exchange.leave_sent && wifi_wpa5.exchange.keys_erased);
  for(unsigned j=0;j<16;j++)assert(!wifi_wpa5.exchange.keys.tk[j] && !wifi_wpa5.exchange.keys.gtk[j]);
 }
 for(unsigned i=7;i<=12;i++)for(unsigned uncertain=0;uncertain<2;uncertain++){
  begin();fake.data_mode=true;fake.fail_command=i;fake.uncertain_command=uncertain;assert(!run() && fake.quarantine && !wifi_wpa5.restored);
 }
 for(unsigned i=first;i<=checks;i++){
  begin();fake.data_mode=true;fake.lose=i;assert(!run() && fake.quarantine && !wifi_wpa5.restored);
 }
 puts("xhci-data5: PASS real M1/four-way parent,independent AESCCM DHCP/DNS/echo frames,six monotonic TX PNs,group/unicast RX,padding,all encrypted sends/ownership and61 allocation/alias refusals,MIC/peer/timeouts,reserved RX cleanup,forward/reserve exhaustion and drain ownership loss,software erase/leave/full restoration;mocked AP/USB/RF/entropy,no physical data acceptance");
 return 0;
}
