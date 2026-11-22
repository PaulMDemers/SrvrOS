#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned calls,auth,fail,deny,bad,bad_bit;uint32_t values[2];struct mt76_band_mac *s;};
static struct mt76_band_mac states[18];static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
 struct fake *f=p;assert(f->calls==op && length==4);if(save)states[op]=*f->s;unsigned i=op/9,n=op%9;bool w;uint32_t expected;assert(mt76_band_mac_request(op,&w,&expected) && w==write && a==expected);
 uint32_t v=0;if(write){for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);assert(v==mt76_band5g_mac_value(i));f->values[i]=v;}
 else{v=n<3?(n==1?0xf000f200:n==2?0x40000050:0):n>=6?(n==7?0xf000f200:n==8?0x40000050:0):f->values[i];if(f->calls+1==f->bad)v^=1u<<f->bad_bit;for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_band_mac *s){f->s=s;const struct mt76_band_mac_io io={f,auth,transfer};return mt76_band5g_mac_apply(s,&io);}
static uint32_t fixture_word(FILE *f){uint32_t v=0;for(unsigned j=0;j<4;j++){int c=fgetc(f);assert(c!=EOF);v|=(uint32_t)c<<(8*j);}return v;}
int main(int argc,char **argv){
 assert(argc==2);FILE *fixture=fopen(argv[1],"rb");assert(fixture);
 for(unsigned i=0;i<2;i++){assert(mt76_band_mac_address(i)==fixture_word(fixture));assert(mt76_band5g_mac_value(i)==fixture_word(fixture));}
 assert(fgetc(fixture)==EOF);fclose(fixture);
 assert(mt76_band5g_mac_value(2)==UINT32_MAX && mt76_band_mac_address(2)==UINT32_MAX);
 bool write;uint32_t address;assert(!mt76_band_mac_request(18,&write,&address));
 assert(!mt76_band_mac_request(0,NULL,&address) && !mt76_band_mac_request(0,&write,NULL));

 struct fake f={.values={0,0}};struct mt76_band_mac s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_band5g_mac_exact_profile_valid(&s));save=false;assert(f.calls==18 && f.auth==19 && s.reads==16 && s.writes==2 && s.verified==2 && s.after[1]==0x01010102 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=0;op<18;op++){
  f=(struct fake){.fail=op+1};s=(struct mt76_band_mac){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(op%9!=4)for(unsigned bit=0;bit<32;bit++){f=(struct fake){.bad=op+1,.bad_bit=bit};s=(struct mt76_band_mac){0};enum mt76_mcu_timing_status result=run(&f,&s);
   /* DMA guard admits other high bits while its four busy bits remain clear. */
   if((op%9==2 || op%9==8) && bit>=4)assert(result==MT76_TIMING_OK && mt76_band5g_mac_exact_profile_valid(&s));
   else assert(result==MT76_TIMING_STATE && !s.complete && f.calls==op+1);
  }
  s=states[op];bool w;uint32_t a;assert(mt76_band_mac_request(op,&w,&a));uint8_t data[4]={0};if(w){uint32_t v=mt76_band5g_mac_value(op/9);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
  assert(mt76_band5g_mac_transfer_valid(&s,op,w,a,data,4));assert(!mt76_band5g_mac_transfer_valid(&s,op,!w,a,data,4));assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a+1,data,4));assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a,data,3));
  if(w)for(unsigned b=0;b<32;b++){data[b/8]^=1u<<(b%8);assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a,data,4));data[b/8]^=1u<<(b%8);}
  s.reads++;assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a,data,4));s=states[op];s.writes++;assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a,data,4));s=states[op];s.verified++;assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a,data,4));s=states[op];s.complete=true;assert(!mt76_band5g_mac_transfer_valid(&s,op,w,a,data,4));
 }
 for(unsigned n=1;n<=19;n++){f=(struct fake){.deny=n};s=(struct mt76_band_mac){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<2;i++){f=(struct fake){0};f.values[i]=UINT32_MAX;s=(struct mt76_band_mac){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.calls==i*9+4);}
 for(unsigned i=0;i<2;i++)for(unsigned bit=0;bit<32;bit++){s=states[9*i+4];s.before[i]^=1u<<bit;uint8_t data[4]={0};uint32_t v=mt76_band5g_mac_value(i);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));assert(!mt76_band5g_mac_transfer_valid(&s,9*i+4,true,mt76_band_mac_address(i),data,4));}
 struct mt76_band_mac good;
 f=(struct fake){0};s=(struct mt76_band_mac){0};assert(run(&f,&s)==MT76_TIMING_OK);good=s;
 assert(!mt76_band_mac_exact_profile_valid(&good)); /* Legacy band cannot accept5GHz proof. */
 for(unsigned i=0;i<2;i++)for(unsigned bit=0;bit<32;bit++){
  s=good;s.before[i]^=1u<<bit;assert(!mt76_band5g_mac_exact_profile_valid(&s));
  s=good;s.after[i]^=1u<<bit;assert(!mt76_band5g_mac_exact_profile_valid(&s));
 }
 for(unsigned g=0;g<4;g++)for(unsigned j=0;j<3;j++)for(unsigned bit=0;bit<32;bit++){
  s=good;s.guards[g][j]^=1u<<bit;assert(mt76_band5g_mac_exact_profile_valid(&s)==(j==2 && bit>=4));
 }
 for(unsigned op=4;op<18;op+=9){
  s=states[op];uint8_t data[4];uint32_t v=mt76_band_mac_value(op/9,0);
  for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
  assert(!mt76_band5g_mac_transfer_valid(&s,op,true,mt76_band_mac_address(op/9),data,4));
  v=mt76_band5g_mac_value(op/9);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
  assert(!mt76_band_mac_transfer_valid(&s,op,true,mt76_band_mac_address(op/9),data,4));
 }
 assert(!mt76_band5g_mac_exact_profile_valid(NULL));s=good;s.complete=false;assert(!mt76_band5g_mac_exact_profile_valid(&s));
 s=good;s.operations--;assert(!mt76_band5g_mac_exact_profile_valid(&s));
 s=good;s.reads++;assert(!mt76_band5g_mac_exact_profile_valid(&s));
 s=good;s.writes++;assert(!mt76_band5g_mac_exact_profile_valid(&s));
 s=good;s.verified++;assert(!mt76_band5g_mac_exact_profile_valid(&s));
 puts("5GHz band MAC: PASS fixed words, every transfer/cancel/readback/guard boundary and no replay");
}
