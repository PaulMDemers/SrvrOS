#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
struct fake {unsigned op,auth,fail,deny,bad;bool final_bad;struct mt76_gain_inventory *s;};
static bool auth(void *p){struct fake *f=p;++f->auth;if(f->final_bad && f->auth==28)f->s->values[0]=UINT32_MAX;return f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->op++;assert(a==mt76_gain_inventory_address(op));unsigned n=op<3?op:op>=24?op-24:3;*v=n==0?0:n==1?0xf000f200:n==2?0x40000050:op+100;if(f->bad==op+1)*v=0xffffffff;return f->fail!=op+1;}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_gain_inventory *s){f->s=s;const struct mt76_mcu_timing_io io={f,auth,read_reg,0};return mt76_gain_inventory_capture(s,&io);}
int main(void){struct fake f={0};struct mt76_gain_inventory s={0};assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==27 && f.op==27 && f.auth==28 && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=0;op<27;op++){f=(struct fake){.fail=op+1};s=(struct mt76_gain_inventory){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.op==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);f=(struct fake){.bad=op+1};s=(struct mt76_gain_inventory){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.op==op+1);}
 for(unsigned n=1;n<=28;n++){f=(struct fake){.deny=n};s=(struct mt76_gain_inventory){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.op==n-1);}
 f=(struct fake){.final_bad=true};s=(struct mt76_gain_inventory){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && s.reads==27);
 assert(mt76_gain_inventory_target(21)==UINT32_MAX && mt76_gain_inventory_address(27)==UINT32_MAX);puts("gain inventory: PASS27 fixed reads, every failure/cancellation/all-ones boundary and no replay");}
