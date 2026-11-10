/* Explicit v139 native HTTP bridge. All process/IO state survives failure.
 * The legacy WPA callback ALWAYS erases software keys after it returns, even
 * UNCERTAIN; never return to that owner with outstanding HTTP obligations. */
#include <srvros/http_network_probe.h>
static struct http_network_probe wifi_http5_probe;
static struct http_network_probe_io wifi_http5_io;
static bool wifi_http5_requested,wifi_http5_prepared,wifi_http5_capture_attempted,wifi_http5_callback_attempted;
static __attribute__((noreturn)) void wifi_http5_hold(void) {
#ifdef SRVROS_XHCI_HTTP5_HOST_TEST
 extern __attribute__((noreturn)) void wifi_http5_test_hold(void);
 wifi_http5_test_hold();
#else
 console_write("wifi-http5-held: software obligations incomplete; key/provider/DMA retained; no key erase or disk save; power off manually for Ubuntu\n");
 console_sync();__asm__ volatile("cli":::"memory");
 for(;;)__asm__ volatile("hlt":::"memory");
#endif
}
bool xhci_wifi_http5_prepare(void) {
 if(wifi_http5_requested || wifi_stack5.selected || wifi_stream5.attempted || wifi_stream5.stack_mode)return false;
 wifi_http5_requested=true;wifi_http5_prepared=http_network_probe_prepare(&wifi_http5_probe);
 console_printf("wifi-http5-prepare: prepared=%u retained=1\n",(uint64_t)wifi_http5_prepared);
 return wifi_http5_prepared;
}
static bool wifi_http5_drained(void *context,uint64_t epoch) {
 struct mt76_stack5_observation observed={0};
 return wifi_stack5_probe_observe(context,epoch,&observed) && observed.drained;
}
static bool wifi_http5_exercise(void *context,struct mt76_net5 *provider) {
 if(context!=&wifi_http5_probe || provider!=&wifi_stack5.net || !wifi_http5_requested ||
    !wifi_http5_prepared || wifi_http5_callback_attempted ||
    wifi_stack5.start>UINT64_MAX-WIFI_STACK5_TOTAL_US)wifi_http5_hold();
 wifi_http5_callback_attempted=true;
 /* Static source and context remain valid through every retained failure.
  * These exact existing provider guards do not depend on child liveness. */
 wifi_http5_io=(struct http_network_probe_io){.context=&wifi_stream5,
  .current=wifi_stack5_probe_current,.retry_ready=wifi_stack5_probe_retry,
  .drained=wifi_http5_drained,.epoch=wifi_stream5.epoch,
  .start_us=wifi_stack5.start,.deadline_us=wifi_stack5.start+WIFI_STACK5_TOTAL_US,
  .test_ip_override=0};
 bool good=http_network_probe_run(&wifi_http5_probe,&wifi_http5_io);
 if(!wifi_http5_probe.result.quiescent || !wifi_http5_probe.result.child_retired ||
    !wifi_http5_probe.result.worker_finished || !wifi_http5_probe.result.tcp_empty ||
    !wifi_http5_probe.result.provider_drained)wifi_http5_hold();
 return good;
}
bool xhci_wifi_capture_http5(void) {
 if(!wifi_http5_requested || !wifi_http5_prepared || wifi_http5_capture_attempted)return false;
 wifi_http5_capture_attempted=true;
 return xhci_wifi_capture_stack5(wifi_http5_exercise,&wifi_http5_probe);
}
bool xhci_wifi_http5_finish_capture(bool parent_complete) {
 /* Missing adapter, firmware/RF/handshake refusal or unsupported recipe may
  * bypass the callback. Retire the waiting prelive process and unused parked
  * worker before logger flush/unmount, without claiming an HTTP workload. */
 if(wifi_http5_requested) {
  if(!wifi_http5_probe.result.attempted && !http_network_probe_abort_prepare(&wifi_http5_probe))wifi_http5_hold();
  if(!wifi_http5_probe.result.quiescent)wifi_http5_hold();
 }
 console_printf("wifi-http5-bridge: version=1 prepared=%u capture_attempted=%u callback_attempted=%u quiescent=%u child_retired=%u worker_finished=%u retained=1\n",
  (uint64_t)wifi_http5_prepared,(uint64_t)wifi_http5_capture_attempted,
  (uint64_t)wifi_http5_callback_attempted,
  (uint64_t)(!wifi_http5_requested || wifi_http5_probe.result.quiescent),
  (uint64_t)wifi_http5_probe.result.child_retired,(uint64_t)wifi_http5_probe.result.worker_finished);
 if(wifi_http5_requested)http_network_probe_emit(&wifi_http5_probe);
 return parent_complete && wifi_http5_requested && wifi_http5_prepared &&
  wifi_http5_capture_attempted && wifi_http5_callback_attempted && wifi_http5_probe.result.complete &&
  wifi_http5_probe.result.quiescent && wifi_http5_probe.result.child_retired && wifi_http5_probe.result.worker_finished;
}
