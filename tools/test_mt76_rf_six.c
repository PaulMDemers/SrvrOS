#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct fake {unsigned auth,ops,fail,deny,bad,mutate_op,byte,bit;int received;uint8_t first,later;};
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=f->ops++,n=op<3?op:(op-4)%4;assert(n<3 && a==(n==0?0x1004:n==1?0x2000:0x208));
 if(f->ops==f->fail)return false;
 *v=n==1?0xf000f200:n==2?0x40000050:0;if(f->ops==f->bad)*v^=1;return true;
}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t capacity){
 struct fake *f=p;unsigned op=f->ops++,i=(op-3)/4;assert(op>=3 && (op-3)%4==0 && i<5 && n==16 && capacity==1024);
 static const uint8_t regs[5]={6,1,2,6,22};uint8_t expected[16]={8,0,(uint8_t)(0xa6+i),0x50,regs[i],0,0,0x80,0,0,0,0,0,0,0,0};assert(!memcmp(packet,expected,16));
 if(f->ops==f->fail)return -1;
 uint32_t h=0x4a000008|((6+i)<<16),words[4]={h,0x80000000|regs[i],i==0?f->first:i==3?f->later:i==1?1u:i==2?17u:0u,h};
 memset(reply,0,capacity);for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(words[j]>>(8*b));
 if(f->ops==f->mutate_op)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_six *s){const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_rf_six_apply(s,&io);}
static struct mt76_rf_central prefix(unsigned value){
 struct mt76_rf_central s={.attempted=true,.operations=value==65?45:44,.reads=33,.exchanges=12,.acks=6,.verified=value==65?6:5,.received=16,.words={0x4a050008,0x80000006,value,0x4a050008}};
 for(unsigned i=0;i<6;i++)s.values[i]=mt76_rf_central_value(i);
 for(unsigned i=0;i<11;i++){s.guards[i][1]=0xf000f200;s.guards[i][2]=0x40000050;}return s;
}
int main(void){
 for(unsigned value=0;value<256;value++){
  struct mt76_rf_central s=prefix(value);enum mt76_mcu_timing_status status=value==65?MT76_TIMING_CANCELLED:MT76_TIMING_STATE;
  assert(mt76_rf_six_prefix_valid(&s,status));assert(!mt76_rf_six_prefix_valid(&s,MT76_TIMING_OK));
  for(unsigned i=0;i<33;i++){s.guards[i/3][i%3]^=1;assert(!mt76_rf_six_prefix_valid(&s,status));s.guards[i/3][i%3]^=1;}
  for(unsigned fault=0;fault<15;fault++){
   struct mt76_rf_central bad=s;
   switch(fault){case 0:bad.attempted=false;break;case 1:bad.complete=true;break;case 2:bad.operations++;break;case 3:bad.reads--;break;case 4:bad.exchanges--;break;case 5:bad.acks--;break;case 6:bad.verified--;break;case 7:bad.received=15;break;case 8:bad.words[0]^=1;break;case 9:bad.words[1]^=1;break;case 10:bad.words[2]=256;break;case 11:bad.words[3]^=1;break;case 12:bad.values[0]^=1;break;case 13:bad.values[4]^=1;break;case 14:bad.guards[10][2]=UINT32_MAX;break;}
   assert(!mt76_rf_six_prefix_valid(&bad,status));
  }
  struct fake f={.received=16,.first=value,.later=255-value};struct mt76_rf_six o={0};assert(run(&f,&o)==MT76_TIMING_OK && o.complete && o.operations==23 && o.reads==18 && o.requests==5 && f.ops==23 && f.auth==24);
  assert(mt76_rf_six_complete_valid(&o));
  for(unsigned i=0;i<18;i++){o.guards[i/3][i%3]^=1;assert(!mt76_rf_six_complete_valid(&o));o.guards[i/3][i%3]^=1;}
  for(unsigned i=0;i<5;i++)for(unsigned n=0;n<4;n++){o.words[i][n]^=0x100;assert(!mt76_rf_six_complete_valid(&o));o.words[i][n]^=0x100;}
  assert(run(&f,&o)==MT76_TIMING_REFUSED);
 }
 for(unsigned op=0;op<23;op++){
  bool command=op>=3 && (op-3)%4==0;uint8_t packet[18];memset(packet,0xcc,sizeof packet);
  assert(mt76_rf_six_packet(op,packet+1,16)==(command?16u:0u) && packet[0]==0xcc && packet[17]==0xcc);
  assert(!mt76_rf_six_packet(op,NULL,16) && !mt76_rf_six_packet(op,packet,15));
  struct fake f={.received=16,.fail=op+1};struct mt76_rf_six s={0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op+1);
  if(!command){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_rf_six){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=(struct fake){.received=16,.mutate_op=op+1,.byte=byte,.bit=bit};s=(struct mt76_rf_six){0};enum mt76_mcu_timing_status status=run(&f,&s);
   bool observed=byte==8 && (op==3 || op==15);assert(status==(observed?MT76_TIMING_OK:byte==8?MT76_TIMING_STATE:MT76_TIMING_REPLY));
   assert(s.complete==observed);
  }
 }
 for(unsigned n=1;n<=24;n++){struct fake f={.received=16,.deny=n};struct mt76_rf_six s={0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==n-1);}
 for(int n=0;n<=1025;n++)if(n!=16){struct fake f={.received=n};struct mt76_rf_six s={0};assert(run(&f,&s)==MT76_TIMING_REPLY && f.ops==4 && !s.complete);}
 assert(!mt76_rf_six_prefix_valid(NULL,MT76_TIMING_STATE) && mt76_rf_six_guard_address(23)==UINT32_MAX && !mt76_rf_six_packet(23,NULL,16));
 puts("mt76 RF six: PASS bounded prefix states, fixed read packets, observations vs controls, every failure/cancel/response mutation and no replay");
}
