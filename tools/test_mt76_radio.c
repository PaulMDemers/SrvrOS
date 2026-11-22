#include <srvros/mt76_radio.h>
#include <assert.h>
#include <stdio.h>
struct fake { unsigned ops,fail,deny,delays,writes; uint32_t ctrl; bool busy,missing,bad_release; };
static bool authorize(void *p) { struct fake *f=p;return !f->deny || f->ops<f->deny; }
static bool read_reg(void *p,uint32_t reg,uint32_t *value) {
    struct fake *f=p;if(++f->ops==f->fail)return false;
    if(reg==0x1000)*value=0x76502000;
    else if(reg==0x1004)*value=f->ctrl;
    else { assert(reg==0x208 || reg==0x1200);*value=f->missing?UINT32_MAX:f->busy?0x0f:0; }
    return true;
}
static bool write_reg(void *p,uint32_t reg,uint32_t value) {
    struct fake *f=p;assert(reg==0x1004);assert(value==3 || value==0);
    assert((value&12)==0);assert(value==(f->writes?0u:3u));
    if(++f->ops==f->fail)return false;
    if(!value)assert(f->delays==200000);
    f->ctrl=f->bad_release && !value?3:value;f->writes++;return true;
}
static bool delay(void *p,uint32_t us) {
    struct fake *f=p;assert(us==1000 || us==5000);
    if(++f->ops==f->fail)return false;
    f->delays+=us;return true;
}
static enum mt76_radio_status run(struct fake *f,struct mt76_radio_prepare *s) {
    const struct mt76_radio_io io={{f,read_reg,write_reg,delay},authorize};
    const struct mt76_identity identity={0x76100002,0x76502000};
    return mt76_radio_prepare_reset(s,&io,&identity);
}

struct clock_fake {unsigned reads,writes,auth,deny,fail_read,bad_read;bool fail_write;uint32_t bad;};
static bool clock_auth(void *p){struct clock_fake *f=p;return ++f->auth!=f->deny;}
static bool clock_read_fake(void *p,uint32_t reg,uint32_t *value){
 struct clock_fake *f=p;const uint32_t addresses[]={0x1004,0x1200,0x400,0x400,0x1004,0x1200};
 assert(f->reads<6 && reg==addresses[f->reads]);f->reads++;
 *value=reg==0x400?(f->writes?0x80c00:0x80000):0;
 if(f->bad_read==f->reads)*value=f->bad;
 return f->reads!=f->fail_read;
}
static bool clock_write_fake(void *p,uint32_t reg,uint32_t value){
 struct clock_fake *f=p;assert(reg==0x400 && value==0x80c00 && f->reads==3 && !f->writes);
 f->writes++;return !f->fail_write;
}
static enum mt76_radio_status clock_run(struct clock_fake *f,struct mt76_clock_probe *s){
 const struct mt76_clock_io io={{f,clock_auth,clock_read_fake},clock_write_fake};return mt76_clock_enable(s,&io);
}
static void test_clock(void){
 struct clock_fake f={0};struct mt76_clock_probe s={0};
 assert(clock_run(&f,&s)==MT76_RADIO_OK && s.complete && s.written && s.write_attempted && s.reads==6 && f.writes==1 && f.auth==8);
 assert(s.pbf_before==0x80000 && s.pbf_after==0x80c00 && !s.mac_before && !s.mac_after);
 assert(clock_run(&f,&s)==MT76_RADIO_REFUSED && f.writes==1);
 for(unsigned n=1;n<=6;n++){
  f=(struct clock_fake){.fail_read=n};s=(struct mt76_clock_probe){0};assert(clock_run(&f,&s)==MT76_RADIO_IO && !s.complete && f.writes==(n>3));
  f=(struct clock_fake){.bad_read=n,.bad=UINT32_MAX};s=(struct mt76_clock_probe){0};assert(clock_run(&f,&s)==MT76_RADIO_STATE && !s.complete);
 }
 for(unsigned n=1;n<=8;n++){
  f=(struct clock_fake){.deny=n};s=(struct mt76_clock_probe){0};assert(clock_run(&f,&s)==MT76_RADIO_CANCELLED && !s.complete);
 }
 for(unsigned n=1;n<=6;n++){
  f=(struct clock_fake){.bad_read=n,.bad=n==3?0x80c00:n==4?0x80000:1};s=(struct mt76_clock_probe){0};
  assert(clock_run(&f,&s)==MT76_RADIO_STATE && !s.complete && f.writes==(n>3));
 }
 f=(struct clock_fake){.fail_write=true};s=(struct mt76_clock_probe){0};assert(clock_run(&f,&s)==MT76_RADIO_IO && s.write_attempted && !s.written && f.reads==3);
 assert(clock_run(&f,&s)==MT76_RADIO_REFUSED);
 assert(mt76_clock_enable(NULL,NULL)==MT76_RADIO_REFUSED);
 puts("mt76 clock: PASS exact baseline/transition, every failure/cancel boundary, no retry, disabled/idle MAC");
}
int main(void) {
    test_clock();
    struct fake f={.ctrl=0x20};struct mt76_radio_prepare s={0};
    assert(run(&f,&s)==MT76_RADIO_OK);
    assert(s.prepared && s.reset_released && f.writes==2 && s.delay_us==200000 && f.ctrl==0);
    unsigned ops=f.ops;
    assert(run(&f,&s)==MT76_RADIO_REFUSED && f.ops==ops);
    for(unsigned fail=1;fail<=ops;fail++) {
        f=(struct fake){.ctrl=0x20,.fail=fail};s=(struct mt76_radio_prepare){0};
        assert(run(&f,&s)!=MT76_RADIO_OK && !s.prepared && f.ops==fail);
        assert(run(&f,&s)==MT76_RADIO_REFUSED && f.ops==fail);
    }
    for(unsigned deny=1;deny<ops;deny++) {
        f=(struct fake){.ctrl=0x20,.deny=deny};s=(struct mt76_radio_prepare){0};
        assert(run(&f,&s)==MT76_RADIO_CANCELLED && !s.prepared && f.ops==deny);
    }
    f=(struct fake){.ctrl=0x20,.busy=true};s=(struct mt76_radio_prepare){0};
    assert(run(&f,&s)==MT76_RADIO_TIMEOUT && !f.writes && s.delay_us==100000);
    f=(struct fake){.ctrl=0x20,.missing=true};s=(struct mt76_radio_prepare){0};
    assert(run(&f,&s)==MT76_RADIO_STATE && !f.writes);
    f=(struct fake){.ctrl=0x2c};s=(struct mt76_radio_prepare){0};
    assert(run(&f,&s)==MT76_RADIO_STATE && !f.writes);
    f=(struct fake){.ctrl=0x20,.bad_release=true};s=(struct mt76_radio_prepare){0};
    assert(run(&f,&s)==MT76_RADIO_STATE && !s.prepared && !s.reset_released);
    struct mt76_radio_io io={{&f,read_reg,write_reg,delay},authorize};
    struct mt76_identity id={0x76100002,0};s=(struct mt76_radio_prepare){0};
    assert(mt76_radio_prepare_reset(&s,&io,&id)==MT76_RADIO_REFUSED && !s.attempted);
    puts("mt76 radio reset prerequisite: PASS success, all operation failures, cancellation, idle timeout and refusal");
}
