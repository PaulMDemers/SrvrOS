#include <srvros/http_network_probe.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <limits.h>
#include <stddef.h>
#define SUPERVISOR_TURNS 150000u
#ifdef SRVROS_HTTP_NETWORK_HOST_TEST
extern uint64_t http_network_test_irq_save(void);
extern void http_network_test_irq_restore(uint64_t);
static uint64_t lock(void){return http_network_test_irq_save();}
static void unlock(uint64_t f){http_network_test_irq_restore(f);}
#else
static uint64_t lock(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static void unlock(uint64_t f){if(f&(1ull<<9))__asm__ volatile("sti":::"memory");}
#endif
static bool zero(const void *v,size_t n){const uint8_t *p=v;unsigned bits=0;while(n--)bits|=*p++;return !bits;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *v,size_t n){volatile uint8_t *p=v;while(n--)*p++=0;}
static bool fail(struct http_network_probe *s,unsigned reason){if(!s->result.reason)s->result.reason=reason;return false;}
static bool same(const struct http_network_probe_io *a,const struct http_network_probe_io *b){
 return a->context==b->context && a->current==b->current && a->retry_ready==b->retry_ready &&
  a->drained==b->drained && a->terminal_rx_handoff==b->terminal_rx_handoff && a->epoch==b->epoch && a->start_us==b->start_us &&
  a->deadline_us==b->deadline_us && a->test_ip_override==b->test_ip_override;
}
static bool owned(struct http_network_probe *s){return s && s->self==(uintptr_t)s && s->source && same(&s->io,s->source);}
static bool current(struct http_network_probe *s){
 uint64_t f=lock();bool good=false;
 if(!owned(s) || s->checking || s->guard_failed){if(s){s->guard_failed=true;fail(s,HTTP_NET_OWNER);}goto done;}
 s->checking=true;good=s->io.current(s->io.context,s->io.epoch) && owned(s);
 uint64_t us=timer_monotonic_us(),tick=timer_ticks();
 if(!good){s->guard_failed=true;fail(s,HTTP_NET_OWNER);}
 else if(us<s->last_us || us<s->io.start_us || us>=s->io.deadline_us || tick<s->last_tick){s->guard_failed=true;good=fail(s,HTTP_NET_CLOCK);}
 if(good){s->last_us=us;s->last_tick=tick;}
 s->checking=false;
 done:unlock(f);return good;
}
static bool net_current(void *p,uint64_t epoch){struct http_network_probe *s=p;return s && epoch==s->io.epoch && current(s);}
static bool net_retry(void *p,uint64_t epoch){
 struct http_network_probe *s=p;uint64_t f=lock();
 bool good=s && epoch==s->io.epoch && current(s) && s->io.retry_ready(s->io.context,epoch) && current(s);
 unlock(f);return good;
}
static bool drained(struct http_network_probe *s){
 uint64_t f=lock();bool good=current(s) && s->io.drained(s->io.context,s->io.epoch) && current(s);
 unlock(f);return good;
}
/* A worker may publish TCP maintenance in the next turn. Only this atomic
 * check+stop transaction closes that gap after the child/context is retired. */
static bool stop_when_empty(struct http_network_probe *s){
 uint64_t f=lock();bool good=s->result.child_observed && current(s) &&
  net_probe_worker_current() && net_probe_worker_empty() && drained(s) && net_probe_worker_stop();
 if(good){s->worker_stopped=true;s->result.tcp_empty=s->result.provider_drained=true;}
 unlock(f);return good;
}
/* The atomic stop freezes net software publication, not the independent
 * controller receive owner. Consume a separate terminal ingress authority after
 * all process/worker borrowers are gone. A posted raw receive may still exist;
 * its exact ownership is the callback's obligation, never a fabricated drain. */
static bool terminal_ready(const struct http_network_probe *s){
 return s->worker_stopped && s->result.child_observed && s->result.child_reaped &&
  s->result.child_retired && s->result.worker_finished && s->result.tcp_empty &&
  s->result.provider_drained && !s->result.receiver_closed;
}
static bool terminal_rx_handoff(struct http_network_probe *s){
 uint64_t f=lock();bool good=false;
 if(!terminal_ready(s) || !current(s))goto done;
 const struct http_network_probe_io before=s->io;
 const struct http_network_probe_io *original=s->source;
 s->checking=true;
 bool closed=before.terminal_rx_handoff(before.context,before.epoch);
 s->checking=false;
 /* The native provider is legitimately CLOSING now: current()/drained() are
  * no longer admissible. Freeze identity across this consuming boundary. */
 if(s->source!=original || !owned(s) || !same(&before,&s->io) ||
    !same(&before,s->source) || s->guard_failed || !terminal_ready(s)){
  s->guard_failed=true;fail(s,HTTP_NET_OWNER);goto done;
 }
 uint64_t us=timer_monotonic_us(),tick=timer_ticks();
 if(us<s->last_us || us<before.start_us || us>=before.deadline_us || tick<s->last_tick){
  s->guard_failed=true;fail(s,HTTP_NET_CLOCK);goto done;
 }
 s->last_us=us;s->last_tick=tick;
 if(!closed){fail(s,HTTP_NET_RX_HANDOFF_FAILED);goto done;}
 s->result.receiver_closed=true;s->result.quiescent=true;good=true;
 done:unlock(f);return good;
}
static bool tick_due(void *p){return timer_ticks()>=*(uint64_t *)p;}
static void pause_one(struct http_network_probe *s){
 uint64_t tick=timer_ticks();if(tick==UINT64_MAX)return;
 tick++;if(!scheduler_wait_timeout(&s->wait,tick_due,&tick,tick))scheduler_yield();
}
static bool turn(struct http_network_probe *s){
 if(s->result.supervisor_turns>=SUPERVISOR_TURNS)return fail(s,HTTP_NET_CLOCK);
 s->result.supervisor_turns++;return current(s);
}
static bool child_cleanup(struct http_network_probe *s,bool cancel){
 if(cancel)(void)http_child_cancel(&s->child);
 uint64_t start=timer_monotonic_us(),last=start;if(start>UINT64_MAX-5000000)return false;
 for(unsigned i=0;i<1000;i++){
  uint64_t status=0;
  if(!s->child.reaped && http_child_observe(&s->child,&status)){
   s->result.child_observed=true;s->result.child_status=status;
   if(!http_child_reap(&s->child))return false;
   s->result.child_reaped=true;
  }
  if(http_child_retire(&s->child)){s->result.child_retired=true;return true;}
  uint64_t now=timer_monotonic_us();if(now<last || now-start>=5000000)return false;
  last=now;pause_one(s);
 }
 return false;
}
bool http_network_probe_prepare(struct http_network_probe *s){
 if(!s || !zero(s,sizeof *s) || process_current())return false;
 s->self=(uintptr_t)s;s->result.version=2;s->result.phase=HTTP_NET_PREPARE;
 bool child=http_child_prepare(&s->child,false),worker=false;
 if(child && timer_idle_wakeup_ready()){
  s->result.worker_allocated=net_prepare_probe_worker();
 }
 if(child && s->result.worker_allocated){
  uint64_t start=timer_monotonic_us(),last=start;
  if(start<=UINT64_MAX-5000000)for(unsigned i=0;i<1000;i++){
   if(net_probe_worker_prepared()){worker=true;break;}
   uint64_t now=timer_monotonic_us();if(now<last || now-start>=5000000)break;
   last=now;pause_one(s);
  }
 }
 if(child && worker){s->result.prepared=true;s->result.phase=HTTP_NET_READY;return true;}
 fail(s,HTTP_NET_PRELIVE);
 /* No bootstrap/worker handoff or GO has happened: no socket/worker callback
  * can own a provider. The permanently parked prepared thread may remain. */
 (void)http_network_probe_abort_prepare(s);
 return false;
}
bool http_network_probe_abort_prepare(struct http_network_probe *s){
 if(!s || s->self!=(uintptr_t)s || s->result.attempted || s->handoff_attempted ||
    s->source || s->result.go || s->child.published || s->bootstrap.identity || process_current())return false;
 if(s->result.quiescent && s->child.retired)return true;
 if(!s->result.reason)s->result.reason=HTTP_NET_NOT_RUN;
 if(s->result.worker_allocated){
  struct net_probe_worker_info w={0};
  if(!net_probe_worker_snapshot(&w) || w.ever_active || w.active || w.busy || w.poll_busy || w.checking ||
     (w.handoff_started && !s->worker_stopped))return false;
  if(!s->worker_stopped){
   if(!net_probe_worker_stop())return false;
   s->worker_stopped=true;
  }
  uint64_t start=timer_monotonic_us(),last=start;
  if(start>UINT64_MAX-5000000)return false;
  for(unsigned i=0;i<1000;i++){
   (void)net_probe_worker_finish();
   if(!net_probe_worker_snapshot(&w))return false;
   if(w.finished && w.parked && w.parked_waiters==1 && !w.active && !w.busy && !w.poll_busy && !w.checking){
    s->worker=w;s->result.worker_finished=true;break;
   }
   uint64_t now=timer_monotonic_us();if(now<last || now-start>=5000000)return false;
   last=now;pause_one(s);
  }
  if(!s->result.worker_finished)return false;
 }
 if(!child_cleanup(s,true))return false;
 s->result.quiescent=s->result.refs_scrubbed=true;s->answer=0;
 return true;
}
static bool bootstrap_drain(struct http_network_probe *s){
 for(unsigned i=0;i<SUPERVISOR_TURNS;i++){
  if(!turn(s))return false;
  if(drained(s))return true;
  if(!net_bounded_probe_poll(&s->bootstrap))return fail(s,HTTP_NET_DRAIN_FAILED);
 }
 return fail(s,HTTP_NET_DRAIN_FAILED);
}
static bool worker_snapshot(struct http_network_probe *s){
 if(!net_probe_worker_snapshot(&s->worker))return false;
 s->result.worker_polls=s->worker.polls;s->result.worker_turns=s->worker.turns;
 return true;
}
static bool finish_worker(struct http_network_probe *s){
 if(!s->worker_stopped && !net_probe_worker_stop())return false;
 s->worker_stopped=true;
 for(unsigned i=0;i<1000;i++){
  bool ok=net_probe_worker_finish();
  if(!worker_snapshot(s))return false;
  if(s->worker.finished && s->worker.parked && s->worker.parked_waiters==1 &&
     !s->worker.active && !s->worker.busy && !s->worker.poll_busy && !s->worker.checking){
   s->result.worker_finished=true;
   if(!ok || s->worker.failed)fail(s,HTTP_NET_WORKER_FAILED);
   return true;
  }
  if(!turn(s))return false;
  pause_one(s);
 }
 return false;
}
static bool finish_without_handoff(struct http_network_probe *s){
 /* Finish always scrubs this terminal bootstrap, including handled failures.
  * Its return value is success, not the separate reference-closure observation. */
 if(s->bootstrap.identity && !s->bootstrap.closed)(void)net_bounded_probe_finish(&s->bootstrap);
 s->bootstrap_open=s->bootstrap.identity && !s->bootstrap.closed;
 if(s->bootstrap_open || !worker_snapshot(s) || s->worker.handoff_started || s->worker.ever_active ||
    s->worker.active || s->worker.busy || s->worker.poll_busy || s->worker.checking ||
    !s->worker.parked || s->worker.parked_waiters!=1 || !drained(s))return false;
 s->result.provider_drained=true;s->result.tcp_empty=true;
 return finish_worker(s) && child_cleanup(s,true);
}
bool http_network_probe_run(struct http_network_probe *s,const struct http_network_probe_io *io){
 if(!s || s->self!=(uintptr_t)s || !s->result.prepared || s->result.attempted || s->child.retired || s->worker_stopped || s->result.quiescent || !io ||
    !io->context || !io->current || !io->retry_ready || !io->drained || !io->terminal_rx_handoff || !io->epoch ||
    io->start_us<s->child.ready_us || io->start_us>INT64_MAX-HTTP_CHILD_WINDOW_US ||
    io->deadline_us!=io->start_us+HTTP_CHILD_WINDOW_US ||
    (io->test_ip_override && (!(io->test_ip_override>>24) || (io->test_ip_override>>24)==127 || (io->test_ip_override>>24)>=224)) ||
    overlap(s,sizeof *s,io,sizeof *io) || overlap(s,sizeof *s,io->context,1))return false;
 s->result.attempted=true;s->io=*io;s->source=io;s->last_us=io->start_us;s->last_tick=timer_ticks();
 s->result.test_override=io->test_ip_override!=0;s->result.phase=HTTP_NET_DHCP;
 s->net_io=(struct net_bounded_probe_io){s,net_current,net_retry};
 bool good=false;
 if(!current(s) || !net_probe_worker_prepared() || !net_init_bounded_probe() ||
    !net_bounded_probe_init(&s->bootstrap,&s->net_io,io->epoch,io->deadline_us)){fail(s,HTTP_NET_INIT);goto done;}
 s->bootstrap_open=true;
 if(net_dhcp_request_bounded(&s->bootstrap)<0){fail(s,HTTP_NET_DHCP_FAILED);goto done;}s->result.dhcp=1;
 if(!bootstrap_drain(s))goto done;
 s->result.phase=HTTP_NET_DNS;
 if(io->test_ip_override)s->answer=io->test_ip_override;
 else {
  if(net_dns_resolve_bounded(&s->bootstrap,"httpbin.org",&s->answer)<0 || !s->answer){fail(s,HTTP_NET_DNS_FAILED);goto done;}
  s->result.dns=1;
 }
 if(!bootstrap_drain(s))goto done;
 s->result.phase=HTTP_NET_ROUTE;
 if(net_prepare_route_bounded(&s->bootstrap,s->answer)<0){fail(s,HTTP_NET_ROUTE_FAILED);goto done;}s->result.route=1;
 if(!bootstrap_drain(s))goto done;
 s->result.phase=HTTP_NET_HANDOFF;s->handoff_attempted=true;
 if(!net_probe_worker_handoff(&s->bootstrap)){fail(s,HTTP_NET_HANDOFF_FAILED);goto done;}
 s->bootstrap_open=false;s->result.handoff=true;
 s->result.phase=HTTP_NET_GO;
 if(!net_probe_worker_current() || !current(s)){fail(s,HTTP_NET_GO_FAILED);goto done;}
 if(!http_child_publish(&s->child,io->epoch,io->start_us,io->deadline_us,s->answer)){
  fail(s,HTTP_NET_GO_FAILED);(void)http_child_cancel(&s->child);
 } else s->result.go=1;
 s->result.phase=HTTP_NET_DRAIN;
 for(unsigned i=0;i<SUPERVISOR_TURNS;i++){
  if(!turn(s) || !net_probe_worker_current())goto done;
  uint64_t status=0;
  if(!s->result.child_observed && http_child_observe(&s->child,&status)){
   s->result.child_observed=true;s->result.child_status=status;
   if(status)fail(s,HTTP_NET_CHILD_FAILED);
  }
  if(stop_when_empty(s)){good=!s->result.reason;break;}
  pause_one(s);
 }
 if(!s->result.child_observed || !s->result.tcp_empty){fail(s,HTTP_NET_DRAIN_FAILED);goto done;}
 s->result.phase=HTTP_NET_FINISH;
 if(!finish_worker(s)){fail(s,HTTP_NET_WORKER_FAILED);goto done;}
 if(!http_child_reap(&s->child)){fail(s,HTTP_NET_RETIRE_FAILED);goto done;}
 s->result.child_reaped=true;
 if(!http_child_retire(&s->child)){fail(s,HTTP_NET_RETIRE_FAILED);goto done;}
 s->result.child_retired=true;
 /* Ordinary posted RX can complete after the stop snapshot. The finished
  * software borrowers hand its retained ownership to terminal native cleanup. */
 if(!terminal_rx_handoff(s))good=false;
 done:
 s->result.bootstrap_polls=s->bootstrap.polls;s->result.retries=s->bootstrap.dhcp_retries;
 (void)worker_snapshot(s);
 if(!s->result.quiescent){
  if(!s->handoff_attempted)s->result.quiescent=finish_without_handoff(s);
  else {
   /* Do not force-close/reap/free a child or TCP owner while a worker/provider
    * may still use it. Retain the entire static object and key on uncertainty. */
   (void)http_child_cancel(&s->child);
  }
 }
 uint64_t now=timer_monotonic_us();
 if(now<s->last_us || now<s->io.start_us || now>=s->io.deadline_us){good=false;fail(s,HTTP_NET_CLOCK);}
 else s->result.elapsed_us=now-s->io.start_us;
 bool snapshot_ok=worker_snapshot(s);
 if(s->result.handoff && (!snapshot_ok || !s->worker.finished || !s->worker.parked ||
    s->worker.parked_waiters!=1 || s->worker.active || s->worker.busy || s->worker.poll_busy ||
    s->worker.checking || s->worker.failed || s->worker.polls<s->result.bootstrap_polls ||
    s->worker.polls>=SUPERVISOR_TURNS || !s->worker.turns)){
  good=false;fail(s,HTTP_NET_WORKER_FAILED);
 }
 if(s->result.quiescent){
  scrub(&s->net_io,sizeof s->net_io);scrub(&s->io,sizeof s->io);s->source=NULL;s->answer=0;
  s->result.refs_scrubbed=true;
 }
 s->result.complete=good && s->result.quiescent && s->result.worker_finished &&
  s->result.child_observed && s->result.child_reaped && s->result.child_retired &&
  s->result.tcp_empty && s->result.provider_drained && s->result.receiver_closed && !s->result.child_status && !s->result.reason;
 if(s->result.complete)s->result.phase=HTTP_NET_DONE;
 return s->result.complete;
}
void http_network_probe_emit(const struct http_network_probe *s){
 if(!s)return;
 const struct http_network_result *r=&s->result;
 console_printf("wifi-http5-workload: version=%u complete=%u phase=%u reason=%u prepared=%u worker_allocated=%u attempted=%u dhcp=%u retries=%u dns=%u route=%u handoff=%u go=%u child_status=%u child_observed=%u child_reaped=%u child_retired=%u tcp_empty=%u provider_drained=%u receiver_closed=%u worker_finished=%u quiescent=%u refs_scrubbed=%u test_override=%u request_bytes=4159 body_bytes=3072 body_eof=%u bootstrap_polls=%u worker_polls=%u worker_turns=%u supervisor_turns=%u elapsed_us=%u retained=%u\n",
 r->version,(uint64_t)r->complete,r->phase,r->reason,(uint64_t)r->prepared,(uint64_t)r->worker_allocated,(uint64_t)r->attempted,
 r->dhcp,r->retries,r->dns,r->route,(uint64_t)r->handoff,r->go,r->child_status,
 (uint64_t)r->child_observed,(uint64_t)r->child_reaped,(uint64_t)r->child_retired,
 (uint64_t)r->tcp_empty,(uint64_t)r->provider_drained,(uint64_t)r->receiver_closed,(uint64_t)r->worker_finished,
 (uint64_t)r->quiescent,(uint64_t)r->refs_scrubbed,(uint64_t)r->test_override,(uint64_t)(r->go && r->child_observed && !r->child_status),
 r->bootstrap_polls,r->worker_polls,r->worker_turns,r->supervisor_turns,r->elapsed_us,(uint64_t)!r->quiescent);
}
