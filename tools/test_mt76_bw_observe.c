#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad,mutate,bit;int received;struct mt76_bw_observe *s;};
static struct mt76_bw_observe states[7];static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->calls++;if(save)states[op]=*f->s;assert(a==mt76_bw_observe_guard_address(op));unsigned j=op<3?op:op-4;*v=j==1?0xf000f200:j==2?0x40000050:0;if(f->calls==f->bad)*v^=1;return f->calls!=f->fail;}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
 struct fake *f=p;if(save)states[3]=*f->s;assert(f->calls++==3 && n==16 && cap==1024);uint8_t expected[16]={8,0,31,80,2,0,0,0,0,0,0,0,0,0,0,0};assert(!memcmp(packet,expected,16));memset(reply,0,cap);
 uint32_t words[]={0x4a0f0008,0x12345678,0xffffffff,0xabcdef01};for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(words[j]>>(8*b));if(f->mutate)reply[f->mutate-1]^=1u<<f->bit;
 return f->calls==f->fail?-1:f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_bw_observe *s){f->s=s;const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_bw_observe_capture(s,&io);}
int main(void){
 struct fake f={.received=16};struct mt76_bw_observe s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==7 && s.reads==6 && s.exchanges==1 && f.auth==8 && s.words[2]==0xffffffff);save=false;assert(run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=0;op<7;op++){
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_bw_observe){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op!=3){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_bw_observe){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op+1);}
  s=states[op];assert(mt76_bw_observe_admit(&s,op));s.operations++;assert(!mt76_bw_observe_admit(&s,op));s=states[op];s.reads++;assert(!mt76_bw_observe_admit(&s,op));s=states[op];s.exchanges++;assert(!mt76_bw_observe_admit(&s,op));s=states[op];s.complete=true;assert(!mt76_bw_observe_admit(&s,op));
  for(unsigned q=0;q<op;q++)if(q!=3){s=states[op];s.guards[q>3][q<3?q:q-4]^=1;assert(!mt76_bw_observe_admit(&s,op));}
 }
 for(unsigned n=1;n<=8;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_bw_observe){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);}
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_bw_observe){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.calls==4);}
 for(unsigned b=0;b<16;b++)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.received=16,.mutate=b+1,.bit=bit};s=(struct mt76_bw_observe){0};assert(run(&f,&s)==(b<4?MT76_TIMING_REPLY:MT76_TIMING_OK));}
 uint8_t packet[16];assert(!mt76_bw_observe_packet(packet,15) && mt76_bw_observe_packet(packet,16)==16 && mt76_bw_observe_guard_address(7)==UINT32_MAX);
 puts("bandwidth response observation: PASS fixed packet, framing vs opaque payload, all lengths/I/O/cancel/guards/state faults and no replay");
}
