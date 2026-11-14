#include <srvros/mt76_stack5_probe.h>
#include <srvros/net.h>
#include <srvros/timer.h>
#include <srvros/console.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned a=0;while(n--)a|=*v++;return !a;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
struct run {
 bool connection;unsigned positions;
 struct net_bounded_probe net;struct net_bounded_probe_io net_io;
 struct mt76_stack5_probe_io io;const struct mt76_stack5_probe_io *source;
 struct mt76_net5 *provider;struct mt76_stack5_probe *result;
 uint64_t start,last,enqueued,submitted,completed,received;
 struct mt76_stack5_observation observation;
 struct srv_net_status_info status;uint32_t answer,gateway;
};
static bool same_io(const struct mt76_stack5_probe_io *a,const struct mt76_stack5_probe_io *b){return a->context==b->context && a->current==b->current && a->retry_ready==b->retry_ready && a->observe==b->observe && a->epoch==b->epoch && a->deadline_us==b->deadline_us;}
static bool fail(struct run *s,unsigned why){if(!s->result->reason)s->result->reason=why;return false;}
static bool current(struct run *s){
 if(!same_io(&s->io,s->source) || !net_bounded_probe_current(&s->net) || !same_io(&s->io,s->source))return fail(s,MT76_STACK5_OWNER);
 uint64_t now=timer_monotonic_us();if(now<s->last || now<s->start || now>=s->io.deadline_us)return fail(s,MT76_STACK5_CLOCK);s->last=now;return true;
}
static bool observe(struct run *s){
 if(!current(s))return false;
 struct mt76_stack5_observation o={0};bool good=s->io.observe(s->io.context,s->io.epoch,&o);
 if(!current(s))return false;
 struct mt76_net5 *p=s->provider;
 if(p->enqueued<s->enqueued || p->submitted<s->submitted || p->completed<s->completed || p->received<s->received)return fail(s,MT76_STACK5_ACCOUNTING);
 if(!good || o.tx_posts!=4+o.data_tx || o.data_tx!=p->submitted-s->submitted || o.tx_posts>=MT76_STACK5_TX_LIMIT || o.rx_posts>MT76_STACK5_RX_LIMIT-8 || o.tx_cursor>s->positions || o.rx_cursor>s->positions || o.fences>MT76_STACK5_FENCE_LIMIT || o.fences!=o.tx_wraps+o.rx_wraps || o.services>=MT76_STACK5_POLLS || p->epoch!=s->io.epoch || p->state!=MT76_NET5_UP || p->faults || p->dropped || p->enqueued<p->submitted || p->submitted<p->completed)return fail(s,MT76_STACK5_OBSERVATION);
 if(s->connection && (o.tx_wraps || o.rx_wraps>1 || o.tx_after || o.tx_cursor!=o.tx_posts || o.rx_posts<127u*o.rx_wraps || o.rx_cursor!=o.rx_posts-127u*o.rx_wraps))return fail(s,MT76_STACK5_OBSERVATION);
 s->observation=o;return true;
}
static bool drain(struct run *s){
 for(unsigned i=0;i<MT76_STACK5_POLLS;i++){
  if(!observe(s))return false;
  if(s->observation.drained){struct mt76_net5 *p=s->provider;if(p->count || p->pending || p->submitted!=p->completed)return fail(s,MT76_STACK5_ACCOUNTING);return true;}
  if(!net_bounded_probe_poll(&s->net))return fail(s,MT76_STACK5_DRAIN_FAILED);
 }
 return fail(s,MT76_STACK5_DRAIN_FAILED);
}
static bool run_probe(struct mt76_stack5_probe *r,struct mt76_net5 *p,const struct mt76_stack5_probe_io *io,bool connection){
 if(!r || !p || !io || !io->context || !io->current || !io->retry_ready || !io->observe || !io->epoch || !io->deadline_us || !zero(r,sizeof *r) || overlap(r,sizeof *r,p,sizeof *p) || overlap(r,sizeof *r,io,sizeof *io) || overlap(r,sizeof *r,io->context,1))return false;
 r->attempted=true;r->connection_scope=connection;r->version=1;r->requested=MT76_STACK5_PINGS;r->phase=MT76_STACK5_INIT;
 struct run s={.connection=connection,.positions=connection?127u:63u,.io=*io,.source=io,.provider=p,.result=r};s.start=s.last=timer_monotonic_us();
 s.net_io=(struct net_bounded_probe_io){io->context,io->current,io->retry_ready};
 bool initialized=false,good=false;
 if(!same_io(io,&s.io) || !io->current(io->context,io->epoch) || !same_io(io,&s.io)){fail(&s,MT76_STACK5_INITIALIZATION);goto done;}
 if(connection) initialized=net_bounded_probe_init_connection(&s.net,&s.net_io,&p->ops,io->epoch,io->deadline_us);
 else initialized=net_init_bounded_probe() && net_device_selected_is(&p->ops) && net_bounded_probe_init(&s.net,&s.net_io,io->epoch,io->deadline_us);
 s.enqueued=connection?p->enqueued:0;s.submitted=connection?p->submitted:0;
 s.completed=connection?p->completed:0;s.received=connection?p->received:0;
 if(!initialized){fail(&s,MT76_STACK5_INITIALIZATION);goto done;}
 if(!observe(&s))goto done;
 if(p->enqueued!=s.enqueued || p->submitted!=s.submitted || p->completed!=s.completed || p->enqueued!=p->submitted || p->submitted!=p->completed || s.observation.data_tx || s.observation.icmp_count){fail(&s,MT76_STACK5_ACCOUNTING);goto done;}
 r->phase=MT76_STACK5_DHCP;
 if(net_dhcp_request_bounded(&s.net)<0){fail(&s,MT76_STACK5_DHCP_FAILED);goto done;}r->dhcp=1;
 if(!drain(&s))goto done;
 r->phase=MT76_STACK5_DNS;
 if(net_dns_resolve_bounded(&s.net,"example.com",&s.answer)<0 || !s.answer){fail(&s,MT76_STACK5_DNS_FAILED);goto done;}r->dns=1;
 if(!drain(&s) || !current(&s))goto done;
 if(net_status_info(&s.status)<0 || !current(&s) || !s.status.dhcp_configured || !s.status.router_ip){fail(&s,MT76_STACK5_ACCOUNTING);goto done;}
 s.gateway=s.status.router_ip;r->phase=MT76_STACK5_PING;
 if(!observe(&s))goto done;
 r->setup_tx=(unsigned)s.observation.data_tx;
 if(r->setup_tx>11 || MT76_STACK5_TX_LIMIT-1-s.observation.tx_posts<MT76_STACK5_PINGS || MT76_STACK5_RX_LIMIT-8-s.observation.rx_posts<MT76_STACK5_PINGS){fail(&s,MT76_STACK5_CREDIT);goto done;}
 for(unsigned i=0;i<MT76_STACK5_PINGS;i++){
  if(!observe(&s))goto done;
  if(MT76_STACK5_TX_LIMIT-1-s.observation.tx_posts<MT76_STACK5_PINGS-i){fail(&s,MT76_STACK5_CREDIT);goto done;}
  uint64_t before=s.observation.icmp_count,elapsed=0;r->started++;
  if(net_ping_bounded(&s.net,s.gateway,(uint16_t)(0x4000+i),&elapsed)<0){fail(&s,MT76_STACK5_PING_FAILED);goto done;}
  if(!drain(&s))goto done;
  if(s.observation.icmp_count!=before+1 || s.observation.icmp_sequence!=0x4000+i || !s.observation.icmp_pn || s.observation.icmp_pn>=(1ull<<48) || (i && s.observation.icmp_pn<=r->ping_pn[i-1]) || s.net.pings_sent!=i+1 || s.net.pings_matched!=i+1){fail(&s,MT76_STACK5_ACCOUNTING);goto done;}
  r->ping_pn[i]=s.observation.icmp_pn;r->matched++;r->exact++;r->body_bytes+=MT76_STACK5_BODY;
 }
 if(!drain(&s))goto done;
 if((!connection && (!s.observation.tx_wraps || !s.observation.rx_wraps || !s.observation.tx_after || !s.observation.rx_after)) || s.observation.data_tx<r->setup_tx+MT76_STACK5_PINGS || p->enqueued!=p->submitted || p->submitted!=p->completed){fail(&s,MT76_STACK5_ACCOUNTING);goto done;}
 good=true;
 done:
 r->polls=s.net.polls;r->retries=(unsigned)s.net.dhcp_retries;r->arp=(unsigned)s.net.arp_resolutions;r->services=s.observation.services;r->data_tx=(unsigned)s.observation.data_tx;
 if(s.net.identity){
  bool closed=connection && good?net_bounded_probe_disconnect(&s.net):net_bounded_probe_finish(&s.net);
  r->disconnected=s.net.disconnected;
  if(!closed){good=false;fail(&s,MT76_STACK5_OWNER);}
 }
 uint64_t now=timer_monotonic_us();if(now<s.start || now<s.last || now>=s.io.deadline_us){good=false;fail(&s,MT76_STACK5_CLOCK);}else r->elapsed_us=now-s.start;
 if(good && !r->reason){r->complete=true;r->phase=MT76_STACK5_DONE;}
 r->dhcp_observation=s.net.dhcp_observation;
 scrub(&s,sizeof s);r->private_erased=zero(&s,sizeof s);return r->complete && r->private_erased;
}
bool mt76_stack5_probe_run(struct mt76_stack5_probe *r,struct mt76_net5 *p,const struct mt76_stack5_probe_io *io){
 return run_probe(r,p,io,false);
}
bool mt76_stack5_probe_run_connection(struct mt76_stack5_probe *r,struct mt76_net5 *p,const struct mt76_stack5_probe_io *io){
 return run_probe(r,p,io,true);
}
void mt76_stack5_probe_emit(const struct mt76_stack5_probe *r){
 if(!r)return;
 if(r->matched){console_write("wifi-stack5-pings:");for(unsigned i=0;i<r->matched && i<MT76_STACK5_PINGS;i++)console_printf(" %u",r->ping_pn[i]);console_write("\n");}
 console_printf("wifi-stack5-workload: version=%u complete=%u phase=%u reason=%u dhcp=%u retries=%u dns=%u requested=%u started=%u matched=%u exact=%u body_bytes=%u icmp_bytes=40 payload_bytes=32 setup_tx=%u data_tx=%u arp=%u polls=%u services=%u elapsed_us=%u private_erased=%u\n",(uint64_t)r->version,(uint64_t)r->complete,(uint64_t)r->phase,(uint64_t)r->reason,(uint64_t)r->dhcp,(uint64_t)r->retries,(uint64_t)r->dns,(uint64_t)r->requested,(uint64_t)r->started,(uint64_t)r->matched,(uint64_t)r->exact,(uint64_t)r->body_bytes,(uint64_t)r->setup_tx,(uint64_t)r->data_tx,(uint64_t)r->arp,r->polls,r->services,r->elapsed_us,(uint64_t)r->private_erased);
 const struct net_dhcp_observation *d=&r->dhcp_observation;
 console_printf("wifi-stack5-dhcp: version=%u started=%u finished=%u success=%u end_reason=%u discovers=%u discover_queued=%u requests=%u request_queued=%u frames=%u ipv4=%u udp=%u dhcp=%u offers=%u acks=%u naks=%u accepted_offer=%u accepted_ack=%u rejected=%u first_reject=%u last_reject=%u reject_mask=%u last_message=%u last_options=%u have_offer=%u request_sent=%u configured=%u polls=%u elapsed_us=%u elapsed_ticks=%u\n",
 d->version,d->started,d->finished,d->success,d->end_reason,d->discovers,d->discover_queued,d->requests,d->request_queued,
 d->frames,d->ipv4,d->udp,d->dhcp,d->offers,d->acks,d->naks,d->accepted_offer,d->accepted_ack,d->rejected,
 d->first_reject,d->last_reject,d->reject_mask,d->last_message,d->last_options,d->have_offer,d->request_sent,d->configured,d->polls,d->elapsed_us,d->elapsed_ticks);
}
