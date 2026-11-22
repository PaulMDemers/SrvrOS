/* Production ledger with the native 63 usable positions. Fence booleans below
 * are synthetic external proofs, never controller or radio acceptance. */
#include <srvros/mt76_dma5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define POSITIONS 63u
#define EPOCH 17u
static struct mt76_dma5 s;
static const uint64_t rings[2]={0x1000,0x2000};
static const uint64_t pages[2][4]={{0x3000,0x4000,0x5000,0x6000},{0x7000,0x8000,0x9000,0xa000}};
static void begin(void){memset(&s,0,sizeof s);assert(mt76_dma5_init_ring(&s,EPOCH,rings,pages,POSITIONS,100));}
static struct mt76_dma5_lease issue(unsigned d,uint64_t token){
 struct mt76_dma5_lease l={0};assert(mt76_dma5_prepare(&s,d,token,4096,100,&l));assert(mt76_dma5_publish(&s,&l,100));return l;
}
static void consume(struct mt76_dma5_lease l){
 assert(mt76_dma5_complete(&s,&l,l.direction?13:1,l.direction?3072:0,100));
 if(l.direction){struct mt76_dma5_lease loan;assert(mt76_dma5_acquire(&s,EPOCH,&loan));assert(loan.received==1024);assert(mt76_dma5_release(&s,&loan));}
}
static struct mt76_dma5_fence fence(unsigned d){
 return (struct mt76_dma5_fence){.epoch=EPOCH,.sequence=s.last_fence+1,.dequeue=rings[d]|(s.cycle[d]^1u),.direction=d,.stopped=true,.events_drained=true,.dequeue_set=true};
}
static void fill(unsigned d){for(unsigned i=0;i<POSITIONS;i++)consume(issue(d,i+1));}
static void repeated(void){
 begin();unsigned counts[2][4]={{0}};uint64_t token[2]={0};
 for(unsigned round=0;round<64;round++){
  for(unsigned i=0;i<POSITIONS;i++){
   /* Outstanding RX and TX coexist; each direction still has only one TD. */
   struct mt76_dma5_lease rx=issue(1,++token[1]),tx=issue(0,++token[0]),out;
   assert(rx.trb==rings[1]+16*i && tx.trb==rings[0]+16*i);
   assert(rx.cycle==(1u^(round&1u)) && tx.cycle==rx.cycle);
   assert(rx.ticket!=tx.ticket);counts[1][rx.index]++;counts[0][tx.index]++;
   assert(!mt76_dma5_prepare(&s,0,token[0]+1,100,100,&out));
   assert(!mt76_dma5_prepare(&s,1,token[1]+1,100,100,&out));
   consume(rx);consume(tx);
  }
  for(unsigned d=0;d<2;d++){
   struct mt76_dma5_lease out;struct mt76_dma5_fence f=fence(d);
   assert(!mt76_dma5_prepare(&s,d,token[d]+1,100,100,&out));
   assert(mt76_dma5_wrap(&s,&f));assert(!mt76_dma5_wrap(&s,&f));
  }
 }
 assert(s.published==8064 && s.completed==8064 && s.borrowed==4032 && s.released==4032 && s.wraps==128 && !s.faults);
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<4;i++)assert(counts[d][i]==1008);
}
static void fences(void){
 for(unsigned d=0;d<2;d++)for(unsigned field=0;field<8;field++){
  begin();fill(d);struct mt76_dma5_fence f=fence(d);struct mt76_dma5 before=s;
  switch(field){case 0:f.stopped=false;break;case 1:f.events_drained=false;break;case 2:f.dequeue_set=false;break;case 3:f.dequeue^=1;break;case 4:f.dequeue+=16;break;case 5:f.epoch++;break;case 6:f.sequence=0;break;case 7:f.direction=2;break;}
  assert(!mt76_dma5_wrap(&s,&f) && !memcmp(&s,&before,sizeof s));
 }
 begin();struct mt76_dma5_fence f=fence(0);assert(!mt76_dma5_wrap(&s,&f));
 /* A globally monotonic fence sequence cannot be replayed on another endpoint. */
 fill(0);fill(1);f=fence(0);assert(mt76_dma5_wrap(&s,&f));f.direction=1;f.dequeue=rings[1];assert(!mt76_dma5_wrap(&s,&f));f.sequence++;assert(mt76_dma5_wrap(&s,&f));
}
static void loans(void){
 begin();for(unsigned i=1;i<POSITIONS;i++)consume(issue(1,i));
 struct mt76_dma5_lease l=issue(1,POSITIONS),loan;assert(mt76_dma5_complete(&s,&l,13,3072,100));assert(mt76_dma5_acquire(&s,EPOCH,&loan));
 struct mt76_dma5_fence f=fence(1);assert(!mt76_dma5_wrap(&s,&f));
 assert(s.pages[1][loan.index].state==MT76_DMA5_BORROWED);assert(mt76_dma5_release(&s,&loan));assert(mt76_dma5_wrap(&s,&f));assert(!mt76_dma5_release(&s,&loan));
 /* Consumer loans, not page count, bound receive capacity. */
 begin();struct mt76_dma5_lease held[4],out;
 for(unsigned i=0;i<4;i++){l=issue(1,i+1);assert(mt76_dma5_complete(&s,&l,13,3072,100));assert(mt76_dma5_acquire(&s,EPOCH,&held[i]));}
 assert(!mt76_dma5_prepare(&s,1,5,4096,100,&out));
 assert(mt76_dma5_release(&s,&held[2]));l=issue(1,5);assert(l.index==held[2].index);consume(l);
 for(unsigned i=0;i<4;i++)if(i!=2)assert(mt76_dma5_release(&s,&held[i]));
}
static void stale(void){
 for(unsigned wraps=1;wraps<=2;wraps++){
  begin();struct mt76_dma5_lease old=issue(0,1);consume(old);
  for(unsigned i=2;i<=wraps*POSITIONS;i++){
   if(s.cursor[0]==POSITIONS){struct mt76_dma5_fence f=fence(0);assert(mt76_dma5_wrap(&s,&f));}
   consume(issue(0,i));
  }
  struct mt76_dma5_fence f=fence(0);assert(mt76_dma5_wrap(&s,&f));
  struct mt76_dma5_lease current=issue(0,wraps*POSITIONS+1);
  assert(current.trb==old.trb && current.ticket!=old.ticket);
  assert(current.cycle==(old.cycle^(wraps&1u)));
  /* At two wraps even descriptor address AND cycle alias. Only an independently
   * drained hardware frontier lets the native binder select this current lease. */
  struct mt76_dma5_lease prior_epoch=current;prior_epoch.epoch--;struct mt76_dma5 before=s;
  assert(!mt76_dma5_complete(&s,&prior_epoch,1,0,100) && !memcmp(&s,&before,sizeof s));
  assert(!mt76_dma5_complete(&s,&old,1,0,100) && s.state==MT76_DMA5_FAULT);
  assert(s.pages[0][current.index].state==MT76_DMA5_DEVICE);
 }
}
int main(void){repeated();fences();loans();stale();puts("mt76-dma5-native63: PASS8064 completions/4032 loans/128 synthetic fences;all8 pages reused1008times;exact63-position boundaries,opposite-cycle proof,late lease/epoch refusal and loan backpressure;software only,no native fence authority");return 0;}
