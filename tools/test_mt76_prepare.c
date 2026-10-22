#include <assert.h>
#include <stdio.h>
#include <srvros/mt76_bringup.h>
struct op {char kind;uint32_t address,value;};
static const struct op expected[]={
 {'r',0x730,0},{'r',0x80,0x5a120000},{'w',0x80,0x5a120003},{'d',20,0},{'r',0x80,0x5a120003},{'r',0x20,0xc00000},{'r',0x238,0},{'w',0x238,0xc00000},{'w',0x1004,0x20},{'r',0x1004,0x20},
 {'r',0x238,0xc00000},{'w',0x238,0xc00020},{'x',0,0},{'d',5000,0},
 {'w',0x800,1},{'w',0x9a0,0x400230},{'w',0x9a4,1},{'w',0x9c4,0x44},
 {'w',0xa6c,3},{'r',0x238,0xc00020},{'w',0x238,0xc10020},{'w',0x238,0xc00020},{'r',0x1004,0x20}
};
struct fake {unsigned calls,fail,bad_read;uint32_t bad_value;bool clock_wait;};
static bool operation(struct fake *f,char kind,uint32_t address,uint32_t *value) {
 unsigned i=f->calls++;
 if(f->clock_wait && i>=5) {
  assert(address==((i-5)%2?20u:0x20u) && kind==((i-5)%2?'d':'r'));
  if(kind=='r')*value=0;
  return f->calls!=f->fail;
 }
 assert(i<sizeof expected/sizeof expected[0]);
 assert(kind==expected[i].kind && address==expected[i].address);
 if(kind=='w')assert(*value==expected[i].value);
 if(kind=='r')*value=f->bad_read==i+1?f->bad_value:expected[i].value;
 return f->calls!=f->fail;
}
static bool readreg(void *p,uint32_t a,uint32_t *v){return operation(p,'r',a,v);}
static bool writereg(void *p,uint32_t a,uint32_t v){return operation(p,'w',a,&v);}
static bool reset(void *p){return operation(p,'x',0,0);}
static bool delay(void *p,uint32_t us){return operation(p,'d',us,0);}
static enum mt76_upload_status run(struct fake *f,struct mt76_prepare_session *s) {
 struct mt76_prepare_io io={{f,readreg,writereg,delay},reset};
 struct mt76_identity id={0x76100002,0x76502000};
 return mt76_prepare_firmware(s,&io,&id);
}
int main(void) {
 struct fake f={0};struct mt76_prepare_session s={0};
 assert(run(&f,&s)==MT76_UPLOAD_OK && s.prepared && f.calls==23);
 assert(run(&f,&s)==MT76_UPLOAD_REFUSED && f.calls==23);
 for(unsigned fail=1;fail<=23;fail++) {
  f=(struct fake){.fail=fail};s=(struct mt76_prepare_session){0};
  assert(run(&f,&s)!=MT76_UPLOAD_OK && !s.prepared && f.calls==fail);
  assert(run(&f,&s)==MT76_UPLOAD_REFUSED && f.calls==fail);
 }
 for(unsigned i=0;i<6;i++) {
  unsigned call=(unsigned[]){1,7,10,11,20,23}[i];
  for(unsigned n=0;n<2;n++) {
   f=(struct fake){.bad_read=call,.bad_value=n?0xffffffffu:(call==1?1:0xc0000000)};
   s=(struct mt76_prepare_session){0};
   assert(run(&f,&s)==MT76_UPLOAD_REFUSED && !s.prepared && f.calls==call);
  }
 }
 for(unsigned i=0;i<2;i++) {
  f=(struct fake){.bad_read=i?23:10,.bad_value=0x2c};s=(struct mt76_prepare_session){0};
  assert(run(&f,&s)==MT76_UPLOAD_REFUSED && !s.prepared && f.calls==f.bad_read);
 }
 for(unsigned i=0;i<5;i++) {
  f=(struct fake){.bad_read=(unsigned[]){2,2,5,5,6}[i],
      .bad_value=(uint32_t[]){0xffffffffu,0x5a120004,0x5a120001,0x5b120003,0xffffffffu}[i]};
  s=(struct mt76_prepare_session){0};
  assert(run(&f,&s)==MT76_UPLOAD_REFUSED && !s.prepared && f.calls==f.bad_read);
 }
 f=(struct fake){.bad_read=5,.bad_value=0x5a12ff03};s=(struct mt76_prepare_session){0};
 assert(run(&f,&s)==MT76_UPLOAD_OK); /* Live GPIO input changes allowed. */
 f=(struct fake){.clock_wait=true};s=(struct mt76_prepare_session){0};
 assert(run(&f,&s)==MT76_UPLOAD_TIMEOUT && f.calls==205 && !s.prepared);
 assert(run(&f,&s)==MT76_UPLOAD_REFUSED && f.calls==205);
 f=(struct fake){.clock_wait=true,.fail=7};s=(struct mt76_prepare_session){0};
 assert(run(&f,&s)==MT76_UPLOAD_CANCELLED && !s.prepared && f.calls==7);
 puts("mt76-prepare: PASS exact reset/DMA ordering, every-operation failure, no retry, already-running/unreadable/busy refusal");
}
