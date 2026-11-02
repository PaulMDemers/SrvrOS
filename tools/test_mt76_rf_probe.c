#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const uint8_t request[16]={8,0,0xa8,0x50,0x16,0,0,0x80,0,0,0,0,0,0,0,0};
struct fake {unsigned auth,ops,fail,deny,bad,reads;int received;unsigned mutate,bit;};
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned n=f->reads++;assert(n<6 && a==mt76_rf_probe_read_address(n));
 if(++f->ops==f->fail)return false;
 *v=n%3==1?0xf000f200:n%3==2?0x40000050:0;
 if(f->bad==f->ops)*v^=1;
 return true;
}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t capacity){
 struct fake *f=p;assert(n==16 && !memcmp(packet,request,16) && capacity==1024 && f->ops==3);
 if(++f->ops==f->fail)return -1;
 static const uint8_t response[16]={8,0,8,0x4a,0x16,0,0,0x80,0x68,0,0,0,8,0,8,0x4a};
 memset(reply,0,capacity);memcpy(reply,response,16);
 if(f->mutate<16)reply[f->mutate]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_probe *s){
 const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_rf_probe_apply(s,&io);
}
static struct fake fresh(void){return (struct fake){.received=16,.mutate=16};}
int main(void){
 uint8_t packet[18];memset(packet,0xcc,sizeof packet);assert(mt76_rf_probe_packet(packet+1,16)==16 && !memcmp(packet+1,request,16) && packet[0]==0xcc && packet[17]==0xcc);
 assert(!mt76_rf_probe_packet(packet,15) && !mt76_rf_probe_packet(NULL,16));
 assert(mt76_rf_probe_read_address(6)==UINT32_MAX);
 struct fake f=fresh();struct mt76_rf_probe s={0};assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.reply_valid && s.reads==6 && s.value==0x68 && f.ops==7 && f.auth==8);
 assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==7);
 for(unsigned n=1;n<=7;n++){f=fresh();f.fail=n;s=(struct mt76_rf_probe){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==n);assert(run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=8;n++){f=fresh();f.deny=n;s=(struct mt76_rf_probe){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.auth==n);}
 for(unsigned n=1;n<=7;n++)if(n!=4){f=fresh();f.bad=n;s=(struct mt76_rf_probe){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==n);}
 for(unsigned n=0;n<33;n++)if(n!=16){f=fresh();f.received=(int)n;s=(struct mt76_rf_probe){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.ops==4);}
 for(unsigned n=0;n<16;n++)for(unsigned b=0;b<8;b++){
  f=fresh();f.mutate=n;f.bit=b;s=(struct mt76_rf_probe){0};enum mt76_mcu_timing_status r=run(&f,&s);
  assert(r==(n==8?MT76_TIMING_OK:MT76_TIMING_REPLY));assert(s.complete==(n==8));
 }
 assert(mt76_rf_probe_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 RF probe: PASS fixed read packet, exact framing/address, byte value, guards, all failure/cancel boundaries and no retry");
}
