#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned op,auth,fail,deny,bad,mutate,byte,bit;int received;uint8_t dac,vco;bool clear;struct mt76_rf_cal *s;};
static struct mt76_rf_cal states[70];static bool save_states;
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static void save(struct fake *f){if(save_states)states[f->op]=*f->s;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;save(f);unsigned op=f->op++,n=op%7;assert(n!=3 && a==mt76_rf_cal_guard_address(op));
 n=n<3?n:n-4;*v=n==1?0xf000f200:n==2?0x40000050:0;
 if(f->op==f->bad)*v^=1;
 return f->op!=f->fail;
}
static uint32_t word(const uint8_t *p){uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)p[b]<<(8*b);return v;}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
 struct fake *f=p;save(f);unsigned op=f->op++,q=op/7,seq=(418+q)%15+1;bool wr=q<8 && (q&1);
 assert(op%7==3 && n==16 && cap==1024);
 assert(word(packet)==(8u|(seq<<16)|((wr?12u:10u)<<20)|0x50000000));
 assert(word(packet+4)==(0x80000000u|((q<6 || q==8)?73:4)) && !word(packet+12));
 uint8_t *v=(q<6 || q==8)?&f->dac:&f->vco;
 if(wr){unsigned expected=q==3?*v&0x7f:*v|0x80;assert(word(packet+8)==expected);*v=(uint8_t)expected;}
 else assert(!word(packet+8));
 if(f->op==f->fail)return -1;
 uint32_t value=*v;if(q==9 && f->clear)value&=0x7f;
 uint32_t w[]={0x4a000008u|(seq<<16),word(packet+4),value,0x4a000008u|(seq<<16)};
 memset(reply,0,cap);for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(w[j]>>(8*b));
 if(f->op==f->mutate){reply[f->byte]^=1u<<f->bit;if(!wr && f->byte==8)*v^=1u<<f->bit;}
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_cal *s){f->s=s;const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};return mt76_rf_cal_capture(s,&io);}
int main(void){
 struct mt76_rf_cal s={0};struct fake f={.received=16,.dac=0x93,.vco=0x30,.clear=true};save_states=true;
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==70 && s.reads==60 && s.exchanges==10 && s.acks==4 && s.observed==6);
 assert(s.words[1][2]==0x93 && s.words[3][2]==0x13 && s.words[5][2]==0x93 && s.words[7][2]==0xb0 && s.words[8][2]==0x93 && s.words[9][2]==0x30);save_states=false;
 assert(f.op==70 && f.auth==71 && run(&f,&s)==MT76_TIMING_REFUSED);
 assert(!mt76_rf_cal_sequence(10) && mt76_rf_cal_address(10)==UINT32_MAX && mt76_rf_cal_guard_address(70)==UINT32_MAX);
 for(unsigned v=0;v<256;v++)for(unsigned clear=0;clear<2;clear++){
  f=(struct fake){.received=16,.dac=(uint8_t)v,.vco=(uint8_t)(255-v),.clear=clear};s=(struct mt76_rf_cal){0};
  assert(run(&f,&s)==MT76_TIMING_OK && s.words[8][2]==(v|0x80) && s.words[9][2]==(clear?((255-v)&0x7f):((255-v)|0x80)));
 }
 for(unsigned op=0;op<70;op++){
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_rf_cal){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.op==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%7!=3){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_rf_cal){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.op==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=(struct fake){.received=16,.mutate=op+1,.byte=byte,.bit=bit};s=(struct mt76_rf_cal){0};enum mt76_mcu_timing_status r=run(&f,&s);
   if(byte==8 && !mt76_rf_cal_write(op/7))assert(r==MT76_TIMING_OK);else assert(r==MT76_TIMING_REPLY && f.op==op+1);
  }
  s=states[op];assert(mt76_rf_cal_admit(&s,op));uint8_t packet[16];assert(mt76_rf_cal_packet(&s,op,packet,16)==(op%7==3?16:0));assert(!mt76_rf_cal_packet(&s,op,packet,15));
  s.reads++;assert(!mt76_rf_cal_admit(&s,op));s=states[op];s.exchanges++;assert(!mt76_rf_cal_admit(&s,op));s=states[op];s.acks++;assert(!mt76_rf_cal_admit(&s,op));
  s=states[op];s.observed++;assert(!mt76_rf_cal_admit(&s,op));s=states[op];s.complete=true;assert(!mt76_rf_cal_admit(&s,op));
  for(unsigned i=0;i<states[op].exchanges;i++)for(unsigned j=0;j<4;j++)if(j!=2 || mt76_rf_cal_write(i)){
   s=states[op];s.words[i][j]^=1;assert(!mt76_rf_cal_admit(&s,op));
  }
  for(unsigned q=0;q<op/7;q++)for(unsigned edge=0;edge<2;edge++)for(unsigned j=0;j<3;j++){
   s=states[op];s.guards[2*q+edge][j]^=1;assert(!mt76_rf_cal_admit(&s,op));
  }
 }
 for(unsigned n=1;n<=71;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_rf_cal){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.op==n-1);}
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_rf_cal){0};assert(run(&f,&s)==MT76_TIMING_REPLY && f.op==4);}
 f=(struct fake){.received=16,.dac=147,.vco=48,.clear=false};s=(struct mt76_rf_cal){0};assert(run(&f,&s)==MT76_TIMING_OK && mt76_rf_cal_pending_profile_valid(&s));
 struct mt76_rf_cal good=s;
 for(unsigned q=0;q<10;q++)for(unsigned j=0;j<4;j++)for(unsigned b=0;b<32;b++){s=good;s.words[q][j]^=1u<<b;assert(!mt76_rf_cal_pending_profile_valid(&s));}
 for(unsigned q=0;q<20;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_rf_cal_pending_profile_valid(&s));}
 s=good;s.complete=false;assert(!mt76_rf_cal_pending_profile_valid(&s));s=good;s.operations--;assert(!mt76_rf_cal_pending_profile_valid(&s));assert(!mt76_rf_cal_pending_profile_valid(NULL));
 f=(struct fake){.received=16,.dac=147,.vco=48,.clear=true};s=(struct mt76_rf_cal){0};assert(run(&f,&s)==MT76_TIMING_OK && mt76_rf_cal_cleared_profile_valid(&s) && !mt76_rf_cal_pending_profile_valid(&s));
 good=s;
 for(unsigned q=0;q<10;q++)for(unsigned j=0;j<4;j++)for(unsigned b=0;b<32;b++){s=good;s.words[q][j]^=1u<<b;assert(!mt76_rf_cal_cleared_profile_valid(&s));}
 for(unsigned q=0;q<20;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_rf_cal_cleared_profile_valid(&s));}
 for(unsigned v=0;v<256;v++){s=good;s.words[9][2]=v;assert(mt76_rf_cal_cleared_profile_valid(&s)==(v==48));assert(mt76_rf_cal_pending_profile_valid(&s)==(v==176));}
 s=good;s.complete=false;assert(!mt76_rf_cal_cleared_profile_valid(&s));s=good;s.operations--;assert(!mt76_rf_cal_cleared_profile_valid(&s));assert(!mt76_rf_cal_cleared_profile_valid(NULL));
 puts("RF DAC/VCO: PASS preserved RMW bits, clear/still-set observation, every I/O/cancel/guard/reply boundary, mutated proof and no replay");
}
