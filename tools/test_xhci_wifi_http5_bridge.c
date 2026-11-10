/* Actual native bridge source; HOST process/provider/key-erasure model only.
 * Each reset models a distinct boot. No real USB/key/ELF/runtime acceptance. */
#define SRVROS_XHCI_HTTP5_HOST_TEST 1
#include <srvros/http_network_probe.h>
#include <srvros/mt76_stack5_probe.h>
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define WIFI_STACK5_TOTAL_US 15000000u
static struct {struct mt76_net5 net;uint64_t start;bool selected;} wifi_stack5;
static struct {uint64_t epoch;bool attempted,stack_mode;} wifi_stream5;
static unsigned prepares,aborts,runs,erasures,held,emits,guard_calls;
static const char *mode;static jmp_buf escape;static bool key_live;
static bool is(const char *s){return !strcmp(mode,s);}
static bool wifi_stack5_probe_current(void *c,uint64_t e){assert(c==&wifi_stream5 && e==31);guard_calls++;return true;}
static bool wifi_stack5_probe_retry(void *c,uint64_t e){return wifi_stack5_probe_current(c,e);}
static bool wifi_stack5_probe_observe(void *c,uint64_t e,struct mt76_stack5_observation *o){assert(wifi_stack5_probe_current(c,e));o->drained=!is("not-drained");return true;}
bool xhci_wifi_capture_stack5(bool (*exercise)(void *,struct mt76_net5 *),void *context);
void console_printf(const char *fmt,...){assert(!key_live);assert(strstr(fmt,"wifi-http5-") || strstr(fmt,"workload"));}
#include "../kernel/src/drivers/xhci_wifi_http5.inc"
__attribute__((noreturn)) void wifi_http5_test_hold(void){held++;longjmp(escape,1);}
bool http_network_probe_prepare(struct http_network_probe *p){
 assert(p==&wifi_http5_probe && !p->self);prepares++;p->self=(uintptr_t)p;p->result.version=1;
 p->result.prepared=!is("prepare-fail");return p->result.prepared;
}
bool http_network_probe_abort_prepare(struct http_network_probe *p){
 assert(p==&wifi_http5_probe && !p->result.attempted && !key_live);aborts++;
 if(is("abort-fail"))return false;
 p->result.quiescent=p->result.child_retired=p->result.worker_finished=true;p->result.reason=HTTP_NET_NOT_RUN;return true;
}
bool http_network_probe_run(struct http_network_probe *p,const struct http_network_probe_io *io){
 assert(key_live && p==&wifi_http5_probe && io==&wifi_http5_io && io->context==&wifi_stream5);
 assert(io->current==wifi_stack5_probe_current && io->retry_ready==wifi_stack5_probe_retry && io->drained==wifi_http5_drained);
 assert(io->epoch==31 && io->start_us==1000000 && io->deadline_us==16000000 && !io->test_ip_override);
 assert(io->current(io->context,io->epoch) && io->retry_ready(io->context,io->epoch));
 assert(io->drained(io->context,io->epoch)==!is("not-drained"));
 runs++;p->result.attempted=!is("network-no-attempt");p->source=io;
 p->result.quiescent=!is("uncertain") && !is("network-no-attempt") && !is("not-drained");
 p->result.child_retired=p->result.worker_finished=p->result.tcp_empty=p->result.provider_drained=p->result.quiescent;
 p->result.complete=p->result.quiescent && !is("safe-failure");
 return p->result.complete;
}
void http_network_probe_emit(const struct http_network_probe *p){assert(p==&wifi_http5_probe && !key_live && p->result.quiescent);emits++;}
bool xhci_wifi_capture_stack5(bool (*exercise)(void *,struct mt76_net5 *),void *context){
 assert(context==&wifi_http5_probe && exercise==wifi_http5_exercise);
 if(is("early-parent-fail"))return false;
 wifi_stack5.selected=true;key_live=true;
 bool good=exercise(context,&wifi_stack5.net);
 /* Model the unavoidable unconditional erasure in unmodified mt76_wpa5.run.
  * An uncertain HTTP callback must never reach this line. */
 erasures++;key_live=false;return good && !is("late-parent-fail");
}
static void reset(const char *s){
 mode=s;memset(&wifi_http5_probe,0,sizeof wifi_http5_probe);memset(&wifi_http5_io,0,sizeof wifi_http5_io);
 memset(&wifi_stack5,0,sizeof wifi_stack5);memset(&wifi_stream5,0,sizeof wifi_stream5);
 wifi_http5_requested=wifi_http5_prepared=wifi_http5_capture_attempted=wifi_http5_callback_attempted=false;
 wifi_stack5.start=1000000;wifi_stream5.epoch=31;prepares=aborts=runs=erasures=held=emits=guard_calls=0;key_live=false;
}
int main(void){
 const char *safe[]={"good","safe-failure","late-parent-fail"};
 for(unsigned i=0;i<3;i++){
  reset(safe[i]);assert(xhci_wifi_http5_prepare() && prepares==1);assert(!xhci_wifi_http5_prepare());
  bool good=xhci_wifi_capture_http5();assert(erasures==1 && runs==1 && !key_live && !held);
  bool final=xhci_wifi_http5_finish_capture(good);assert(final==(!is("safe-failure") && !is("late-parent-fail")) && emits==1 && !aborts);
  assert(!xhci_wifi_capture_http5());
 }
 const char *unsafe[]={"uncertain","network-no-attempt","not-drained"};
 for(unsigned i=0;i<3;i++){
  reset(unsafe[i]);assert(xhci_wifi_http5_prepare());
  if(!setjmp(escape)){(void)xhci_wifi_capture_http5();assert(!"uncertain callback returned to WPA erasure");}
  assert(held==1 && !erasures && key_live && !emits && !wifi_http5_probe.result.quiescent);
 }
 reset("missing-adapter");assert(xhci_wifi_http5_prepare());assert(!xhci_wifi_http5_finish_capture(false) && aborts==1 && !runs && !erasures && emits==1);
 reset("early-parent-fail");assert(xhci_wifi_http5_prepare());assert(!xhci_wifi_capture_http5());assert(!xhci_wifi_http5_finish_capture(false) && aborts==1 && !runs && emits==1);
 reset("prepare-fail");assert(!xhci_wifi_http5_prepare());assert(!xhci_wifi_http5_finish_capture(false) && aborts==1 && !runs && emits==1);
 reset("abort-fail");assert(xhci_wifi_http5_prepare());if(!setjmp(escape)){(void)xhci_wifi_http5_finish_capture(false);assert(!"uncertain abort reached disk-save return");}assert(held==1 && !erasures && !emits);
 reset("unprepared");assert(!xhci_wifi_capture_http5());assert(!xhci_wifi_http5_finish_capture(false) && !emits && !prepares);
 reset("already-selected");wifi_stack5.selected=true;assert(!xhci_wifi_http5_prepare() && !prepares);
 puts("xhci-http5-bridge: PASS actual bridge;HOST native-guard/staticIO/originaldeadline/uncertain-no-erasure/early-abort models;not USB/key/native acceptance");return 0;
}
