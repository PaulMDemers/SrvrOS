/* Production provider/registry;mock transport readiness,no native authority. */
#define main mt76_net5_legacy_test_main
#include "test_mt76_net5.c"
#undef main
static unsigned ready_calls,ready_action;
static bool ready_available;
static bool ready_callback(void *v,uint64_t epoch){
 struct mock *m=v;struct mt76_net5 *s=m->s;assert(epoch==m->epoch && s->callbacks && s->io_busy);ready_calls++;
 uint8_t p[64];frame(p,sizeof p,0x19);
 assert(!net_device_send(p,sizeof p) && !net_device_poll(1,false) && !mt76_net5_service(s));
 assert(!mt76_net5_set_ready(s,epoch,NULL));
 switch(ready_action){
 case 1:assert(mt76_net5_disconnect(s,epoch) && !mt76_net5_finish_detach(s,epoch));break;
 case 2:s->epoch++;break;
 case 3:s->ready=NULL;break;
 case 4:s->io.receive=NULL;break;
 case 5:s->queue[s->head][14]^=1;break;
 case 6:s->next_token++;break;
 case 7:m->live=false;break;
 case 8:now--;break;
 case 9:assert(!mt76_net5_retire(s) && s->attached && s->io.context==m);break;
 case 10:s->ready=s->ready_guard=NULL;break;
 case 11:s->io.service=s->io_guard.service=NULL;break;
 case 12:s->identity++;break;
 default:break;
 }
 return ready_available;
}
static struct mt76_net5 *ready_make(struct mock **m){
 struct mt76_net5 *s=make(m);ready_calls=ready_action=0;ready_available=false;
 assert(!mt76_net5_set_ready(s,2,ready_callback));
 assert(mt76_net5_set_ready(s,1,ready_callback));return s;
}
static void pressure(void){
 struct mock *m;struct mt76_net5 *s=ready_make(&m);uint8_t p[64];
 for(unsigned i=0;i<4;i++){frame(p,sizeof p,i);p[14]=(uint8_t)i;assert(net_device_send(p,sizeof p));}memset(p,0,sizeof p);
 assert(!net_device_send(p,sizeof p) && !mt76_net5_set_ready(s,1,NULL));
 for(unsigned i=0;i<20;i++){
  now+=MT76_NET5_TX_US*2;
  assert(mt76_net5_service(s));assert(!net_device_poll(32,false));
  assert(s->state==MT76_NET5_UP && s->count==4 && !s->pending && !s->next_token && !s->tx_started && !s->submitted && !s->dropped && !s->faults && !m->submit_calls);
  for(unsigned j=0;j<4;j++)assert(s->queue[(s->head+j)%4][14]==j);
 }
 assert(m->service_calls>=40 && ready_calls>=40 && m->receive_calls==20);
 ready_available=true;
 for(unsigned i=0;i<4;i++){
  assert(mt76_net5_service(s) && s->pending && m->submit_calls==i+1 && m->submitted[14]==i && m->token==i+1);
  assert(!mt76_net5_set_ready(s,1,NULL));assert(mt76_net5_tx_complete(s,1,m->token,true));
 }
 assert(!s->count && !s->pending && s->completed==4 && s->next_token==4 && !s->dropped && !s->faults);
 assert(mt76_net5_set_ready(s,1,NULL));unsigned calls=ready_calls;frame(p,sizeof p,0x77);
 assert(net_device_send(p,sizeof p) && mt76_net5_service(s) && ready_calls==calls && mt76_net5_tx_complete(s,1,m->token,true));
 assert(mt76_net5_disconnect(s,1));m->quiet=true;assert(mt76_net5_finish_detach(s,1));
 assert(!s->ready && !s->ready_guard && !s->io.context && !s->io_guard.context);
 gone(s,m);
}
static void callback_faults(void){
 for(unsigned action=1;action<=12;action++){
  struct mock *m;struct mt76_net5 *s=ready_make(&m);uint8_t p[64];frame(p,sizeof p,0x43);assert(net_device_send(p,sizeof p));ready_action=action;ready_available=true;
  assert(!mt76_net5_service(s) && !m->submit_calls && !s->pending && s->attached && s->io_guard.context==m);
  assert(s->state==((action==1 || action==9)?MT76_NET5_CLOSING:MT76_NET5_FAULT));
  /* Test-only repair permits fixture teardown;production corrupt objects stay
   * pinned and cannot invoke changed callbacks or release the original owner. */
  if(action==2)s->epoch=m->epoch;
  if(action==3){assert(!mt76_net5_finish_detach(s,1));s->ready=s->ready_guard;}
  if(action==4){assert(!mt76_net5_finish_detach(s,1));s->io=s->io_guard;}
  if(action==12)s->identity=(uintptr_t)s;
  gone(s,m);
 }
}
static void pending_deadline(void){
 struct mock *m;struct mt76_net5 *s=ready_make(&m);uint8_t p[64];frame(p,sizeof p,0x65);ready_available=true;
 assert(net_device_send(p,sizeof p) && mt76_net5_service(s));unsigned calls=ready_calls;uint64_t token=m->token;
 ready_available=false;now+=MT76_NET5_TX_US;
 assert(!mt76_net5_service(s) && s->state==MT76_NET5_FAULT && s->pending && s->pending_token==token && ready_calls==calls && s->next_token==1);
 gone(s,m);
}
int main(void){
 assert(!mt76_net5_legacy_test_main());pressure();callback_faults();pending_deadline();
 puts("mt76-ready5: PASS optional pre-pop readiness,4 copied FIFO frames held across long waits,service/RX progress,no token/PN/deadline/drop/fault while busy,default compatibility,reentry/epoch/table/queue/clock/ownership/close refusal,detach scrub and unchanged pending deadline;mock transport,no native acceptance");return 0;
}
