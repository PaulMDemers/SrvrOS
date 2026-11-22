#include <srvros/mt76_fence5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct mt76_dma5 replacement;static struct mt76_dma5 dma;static struct mt76_fence5 f;
static struct mt76_fence5_command pending;
static const uint64_t rings[2]={0x1000,0x2000},pages[2][4]={{0x3000,0x4000,0x5000,0x6000},{0x7000,0x8000,0x9000,0xa000}};
static const unsigned dcis[2]={8,9};
static unsigned mutation,submits,waits,reads,drains;static bool held;
static bool owned(void *c,uint64_t e,unsigned slot){assert(c==&pending && e==9 && slot==2);switch(mutation){case 50:f.dma=&replacement;break;case 51:f.state=MT76_FENCE5_COMMIT;break;case 52:f.io.submit=0;break;case 53:f.command.token++;break;case 54:(void)mt76_fence5_cancel(&f,9);break;case 55:dma.last_token[1]++;break;case 56:f.guard.identity=0;break;case 69:dma.positions=255;break;}return mutation!=1;}
static void mutate_command(struct mt76_fence5_command *c,unsigned field){switch(field){case 0:c->epoch++;break;case 1:c->token++;break;case 2:c->pointer++;break;case 3:c->dequeue++;break;case 4:c->slot++;break;case 5:c->dci++;break;case 6:c->type++;break;}}
static enum mt76_fence5_result submit(void *c,const struct mt76_fence5_command *want,struct mt76_fence5_command *out){
 assert(c==&pending);submits++;pending=*want;pending.pointer=mutation==18?0x100010:0x100000+16*want->token;if(mutation==19)pending.pointer=rings[0];
 if(mutation==66)pending.pointer=pages[1][2];
 if(mutation==67)pending.pointer=0x1003f0;
 *out=pending;
 if(mutation>=10 && mutation<17)mutate_command(out,mutation-10);
 if(mutation==60)f.command.token++;
 return mutation==17?MT76_FENCE5_FAILED:MT76_FENCE5_READY;
}
static enum mt76_fence5_result completion(void *c,struct mt76_fence5_completion *out){
 assert(c==&pending);waits++;if(held)return MT76_FENCE5_PENDING;
 out->command=pending;out->cc=1;
 if(mutation>=20 && mutation<27)mutate_command(&out->command,mutation-20);
 if(mutation==27)out->cc=6;
 if(mutation==61)f.phase_us++;
 if(mutation==64)(void)mt76_fence5_cancel(&f,9);
 return mutation==28?MT76_FENCE5_FAILED:MT76_FENCE5_READY;
}
static bool endpoint(void *c,unsigned slot,unsigned dci,struct mt76_fence5_context *out){
 assert(c==&pending);reads++;*out=(struct mt76_fence5_context){.epoch=9,.slot=slot,.dci=dci,.state=3,.dequeue=pending.type==15?rings[f.direction]|1:pending.dequeue};
 switch(mutation){case 30:out->epoch++;break;case 31:out->slot++;break;case 32:out->dci++;break;case 33:out->state=1;break;case 34:if(pending.type==16)out->dequeue^=1;break;case 36:out->dequeue|=2;break;case 37:out->dequeue=rings[f.direction]-16;break;case 38:out->dequeue=rings[f.direction]+1024;break;case 68:out->dequeue=rings[f.direction]+16*(dma.positions+1);break;}
 if(mutation==62)f.state=MT76_FENCE5_COMMIT;
 return mutation!=35;
}
static bool frontier(void *c,struct mt76_fence5_frontier *out){
 assert(c==&pending);drains++;*out=(struct mt76_fence5_frontier){.epoch=9,.sequence=drains,.empty=true};
 switch(mutation){case 40:out->epoch++;break;case 41:out->sequence=0;break;case 42:out->target_events=1;break;case 43:out->command_pending=true;break;case 44:out->scanned=33;break;case 45:out->empty=false;out->scanned=16;break;case 46:out->sequence=1;break;}
 if(mutation==63)f.dma=&replacement;
 if(mutation==65)(void)mt76_fence5_cancel(&f,9);
 return mutation!=47;
}
static void begin_positions(unsigned positions){
 memset(&dma,0,sizeof dma);memset(&f,0,sizeof f);memset(&pending,0,sizeof pending);mutation=submits=waits=reads=drains=0;held=false;
 assert(mt76_dma5_init_ring(&dma,9,rings,pages,positions,100));
 struct mt76_fence5_io io={&pending,owned,submit,completion,endpoint,frontier};assert(mt76_fence5_init(&f,&dma,9,2,dcis,&io,100));
}
static void begin(void){begin_positions(63);}
static void fill(unsigned d){for(unsigned i=0;i<dma.positions;i++){
 struct mt76_dma5_lease l,loan;assert(mt76_dma5_prepare(&dma,d,dma.last_token[d]+1,4096,100,&l));assert(mt76_dma5_publish(&dma,&l,100));assert(mt76_dma5_complete(&dma,&l,d?13:1,d?3072:0,100));if(d){assert(mt76_dma5_acquire(&dma,9,&loan));assert(mt76_dma5_release(&dma,&loan));}
}}
static bool run(void){for(unsigned i=0;i<32;i++){if(!mt76_fence5_step(&f,100))return false;if(f.state==MT76_FENCE5_IDLE)return true;}assert(!"unbounded engine");return false;}
int main(void){
 begin();for(unsigned round=0;round<15;round++)for(unsigned d=0;d<2;d++){fill(d);assert(mt76_fence5_begin(&f,d,100));assert(run());assert(dma.cursor[d]==0 && dma.cycle[d]==(1u^((round+1)&1u)));}
 assert(f.completed==30 && dma.wraps==30 && submits==60 && !dma.faults);
 unsigned cases[]={1,10,11,12,13,14,15,16,17,19,20,21,22,23,24,25,26,27,28,30,31,32,33,34,35,36,37,38,40,41,42,43,44,45,46,47,50,51,52,53,54,55,56,60,61,62,63,64,65,66,67};
 for(unsigned i=0;i<sizeof cases/sizeof *cases;i++){begin();fill(0);assert(mt76_fence5_begin(&f,0,100));mutation=cases[i];assert(!run());assert(f.state==MT76_FENCE5_FAULT && dma.state==MT76_DMA5_FAULT && !dma.wraps && dma.cursor[0]==63);}
 begin();fill(0);assert(mt76_fence5_begin(&f,0,100));assert(mt76_fence5_step(&f,100));held=true;assert(mt76_fence5_step(&f,750099));assert(!mt76_fence5_step(&f,750100));assert(dma.state==MT76_DMA5_FAULT);
 begin();fill(0);assert(mt76_fence5_begin(&f,0,100));assert(!mt76_fence5_step(&f,99));assert(dma.state==MT76_DMA5_FAULT);
 begin();fill(0);assert(mt76_fence5_begin(&f,0,100));dma.last_token[1]++;assert(!mt76_fence5_step(&f,100));assert(dma.state==MT76_DMA5_FAULT);
 begin();fill(0);assert(mt76_fence5_begin(&f,0,100));f.io.context=&dma;assert(!mt76_fence5_step(&f,100));
 begin();fill(0);struct mt76_fence5 copied=f;assert(!mt76_fence5_begin(&copied,0,100) && !dma.faults);
 begin();fill(0);assert(!mt76_fence5_begin(&f,1,100));assert(mt76_fence5_begin(&f,0,100));assert(!mt76_fence5_cancel(&f,8) && f.state==MT76_FENCE5_STOP);assert(!mt76_fence5_cancel(&f,9) && f.state==MT76_FENCE5_FAULT && dma.state==MT76_DMA5_FAULT);
 begin();fill(0);f.next_token=UINT64_MAX-1;assert(!mt76_fence5_begin(&f,0,100));
 begin();fill(0);f.sequence=dma.last_fence=UINT64_MAX;assert(!mt76_fence5_begin(&f,0,100));
 begin();fill(1);dma.pages[1][0].state=MT76_DMA5_BORROWED;assert(!mt76_fence5_begin(&f,1,100) && !dma.wraps && dma.state==MT76_DMA5_RUNNING);
 begin();fill(0);assert(mt76_fence5_begin(&f,0,100));assert(mt76_dma5_close(&dma,9));assert(!mt76_fence5_step(&f,100) && dma.state==MT76_DMA5_FAULT);
 begin();fill(0);mutation=18;assert(mt76_fence5_begin(&f,0,100));assert(!run() && dma.state==MT76_DMA5_FAULT);
 begin();for(unsigned i=0;i<31;i++){fill(i%2);assert(mt76_fence5_begin(&f,i%2,100));assert(run());}fill(0);assert(!mt76_fence5_begin(&f,0,100) && dma.state==MT76_DMA5_FAULT && submits==62);
 begin();fill(0);dma.last_us=101;assert(!mt76_fence5_begin(&f,0,100) && dma.state==MT76_DMA5_FAULT);
 begin();fill(0);assert(mt76_fence5_begin(&f,0,100));while(f.state!=MT76_FENCE5_COMMIT)assert(mt76_fence5_step(&f,100));mutation=1;assert(!mt76_fence5_step(&f,100) && !dma.wraps && dma.state==MT76_DMA5_FAULT);
 begin();fill(0);struct mt76_dma5_lease other;assert(mt76_dma5_prepare(&dma,1,1,4096,100,&other));assert(mt76_dma5_publish(&dma,&other,100));assert(!mt76_fence5_begin(&f,0,100) && dma.state==MT76_DMA5_RUNNING);
 begin();fill(0);dma.pages[1][2].lease.page=pages[0][0];assert(!mt76_fence5_begin(&f,0,100) && dma.state==MT76_DMA5_FAULT);
 /* A larger transfer segment changes no command-pointer/no-reuse bound. */
 begin_positions(127);
 for(unsigned round=0;round<2;round++)for(unsigned d=0;d<2;d++){
  fill(d);assert(mt76_fence5_begin(&f,d,100));assert(run());
  assert(!dma.cursor[d] && dma.cycle[d]==(1u^((round+1)&1u)));
 }
 assert(f.completed==4 && dma.wraps==4 && submits==8 && !dma.faults);
 for(unsigned m=68;m<=69;m++){
  begin_positions(127);fill(1);assert(mt76_fence5_begin(&f,1,100));mutation=m;
  assert(!run() && f.state==MT76_FENCE5_FAULT && dma.state==MT76_DMA5_FAULT && !dma.wraps);
 }
 begin_positions(127);fill(0);dma.positions=63;assert(!mt76_fence5_begin(&f,0,100) && f.state==MT76_FENCE5_FAULT && dma.cursor[0]==127 && !dma.wraps);
 begin();fill(0);dma.positions=127;assert(!mt76_fence5_begin(&f,0,100) && dma.state==MT76_DMA5_FAULT);
 const unsigned bad_positions[]={64,126,128,255};
 for(unsigned i=0;i<sizeof bad_positions/sizeof *bad_positions;i++){unsigned n=bad_positions[i];
  begin();memset(&f,0,sizeof f);dma.positions=n;
  struct mt76_fence5_io io={&pending,owned,submit,completion,endpoint,frontier};
  assert(!mt76_fence5_init(&f,&dma,9,2,dcis,&io,100));
 }
 begin();memset(&f,0,sizeof f);memset(&dma,0,sizeof dma);
 assert(mt76_dma5_init_noop_ring(&dma,9,rings[0],127,100));
 struct mt76_fence5_io rejected_noop={&pending,owned,submit,completion,endpoint,frontier};
 assert(!mt76_fence5_init(&f,&dma,9,2,dcis,&rejected_noop,100));
 assert(!replacement.identity && replacement.state==MT76_DMA5_EMPTY);
 puts("mt76-fence5: PASS30 two-direction fences/60 exact commands,unique62command bound,identity/context/frontier faults,time bounds,loans,cancel,epoch/ledger/IO tamper;127-position transfer fences and sealed geometry refusals;software callback engine,no native acceptance");return 0;
}
