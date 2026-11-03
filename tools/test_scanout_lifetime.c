#include <srvros/scanout_lifetime.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct scanout_profile profile(void){return (struct scanout_profile){.physical={0xb0000000,0x1000000,0x2000000},.bytes=6144*900,.width=1440,.height=900,.pitch=6144,.mode=SCANOUT_SHADOW,.assured=SCANOUT_RAM_PIN};}
static struct scanout_pool pool;
static struct scanout_lifetime o;
static uint64_t time;
static void start(void){o=(struct scanout_lifetime){0};struct scanout_profile p=profile();assert(scanout_open(&o,&pool,&p,42,++time));assert(scanout_invariant(&o));}
static struct scanout_token copy(void){struct scanout_token t;assert(scanout_copy_begin(&o,42,&t));assert(t.target!=o.front);assert(scanout_copy_end(&o,&t,true));return t;}
static struct scanout_token request(void){struct scanout_token t;assert(scanout_request(&o,42,++time,&t));assert(scanout_invariant(&o));return t;}
static void complete(struct scanout_token t){assert(scanout_issued(&o,&t,SCANOUT_ISSUED,++time));assert(scanout_complete(&o,&t,t.target,SCANOUT_SHADOW,++time));assert(scanout_invariant(&o));}
static void release(void){uint64_t p;assert(scanout_take_release(&o,1,&p) && p==profile().physical[1]);assert(!scanout_take_release(&o,1,&p));assert(scanout_take_release(&o,2,&p));assert(!scanout_take_release(&o,0,&p));assert(scanout_invariant(&o));}
static void close_all(void){assert(scanout_close(&o,42,o.epoch));if(o.state!=SCANOUT_RETIRED)complete(request());assert(o.state==SCANOUT_RETIRED);release();}
int main(void){
 start();assert(!scanout_request(&o,42,++time,&(struct scanout_token){0}));
 for(unsigned i=0;i<20;i++){copy();struct scanout_token t=request();assert(!scanout_copy_begin(&o,42,&(struct scanout_token){0}));assert(!scanout_take_release(&o,1,&time));complete(t);assert(o.front==t.target);}
 close_all();uint64_t epoch=o.epoch;struct scanout_profile p=profile();assert(scanout_open(&o,&pool,&p,43,++time) && o.epoch>epoch);assert(!scanout_close(&o,42,epoch));assert(scanout_close(&o,43,o.epoch));release();
 /* Cancel during copy and before any publication: CPU work must drain first. */
 start();struct scanout_token c;assert(scanout_copy_begin(&o,42,&c));assert(scanout_close(&o,42,o.epoch));assert(o.state==SCANOUT_CLOSING);assert(!scanout_take_release(&o,1,&time));assert(scanout_copy_end(&o,&c,true));assert(o.state==SCANOUT_RETIRED);release();
 start();copy();assert(scanout_close(&o,42,o.epoch));release();
 start();copy();complete(request());assert(scanout_copy_begin(&o,42,&c));assert(scanout_close(&o,42,o.epoch));assert(scanout_copy_end(&o,&c,true));assert(o.state==SCANOUT_CLOSING && o.front==1);complete(request());assert(o.state==SCANOUT_RETIRED);release();
 /* Cancel while submission is being issued and while the flip is pending. */
 for(unsigned phase=0;phase<2;phase++){
  start();copy();struct scanout_token t=request();if(phase)assert(scanout_issued(&o,&t,SCANOUT_ISSUED,++time));
  assert(scanout_close(&o,42,o.epoch));assert(!scanout_take_release(&o,1,&time));if(!phase)assert(scanout_issued(&o,&t,SCANOUT_ISSUED,++time));
  assert(scanout_complete(&o,&t,t.target,SCANOUT_SHADOW,++time));assert(o.state==SCANOUT_CLOSING && o.front==1);complete(request());assert(o.state==SCANOUT_RETIRED);release();
 }
 start();copy();struct scanout_token stale=request();assert(scanout_issued(&o,&stale,SCANOUT_REJECTED,++time));assert(!o.phase && !o.front);struct scanout_token next=request();assert(next.serial!=stale.serial);assert(scanout_issued(&o,&next,SCANOUT_ISSUED,++time));assert(!scanout_complete(&o,&stale,1,SCANOUT_SHADOW,++time));assert(!scanout_complete(&o,&next,2,SCANOUT_SHADOW,++time));assert(!scanout_complete(&o,&next,1,SCANOUT_HARDWARE,++time));assert(scanout_complete(&o,&next,1,SCANOUT_SHADOW,++time));assert(!scanout_complete(&o,&next,1,SCANOUT_SHADOW,++time));close_all();
 start();copy();struct scanout_token rejected=request();assert(scanout_close(&o,42,o.epoch));assert(scanout_issued(&o,&rejected,SCANOUT_REJECTED,++time));assert(o.state==SCANOUT_RETIRED);release();struct scanout_pool reset_pool={0};p=profile();assert(!scanout_open(&o,&reset_pool,&p,42,++time));
 /* An uncertain write, missing/late completion, clock rollback or changed
  * route retains both allocations, including the never-front one. */
 for(unsigned why=0;why<5;why++){
  start();copy();struct scanout_token t=request();
  if(why==0)assert(!scanout_issued(&o,&t,SCANOUT_UNCERTAIN,++time));
  else {assert(scanout_issued(&o,&t,SCANOUT_ISSUED,++time));if(why==1)assert(!scanout_tick(&o,o.deadline_us));if(why==2)assert(!scanout_complete(&o,&t,1,SCANOUT_SHADOW,o.deadline_us+1));if(why==3)assert(!scanout_tick(&o,0));if(why==4)assert(scanout_poison(&o,SCANOUT_CHANGED));}
  assert(o.state==SCANOUT_FAULT && scanout_invariant(&o));uint64_t pa;assert(!scanout_take_release(&o,1,&pa) && !scanout_take_release(&o,2,&pa));assert(!scanout_close(&o,42,o.epoch));assert(!scanout_open(&o,&pool,&p,42,++time));
 }
 for(unsigned bit=0;bit<6;bit++){o=(struct scanout_lifetime){0};p=profile();p.mode=SCANOUT_HARDWARE;p.assured=SCANOUT_ASSURED_ALL&~(1u<<bit);assert(!scanout_open(&o,&pool,&p,42,++time));}
 for(unsigned bad=0;bad<8;bad++){o=(struct scanout_lifetime){0};p=profile();switch(bad){case 0:p.physical[1]=p.physical[0];break;case 1:p.physical[2]=p.physical[1]+4096;break;case 2:p.physical[1]++;break;case 3:p.physical[1]=UINT64_MAX-4095;break;case 4:p.bytes++;break;case 5:p.pitch++;break;case 6:p.assured=0;break;case 7:p.mode=3;break;}assert(!scanout_open(&o,&pool,&p,42,++time));}
 start();o.serial=UINT64_MAX;assert(!scanout_copy_begin(&o,42,&c));assert(scanout_close(&o,42,o.epoch));release();pool.epoch=UINT64_MAX;assert(!scanout_open(&o,&pool,&p,42,++time));pool.epoch=0;
 /* Deterministic interleavings: invalid/stale operations cannot release a
  * protected buffer, and every exposed CPU-write target stays inactive. */
 unsigned seed=7;for(unsigned run=0;run<1000;run++){
  start();struct scanout_token token={0},ct={0};
  for(unsigned step=0;step<100;step++){
   seed=seed*1664525u+1013904223u;unsigned op=seed%9;
   if(op==0 && scanout_copy_begin(&o,42,&ct))assert(ct.target!=o.front && !o.phase);
   if(op==1)(void)scanout_copy_end(&o,&ct,true);
   if(op==2)(void)scanout_request(&o,42,++time,&token);
   if(op==3)(void)scanout_issued(&o,&token,SCANOUT_ISSUED,++time);
   if(op==4)(void)scanout_complete(&o,&token,token.target,SCANOUT_SHADOW,++time);
   if(op==5)(void)scanout_close(&o,42,o.epoch);
   if(op==6)(void)scanout_tick(&o,++time);
   if(op==7){uint64_t pa;bool freed=scanout_take_release(&o,1,&pa);if(freed)assert(o.state==SCANOUT_RETIRED && !o.front && !o.phase && o.copy==SCANOUT_NONE);}
   if(op==8){struct scanout_token invalid=token;invalid.epoch++;assert(!scanout_complete(&o,&invalid,invalid.target,SCANOUT_SHADOW,time));}
   assert(scanout_invariant(&o));
  }
 }
 puts("scanout-lifetime: PASS inactive writes, completion identity, cancellation/drain/restore, retained faults, epochs and 100000 interleavings");
}
