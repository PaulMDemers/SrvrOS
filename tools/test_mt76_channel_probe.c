#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad,mutate,byte,bit;int received;struct mt76_channel_probe *s;};
static struct mt76_channel_probe states[116];static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->calls++;if(save)states[op]=*f->s;bool cmd;uint32_t expected;assert(mt76_channel_probe_request(op,&cmd,&expected) && !cmd && a==expected);*v=a==0x2000?0xf000f200:a==0x208?0x40000050:0;if(f->calls==f->bad)*v=UINT32_MAX;return f->calls!=f->fail;}
static uint32_t word(const uint8_t *p){uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)p[b]<<(8*b);return v;}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
 struct fake *f=p;unsigned op=f->calls++;if(save)states[op]=*f->s;unsigned q=(op-11)/7;assert(n==16 && cap==1024 && word(packet)==(0x50a00008u|((q+1)<<16)) && word(packet+4)==mt76_channel_probe_rf_address(q) && !word(packet+8) && !word(packet+12));
 memset(reply,0,cap);uint32_t h=0x4a000008u|((q+1)<<16),w[]={h,mt76_channel_probe_rf_address(q),q,h};for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(w[j]>>(8*b));if(f->calls==f->mutate)reply[f->byte]^=1u<<f->bit;return f->calls==f->fail?-1:f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_channel_probe *s){f->s=s;const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_channel_probe_capture(s,&io);}
int main(void){
 struct fake f={.received=16};struct mt76_channel_probe s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==116 && s.reads==101 && s.exchanges==15 && f.auth==117);save=false;assert(run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=0;op<116;op++){
  bool cmd;uint32_t a;assert(mt76_channel_probe_request(op,&cmd,&a));f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_channel_probe){0};assert(run(&f,&s)==MT76_TIMING_IO && f.calls==op+1 && !s.complete);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(!cmd){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_channel_probe){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==op+1 && !s.complete);}
  else for(unsigned b=0;b<16;b++)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.received=16,.mutate=op+1,.byte=b,.bit=bit};s=(struct mt76_channel_probe){0};assert(run(&f,&s)==(b==8?MT76_TIMING_OK:MT76_TIMING_REPLY));}
  s=states[op];assert(mt76_channel_probe_admit(&s,op));uint8_t packet[16];assert(mt76_channel_probe_packet(&s,op,packet,16)==(cmd?16:0));assert(!mt76_channel_probe_packet(&s,op,packet,15));
  s.reads++;assert(!mt76_channel_probe_admit(&s,op));s=states[op];s.exchanges++;assert(!mt76_channel_probe_admit(&s,op));s=states[op];s.complete=true;assert(!mt76_channel_probe_admit(&s,op));
 }
 for(unsigned n=1;n<=117;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_channel_probe){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_channel_probe){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.calls==15);}
 bool cmd;uint32_t a;assert(!mt76_channel_probe_request(116,&cmd,&a) && mt76_channel_probe_mac_address(5)==UINT32_MAX && mt76_channel_probe_rf_address(15)==UINT32_MAX);
 puts("channel prerequisites: PASS116 fixed operations,all I/O/cancel/guard/all-ones/reply-bit/length boundaries, packet/state checks and no replay");
}
