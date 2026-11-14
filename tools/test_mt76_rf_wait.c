#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned op,auth,waits,fail,deny,bad,mutate,byte,bit,delay_fail;int received;uint8_t values[6];struct mt76_rf_wait *s;};
static struct mt76_rf_wait states[42],before[6];static bool save_states;
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool delay(void *p,uint32_t us){struct fake *f=p;unsigned q=f->waits++;static const unsigned expected[]={10000,40000,50000,100000,300000,500000};assert(q<6 && us==expected[q] && f->op==q*7);if(save_states)before[q]=*f->s;return f->waits!=f->delay_fail;}
static void save(struct fake *f){if(save_states)states[f->op]=*f->s;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;save(f);unsigned op=f->op++,n=op%7;assert(n!=3 && a==mt76_rf_wait_guard_address(op));n=n<3?n:n-4;*v=n==1?0xf000f200:n==2?0x40000050:0;if(f->op==f->bad)*v^=1;return f->op!=f->fail;}
static uint32_t word(const uint8_t *p){uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)p[b]<<(8*b);return v;}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
 struct fake *f=p;save(f);unsigned op=f->op++,q=op/7,seq=(428+q)%15+1;
 assert(op%7==3 && n==16 && cap==1024 && word(packet)==(8u|(seq<<16)|(10u<<20)|0x50000000));
 assert(word(packet+4)==0x80000004u && !word(packet+8) && !word(packet+12));if(f->op==f->fail)return -1;
 uint32_t w[]={0x4a000008u|(seq<<16),0x80000004u,f->values[q],0x4a000008u|(seq<<16)};memset(reply,0,cap);
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(w[j]>>(8*b));
 if(f->op==f->mutate)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_wait *s){f->s=s;const struct mt76_rf_wait_io io={{f,authorize,read_reg,exchange},delay};return mt76_rf_wait_capture(s,&io);}
int main(void){
 struct mt76_rf_wait s={0};struct fake f={.received=16,.values={176,176,48,176,48,48}};save_states=true;
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==42 && s.reads==36 && s.exchanges==6 && s.waits==6 && s.delay_us==1000000 && s.first_clear==3);save_states=false;
 assert(f.auth==49 && f.op==42 && run(&f,&s)==MT76_TIMING_REFUSED);
 assert(mt76_rf_wait_guard_address(42)==UINT32_MAX && !mt76_rf_wait_sequence(6) && !mt76_rf_wait_delay(6));
 for(unsigned pattern=0;pattern<64;pattern++){
  f=(struct fake){.received=16};unsigned first=0;
  for(unsigned q=0;q<6;q++){f.values[q]=(pattern&(1u<<q))?48:176;if(!first && f.values[q]==48)first=q+1;}
  s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_OK && s.first_clear==first);
 }
 for(unsigned op=0;op<42;op++){
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.op==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%7!=3){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.op==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=(struct fake){.received=16,.mutate=op+1,.byte=byte,.bit=bit};s=(struct mt76_rf_wait){0};enum mt76_mcu_timing_status r=run(&f,&s);
   if(byte==8)assert(r==MT76_TIMING_OK);else assert(r==MT76_TIMING_REPLY && f.op==op+1);
  }
  s=states[op];assert(mt76_rf_wait_admit(&s,op));uint8_t packet[16];assert(mt76_rf_wait_packet(&s,op,packet,16)==(op%7==3?16:0));assert(!mt76_rf_wait_packet(&s,op,packet,15));
  s.waits++;assert(!mt76_rf_wait_admit(&s,op));s=states[op];s.delay_us++;assert(!mt76_rf_wait_admit(&s,op));s=states[op];s.first_clear^=7;assert(!mt76_rf_wait_admit(&s,op));
  s=states[op];s.complete=true;assert(!mt76_rf_wait_admit(&s,op));s=states[op];s.reads++;assert(!mt76_rf_wait_admit(&s,op));
  for(unsigned q=0;q<states[op].exchanges;q++)for(unsigned j=0;j<4;j++)if(j!=2){s=states[op];s.words[q][j]^=1;assert(!mt76_rf_wait_admit(&s,op));}
  for(unsigned q=0;q<op/7;q++)for(unsigned edge=0;edge<2;edge++)for(unsigned j=0;j<3;j++){s=states[op];s.guards[2*q+edge][j]^=1;assert(!mt76_rf_wait_admit(&s,op));}
 }
 for(unsigned n=1;n<=49;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);}
 for(unsigned q=0;q<6;q++){
  f=(struct fake){.received=16,.delay_fail=q+1};s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_IO && f.op==q*7 && !s.complete && s.waits==q);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  s=before[q];assert(mt76_rf_wait_delay_admit(&s));s.waits++;assert(!mt76_rf_wait_delay_admit(&s));s=before[q];s.delay_us++;assert(!mt76_rf_wait_delay_admit(&s));
 }
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_REPLY && f.op==4);}
 f=(struct fake){.received=16,.values={48,48,48,48,48,48}};s=(struct mt76_rf_wait){0};assert(run(&f,&s)==MT76_TIMING_OK && mt76_rf_wait_clear_profile_valid(&s));
 struct mt76_rf_wait good=s;
 for(unsigned q=0;q<6;q++)for(unsigned j=0;j<4;j++)for(unsigned b=0;b<32;b++){s=good;s.words[q][j]^=1u<<b;assert(!mt76_rf_wait_clear_profile_valid(&s));}
 for(unsigned q=0;q<12;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_rf_wait_clear_profile_valid(&s));}
 s=good;s.complete=false;assert(!mt76_rf_wait_clear_profile_valid(&s));s=good;s.waits--;assert(!mt76_rf_wait_clear_profile_valid(&s));
 s=good;s.delay_us++;assert(!mt76_rf_wait_clear_profile_valid(&s));s=good;s.first_clear++;assert(!mt76_rf_wait_clear_profile_valid(&s));assert(!mt76_rf_wait_clear_profile_valid(NULL));
 puts("RF delayed VCO: PASS six fixed delays, all64 clear/set patterns, every I/O/cancel/delay/guard/reply boundary, proof mutation and no replay");
}
