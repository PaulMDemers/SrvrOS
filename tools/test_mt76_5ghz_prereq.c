#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint32_t targets[7]={0x13c8,0x13a0,0x518,0x13a8,0x13b4,0x80050000,0x80060000};
static struct mt76_5ghz_prereq states[49];static bool save;
struct fake {struct mt76_5ghz_prereq *s;unsigned ops,auth,fail,deny,bad,bit,byte,mutate;int received;};
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static void snapshot(struct fake *f){assert(f->ops<49);if(save)states[f->ops]=*f->s;}
static bool read_value(void *p,uint32_t address,uint32_t *v){
 struct fake *f=p;snapshot(f);unsigned op=f->ops++,i=op/7,n=op%7;
 assert(!mt76_5ghz_prereq_rf(op));uint32_t a=n==3?targets[i]:n==0 || n==4?0x1004:n==1 || n==5?0x2000:0x208;
 assert(address==a);*v=n==3?(i+1)*17:n==1 || n==5?0xf000f200:n==2 || n==6?0x40000050:0;
 if(f->bad==op+1)*v=UINT32_MAX;
 return f->ops!=f->fail;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct fake *f=p;snapshot(f);unsigned op=f->ops++,i=op/7;assert((op==38 || op==45) && length==16 && capacity==1024);
 uint32_t words[4]={0};for(unsigned n=0;n<4;n++)for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)packet[4*n+b]<<(8*b);
 assert(words[0]==(0x50a00008u|((i-4)<<16)) && words[1]==targets[i] && words[2]==0 && words[3]==0);
 if(f->ops==f->fail)return -1;
 words[0]=words[3]=0x4a000008u|((i-4)<<16);words[2]=i==5?0x44:0x45;
 memset(reply,0,capacity);for(unsigned n=0;n<4;n++)for(unsigned b=0;b<4;b++)reply[4*n+b]=(uint8_t)(words[n]>>(8*b));
 if(f->mutate==op+1)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_5ghz_prereq *s){f->s=s;const struct mt76_mcu_timing_io io={f,authorize,read_value,exchange};return mt76_5ghz_prereq_capture(s,&io);}
int main(void){
 for(unsigned i=0;i<7;i++)assert(mt76_5ghz_prereq_target(i)==targets[i]);
 assert(mt76_5ghz_prereq_target(7)==UINT32_MAX && mt76_5ghz_prereq_address(49)==UINT32_MAX);
 struct mt76_5ghz_prereq s={0};struct fake f={.received=16};save=true;
 assert(run(&f,&s)==MT76_TIMING_OK && mt76_5ghz_prereq_valid(&s));save=false;struct mt76_5ghz_prereq good=s;
 assert(f.ops==49 && f.auth==50 && s.reads==47 && s.exchanges==2 && s.observed==7 && s.values[5]==0x44 && s.values[6]==0x45);
 assert(run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=0;op<49;op++){
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(!mt76_5ghz_prereq_rf(op)){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op+1);}
  uint8_t packet[16];assert(mt76_5ghz_prereq_packet(op,packet,16)==(mt76_5ghz_prereq_rf(op)?16u:0u));assert(!mt76_5ghz_prereq_packet(op,packet,15));
  s=states[op];assert(mt76_5ghz_prereq_admit(&s,op));s.reads++;assert(!mt76_5ghz_prereq_admit(&s,op));s=states[op];s.exchanges++;assert(!mt76_5ghz_prereq_admit(&s,op));s=states[op];s.observed++;assert(!mt76_5ghz_prereq_admit(&s,op));
 }
 for(unsigned n=1;n<=50;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==n-1);}
 for(unsigned op=38;op<=45;op+=7)for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
  f=(struct fake){.received=16,.mutate=op+1,.byte=byte,.bit=bit};s=(struct mt76_5ghz_prereq){0};enum mt76_mcu_timing_status r=run(&f,&s);
  if(byte==8)assert(r==MT76_TIMING_OK && mt76_5ghz_prereq_valid(&s));else assert(r==MT76_TIMING_REPLY && !s.complete && f.ops==op+1);
 }
 for(int length=0;length<=1025;length++)if(length!=16){f=(struct fake){.received=length};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.ops==39);}
 for(unsigned g=0;g<14;g++)for(unsigned n=0;n<3;n++)for(unsigned bit=0;bit<32;bit++){
  s=good;s.guards[g][n]^=1u<<bit;assert(mt76_5ghz_prereq_valid(&s)==(n==2 && bit>=4));
 }
 for(unsigned q=0;q<2;q++)for(unsigned n=0;n<4;n++)for(unsigned bit=0;bit<32;bit++){s=good;s.words[q][n]^=1u<<bit;assert(!mt76_5ghz_prereq_valid(&s));}
 for(unsigned i=0;i<7;i++){s=good;s.values[i]=UINT32_MAX;assert(!mt76_5ghz_prereq_valid(&s));}
 s=good;s.complete=false;assert(!mt76_5ghz_prereq_valid(&s));s=good;s.operations--;assert(!mt76_5ghz_prereq_valid(&s));assert(!mt76_5ghz_prereq_valid(NULL));
 puts("5GHz prerequisite inventory: PASS fixed read-only targets/packets,49 failure and50 cancellation boundaries, reply bits/lengths, guard/proof mutations and no replay");
}
