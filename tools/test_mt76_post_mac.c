#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad;uint32_t values[3];struct mt76_post_mac *s;};
static struct mt76_post_mac states[27];static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
 struct fake *f=p;assert(f->calls==op && length==4);if(save)states[op]=*f->s;unsigned i=op/9,n=op%9;bool w;uint32_t expected;assert(mt76_post_mac_request(op,&w,&expected) && w==write && a==expected);
 uint32_t v=0;if(write){for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);assert(v==mt76_post_mac_value(i,f->values[i]));f->values[i]=v;}
 else{v=n<3?(n==1?0xf000f200:n==2?0x40000050:0):n>=6?(n==7?0xf000f200:n==8?0x40000050:0):f->values[i];if(f->calls+1==f->bad)v^=1;for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_post_mac *s){f->s=s;const struct mt76_post_mac_io io={f,auth,transfer};return mt76_post_mac_apply(s,&io);}
int main(void){
 struct fake f={.values={1,0x6c6c6c6c,0xc9540000}};struct mt76_post_mac s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_post_mac_exact_profile_valid(&s));save=false;assert(f.calls==27 && f.auth==28 && s.reads==24 && s.writes==3 && s.verified==3 && s.after[2]==0x89440000 && run(&f,&s)==MT76_TIMING_REFUSED);
 uint32_t seed=12345,mask[]={0xc,UINT32_MAX,0x769bff00},bits[]={0,0x63707400,0};
 for(unsigned q=0;q<1024;q++)for(unsigned i=0;i<3;i++){seed=seed*1664525+1013904223;assert(mt76_post_mac_value(i,seed)==((seed&~mask[i])|bits[i]));}
 for(unsigned op=0;op<27;op++){
  f=(struct fake){.fail=op+1,.values={1,0x6c6c6c6c,0xc9540000}};s=(struct mt76_post_mac){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%9!=4){f=(struct fake){.bad=op+1,.values={1,0x6c6c6c6c,0xc9540000}};s=(struct mt76_post_mac){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op+1);}
  s=states[op];bool w;uint32_t a;assert(mt76_post_mac_request(op,&w,&a));uint8_t data[4]={0};if(w){uint32_t v=mt76_post_mac_value(op/9,s.before[op/9]);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
  assert(mt76_post_mac_transfer_valid(&s,op,w,a,data,4));assert(!mt76_post_mac_transfer_valid(&s,op,!w,a,data,4));assert(!mt76_post_mac_transfer_valid(&s,op,w,a+1,data,4));assert(!mt76_post_mac_transfer_valid(&s,op,w,a,data,3));
  if(w)for(unsigned b=0;b<32;b++){data[b/8]^=1u<<(b%8);assert(!mt76_post_mac_transfer_valid(&s,op,w,a,data,4));data[b/8]^=1u<<(b%8);}
  s.reads++;assert(!mt76_post_mac_transfer_valid(&s,op,w,a,data,4));s=states[op];s.writes++;assert(!mt76_post_mac_transfer_valid(&s,op,w,a,data,4));s=states[op];s.verified++;assert(!mt76_post_mac_transfer_valid(&s,op,w,a,data,4));s=states[op];s.complete=true;assert(!mt76_post_mac_transfer_valid(&s,op,w,a,data,4));
 }
 for(unsigned n=1;n<=28;n++){f=(struct fake){.deny=n,.values={1,0x6c6c6c6c,0xc9540000}};s=(struct mt76_post_mac){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<3;i++){f=(struct fake){.values={1,0x6c6c6c6c,0xc9540000}};f.values[i]=UINT32_MAX;s=(struct mt76_post_mac){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==i*9+4);}
 puts("post MAC: PASS three fixed masks, fresh-profile refusal, all transfer/cancel/readback/guard boundaries and no replay");
}
