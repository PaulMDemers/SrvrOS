#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad;uint32_t values[5];struct mt76_bbp20 *s;};
static struct mt76_bbp20 states[45];static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
 struct fake *f=p;assert(f->calls==op && length==4);if(save)states[op]=*f->s;unsigned i=op/9,n=op%9;bool w;uint32_t expected;assert(mt76_bbp20_request(op,&w,&expected) && w==write && a==expected);
 uint32_t v=0;if(write){for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);assert(v==mt76_bbp20_value(i,f->values[i]));f->values[i]=v;}
 else{v=n<3?(n==1?0xf000f200:n==2?0x40000050:0):n>=6?(n==7?0xf000f200:n==8?0x40000050:0):f->values[i];if(f->calls+1==f->bad)v^=1;for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_bbp20 *s){f->s=s;const struct mt76_bbp20_io io={f,auth,transfer};return mt76_bbp20_apply(s,&io);}
int main(void){
 struct fake f={.values={2,0x21400,0,4,0xffe4}};struct mt76_bbp20 s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_bbp20_exact_profile_valid(&s));save=false;assert(f.calls==45 && f.auth==46 && s.reads==40 && s.writes==5 && s.verified==5 && s.after[4]==0xf1e4 && run(&f,&s)==MT76_TIMING_REFUSED);
 uint32_t seed=12345,mask[]={0x18,0x7300,3,7,0xfff},bits[]={0,0x1000,0,4,0x1e4};
 for(unsigned q=0;q<1024;q++)for(unsigned i=0;i<5;i++){seed=seed*1664525+1013904223;assert(mt76_bbp20_value(i,seed)==((seed&~mask[i])|bits[i]));}
 for(unsigned op=0;op<45;op++){
  f=(struct fake){.fail=op+1};s=(struct mt76_bbp20){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%9!=3 && op%9!=4){f=(struct fake){.bad=op+1};s=(struct mt76_bbp20){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op+1);}
  s=states[op];bool w;uint32_t a;assert(mt76_bbp20_request(op,&w,&a));uint8_t data[4]={0};if(w){uint32_t v=mt76_bbp20_value(op/9,s.before[op/9]);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
  assert(mt76_bbp20_transfer_valid(&s,op,w,a,data,4));assert(!mt76_bbp20_transfer_valid(&s,op,!w,a,data,4));assert(!mt76_bbp20_transfer_valid(&s,op,w,a+1,data,4));assert(!mt76_bbp20_transfer_valid(&s,op,w,a,data,3));
  if(w)for(unsigned b=0;b<32;b++){data[b/8]^=1u<<(b%8);assert(!mt76_bbp20_transfer_valid(&s,op,w,a,data,4));data[b/8]^=1u<<(b%8);}
  s.reads++;assert(!mt76_bbp20_transfer_valid(&s,op,w,a,data,4));s=states[op];s.writes++;assert(!mt76_bbp20_transfer_valid(&s,op,w,a,data,4));s=states[op];s.verified++;assert(!mt76_bbp20_transfer_valid(&s,op,w,a,data,4));s=states[op];s.complete=true;assert(!mt76_bbp20_transfer_valid(&s,op,w,a,data,4));
 }
 for(unsigned n=1;n<=46;n++){f=(struct fake){.deny=n};s=(struct mt76_bbp20){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<5;i++){f=(struct fake){0};f.values[i]=UINT32_MAX;s=(struct mt76_bbp20){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==i*9+4);}
 struct mt76_bw_observe b={.attempted=true,.complete=true,.operations=7,.reads=6,.exchanges=1,.received=16,.guards={{0,0xf000f200,0x40000050},{0,0xf000f200,0x40000050}},.words={0x4a0f0008,2,0,0x4a0f0008}};
 assert(mt76_bw_observe_exact_profile_valid(&b));struct mt76_bw_observe good=b;
 for(unsigned i=0;i<4;i++)for(unsigned bit=0;bit<32;bit++){b=good;b.words[i]^=1u<<bit;assert(!mt76_bw_observe_exact_profile_valid(&b));}
 for(unsigned e=0;e<2;e++)for(unsigned j=0;j<3;j++)for(unsigned bit=0;bit<32;bit++){b=good;b.guards[e][j]^=1u<<bit;assert(!mt76_bw_observe_exact_profile_valid(&b));}
 b=good;b.complete=false;assert(!mt76_bw_observe_exact_profile_valid(&b));assert(!mt76_bw_observe_exact_profile_valid(NULL));
 puts("BBP20: PASS five masks,5120 preserved-word cases, every transfer/cancel/readback/guard boundary, exact response bit mutation and no replay");
}
