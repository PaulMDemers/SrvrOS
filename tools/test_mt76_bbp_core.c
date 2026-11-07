#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const uint8_t packets[2][24]={
 {16,0,0xc8,0x50,4,0x20,0x41,0,2,0,0,0,16,0x20,0x41,0,0,0,0,0,0,0,0,0},
 {16,0,0xc9,0x50,0x60,0x20,0x41,0,0,0,0,0,0x80,0x20,0x41,0,10,0,3,0x40,0,0,0,0}
};
static const uint8_t replies[2][16]={
 {8,0,8,0x4a,4,0x20,0x41,0,2,0,0,0,8,0,8,0x4a},
 {8,0,9,0x4a,0x60,0x20,0x41,0,0,0,0,0,8,0,9,0x4a}
};
struct fake {unsigned ops,auth,deny,fail,writes,bad_ack,bad_value,enabled,missing;bool stale;};
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t address,uint32_t *value){
 struct fake *f=p;unsigned batch=f->ops/9,step=f->ops%9;
 static const uint32_t order[2][9]={{0x1004,0x2000,0x2004,0x2010,0,0x2004,0x2010,0x1004,0x2000},
 {0x1004,0x2000,0x2060,0x2080,0,0x2060,0x2080,0x1004,0x2000}};
 static const uint32_t targets[2][2]={{2,0},{0,0x4003000a}};
 assert(batch<2 && address==order[batch][step]);
 if(++f->ops==f->fail)return false;
 *value=address==0x1004?0:address==0x2000?0xf000f200:step<4?0x55:targets[batch][step-5];
 if(f->ops==f->missing)*value=UINT32_MAX;
 if(f->ops==f->enabled){assert(address==0x1004);*value=12;}
 if(f->ops==f->bad_value)*value^=1;
 return true;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct fake *f=p;unsigned batch=f->ops/9;
 assert(batch<2 && f->ops%9==4 && length==24 && capacity>=16);
 assert(!memcmp(packet,packets[batch],24));
 if(++f->ops==f->fail)return -1;
 f->writes++;memcpy(reply,replies[f->stale?0:batch],16);
 if(f->bad_ack==batch+1)reply[4]^=1;
 return 16;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_mcu_mac *s){
 const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};
 return mt76_mcu_apply_bbp_core(s,&io);
}
int main(void){
 for(unsigned b=0;b<2;b++){
  uint8_t out[26];memset(out,0xcc,sizeof out);
  assert(mt76_mcu_bbp_packet(b,out+1,24)==24 && out[0]==0xcc && out[25]==0xcc);
  assert(!memcmp(out+1,packets[b],24));
  assert(!mt76_mcu_bbp_packet(b,out,23));
  assert(mt76_mcu_validate_bbp_reply(b,replies[b],16));
  assert(!mt76_mcu_validate_bbp_reply(b,replies[1-b],16));
  assert(!mt76_mcu_validate_bbp_reply(b,NULL,16));
  for(size_t n=0;n<18;n++)if(n!=16)assert(!mt76_mcu_validate_bbp_reply(b,replies[b],n));
  for(unsigned i=0;i<16;i++)for(unsigned bit=0;bit<8;bit++){
   uint8_t r[16];memcpy(r,replies[b],16);r[i]^=1u<<bit;
   assert(!mt76_mcu_validate_bbp_reply(b,r,16));
  }
 }
 uint8_t buf[24];memset(buf,0xcc,sizeof buf);
 assert(!mt76_mcu_bbp_packet(2,buf,sizeof buf) && buf[0]==0xcc);
 assert(!mt76_mcu_validate_bbp_reply(2,replies[0],16));
 struct fake f={0};struct mt76_mcu_mac s={0};
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.ack_mask==3 && s.reads==16);
 assert(f.ops==18 && f.writes==2 && s.batches_completed==2);
 assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==18);
 for(unsigned fail=1;fail<=18;fail++){
  f=(struct fake){.fail=fail};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_IO && f.ops==fail && !s.complete);
  assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==fail);
 }
 for(unsigned deny=1;deny<=20;deny++){
  f=(struct fake){.deny=deny};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.auth==deny);
 }
 for(unsigned b=1;b<=2;b++){
  f=(struct fake){.bad_ack=b};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.writes==b);
 }
 const unsigned reads[]={6,7,15,16};
 for(unsigned i=0;i<4;i++){
  f=(struct fake){.bad_value=reads[i]};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==reads[i]);
 }
 const unsigned boundaries[]={1,8,10,17};
 for(unsigned i=0;i<4;i++){
  f=(struct fake){.enabled=boundaries[i]};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==boundaries[i]);
 }
 const unsigned cores[]={2,9,11,18};
 for(unsigned n=0;n<4;n++){
  f=(struct fake){.bad_value=cores[n]};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==cores[n]);
 }
 f=(struct fake){.stale=true};s=(struct mt76_mcu_mac){0};
 assert(run(&f,&s)==MT76_TIMING_REPLY && s.batches_completed==1);
 f=(struct fake){.missing=2};s=(struct mt76_mcu_mac){0};
 assert(run(&f,&s)==MT76_TIMING_STATE && f.writes==0);
 assert(mt76_mcu_apply_bbp_core(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 BBP CORE: PASS fixed packets, ACK mutations, per-operation failures, stale reply, readbacks, ownership and MAC gates");
}
