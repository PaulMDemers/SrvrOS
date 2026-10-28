#include <srvros/mt76_radio.h>
#include <assert.h>
#include <stdio.h>
struct fake {unsigned ops,auth,fail,deny,zeros,core_reads,bad;uint32_t bad_value;};
static bool authorize(void *context){struct fake *f=context;return ++f->auth!=f->deny;}
static bool read_reg(void *context,uint32_t address,uint32_t *value){
 struct fake *f=context;unsigned op=++f->ops;
 unsigned polls=f->zeros<20?f->zeros+1:20;
 uint32_t expected=op==1?0x1004:op==2?0x1200:op==3?0x400:op<=3+polls?0x2000:op==4+polls?0x1004:0x1200;
 assert(address==expected);
 if(op==f->fail)return false;
 *value=address==0x400?0x80c00:address==0x2000?(f->core_reads++<f->zeros?0:0x12345678):0;
 if(op==f->bad)*value=f->bad_value;
 return true;
}
static enum mt76_radio_status run(struct fake *f,struct mt76_bbp_probe *s){
 struct mt76_bbp_io io={f,authorize,read_reg};return mt76_bbp_probe_ready(s,&io);
}
int main(void){
 struct fake f={0};struct mt76_bbp_probe s={0};
 assert(run(&f,&s)==MT76_RADIO_OK && s.complete && s.reads==6 && s.core_reads==1 && s.core==0x12345678 && s.pbf==0x80c00);
 assert(run(&f,&s)==MT76_RADIO_REFUSED && f.ops==6);
 for(unsigned n=1;n<=6;n++){
  f=(struct fake){.fail=n};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_IO && !s.complete && f.ops==n);
  assert(run(&f,&s)==MT76_RADIO_REFUSED && f.ops==n);
  f=(struct fake){.bad=n,.bad_value=UINT32_MAX};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_STATE && !s.complete && f.ops==n);
 }
 for(unsigned n=1;n<=7;n++){
  f=(struct fake){.deny=n};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_CANCELLED && !s.complete && f.auth==n);
 }
 const unsigned boundaries[]={1,2,5,6};
 for(unsigned n=0;n<4;n++){
  f=(struct fake){.bad=boundaries[n],.bad_value=1};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_STATE && !s.complete && f.ops==boundaries[n]);
 }
 for(unsigned n=0;n<20;n++){
  f=(struct fake){.zeros=n};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_OK && s.complete && s.reads==6+n && s.core_reads==n+1);
 }
 f=(struct fake){.zeros=20};s=(struct mt76_bbp_probe){0};
 assert(run(&f,&s)==MT76_RADIO_TIMEOUT && !s.complete && s.reads==23 && s.core_reads==20);
 assert(run(&f,&s)==MT76_RADIO_REFUSED && f.ops==23);
 for(unsigned n=4;n<=23;n++){
  f=(struct fake){.zeros=20,.deny=n};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_CANCELLED && !s.complete);
  f=(struct fake){.zeros=20,.fail=n};s=(struct mt76_bbp_probe){0};
  assert(run(&f,&s)==MT76_RADIO_IO && !s.complete && f.ops==n);
 }
 assert(mt76_bbp_probe_ready(NULL,NULL)==MT76_RADIO_REFUSED);
 puts("mt76 BBP: PASS bounded readiness polling, exact reads, all faults/cancellations, MAC idle boundaries and no retry");
}
