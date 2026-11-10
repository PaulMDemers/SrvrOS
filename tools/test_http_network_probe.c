/* Production portable orchestration with HOST process/network/IRQ models.
 * Actual ELF/TCP/timer/provider execution belongs to the separate guest test. */
#include <srvros/http_network_probe.h>
#include <srvros/timer.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
static const char *scenario;
static uint64_t now,irq_flags=512,go_time;
static unsigned wait_turns,cancels,reaps,releases,dns_calls,publishes,stops,prepare_calls,current_calls;
static bool child_clean,child_canceled,table_empty,pending,atomic_pending,bootstrap_finished;
static bool raw_ready,raw_posted,decoded_cache,borrowed,provider_closed,hardware_latched;
static unsigned handoffs,current_at_handoff;
static struct net_probe_worker_info wp;
static const struct net_bounded_probe_io *net_source;
static struct http_network_probe *object;
static struct http_network_probe_io source;
static struct http_network_probe objects[64];
static int is(const char *name){return !strcmp(scenario,name);}
uint64_t http_network_test_irq_save(void){uint64_t f=irq_flags;irq_flags=0;return f;}
void http_network_test_irq_restore(uint64_t f){
 assert(!f || !object || !object->checking); /* only IRQ re-enable may preempt a checked callback */
 if(f && atomic_pending && wp.active){pending=true;atomic_pending=false;}
 if(f && wp.stop_requested && !hardware_latched){
  hardware_latched=true;
  if(is("terminal-raw-ready"))raw_ready=true;
  if(is("terminal-cache"))decoded_cache=true;
  if(is("terminal-loan"))borrowed=true;
 }
 irq_flags=f;
}
uint64_t timer_monotonic_us(void){now+=10;return now;}
uint64_t timer_ticks(void){return now/10000;}
bool timer_idle_wakeup_ready(void){return !is("no-irq");}
struct process *process_current(void){return NULL;}
bool http_child_prepare(struct http_child_owner *o,bool preflight){
 assert(!preflight && !o->self);prepare_calls++;o->self=(uintptr_t)o;o->token=9;o->pid=42;o->ready_us=now;
 o->prepared=!is("child-prepare-fail");return o->prepared;
}
bool http_child_publish(struct http_child_owner *o,uint64_t epoch,uint64_t start,uint64_t end,uint32_t ip){
 assert(o==&object->child && wp.active && bootstrap_finished && epoch==source.epoch && start==source.start_us && end==source.deadline_us && ip);
 publishes++;if(is("publish-fail"))return false;
 o->published=true;go_time=now;table_empty=false;return true;
}
bool http_child_observe(struct http_child_owner *o,uint64_t *status){
 assert(o==&object->child);if(!child_clean || o->reaped)return false;
 *status=is("child-status")?21:0;o->observed=true;o->status=*status;return true;
}
bool http_child_reap(struct http_child_owner *o){
 assert(o==&object->child && child_clean && !wp.active && (!object->result.attempted || wp.finished) && table_empty);
 if(is("reap-fail"))return false;
 reaps++;o->reaped=true;return true;
}
bool http_child_cancel(struct http_child_owner *o){assert(o==&object->child);cancels++;child_canceled=true;return true;}
bool http_child_retire(struct http_child_owner *o){
 assert(o==&object->child);if(!child_clean)return false;
 assert(!wp.active && (!wp.prepared || wp.finished || !object->result.attempted));
 if(is("retire-fail"))return false;
 o->retired=true;releases++;return true;
}
bool net_prepare_probe_worker(void){wp.prepared=!is("worker-prepare-fail");wp.worker_identity=17;wp.supervisor_identity=11;return wp.prepared;}
bool net_probe_worker_prepared(void){return wp.prepared && wp.parked && wp.parked_waiters==1 && !wp.ever_active;}
bool net_probe_worker_snapshot(struct net_probe_worker_info *out){if(!wp.prepared)return false;*out=wp;return true;}
bool net_init_bounded_probe(void){return !is("init-fail");}
bool net_bounded_probe_init(struct net_bounded_probe *p,const struct net_bounded_probe_io *io,uint64_t epoch,uint64_t end){
 assert(p==&object->bootstrap && io==&object->net_io && io->context==object && epoch==source.epoch && end==source.deadline_us);
 p->identity=(uintptr_t)p;p->source=io;p->io=p->io_guard=*io;p->epoch=epoch;p->deadline_us=end;net_source=io;return true;
}
bool net_bounded_probe_poll(struct net_bounded_probe *p){assert(p==&object->bootstrap);p->polls++;pending=false;return net_source->current(net_source->context,p->epoch);}
bool net_bounded_probe_finish(struct net_bounded_probe *p){
 assert(p==&object->bootstrap);p->closed=true;p->source=NULL;memset(&p->io,0,sizeof p->io);memset(&p->io_guard,0,sizeof p->io_guard);bootstrap_finished=true;return !p->failed;
}
int64_t net_dhcp_request_bounded(struct net_bounded_probe *p){
 assert(net_source->current(net_source->context,p->epoch));p->polls+=3;
 if(is("dhcp-fail") || is("dhcp-pending")){p->failed=true;pending=is("dhcp-pending");return -1;}return 0;
}
int64_t net_dns_resolve_bounded(struct net_bounded_probe *p,const char *name,uint32_t *ip){
 assert(p==&object->bootstrap && !strcmp(name,"httpbin.org"));dns_calls++;p->polls+=2;
 if(is("dns-fail")){p->failed=true;return -1;}*ip=0xc0000201;return 0;
}
int64_t net_prepare_route_bounded(struct net_bounded_probe *p,uint32_t ip){
 assert(p==&object->bootstrap && ip);p->polls++;
 if(is("route-fail")){p->failed=true;return -1;}return 0;
}
bool net_probe_worker_handoff(struct net_bounded_probe *p){
 assert(net_probe_worker_prepared() && p->source==&object->net_io && p->source->context==object);
 wp.handoff_started=1;
 if(is("handoff-fail")){wp.failed=1;return false;}
 assert(net_bounded_probe_finish(p));wp.ever_active=wp.active=1;wp.parked=wp.parked_waiters=0;
 wp.start_us=source.start_us;wp.deadline_us=source.deadline_us;wp.polls=p->polls;
 if(is("mutate-source"))source.epoch++;
 return true;
}
bool net_probe_worker_current(void){
 if(!wp.active || wp.failed)return false;
 bool good=net_source->current(net_source->context,object->io.epoch);
 if(!good){wp.failed=1;wp.active=0;}
 return good;
}
bool net_probe_worker_empty(void){
 if(!net_probe_worker_current())return false;
 if(child_clean && !is("table-never-empty") && (child_canceled || now-go_time>=800000))table_empty=true;
 if(table_empty && child_clean)atomic_pending=true;
 return table_empty;
}
bool net_probe_worker_stop(void){
 if(wp.ever_active)assert(!irq_flags && child_clean && table_empty && !pending);
 stops++;wp.handoff_started=wp.stop_requested=1;wp.active=0;wp.parked=wp.parked_waiters=0;return true;
}
bool net_probe_worker_finish(void){
 if(!wp.stop_requested || !wp.parked || wp.parked_waiters!=1 || !table_empty)return false;
 wp.finished=1;if(is("worker-finish-failed"))wp.failed=1;return !wp.failed;
}
static bool provider_current(void *p,uint64_t epoch){
 assert(p==&source && !irq_flags);current_calls++;return !provider_closed && epoch==source.epoch && !is("owner-fail");
}
static bool retry_ready(void *p,uint64_t epoch){assert(p==&source && !irq_flags && epoch==source.epoch);return true;}
static bool provider_drained(void *p,uint64_t epoch){
 assert(p==&source && !irq_flags && epoch==source.epoch);
 if(is("post-stop-pending") && wp.finished)pending=true;
 return !pending && !raw_ready && !decoded_cache && !borrowed;
}
static bool terminal_rx_handoff(void *p,uint64_t epoch){
 assert(p==&source && !irq_flags && epoch==source.epoch && !wp.active && wp.finished && wp.parked && wp.parked_waiters==1 && table_empty);
 assert(object->result.child_observed && object->result.child_reaped && object->result.child_retired && object->result.tcp_empty);
 handoffs++;
 if(is("post-stop-pending"))pending=true; /* unknown debt is NOT raw owner handoff */
 if(pending || decoded_cache || borrowed || is("terminal-refuse"))return false;
 if(is("terminal-recursive")){assert(!net_source->current(net_source->context,epoch));return true;}
 if(is("terminal-mutate-pointer")){object->source=(const struct http_network_probe_io *)(uintptr_t)1;return true;}
 if(is("terminal-mutate")){source.epoch++;return true;}
 if(is("terminal-mutate-both")){source.epoch++;object->io.epoch++;return true;}
 if(is("terminal-clock")){now=source.deadline_us;return true;}
 provider_closed=true;current_at_handoff=current_calls;
 return true;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t end){
 (void)q;(void)fn;(void)arg;(void)end;assert(irq_flags==512);wait_turns++;now+=10000;
 if(wp.prepared && !wp.active && !is("worker-no-park") && (!is("stop-no-park") || !wp.stop_requested)){wp.parked=wp.parked_waiters=1;}
 if((object->child.published || child_canceled) && !is("child-never-exit") && wait_turns>=4)child_clean=true;
 if(child_canceled && !object->child.published)table_empty=true;
 if(wp.active){wp.polls++;wp.turns++;}
 return true;
}
void scheduler_yield(void){assert(0);}
void console_printf(const char *fmt,...){(void)fmt;assert(0);}
static void setup(unsigned i,const char *name){
 scenario=name;object=&objects[i];now=1000000;irq_flags=512;go_time=0;
 wait_turns=cancels=reaps=releases=dns_calls=publishes=stops=prepare_calls=current_calls=0;
 child_clean=child_canceled=pending=atomic_pending=bootstrap_finished=false;table_empty=true;
 raw_ready=raw_posted=decoded_cache=borrowed=provider_closed=hardware_latched=false;
 handoffs=current_at_handoff=0;
 if(is("terminal-raw-pending"))raw_posted=true;
 memset(&wp,0,sizeof wp);net_source=NULL;
 source=(struct http_network_probe_io){.context=&source,.current=provider_current,.retry_ready=retry_ready,.drained=provider_drained,.terminal_rx_handoff=terminal_rx_handoff,.epoch=71};
}
int main(void){
 const char *cases[]={"good","vm-target","child-status","publish-fail","dhcp-fail","dns-fail","route-fail","init-fail","dhcp-pending","handoff-fail","mutate-source","owner-fail","table-never-empty","child-never-exit","stop-no-park","post-stop-pending","worker-finish-failed","reap-fail","retire-fail","child-prepare-fail","no-irq","worker-prepare-fail","worker-no-park","terminal-raw-ready","terminal-raw-pending","terminal-cache","terminal-loan","terminal-refuse","terminal-recursive","terminal-mutate","terminal-mutate-both","terminal-clock","terminal-mutate-pointer"};
 const unsigned count=sizeof cases/sizeof cases[0];
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
  setup(i,cases[i]);bool prepared=http_network_probe_prepare(object);
  if(is("child-prepare-fail") || is("no-irq") || is("worker-prepare-fail") || is("worker-no-park")){assert(!prepared && object->result.reason==HTTP_NET_PRELIVE);continue;}
  assert(prepared && prepare_calls==1);source.start_us=now;source.deadline_us=now+15000000;
  if(is("vm-target"))source.test_ip_override=0x0a000264;
  bool result=http_network_probe_run(object,&source);
  if(is("good") || is("vm-target") || is("terminal-raw-ready") || is("terminal-raw-pending")){
   assert(result && object->result.complete && object->result.quiescent && object->result.child_reaped && object->result.child_retired &&
    object->result.worker_finished && object->result.tcp_empty && object->result.provider_drained && !object->source && !object->answer && !pending && object->result.receiver_closed && provider_closed && handoffs==1);
   assert(current_calls==current_at_handoff); /* consumed current() must not be reopened */
   assert(raw_ready==is("terminal-raw-ready") && raw_posted==is("terminal-raw-pending"));
   assert(object->result.worker_turns>=80 && dns_calls==(is("vm-target")?0u:1u));
  }else{
   if(result || object->result.complete || !object->result.reason)fprintf(stderr,"unexpected %s result=%d complete=%d reason=%llu\n",scenario,result,object->result.complete,(unsigned long long)object->result.reason);
   assert(!result && !object->result.complete && object->result.reason);
   if(is("terminal-mutate-pointer")){
    assert(object->source==(const struct http_network_probe_io *)(uintptr_t)1 && object->guard_failed && object->result.reason==HTTP_NET_OWNER && !object->result.quiescent && !object->result.refs_scrubbed);
    object->source=&source; /* Fixture-only restoration AFTER retained refusal. */
   }
   bool safe=is("child-status") || is("publish-fail") || is("dhcp-fail") || is("dns-fail") || is("route-fail") || is("init-fail") || is("worker-finish-failed");
   assert(object->result.quiescent==safe);
   if(!safe)assert(object->source==&source && object->io.context==&source && !object->result.refs_scrubbed);
  }
  unsigned before_again=current_calls;
  assert(!http_network_probe_run(object,&source) && current_calls==before_again);
  assert(!http_network_probe_abort_prepare(object));
  assert(irq_flags==512);
 }
 setup(count,"abort-unrun");assert(http_network_probe_prepare(object));
 assert(http_network_probe_abort_prepare(object) && object->result.quiescent && object->result.worker_finished &&
  object->result.child_retired && !object->result.attempted && !object->result.go && !object->source);
 unsigned before_cancel=cancels;assert(http_network_probe_abort_prepare(object) && cancels==before_cancel);
 source.start_us=now;source.deadline_us=now+15000000;
 unsigned before_current=current_calls;
 assert(!http_network_probe_run(object,&source) && !object->source && !object->result.attempted && current_calls==before_current);
 setup(count+1,"abort-ambiguous");assert(http_network_probe_prepare(object));wp.handoff_started=1;
 assert(!http_network_probe_abort_prepare(object) && !object->result.quiescent && !cancels);
 setup(count+2,"missing-hook");assert(http_network_probe_prepare(object));source.start_us=now;source.deadline_us=now+15000000;source.terminal_rx_handoff=NULL;
 assert(!http_network_probe_run(object,&source) && !object->result.attempted && !handoffs);
 assert(http_network_probe_abort_prepare(object) && object->result.quiescent);
 printf("http-network-probe: %zu HOST cases PASS immutable window,atomic stop,FIN wait,consuming terminal RX handoff/raw retention/cache-loan-mutation refusals; no guest/native acceptance\n",sizeof cases/sizeof cases[0]+3);
}
