#include <srvros/mt76_dma5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct mt76_dma5 s;
static const uint64_t rings[2]={0x1000,0x2000},pages[2][4]={{0x3000,0x4000,0x5000,0x6000},{0x7000,0x8000,0x9000,0xa000}};
static void begin(void){memset(&s,0,sizeof s);assert(mt76_dma5_init(&s,9,rings,pages,100));}
static struct mt76_dma5_retirement retirement(void){return (struct mt76_dma5_retirement){.epoch=9,.keys_erased=true,.mac_disabled=true,.tx_stopped=true,.rx_stopped=true,.events_drained=true,.callbacks_drained=true,.parent_restored=true};}
static struct mt76_dma5_lease issue(unsigned d,uint64_t token){struct mt76_dma5_lease l;assert(mt76_dma5_prepare(&s,d,token,4096,100,&l));assert(mt76_dma5_publish(&s,&l,100));return l;}
static void wrap(unsigned d,uint64_t sequence){struct mt76_dma5_fence f={.epoch=9,.sequence=sequence,.dequeue=rings[d]|(s.cycle[d]^1u),.direction=d,.stopped=true,.events_drained=true,.dequeue_set=true};assert(mt76_dma5_wrap(&s,&f));}
int main(void){
 begin();struct mt76_dma5_lease first={0},l,rx,borrow,out={0};unsigned reused[2][4]={{0}};
 for(unsigned i=1;i<=2048;i++){
  for(unsigned d=0;d<2;d++){
   if(s.cursor[d]==255)wrap(d,s.last_fence+1);
   l=issue(d,i);reused[d][l.index]++;if(i==1 && d==0)first=l;
   assert(l.trb==rings[d]+16*((i-1)%255) && l.cycle==(1u^(((i-1)/255)&1u)));
   assert(!mt76_dma5_prepare(&s,d,i+1,100,100,&out));
   assert(mt76_dma5_complete(&s,&l,d?13:1,d?200:0,100));
   if(d){assert(mt76_dma5_acquire(&s,9,&borrow) && borrow.received==3896);assert(mt76_dma5_release(&s,&borrow));assert(!mt76_dma5_release(&s,&borrow));}
  }
 }
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<4;i++)assert(reused[d][i]==512);
 assert(s.published==4096 && s.completed==4096 && s.borrowed==2048 && s.released==2048 && s.wraps==16);
 assert(!mt76_dma5_complete(&s,&first,1,0,100) && s.state==MT76_DMA5_FAULT);
 struct mt76_dma5_retirement p=retirement();assert(mt76_dma5_retire(&s,&p));assert(!mt76_dma5_prepare(&s,0,3000,1,100,&out));
 /* Four copied-but-borrowed pages exhaust capacity. Close/fault must not revoke
  * any consumer loan,even after every independent hardware proof is true. */
 begin();struct mt76_dma5_lease loans[4];
 for(unsigned i=0;i<4;i++){rx=issue(1,i+1);assert(mt76_dma5_complete(&s,&rx,13,4000,100));assert(mt76_dma5_acquire(&s,9,&loans[i]));}
 assert(!mt76_dma5_prepare(&s,1,5,4096,100,&out));assert(mt76_dma5_close(&s,9));assert(!mt76_dma5_retire(&s,&p));mt76_dma5_fault(&s);
 for(unsigned i=0;i<4;i++)assert(mt76_dma5_release(&s,&loans[i]));
 assert(mt76_dma5_retire(&s,&p));
 /* Each retirement field is necessary;completion alone never retires context. */
 for(unsigned field=0;field<7;field++){
  begin();l=issue(0,1);assert(mt76_dma5_close(&s,9));p=retirement();
  bool *fields[]={&p.keys_erased,&p.mac_disabled,&p.tx_stopped,&p.rx_stopped,&p.events_drained,&p.callbacks_drained,&p.parent_restored};*fields[field]=false;
  assert(!mt76_dma5_retire(&s,&p) && s.pages[0][l.index].state==MT76_DMA5_DEVICE);*fields[field]=true;assert(mt76_dma5_retire(&s,&p));
 }
 begin();rx=issue(1,1);assert(mt76_dma5_tick(&s,1000000000) && s.pages[1][rx.index].state==MT76_DMA5_DEVICE);assert(mt76_dma5_close(&s,9));p=retirement();p.rx_stopped=false;assert(!mt76_dma5_retire(&s,&p));p.rx_stopped=true;assert(mt76_dma5_retire(&s,&p));
 begin();l=issue(0,1);assert(mt76_dma5_tick(&s,750099));assert(!mt76_dma5_tick(&s,750100));assert(s.pages[0][l.index].state==MT76_DMA5_DEVICE);p=retirement();assert(mt76_dma5_retire(&s,&p));
 begin();l=issue(0,1);assert(!mt76_dma5_complete(&s,&l,1,1,100) && s.pages[0][l.index].state==MT76_DMA5_DEVICE);
 begin();rx=issue(1,1);assert(!mt76_dma5_complete(&s,&rx,13,4097,100));
 begin();rx=issue(1,1);assert(!mt76_dma5_complete(&s,&rx,6,0,100));
 begin();l=issue(0,1);rx=l;rx.epoch=8;struct mt76_dma5 unchanged=s;assert(!mt76_dma5_complete(&s,&rx,1,0,100) && !memcmp(&s,&unchanged,sizeof s));
 for(unsigned field=0;field<9;field++){
  begin();l=issue(0,1);rx=l;
  switch(field){case 0:rx.ticket++;break;case 1:rx.token++;break;case 2:rx.page+=4096;break;case 3:rx.trb+=16;break;case 4:rx.direction=1;break;case 5:rx.index=3;break;case 6:rx.length--;break;case 7:rx.cycle^=1;break;case 8:rx.received++;break;}
  assert(!mt76_dma5_complete(&s,&rx,1,0,100) && s.state==MT76_DMA5_FAULT && s.pages[0][l.index].state==MT76_DMA5_DEVICE);
 }
 begin();assert(mt76_dma5_prepare(&s,0,1,1500,100,&l));assert(mt76_dma5_cancel(&s,&l));assert(!mt76_dma5_prepare(&s,0,1,1500,100,&out));assert(mt76_dma5_prepare(&s,0,2,1500,100,&rx));assert(rx.trb==l.trb && rx.ticket!=l.ticket);assert(!mt76_dma5_cancel(&s,&l));assert(mt76_dma5_cancel(&s,&rx));
 begin();l=issue(0,1);assert(!mt76_dma5_cancel(&s,&l));assert(mt76_dma5_complete(&s,&l,1,0,100));assert(!mt76_dma5_complete(&s,&l,1,0,100));
 /* Wrap is a separate checked fence,not an implicit modulo operation. */
 for(unsigned missing=0;missing<6;missing++){
  begin();for(unsigned i=1;i<=255;i++){l=issue(0,i);assert(mt76_dma5_complete(&s,&l,1,0,100));}
  assert(!mt76_dma5_prepare(&s,0,256,1,100,&out));
  struct mt76_dma5_fence f={.epoch=9,.sequence=1,.dequeue=rings[0],.direction=0,.stopped=true,.events_drained=true,.dequeue_set=true};
  switch(missing){case 0:f.stopped=false;break;case 1:f.events_drained=false;break;case 2:f.dequeue_set=false;break;case 3:f.dequeue|=1;break;case 4:f.epoch++;break;case 5:f.sequence=0;break;}
  unchanged=s;assert(!mt76_dma5_wrap(&s,&f) && !memcmp(&s,&unchanged,sizeof s));wrap(0,1);assert(s.cursor[0]==0 && s.cycle[0]==0);l=issue(0,256);assert(l.trb==rings[0] && l.cycle==0);
 }
 /* At the wrap boundary,a returned USB event still cannot revoke an RX loan. */
 begin();for(unsigned i=1;i<=255;i++){rx=issue(1,i);assert(mt76_dma5_complete(&s,&rx,13,4096,100));assert(mt76_dma5_acquire(&s,9,&borrow));if(i<255)assert(mt76_dma5_release(&s,&borrow));}
 struct mt76_dma5_fence held={.epoch=9,.sequence=1,.dequeue=rings[1],.direction=1,.stopped=true,.events_drained=true,.dequeue_set=true};
 assert(!mt76_dma5_wrap(&s,&held));assert(mt76_dma5_release(&s,&borrow));assert(mt76_dma5_wrap(&s,&held));assert(!mt76_dma5_wrap(&s,&held));
 begin();rx=issue(1,1);assert(mt76_dma5_complete(&s,&rx,13,0,100));assert(mt76_dma5_close(&s,9));assert(!mt76_dma5_acquire(&s,9,&borrow));p=retirement();assert(mt76_dma5_retire(&s,&p));
 begin();l=issue(0,1);assert(mt76_dma5_close(&s,9));assert(!mt76_dma5_prepare(&s,0,2,1,100,&out));assert(mt76_dma5_complete(&s,&l,1,0,100));p=retirement();assert(mt76_dma5_retire(&s,&p));
 begin();assert(mt76_dma5_prepare(&s,0,1,10,100,&l));mt76_dma5_fault(&s);assert(!mt76_dma5_cancel(&s,&l) && s.pages[0][l.index].state==MT76_DMA5_PREPARED);
 memset(&s,0,sizeof s);assert(mt76_dma5_init_ring(&s,9,rings,pages,63,100));for(unsigned i=1;i<=63;i++){l=issue(0,i);assert(mt76_dma5_complete(&s,&l,1,0,100));}assert(!mt76_dma5_prepare(&s,0,64,1,100,&out));wrap(0,1);assert(s.cursor[0]==0 && s.cycle[0]==0);
 memset(&s,0,sizeof s);assert(!mt76_dma5_init_ring(&s,9,rings,pages,1,100));assert(!mt76_dma5_init_ring(&s,9,rings,pages,256,100));
 begin();assert(!mt76_dma5_tick(&s,99));
 begin();s.next_ticket=UINT64_MAX;assert(!mt76_dma5_prepare(&s,0,1,1,100,&out) && s.state==MT76_DMA5_FAULT);
 begin();struct mt76_dma5 copy=s;assert(!mt76_dma5_prepare(&copy,0,1,1,100,&out));assert(!mt76_dma5_prepare(&s,0,1,1,100,&s.pages[0][0].lease));
 for(unsigned i=0;i<10;i++){
  uint64_t bad[2][4];memcpy(bad,pages,sizeof bad);uint64_t r[2]={rings[0],rings[1]};uint64_t *v=i<2?&r[i]:&bad[(i-2)/4][(i-2)%4];*v=0x1001;memset(&s,0,sizeof s);assert(!mt76_dma5_init(&s,9,r,bad,100) && !s.identity);
 }
 memset(&s,0,sizeof s);uint64_t alias[2][4];memcpy(alias,pages,sizeof alias);alias[1][3]=rings[0];assert(!mt76_dma5_init(&s,9,rings,alias,100));
 puts("mt76-dma5: PASS4096 completions/2048 RX loans/16 explicit wrap fences,all eight pages reused512 times,stale handles/epochs,tokens,length/CC/time faults,borrowed lifetime,retirement and alias refusals;software external-proof model,no native ring authority");return 0;
}
