#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
struct fake {unsigned calls,auth,fail,deny,bad;struct mt76_channel_state *s;};
static struct mt76_channel_state states[18];static bool save_states;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static const uint32_t targets[]={0x2a4,0x1340,0x2004,0x2300,0x2700,0x132c,0x141c,0x1330,0x2124,0x13b0,0x1114,0x238};
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=f->calls;if(save_states)states[op]=*f->s;
 assert(a==(op<3?(op==0?0x1004:op==1?0x2000:0x208):op>=15?(op==15?0x1004:op==16?0x2000:0x208):targets[op-3]));
 if(op<3 || op>=15){unsigned n=op<3?op:op-15;*v=n==1?0xf000f200:n==2?0x40000050:0;}
 else *v=(op-3)*0x111111;
 if(op+1==f->bad)*v=UINT32_MAX;
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_channel_state *s){f->s=s;const struct mt76_mcu_timing_io io={.context=f,.authorize=auth,.read=read_reg};return mt76_channel_state_capture(s,&io);}
int main(void){
 struct mt76_channel_state s={0};struct fake f={0};save_states=true;assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==18);save_states=false;
 assert(f.calls==18 && f.auth==19 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned i=0;i<12;i++)assert(mt76_channel_state_address(i)==targets[i]);
 assert(mt76_channel_state_address(12)==UINT32_MAX && mt76_channel_state_request(18)==UINT32_MAX);
 for(unsigned op=0;op<18;op++){
  f=(struct fake){.fail=op+1};s=(struct mt76_channel_state){0};assert(run(&f,&s)==MT76_TIMING_IO && f.calls==op+1 && !s.complete);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  f=(struct fake){.bad=op+1};s=(struct mt76_channel_state){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==op+1 && !s.complete);
  s=states[op];assert(mt76_channel_state_admit(&s));s.complete=true;assert(!mt76_channel_state_admit(&s));
  for(unsigned q=0;q<op;q++){s=states[op];if(q<3 || q>=15)s.guards[q>=15][q<3?q:q-15]^=1;else s.values[q-3]=UINT32_MAX;assert(!mt76_channel_state_admit(&s));}
 }
 for(unsigned n=1;n<=19;n++){f=(struct fake){.deny=n};s=(struct mt76_channel_state){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 assert(!mt76_channel_state_admit(NULL));puts("channel prerequisite inventory: PASS fixed12 targets,18 read-only operations, every transfer/cancel/all-ones/guard boundary, proof mutation and no replay");
}
