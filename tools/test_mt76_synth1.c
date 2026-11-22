#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad,mutate,byte,bit;int received;uint8_t regs[256];struct mt76_synth1 *s;};
static struct mt76_synth1 states[441];static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static void snapshot(struct fake *f){if(save)states[f->calls]=*f->s;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;snapshot(f);unsigned op=f->calls++,n=op%7;assert(n!=3 && a==mt76_synth1_guard_address(op));unsigned j=n<3?n:n-4;*v=j==1?0xf000f200:j==2?0x40000050:0;if(f->bad==op+1)*v^=1;return f->fail!=op+1;}
static int exchange(void *p,const uint8_t *packet,size_t len,uint8_t *reply,size_t cap){
 struct fake *f=p;snapshot(f);unsigned op=f->calls++,q=op/7,i=q/3;assert(op%7==3 && len==16 && cap==1024);uint32_t h=0,a=0,v=0;
 for(unsigned j=0;j<4;j++){h|=(uint32_t)packet[j]<<(8*j);a|=(uint32_t)packet[4+j]<<(8*j);v|=(uint32_t)packet[8+j]<<(8*j);assert(packet[12+j]==0);}
 unsigned seq=(q+5)%15+1;assert(h==(8u|(seq<<16)|((q%3==1?12u:10u)<<20)|0x50000000));assert(a==mt76_synth1_address(i));
 unsigned reg=a&255;assert(v==(q%3==1?mt76_synth1_value(i,f->regs[reg]):0));if(f->fail==op+1)return -1;
 if(q%3==1)f->regs[reg]=(uint8_t)v;else v=f->regs[reg];
 uint32_t words[]={0x4a000008u|(seq<<16),a,v,0x4a000008u|(seq<<16)};memset(reply,0,cap);
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(words[j]>>(8*b));
 if(f->mutate==op+1)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_synth1 *s){f->s=s;f->regs[24]=0x33;const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_synth1_capture(s,&io);}
int main(void){
 struct fake f={.received=16};struct mt76_synth1 s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_synth1_exact_profile_valid(&s));save=false;
 assert(f.calls==441 && f.auth==442 && s.reads==378 && s.exchanges==63 && s.acks==21 && s.observed==42 && run(&f,&s)==MT76_TIMING_REFUSED);struct mt76_synth1 good=s;
 for(unsigned op=0;op<441;op++){
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_synth1){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%7!=3){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_synth1){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op+1);}
  else for(unsigned b=0;b<16;b++)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.received=16,.mutate=op+1,.byte=b,.bit=bit};s=(struct mt76_synth1){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.calls==op+1);}
  s=states[op];uint8_t packet[16];assert(mt76_synth1_admit(&s,op) && mt76_synth1_packet(&s,op,packet,16)==(op%7==3?16:0));assert(!mt76_synth1_packet(&s,op,packet,15));s.reads++;assert(!mt76_synth1_admit(&s,op));s=states[op];s.exchanges++;assert(!mt76_synth1_admit(&s,op));s=states[op];s.acks++;assert(!mt76_synth1_admit(&s,op));s=states[op];s.observed++;assert(!mt76_synth1_admit(&s,op));s=states[op];s.complete=true;assert(!mt76_synth1_admit(&s,op));
 }
 for(unsigned n=1;n<=442;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_synth1){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_synth1){0};assert(run(&f,&s)==MT76_TIMING_REPLY && f.calls==4 && !s.complete);}
 for(unsigned q=0;q<63;q++)for(unsigned j=0;j<4;j++)for(unsigned bit=0;bit<32;bit++){s=good;s.words[q][j]^=1u<<bit;assert(!mt76_synth1_exact_profile_valid(&s));}
 for(unsigned q=0;q<126;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_synth1_exact_profile_valid(&s));}
 for(unsigned i=0;i<21;i++)for(unsigned v=0;v<256;v++)assert(mt76_synth1_value(i,v)==((v&~mt76_synth1_mask(i))|mt76_synth1_bits(i)));
 assert(mt76_synth1_address(21)==UINT32_MAX && mt76_synth1_sequence(63)==0 && mt76_synth1_value(21,0)==UINT32_MAX && mt76_synth1_value(0,256)==UINT32_MAX && !mt76_synth1_exact_profile_valid(NULL));
 puts("synth1: PASS441 ordered operations,5376 masked words, every I/O/cancel/guard/reply-bit/length boundary, full-proof mutations and no replay");
}
