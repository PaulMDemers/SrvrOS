/* Reuse only the existing standalone fence IO oracle;production functions are
 * linked separately. All completions/frontiers below are software fixtures. */
#define main existing_fence_fixture_main
#include "test_mt76_fence5.c"
#undef main
static void noop_begin(void){
 begin();memset(&dma,0,sizeof dma);memset(&f,0,sizeof f);
 assert(mt76_dma5_init_noop_ring(&dma,9,rings[0],63,100));
 struct mt76_fence5_io io={&pending,owned,submit,completion,endpoint,frontier};
 assert(mt76_fence5_init(&f,&dma,9,2,dcis,&io,100));
}
static struct mt76_dma5_noop_lease issue_noop(uint64_t token){struct mt76_dma5_noop_lease l;assert(mt76_dma5_noop_prepare(&dma,token,100,&l));assert(mt76_dma5_noop_publish(&dma,&l,100));return l;}
static struct mt76_dma5_retirement retired(void){return (struct mt76_dma5_retirement){.epoch=9,.keys_erased=true,.mac_disabled=true,.tx_stopped=true,.rx_stopped=true,.events_drained=true,.callbacks_drained=true,.parent_restored=true};}
int main(void){
 noop_begin();struct mt76_dma5_noop_lease first={0},l,out;struct mt76_dma5_lease data;
 for(unsigned i=0;i<63;i++){l=issue_noop(i+1);if(!i)first=l;assert(l.trb==rings[0]+16*i && l.cycle==1);assert(!mt76_dma5_noop_prepare(&dma,i+2,100,&out));assert(mt76_dma5_noop_complete(&dma,&l,1,0,100));}
 assert(!mt76_dma5_noop_prepare(&dma,64,100,&out));assert(dma.cursor[0]==63 && dma.next_ticket==63 && dma.last_token[0]==63);
 assert(mt76_fence5_begin(&f,0,100));assert(run());l=issue_noop(64);assert(l.trb==first.trb && l.ticket!=first.ticket && l.cycle==0);assert(mt76_dma5_noop_complete(&dma,&l,1,0,100));
 assert(dma.noop_prepared==64 && dma.noop_published==64 && dma.noop_completed==64 && dma.wraps==1 && !dma.prepared && !dma.published && !dma.completed && !dma.borrowed);
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<4;i++)assert(!dma.pages[d][i].lease.page && dma.pages[d][i].state==MT76_DMA5_FREE);
 assert(!dma.ring[1] && !mt76_dma5_prepare(&dma,0,65,1,100,&data) && !mt76_dma5_prepare(&dma,1,1,1,100,&data) && !mt76_dma5_acquire(&dma,9,&data));
 assert(!mt76_dma5_noop_complete(&dma,&first,1,0,100) && dma.state==MT76_DMA5_FAULT);
 for(unsigned field=0;field<5;field++){noop_begin();l=issue_noop(1);out=l;switch(field){case 0:out.ticket++;break;case 1:out.token++;break;case 2:out.trb+=16;break;case 3:out.cycle^=1;break;case 4:out.epoch++;break;}struct mt76_dma5 before=dma;assert(!mt76_dma5_noop_complete(&dma,&out,1,0,100));if(field==4)assert(!memcmp(&dma,&before,sizeof dma));else assert(dma.state==MT76_DMA5_FAULT && dma.noop_state==MT76_DMA5_DEVICE);}
 noop_begin();assert(mt76_dma5_noop_prepare(&dma,1,100,&l));assert(mt76_dma5_noop_cancel(&dma,&l));assert(!mt76_dma5_noop_prepare(&dma,1,100,&out));assert(mt76_dma5_noop_prepare(&dma,2,100,&out));assert(out.trb==l.trb && out.ticket!=l.ticket);assert(!mt76_dma5_noop_cancel(&dma,&l));assert(mt76_dma5_noop_publish(&dma,&out,100));assert(!mt76_dma5_noop_cancel(&dma,&out));
 noop_begin();l=issue_noop(1);assert(mt76_dma5_tick(&dma,750099));assert(!mt76_dma5_tick(&dma,750100) && dma.noop_state==MT76_DMA5_DEVICE);
 noop_begin();l=issue_noop(1);assert(!mt76_dma5_noop_complete(&dma,&l,1,0,99) && dma.state==MT76_DMA5_FAULT);
 for(unsigned code=0;code<2;code++){noop_begin();l=issue_noop(1);assert(!mt76_dma5_noop_complete(&dma,&l,code?1:13,code?1:0,100) && dma.noop_state==MT76_DMA5_DEVICE && dma.state==MT76_DMA5_FAULT);}
 noop_begin();l=issue_noop(1);assert(mt76_dma5_close(&dma,9));assert(!mt76_dma5_noop_prepare(&dma,2,100,&out));assert(mt76_dma5_noop_complete(&dma,&l,1,0,100));struct mt76_dma5_retirement p=retired();p.tx_stopped=false;assert(!mt76_dma5_retire(&dma,&p));p.tx_stopped=true;assert(mt76_dma5_retire(&dma,&p));
 noop_begin();l=issue_noop(1);assert(mt76_dma5_close(&dma,9));p=retired();p.events_drained=false;assert(!mt76_dma5_retire(&dma,&p));p.events_drained=true;assert(mt76_dma5_retire(&dma,&p));
 begin();assert(!mt76_dma5_prepare(&dma,0,1,0,100,&data));assert(mt76_dma5_prepare(&dma,0,1,1,100,&data));assert(!mt76_dma5_noop_prepare(&dma,2,100,&out));assert(mt76_dma5_publish(&dma,&data,100));assert(!mt76_dma5_noop_prepare(&dma,2,100,&out));assert(mt76_dma5_complete(&dma,&data,1,0,100));l=issue_noop(2);assert(l.ticket==data.ticket+1 && l.trb==0x1010);assert(!mt76_dma5_prepare(&dma,0,3,1,100,&data));assert(mt76_dma5_noop_complete(&dma,&l,1,0,100));assert(mt76_dma5_prepare(&dma,0,3,1,100,&data));assert(data.ticket==l.ticket+1 && data.trb==0x1020);
 noop_begin();for(unsigned i=1;i<=63;i++){l=issue_noop(i);if(i<63)assert(mt76_dma5_noop_complete(&dma,&l,1,0,100));}assert(!mt76_fence5_begin(&f,0,100));struct mt76_dma5_fence boundary={.epoch=9,.sequence=1,.dequeue=rings[0],.direction=0,.stopped=true,.events_drained=true,.dequeue_set=true};assert(!mt76_dma5_wrap(&dma,&boundary));assert(mt76_dma5_noop_complete(&dma,&l,1,0,100));assert(mt76_fence5_begin(&f,0,100));assert(run());
 noop_begin();assert(!mt76_dma5_init_noop_ring(&dma,9,rings[0],63,100));
 noop_begin();assert(!mt76_dma5_noop_prepare(&dma,1,100,&dma.noop));struct mt76_dma5 copy=dma;assert(!mt76_dma5_noop_prepare(&copy,1,100,&out));dma.next_ticket=UINT64_MAX;assert(!mt76_dma5_noop_prepare(&dma,1,100,&out) && dma.state==MT76_DMA5_FAULT);
 puts("mt76-dma5-noop: PASS63 typed bufferless descriptors/fence/opposite-cycle64th,separate counters,no payload/page/RX authority,shared TX namespace,identity/cancel/clock/deadline/CC faults;software only");return 0;
}
