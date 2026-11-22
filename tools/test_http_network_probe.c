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
static struct http_network_probe objects[112];
static bool connection_test;static unsigned disconnects;
static unsigned provider_identity;
static struct http_network_probe_io replacement_source;
static unsigned replacement_context,mutation_count;
static int is(const char *name){return !strcmp(scenario,name);}
static bool starts(const char *prefix){return !strncmp(scenario,prefix,strlen(prefix));}
static bool replacement_callback(void *p,uint64_t epoch){(void)p;(void)epoch;assert(!"mutated callback invoked");return false;}
static void mutate_at(const char *site){
 char prefix[32];int count=snprintf(prefix,sizeof prefix,"live-%s-",site);assert(count>0 && (size_t)count<sizeof prefix);
 if(mutation_count || !starts(prefix))return;
 mutation_count++;const char *kind=scenario+strlen(prefix);
 if(!strcmp(kind,"context"))source.context=object->io.context=&replacement_context;
 else if(!strcmp(kind,"epoch"))source.epoch=++object->io.epoch;
 else if(!strcmp(kind,"start"))source.start_us=++object->io.start_us;
 else if(!strcmp(kind,"deadline")){now=source.deadline_us+1;source.deadline_us=object->io.deadline_us+=1000000;}
 else if(!strcmp(kind,"current"))source.current=object->io.current=replacement_callback;
 else if(!strcmp(kind,"retry"))source.retry_ready=object->io.retry_ready=replacement_callback;
 else if(!strcmp(kind,"drained"))source.drained=object->io.drained=replacement_callback;
 else if(!strcmp(kind,"terminal"))source.terminal_rx_handoff=object->io.terminal_rx_handoff=replacement_callback;
 else if(!strcmp(kind,"override"))source.test_ip_override=object->io.test_ip_override=0xc0000201;
 else if(!strcmp(kind,"pointer"))object->source=(const struct http_network_probe_io *)(uintptr_t)1;
 else if(!strcmp(kind,"latched-pointer")){object->guard_failed=true;object->source=(const struct http_network_probe_io *)(uintptr_t)1;}
 else if(!strcmp(kind,"replacement-pointer")){replacement_source=source;object->source=&replacement_source;}
 else assert(!"unknown mutation");
}
static bool bytes_zero(const void *p,size_t n){const unsigned char *b=p;while(n--)if(*b++)return false;return true;}
static void assert_guard(const struct http_network_probe_io *expected){
 const struct http_network_probe_io *g=&object->io_guard;
 assert(object->source_guard==&source && g->context==expected->context && g->current==expected->current &&
  g->retry_ready==expected->retry_ready && g->drained==expected->drained && g->terminal_rx_handoff==expected->terminal_rx_handoff &&
  g->epoch==expected->epoch && g->start_us==expected->start_us && g->deadline_us==expected->deadline_us && g->test_ip_override==expected->test_ip_override);
}
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
bool net_prepare_connection_probe_worker(void){assert(connection_test);return net_prepare_probe_worker();}
bool net_probe_worker_prepared(void){return wp.prepared && wp.parked && wp.parked_waiters==1 && !wp.ever_active;}
bool net_probe_worker_snapshot(struct net_probe_worker_info *out){if(!wp.prepared)return false;*out=wp;return true;}
bool net_init_bounded_probe(void){return !is("init-fail");}
bool net_bounded_probe_init(struct net_bounded_probe *p,const struct net_bounded_probe_io *io,uint64_t epoch,uint64_t end){
 assert(p==&object->bootstrap && io==&object->net_io && io->context==object && epoch==source.epoch && end==source.deadline_us);
 p->identity=(uintptr_t)p;p->source=io;p->io=p->io_guard=*io;p->epoch=epoch;p->deadline_us=end;net_source=io;return true;
}
bool net_bounded_probe_init_connection(struct net_bounded_probe *p,const struct net_bounded_probe_io *io,const struct net_device_ops *provider,uint64_t epoch,uint64_t end){
 assert(connection_test && provider==(const struct net_device_ops *)&provider_identity);
 if(!net_init_bounded_probe())return false;
 bool good=net_bounded_probe_init(p,io,epoch,end);p->connection_scope=true;return good;
}
bool net_probe_worker_disconnect(struct net_bounded_probe *p){
 assert(connection_test && p==&object->bootstrap && p->connection_scope && p->closed &&
  object->result.child_retired && object->result.child_reaped && wp.finished && !wp.active && table_empty && !provider_closed);
 disconnects++;
 if(is("disconnect-refuse"))return false;
 p->disconnected=true;return true;
}
bool net_probe_worker_abort_connection(struct net_bounded_probe *p,const struct net_bounded_probe_io *io,uint64_t epoch,uint64_t deadline){
 assert(connection_test && p==&object->bootstrap && io==&object->net_io && p->failed && p->protocol_failed &&
  epoch==source.epoch && deadline==source.deadline_us && object->result.child_retired && wp.finished && !wp.ever_active && table_empty);
 disconnects++;p->closed=p->disconnected=true;p->source=NULL;memset(&p->io,0,sizeof p->io);memset(&p->io_guard,0,sizeof p->io_guard);bootstrap_finished=true;return true;
}

