#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned op,auth,waits,fail,deny,bad,mutate,byte,bit,delay_fail,auth_mutate;int received;uint8_t values[9];struct mt76_post_vco *s;};
static struct mt76_post_vco states[63],before[6];static bool save_states;
static bool authorize(void *p){struct fake *f=p;++f->auth;if(f->auth_mutate==f->auth)f->s->reads++;return f->auth!=f->deny;}
static bool delay(void *p,uint32_t us){struct fake *f=p;unsigned q=f->waits++;static const unsigned expected[]={10000,40000,50000,100000,300000,500000};assert(q<6 && us==expected[q] && f->op==(q+3)*7);if(save_states)before[q]=*f->s;return f->waits!=f->delay_fail;}
static void save(struct fake *f){if(save_states)states[f->op]=*f->s;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;save(f);unsigned op=f->op++,n=op%7;assert(n!=3 && a==mt76_post_vco_guard_address(op));n=n<3?n:n-4;*v=n==1?0xf000f200:n==2?0x40000050:0;if(f->op==f->bad)*v^=1;return f->op!=f->fail;}
static uint32_t word(const uint8_t *p){uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)p[b]<<(8*b);return v;}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
 struct fake *f=p;save(f);unsigned op=f->op++,q=op/7,seq=(1+q)%15+1;
 assert(op%7==3 && n==16 && cap==1024 && word(packet)==(8u|(seq<<16)|((q==1?12u:10u)<<20)|0x50000000));
 assert(word(packet+4)==0x80000004u && word(packet+8)==(q==1?0xb0u:0) && !word(packet+12));if(f->op==f->fail)return -1;
 uint32_t w[]={0x4a000008u|(seq<<16),0x80000004u,f->values[q],0x4a000008u|(seq<<16)};memset(reply,0,cap);
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(w[j]>>(8*b));
 if(f->op==f->mutate)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_post_vco *s){f->s=s;const struct mt76_rf_wait_io io={{f,authorize,read_reg,exchange},delay};return mt76_post_vco_capture(s,&io);}
static struct fake fixture(void){return (struct fake){.received=16,.values={48,176,48,176,176,48,176,48,48}};}
int main(void){
 struct mt76_post_vco s={0};struct fake f=fixture();save_states=true;
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==63 && s.reads==54 && s.exchanges==9 && s.acks==1 && s.observed==8 && s.waits==6 && s.delay_us==1000000 && s.first_clear==3);save_states=false;
 assert(f.auth==70 && f.op==63 && run(&f,&s)==MT76_TIMING_REFUSED);
 assert(mt76_post_vco_guard_address(63)==UINT32_MAX && !mt76_post_vco_sequence(9) && !mt76_post_vco_delay(6));
 for(unsigned pattern=0;pattern<64;pattern++){
  f=fixture();unsigned first=0;
  for(unsigned q=0;q<6;q++){f.values[q+3]=(pattern&(1u<<q))?48:176;if(!first && f.values[q+3]==48)first=q+1;}
  s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_OK && s.first_clear==first);
 }
 for(unsigned op=0;op<63;op++){
  f=fixture();f.fail=op+1;s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.op==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%7!=3){f=fixture();f.bad=op+1;s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.op==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=fixture();f.mutate=op+1;f.byte=byte;f.bit=bit;s=(struct mt76_post_vco){0};enum mt76_mcu_timing_status r=run(&f,&s);
   if(byte==8 && op/7>=2)assert(r==MT76_TIMING_OK);else assert(r==MT76_TIMING_REPLY && f.op==op+1);
  }
  s=states[op];assert(mt76_post_vco_admit(&s,op));uint8_t packet[16];assert(mt76_post_vco_packet(&s,op,packet,16)==(op%7==3?16:0));assert(!mt76_post_vco_packet(&s,op,packet,15));
  s.waits++;assert(!mt76_post_vco_admit(&s,op));s=states[op];s.delay_us++;assert(!mt76_post_vco_admit(&s,op));s=states[op];s.first_clear^=7;assert(!mt76_post_vco_admit(&s,op));
  s=states[op];s.complete=true;assert(!mt76_post_vco_admit(&s,op));s=states[op];s.reads++;assert(!mt76_post_vco_admit(&s,op));
  for(unsigned q=0;q<states[op].exchanges;q++)for(unsigned j=0;j<4;j++)if(j!=2){s=states[op];s.words[q][j]^=1;assert(!mt76_post_vco_admit(&s,op));}
  for(unsigned q=0;q<op/7;q++)for(unsigned edge=0;edge<2;edge++)for(unsigned j=0;j<3;j++){s=states[op];s.guards[2*q+edge][j]^=1;assert(!mt76_post_vco_admit(&s,op));}
 }
 for(unsigned n=1;n<=70;n++){f=fixture();f.deny=n;s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);f=fixture();f.auth_mutate=n;s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete);}
 for(unsigned q=0;q<6;q++){
  f=fixture();f.delay_fail=q+1;s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_IO && f.op==(q+3)*7 && !s.complete && s.waits==q);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  s=before[q];assert(mt76_post_vco_delay_admit(&s));s.waits++;assert(!mt76_post_vco_delay_admit(&s));s=before[q];s.delay_us++;assert(!mt76_post_vco_delay_admit(&s));
 }
 for(int n=0;n<=1025;n++)if(n!=16){f=fixture();f.received=n;s=(struct mt76_post_vco){0};assert(run(&f,&s)==MT76_TIMING_REPLY && f.op==4);}
 puts("post VCO: PASS fixed channel1 read/set/read and six delays,all clear patterns,all transfer/auth/guard/packet/reply boundaries,no replay");
}
