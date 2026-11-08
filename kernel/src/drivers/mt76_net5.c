#include <srvros/mt76_net5.h>
#include <srvros/net.h>
#include <srvros/timer.h>
static uint64_t lock(void){
#ifdef SRVROS_MT76_NET5_HOST_TEST
 return 0;
#else
 uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f){
#ifdef SRVROS_MT76_NET5_HOST_TEST
 (void)f;
#else
 if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static void copy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;while(n--)*a++=*b++;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned a=0;while(n--)a|=*v++;return !a;}
static bool equal(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;unsigned d=0;while(n--)d|=*x++^*y++;return !d;}
static bool overlaps(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void inc(uint64_t *v){if(*v!=UINT64_MAX)(*v)++;}
static bool send(void *,const uint8_t *,uint16_t);
static uint64_t poll(void *,uint64_t,bool);
static bool link(void *,struct net_device_link *);
static bool valid(const struct mt76_net5 *s){
 return s && s->identity==(uintptr_t)s && s->state>=MT76_NET5_DOWN && s->state<=MT76_NET5_RETIRED &&
 s->head<MT76_NET5_QUEUE && s->count<=MT76_NET5_QUEUE && s->loan_size<=NET_DEVICE_FRAME_MAX &&
 (s->state!=MT76_NET5_UP || s->attached) && (!s->pending || (s->attached && s->pending_token && s->pending_token<=s->next_token)) &&
 (!s->attached || (s->io.context && s->io.owned && s->io.submit && s->io.service && s->io.receive && s->io.retired)) &&
 s->ops.context==s && s->ops.send==send && s->ops.poll==poll && s->ops.link==link;
}
static bool protocol(const uint8_t *p){return p[12]==8 && (p[13]==0 || p[13]==6);}
static void idle_scrub(struct mt76_net5 *s){
 if(!s->io_busy){scrub(s->loan,sizeof s->loan);s->loan_size=0;}
 if(!s->polling && !s->delivering && !s->io_busy)scrub(s->rx,sizeof s->rx);
}
static void purge(struct mt76_net5 *s){
 for(unsigned i=0;i<MT76_NET5_QUEUE && i<s->count;i++)inc(&s->dropped);
 scrub(s->queue,sizeof s->queue);scrub(s->length,sizeof s->length);s->head=s->count=0;idle_scrub(s);
}
static void fault(struct mt76_net5 *s){
 if(s->state==MT76_NET5_RETIRED)return;
 if(s->state!=MT76_NET5_FAULT)inc(&s->faults);
 s->state=MT76_NET5_FAULT;purge(s);net_notify_rx();
}
static bool clock_ok(struct mt76_net5 *s){
 uint64_t now=timer_monotonic_us();
 if(now<s->last_us || (s->pending && (now<s->tx_started || now-s->tx_started>=MT76_NET5_TX_US))){fault(s);return false;}
 s->last_us=now;return true;
}
/* io_busy pins callback context and forbids recursive transport callbacks.
 * Disconnect may change state during a callback;never call it again afterward. */
static bool owned(struct mt76_net5 *s){
 uint64_t epoch=s->epoch;bool ok=s->io.owned(s->io.context,epoch);
 if(s->state!=MT76_NET5_UP)return false;
 if(s->epoch!=epoch){fault(s);return false;}
 if(!ok){fault(s);return false;}return true;
}
bool mt76_net5_init(struct mt76_net5 *s,const uint8_t station[6]){
 if(!s || !station || overlaps(s,sizeof *s,station,6) || !zero(s,sizeof *s) || (station[0]&1) || zero(station,6))return false;
 uint64_t f=lock();
 struct net_device_ops ops={.name="mt76-ccmp",.context=s,.priority=40,.send=send,.poll=poll,.link=link};copy(ops.mac,station,6);
 bool ok=net_device_register(&ops);
 if(ok){s->ops=ops;s->identity=(uintptr_t)s;s->state=MT76_NET5_DOWN;}
 unlock(f);return ok;
}
bool mt76_net5_attach(struct mt76_net5 *s,uint64_t epoch,const struct mt76_net5_io *io){
 if(!valid(s) || !io || !io->context || !io->owned || !io->submit || !io->service || !io->receive || !io->retired ||
 overlaps(s,sizeof *s,io,sizeof *io) || overlaps(s,sizeof *s,io->context,1))return false;
 uint64_t f=lock();bool ok=false;
 if(s->state==MT76_NET5_DOWN && !s->retire_requested && !s->callbacks && !s->io_busy && !s->polling && !s->attached && epoch>s->epoch){
  struct mt76_net5_io candidate=*io;s->callbacks++;s->io_busy=true;
  bool admitted=candidate.owned(candidate.context,epoch);
  s->io_busy=false;s->callbacks--;
  if(admitted && s->state==MT76_NET5_DOWN && !s->retire_requested){
   s->io=candidate;s->epoch=epoch;s->attached=true;s->state=MT76_NET5_UP;s->last_us=timer_monotonic_us();ok=true;net_notify_rx();
  }
 }
 unlock(f);return ok;
}
static bool send(void *context,const uint8_t *frame,uint16_t n){
 struct mt76_net5 *s=context;if(!valid(s) || !frame || n<14 || n>NET_DEVICE_FRAME_MAX || overlaps(s,sizeof *s,frame,n))return false;
 uint64_t f=lock();bool ok=false;
 if(s->state==MT76_NET5_UP && !s->io_busy && s->count<MT76_NET5_QUEUE && protocol(frame) && equal(frame+6,s->ops.mac,6)){
  s->callbacks++;s->io_busy=true;
  if(owned(s)){
   unsigned tail=(s->head+s->count)%MT76_NET5_QUEUE;copy(s->queue[tail],frame,n);s->length[tail]=n;s->count++;inc(&s->enqueued);ok=true;
  }
  s->io_busy=false;s->callbacks--;idle_scrub(s);
 }
 unlock(f);return ok;
}
bool mt76_net5_service(struct mt76_net5 *s){
 if(!valid(s))return false;
 uint64_t f=lock();bool ok=false;
 if(s->state==MT76_NET5_UP && !s->io_busy){
  s->io_busy=true;s->callbacks++;
  if(clock_ok(s) && owned(s)){
   uint64_t epoch=s->epoch;bool serviced=s->io.service(s->io.context,epoch);
   if(s->state==MT76_NET5_UP && (!serviced || s->epoch!=epoch))fault(s);
   if(s->state==MT76_NET5_UP && clock_ok(s) && owned(s)){
    if(!s->pending && s->count){
     unsigned i=s->head;uint16_t n=s->length[i];
     if(n<14 || n>NET_DEVICE_FRAME_MAX || !protocol(s->queue[i]) || !equal(s->queue[i]+6,s->ops.mac,6) || s->next_token==UINT64_MAX)fault(s);
     else{
      copy(s->loan,s->queue[i],n);s->loan_size=n;scrub(s->queue[i],sizeof s->queue[i]);s->length[i]=0;s->head=(i+1)%MT76_NET5_QUEUE;s->count--;
      s->pending=true;s->pending_token=++s->next_token;s->tx_started=s->last_us;uint64_t token=s->pending_token;
      enum mt76_net5_submit result=s->io.submit(s->io.context,epoch,token,s->loan,n);
      if(result==MT76_NET5_QUEUED)inc(&s->submitted);
      else if(result==MT76_NET5_REFUSED && s->pending && s->pending_token==token){s->pending=false;s->pending_token=0;inc(&s->dropped);}
      else fault(s); /* Unknown submission retains pending identity. */
      if(s->state==MT76_NET5_UP){
       if(s->epoch!=epoch)fault(s);
       else if(!clock_ok(s) || !owned(s))ok=false;
      }
     }
    }
    ok=s->state==MT76_NET5_UP;
   }
  }
  s->io_busy=false;s->callbacks--;idle_scrub(s);
 }
 unlock(f);return ok;
}
bool mt76_net5_tx_complete(struct mt76_net5 *s,uint64_t epoch,uint64_t token,bool success){
 if(!valid(s))return false;
 uint64_t f=lock();bool ok=false;
 if(s->attached && epoch==s->epoch){
  if(!s->pending || !token || token!=s->pending_token)fault(s);
  else{
   bool timely=clock_ok(s);s->pending=false;s->pending_token=0;
   if(success && timely){inc(&s->completed);ok=true;}else fault(s);
  }
 }
 unlock(f);return ok;
}
static uint64_t poll(void *context,uint64_t budget,bool verbose){
 (void)verbose;struct mt76_net5 *s=context;if(!valid(s) || !budget)return 0;
 if(budget>32)budget=32;
 uint64_t f=lock(),delivered=0;
 if(s->state==MT76_NET5_UP && !s->polling && !s->io_busy){
  s->polling=true;s->callbacks++;(void)mt76_net5_service(s);
  for(uint64_t i=0;i<budget && s->state==MT76_NET5_UP;i++){
   size_t n=0;bool present=false;s->io_busy=true;
   bool ok=clock_ok(s) && owned(s);
   if(ok){uint64_t epoch=s->epoch;ok=s->io.receive(s->io.context,epoch,s->rx,sizeof s->rx,&n,&present);if(s->epoch!=epoch)ok=false;}
   if(s->state==MT76_NET5_UP && (!ok || n>sizeof s->rx || (!present && n) || (present && n<14)))fault(s);
   if(s->state==MT76_NET5_UP)ok=clock_ok(s) && owned(s);else ok=false;
   s->io_busy=false;
   if(ok && present && protocol(s->rx)){
    s->delivering=true;net_handle_ethernet_frame(s->rx,(uint16_t)n);s->delivering=false;inc(&s->received);delivered++;
   }
   scrub(s->rx,sizeof s->rx);
   if(!ok || !present)break;
  }
  if(s->state==MT76_NET5_UP)(void)mt76_net5_service(s);
  s->polling=false;s->callbacks--;idle_scrub(s);
 }
 unlock(f);return delivered;
}
static bool link(void *context,struct net_device_link *out){
 struct mt76_net5 *s=context;if(!valid(s) || !out || overlaps(s,sizeof *s,out,sizeof *out))return false;
 uint64_t f=lock();*out=(struct net_device_link){.up=s->state==MT76_NET5_UP};unlock(f);return true;
}
bool mt76_net5_disconnect(struct mt76_net5 *s,uint64_t epoch){
 if(!valid(s))return false;
 uint64_t f=lock();bool ok=s->attached && epoch==s->epoch;
 if(ok){if(s->state==MT76_NET5_UP)s->state=MT76_NET5_CLOSING;purge(s);net_notify_rx();}
 unlock(f);return ok;
}
bool mt76_net5_finish_detach(struct mt76_net5 *s,uint64_t epoch){
 if(!valid(s))return false;
 uint64_t f=lock();bool ok=false;
 if(s->attached && epoch==s->epoch && (s->state==MT76_NET5_CLOSING || s->state==MT76_NET5_FAULT) && !s->callbacks && !s->io_busy && !s->polling && !s->delivering){
  enum mt76_net5_state state=s->state;s->io_busy=true;s->callbacks++;
  bool retired=s->io.retired(s->io.context,epoch);
  s->callbacks--;s->io_busy=false;
  if(retired && s->epoch==epoch && s->state==state){
   purge(s);scrub(&s->io,sizeof s->io);s->attached=s->pending=false;s->pending_token=0;s->state=MT76_NET5_DOWN;ok=true;
  }
 }
 unlock(f);return ok;
}
bool mt76_net5_retire(struct mt76_net5 *s){
 if(!valid(s))return false;
 uint64_t f=lock();bool ok=false;
 if(s->state==MT76_NET5_RETIRED)ok=true;
 else{
  s->retire_requested=true;
  if(s->attached)(void)mt76_net5_disconnect(s,s->epoch);
  s->registry_drained=net_device_retire(&s->ops);
  if(s->registry_drained && !s->callbacks && !s->io_busy && !s->polling && !s->delivering){
   if(!s->attached || mt76_net5_finish_detach(s,s->epoch)){s->state=MT76_NET5_RETIRED;ok=true;}
  }
 }
 unlock(f);return ok;
}
