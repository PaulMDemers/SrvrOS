#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
struct fake {struct mt76_rf_write *s;unsigned auth,ops,fail,deny,bad,delay,stuck_phase;unsigned value;bool corrupt;struct mt76_rf_write states[107];};
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t address,uint32_t *v){
 struct fake *f=p;struct mt76_rf_write *s=f->s;unsigned phase=s->phase;assert(op==f->ops && op<107);f->states[op]=*s;f->ops++;
 assert(write==(phase==4 || phase==9));
 if(phase<3 || (phase>=6 && phase<9) || phase>=11){unsigned n=phase<3?phase:phase<9?phase-6:phase-11;assert(address==(n==0?0x1004:n==1?0x2000:0x208));*v=n==1?0xf000f200:n==2?0x40000050:0;}
 else{assert(address==0x500);if(write)assert(*v==(phase==4?0xc0000641u:0x80000600u));
 else{unsigned i=phase==3?0:phase==5?1:2;
  if(f->stuck_phase==phase || s->polls[i]<f->delay)*v=0x80000600;
  else *v=phase==3?0x1600:phase==5?0x40000641:0x600|f->value;
  if(f->corrupt && phase==10)*v^=0x100;
 }}
 if(f->ops==f->bad)*v=UINT32_MAX;
 return f->ops!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_write *s){f->s=s;const struct mt76_rf_csr_io io={f,auth,transfer};return mt76_rf_write_apply(s,&io);}
int main(void){
 struct mt76_rf_write s={0};static struct fake f,baseline;f=(struct fake){0};
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==14 && s.reads==12 && s.writes==2 && s.phase==14 && f.ops==14 && f.auth==15);baseline=f;
 assert(s.words[2]==0x600 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned v=0;v<256;v++){s=(struct mt76_rf_write){0};f=(struct fake){.value=v};assert(run(&f,&s)==MT76_TIMING_OK && (s.words[2]&255)==v);}
 for(unsigned op=1;op<=14;op++){
  s=(struct mt76_rf_write){0};f=(struct fake){.fail=op};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  unsigned phase=baseline.states[op-1].phase;
  if(phase!=4 && phase!=9){s=(struct mt76_rf_write){0};f=(struct fake){.bad=op};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op);}
 }
 for(unsigned op=1;op<=15;op++){s=(struct mt76_rf_write){0};f=(struct fake){.deny=op};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==op-1);}
 for(unsigned delay=1;delay<32;delay++){
  s=(struct mt76_rf_write){0};f=(struct fake){.delay=delay};assert(run(&f,&s)==MT76_TIMING_OK && s.operations==14+3*delay && s.reads==12+3*delay && s.writes==2);
 }
 for(unsigned i=0;i<3;i++){const unsigned phases[]={3,5,10};s=(struct mt76_rf_write){0};f=(struct fake){.stuck_phase=phases[i]};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==phases[i]+32);}
 s=(struct mt76_rf_write){0};f=(struct fake){.corrupt=true};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==11);
 for(unsigned op=0;op<14;op++){
  s=baseline.states[op];bool w;uint32_t a,v;assert(mt76_rf_write_request(&s,&w,&a,&v));
  s.operations++;assert(!mt76_rf_write_request(&s,&w,&a,&v));s=baseline.states[op];s.reads++;assert(!mt76_rf_write_request(&s,&w,&a,&v));
  s=baseline.states[op];s.writes++;assert(!mt76_rf_write_request(&s,&w,&a,&v));s=baseline.states[op];s.complete=true;assert(!mt76_rf_write_request(&s,&w,&a,&v));
  for(unsigned b=0;b<3;b++)for(unsigned n=0;n<3;n++){
   const unsigned first[]={0,6,11};if(op<=first[b]+n)continue;
   s=baseline.states[op];s.guards[b][n]^=1;assert(!mt76_rf_write_request(&s,&w,&a,&v));
  }
 }
 puts("mt76 RF write: PASS one fixed write/read, all observed bytes, every normal I/O/cancel/guard boundary, poll limits, accounting and no replay");
}
