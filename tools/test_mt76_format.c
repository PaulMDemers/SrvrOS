#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const uint8_t packets[2][24]={
 {16,0,0xc6,0x50,0x50,2,0x41,0,0,0,0,0,0x60,2,0x41,0,0,0,0,0,0,0,0,0},
 {16,0,0xc7,0x50,0x10,0x14,0x41,0,0xff,1,0,0,12,0x15,0x41,0,3,0,0,0,0,0,0,0}
};
static const uint8_t replies[2][16]={
 {8,0,6,0x4a,0x50,2,0x41,0,0,0,0,0,8,0,6,0x4a},
 {8,0,7,0x4a,0x10,0x14,0x41,0,0xff,1,0,0,8,0,7,0x4a}
};
struct fake {unsigned ops,auth,deny,fail,writes,bad_ack,bad_value,enabled,missing;bool stale;};
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t address,uint32_t *value){
 struct fake *f=p;unsigned batch=f->ops/7,step=f->ops%7;
 static const uint32_t order[2][7]={{0x1004,0x0250,0x0260,0,0x0250,0x0260,0x1004},
 {0x1004,0x1410,0x150c,0,0x1410,0x150c,0x1004}};
 static const uint32_t targets[2][2]={{0,0},{0x1ff,3}};
 assert(batch<2 && address==order[batch][step]);
 if(++f->ops==f->fail)return false;
 *value=address==0x1004?0:step<3?0x55:targets[batch][step-4];
 if(f->ops==f->missing)*value=UINT32_MAX;
 if(f->ops==f->enabled){assert(address==0x1004);*value=12;}
 if(f->ops==f->bad_value)*value^=1;
 return true;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct fake *f=p;unsigned batch=f->ops/7;
 assert(batch<2 && f->ops%7==3 && length==24 && capacity>=16);
 assert(!memcmp(packet,packets[batch],24));
 if(++f->ops==f->fail)return -1;
 f->writes++;memcpy(reply,replies[f->stale?0:batch],16);
 if(f->bad_ack==batch+1)reply[4]^=1;
 return 16;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_mcu_mac *s){
 const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};
 return mt76_mcu_apply_mac_format(s,&io);
}
int main(void){
 for(unsigned b=0;b<2;b++){
  uint8_t out[26];memset(out,0xcc,sizeof out);
  assert(mt76_mcu_format_packet(b,out+1,24)==24 && out[0]==0xcc && out[25]==0xcc);
  assert(!memcmp(out+1,packets[b],24));
  assert(!mt76_mcu_format_packet(b,out,23));
  assert(mt76_mcu_validate_format_reply(b,replies[b],16));
  assert(!mt76_mcu_validate_format_reply(b,replies[1-b],16));
  assert(!mt76_mcu_validate_format_reply(b,NULL,16));
  for(size_t n=0;n<18;n++)if(n!=16)assert(!mt76_mcu_validate_format_reply(b,replies[b],n));
  for(unsigned i=0;i<16;i++)for(unsigned bit=0;bit<8;bit++){
   uint8_t r[16];memcpy(r,replies[b],16);r[i]^=1u<<bit;
   assert(!mt76_mcu_validate_format_reply(b,r,16));
  }
 }
 uint8_t buf[24];memset(buf,0xcc,sizeof buf);
 assert(!mt76_mcu_format_packet(2,buf,sizeof buf) && buf[0]==0xcc);
 assert(!mt76_mcu_validate_format_reply(2,replies[0],16));
 struct fake f={0};struct mt76_mcu_mac s={0};
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.ack_mask==3 && s.reads==12);
 assert(f.ops==14 && f.writes==2 && s.batches_completed==2);
 assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==14);
 for(unsigned fail=1;fail<=14;fail++){
  f=(struct fake){.fail=fail};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_IO && f.ops==fail && !s.complete);
  assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==fail);
 }
 for(unsigned deny=1;deny<=16;deny++){
  f=(struct fake){.deny=deny};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.auth==deny);
 }
 for(unsigned b=1;b<=2;b++){
  f=(struct fake){.bad_ack=b};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.writes==b);
 }
 const unsigned reads[]={5,6,12,13};
 for(unsigned i=0;i<4;i++){
  f=(struct fake){.bad_value=reads[i]};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==reads[i]);
 }
 const unsigned boundaries[]={1,7,8,14};
 for(unsigned i=0;i<4;i++){
  f=(struct fake){.enabled=boundaries[i]};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==boundaries[i]);
 }
 f=(struct fake){.stale=true};s=(struct mt76_mcu_mac){0};
 assert(run(&f,&s)==MT76_TIMING_REPLY && s.batches_completed==1);
 f=(struct fake){.missing=2};s=(struct mt76_mcu_mac){0};
 assert(run(&f,&s)==MT76_TIMING_STATE && f.writes==0);
 assert(mt76_mcu_apply_mac_format(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 MAC format: PASS fixed packets, ACK mutations, per-operation failures, stale reply, readbacks, ownership and MAC gates");
}
