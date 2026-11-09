/* Actual native USB/key owner + unrenamed production net.c/registry/provider.
 * MMIO/USB and AP are modeled. Public OpenSSL AP crypto is independent. */
#include <srvros/net.h>
#include <srvros/net_device.h>
#include <srvros/mt76_net5.h>
static void stack_test_progress(void);
#define SRVROS_XHCI_STACK5_TEST
/* Rename only historical test_xhci's aborting stubs and its unused legacy
 * callsites. Production net.c and mt76_net5 objects retain their real symbols. */
#define net_device_register legacy_stub_net_device_register
#define net_device_select legacy_stub_net_device_select
#define net_device_name legacy_stub_net_device_name
#define net_device_mac legacy_stub_net_device_mac
#define net_device_send legacy_stub_net_device_send
#define net_device_retire legacy_stub_net_device_retire
#define net_handle_ethernet_frame legacy_stub_net_handle_ethernet_frame
#define net_notify_rx legacy_stub_net_notify_rx
bool net_device_register(const struct net_device_ops *);
bool net_device_select(void);
const char *net_device_name(void);
const uint8_t *net_device_mac(void);
bool net_device_send(const uint8_t *,uint16_t);
bool net_device_retire(const struct net_device_ops *);
void net_handle_ethernet_frame(const uint8_t *,uint16_t);
void net_notify_rx(void);
#define STREAM5_TEST_MAIN legacy_stream5_main
#include "test_xhci_wifi_stream5.c"
#undef net_device_register
#undef net_device_select
#undef net_device_name
#undef net_device_mac
#undef net_device_send
#undef net_device_retire
#undef net_handle_ethernet_frame
#undef net_notify_rx
#include "fixtures/mt76-stack5-peer.h"
#include <sys/wait.h>
#include <unistd.h>
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}
static unsigned stack_process;
struct process *process_current(void){return (struct process *)&stack_process;}
bool process_should_exit_current(void){return false;}
bool process_signal_pending_current(void){return false;}
void process_file_poll_wake(void){}
void scheduler_yield(void){test_time+=10000;}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a,uint64_t end){(void)q;(void)fn;(void)a;(void)end;test_time+=10000;return false;}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a){(void)q;(void)fn;(void)a;assert(0);return false;}
static struct {
 uint8_t response[8][1600];size_t length[8];unsigned head,count;
 uint64_t tx_pn,rx_pn;unsigned arp,tcp,udp,icmp,dhcp,dns,case_id;
 uint32_t server_seq,client_ack;unsigned response_offset;
 bool quiet,bad_mic,wrong_event,drop_tx,closed;
} ap;
static unsigned ap_u16(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static uint32_t ap_u32(const uint8_t *p){return (uint32_t)ap_u16(p)*65536+ap_u16(p+2);}
static void ap_p16(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static void ap_p32(uint8_t *p,uint32_t n){ap_p16(p,n>>16);ap_p16(p+2,n);}
static unsigned ap_sum(const uint8_t *p,size_t n,unsigned sum){for(size_t i=0;i<n;i+=2)sum+=(unsigned)p[i]*256+(i+1<n?p[i+1]:0);while(sum>>16)sum=(sum&65535)+(sum>>16);return sum;}
static const uint8_t ap_mac[6]={2,9,8,7,6,5};
static const uint32_t ap_ip=0x0a000202;
static void ap_enqueue(const uint8_t *eth,size_t n){
 assert(ap.count<8);unsigned at=(ap.head+ap.count)%8;size_t bytes=0;
 assert(stack5_peer_seal(eth,n,++ap.rx_pn,ap.response[at],sizeof ap.response[at],&bytes));
 if(ap.bad_mic)ap.response[at][bytes-1]^=1;
 ap.length[at]=bytes;ap.count++;
}
static void ap_ip_header(uint8_t *reply,const uint8_t *request,unsigned proto,size_t body){
 memset(reply,0,1514);memcpy(reply,hs_spa,6);memcpy(reply+6,ap_mac,6);ap_p16(reply+12,0x800);uint8_t *ip=reply+14;ip[0]=0x45;ap_p16(ip+2,20+body);ip[8]=64;ip[9]=proto;ap_p32(ip+12,ap_ip);memcpy(ip+16,request+26,4);ap_p16(ip+10,~ap_sum(ip,20,0));
}
static void ap_udp_reply(const uint8_t *p,size_t n){
 uint8_t r[1514];const uint8_t *u=p+34;unsigned size=ap_u16(u+4);assert(n>=42 && size>=8 && n==34+size);
 unsigned sport=ap_u16(u),dport=ap_u16(u+2);size_t body=size-8;uint8_t *v=r+42;
 if(sport==68 && dport==67){
  ap.dhcp++;assert(body>=240 && u[8]==1 && u[9]==1 && u[10]==6);unsigned type=0;for(size_t i=240;i<body;){unsigned k=u[8+i++];if(k==255)break;if(!k)continue;assert(i<body);unsigned z=u[8+i++];assert(z<=body-i);if(k==53){assert(z==1);type=u[8+i];}i+=z;}assert(type==1 || type==3);if(ap.case_id==1 && ap.dhcp==1)return;
  body=274;ap_ip_header(r,p,17,8+body);memset(v,0,body);v[0]=2;v[1]=1;v[2]=6;memcpy(v+4,u+12,4);ap_p32(v+16,0x0a00020f);memcpy(v+28,hs_spa,6);ap_p32(v+236,0x63825363);unsigned at=240;v[at++]=53;v[at++]=1;v[at++]=type==1?2:5;
  const unsigned kinds[]={54,1,3,6,51};const uint32_t values[]={0x0a000202,0xffffff00,0x0a000202,0x0a000202,3600};for(unsigned i=0;i<5;i++){v[at++]=kinds[i];v[at++]=4;ap_p32(v+at,values[i]);at+=4;}v[at++]=255;assert(at==body);ap_p32(r+30,0xffffffff);ap_p16(r+24,0);ap_p16(r+24,~ap_sum(r+14,20,0));ap_p16(r+34,67);ap_p16(r+36,68);
 }else if(dport==53){
  ap.dns++;assert(body>=17 && ap_u16(u+12)==1);size_t query=body;body+=16;ap_ip_header(r,p,17,8+body);memcpy(v,u+8,query);ap_p16(v+2,0x8180);ap_p16(v+6,1);ap_p16(v+8,0);ap_p16(v+10,0);uint8_t *a=v+query;ap_p16(a,0xc00c);ap_p16(a+2,1);ap_p16(a+4,1);ap_p32(a+6,60);ap_p16(a+10,4);ap_p32(a+12,ap_ip);ap_p16(r+34,53);ap_p16(r+36,sport);
 }else{
  assert(dport==9000 && body==256);ap.udp++;ap_ip_header(r,p,17,8+body);memcpy(v,u+8,body);ap_p16(r+34,dport);ap_p16(r+36,sport);
 }
 ap_p16(r+38,8+body);ap_p16(r+40,0);ap_enqueue(r,42+body);
}
static const char stack_request[]="GET / HTTP/1.0\r\nHost: example.test\r\n\r\n";
static const char stack_header[]="HTTP/1.0 200 OK\r\nContent-Length: 3072\r\n\r\n";
static void ap_tcp_reply(const uint8_t *p,unsigned flags,uint32_t seq,uint32_t ack,const uint8_t *body,size_t n){
 uint8_t r[1514];ap_ip_header(r,p,6,20+n);uint8_t *t=r+34;ap_p16(t,80);ap_p16(t+2,ap_u16(p+34));ap_p32(t+4,seq);ap_p32(t+8,ack);t[12]=0x50;t[13]=flags;ap_p16(t+14,65535);if(n)memcpy(t+20,body,n);ap_p16(t+16,~ap_sum(t,20+n,ap_sum(r+26,8,6+20+n)));ap_enqueue(r,54+n);
}
static void ap_packet(const uint8_t *p,size_t n){
 assert(n>=42 && !memcmp(p+6,hs_spa,6));unsigned protocol=ap_u16(p+12);
 if(protocol==0x806){
  assert(n==42 && ap_u16(p+20)==1 && ap_u32(p+38)==ap_ip);ap.arp++;uint8_t r[42]={0};memcpy(r,hs_spa,6);memcpy(r+6,ap_mac,6);ap_p16(r+12,0x806);ap_p16(r+14,1);ap_p16(r+16,0x800);r[18]=6;r[19]=4;ap_p16(r+20,2);memcpy(r+22,ap_mac,6);ap_p32(r+28,ap_ip);memcpy(r+32,hs_spa,6);memcpy(r+38,p+28,4);ap_enqueue(r,sizeof r);return;
 }
 assert(protocol==0x800 && p[14]==0x45 && ap_u16(p+16)==n-14 && ap_sum(p+14,20,0)==65535);
 if(p[23]==17){ap_udp_reply(p,n);return;}
 if(p[23]==1){uint8_t r[1514];ap.icmp++;ap_ip_header(r,p,1,n-34);memcpy(r+34,p+34,n-34);r[34]=0;ap_p16(r+36,0);ap_p16(r+36,~ap_sum(r+34,n-34,0));ap_enqueue(r,n);return;}
 assert(p[23]==6);ap.tcp++;const uint8_t *t=p+34;unsigned header=(t[12]>>4)*4,size=(unsigned)n-34;assert(header>=20 && header<=size && ap_u16(t+2)==80 && ap_sum(t,size,ap_sum(p+26,8,6+size))==65535);uint32_t seq=ap_u32(t+4);unsigned flags=t[13];
 if(flags&2){assert(flags==2);ap.server_seq=9000;ap_tcp_reply(p,0x12,ap.server_seq++,seq+1,NULL,0);}
 else if(size>header){
  assert(size-header==sizeof stack_request-1 && !memcmp(t+header,stack_request,sizeof stack_request-1));uint8_t bytes[sizeof stack_header-1+3072];memcpy(bytes,stack_header,sizeof stack_header-1);for(unsigned i=0;i<3072;i++)bytes[sizeof stack_header-1+i]=(uint8_t)('A'+i%26);
  for(unsigned at=0;at<sizeof bytes;){unsigned count=(unsigned)sizeof bytes-at;if(count>1100)count=1100;ap_tcp_reply(p,0x18,ap.server_seq,seq+size-header,bytes+at,count);ap.server_seq+=count;at+=count;}
 }else if(flags&1){ap_tcp_reply(p,0x11,ap.server_seq++,seq+1,NULL,0);ap.closed=true;}
 else assert(flags==0x10);
}
static void stack_test_progress(void){
 if(!wifi_stream5.data_active){stream_test_progress();return;}
 if(wifi_stream5.pending[0]){
  struct mt76_dma5_lease *l=&wifi_stream5.lease[0];
  if(ap.drop_tx)return;
  assert(sim.tx_seen+1==wifi_stream5.posts[0]);sim.tx_seen++;
  uint8_t eth[1514];size_t n=0;uint64_t pn=0;assert(stack5_peer_open(wifi_stream5.buffer[0][l->index],l->length,eth,sizeof eth,&n,&pn));assert(pn==++ap.tx_pn);ap_packet(eth,n);
  uint64_t q=wifi_stream5.physical[0]+16*wifi_stream5.enqueue[0];context_write32(wifi_stream5.d->output_context,8,0,1);context_write32(wifi_stream5.d->output_context,8,2,q|wifi_stream5.cycle[0]);context_write32(wifi_stream5.d->output_context,8,3,q>>32);
  struct xhci_trb e={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=1u<<24,.control=(32u<<10)|(8u<<16)|((uint32_t)wifi_stream5.slot<<24)};if(ap.wrong_event)e.parameter_low+=16;stream_enqueue(&e,0);if(ap.case_id==9)stream_enqueue(&e,1);return;
 }
 if(wifi_stream5.pending[1]){
  unsigned bytes=0;
  if(ap.count){bytes=stream_wrap(ap.response[ap.head],(unsigned)ap.length[ap.head]);ap.head=(ap.head+1)%8;ap.count--;}
  else if(!ap.quiet && wifi_stack5.boundary){uint8_t beacon[24]={0x80};bytes=stream_wrap(beacon,sizeof beacon);}
  if(bytes){struct mt76_dma5_lease *l=&wifi_stream5.lease[1];uint64_t q=wifi_stream5.posts[1]%63?wifi_stream5.physical[1]+16*wifi_stream5.enqueue[1]:wifi_stream5.physical[1]+63*16;unsigned cycle=wifi_stream5.posts[1]%63?(unsigned)wifi_stream5.cycle[1]:l->cycle;context_write32(wifi_stream5.d->output_context,9,0,1);context_write32(wifi_stream5.d->output_context,9,2,q|cycle);context_write32(wifi_stream5.d->output_context,9,3,q>>32);struct xhci_trb e={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=(13u<<24)|(4096-bytes),.control=(32u<<10)|(9u<<16)|((uint32_t)wifi_stream5.slot<<24)};stream_enqueue(&e,0);return;}
 }
 if(xhci.command_pending && wifi_fence5_native.round)stream_test_progress();
}
static bool stack_guard_current(void *c){struct mt76_net5 *p=c;return p==&wifi_stack5.net && p->state==MT76_NET5_UP && wifi_stack5_owned(&wifi_stream5,p->epoch);}
static bool stack_guard_retry(void *c){struct mt76_net5 *p=c;return stack_guard_current(c) && !p->count && !p->pending && p->submitted==p->completed && !wifi_stack5.tx_token && !wifi_stream5.pending[0];}
static bool stack_poll_until_idle(struct mt76_net5 *p,unsigned limit){for(unsigned i=0;i<limit;i++){test_time+=1000;net_device_poll(32,false);if(p->state!=MT76_NET5_UP)return false;if(!p->count && !p->pending && !wifi_stack5.tx_token && !ap.count && !wifi_stream5.cached_present && !wifi_stream5.ready_rx && !wifi_stack5.boundary && !wifi_fence5_native.round)return true;}return false;}
static bool stack_exercise(void *context,struct mt76_net5 *p){
 assert(context==&ap && p==&wifi_stack5.net && wifi_stream5.exchange.handshake.install_handoffs==1);
 assert(net_init_bounded_probe());assert(!strcmp(net_device_name(),"mt76-ccmp"));
 size_t span=7;bool present=true;uint64_t pn=wifi_stream5.data_pn;
 assert(!wifi_stack5_ready(&wifi_stream5,p->epoch+1));
 assert(!wifi_stack5_receive(&wifi_stream5,p->epoch+1,p->rx,sizeof p->rx,&span,&present) && !span && !present);
 assert(!wifi_stack5_receive(&wifi_stream5,p->epoch,p->rx,sizeof p->rx-1,&span,&present));
 assert(wifi_stream5.data_pn==pn);
 if(ap.case_id==7){test_time=wifi_stack5.last-100;assert(!mt76_net5_service(p));return false;}
 if(ap.case_id==8){test_time_step=0;wifi_stack5.services=WIFI_STACK5_POLLS-1;assert(mt76_net5_service(p));assert(!mt76_net5_service(p) && wifi_stream5.failed);return false;}
 if(ap.case_id==6){uint8_t a[42]={0};memset(a,255,6);memcpy(a+6,hs_spa,6);a[12]=8;a[13]=6;ap_p16(a+14,1);ap_p16(a+16,0x800);a[18]=6;a[19]=4;ap_p16(a+20,1);memcpy(a+22,hs_spa,6);ap_p32(a+28,0x0a00020f);ap_p32(a+38,ap_ip);assert(net_device_send(a,sizeof a));assert(mt76_net5_service(p) && p->pending && wifi_stream5.pending[0]);return true;}
 const struct net_dhcp_retry_guard guard={p,stack_guard_current,stack_guard_retry};
 if(ap.case_id==4){ap.drop_tx=true;assert(net_dhcp_request_discover_retry(&guard)<0);assert(p->pending && wifi_stream5.pending[0] && wifi_stream5.data_pn==1);return false;}
 if(ap.case_id==5 || ap.case_id==9){ap.wrong_event=ap.case_id==5;assert(net_dhcp_request_discover_retry(&guard)<0);assert(wifi_stream5.failed && p->pending && wifi_stream5.data_pn==1);return false;}
 if(net_dhcp_request_discover_retry(&guard)<0)return false;
 struct srv_net_status_info status;assert(!net_status_info(&status) && status.dhcp_configured && status.local_ip==0x0a00020f && status.router_ip==ap_ip);
 uint32_t resolved=0;if(net_dns_resolve("example.com",&resolved)<0)return false;assert(resolved==ap_ip && ap.dns==1);
 uint64_t elapsed=0;if(net_ping(ap_ip,17,100,&elapsed)<0)return false;
 int64_t udp=net_udp_open();assert(udp>0);uint8_t payload[256],out[256];
 for(unsigned i=0;i<52;i++){
  for(unsigned j=0;j<sizeof payload;j++)payload[j]=(uint8_t)(i+73*j);
  assert(net_udp_sendto_mode((uint64_t)udp,ap_ip,9000,payload,sizeof payload,true)==sizeof payload);
  if(!stack_poll_until_idle(p,2000)){assert(ap.quiet || ap.bad_mic);return false;}
  uint32_t ip=0;uint16_t port=0;int64_t n=net_udp_recvfrom((uint64_t)udp,out,sizeof out,&ip,&port,true);if(n<0)return false;
  assert(n==sizeof payload && ip==ap_ip && port==9000 && !memcmp(out,payload,sizeof out));
 }
 assert(!net_close((uint64_t)udp));
 int64_t tcp=net_connect(ap_ip,80,true);assert(tcp>0);if(!stack_poll_until_idle(p,2000))return false;
 assert(net_respond((uint64_t)tcp,stack_request,sizeof stack_request-1,true)==sizeof stack_request-1);if(!stack_poll_until_idle(p,2000))return false;
 uint8_t bytes[4096];int64_t n=net_read((uint64_t)tcp,(char *)bytes,sizeof bytes,true);assert(n==sizeof stack_header-1+3072 && !memcmp(bytes,stack_header,sizeof stack_header-1));for(unsigned i=0;i<3072;i++)assert(bytes[sizeof stack_header-1+i]==(uint8_t)('A'+i%26));
 if(!stack_poll_until_idle(p,2000))return false;
 assert(!net_close((uint64_t)tcp));if(!stack_poll_until_idle(p,2000))return false;assert(ap.closed);
 assert(wifi_stream5.wraps[0]==1 && wifi_stream5.wraps[1]>=1 && p->received && p->submitted==p->completed && wifi_stream5.dma.borrowed==wifi_stream5.dma.released);
 return true;
}
static void stack_case(unsigned which){
 stream_setup();memset(&wifi_stack5,0,sizeof wifi_stack5);memset(&ap,0,sizeof ap);ap.case_id=which;ap.quiet=which==2;ap.bad_mic=which==3;bool good=xhci_wifi_capture_stack5(stack_exercise,&ap);
 if(which<2){assert(good && wifi_stream5.complete && wifi_stream5.retired && wifi_stack5.retired && wifi_stack5.detached && !wifi_stack5.net.io.context);assert(ap.dhcp==(which==1?3u:2u) && ap.udp==52 && ap.icmp==1 && ap.dns==1);}
 else{assert(!good && !wifi_stream5.complete);if(which==2 || which>=4)assert(wifi_stream5.failed && !wifi_stream5.retired && !wifi_stack5.detached && wifi_stack5.net.io.context);}
 assert(wifi_stream5.exchange.keys_erased && !wifi_stream5.exchange.handshake.keys_may_live);reset_test();
}
int main(int argc,char **argv){
 if(argc==3 && !strcmp(argv[1],"--case")){stack_case((unsigned)strtoul(argv[2],NULL,0));return 0;}assert(argc==1);
 for(unsigned i=0;i<10;i++){pid_t p=fork();assert(p>=0);if(!p){stack_case(i);exit(0);}int status=0;assert(waitpid(p,&status,0)==p && WIFEXITED(status) && WEXITSTATUS(status)==0);}
 puts("xhci-stack5: PASS original encrypted native owner with real production registry/provider/DHCP,DNS,ARP,ICMP,52 UDP echoes,multipacket3072-byte HTTP/TCP and teardown;independent OpenSSL AP,mocked USB,no native acceptance");return 0;
}
