/* Real provider registry;mock nonblocking USB/CCMP lifetime,not native proof. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/mt76_net5.h>
#define SRVROS_NET_DEVICE_HOST_TEST
#include "../kernel/src/net_device.c"
static uint64_t now=1000;
uint64_t timer_monotonic_us(void){return now;}
static unsigned notifications,deliveries;
void net_notify_rx(void){notifications++;}
static uint8_t station[6]={2,1,2,3,4,5};
static void frame(uint8_t *p,unsigned n,unsigned value){
 memset(p,value,n);memset(p,255,6);memcpy(p+6,station,6);p[12]=8;p[13]=0;
}
struct mock {
 struct mt76_net5 *s;uint64_t epoch,token;unsigned submit_calls,service_calls,owner_calls,retired_calls,receive_calls;
 bool live,quiet,service_ok,present,complete_inline,close_owned,close_submit,close_receive,close_retired,retire_submit;
 bool lose_after_submit,lose_receive,fail_receive,repeat_rx;
 enum mt76_net5_submit result;size_t rx_size;uint16_t size;
 uint8_t submitted[1514],rx[1514];
};
static struct mock *delivery_context;
static unsigned delivery_action;
void net_handle_ethernet_frame(const uint8_t *p,uint16_t n){
 assert(delivery_context && n>=14 && n<=1514);deliveries++;
 uint8_t original[1514];memcpy(original,p,n);
 if(delivery_action==1){
  uint8_t reply[64];frame(reply,sizeof reply,0x51);assert(net_device_send(reply,sizeof reply));
  memset(reply,0,sizeof reply);assert(!net_device_poll(32,false));
 }else if(delivery_action==2 || delivery_action==3){
  struct mt76_net5 *s=delivery_context->s;
  assert(mt76_net5_disconnect(s,delivery_context->epoch));
  assert(!mt76_net5_finish_detach(s,delivery_context->epoch));
  if(delivery_action==3)assert(!mt76_net5_retire(s));
  assert(!memcmp(original,p,n)); /* Borrow remains immutable until return. */
 }
 assert(!memcmp(original,p,n));
}
static bool mock_owned(void *v,uint64_t epoch){
 struct mock *m=v;assert(epoch==m->epoch);m->owner_calls++;
 if(m->close_owned){m->close_owned=false;assert(mt76_net5_disconnect(m->s,epoch));}
 return m->live;
}
static enum mt76_net5_submit mock_submit(void *v,uint64_t epoch,uint64_t token,const uint8_t *p,uint16_t n){
 struct mock *m=v;assert(epoch==m->epoch && token && n>=14 && n<=1514 && m->live);
 m->submit_calls++;m->token=token;m->size=n;memcpy(m->submitted,p,n);
 /* Cannot recursively publish or enter the transport from its callback. */
 assert(!net_device_send(p,n) && !mt76_net5_service(m->s) && !net_device_poll(1,false));
 if(m->close_submit || m->retire_submit){
  assert(mt76_net5_disconnect(m->s,epoch));assert(!mt76_net5_finish_detach(m->s,epoch));
  if(m->retire_submit)assert(!mt76_net5_retire(m->s));
  assert(!memcmp(m->submitted,p,n));
 }
 if(m->complete_inline)assert(mt76_net5_tx_complete(m->s,epoch,token,true));
 if(m->lose_after_submit)m->live=false;
 return m->result;
}
static bool mock_service(void *v,uint64_t epoch){struct mock *m=v;assert(epoch==m->epoch);m->service_calls++;return m->service_ok;}
static bool mock_receive(void *v,uint64_t epoch,uint8_t *out,size_t cap,size_t *n,bool *present){
 struct mock *m=v;assert(epoch==m->epoch && cap==1514);m->receive_calls++;
 if(m->close_receive){m->close_receive=false;assert(mt76_net5_disconnect(m->s,epoch));assert(!mt76_net5_finish_detach(m->s,epoch));}
 if(m->present){if(m->rx_size<=cap)memcpy(out,m->rx,m->rx_size);*n=m->rx_size;*present=true;if(!m->repeat_rx)m->present=false;}
 if(m->lose_receive)m->live=false;
 return !m->fail_receive;
}
static bool mock_retired(void *v,uint64_t epoch){
 struct mock *m=v;assert(epoch==m->epoch);m->retired_calls++;
 if(m->close_retired)assert(!mt76_net5_finish_detach(m->s,epoch));
 return m->quiet;
}
static struct mt76_net5_io io(struct mock *m){return (struct mt76_net5_io){m,mock_owned,mock_submit,mock_service,mock_receive,mock_retired};}
static void reset_registry(void){
 memset(providers,0,sizeof providers);memset(identities,0,sizeof identities);memset(names,0,sizeof names);
 memset(references,0,sizeof references);memset(retiring,0,sizeof retiring);provider_count=0;selected=NULL;sealed=false;
 now=1000;notifications=deliveries=delivery_action=0;delivery_context=NULL;
}
static struct mt76_net5 *make(struct mock **out){
 reset_registry();struct mt76_net5 *s=calloc(1,sizeof *s);struct mock *m=calloc(1,sizeof *m);assert(s && m);
 *m=(struct mock){.s=s,.epoch=1,.live=true,.service_ok=true,.result=MT76_NET5_QUEUED};
 assert(mt76_net5_init(s,station));assert(net_device_select());struct net_device_link link;
 assert(net_device_link(&link) && !link.up && !link.speed_mbps && !link.duplex_known);
 assert(!net_device_rx_wake_ready());
 struct mt76_net5_io ops=io(m);assert(mt76_net5_attach(s,1,&ops));
 memset(&ops,0,sizeof ops); /* Connection owns its callback table copy. */
 assert(net_device_link(&link) && link.up && !link.speed_mbps && !link.duplex_known);
 delivery_context=m;*out=m;return s;
}
static void gone(struct mt76_net5 *s,struct mock *m){
 m->quiet=true;assert(mt76_net5_retire(s));unsigned calls=m->owner_calls+m->retired_calls+m->service_calls+m->receive_calls;
 free(m);free(s); /* Registry owns no callable reference to either allocation. */
 uint8_t p[64];frame(p,sizeof p,0x64);struct net_device_link link;struct srv_net_adapter_info inventory[2];
 assert(!net_device_send(p,sizeof p) && !net_device_poll(32,false) && !net_device_link(&link));
 assert(net_device_inventory(inventory,2)==1 && !strcmp(inventory[0].driver,"mt76-ccmp"));
 (void)calls;
}
static void test_queue(void){
 struct mock *m;struct mt76_net5 *s=make(&m);uint8_t p[1514];frame(p,sizeof p,0x42);
 for(unsigned i=0;i<MT76_NET5_QUEUE;i++){p[14]=(uint8_t)i;assert(net_device_send(p,sizeof p));}
 assert(!net_device_send(p,sizeof p) && !m->submit_calls && s->enqueued==4);
 memset(p,0,sizeof p);assert(mt76_net5_service(s) && s->pending && m->submit_calls==1 && m->submitted[14]==0);
 assert(!s->loan_size && !memcmp(s->loan,p,sizeof p));
 assert(mt76_net5_service(s) && m->submit_calls==1); /* One pending transfer. */
 for(unsigned i=0;i<4;i++){
  assert(m->submitted[14]==i);assert(mt76_net5_tx_complete(s,1,m->token,true));
  assert(mt76_net5_service(s));
 }
 assert(!s->pending && !s->count && s->completed==4 && s->next_token==4);
 assert(!memcmp(s->queue[0],p,sizeof p));
 for(unsigned i=0;i<1000;i++){
  frame(p,64,(uint8_t)i);assert(net_device_send(p,64));assert(mt76_net5_service(s));
  assert(m->submitted[14]==(uint8_t)i && m->token==i+5);assert(mt76_net5_tx_complete(s,1,m->token,true));
 }
 assert(s->next_token==1004 && s->completed==1004);gone(s,m);
}
static void test_receive(void){
 for(unsigned action=0;action<=3;action++){
  struct mock *m;struct mt76_net5 *s=make(&m);delivery_action=action;
  frame(m->rx,64,0x84);m->rx_size=64;m->present=true;
  assert(net_device_poll(32,false)==1 && deliveries==1 && s->received==1);
  uint8_t zeros[1514]={0};assert(!memcmp(s->rx,zeros,sizeof zeros));
  if(action==1){assert(m->submit_calls==1 && m->size==64 && m->submitted[14]==0x51);assert(mt76_net5_tx_complete(s,1,m->token,true));}
  if(action>=2)assert(s->state==MT76_NET5_CLOSING && s->attached);
  gone(s,m);
 }
 for(unsigned kind=0;kind<5;kind++){
  struct mock *m;struct mt76_net5 *s=make(&m);frame(m->rx,64,0x84);m->present=true;
  m->rx_size=kind==0?1515:kind==1?13:64;m->fail_receive=kind==2;m->close_receive=kind==3;m->lose_receive=kind==4;
  assert(!net_device_poll(32,false) && !deliveries);assert(s->state==(kind==3?MT76_NET5_CLOSING:MT76_NET5_FAULT));gone(s,m);
 }
 struct mock *m;struct mt76_net5 *s=make(&m);frame(m->rx,64,0x84);m->rx_size=64;m->present=m->repeat_rx=true;
 unsigned calls=m->owner_calls+m->service_calls+m->receive_calls;
 struct srv_net_adapter_info rows[2];assert(net_device_inventory(rows,2)==1);
 assert(calls==m->owner_calls+m->service_calls+m->receive_calls); /* Inventory is passive. */
 assert(net_device_poll(UINT64_MAX,false)==32 && deliveries==32 && m->receive_calls==32);
 assert(s->ops.poll(s,UINT64_MAX,false)==32 && deliveries==64 && m->receive_calls==64);gone(s,m);
}
static void test_detach(void){
 struct mock *m;struct mt76_net5 *s=make(&m);uint8_t p[64];frame(p,sizeof p,0x71);
 assert(net_device_send(p,sizeof p) && mt76_net5_service(s));uint64_t token=m->token;
 assert(net_device_send(p,sizeof p));assert(mt76_net5_disconnect(s,1));
 assert(s->pending && !s->count && !net_device_send(p,sizeof p));
 assert(!mt76_net5_finish_detach(s,1));m->quiet=true;m->close_retired=true;
 assert(mt76_net5_finish_detach(s,1));assert(!s->attached && !s->pending && !s->io.context);
 unsigned calls=m->owner_calls+m->retired_calls+m->service_calls+m->receive_calls;free(m);
 struct net_device_link link;assert(net_device_link(&link) && !link.up && !net_device_poll(32,false));
 assert(!mt76_net5_tx_complete(s,1,token,true));
 m=calloc(1,sizeof *m);assert(m);*m=(struct mock){.s=s,.epoch=2,.live=true,.quiet=true,.service_ok=true};
 struct mt76_net5_io ops=io(m);assert(!mt76_net5_attach(s,1,&ops));assert(mt76_net5_attach(s,2,&ops));
 assert(!mt76_net5_tx_complete(s,1,token,true) && s->state==MT76_NET5_UP);
 assert(net_device_send(p,sizeof p) && mt76_net5_service(s) && m->token>token);
 assert(mt76_net5_tx_complete(s,2,m->token,true));assert(!net_device_register(&s->ops));gone(s,m);(void)calls;
}
static void test_retirement_proof(void){
 struct mock *m;struct mt76_net5 *s=make(&m);uint8_t p[64];frame(p,sizeof p,0x71);
 assert(net_device_send(p,sizeof p) && mt76_net5_service(s));
 assert(!mt76_net5_retire(s) && s->registry_drained && s->attached && s->pending);
 unsigned calls=m->owner_calls+m->service_calls+m->receive_calls;
 assert(!net_device_send(p,sizeof p) && !net_device_poll(32,false));
 struct srv_net_adapter_info rows[1];assert(net_device_inventory(rows,1)==1);
 assert(calls==m->owner_calls+m->service_calls+m->receive_calls);
 assert(mt76_net5_tx_complete(s,1,m->token,true));
 assert(!mt76_net5_retire(s) && s->attached); /* Completion is not retirement. */
 gone(s,m);
}
static void test_faults(void){
 {
  struct mock *m;struct mt76_net5 *s=make(&m);uint8_t p[64];frame(p,sizeof p,0x55);
  assert(net_device_send(p,sizeof p) && mt76_net5_service(s));now+=MT76_NET5_TX_US-1;
  assert(mt76_net5_tx_complete(s,1,m->token,true) && s->state==MT76_NET5_UP);gone(s,m);
 }
 for(unsigned kind=0;kind<12;kind++){
  struct mock *m;struct mt76_net5 *s=make(&m);uint8_t p[64];frame(p,sizeof p,0x55);
  assert(net_device_send(p,sizeof p));
  if(kind==0)m->live=false;
  if(kind==1)m->service_ok=false;
  if(kind==2)m->result=MT76_NET5_UNCERTAIN;
  if(kind==3)m->result=(enum mt76_net5_submit)99;
  if(kind==4)s->next_token=UINT64_MAX;
  if(kind==5)m->lose_after_submit=true;
  if(kind==6)now=999;
  bool serviced=mt76_net5_service(s);
  if(kind<7)assert(!serviced && s->state==MT76_NET5_FAULT);
  else{
   assert(serviced && s->pending);
   if(kind==7)assert(!mt76_net5_tx_complete(s,1,m->token+1,true));
   if(kind==8){now+=MT76_NET5_TX_US;assert(!mt76_net5_service(s));}
   if(kind==9)assert(!mt76_net5_tx_complete(s,1,m->token,false));
   if(kind==10){assert(mt76_net5_tx_complete(s,1,m->token,true));assert(!mt76_net5_tx_complete(s,1,m->token,true));}
   if(kind==11){now+=MT76_NET5_TX_US;assert(!mt76_net5_tx_complete(s,1,m->token,true));}
   assert(s->state==MT76_NET5_FAULT);
  }
  if(kind==2 || kind==3 || kind==5 || kind==7 || kind==8)assert(s->pending && s->io.context==m);
  assert(!mt76_net5_finish_detach(s,1));gone(s,m);
 }
 for(unsigned action=0;action<5;action++){
  struct mock *m;struct mt76_net5 *s=make(&m);uint8_t p[64];frame(p,sizeof p,0x55);
  if(action==0)m->close_owned=true;
  assert(net_device_send(p,sizeof p)==(action!=0));
  m->close_submit=action==1;m->retire_submit=action==2;m->complete_inline=action==3;m->result=action==4?MT76_NET5_REFUSED:MT76_NET5_QUEUED;
  (void)mt76_net5_service(s);
  if(action<=2)assert(s->state==MT76_NET5_CLOSING);
  if(action==3)assert(!s->pending && s->completed==1);
  if(action==4)assert(!s->pending && s->dropped==1 && s->state==MT76_NET5_UP);
  gone(s,m);
 }
}
static void test_admission(void){
 reset_registry();struct mt76_net5 *s=calloc(1,sizeof *s);assert(s);
 uint8_t bad[6]={1,2,3,4,5,6};assert(!mt76_net5_init(s,bad));assert(!mt76_net5_init(s,(uint8_t[6]){0}));
 assert(mt76_net5_init(s,station) && !mt76_net5_init(s,station));
 assert(net_device_select());struct mock m={.s=s,.epoch=1,.live=true,.quiet=true,.service_ok=true};struct mt76_net5_io ops=io(&m);
 assert(!mt76_net5_attach(s,0,&ops));struct mt76_net5_io bad_io=ops;bad_io.receive=NULL;assert(!mt76_net5_attach(s,1,&bad_io));
 m.live=false;assert(!mt76_net5_attach(s,1,&ops));m.live=true;assert(mt76_net5_attach(s,1,&ops));
 struct mt76_net5 *clone=malloc(sizeof *clone);assert(clone);memcpy(clone,s,sizeof *s);assert(!mt76_net5_service(clone) && !mt76_net5_disconnect(clone,1));free(clone);
 uint8_t p[64];frame(p,sizeof p,0x12);p[6]^=2;assert(!net_device_send(p,sizeof p));p[6]^=2;p[13]=0xdd;assert(!net_device_send(p,sizeof p));
 assert(!net_device_send(p,13));assert(mt76_net5_retire(s));assert(mt76_net5_retire(s));assert(!mt76_net5_attach(s,2,&ops));free(s);
 reset_registry();assert(!net_device_select());s=calloc(1,sizeof *s);assert(s && !mt76_net5_init(s,station));assert(!s->identity);free(s);
}
int main(void){
 test_queue();test_receive();test_detach();test_retirement_proof();test_faults();test_admission();
 puts("mt76-net5: PASS real registry,nonblocking copied queue,1004 unique completions,nested replies/close/retire,borrowed-buffer preservation,deadline/identity/owner/span faults,epoch reconnect and post-release registry access;mock transport/CCMP,no native provider acceptance");return 0;
}
