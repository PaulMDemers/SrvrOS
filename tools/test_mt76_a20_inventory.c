#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_rf_table *s;unsigned calls,auth,fail,deny,bad,corrupt,byte,bit,seed;int size;};
static struct mt76_rf_table states[168],good;static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->calls++;if(save)states[op]=*f->s;assert(op%7!=3 && a==mt76_a20_inventory_guard_address(op));unsigned n=op%7,j=n<3?n:n-4;*v=j==1?0xf000f200:j==2?0x40000050:0;if(f->bad==op+1)*v^=1;return f->fail!=op+1;}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t cap){
 struct fake *f=p;unsigned op=f->calls++,q=op/7;if(save)states[op]=*f->s;assert(op%7==3 && length==16 && cap==1024);
 uint32_t words[4]={0};for(unsigned i=0;i<16;i++)words[i/4]|=(uint32_t)packet[i]<<(8*(i%4));unsigned seq=(q+2)%15+1;
 assert(words[0]==(0x50a00008u|(seq<<16)) && words[1]==mt76_rf_a20_address(q) && words[2]==0 && words[3]==0);
 if(f->fail==op+1)return -1;
 words[0]=words[3]=0x4a000008u|(seq<<16);words[2]=(f->seed+q*13)&255;memset(reply,0,cap);
 for(unsigned i=0;i<16;i++)reply[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
 if(f->corrupt==op+1)reply[f->byte]^=1u<<f->bit;
 return f->size;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_table *s){f->s=s;const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_a20_inventory_capture(s,&io);}
int main(void){
 struct mt76_rf_table s={0};struct fake f={.size=16};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_a20_inventory_valid(&s));save=false;good=s;
 assert(f.calls==168 && f.auth==169 && s.reads==144 && s.exchanges==24 && s.observed==24 && s.acks==0 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=168;n++){
  f=(struct fake){.size=16,.fail=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==n);
  s=states[n-1];assert(mt76_a20_inventory_admit(&s,n-1));s.reads++;assert(!mt76_a20_inventory_admit(&s,n-1));
  if((n-1)%7!=3){f=(struct fake){.size=16,.bad=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete);}
  else for(unsigned b=0;b<16;b++)if(b!=8)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.size=16,.corrupt=n,.byte=b,.bit=bit};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);}
 }
 for(unsigned n=1;n<=169;n++){f=(struct fake){.size=16,.deny=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned seed=0;seed<256;seed++){f=(struct fake){.size=16,.seed=seed};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_OK && mt76_a20_inventory_valid(&s));}
 for(unsigned q=0;q<24;q++){s=good;s.values[q]^=1;assert(!mt76_a20_inventory_valid(&s));s=good;s.words[q][1]^=1;assert(!mt76_a20_inventory_valid(&s));}
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.size=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);}
 puts("A20 inventory: PASS24 fixed RF reads,no write packets,168 I/O/169 cancellation boundaries,all byte values,reply lengths/header/address/trailer/upper-bit mutations,proof changes and no replay");
}