bool net_bounded_probe_poll(struct net_bounded_probe *p){assert(p==&object->bootstrap);p->polls++;pending=false;return net_source->current(net_source->context,p->epoch);}
bool net_bounded_probe_finish(struct net_bounded_probe *p){
 assert(p==&object->bootstrap);p->closed=true;p->source=NULL;memset(&p->io,0,sizeof p->io);memset(&p->io_guard,0,sizeof p->io_guard);bootstrap_finished=true;return !p->failed;
}
int64_t net_dhcp_request_bounded(struct net_bounded_probe *p){
 assert(net_source->current(net_source->context,p->epoch));p->polls+=3;
 if(starts("live-retry-") && !net_source->retry_ready(net_source->context,p->epoch)){p->failed=true;return -1;}
 if(is("dhcp-fail") || is("dhcp-pending")){p->failed=p->protocol_failed=true;pending=is("dhcp-pending");return -1;}return 0;
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
 assert(p==&source && !irq_flags);current_calls++;mutate_at("current");return !provider_closed && (starts("live-current-") || epoch==source.epoch) && !is("owner-fail");
}
static bool retry_ready(void *p,uint64_t epoch){assert(p==&source && !irq_flags && epoch==source.epoch);mutate_at("retry");return true;}
static bool provider_drained(void *p,uint64_t epoch){
 assert(p==&source && !irq_flags && epoch==source.epoch);mutate_at("drained");
 if(is("post-stop-pending") && wp.finished)pending=true;
 return !pending && !raw_ready && !decoded_cache && !borrowed;
}
static bool terminal_rx_handoff(void *p,uint64_t epoch){
 assert(p==&source && !irq_flags && epoch==source.epoch && !wp.active && wp.finished && wp.parked && wp.parked_waiters==1 && table_empty);
 assert(object->result.child_observed && object->result.child_reaped && object->result.child_retired && object->result.tcp_empty);
 if(connection_test)assert(object->result.disconnected && disconnects==1);
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
 scenario=name;object=&objects[i];connection_test=false;disconnects=0;now=1000000;irq_flags=512;go_time=0;
 wait_turns=cancels=reaps=releases=dns_calls=publishes=stops=prepare_calls=current_calls=0;
 child_clean=child_canceled=pending=atomic_pending=bootstrap_finished=false;table_empty=true;
 raw_ready=raw_posted=decoded_cache=borrowed=provider_closed=hardware_latched=false;
 handoffs=current_at_handoff=mutation_count=0;
 if(is("terminal-raw-pending"))raw_posted=true;
 memset(&wp,0,sizeof wp);net_source=NULL;
 source=(struct http_network_probe_io){.context=&source,.current=provider_current,.retry_ready=retry_ready,.drained=provider_drained,.terminal_rx_handoff=terminal_rx_handoff,.epoch=71};
}
int main(void){
#define LIVE_CASES(site) "live-" site "-context", \
 "live-" site "-epoch", \
 "live-" site "-start", \
 "live-" site "-deadline", \
 "live-" site "-current", \
 "live-" site "-retry", \
 "live-" site "-drained", \
 "live-" site "-terminal", \
 "live-" site "-override", \
 "live-" site "-pointer", \
 "live-" site "-latched-pointer", \
 "live-" site "-replacement-pointer"
 const char *cases[]={
  "good","vm-target","child-status","publish-fail","dhcp-fail","dns-fail",
  "route-fail","init-fail","dhcp-pending","handoff-fail","mutate-source","owner-fail",
  "table-never-empty","child-never-exit","stop-no-park","post-stop-pending","worker-finish-failed","reap-fail",
  "retire-fail","child-prepare-fail","no-irq","worker-prepare-fail","worker-no-park","terminal-raw-ready",
  "terminal-raw-pending","terminal-cache","terminal-loan","terminal-refuse","terminal-recursive","terminal-mutate",
  "terminal-mutate-both","terminal-clock","terminal-mutate-pointer",
  LIVE_CASES("current"),LIVE_CASES("drained"),LIVE_CASES("retry")};
 const unsigned count=sizeof cases/sizeof cases[0];
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
  setup(i,cases[i]);bool prepared=http_network_probe_prepare(object);
  if(is("child-prepare-fail") || is("no-irq") || is("worker-prepare-fail") || is("worker-no-park")){assert(!prepared && object->result.reason==HTTP_NET_PRELIVE);continue;}
  assert(prepared && prepare_calls==1);source.start_us=now;source.deadline_us=now+15000000;
  if(is("vm-target"))source.test_ip_override=0x0a000264;
  const struct http_network_probe_io admitted=source;
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
   if(!safe)assert_guard(&admitted);
   if(starts("live-")){
    assert(mutation_count==1 && object->guard_failed && object->result.reason==HTTP_NET_OWNER && !object->result.quiescent && !object->result.refs_scrubbed);
    /* Even a latched failure plus an invalid source pointer must not read it
     * or invoke the now-mutated callback on a later borrowed current check. */
    unsigned before=current_calls;
    assert(!object->net_io.current(object->net_io.context,admitted.epoch) && current_calls==before);
   }else if(!safe)assert(object->source==&source && object->io.context==&source && !object->result.refs_scrubbed);
  }
  if(object->result.quiescent){assert(!object->source && !object->source_guard && bytes_zero(&object->io_guard,sizeof object->io_guard) &&
   bytes_zero(&object->io,sizeof object->io) && bytes_zero(&object->net_io,sizeof object->net_io));}
  unsigned before_again=current_calls;
  assert(!http_network_probe_run(object,&source) && current_calls==before_again);
  assert(!http_network_probe_abort_prepare(object));
  assert(irq_flags==512);
 }
 setup(count,"abort-unrun");assert(http_network_probe_prepare(object));
 assert(http_network_probe_abort_prepare(object) && object->result.quiescent && object->result.worker_finished &&
  object->result.child_retired && !object->result.attempted && !object->result.go && !object->source && !object->source_guard && bytes_zero(&object->io_guard,sizeof object->io_guard));
 unsigned before_cancel=cancels;assert(http_network_probe_abort_prepare(object) && cancels==before_cancel);
 source.start_us=now;source.deadline_us=now+15000000;
 unsigned before_current=current_calls;
 assert(!http_network_probe_run(object,&source) && !object->source && !object->result.attempted && current_calls==before_current);
 setup(count+1,"abort-ambiguous");assert(http_network_probe_prepare(object));wp.handoff_started=1;
 assert(!http_network_probe_abort_prepare(object) && !object->result.quiescent && !cancels);
 setup(count+2,"missing-hook");assert(http_network_probe_prepare(object));source.start_us=now;source.deadline_us=now+15000000;source.terminal_rx_handoff=NULL;
 assert(!http_network_probe_run(object,&source) && !object->result.attempted && !handoffs);
 assert(http_network_probe_abort_prepare(object) && object->result.quiescent);

 const char *connection_cases[]={"good","good","disconnect-refuse","dhcp-fail","retire-fail","table-never-empty","init-fail"};
 for(unsigned i=0;i<sizeof connection_cases/sizeof *connection_cases;i++){
  setup(count+3+i,connection_cases[i]);connection_test=true;
  assert(http_network_probe_prepare_connection(object) && object->result.connection_scope);
  source.start_us=now;source.deadline_us=now+15000000;
  assert(!http_network_probe_run(object,&source) && !object->result.attempted);
  assert(!http_network_probe_run_connection(object,&source,NULL) && !object->result.attempted);
  bool good=http_network_probe_run_connection(object,&source,(const struct net_device_ops *)&provider_identity);
  if(is("good")){
   assert(good && disconnects==1 && object->result.disconnected && object->result.quiescent && handoffs==1);
   assert(object->result.child_retired && object->result.worker_finished && !object->source);
  }else if(is("dhcp-fail")){
   assert(!good && !object->result.complete && object->result.reason==HTTP_NET_DHCP_FAILED);
   assert(object->result.disconnected && object->result.quiescent && object->result.receiver_closed && object->result.refs_scrubbed);
   assert(object->result.child_retired && object->result.worker_finished && !object->source && handoffs==1 && disconnects==1);
   assert(object->bootstrap.failed && object->bootstrap.protocol_failed && object->bootstrap.closed && !publishes);
  }else{
   assert(!good && !object->result.complete && !object->result.disconnected && object->result.quiescent==is("init-fail") && !handoffs);
   if(!is("init-fail"))assert(object->source==&source && !object->result.refs_scrubbed);
  }
  unsigned before=current_calls;
  assert(!http_network_probe_run_connection(object,&source,(const struct net_device_ops *)&provider_identity) && before==current_calls);
 }
 puts("http-network-connection: seven HOST orchestration cases PASS including successful closure and mode/null/stale/disconnect/DHCP/retirement/table/init refusals;production net worker tested separately");
 printf("http-network-probe: %zu HOST cases PASS immutable window,atomic stop,FIN wait,admission descriptor/source/window guard at current/drained/retry,consuming terminal RX handoff/raw retention/cache-loan-mutation refusals; no guest/native acceptance\n",sizeof cases/sizeof cases[0]+3);
}
