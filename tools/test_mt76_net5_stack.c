/* Actual Ethernet/ARP/TCP stack and provider;synthetic plaintext peer only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/mt76_net5.h>
#include "../kernel/src/net.c"
static uint64_t ticks=100;
uint64_t timer_ticks(void){return ticks;}
uint64_t timer_monotonic_us(void){return ticks*1000;}
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}
void console_printf(const char *f,...){(void)f;}
void console_write(const char *p){(void)p;}
static unsigned owner_token;
struct process *process_current(void){return (struct process *)&owner_token;}
bool process_should_exit_current(void){return false;}
bool process_signal_pending_current(void){return false;}
void process_file_poll_wake(void){}
void scheduler_yield(void){ticks++;}
void scheduler_wake_all(struct scheduler_wait_queue *q){(void)q;}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a,uint64_t end){(void)q;(void)fn;(void)a;(void)end;ticks++;return false;}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a){(void)q;(void)fn;(void)a;assert(0);return false;}
static const uint8_t mac[6]={2,1,2,3,4,5},peer_mac[6]={2,9,8,7,6,5};
static const uint8_t peer_ip[4]={10,0,2,2};
static const char request[]="GET / HTTP/1.0\r\nHost: example.test\r\n\r\n";
static const char response[]="HTTP/1.0 200 OK\r\nContent-Length: 5\r\n\r\nhello";
static struct mt76_net5 connection;
static uint8_t incoming[1514];static size_t incoming_size;
static bool quiet;static unsigned arp_sent,syn_sent,http_sent,fin_sent,ack_sent;
static uint32_t server_sequence=9000;
static unsigned p_u16(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static uint32_t p_u32(const uint8_t *p){return (uint32_t)p_u16(p)*65536+p_u16(p+2);}
static void p_put16(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void p_put32(uint8_t *p,uint32_t v){p_put16(p,v>>16);p_put16(p+2,v);}
static unsigned p_sum(const uint8_t *p,size_t n,unsigned sum){
 for(size_t i=0;i<n;i+=2)sum+=(unsigned)p[i]*256+(i+1<n?p[i+1]:0);
 while(sum>>16)sum=(sum&65535)+(sum>>16);
 return sum;
}
static void peer_tcp(const uint8_t *sent,unsigned flags,uint32_t sequence,uint32_t ack,const void *body,size_t size){
 assert(!incoming_size && size<1400);memset(incoming,0,sizeof incoming);
 memcpy(incoming,mac,6);memcpy(incoming+6,peer_mac,6);p_put16(incoming+12,0x800);
 uint8_t *ip=incoming+14,*tcp=ip+20;ip[0]=0x45;p_put16(ip+2,40+size);ip[8]=64;ip[9]=6;
 memcpy(ip+12,peer_ip,4);memcpy(ip+16,sent+26,4);p_put16(ip+10,~p_sum(ip,20,0));
 p_put16(tcp,80);p_put16(tcp+2,p_u16(sent+34));p_put32(tcp+4,sequence);p_put32(tcp+8,ack);
 tcp[12]=0x50;tcp[13]=flags;p_put16(tcp+14,65535);if(size)memcpy(tcp+20,body,size);
 unsigned sum=p_sum(ip+12,8,6+20+(unsigned)size);p_put16(tcp+16,~p_sum(tcp,20+size,sum));incoming_size=54+size;
}
static bool backend_owned(void *ctx,uint64_t epoch){return ctx==&owner_token && epoch==1;}
static bool backend_retired(void *ctx,uint64_t epoch){assert(ctx==&owner_token && epoch==1);return quiet;}
static enum mt76_net5_submit backend_submit(void *ctx,uint64_t epoch,uint64_t token,const uint8_t *p,uint16_t n){
 assert(backend_owned(ctx,epoch) && token && n>=42 && !memcmp(p+6,mac,6));
 if(p_u16(p+12)==0x806){
  assert(!incoming_size && n==42 && p_u16(p+20)==1 && !memcmp(p+38,peer_ip,4));arp_sent++;
  memset(incoming,0,42);memcpy(incoming,mac,6);memcpy(incoming+6,peer_mac,6);p_put16(incoming+12,0x806);
  p_put16(incoming+14,1);p_put16(incoming+16,0x800);incoming[18]=6;incoming[19]=4;p_put16(incoming+20,2);
  memcpy(incoming+22,peer_mac,6);memcpy(incoming+28,peer_ip,4);memcpy(incoming+32,mac,6);memcpy(incoming+38,p+28,4);incoming_size=42;
 }else{
  assert(p_u16(p+12)==0x800 && p[14]==0x45 && p[23]==6 && p_u16(p+16)==(unsigned)n-14 && p_sum(p+14,20,0)==65535);
  const uint8_t *tcp=p+34;unsigned tcp_size=n-34,header=(tcp[12]>>4)*4;assert(header>=20 && header<=tcp_size && p_u16(tcp+2)==80);
  assert(p_sum(tcp,tcp_size,p_sum(p+26,8,6+tcp_size))==65535);
  uint32_t seq=p_u32(tcp+4);unsigned flags=tcp[13];
  if(flags&2){assert(flags==2);syn_sent++;peer_tcp(p,0x12,server_sequence,seq+1,NULL,0);server_sequence++;}
  else if(tcp_size>header){
   assert(tcp_size-header==sizeof(request)-1 && !memcmp(tcp+header,request,sizeof(request)-1));http_sent++;
   peer_tcp(p,0x18,server_sequence,seq+(uint32_t)(sizeof(request)-1),response,sizeof(response)-1);server_sequence+=sizeof(response)-1;
  }else if(flags&1){fin_sent++;peer_tcp(p,0x11,server_sequence,seq+1,NULL,0);server_sequence++;}
  else{assert(flags==0x10 && p_u32(tcp+8)==server_sequence);ack_sent++;}
 }
 return MT76_NET5_QUEUED;
}
static bool backend_service(void *ctx,uint64_t epoch){
 assert(backend_owned(ctx,epoch));
 if(connection.pending)assert(mt76_net5_tx_complete(&connection,epoch,connection.pending_token,true));
 return true;
}
static bool backend_receive(void *ctx,uint64_t epoch,uint8_t *out,size_t cap,size_t *n,bool *present){
 assert(backend_owned(ctx,epoch) && cap==1514);
 if(incoming_size){memcpy(out,incoming,incoming_size);*n=incoming_size;*present=true;incoming_size=0;}return true;
}
#ifndef MT76_STACK_SUBMIT
#define MT76_STACK_SUBMIT backend_submit
#define MT76_STACK_SERVICE backend_service
#define MT76_STACK_RECEIVE backend_receive
#define MT76_STACK_SETUP() ((void)0)
#define MT76_STACK_EXERCISE() ((void)0)
#define MT76_STACK_FINISH() ((void)0)
#endif
int main(void){
 MT76_STACK_SETUP();
 assert(mt76_net5_init(&connection,mac));
 struct mt76_net5_io ops={&owner_token,backend_owned,MT76_STACK_SUBMIT,MT76_STACK_SERVICE,MT76_STACK_RECEIVE,backend_retired};
 assert(mt76_net5_attach(&connection,1,&ops));net_init();assert(initialized && !strcmp(net_device_name(),"mt76-ccmp"));
 int64_t h=net_connect(0x0a000202,80,true);assert(h>0);
 for(unsigned i=0;i<16;i++)net_device_poll(32,false);
 assert(arp_sent==1 && syn_sent==1 && ack_sent==1);
 assert(net_respond((uint64_t)h,request,sizeof(request)-1,true)==sizeof(request)-1);
 for(unsigned i=0;i<16;i++)net_device_poll(32,false);
 char bytes[128]={0};assert(net_read((uint64_t)h,bytes,sizeof bytes,true)==sizeof(response)-1);
 assert(!memcmp(bytes,response,sizeof(response)-1) && http_sent==1 && ack_sent==2);
 /* Consuming receive bytes advertises the reopened TCP window with an ACK. */
 for(unsigned i=0;i<4;i++)net_device_poll(32,false);
 assert(ack_sent==3 && !connection.count && !connection.pending);
 assert(!net_close((uint64_t)h));for(unsigned i=0;i<16;i++)net_device_poll(32,false);
 assert(fin_sent==1 && ack_sent==4 && !connection.count && !connection.pending);
 assert(tcp_connections[0].state==TCP_STATE_TIME_WAIT && tcp_connections[0].peer_closed && !tcp_connections[0].refs);
 ticks+=100000;net_timer_tick();net_process_cleanup(process_current());
 MT76_STACK_EXERCISE();
 assert(mt76_net5_disconnect(&connection,1) && !mt76_net5_finish_detach(&connection,1));
 quiet=true;assert(mt76_net5_finish_detach(&connection,1) && mt76_net5_retire(&connection));
 MT76_STACK_FINISH();
 puts("mt76-net5-stack: PASS actual registry and production ARP/TCP stack,SYN/ACK,HTTP GET/200 payload,FIN/ACK,nested replies and provider retirement;synthetic plaintext peer,no native USB/CCMP/provider acceptance");return 0;
}
