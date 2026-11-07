#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad;uint32_t values[1];struct mt76_usb_cycle *s;};
static struct mt76_usb_cycle states[9];static bool save_states;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
 struct fake *f=p;assert(f->calls==op && length==4);if(save_states)states[op]=*f->s;
 unsigned i=op/9,n=op%9;assert(write==(n==4));uint32_t v=0;
 uint32_t expected=n<3?(n==0?0x1004:n==1?0x2000:0x208):n>=6?(n==6?0x1004:n==7?0x2000:0x208):0x2a4;assert(a==expected);
 if(write){for(unsigned b=0;b<4;b++)v|=(uint32_t)data[b]<<(8*b);assert(v==((f->values[i]&~255u)|30u));f->values[i]=v;}
 else {v=n<3?(n==1?0xf000f200:n==2?0x40000050:0):n>=6?(n==7?0xf000f200:n==8?0x40000050:0):f->values[i];if(op+1==f->bad)v^=1;for(unsigned b=0;b<4;b++)data[b]=(uint8_t)(v>>(8*b));}
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_usb_cycle *s){f->s=s;const struct mt76_usb_cycle_io io={f,auth,transfer};return mt76_usb_cycle_apply(s,&io);}
int main(void){
 struct mt76_usb_cycle s={0};struct fake f={.values={0xabcd1200}};save_states=true;
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==9 && s.reads==8 && s.writes==1 && s.verified==1 && s.after[0]==0xabcd121e);save_states=false;
 assert(f.calls==9 && f.auth==10 && run(&f,&s)==MT76_TIMING_REFUSED);
 uint32_t seed=0x1924abcd;
 for(unsigned q=0;q<1024;q++){seed=seed*1664525u+1013904223u;uint32_t a=seed;seed=seed*1664525u+1013904223u;uint32_t b=seed;if(a==UINT32_MAX || b==UINT32_MAX)continue;
  f=(struct fake){.values={a}};s=(struct mt76_usb_cycle){0};assert(run(&f,&s)==MT76_TIMING_OK && s.after[0]==((a&~255u)|30u));
 }
 for(unsigned op=0;op<9;op++){
  f=(struct fake){.fail=op+1};s=(struct mt76_usb_cycle){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  unsigned n=op%9;if(n!=3 && n!=4){f=(struct fake){.bad=op+1};s=(struct mt76_usb_cycle){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==op+1 && !s.complete);}
  s=states[op];bool w;uint32_t a;assert(mt76_usb_cycle_request(op,&w,&a));uint8_t bytes[4]={0};if(w)for(unsigned j=0;j<4;j++)bytes[j]=(uint8_t)(mt76_usb_cycle_value(op/9,s.before[op/9])>>(8*j));
  assert(mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));assert(!mt76_usb_cycle_transfer_valid(&s,op,!w,a,bytes,4));assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a+1,bytes,4));assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,3));
  if(w)for(unsigned bit=0;bit<32;bit++){bytes[bit/8]^=1u<<(bit%8);assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));bytes[bit/8]^=1u<<(bit%8);}
  s.reads++;assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));s=states[op];s.writes++;assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));s=states[op];s.verified++;assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));
  s=states[op];s.complete=true;assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));
  for(unsigned j=0;j<3;j++)if(op>j){s=states[op];s.guards[0][j]^=1;assert(!mt76_usb_cycle_transfer_valid(&s,op,w,a,bytes,4));}
 }
 for(unsigned n=1;n<=10;n++){f=(struct fake){.deny=n};s=(struct mt76_usb_cycle){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<1;i++){f=(struct fake){0};f.values[i]=UINT32_MAX;s=(struct mt76_usb_cycle){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==9*i+4 && !s.complete);}
 bool w;uint32_t a;assert(!mt76_usb_cycle_request(9,&w,&a));assert(mt76_usb_cycle_address(1)==UINT32_MAX && mt76_usb_cycle_value(1,0)==UINT32_MAX);
 f=(struct fake){.values={0}};s=(struct mt76_usb_cycle){0};assert(run(&f,&s)==MT76_TIMING_OK && mt76_usb_cycle_exact_profile_valid(&s));
 struct mt76_usb_cycle good=s;
 for(unsigned q=0;q<1;q++)for(unsigned b=0;b<32;b++){s=good;s.before[q]^=1u<<b;assert(!mt76_usb_cycle_exact_profile_valid(&s));s=good;s.after[q]^=1u<<b;assert(!mt76_usb_cycle_exact_profile_valid(&s));}
 for(unsigned q=0;q<2;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_usb_cycle_exact_profile_valid(&s));}
 s=good;s.complete=false;assert(!mt76_usb_cycle_exact_profile_valid(&s));s=good;s.operations--;assert(!mt76_usb_cycle_exact_profile_valid(&s));s=good;s.verified--;assert(!mt76_usb_cycle_exact_profile_valid(&s));assert(!mt76_usb_cycle_exact_profile_valid(NULL));
 struct mt76_channel_state c={.attempted=true,.complete=true,.raw_ibi9=true,.reads=18,.values={0,0x583f,2,0x21400,0,4,0xffe4,0x601,0xffffffff,0x2f2f1b1b,0x640,0xc00020},.guards={{0,0xf000f200,0x40000050},{0,0xf000f200,0x40000050}}};
 assert(mt76_channel_observe_exact_profile_valid(&c));struct mt76_channel_state cg=c;
 for(unsigned q=0;q<12;q++)for(unsigned b=0;b<32;b++){c=cg;c.values[q]^=1u<<b;assert(!mt76_channel_observe_exact_profile_valid(&c));}
 for(unsigned q=0;q<2;q++)for(unsigned j=0;j<3;j++)for(unsigned b=0;b<32;b++){c=cg;c.guards[q][j]^=1u<<b;assert(!mt76_channel_observe_exact_profile_valid(&c));}
 c=cg;c.raw_ibi9=false;assert(!mt76_channel_observe_exact_profile_valid(&c));c=cg;c.complete=false;assert(!mt76_channel_observe_exact_profile_valid(&c));c=cg;c.reads--;assert(!mt76_channel_observe_exact_profile_valid(&c));assert(!mt76_channel_observe_exact_profile_valid(NULL));
 puts("baseband path: PASS fixed low-byte timing mask,1024 preserved-word cases, every transfer/cancel/guard/readback boundary, payload/proof mutation and no replay");
}
