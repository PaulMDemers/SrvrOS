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
#include <inttypes.h>
#include <stdlib.h>
#define WIFI_STACK5_TOTAL_US 15000000u
static struct {uint64_t start,services,tx_token,terminal_epoch,terminal_ticket,terminal_token,terminal_posts,terminal_completions;bool selected,running,failed,boundary,terminal_handoff,terminal_pending,terminal_ready,terminal_provider_closing;} wifi_stack5;
static struct mt76_net5 wifi_stack5_net;
static bool wifi_http_reconnect5_reserved;
static struct {uint64_t epoch,posts[2];unsigned phase,fault_phase;struct {uint64_t ticket;} loan;
 bool attempted,stack_mode,failed,pending[2],ready_rx,cached_present;} wifi_stream5;
static struct {unsigned round;} wifi_fence5_native;
static struct {bool command_pending;} xhci;
static uint64_t irq_flags=512;
static unsigned held_rows,syncs,writes,freezes;
static unsigned clears;
static size_t panel_start;
static bool mutate_during_output;
static char held_log[8192];static size_t held_length;
static uint64_t event_irq_save(void){uint64_t f=irq_flags;irq_flags=0;freezes++;return f;}
static void event_irq_restore(uint64_t f){assert(!irq_flags);irq_flags=f;}
static unsigned prepares,aborts,runs,erasures,held,emits,guard_calls;
static const char *mode;static jmp_buf escape;static bool key_live;
static bool is(const char *s){return !strcmp(mode,s);}
static bool wifi_stack5_probe_current(void *c,uint64_t e){assert(c==&wifi_stream5 && e==31);guard_calls++;return true;}
static bool wifi_stack5_terminal_rx_handoff(void *c,uint64_t e){return wifi_stack5_probe_current(c,e);}
static bool wifi_stack5_probe_retry(void *c,uint64_t e){return wifi_stack5_probe_current(c,e);}
static bool wifi_stack5_probe_observe(void *c,uint64_t e,struct mt76_stack5_observation *o){assert(wifi_stack5_probe_current(c,e));o->drained=!is("not-drained");return true;}
bool xhci_wifi_capture_stack5(bool (*exercise)(void *,struct mt76_net5 *),void *context);
static void mutate_live_state(void);
void console_printf(const char *fmt,...){
 assert(irq_flags==512); /* no console lock/render/UART work in snapshot exclusion */
 if(!strstr(fmt,"wifi-http5-held-") && !clears){assert(!key_live);assert(strstr(fmt,"wifi-http5-"));return;}
 /* Production console %u consumes uint64_t. Adapt that contract for libc QA. */
 char adapted[2048],row[2048];size_t n=0;
 for(size_t i=0;fmt[i];i++){if(fmt[i]=='%' && fmt[i+1]=='u'){
  memcpy(adapted+n,"%" PRIu64,sizeof("%" PRIu64)-1);n+=sizeof("%" PRIu64)-1;i++;
 }else adapted[n++]=fmt[i];}adapted[n]=0;
 va_list ap;va_start(ap,fmt);int count=vsnprintf(row,sizeof row,adapted,ap);va_end(ap);
 assert(count>0 && count<1024); /* one saved-console record, even worst scalar values */
 if(clears)assert(count<=120); /* one physical row at 2880px / 24px text cells */
 assert(held_length+(size_t)count<sizeof held_log);
 memcpy(held_log+held_length,row,(size_t)count+1);held_length+=(size_t)count;held_rows++;
 /* Simulate preemption after the first console call. Later rows must still
  * print the IRQ-off frozen values, never read changing source records. */
 if(mutate_during_output){mutate_during_output=false;mutate_live_state();}
}
void console_write(const char *s){assert(irq_flags==512 && strstr(s,"key/provider/DMA retained") && strstr(s,"photograph the diagnostic rows above"));writes++;}
bool console_sync(void){assert(irq_flags==512);syncs++;return true;}
void console_clear(void){assert(irq_flags==512);clears++;panel_start=held_length;}
void console_set_framebuffer_muted(bool muted){assert(irq_flags==512 && !muted);}
#include "../kernel/src/drivers/xhci_wifi_http5.inc"
static void mutate_live_state(void){
 wifi_http5_probe.result.phase=HTTP_NET_DONE;wifi_http5_probe.result.reason=HTTP_NET_OK;
 wifi_http5_probe.result.dhcp=999;wifi_http5_probe.worker.polls=999;
 wifi_stack5.services=999;wifi_stream5.posts[0]=999;wifi_stream5.pending[0]=false;
}
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
 assert(io->current==wifi_stack5_probe_current && io->retry_ready==wifi_stack5_probe_retry && io->drained==wifi_http5_drained && io->terminal_rx_handoff==wifi_stack5_terminal_rx_handoff);
 assert(io->epoch==31 && io->start_us==1000000 && io->deadline_us==16000000 && !io->test_ip_override);
 assert(io->current(io->context,io->epoch) && io->retry_ready(io->context,io->epoch));
 assert(io->drained(io->context,io->epoch)==!is("not-drained"));
 runs++;
 p->result.phase=HTTP_NET_DNS;p->result.reason=HTTP_NET_DNS_FAILED;
 p->result.dhcp=1;p->result.retries=2;p->result.bootstrap_polls=13;p->result.worker_allocated=true;
 p->worker=(struct net_probe_worker_info){.prepared=1,.parked=1,.parked_waiters=1,.polls=17,.turns=3,.worker_identity=0x3456789abcdef,.supervisor_identity=0x456789abcdef};
 p->guard_failed=true;p->bootstrap_open=true;
 if(is("deadline")){p->result.phase=HTTP_NET_DRAIN;p->result.reason=HTTP_NET_CLOCK;p->result.go=1;p->worker.active=1;p->worker.parked=0;}
 if(is("worker-failed")){p->result.phase=HTTP_NET_FINISH;p->result.reason=HTTP_NET_WORKER_FAILED;p->worker.failed=1;}
 if(is("dhcp-uncertain")){p->result.phase=HTTP_NET_DHCP;p->result.reason=HTTP_NET_DHCP_FAILED;p->result.dhcp=0;}
 p->result.child_status=23;p->result.child_observed=true;p->result.child_reaped=true;
 p->result.attempted=!is("network-no-attempt");p->source=io;
 p->result.quiescent=!is("uncertain") && !is("network-no-attempt") && !is("not-drained") && !is("deadline") && !is("worker-failed") && !is("dhcp-uncertain");
 p->result.handoff=!is("safe-bootstrap");
 p->result.receiver_closed=p->result.child_retired=p->result.worker_finished=p->result.tcp_empty=p->result.provider_drained=p->result.quiescent;
 if(is("safe-bootstrap"))p->result.receiver_closed=false;
 if(is("missing-receiver"))p->result.receiver_closed=false;
 wifi_stack5.terminal_handoff=wifi_stack5.terminal_provider_closing=p->result.quiescent && p->result.handoff;
 if(is("missing-child"))p->result.child_retired=false;
 if(is("missing-worker"))p->result.worker_finished=false;
 if(is("missing-tcp"))p->result.tcp_empty=false;
 if(is("missing-provider"))p->result.provider_drained=false;
 p->result.complete=p->result.quiescent && !is("safe-failure") && !is("safe-bootstrap");
 return p->result.complete;
}
void http_network_probe_emit(const struct http_network_probe *p){assert(p==&wifi_http5_probe && !key_live && p->result.quiescent);emits++;}
bool xhci_wifi_capture_stack5(bool (*exercise)(void *,struct mt76_net5 *),void *context){
 assert(context==&wifi_http5_probe && exercise==wifi_http5_exercise);
 if(is("early-parent-fail"))return false;
 wifi_stack5.selected=true;key_live=true;
 wifi_stack5.running=true;wifi_stack5.services=43;wifi_stack5_net.count=2;
 wifi_stack5_net.faults=7;wifi_stack5_net.enqueued=5;wifi_stack5_net.submitted=4;wifi_stack5_net.completed=3;
 wifi_stream5.posts[0]=11;wifi_stream5.posts[1]=19;wifi_stream5.phase=3;wifi_stream5.fault_phase=2;
 wifi_stream5.pending[0]=true;wifi_stream5.pending[1]=true;wifi_stream5.ready_rx=true;
 wifi_stream5.cached_present=true;wifi_stream5.loan.ticket=0x123456789abcdef;wifi_stack5.tx_token=0x23456789abcdef;
 wifi_stack5.boundary=true;wifi_fence5_native.round=2;xhci.command_pending=true;
 bool good=exercise(is("admission")?NULL:context,&wifi_stack5_net);
 /* Model the unavoidable unconditional erasure in unmodified mt76_wpa5.run.
  * An uncertain HTTP callback must never reach this line. */
 erasures++;key_live=false;return good && !is("late-parent-fail");
}
static void reset(const char *s){
 mode=s;memset(&wifi_http5_probe,0,sizeof wifi_http5_probe);memset(&wifi_http5_io,0,sizeof wifi_http5_io);
 memset(&wifi_stack5,0,sizeof wifi_stack5);memset(&wifi_stack5_net,0,sizeof wifi_stack5_net);memset(&wifi_stream5,0,sizeof wifi_stream5);
 memset(&wifi_http5_held_state,0,sizeof wifi_http5_held_state);
 memset(&wifi_fence5_native,0,sizeof wifi_fence5_native);memset(&xhci,0,sizeof xhci);
 held_rows=syncs=writes=freezes=0;held_length=0;held_log[0]=0;irq_flags=512;mutate_during_output=false;
 clears=0;panel_start=0;
 wifi_http5_requested=wifi_http5_prepared=wifi_http5_capture_attempted=wifi_http5_callback_attempted=false;
 wifi_http5_active_probe=&wifi_http5_probe;wifi_http5_connection_capture_attempted=false;
 wifi_http5_connection_callback_probe=NULL;wifi_http_reconnect5_reserved=false;
 wifi_stack5.start=1000000;wifi_stream5.epoch=31;prepares=aborts=runs=erasures=held=emits=guard_calls=0;key_live=false;
}
int main(void){
 const char *safe[]={"good","safe-failure","late-parent-fail","safe-bootstrap"};
 for(unsigned i=0;i<sizeof safe/sizeof *safe;i++){
  reset(safe[i]);assert(xhci_wifi_http5_prepare() && prepares==1);assert(!xhci_wifi_http5_prepare());
  bool good=xhci_wifi_capture_http5();assert(erasures==1 && runs==1 && !key_live && !held && !held_rows && !freezes && !writes && !syncs);
  bool final=xhci_wifi_http5_finish_capture(good);assert(final==(!is("safe-failure") && !is("late-parent-fail") && !is("safe-bootstrap")) && emits==1 && !aborts);
  assert(!xhci_wifi_capture_http5());
 }
 const char *unsafe[]={"uncertain","network-no-attempt","not-drained","deadline","worker-failed","dhcp-uncertain",
  "missing-child","missing-worker","missing-tcp","missing-provider","missing-receiver","admission"};
 for(unsigned i=0;i<sizeof unsafe/sizeof *unsafe;i++){
  reset(unsafe[i]);assert(xhci_wifi_http5_prepare());mutate_during_output=true;
  if(!setjmp(escape)){(void)xhci_wifi_capture_http5();assert(!"uncertain callback returned to WPA erasure");}
  assert(held==1 && !erasures && key_live && !emits && held_rows==15 && writes==1 && syncs==1 && freezes==1 && clears==1);
  assert(!strncmp(held_log+panel_start,"Wi-Fi HTTP test STOPPED",22));
  assert(strstr(held_log+panel_start,"Codes: site=") && strstr(held_log+panel_start,"No commands needed."));
  if(is("deadline") && getenv("SRVROS_HELD_PANEL_FIXTURE")){
   FILE *f=fopen(getenv("SRVROS_HELD_PANEL_FIXTURE"),"wb");assert(f);
   assert(fputs(held_log+panel_start,f)>=0 && !fclose(f));
  }
  assert(wifi_http5_held_state.site==(is("admission")?1u:2u));
  assert(strstr(held_log,is("admission")?
   "requested=1 prepared=1 capture_attempted=1 callback_attempted=0":
   "requested=1 prepared=1 capture_attempted=1 callback_attempted=1"));
  assert(strstr(held_log,"services=43") && strstr(held_log,"tx_posts=11 rx_posts=19"));
  assert(strstr(held_log,"tx=1 rx=1 ready_rx=1 cached_rx=1 borrowed_rx=1 tx_token_present=1 boundary=1 fence_round=2 command_pending=1 retained=1"));
  assert(!wifi_http5_held_state.worker.worker_identity && !wifi_http5_held_state.worker.supervisor_identity);
  assert(!strstr(held_log,"999") && !strstr(held_log,"123456789") && !strstr(held_log,"23456789"));
  if(!is("admission")){
   assert(strstr(held_log,"child_status=23 child_observed=1 child_reaped=1"));
   assert(strstr(held_log,"polls=17 turns=3"));
   assert(wifi_http5_held_state.result.reason==(is("deadline")?HTTP_NET_CLOCK:is("worker-failed")?HTTP_NET_WORKER_FAILED:is("dhcp-uncertain")?HTTP_NET_DHCP_FAILED:HTTP_NET_DNS_FAILED));
   char code[192];snprintf(code,sizeof code,"wifi-http5-held-code: version=1 site=2 phase=%" PRIu64 " reason=%" PRIu64 " child_status=23 connection=0 disconnected=0\n",wifi_http5_held_state.result.phase,wifi_http5_held_state.result.reason);
   assert(strstr(held_log,code)); /* same frozen values despite output preemption */
   assert(guard_calls==3); /* only run's IO verification; holding invokes no guard */
  }else assert(!guard_calls && !runs);
  /* A second snapshot request cannot replace the first terminal record. */
  wifi_http5_freeze_hold(99);assert(wifi_http5_held_state.site==(is("admission")?1u:2u));
 }
 reset("missing-adapter");assert(xhci_wifi_http5_prepare());assert(!xhci_wifi_http5_finish_capture(false) && aborts==1 && !runs && !erasures && emits==1);
 reset("early-parent-fail");assert(xhci_wifi_http5_prepare());assert(!xhci_wifi_capture_http5());assert(!xhci_wifi_http5_finish_capture(false) && aborts==1 && !runs && emits==1);
 reset("prepare-fail");assert(!xhci_wifi_http5_prepare());assert(!xhci_wifi_http5_finish_capture(false) && aborts==1 && !runs && emits==1);
 reset("abort-fail");assert(xhci_wifi_http5_prepare());if(!setjmp(escape)){(void)xhci_wifi_http5_finish_capture(false);assert(!"uncertain abort reached disk-save return");}assert(held==1 && !erasures && !emits && held_rows==15 && wifi_http5_held_state.site==3);
 reset("finish-not-quiescent");assert(xhci_wifi_http5_prepare());wifi_http5_probe.result.attempted=true;
 if(!setjmp(escape)){(void)xhci_wifi_http5_finish_capture(false);assert(!"finish uncertainty returned");}
 assert(held==1 && !aborts && !erasures && !emits && held_rows==15 && wifi_http5_held_state.site==4);
 /* Every printed scalar, including unexpected maximum-width counts, fits the
  * console's 1KiB saved-record bound; identities never appear in any format. */
 reset("max-width");memset(&wifi_http5_held_state.result,0,sizeof wifi_http5_held_state.result);
 memset(&wifi_http5_held_state.worker,0xff,sizeof wifi_http5_held_state.worker);
 wifi_http5_held_state.result.phase=wifi_http5_held_state.result.reason=UINT64_MAX;
 wifi_http5_held_state.result.go=wifi_http5_held_state.result.child_status=UINT64_MAX;
 wifi_http5_held_state.result.dhcp=wifi_http5_held_state.result.retries=wifi_http5_held_state.result.dns=UINT64_MAX;
 wifi_http5_held_state.result.route=wifi_http5_held_state.result.bootstrap_polls=UINT64_MAX;
 wifi_http5_held_state.result.worker_polls=wifi_http5_held_state.result.worker_turns=UINT64_MAX;
 wifi_http5_held_state.result.supervisor_turns=wifi_http5_held_state.result.elapsed_us=UINT64_MAX;
 wifi_http5_held_state.site=UINT64_MAX;
 wifi_http5_held_state.requested=UINT64_MAX;
 wifi_http5_held_state.prepared=UINT64_MAX;
 wifi_http5_held_state.capture_attempted=UINT64_MAX;
 wifi_http5_held_state.callback_attempted=UINT64_MAX;
 wifi_http5_held_state.guard_failed=UINT64_MAX;
 wifi_http5_held_state.checking=UINT64_MAX;
 wifi_http5_held_state.bootstrap_open=UINT64_MAX;
 wifi_http5_held_state.handoff_attempted=UINT64_MAX;
 wifi_http5_held_state.worker_stopped=UINT64_MAX;
 wifi_http5_held_state.selected=UINT64_MAX;
 wifi_http5_held_state.running=UINT64_MAX;
 wifi_http5_held_state.stack_failed=UINT64_MAX;
 wifi_http5_held_state.stream_failed=UINT64_MAX;
 wifi_http5_held_state.stream_phase=UINT64_MAX;
 wifi_http5_held_state.fault_phase=UINT64_MAX;
 wifi_http5_held_state.provider_state=UINT64_MAX;
 wifi_http5_held_state.provider_pending=UINT64_MAX;
 wifi_http5_held_state.queued=UINT64_MAX;
 wifi_http5_held_state.provider_faults=UINT64_MAX;
 wifi_http5_held_state.services=UINT64_MAX;
 wifi_http5_held_state.enqueued=UINT64_MAX;
 wifi_http5_held_state.submitted=UINT64_MAX;
 wifi_http5_held_state.completed=UINT64_MAX;
 wifi_http5_held_state.received=UINT64_MAX;
 wifi_http5_held_state.tx_posts=UINT64_MAX;
 wifi_http5_held_state.rx_posts=UINT64_MAX;
 wifi_http5_held_state.tx_pending=UINT64_MAX;
 wifi_http5_held_state.rx_pending=UINT64_MAX;
 wifi_http5_held_state.ready_rx=UINT64_MAX;
 wifi_http5_held_state.cached_rx=UINT64_MAX;
 wifi_http5_held_state.borrowed_rx=UINT64_MAX;
 wifi_http5_held_state.tx_token=UINT64_MAX;
 wifi_http5_held_state.boundary=UINT64_MAX;
 wifi_http5_held_state.fence_round=UINT64_MAX;
 wifi_http5_held_state.command_pending=UINT64_MAX;
 wifi_http5_emit_hold();assert(held_rows==7);
 /* Connection observations bind the selected retained object. The unused
  * second object must not inherit the first object's callback-attempt flag. */
 reset("connection-observation");static struct http_network_probe pair[2];
 wifi_http_reconnect5_reserved=wifi_http5_connection_capture_attempted=true;
 wifi_http5_connection_callback_probe=&pair[0];
 for(unsigned g=0;g<2;g++){
  memset(&wifi_http5_held_state,0,sizeof wifi_http5_held_state);
  pair[g].result.prepared=pair[g].result.connection_scope=true;wifi_http5_active_probe=&pair[g];
  wifi_http5_freeze_hold(7);
  assert(wifi_http5_held_state.requested && wifi_http5_held_state.prepared && wifi_http5_held_state.capture_attempted);
  assert(wifi_http5_held_state.callback_attempted==(g==0));
  assert(!wifi_http5_requested && !wifi_http5_prepared && !wifi_http5_capture_attempted && !wifi_http5_callback_attempted);
 }
 reset("unprepared");assert(!xhci_wifi_capture_http5());assert(!xhci_wifi_http5_finish_capture(false) && !emits && !prepares);
 reset("already-selected");wifi_stack5.selected=true;assert(!xhci_wifi_http5_prepare() && !prepares);
 puts("xhci-http5-bridge: PASS actual bridge;HOST native-guard/staticIO/originaldeadline/uncertain-no-erasure/frozen-numeric-held-receipts/early-abort models;not USB/key/native acceptance");return 0;
}
