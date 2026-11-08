#include <srvros/mt76_fence5.h>
static void copy(void *a,const void *b,size_t n){uint8_t *x=a;const uint8_t *y=b;while(n--)*x++=*y++;}
static bool equal(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;unsigned v=0;while(n--)v|=*x++^*y++;return !v;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned x=0;while(n--)x|=*v++;return !x;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool io_equal(const struct mt76_fence5_io *a,const struct mt76_fence5_io *b){return a->context==b->context && a->owned==b->owned && a->submit==b->submit && a->completion==b->completion && a->endpoint==b->endpoint && a->frontier==b->frontier;}
static uint64_t signature(const struct mt76_fence5 *s){const uint8_t *v=(const uint8_t *)s;uint64_t h=1469598103934665603ull;for(size_t i=0;i<sizeof *s;i++)if(i<offsetof(struct mt76_fence5,seal) || i>=offsetof(struct mt76_fence5,seal)+sizeof s->seal)h=(h^v[i])*1099511628211ull;return h;}
static void seal(struct mt76_fence5 *s){s->seal=signature(s);}
static bool valid(struct mt76_fence5 *s){return s && s->identity==(uintptr_t)s && s->seal==signature(s) && s->dma && s->dma==(struct mt76_dma5 *)s->guard.identity && s->epoch && s->state>=MT76_FENCE5_IDLE && s->state<=MT76_FENCE5_FAULT && io_equal(&s->io,&s->io_guard);}
static bool pin(struct mt76_fence5 *s,struct mt76_dma5 *d){s->state=MT76_FENCE5_FAULT;s->busy=false;mt76_dma5_fault(d);seal(s);return false;}
static bool clean(struct mt76_fence5 *s,uint64_t stamp,struct mt76_dma5 *d){return signature(s)==stamp && s->dma==d && io_equal(&s->io,&s->io_guard) && equal(d,&s->guard,sizeof s->guard);}
static bool layout(struct mt76_fence5 *s){struct mt76_dma5 *d=s->dma;for(unsigned dir=0;dir<2;dir++)for(unsigned i=0;i<4;i++)if(d->pages[dir][i].lease.page!=s->guard.pages[dir][i].lease.page)return false;return d->identity==(uintptr_t)d && d->epoch==s->epoch && d->state==MT76_DMA5_RUNNING && d->positions==63 && d->ring[0]==s->guard.ring[0] && d->ring[1]==s->guard.ring[1];}
static void phase(struct mt76_fence5 *s,enum mt76_fence5_state state,uint64_t now){s->state=state;s->phase_us=now;}
static bool command_equal(const struct mt76_fence5_command *a,const struct mt76_fence5_command *b){return a->epoch==b->epoch && a->token==b->token && a->pointer==b->pointer && a->dequeue==b->dequeue && a->slot==b->slot && a->dci==b->dci && a->type==b->type;}
bool mt76_fence5_init(struct mt76_fence5 *s,struct mt76_dma5 *d,uint64_t epoch,unsigned slot,const unsigned dci[2],const struct mt76_fence5_io *io,uint64_t now){
 if(!s || !d || !dci || !io || !zero(s,sizeof *s) || overlap(s,sizeof *s,d,sizeof *d) || overlap(s,sizeof *s,io,sizeof *io) || overlap(s,sizeof *s,dci,2*sizeof *dci) || !epoch || !slot || slot>255 || !dci[0] || dci[0]>31 || !dci[1] || dci[1]>31 || dci[0]==dci[1] || !io->context || overlap(s,sizeof *s,io->context,1) || overlap(d,sizeof *d,io->context,1) || !io->owned || !io->submit || !io->completion || !io->endpoint || !io->frontier || d->identity!=(uintptr_t)d || d->epoch!=epoch || d->state!=MT76_DMA5_RUNNING || d->positions!=63 || now<d->last_us)return false;
 s->identity=(uintptr_t)s;s->dma=d;s->epoch=epoch;s->slot=slot;s->dci[0]=dci[0];s->dci[1]=dci[1];s->io=*io;s->io_guard=*io;copy(&s->guard,d,sizeof *d);s->last_us=now;s->state=MT76_FENCE5_IDLE;s->sequence=d->last_fence;seal(s);return true;
}
bool mt76_fence5_begin(struct mt76_fence5 *s,unsigned direction,uint64_t now){
 if(!valid(s) || s->busy || s->state!=MT76_FENCE5_IDLE || direction>1)return false;
 if(now<s->last_us || now<s->dma->last_us || !layout(s))return pin(s,s->dma);
 struct mt76_dma5 *d=s->dma;
 if(d->cursor[direction]!=63)return false;
 for(unsigned dir=0;dir<2;dir++)for(unsigned i=0;i<4;i++)if(d->pages[dir][i].state!=MT76_DMA5_FREE)return false;
 if(s->pointer_count>61 || s->sequence==UINT64_MAX || s->next_token>UINT64_MAX-2 || d->last_fence!=s->sequence)return pin(s,s->dma);
 copy(&s->guard,d,sizeof *d);s->direction=direction;s->last_us=now;s->scanned=0;phase(s,MT76_FENCE5_STOP,now);seal(s);return true;
}
static bool submit(struct mt76_fence5 *s,unsigned type,uint64_t now){
 struct mt76_fence5_command wanted={.epoch=s->epoch,.token=++s->next_token,.slot=s->slot,.dci=s->dci[s->direction],.type=type,.dequeue=type==16?(s->dma->ring[s->direction]|(s->dma->cycle[s->direction]^1u)):0},actual={0};
 struct mt76_dma5 *d=s->dma;uint64_t stamp=signature(s);enum mt76_fence5_result result=s->io.submit(s->io.context,&wanted,&actual);
 if(!clean(s,stamp,d) || result!=MT76_FENCE5_READY || !actual.pointer || (actual.pointer&15))return false;
 wanted.pointer=actual.pointer;if(!command_equal(&wanted,&actual))return false;
 uint64_t base=actual.pointer&~4095ull;
 if(!base || actual.pointer-base>=63*16 || (s->command_ring && s->command_ring!=base))return false;
 for(unsigned dir=0;dir<2;dir++){if(base==d->ring[dir])return false;for(unsigned i=0;i<4;i++)if(base==d->pages[dir][i].lease.page)return false;}
 if(!s->command_ring)s->command_ring=base;
 for(unsigned i=0;i<s->pointer_count;i++)if(s->command_pointers[i]==actual.pointer)return false;
 if(s->pointer_count>=63)return false;
 s->command_pointers[s->pointer_count++]=actual.pointer;
 s->command=actual;phase(s,type==15?MT76_FENCE5_STOP_WAIT:MT76_FENCE5_REBASE_WAIT,now);return true;
}
static bool wait(struct mt76_fence5 *s,uint64_t now){
 struct mt76_fence5_completion e={0};struct mt76_dma5 *d=s->dma;uint64_t stamp=signature(s);enum mt76_fence5_result r=s->io.completion(s->io.context,&e);
 if(!clean(s,stamp,d))return false;
 if(r==MT76_FENCE5_PENDING)return true;
 if(r!=MT76_FENCE5_READY || e.cc!=1 || !command_equal(&e.command,&s->command))return false;
 phase(s,s->state==MT76_FENCE5_STOP_WAIT?MT76_FENCE5_STOP_CONTEXT:MT76_FENCE5_VERIFY,now);return true;
}
static bool endpoint(struct mt76_fence5 *s,uint64_t now){
 struct mt76_fence5_context e={0};struct mt76_dma5 *d=s->dma;uint64_t ring=d->ring[s->direction],stamp=signature(s);bool result=s->io.endpoint(s->io.context,s->slot,s->dci[s->direction],&e);
 if(!clean(s,stamp,d) || !result || e.epoch!=s->epoch || e.slot!=s->slot || e.dci!=s->dci[s->direction] || e.state!=3 || (e.dequeue&14) || (e.dequeue&~15ull)<ring || (e.dequeue&~15ull)>ring+16*63 || (s->state==MT76_FENCE5_VERIFY && e.dequeue!=s->command.dequeue))return false;
 s->scanned=0;phase(s,s->state==MT76_FENCE5_STOP_CONTEXT?MT76_FENCE5_DRAIN:MT76_FENCE5_FINAL_DRAIN,now);return true;
}
static bool drain(struct mt76_fence5 *s,uint64_t now){
 struct mt76_fence5_frontier e={0};struct mt76_dma5 *d=s->dma;uint64_t stamp=signature(s);bool result=s->io.frontier(s->io.context,&e);
 if(!clean(s,stamp,d) || !result || e.epoch!=s->epoch || !e.sequence || e.sequence<=s->last_frontier || e.target_events || e.command_pending || e.scanned>MT76_FENCE5_EVENTS-s->scanned)return false;
 s->last_frontier=e.sequence;s->scanned+=e.scanned;
 if(e.empty)phase(s,s->state==MT76_FENCE5_DRAIN?MT76_FENCE5_REBASE:MT76_FENCE5_COMMIT,now);
 else if(s->scanned==MT76_FENCE5_EVENTS)return false;
 return true;
}
bool mt76_fence5_step(struct mt76_fence5 *s,uint64_t now){
 if(!valid(s) || s->busy || s->state==MT76_FENCE5_IDLE || s->state==MT76_FENCE5_FAULT)return false;
 if(now<s->last_us || now<s->phase_us || now-s->phase_us>=MT76_FENCE5_WAIT_US || !layout(s) || !equal(s->dma,&s->guard,sizeof s->guard))return pin(s,s->dma);
 struct mt76_dma5 *original=s->dma;s->last_us=now;s->busy=true;uint64_t stamp=signature(s);bool good=s->io.owned(s->io.context,s->epoch,s->slot);
 if(!clean(s,stamp,original))good=false;
 if(good && (!io_equal(&s->io,&s->io_guard) || !equal(s->dma,&s->guard,sizeof s->guard) || s->state==MT76_FENCE5_FAULT))good=false;
 if(good)switch(s->state){
 case MT76_FENCE5_STOP:good=submit(s,15,now);break;
 case MT76_FENCE5_STOP_WAIT:case MT76_FENCE5_REBASE_WAIT:good=wait(s,now);break;
 case MT76_FENCE5_STOP_CONTEXT:case MT76_FENCE5_VERIFY:good=endpoint(s,now);break;
 case MT76_FENCE5_DRAIN:case MT76_FENCE5_FINAL_DRAIN:good=drain(s,now);break;
 case MT76_FENCE5_REBASE:good=submit(s,16,now);break;
 case MT76_FENCE5_COMMIT:{struct mt76_dma5_fence f={.epoch=s->epoch,.sequence=s->sequence+1,.dequeue=s->command.dequeue,.direction=s->direction,.stopped=true,.events_drained=true,.dequeue_set=true};good=mt76_dma5_wrap(s->dma,&f);if(good){s->dma->last_us=now;s->sequence++;s->completed++;copy(&s->guard,s->dma,sizeof s->guard);phase(s,MT76_FENCE5_IDLE,now);}break;}
 default:good=false;break;
 }
 s->busy=false;
 if(!good || !io_equal(&s->io,&s->io_guard) || s->dma!=original || !equal(original,&s->guard,sizeof s->guard) || s->state==MT76_FENCE5_FAULT)return pin(s,original);
 seal(s);return true;
}
bool mt76_fence5_cancel(struct mt76_fence5 *s,uint64_t epoch){if(s && s->identity==(uintptr_t)s && s->epoch==epoch && s->busy){s->state=MT76_FENCE5_FAULT;return false;}if(!valid(s) || epoch!=s->epoch)return false;return pin(s,s->dma);}
