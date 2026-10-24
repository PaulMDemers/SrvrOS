#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct fake {unsigned auth,ops,fail,deny,bad,mutate_op,byte,bit;int received;};
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=f->ops++,step=op%8;
 assert(step<3 || step>=5);unsigned n=step<3?step:step-5;
 assert(a==(n==0?0x1004:n==1?0x2000:0x208));
 if(f->ops==f->fail)return false;
 *v=n==1?0xf000f200:n==2?0x40000050:0;
 if(f->ops==f->bad)*v^=1;
 return true;
}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t capacity){
 struct fake *f=p;unsigned op=f->ops++,i=op/8,step=op%8;assert(step==3 || step==4);
 unsigned seq=(8+2*i+(step==4))%15+1;
 uint32_t expected_header=8|(seq<<16)|((step==3?12u:10u)<<20)|0x50000000;
 uint32_t header=0,address=0,value=0;
 for(unsigned b=0;b<4;b++){header|=(uint32_t)packet[b]<<(8*b);address|=(uint32_t)packet[4+b]<<(8*b);value|=(uint32_t)packet[8+b]<<(8*b);assert(!packet[12+b]);}
 assert(n==16 && capacity==1024 && header==expected_header && address==mt76_rf_central_address(i));
 assert(value==(step==3?mt76_rf_central_value(i):0));
 if(f->ops==f->fail)return -1;
 memset(reply,0,capacity);uint32_t words[4]={0x4a000008|(seq<<16),address,mt76_rf_central_value(i),0x4a000008|(seq<<16)};
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[j*4+b]=(uint8_t)(words[j]>>(8*b));
 if(f->ops==f->mutate_op)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_central *s){
 const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_rf_central_apply(s,&io);
}
int main(void){
 struct fake f={.received=16};struct mt76_rf_central s={0};
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==352 && f.ops==352 && f.auth==353 && s.reads==264 && s.exchanges==88 && s.acks==44 && s.verified==44);
 assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==352);
 assert(mt76_rf_central_address(44)==UINT32_MAX && mt76_rf_central_value(44)==UINT32_MAX && !mt76_rf_central_sequence(44,true));
 for(unsigned op=0;op<352;op++){
  uint8_t packet[18];memset(packet,0xcc,18);bool cmd=op%8==3 || op%8==4;
  assert(mt76_rf_central_packet(op,packet+1,16)==(cmd?16u:0u));assert(packet[0]==0xcc && packet[17]==0xcc);
  assert(!mt76_rf_central_packet(op,packet,15) && !mt76_rf_central_packet(op,NULL,16));
  assert((mt76_rf_central_guard_address(op)==UINT32_MAX)==cmd);
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_rf_central){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(!cmd){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_rf_central){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=(struct fake){.received=16,.mutate_op=op+1,.byte=byte,.bit=bit};s=(struct mt76_rf_central){0};
   enum mt76_mcu_timing_status status=run(&f,&s);assert(status==(byte==8?MT76_TIMING_STATE:MT76_TIMING_REPLY));assert(!s.complete && f.ops==op+1);
  }
 }
 for(unsigned n=1;n<=353;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_rf_central){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==n-1);}
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_rf_central){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.ops==4);}
 assert(mt76_rf_central_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 RF central: PASS 44 writes/readbacks, exact packets, 352 failures/353 cancellations, guard and all response-bit mutations, no retry");
}
