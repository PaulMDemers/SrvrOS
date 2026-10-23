#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const uint8_t packet[40]={32,0,0xce,0x50,
 0x3c,0x23,0x41,0,0xb8,0x8c,0xa2,0x88,0x58,0x23,0x41,0,0x21,0x1e,0,0,
 4,0x23,0x41,0,3,0,0,0,8,0x23,0x41,0,0x64,0x64,0x3a,0,0,0,0,0};
static const uint8_t reply[16]={8,0,14,0x4a,0x3c,0x23,0x41,0,0xb8,0x8c,0xa2,0x88,8,0,14,0x4a};
struct fake {unsigned ops,auth,deny,fail,writes,bad_value;int bad_reply;};
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t address,uint32_t *value){
 struct fake *f=p;
 static const uint32_t order[]={0x1004,0x2000,0x233c,0x2358,0x2304,0x2308,0,0x233c,0x2358,0x2304,0x2308,0x1004,0x2000};
 static const uint32_t values[]={0,0xf000f200,0x88a28cb8,0x1e21,1,0x6464,0,0x88a28cb8,0x1e21,3,0x3a6464,0,0xf000f200};
 assert(f->ops<13 && address==order[f->ops]);*value=values[f->ops];
 if(++f->ops==f->fail)return false;
 if(f->ops==f->bad_value)*value^=1;
 return true;
}
static int exchange(void *p,const uint8_t *data,size_t length,uint8_t *out,size_t capacity){
 struct fake *f=p;assert(f->ops==6 && length==40 && capacity>=16 && !memcmp(data,packet,40));
 if(++f->ops==f->fail)return -1;
 f->writes++;memcpy(out,reply,16);
 if(f->bad_reply==1)out[2]=13; /* stale previous command */
 if(f->bad_reply==2)return 15;
 if(f->bad_reply==3)return 17;
 return 16;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_mcu_mac *s){
 const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};return mt76_mcu_apply_bbp_batch(s,&io);
}
int main(void){
 uint8_t out[42];memset(out,0xcc,sizeof out);
 assert(mt76_mcu_bbp_batch_packet(0,out+1,40)==40 && !memcmp(out+1,packet,40));
 assert(out[0]==0xcc && out[41]==0xcc);
 for(unsigned cap=0;cap<40;cap++){memset(out,0xcc,sizeof out);assert(!mt76_mcu_bbp_batch_packet(0,out,cap) && out[0]==0xcc);}
 assert(!mt76_mcu_bbp_batch_packet(1,out,40) && !mt76_mcu_bbp_batch_packet(0,NULL,40));
 assert(mt76_mcu_validate_bbp_batch_reply(0,reply,16));
 assert(!mt76_mcu_validate_bbp_batch_reply(1,reply,16) && !mt76_mcu_validate_bbp_batch_reply(0,NULL,16));
 for(unsigned n=0;n<18;n++)if(n!=16)assert(!mt76_mcu_validate_bbp_batch_reply(0,reply,n));
 for(unsigned i=0;i<16;i++)for(unsigned bit=0;bit<8;bit++){
  uint8_t r[16];memcpy(r,reply,16);r[i]^=1u<<bit;assert(!mt76_mcu_validate_bbp_batch_reply(0,r,16));
 }
 struct fake f={0};struct mt76_mcu_mac s={0};
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.ack_mask==1 && s.reads==12);
 assert(s.before[2]==1 && s.after[2]==3 && s.before[3]==0x6464 && s.after[3]==0x3a6464);
 assert(f.ops==13 && f.auth==14 && f.writes==1 && s.batches_completed==1);
 assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==13);
 for(unsigned n=1;n<=13;n++){
  f=(struct fake){.fail=n};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_IO && f.ops==n && !s.complete);
  assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==n);
 }
 for(unsigned n=1;n<=14;n++){
  f=(struct fake){.deny=n};s=(struct mt76_mcu_mac){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && f.auth==n && !s.complete);
 }
 const unsigned guarded[]={1,2,8,9,10,11,12,13};
 for(unsigned i=0;i<8;i++){
  f=(struct fake){.bad_value=guarded[i]};s=(struct mt76_mcu_mac){0};
  assert(run(&f,&s)==MT76_TIMING_STATE && f.ops==guarded[i] && !s.complete);
 }
 for(int n=1;n<=3;n++){
  f=(struct fake){.bad_reply=n};s=(struct mt76_mcu_mac){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.ops==7);
 }
 assert(mt76_mcu_apply_bbp_batch(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 BBP batch: PASS four-pair packet, strict reply mutations, all failures/authorization boundaries, readbacks, MAC/core guards and no retry");
}
