#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_rf_csr *s;unsigned auth,ops,fail,deny,bad,delay;bool stuck;unsigned mutated_bit;bool mutate;struct mt76_rf_csr states[355];};
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t address,uint32_t *v){
 struct fake *f=p;struct mt76_rf_csr *s=f->s;assert(op==f->ops && op<355);f->states[op]=*s;f->ops++;
 unsigned phase=s->phase,i=s->index;const unsigned regs[]={6,1,2,6,22},values[]={0,1,17,65,0};
 assert(write==(phase==4));
 if(phase<3 || phase>=6){unsigned n=phase<3?phase:phase-6;assert(address==(n==0?0x1004:n==1?0x2000:0x208));*v=n==1?0xf000f200:n==2?0x40000050:0;}
 else{assert(address==0x500);if(write)assert(*v==(0x80000000u|(regs[i]<<8)) && !(*v&0x400000ff));
 else if(f->stuck || s->polls[i][phase==5]<f->delay)*v=0x80000600;
 else *v=phase==3?0x600:(regs[i]<<8)|values[i];}
 if(f->ops==f->bad)*v=UINT32_MAX;
 if(f->mutate && phase==5 && i==1)*v^=1u<<f->mutated_bit;
 return f->ops!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_csr *s){f->s=s;const struct mt76_rf_csr_io io={f,auth,transfer};return mt76_rf_csr_apply(s,&io);}
int main(void){
 struct mt76_rf_csr s={0};static struct fake f,baseline;f=(struct fake){0};
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==45 && s.reads==40 && s.writes==5 && s.index==5 && !s.phase && f.ops==45 && f.auth==46);baseline=f;
 assert(s.result[0]==0x600 && s.result[3]==0x641 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=1;op<=45;op++){
  s=(struct mt76_rf_csr){0};f=(struct fake){.fail=op};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(baseline.states[op-1].phase!=4){s=(struct mt76_rf_csr){0};f=(struct fake){.bad=op};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op);}
 }
 for(unsigned op=1;op<=46;op++){s=(struct mt76_rf_csr){0};f=(struct fake){.deny=op};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==op-1);}
 for(unsigned delay=1;delay<32;delay++){
  s=(struct mt76_rf_csr){0};f=(struct fake){.delay=delay};assert(run(&f,&s)==MT76_TIMING_OK && s.operations==45+10*delay && s.reads==40+10*delay && s.writes==5);
 }
 s=(struct mt76_rf_csr){0};f=(struct fake){.stuck=true};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==35 && !s.writes);
 for(unsigned bit=0;bit<32;bit++)if(bit<18 || bit>=30){
  s=(struct mt76_rf_csr){0};f=(struct fake){.mutate=true,.mutated_bit=bit};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete);
 }
 for(unsigned op=0;op<45;op++){
  s=baseline.states[op];bool w;uint32_t a,v;assert(mt76_rf_csr_request(&s,&w,&a,&v));
  s.operations++;assert(!mt76_rf_csr_request(&s,&w,&a,&v));s=baseline.states[op];s.reads++;assert(!mt76_rf_csr_request(&s,&w,&a,&v));
  s=baseline.states[op];s.writes++;assert(!mt76_rf_csr_request(&s,&w,&a,&v));s=baseline.states[op];s.complete=true;assert(!mt76_rf_csr_request(&s,&w,&a,&v));
  for(unsigned i=0;i<baseline.states[op].index;i++){
   s=baseline.states[op];s.result[i]^=0x100;assert(!mt76_rf_csr_request(&s,&w,&a,&v));
   for(unsigned b=0;b<2;b++)for(unsigned n=0;n<3;n++){s=baseline.states[op];s.guards[2*i+b][n]^=1;assert(!mt76_rf_csr_request(&s,&w,&a,&v));}
  }
 }
 assert(mt76_rf_csr_selector(5)==UINT32_MAX);
 puts("mt76 RF CSR: PASS fixed read selectors, every I/O/cancel boundary, identity/control faults, bounded busy polls and no replay");
}
