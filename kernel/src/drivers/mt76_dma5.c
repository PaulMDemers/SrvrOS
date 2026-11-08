#include <srvros/mt76_dma5.h>
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned x=0;while(n--)x|=*v++;return !x;}
static bool valid(const struct mt76_dma5 *s){return s && s->identity==(uintptr_t)s && s->epoch && s->state>=MT76_DMA5_RUNNING && s->state<=MT76_DMA5_RETIRED && s->positions>=2 && s->positions<=MT76_DMA5_TRBS && s->cursor[0]<=s->positions && s->cursor[1]<=s->positions && s->cycle[0]<=1 && s->cycle[1]<=1 && s->next_page[0]<MT76_DMA5_PAGES && s->next_page[1]<MT76_DMA5_PAGES;}
static bool live(const struct mt76_dma5 *s){return valid(s) && (s->state==MT76_DMA5_RUNNING || s->state==MT76_DMA5_CLOSING);}
static bool equal(const struct mt76_dma5_lease *a,const struct mt76_dma5_lease *b){return a->epoch==b->epoch && a->ticket==b->ticket && a->token==b->token && a->page==b->page && a->trb==b->trb && a->direction==b->direction && a->index==b->index && a->length==b->length && a->received==b->received && a->cycle==b->cycle;}
static struct mt76_dma5_page *match(struct mt76_dma5 *s,const struct mt76_dma5_lease *l){
 if(!valid(s) || !l || overlap(s,sizeof *s,l,sizeof *l) || l->epoch!=s->epoch || l->direction>1 || l->index>=MT76_DMA5_PAGES)return 0;
 struct mt76_dma5_page *p=&s->pages[l->direction][l->index];return equal(&p->lease,l)?p:0;
}
void mt76_dma5_fault(struct mt76_dma5 *s){if(valid(s) && s->state!=MT76_DMA5_FAULT && s->state!=MT76_DMA5_RETIRED){s->state=MT76_DMA5_FAULT;s->faults++;}}
bool mt76_dma5_tick(struct mt76_dma5 *s,uint64_t now){
 if(!live(s))return false;
 bool good=now>=s->last_us;
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<MT76_DMA5_PAGES;i++){
  const struct mt76_dma5_page *p=&s->pages[d][i];
  if(d==MT76_DMA5_TX && p->state==MT76_DMA5_DEVICE && (now<p->started || now-p->started>=MT76_DMA5_TIMEOUT_US))good=false;
 }
 if(!good){mt76_dma5_fault(s);return false;}s->last_us=now;return true;
}
bool mt76_dma5_init_ring(struct mt76_dma5 *s,uint64_t epoch,const uint64_t rings[2],const uint64_t pages[2][MT76_DMA5_PAGES],unsigned positions,uint64_t now){
 if(!s || !epoch || positions<2 || positions>MT76_DMA5_TRBS || !rings || !pages || overlap(s,sizeof *s,rings,2*sizeof *rings) || overlap(s,sizeof *s,pages,2*sizeof *pages) || !zero(s,sizeof *s))return false;
 uint64_t all[10];all[0]=rings[0];all[1]=rings[1];
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<MT76_DMA5_PAGES;i++)all[2+d*MT76_DMA5_PAGES+i]=pages[d][i];
 for(unsigned i=0;i<10;i++){if(!all[i] || (all[i]&4095) || all[i]>UINT64_MAX-4095)return false;for(unsigned j=0;j<i;j++)if(all[i]==all[j])return false;}
 s->identity=(uintptr_t)s;s->positions=positions;s->epoch=epoch;s->last_us=now;s->state=MT76_DMA5_RUNNING;
 for(unsigned d=0;d<2;d++){s->ring[d]=rings[d];s->cycle[d]=1;for(unsigned i=0;i<MT76_DMA5_PAGES;i++)s->pages[d][i].lease.page=pages[d][i];}return true;
}
bool mt76_dma5_prepare(struct mt76_dma5 *s,unsigned d,uint64_t token,unsigned length,uint64_t now,struct mt76_dma5_lease *out){
 if(!valid(s) || s->state!=MT76_DMA5_RUNNING || d>1 || !out || overlap(s,sizeof *s,out,sizeof *out) || !token || token<=s->last_token[d] || !length || length>MT76_DMA5_BYTES)return false;
 if(!mt76_dma5_tick(s,now))return false;
 if(s->next_ticket==UINT64_MAX){mt76_dma5_fault(s);return false;}
 if(s->cursor[d]==s->positions)return false;
 for(unsigned i=0;i<MT76_DMA5_PAGES;i++)if(s->pages[d][i].state==MT76_DMA5_PREPARED || s->pages[d][i].state==MT76_DMA5_DEVICE)return false;
 for(unsigned k=0;k<MT76_DMA5_PAGES;k++){
  unsigned i=(s->next_page[d]+k)%MT76_DMA5_PAGES;struct mt76_dma5_page *p=&s->pages[d][i];if(p->state!=MT76_DMA5_FREE)continue;
  p->lease=(struct mt76_dma5_lease){.epoch=s->epoch,.ticket=++s->next_ticket,.token=token,.page=p->lease.page,.trb=s->ring[d]+16*s->cursor[d],.direction=d,.index=i,.length=length,.cycle=s->cycle[d]};
  p->state=MT76_DMA5_PREPARED;s->last_token[d]=token;s->next_page[d]=(i+1)%MT76_DMA5_PAGES;s->prepared++;*out=p->lease;return true;
 }return false;
}
bool mt76_dma5_cancel(struct mt76_dma5 *s,const struct mt76_dma5_lease *l){
 struct mt76_dma5_page *p=match(s,l);if(!live(s) || !p || p->state!=MT76_DMA5_PREPARED)return false;p->state=MT76_DMA5_FREE;s->cancelled++;return true;
}
bool mt76_dma5_publish(struct mt76_dma5 *s,const struct mt76_dma5_lease *l,uint64_t now){
 struct mt76_dma5_page *p=match(s,l);if(!valid(s) || s->state!=MT76_DMA5_RUNNING || !p || p->state!=MT76_DMA5_PREPARED || !mt76_dma5_tick(s,now))return false;
 p->state=MT76_DMA5_DEVICE;p->started=now;s->cursor[l->direction]++;s->published++;return true;
}
bool mt76_dma5_complete(struct mt76_dma5 *s,const struct mt76_dma5_lease *l,unsigned cc,unsigned residue,uint64_t now){
 struct mt76_dma5_page *p=match(s,l);
 if(!live(s) || !l || l->epoch!=s->epoch)return false;
 if(!p || p->state!=MT76_DMA5_DEVICE){mt76_dma5_fault(s);return false;}
 if(!mt76_dma5_tick(s,now))return false;
 bool good=l->direction==MT76_DMA5_TX?cc==1 && !residue:(cc==1 || cc==13) && residue<=l->length;
 if(!good){mt76_dma5_fault(s);return false;}
 p->lease.received=l->length-residue;p->state=l->direction==MT76_DMA5_RX?MT76_DMA5_COMPLETE:MT76_DMA5_FREE;s->completed++;return true;
}
bool mt76_dma5_acquire(struct mt76_dma5 *s,uint64_t epoch,struct mt76_dma5_lease *out){
 if(!valid(s) || s->state!=MT76_DMA5_RUNNING || epoch!=s->epoch || !out || overlap(s,sizeof *s,out,sizeof *out))return false;
 struct mt76_dma5_page *best=0;
 for(unsigned i=0;i<MT76_DMA5_PAGES;i++){struct mt76_dma5_page *p=&s->pages[1][i];if(p->state==MT76_DMA5_COMPLETE && (!best || p->lease.ticket<best->lease.ticket))best=p;}
 if(!best)return false;
 best->state=MT76_DMA5_BORROWED;s->borrowed++;*out=best->lease;return true;
}
bool mt76_dma5_release(struct mt76_dma5 *s,const struct mt76_dma5_lease *l){
 struct mt76_dma5_page *p=match(s,l);if(!valid(s) || s->state==MT76_DMA5_RETIRED || !p || p->state!=MT76_DMA5_BORROWED)return false;p->state=MT76_DMA5_FREE;s->released++;return true;
}
bool mt76_dma5_close(struct mt76_dma5 *s,uint64_t epoch){if(!valid(s) || epoch!=s->epoch || s->state==MT76_DMA5_RETIRED)return false;if(s->state==MT76_DMA5_RUNNING)s->state=MT76_DMA5_CLOSING;return true;}
bool mt76_dma5_wrap(struct mt76_dma5 *s,const struct mt76_dma5_fence *f){
 if(!valid(s) || s->state!=MT76_DMA5_RUNNING || !f || overlap(s,sizeof *s,f,sizeof *f) || f->epoch!=s->epoch || f->direction>1 || !f->sequence || f->sequence<=s->last_fence || !f->stopped || !f->events_drained || !f->dequeue_set)return false;
 unsigned d=f->direction;if(s->cursor[d]!=s->positions || f->dequeue!=(s->ring[d]|(s->cycle[d]^1u)))return false;
 for(unsigned i=0;i<MT76_DMA5_PAGES;i++)if(s->pages[d][i].state!=MT76_DMA5_FREE)return false;
 s->cursor[d]=0;s->cycle[d]^=1;s->last_fence=f->sequence;s->wraps++;return true;
}
bool mt76_dma5_retire(struct mt76_dma5 *s,const struct mt76_dma5_retirement *p){
 if(!valid(s) || !p || overlap(s,sizeof *s,p,sizeof *p) || p->epoch!=s->epoch || (s->state!=MT76_DMA5_CLOSING && s->state!=MT76_DMA5_FAULT) || !p->keys_erased || !p->mac_disabled || !p->tx_stopped || !p->rx_stopped || !p->events_drained || !p->callbacks_drained || !p->parent_restored)return false;
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<MT76_DMA5_PAGES;i++)if(s->pages[d][i].state==MT76_DMA5_BORROWED)return false;
 s->state=MT76_DMA5_RETIRED;return true;
}

bool mt76_dma5_init(struct mt76_dma5 *s,uint64_t epoch,const uint64_t rings[2],const uint64_t pages[2][MT76_DMA5_PAGES],uint64_t now){return mt76_dma5_init_ring(s,epoch,rings,pages,MT76_DMA5_TRBS,now);}
