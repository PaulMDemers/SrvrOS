#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad,mutate;uint32_t values[21];struct mt76_channel_bbp *s;};
static void init(struct fake *f){for(unsigned i=0;i<21;i++)f->values[i]=mt76_channel_bbp_initial(i);}
static struct mt76_channel_bbp states[189];static bool save;
static bool auth(void *p){struct fake *f=p;++f->auth;if(f->mutate==f->auth)f->s->reads++;return f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
 struct fake *f=p;assert(f->calls==op && length==4);if(save)states[op]=*f->s;unsigned i=op/9,n=op%9;bool w;uint32_t expected;assert(mt76_channel_bbp_request(op,&w,&expected) && w==write && a==expected);
 uint32_t v=0;if(write){for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);assert(v==mt76_channel_bbp_value(i,f->values[i]));f->values[i]=v;}
 else{v=n<3?(n==1?0xf000f200:n==2?0x40000050:0):n>=6?(n==7?0xf000f200:n==8?0x40000050:0):f->values[i];if(f->calls+1==f->bad)v^=1;for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_channel_bbp *s){f->s=s;const struct mt76_post_mac_io io={f,auth,transfer};return mt76_channel_bbp_apply(s,&io);}
int main(void){
 struct fake f={0};init(&f);struct mt76_channel_bbp s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_channel_bbp_exact_profile_valid(&s));save=false;assert(f.calls==189 && f.auth==190 && s.reads==168 && s.writes==21 && s.verified==21 && s.after[3]==0x16344ef0 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned i=0;i<21;i++)assert(mt76_channel_bbp_address(i)==mt76_gain_inventory_target(i));
 for(unsigned bit=0;bit<32;bit++){uint32_t v=1u<<bit;assert(mt76_channel_bbp_value(0,v)==(v&~0x20u));}
 for(unsigned op=0;op<189;op++){
  f=(struct fake){.fail=op+1};init(&f);s=(struct mt76_channel_bbp){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%9!=4){f=(struct fake){.bad=op+1};init(&f);s=(struct mt76_channel_bbp){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==op+1);}
  s=states[op];bool w;uint32_t a;assert(mt76_channel_bbp_request(op,&w,&a));uint8_t data[4]={0};if(w){uint32_t v=mt76_channel_bbp_value(op/9,s.before[op/9]);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
  assert(mt76_channel_bbp_transfer_valid(&s,op,w,a,data,4));assert(!mt76_channel_bbp_transfer_valid(&s,op,!w,a,data,4));assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a+1,data,4));assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a,data,3));
  if(w)for(unsigned b=0;b<32;b++){data[b/8]^=1u<<(b%8);assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a,data,4));data[b/8]^=1u<<(b%8);}
  s.reads++;assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a,data,4));s=states[op];s.writes++;assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a,data,4));s=states[op];s.verified++;assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a,data,4));s=states[op];s.complete=true;assert(!mt76_channel_bbp_transfer_valid(&s,op,w,a,data,4));
 }
 for(unsigned n=1;n<=190;n++){f=(struct fake){.deny=n};init(&f);s=(struct mt76_channel_bbp){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<21;i++){f=(struct fake){0};init(&f);f.values[i]=UINT32_MAX;s=(struct mt76_channel_bbp){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==i*9+4);}
 for(unsigned n=1;n<=190;n++){f=(struct fake){.mutate=n};init(&f);s=(struct mt76_channel_bbp){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.calls==n-1);}
 f=(struct fake){0};init(&f);s=(struct mt76_channel_bbp){0};assert(run(&f,&s)==MT76_TIMING_OK);s.guards[41][2]^=1;assert(!mt76_channel_bbp_exact_profile_valid(&s));
 assert(mt76_channel_bbp_initial(21)==UINT32_MAX && mt76_channel_bbp_address(21)==UINT32_MAX);bool w;uint32_t a;assert(!mt76_channel_bbp_request(189,&w,&a));
 puts("channel BBP: PASS21 fixed targets, fresh-profile refusal, all transfer/cancel/readback/guard boundaries and no replay");
}
