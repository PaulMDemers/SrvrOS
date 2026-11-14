/* Real ordinary Ethernet/ARP/TCP/UDP stack and provider,with a synthetic
 * reusable-page DMA transport. External wrap/retirement proofs are model
 * inputs,never native controller evidence. No CCMP or RF authority here. */
#include <srvros/mt76_net5.h>
#include <srvros/mt76_dma5.h>
static enum mt76_net5_submit reuse_submit(void *,uint64_t,uint64_t,const uint8_t *,uint16_t);
static bool reuse_service(void *,uint64_t);
static bool reuse_receive(void *,uint64_t,uint8_t *,size_t,size_t *,bool *);
static void reuse_setup(void),reuse_exercise(void),reuse_finish(void);
#define MT76_STACK_SUBMIT reuse_submit
#define MT76_STACK_SERVICE reuse_service
#define MT76_STACK_RECEIVE reuse_receive
#define MT76_STACK_SETUP() reuse_setup()
#define MT76_STACK_EXERCISE() reuse_exercise()
#define MT76_STACK_FINISH() reuse_finish()
#include "test_mt76_net5_stack.c"
static struct mt76_dma5 transport;
static struct mt76_dma5_lease transmit;
static uint8_t dma_pages[2][4][4096];
static uint64_t rx_token;
static unsigned used[2][4],udp_rounds;
static const uint64_t ring_phys[2]={0x1000,0x2000},page_phys[2][4]={{0x3000,0x4000,0x5000,0x6000},{0x7000,0x8000,0x9000,0xa000}};
static void fence(unsigned direction){
 if(transport.cursor[direction]!=255)return;
 struct mt76_dma5_fence f={.epoch=1,.sequence=transport.last_fence+1,.dequeue=ring_phys[direction]|(transport.cycle[direction]^1u),.direction=direction,.stopped=true,.events_drained=true,.dequeue_set=true};
 assert(mt76_dma5_wrap(&transport,&f));
}
static void reuse_setup(void){assert(mt76_dma5_init(&transport,1,ring_phys,page_phys,timer_monotonic_us()));}
static enum mt76_net5_submit reuse_submit(void *ctx,uint64_t epoch,uint64_t token,const uint8_t *p,uint16_t n){
 assert(backend_owned(ctx,epoch) && !transmit.ticket && n<=1514);fence(0);
 assert(mt76_dma5_prepare(&transport,0,token,n,timer_monotonic_us(),&transmit));
 uint8_t *page=dma_pages[0][transmit.index];memset(page,0,4096);memcpy(page,p,n);used[0][transmit.index]++;
 assert(mt76_dma5_publish(&transport,&transmit,timer_monotonic_us()));
 if(p_u16(page+12)==0x800 && page[23]==17){
  assert(!incoming_size && n==298 && p_u16(page+16)==(unsigned)n-14 && p_sum(page+14,20,0)==65535);
  const uint8_t *udp=page+34;assert(p_u16(udp)==12345 && p_u16(udp+2)==9999 && p_u16(udp+4)==264);
  /* Existing IPv4 UDP sender permits an omitted checksum. The synthetic
   * reply below always supplies an independently computed valid checksum. */
  assert(!p_u16(udp+6) || p_sum(udp,264,p_sum(page+26,8,17+264))==65535);
  memcpy(incoming,page,n);memcpy(incoming,mac,6);memcpy(incoming+6,peer_mac,6);
  memcpy(incoming+26,page+30,4);memcpy(incoming+30,page+26,4);
  incoming[24]=incoming[25]=0;p_put16(incoming+24,~p_sum(incoming+14,20,0));
  p_put16(incoming+34,9999);p_put16(incoming+36,12345);incoming[40]=incoming[41]=0;
  unsigned check=(~p_sum(incoming+34,264,p_sum(incoming+26,8,17+264)))&65535;p_put16(incoming+40,check?check:65535);
  incoming_size=n;udp_rounds++;return MT76_NET5_QUEUED;
 }
 return backend_submit(ctx,epoch,token,page,n);
}
static bool reuse_service(void *ctx,uint64_t epoch){
 if(transmit.ticket){
  assert(mt76_dma5_complete(&transport,&transmit,1,0,timer_monotonic_us()));
  memset(dma_pages[0][transmit.index],0,4096);memset(&transmit,0,sizeof transmit);
 }
 return backend_service(ctx,epoch);
}
static bool reuse_receive(void *ctx,uint64_t epoch,uint8_t *out,size_t cap,size_t *n,bool *present){
 assert(backend_owned(ctx,epoch) && cap==1514);*n=0;*present=false;if(!incoming_size)return true;
 fence(1);struct mt76_dma5_lease rx,loan;
 assert(mt76_dma5_prepare(&transport,1,++rx_token,4096,timer_monotonic_us(),&rx));
 assert(mt76_dma5_publish(&transport,&rx,timer_monotonic_us()));
 size_t bytes=0;bool have=false;uint8_t *page=dma_pages[1][rx.index];memset(page,0,4096);
 assert(backend_receive(ctx,epoch,page,cap,&bytes,&have) && have && bytes<=cap);used[1][rx.index]++;
 assert(mt76_dma5_complete(&transport,&rx,13,4096-(unsigned)bytes,timer_monotonic_us()));
 assert(mt76_dma5_acquire(&transport,epoch,&loan) && loan.received==bytes);
 memcpy(out,page,bytes);assert(mt76_dma5_release(&transport,&loan));memset(page,0,4096);
 *n=bytes;*present=true;return true;
}
static void reuse_exercise(void){
 int64_t socket=net_udp_open();assert(socket>0 && !net_udp_bind((uint64_t)socket,12345));
 uint8_t sent[256],received[256];
 for(unsigned round=0;round<1600;round++){
  for(unsigned i=0;i<256;i++)sent[i]=(uint8_t)(round+i*17);
  assert(net_udp_sendto_mode((uint64_t)socket,0x0a000202,9999,sent,sizeof sent,true)==256);
  memset(sent,0,sizeof sent); /* send caller storage may disappear immediately. */
  for(unsigned i=0;i<8;i++)net_device_poll(32,false);
  uint32_t ip=0;uint16_t port=0;
  assert(net_udp_recvfrom((uint64_t)socket,received,sizeof received,&ip,&port,true)==256 && ip==0x0a000202 && port==9999);
  for(unsigned i=0;i<256;i++)assert(received[i]==(uint8_t)(round+i*17));
  assert(!connection.pending && !connection.count && !transmit.ticket);
 }
 assert(!net_close((uint64_t)socket) && udp_rounds==1600 && transport.wraps==12);
 for(unsigned d=0;d<2;d++)for(unsigned i=0;i<4;i++)assert(used[d][i]>=400);
 assert(transport.borrowed==transport.released && !transport.faults);
}
static void reuse_finish(void){
 assert(quiet && mt76_dma5_close(&transport,1));
 struct mt76_dma5_retirement p={.epoch=1,.keys_erased=true,.mac_disabled=true,.tx_stopped=true,.rx_stopped=true,.events_drained=true,.callbacks_drained=true,.parent_restored=true};
 assert(mt76_dma5_retire(&transport,&p));
 puts("mt76-dma5-stack: PASS real provider/ordinary ARP/TCP HTTP GET/200 and1600 exact256-byte UDP round trips,caller-buffer overwrite,all eight DMA pages reused>=400 times,12 explicit descriptor-wrap fences and full retirement;synthetic transport/peer,no native reuse or CCMP acceptance");
}
