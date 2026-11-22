#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
struct fake {unsigned calls,auth,fail,deny,bad;uint32_t corrupt;struct mt76_rf_state *s;struct mt76_rf_state states[14];};
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=f->calls++;assert(op<14 && a==mt76_rf_state_request(op));f->states[op]=*f->s;
 unsigned n=op<3?op:op>=11?op-11:3;*v=n==1?0xf000f200:n==2?0x40000050:n==0?0:op;
 if(f->calls==f->bad)*v=f->corrupt;
 return f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_state *s){f->s=s;const struct mt76_mcu_timing_io io={.context=f,.authorize=auth,.read=read_reg};return mt76_rf_state_capture(s,&io);}
int main(void){
 struct mt76_rf_state s={0};struct fake f={0};assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==14 && f.calls==14 && f.auth==15);struct fake base=f;
 assert(run(&f,&s)==MT76_TIMING_REFUSED);assert(mt76_rf_state_request(14)==UINT32_MAX && mt76_rf_state_address(8)==UINT32_MAX);
 for(unsigned op=1;op<=14;op++){
  s=(struct mt76_rf_state){0};f=(struct fake){.fail=op};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op);
  assert(run(&f,&s)==MT76_TIMING_REFUSED);
  s=(struct mt76_rf_state){0};f=(struct fake){.bad=op,.corrupt=UINT32_MAX};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op);
  if(op<=3 || op>=12){s=(struct mt76_rf_state){0};f=(struct fake){.bad=op,.corrupt=1};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op);}
 }
 for(unsigned op=1;op<=15;op++){s=(struct mt76_rf_state){0};f=(struct fake){.deny=op};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==op-1);}
 for(unsigned op=0;op<14;op++){
  s=base.states[op];assert(mt76_rf_state_admit(&s));s.complete=true;assert(!mt76_rf_state_admit(&s));
  for(unsigned prev=0;prev<op;prev++){
   s=base.states[op];if(prev<3)s.guards[0][prev]^=1;else if(prev>=11)s.guards[1][prev-11]^=1;else s.values[prev-3]=UINT32_MAX;
   assert(!mt76_rf_state_admit(&s));
  }
 }
 puts("mt76 RF state: PASS fixed reads, I/O/cancel/guard boundaries, prior-proof mutation and no replay");
}
