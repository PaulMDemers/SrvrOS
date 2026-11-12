#include <srvros/mt76_rx_window.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t pages[8][4096] __attribute__((aligned(4096)));
static struct mt76_rx_window window;
static struct mt76_rx_window_plan plan;
static unsigned posts,polls,owners,deny;
static uint64_t clock_us;
static bool complete,stopped,regress;
static bool owned(void *c,bool clean){(void)c;(void)clean;return ++owners!=deny;}
static uint64_t now(void *c){(void)c;clock_us+=100;if(regress && polls==2)return 1;return clock_us;}
static bool post(void *c,unsigned i,uint64_t trb,uint64_t physical){
 (void)c;assert(i==posts++ && trb==0x1000+16*i && physical==plan.physical[i]);return true;
}
static bool poll(void *c,struct mt76_rx_window_event *e,bool *present){
 (void)c;polls++;*present=complete;
 if(complete)*e=(struct mt76_rx_window_event){.trb=0x1000+16*(posts-1),.slot=1,.dci=9,.completion=13,.remaining=4096};
 return true;
}
static struct mt76_rx_window_io io={.owned=owned,.post=post,.poll=poll,.now=now};
static void setup(void){
 window=(struct mt76_rx_window){0};clock_us=0;posts=polls=owners=deny=0;complete=stopped=regress=false;
 plan=(struct mt76_rx_window_plan){.budget_us=10000000,.ring=0x1000,.slot=1,.dci=9,.country={'U','S'},.channel=161};
 for(unsigned i=0;i<8;i++){plan.physical[i]=0x2000+4096*i;plan.buffer[i]=pages[i];}
}
static bool finish(unsigned extra,bool pending){
 stopped=true;struct mt76_rx_window_stop stop={.dequeue=0x1000+16*(posts+extra-(pending?1:0))+1,.slot=1,.dci=9,.state=3,.cleanup_posts=extra};
 return mt76_rx5_window_finish(&window,&io,&stop);
}
int main(void){
 setup();assert(mt76_rx5_window_poll_until(&window,&plan,&io,900));
 assert(window.deadline_us==900 && window.start_us==100 && posts==1 && !window.completions && window.elapsed_us<=1000);
 assert(finish(0,true) && window.complete && !window.item[0].inspected);
 setup();assert(mt76_rx5_window_poll_until(&window,&plan,&io,900));assert(finish(1,true));
 setup();assert(mt76_rx5_window_poll_until(&window,&plan,&io,900));window.deadline_us=0;assert(!finish(1,true));
 setup();complete=true;assert(mt76_rx5_window_poll_until(&window,&plan,&io,100000));
 assert(posts==8 && window.completions==8 && window.start_us+window.elapsed_us<100000 && finish(0,false));
 unsigned calls=owners;
 for(unsigned n=1;n<=calls;n++){
  setup();deny=n;complete=true;
  bool ok=mt76_rx5_window_poll_until(&window,&plan,&io,100000);
  if(ok)ok=finish(0,false);
  assert(!ok && window.failed && window.completions<=8);
 }
 for(unsigned n=0;n<3;n++){
  setup();uint64_t deadline=n==0?0:n==1?100:10000101;
  assert(!mt76_rx5_window_poll_until(&window,&plan,&io,deadline) && !posts && !polls);
  assert(window.attempted && window.failed && !window.polled);
  struct mt76_rx_window refused=window;unsigned before_owners=owners;
  assert(!mt76_rx5_window_poll_until(&window,&plan,&io,900));
  assert(!mt76_rx5_window_poll_until(&window,&plan,&io,0));
  assert(!memcmp(&refused,&window,sizeof window) && owners==before_owners && !posts && !polls);
 }
 assert(!mt76_rx5_window_poll_until(NULL,&plan,&io,0));
 setup();regress=true;assert(!mt76_rx5_window_poll_until(&window,&plan,&io,100000) && window.failed && !window.completions);
 assert(!finish(0,true) && !window.item[0].inspected);
 setup();assert(!mt76_rx_window_poll(&window,&plan,&io) && !posts);
 puts("mt76-rx5-deadline: PASS original-clock shortening, full/quiet/pending and scratch closure, sticky invalid/extended deadline refusal, clock/owner failures and legacy isolation");
 return 0;
}
