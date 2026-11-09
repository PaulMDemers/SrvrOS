/* v136 uses the actual controller owner and independent public encrypted peer.
 * Parent/MMIO/EP0 remain modeled; these receipts are not native acceptance. */
#define STREAM5_TEST_MAIN stream5_legacy_fixture_main
#include "test_xhci_wifi_stream5.c"
#undef STREAM5_TEST_MAIN

static void dhcp_setup(bool drop,unsigned ambient,unsigned noise){
 stream_setup();sim.dhcp_mode=true;sim.drop_first=drop;sim.ambient=ambient;sim.noise=noise;
}
static void dhcp_success(bool drop,unsigned ambient,unsigned noise,bool link){
 dhcp_setup(drop,ambient,noise);sim.link=link;
 assert(xhci_wifi_capture_stream5_dhcp());
 assert(wifi_stream5.discover_retry && wifi_stream5.complete && wifi_stream5.retired && !wifi_fence5_native.retired);
 assert(wifi_stream5.probe.setup.discover_retries==(unsigned)drop && wifi_stream5.probe.setup.sends==6u+(unsigned)drop);
 assert(wifi_stream5.data_sends==70u+(unsigned)drop && wifi_stream5.data_pn==70u+(unsigned)drop && wifi_stream5.posts[0]==75u+(unsigned)drop);
 assert(wifi_stream5.probe.echoes_matched==64 && wifi_stream5.data_after_txwrap && wifi_stream5.data_after_rxwrap);
 assert(wifi_stream5.probe.setup.dhcp_class[0][MT76_IP5_MATCHED]==1 && wifi_stream5.probe.setup.dhcp_class[1][MT76_IP5_MATCHED]==1);
 assert(wifi_stream5.exchange.keys_erased && wifi_stream5.exchange.leave_sent && wifi_stream5.restored);
 if(drop)assert(wifi_stream5.probe.elapsed_us>=2000000);
 if(ambient)assert(wifi_stream5.wraps[1]>=2);
}
static void canonical_context(int argc,char **argv){
 if(argc==11 || argc==19){for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,9,i,(uint32_t)strtoull(argv[i+2],NULL,0));if(argc==19)for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,8,i,(uint32_t)strtoull(argv[i+10],NULL,0));wifi_cal5.d->slot_id=(uint8_t)strtoul(argv[argc-1],NULL,0);}
}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"--size-projection")){dhcp_setup(true,0,2);wifi_stream5.discover_retry=true;assert(wifi_stream5_inspect());assert(wifi_stream5_finish(true));stream_size_projection_emit();return 0;}
 if((argc==2 || argc==11 || argc==19) && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--trace-no-loss") || !strcmp(argv[1],"--trace-no-offer"))){
  dhcp_setup(strcmp(argv[1],"--trace-no-loss")!=0,0,0);sim.no_offer=!strcmp(argv[1],"--trace-no-offer");canonical_context(argc,argv);batch_trace=true;
  bool good=xhci_wifi_capture_stream5_dhcp();assert(good!=sim.no_offer);return 0;
 }
 assert(argc==1);
 dhcp_success(false,0,0,false);dhcp_success(true,0,0,false);dhcp_success(true,64,0,true);dhcp_success(true,0,2,true);
 /* Neither an uncertain first Discover nor an uncertain retransmission retries. */
 for(unsigned at=5;at<=6;at++){
  dhcp_setup(true,0,0);sim.fault=3;sim.fault_at=at;assert(!xhci_wifi_capture_stream5_dhcp());
  assert(wifi_stream5.failed && !wifi_stream5.retired && wifi_stream5.exchange.keys_erased);
  assert(wifi_stream5.probe.setup.discover_retries==at-5 && wifi_stream5.data_pn==at-4 && wifi_stream5.data_sends==at-5);
 }
 /* A second absent Offer remains a safe bounded refusal with retained DMA. */
 dhcp_setup(true,0,0);sim.no_offer=true;assert(!xhci_wifi_capture_stream5_dhcp());
 assert(!wifi_stream5.failed && !wifi_stream5.retired && wifi_stream5.restored && wifi_stream5.exchange.keys_erased && wifi_stream5.exchange.leave_sent);
 assert(wifi_stream5.probe.setup.discover_retries==1 && wifi_stream5.probe.setup.sends==2 && wifi_stream5.data_sends==2 && wifi_stream5.data_pn==2);
 assert(wifi_stream5.probe.elapsed_us>=4000000 && wifi_stream5.probe.elapsed_us<MT76_STREAM5_PROBE_TOTAL_US);
 assert(!wifi_stream5.probe.unsafe && !wifi_stream5.probe.complete && wifi_stream5.probe.echoes_matched==0);
 /* Actual malformed RX just after a real boundary still faults and retains. */
 dhcp_setup(true,64,0);sim.fault=35;sim.fault_at=64;assert(!xhci_wifi_capture_stream5_dhcp());
 assert(wifi_stream5.failed && !wifi_stream5.retired && wifi_stream5.exchange.keys_erased && wifi_stream5.fault_event_valid);
 puts("xhci-stream5-dhcp: PASS actual original-key70/71 encrypted requests,independent71pair peer,one lost Discover,quiet+ambient RX wrap,retry uncertainty/noOffer,64exactechoes,erasure/retention;modeled parent/MMIO,no native acceptance");return 0;
}
