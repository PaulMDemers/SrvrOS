/* Actual native USB/key owner + unrenamed production net.c/registry/provider.
 * MMIO/USB and AP are modeled. Public OpenSSL AP crypto is independent. */
#include <srvros/net.h>
#include <srvros/net_device.h>
#include <srvros/mt76_net5.h>
static void stack_test_progress(void);
static void stack_test_connection(unsigned);
static bool stack_test_nonce(uint8_t [32]);
static bool stack_test_message3(uint8_t *,size_t,size_t *);
#define SRVROS_XHCI_STACK5_TEST
/* Rename only historical test_xhci's aborting stubs and its unused legacy
 * callsites. Production net.c and mt76_net5 objects retain their real symbols. */
#define net_device_register legacy_stub_net_device_register
#define net_device_select legacy_stub_net_device_select
#define net_device_name legacy_stub_net_device_name
#define net_device_send legacy_stub_net_device_send
#define net_device_retire legacy_stub_net_device_retire
#define net_handle_ethernet_frame legacy_stub_net_handle_ethernet_frame
#define net_notify_rx legacy_stub_net_notify_rx
bool net_device_register(const struct net_device_ops *);
bool net_device_select(void);
const char *net_device_name(void);
bool net_device_send(const uint8_t *,uint16_t);
bool net_device_retire(const struct net_device_ops *);
void net_handle_ethernet_frame(const uint8_t *,uint16_t);
void net_notify_rx(void);
#define STREAM5_TEST_MAIN legacy_stream5_main
#include "test_xhci_wifi_stream5.c"
#undef net_device_register
#undef net_device_select
#undef net_device_name
#undef net_device_send
#undef net_device_retire
#undef net_handle_ethernet_frame
#undef net_notify_rx
#include "fixtures/mt76-stack5-peer.h"
static bool stack_test_nonce(uint8_t out[32]){return stack5_peer_nonce(out);}
static bool stack_test_message3(uint8_t *out,size_t cap,size_t *size){return stack5_peer_message3(out,cap,size);}
#include <sys/wait.h>
#include <unistd.h>
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}
static unsigned stack_process;
struct process *process_current(void){return (struct process *)&stack_process;}
bool process_should_exit_current(void){return false;}
bool process_signal_pending_current(void){return false;}
void process_file_poll_wake(void){}
uint64_t scheduler_current_thread_id(void){return 1;}
void scheduler_yield(void){test_time+=10000;}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a,uint64_t end){(void)q;(void)fn;(void)a;(void)end;test_time+=10000;return false;}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *a){(void)q;(void)fn;(void)a;assert(0);return false;}
static struct {
 uint8_t response[8][1600];size_t length[8];unsigned head,count;
 uint64_t tx_pn,rx_pn,ack_release;unsigned ack_noise;unsigned arp,tcp,udp,icmp,dhcp,dns,case_id;
 uint32_t server_seq,client_ack,dhcp_xid;unsigned response_offset;
 bool quiet,bad_mic,wrong_event,drop_tx,closed;
 bool frozen,provider_loss,wrong_payload;unsigned arp_replies,arp_noise_left,background_left;uint64_t held_ticket;
 uint8_t deferred_dns[1514];size_t deferred_dns_size;
} ap;
static bool http_reconnect_peer,http_reconnect_peer_no_fin;
static uint8_t http_peer_request[4159];static unsigned http_peer_length;
static unsigned reconnect_case_id,reconnect_hook_count,connection_dhcp_fault;
static unsigned reconnect_noise(void){return reconnect_case_id==11?2u:reconnect_case_id==12?3u:(reconnect_case_id==10 || (reconnect_case_id>=16 && reconnect_case_id<=18))?1u:0u;}
static bool reconnect_keys_seen[2];
static uint8_t reconnect_first_tk[16];
static bool reconnect_seeded;
static uint32_t reconnect_seed[2][8];
static unsigned reconnect_slot;
static uint64_t reconnect_old_epoch,reconnect_old_token;
static struct wifi_stream5_state reconnect_old_stream;
static struct wifi_fence5_native_state reconnect_old_fence;
static unsigned char reconnect_old_usb[MT76_DATA5_USB_MAX];
static size_t reconnect_old_usb_size;
static uint8_t reconnect_old_physical[10][4096];
static uint8_t reconnect_old_mpdu[1600];
static size_t reconnect_old_mpdu_size;
static void stack_test_connection(unsigned generation){
 assert(generation==reconnect_hook_count++);
 if(!generation){
  assert(!wifi_stack5_net.identity && !wifi_stream5.attempted);
 }else{
  assert(wifi_stream5_current==&wifi_stream5_slots[1] && wifi_stream5_slots[0].connection_closed);
  assert(wifi_stack5_net.state==MT76_NET5_DOWN && !wifi_stack5_net.attached && !wifi_stack5_net.io.context);
  if(http_reconnect_peer)assert(wifi_http_reconnect5.probe[0].result.complete && wifi_http_reconnect5.probe[0].result.disconnected);
  else assert(wifi_reconnect5.probe[0].complete && wifi_reconnect5.probe[0].disconnected);
  reconnect_old_stream=wifi_stream5_slots[0];reconnect_old_fence=wifi_fence5_first;
  for(unsigned i=0;i<10;i++)memcpy(reconnect_old_physical[i],i<2?(void *)wifi_stream5_slots[0].ring[i]:(void *)wifi_stream5_slots[0].buffer[(i-2)/4][(i-2)%4],4096);
  if(reconnect_case_id==8)pool_used=(wifi_stream5_slots[0].physical[0]-(uint64_t)pool)/PMM_FRAME_SIZE;
  reconnect_old_epoch=wifi_stack5_net.epoch;reconnect_old_token=wifi_stack5_net.next_token;
 }
 assert(stack5_peer_generation(generation+1));
 if(generation && reconnect_old_usb_size){uint8_t frame[1514];size_t bytes=0;uint64_t pn=0;
  assert(!stack5_peer_open(reconnect_old_usb,reconnect_old_usb_size,frame,sizeof frame,&bytes,&pn));}
 http_peer_length=0;
 memset(&ap,0,sizeof ap);ap.case_id=connection_dhcp_fault?connection_dhcp_fault:55;
 if(generation && reconnect_case_id==9){assert(reconnect_old_mpdu_size);memcpy(ap.response[0],reconnect_old_mpdu,reconnect_old_mpdu_size);ap.length[0]=reconnect_old_mpdu_size;ap.count=1;}
 sim.tx_seen=sim.rx_seen=sim.response=sim.index=sim.noise_left=0;sim.m1=sim.m3=false;
 if(!generation && reconnect_case_id==4){sim.fault=36;sim.fault_at=9;}
 if(generation && reconnect_case_id==5)sim.fail_nonce=true;
 if(generation && reconnect_case_id==19)ap.case_id=24;
 if(generation && reconnect_case_id>=20 && reconnect_case_id<=24){
  ap.case_id=24;memset(&radio_sim,0,sizeof radio_sim);radio_sim.mode=reconnect_case_id-19;
 }
 if(generation && reconnect_case_id==3){struct xhci_trb old={.parameter_low=(uint32_t)wifi_stream5_slots[0].sync[0].pointer,.parameter_high=wifi_stream5_slots[0].sync[0].pointer>>32,.status=1u<<24,.control=(33u<<10)|(1u<<24)};stream_enqueue(&old,0);}
}
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
 if(wifi_stream5.connection_mode && reconnect_hook_count==1 && !reconnect_old_mpdu_size){reconnect_old_mpdu_size=bytes;memcpy(reconnect_old_mpdu,ap.response[at],bytes);}
 ap.length[at]=bytes;ap.count++;
}
static void ap_ip_header(uint8_t *reply,const uint8_t *request,unsigned proto,size_t body){
 memset(reply,0,1514);memcpy(reply,hs_spa,6);memcpy(reply+6,ap_mac,6);ap_p16(reply+12,0x800);uint8_t *ip=reply+14;ip[0]=0x45;ap_p16(ip+2,20+body);ip[8]=64;ip[9]=proto;ap_p32(ip+12,ap_ip);memcpy(ip+16,request+26,4);ap_p16(ip+10,~ap_sum(ip,20,0));
}
static void ap_udp_reply(const uint8_t *p,size_t n){
 uint8_t r[1514];const uint8_t *u=p+34;unsigned size=ap_u16(u+4);assert(n>=42 && size>=8 && n==34+size);
 unsigned sport=ap_u16(u),dport=ap_u16(u+2);size_t body=size-8;uint8_t *v=r+42;
 if(sport==68 && dport==67){
  ap.dhcp++;assert(body>=240 && u[8]==1 && u[9]==1 && u[10]==6);unsigned type=0,requested=0,selected_server=0;bool client_id=false,parameters=false;for(size_t i=240;i<body;){unsigned k=u[8+i++];if(k==255)break;if(!k)continue;assert(i<body);unsigned z=u[8+i++];assert(z<=body-i);if(k==53){assert(z==1);type=u[8+i];}if(k==61){assert(z==7 && u[8+i]==1 && !memcmp(u+9+i,hs_spa,6));client_id=true;}if(k==55){assert(z==4 && u[8+i]==1 && u[9+i]==3 && u[10+i]==6 && u[11+i]==15);parameters=true;}if(k==50){assert(z==4);requested=ap_u32(u+8+i);}if(k==54){assert(z==4);selected_server=ap_u32(u+8+i);}i+=z;}assert((type==1 || type==3) && client_id && parameters);
  assert(!memcmp(p+6,hs_spa,6) && ap_u32(p+26)==0 && ap_u32(p+30)==0xffffffff && ap_sum(p+14,20,0)==65535);
  assert(!memcmp(u+8+28,hs_spa,6) && (ap_u16(u+8+10)&0x8000) && !ap_u32(u+8+12));
  if(type==1){assert(!requested && !selected_server);if(ap.dhcp_xid)assert(ap.dhcp_xid==ap_u32(u+12));ap.dhcp_xid=ap_u32(u+12);}
  else{assert(ap.dhcp_xid==ap_u32(u+12) && requested==0x0a00020f && selected_server==ap_ip);}
  if((ap.case_id==1 || ap.case_id==11) && ap.dhcp==1)return;
  if(ap.case_id==56 || ap.case_id==57){
   if(ap.case_id==57)for(unsigned i=0;i<(ap.dhcp==1?7u:2u);i++){
    uint8_t noise[42]={0};memcpy(noise,hs_spa,6);memcpy(noise+6,ap_mac,6);ap_p16(noise+12,0x806);
    ap_p16(noise+14,1);ap_p16(noise+16,0x800);noise[18]=6;noise[19]=4;ap_p16(noise+20,2);
    memcpy(noise+22,ap_mac,6);ap_p32(noise+28,ap_ip);memcpy(noise+32,hs_spa,6);ap_p32(noise+38,0x0a00020f);
    ap_enqueue(noise,sizeof noise);
   }
   return;
  }
  if(ap.case_id>=62 && ap.case_id<=64){
   /* Model the hardware pattern: first Discover echoed, late Offer after the
    * existing Discover retry, then Request echoed with no/delayed ACK. */
   memcpy(r,p,n);memcpy(r,hs_spa,6);
   if(type==3 && ap.case_id==64)r[46]^=1; /* wrong transaction, valid IPv4 */
   ap_enqueue(r,n);
   if(type==1 && ap.dhcp==1){
    uint8_t noise[1514];ap_ip_header(noise,p,6,20);noise[0]|=1;
    /* Use a valid group PN above the synthetic M3 RSC, not a replay. */
    ap.rx_pn=1ull<<47;
    ap_enqueue(noise,54);return;
   }
   if(type==3){
    if(ap.case_id!=63)return;
    ap.ack_noise=16;ap.ack_release=test_time+350000;
   }
  }
  /* A unicast AP echo of client UDP is authenticated, not a server reply. */
  if((ap.case_id==60 || ap.case_id==61) && type==1){memcpy(r,p,n);memcpy(r,hs_spa,6);ap_enqueue(r,n);}
  if((ap.case_id==60 || ap.case_id==61) && type==3){ap.ack_noise=68;if(ap.case_id==60)ap.ack_release=test_time+350000;}
  if((ap.case_id==42 || ap.case_id==61) && type==3)return; /* no ACK, not a parser rejection */
  body=274;ap_ip_header(r,p,17,8+body);memset(v,0,body);v[0]=2;v[1]=1;v[2]=6;memcpy(v+4,u+12,4);ap_p32(v+16,0x0a00020f);memcpy(v+28,hs_spa,6);ap_p32(v+236,0x63825363);unsigned at=240;v[at++]=53;v[at++]=1;v[at++]=type==1?2:5;
  const unsigned kinds[]={54,1,3,6,51};const uint32_t values[]={0x0a000202,0xffffff00,0x0a000202,0x0a000202,3600};for(unsigned i=0;i<5;i++){v[at++]=kinds[i];v[at++]=4;ap_p32(v+at,values[i]);at+=4;}v[at++]=255;assert(at==body);ap_p32(r+30,0xffffffff);ap_p16(r+24,0);ap_p16(r+24,~ap_sum(r+14,20,0));ap_p16(r+34,67);ap_p16(r+36,68);
 }else if(dport==53){
  if(ap.case_id==59)return;
  ap.dns++;assert(body>=17 && ap_u16(u+12)==1);size_t query=body;body+=16;ap_ip_header(r,p,17,8+body);memcpy(v,u+8,query);ap_p16(v+2,0x8180);ap_p16(v+6,1);ap_p16(v+8,0);ap_p16(v+10,0);uint8_t *a=v+query;ap_p16(a,0xc00c);ap_p16(a+2,1);ap_p16(a+4,1);ap_p32(a+6,60);ap_p16(a+10,4);ap_p32(a+12,ap_ip);ap_p16(r+34,53);ap_p16(r+36,sport);
 }else{
  assert(dport==9000 && body==256);ap.udp++;ap_ip_header(r,p,17,8+body);memcpy(v,u+8,body);ap_p16(r+34,dport);ap_p16(r+36,sport);
 }
 ap_p16(r+38,8+body);ap_p16(r+40,0);bool saved_bad_mic=ap.bad_mic;
 if(sport==68 && dport==67 && ap.dhcp==2){
  if(ap.case_id==43)v[4]^=1;
  if(ap.case_id==44)v[267]=99; /* omit lease, preserve complete options */
  if(ap.case_id==45)ap_p16(r+40,1); /* bad nonzero UDP checksum */
  if(ap.case_id==46)ap_p32(r+30,0x0a000210); /* unconfigured destination */
  if(ap.case_id==47)v[242]=2; /* duplicate Offer instead of ACK */
  if(ap.case_id==48)v[242]=6; /* NAK remains a refusal */
  if(ap.case_id==49)ap_p32(r+26,0x0a000204); /* changed server source */
  if(ap.case_id==50)v[267]=54; /* duplicate server option */
  if(ap.case_id==51)ap_p16(r+20,0x2000); /* fragment */
  if(ap.case_id==52)ap_p16(r+38,7); /* short UDP header */
  ap_p16(r+24,0);ap_p16(r+24,~ap_sum(r+14,20,0));
  if(ap.case_id==53)ap.bad_mic=true;
  if(ap.case_id==54){v[240]=51;v[241]=0;} /* malformed before option53 */
 }
 ap_enqueue(r,42+body);ap.bad_mic=saved_bad_mic;
}
static void ap_http_request_check(void){
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 static const char suffix[]=" HTTP/1.0\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
 assert(http_peer_length==4159 && !memcmp(http_peer_request,"GET /base64/",12));
 assert(!memcmp(http_peer_request+12+4096,suffix,sizeof suffix-1));
 unsigned body=0;
 for(unsigned i=12;i<12+4096;i+=4){
  unsigned word=0;for(unsigned j=0;j<4;j++){const char *p=strchr(alphabet,http_peer_request[i+j]);assert(p && *p);word=(word<<6)|(unsigned)(p-alphabet);}
  for(unsigned j=0;j<3;j++){assert(((word>>(16-8*j))&255)==(unsigned)('A'+body%26));body++;}
 }
 assert(body==3072);
}
static const char stack_request[]="GET / HTTP/1.0\r\nHost: example.test\r\n\r\n";
static const char stack_header[]="HTTP/1.0 200 OK\r\nContent-Length: 3072\r\n\r\n";
static void ap_tcp_reply(const uint8_t *p,unsigned flags,uint32_t seq,uint32_t ack,const uint8_t *body,size_t n){
 uint8_t r[1514];ap_ip_header(r,p,6,20+n);if(http_reconnect_peer){memcpy(r+26,p+30,4);ap_p16(r+24,0);ap_p16(r+24,~ap_sum(r+14,20,0));}uint8_t *t=r+34;ap_p16(t,80);ap_p16(t+2,ap_u16(p+34));ap_p32(t+4,seq);ap_p32(t+8,ack);t[12]=0x50;t[13]=flags;ap_p16(t+14,65535);if(n)memcpy(t+20,body,n);ap_p16(t+16,~ap_sum(t,20+n,ap_sum(r+26,8,6+20+n)));ap_enqueue(r,54+n);
}
static void ap_arp_request(void){
 uint8_t r[42]={0};memcpy(r,hs_spa,6);memcpy(r+6,ap_mac,6);ap_p16(r+12,0x806);ap_p16(r+14,1);ap_p16(r+16,0x800);r[18]=6;r[19]=4;ap_p16(r+20,1);memcpy(r+22,ap_mac,6);ap_p32(r+28,ap_ip);ap_p32(r+38,0x0a00020f);ap_enqueue(r,sizeof r);
}
static void ap_freeze(void){ap.frozen=true;test_time_step=0;wifi_stack5.services=WIFI_STACK5_POLLS-12;}
static void ap_packet(const uint8_t *p,size_t n){
 assert(n>=42 && !memcmp(p+6,hs_spa,6));unsigned protocol=ap_u16(p+12);
 if(protocol==0x806){
  assert(n==42 && ap_u32(p+38)==ap_ip);
  if(ap_u16(p+20)==2){ap.arp_replies++;if(ap.arp_noise_left){ap.arp_noise_left--;ap_arp_request();}else if(ap.deferred_dns_size){ap_udp_reply(ap.deferred_dns,ap.deferred_dns_size);ap.deferred_dns_size=0;}return;}
  assert(ap_u16(p+20)==1);ap.arp++;if(ap.case_id==13){ap_freeze();return;}uint8_t r[42]={0};memcpy(r,hs_spa,6);memcpy(r+6,ap_mac,6);ap_p16(r+12,0x806);ap_p16(r+14,1);ap_p16(r+16,0x800);r[18]=6;r[19]=4;ap_p16(r+20,2);memcpy(r+22,ap_mac,6);ap_p32(r+28,ap_ip);memcpy(r+32,hs_spa,6);memcpy(r+38,p+28,4);ap_enqueue(r,sizeof r);return;
 }
 assert(protocol==0x800 && p[14]==0x45 && ap_u16(p+16)==n-14 && ap_sum(p+14,20,0)==65535);
 if(p[23]==17){
  if(ap.case_id==17 && ap_u16(p+36)==53){ap_freeze();return;}
  if(ap.case_id==16 && ap_u16(p+36)==53){memcpy(ap.deferred_dns,p,n);ap.deferred_dns_size=n;ap.arp_noise_left=9;ap_arp_request();return;}
  ap_udp_reply(p,n);
  if(ap.case_id==14 && ap_u16(p+36)==53)ap.provider_loss=true;
    return;
 }
 if(p[23]==1){uint8_t r[1514];ap.icmp++;if(ap.case_id==18){ap_freeze();return;}ap_ip_header(r,p,1,n-34);memcpy(r+34,p+34,n-34);r[34]=0;if(ap.case_id==12)r[42]^=1;ap_p16(r+36,0);ap_p16(r+36,~ap_sum(r+34,n-34,0));ap_enqueue(r,n);if(ap.case_id==19 && ap.icmp==1)ap_arp_request();return;}
 assert(p[23]==6);ap.tcp++;const uint8_t *t=p+34;unsigned header=(t[12]>>4)*4,size=(unsigned)n-34;assert(header>=20 && header<=size && ap_u16(t+2)==80 && ap_sum(t,size,ap_sum(p+26,8,6+size))==65535);uint32_t seq=ap_u32(t+4);unsigned flags=t[13];
 if(flags&2){assert(flags==2);ap.server_seq=9000;ap_tcp_reply(p,0x12,ap.server_seq++,seq+1,NULL,0);}
 else if(size>header){
  if(http_reconnect_peer){
   assert(size-header<=sizeof http_peer_request-http_peer_length);
   memcpy(http_peer_request+http_peer_length,t+header,size-header);http_peer_length+=size-header;
   ap_tcp_reply(p,0x10,ap.server_seq,seq+size-header,NULL,0);
   if(http_peer_length<sizeof http_peer_request)return;
   ap_http_request_check();
  }else assert(size-header==sizeof stack_request-1 && !memcmp(t+header,stack_request,sizeof stack_request-1));
  uint8_t bytes[sizeof stack_header-1+3072];memcpy(bytes,stack_header,sizeof stack_header-1);for(unsigned i=0;i<3072;i++)bytes[sizeof stack_header-1+i]=(uint8_t)('A'+i%26);
  for(unsigned at=0;at<sizeof bytes;){unsigned count=(unsigned)sizeof bytes-at;if(count>1100)count=1100;bool last=http_reconnect_peer && !http_reconnect_peer_no_fin && at+count==sizeof bytes;ap_tcp_reply(p,last?0x19:0x18,ap.server_seq,seq+size-header,bytes+at,count);ap.server_seq+=count+(unsigned)last;at+=count;}
 }else if(flags&1){ap_tcp_reply(p,http_reconnect_peer?0x10:0x11,ap.server_seq,seq+1,NULL,0);if(!http_reconnect_peer)ap.server_seq++;ap.closed=true;}
 else assert(flags==0x10);
}
static void stack_test_progress(void){
 if(wifi_stream5.connection_mode && wifi_stream5.data_active){
  unsigned g=wifi_stream5_current==&wifi_stream5_slots[1];
  if(!reconnect_keys_seen[g]){
   assert(wifi_stream5.exchange.keys.phase==WPA2_CCMP_ACTIVE && !wifi_stream5.exchange.keys.tx_pn && !wifi_stream5.data_pn);
   assert(wifi_stream5.exchange.keys.generation==wifi_stream5.epoch);
   if(g)assert(memcmp(reconnect_first_tk,wifi_stream5.exchange.keys.tk,16));
   else memcpy(reconnect_first_tk,wifi_stream5.exchange.keys.tk,16);
   reconnect_keys_seen[g]=true;
  }
 }

 if(wifi_stream5.connection_mode && wifi_stream5.data_active && wifi_stream5.epoch>reconnect_old_epoch && reconnect_hook_count==2 && !ap.tx_pn && !ap.rx_pn){
  if(reconnect_case_id==1){
   struct mt76_net5 before=wifi_stack5_net;
   assert(!wifi_stack5_owned(&wifi_stream5_slots[0],reconnect_old_epoch));
   assert(!wifi_stack5_service(&wifi_stream5_slots[0],reconnect_old_epoch));
   assert(!mt76_net5_tx_complete(&wifi_stack5_net,reconnect_old_epoch,reconnect_old_token,true));
   assert(!memcmp(&before,&wifi_stack5_net,sizeof before));
  }
  if(reconnect_case_id==2 || reconnect_case_id==15){struct mt76_dma5_lease *old=&wifi_stream5_slots[0].lease[1];struct xhci_trb e={.parameter_low=(uint32_t)(reconnect_case_id==15?wifi_stream5_slots[0].physical[1]+126*16:old->trb),.parameter_high=old->trb>>32,.status=(13u<<24)|4096,.control=(32u<<10)|(9u<<16)|(1u<<24)};stream_enqueue(&e,0);return;}
 }

 if(wifi_stream5.connection_mode && wifi_stream5.data_active && reconnect_hook_count==1 && !ap.tx_pn && !ap.rx_pn){
  if(reconnect_case_id==13){wifi_stream5.dma.positions=63;return;}
  if(reconnect_case_id==14){wifi_stream5.ring[0][127].control^=2;return;}
 }
 if(!wifi_stream5.data_active){
  /* Fault only the modeled AP response,after actual AUTH publication. The
   * production owner,decoder,timeout,cleanup and workload remain unmodified. */
  if(ap.case_id==21 && sim.response==1)sim.response=0;
  if(ap.case_id==24 && !wifi_stream5.pending[0] && wifi_stream5.pending[1] && sim.tx_seen==1)
   sim.response=sim.rx_seen<25?6:0;
  bool negative=(ap.case_id==22 || ap.case_id==23) && sim.response==1 && !wifi_stream5.pending[0] && wifi_stream5.pending[1];
  unsigned page=wifi_stream5.lease[1].index;
  stream_test_progress();
  if(negative)stream_le16(wifi_stream5.buffer[1][page]+36+(ap.case_id==22?28:24),ap.case_id==22?13:1);
  if((ap.case_id==21 || ap.case_id==24) && wifi_stream5.exchange.auth_sent && !wifi_stream5.exchange.auth_matched)test_time+=1000;
  return;
 }
 if(wifi_stream5.pending[0] && wifi_stream5.lease[0].ticket!=ap.held_ticket){
  struct mt76_dma5_lease *l=&wifi_stream5.lease[0];
  if(ap.drop_tx)return;
  assert(sim.tx_seen+1==wifi_stream5.posts[0]);sim.tx_seen++;
  if(wifi_stream5.connection_mode && reconnect_hook_count==1 && !reconnect_old_usb_size){reconnect_old_usb_size=l->length;memcpy(reconnect_old_usb,wifi_stream5.buffer[0][l->index],l->length);}
  uint8_t eth[1514];size_t n=0;uint64_t pn=0;assert(stack5_peer_open(wifi_stream5.buffer[0][l->index],l->length,eth,sizeof eth,&n,&pn));assert(pn==++ap.tx_pn);ap_packet(eth,n);
  if(ap.case_id==58 && ap.dhcp){ap.held_ticket=l->ticket;return;}
  if(ap.case_id==15 && ap.icmp==64){ap.held_ticket=l->ticket;return;}
  uint64_t q=wifi_stream5.physical[0]+16*wifi_stream5.enqueue[0];context_write32(wifi_stream5.d->output_context,8,0,1);context_write32(wifi_stream5.d->output_context,8,2,q|wifi_stream5.cycle[0]);context_write32(wifi_stream5.d->output_context,8,3,q>>32);
  struct xhci_trb e={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=1u<<24,.control=(32u<<10)|(8u<<16)|((uint32_t)wifi_stream5.slot<<24)};if(ap.wrong_event)e.parameter_low+=16;stream_enqueue(&e,0);if(ap.case_id==9)stream_enqueue(&e,1);return;
 }
 if(wifi_stream5.pending[1]){
  if(ap.case_id>=10 && !ap.frozen && !ap.count)test_time+=1000;
  unsigned bytes=0;
  if(ap.ack_noise && (ap.dhcp==2 || (ap.case_id>=62 && ap.case_id<=64))){uint8_t beacon[64]={0x80};bytes=stream_wrap(beacon,sizeof beacon);ap.ack_noise--;}
  else if(ap.ack_release && test_time<ap.ack_release){test_time+=1000;}
  else if(wifi_stream5.connection_mode && reconnect_noise() && ap.background_left){uint8_t beacon[24]={0x80};bytes=stream_wrap(beacon,sizeof beacon);ap.background_left--;}
  else if(ap.count){bytes=stream_wrap(ap.response[ap.head],(unsigned)ap.length[ap.head]);ap.head=(ap.head+1)%8;ap.count--;if(wifi_stream5.connection_mode && reconnect_noise())ap.background_left=reconnect_noise();}
  else if(!ap.quiet && wifi_stack5.boundary){uint8_t beacon[24]={0x80};bytes=stream_wrap(beacon,sizeof beacon);}
  if(bytes){struct mt76_dma5_lease *l=&wifi_stream5.lease[1];uint64_t q=wifi_stream5.posts[1]%wifi_stream5_positions()?wifi_stream5.physical[1]+16*wifi_stream5.enqueue[1]:wifi_stream5.physical[1]+wifi_stream5_positions()*16;unsigned cycle=wifi_stream5.posts[1]%wifi_stream5_positions()?(unsigned)wifi_stream5.cycle[1]:l->cycle;context_write32(wifi_stream5.d->output_context,9,0,1);context_write32(wifi_stream5.d->output_context,9,2,q|cycle);context_write32(wifi_stream5.d->output_context,9,3,q>>32);struct xhci_trb e={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=(13u<<24)|(4096-bytes),.control=(32u<<10)|(9u<<16)|((uint32_t)wifi_stream5.slot<<24)};if(ap.provider_loss)wifi_stream5.d->removed=true;stream_enqueue(&e,0);return;}
 }
 if(xhci.command_pending && wifi_fence5_native.round)stream_test_progress();
}
static bool stack_guard_current(void *c){struct mt76_net5 *p=c;return p==&wifi_stack5_net && p->state==MT76_NET5_UP && wifi_stack5_owned(&wifi_stream5,p->epoch);}
static bool stack_guard_retry(void *c){struct mt76_net5 *p=c;return stack_guard_current(c) && !p->count && !p->pending && p->submitted==p->completed && !wifi_stack5.tx_token && !wifi_stream5.pending[0];}
static bool stack_poll_until_idle(struct mt76_net5 *p,unsigned limit){for(unsigned i=0;i<limit;i++){test_time+=1000;net_device_poll(32,false);if(p->state!=MT76_NET5_UP)return false;if(!p->count && !p->pending && !wifi_stack5.tx_token && !ap.count && !wifi_stream5.cached_present && !wifi_stream5.ready_rx && !wifi_stack5.boundary && !wifi_fence5_native.round)return true;}return false;}
static bool stack_exercise(void *context,struct mt76_net5 *p){
 assert(context==&ap && p==&wifi_stack5_net && wifi_stream5.exchange.handshake.install_handoffs==1);
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
 stream_setup();memset(&wifi_stack5,0,sizeof wifi_stack5);memset(&wifi_stack5_net,0,sizeof wifi_stack5_net);memset(&ap,0,sizeof ap);ap.case_id=which;ap.quiet=which==2;ap.bad_mic=which==3;bool good=xhci_wifi_capture_stack5(stack_exercise,&ap);
 if(which<2){assert(good && wifi_stream5.complete && wifi_stream5.retired && wifi_stack5.retired && wifi_stack5.detached && !wifi_stack5_net.io.context);assert(ap.dhcp==(which==1?3u:2u) && ap.udp==52 && ap.icmp==1 && ap.dns==1);}
 else{assert(!good && !wifi_stream5.complete);if(which==2 || which>=4)assert(wifi_stream5.failed && !wifi_stream5.retired && !wifi_stack5.detached && wifi_stack5_net.io.context);}
 assert(wifi_stream5.exchange.keys_erased && !wifi_stream5.exchange.handshake.keys_may_live);reset_test();
}
static void probe_setup(unsigned which){stream_setup();memset(&wifi_stack5,0,sizeof wifi_stack5);memset(&wifi_stack5_net,0,sizeof wifi_stack5_net);memset(&wifi_stack5_probe_result,0,sizeof wifi_stack5_probe_result);memset(&ap,0,sizeof ap);ap.case_id=which;}
static void probe_size_projection(void){
 probe_setup(10);assert(xhci_wifi_capture_stack5_probe());batch_trace=true;fputs("wifi-stack5-size-projection: synthetic=1 acceptance=0\n",stdout);
 wifi_stream5.record_count=WIFI_STREAM5_TX+WIFI_STREAM5_RX;wifi_stream5.completions[1]=WIFI_STREAM5_RX;
 for(unsigned i=0;i<wifi_stream5.record_count;i++){struct wifi_stream5_record *r=&wifi_stream5.records[i];unsigned dir=i>=WIFI_STREAM5_TX,token=dir?i-WIFI_STREAM5_TX+1:i+1;*r=(struct wifi_stream5_record){.ticket=i+1,.token=token,.pn=75,.direction=dir,.page=3,.cycle=1,.bytes=1580,.ethernet=dir?1514:0,.reason=9,.kind=(!dir && i>=4 && i<79)?5:0};r->event=(struct xhci_trb){.parameter_low=0xfffff3e0,.parameter_high=UINT32_MAX,.status=(dir?13u:1u)<<24,.control=(255u<<24)|((dir?9u:8u)<<16)|(32u<<10)|1};}
 for(unsigned phase=0;phase<10;phase++)for(unsigned d=0;d<2;d++)for(unsigned w=0;w<8;w++)wifi_stream5.snapshots[phase][d][w]=UINT32_MAX;
 for(unsigned i=0;i<6;i++){wifi_stream5.sync[i].slot=255;wifi_stream5.sync[i].pointer=wifi_stream5.sync[i].event_pointer=wifi_stream5.sync[i].dequeue=UINT64_MAX;}
 wifi_stream5.wraps[0]=1;wifi_stream5.wraps[1]=3;wifi_stream5.posts[0]=80;wifi_stream5.posts[1]=252;wifi_stream5.data_sends=75;wifi_stream5.data_received=252;wifi_stream5.data_after_txwrap=16;wifi_stream5.data_after_rxwrap=189;wifi_stream5.dma.wraps=4;wifi_stream5.dma.borrowed=wifi_stream5.dma.released=252;
 wifi_stream5.fault_event_valid=true;wifi_stream5.fault_event=(struct xhci_trb){UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};wifi_stream5.fault_phase=9;wifi_fence5_native.count=8;
 for(unsigned i=0;i<8;i++){struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[i];r->id.pointer=UINT64_MAX-16;r->id.dequeue=UINT64_MAX-4095;r->id.type=15+(i&1);r->id.dci=9;r->id.slot=255;r->consumed=true;r->event=(struct xhci_trb){0xfffffff0,UINT32_MAX,1u<<24,(255u<<24)|(33u<<10)|1};if(!(i&1)){r->stop=(struct xhci_trb){0xfffff3e0,UINT32_MAX,27u<<24,(255u<<24)|(9u<<16)|(32u<<10)|1};r->stop_dequeue=UINT64_MAX-4095;}}
 wifi_stack5_net.enqueued=wifi_stack5_net.submitted=wifi_stack5_net.completed=75;wifi_stack5_net.received=252;wifi_stack5.services=WIFI_STACK5_POLLS;wifi_stack5.start=0;wifi_stack5.last=WIFI_STACK5_TOTAL_US;
 wifi_stream5.exchange.polls=wifi_stream5.exchange.ignored=80000;wifi_stream5.exchange.status=wifi_stream5.exchange.aid=65535;
 wifi_stream5.auth_observation.present=wifi_stream5.auth_observation.peer=wifi_stream5.auth_observation.ok=wifi_stream5.auth_observation.invalid=wifi_stream5.auth_observation.rejected=20000;wifi_stream5.auth_observation.clocks=20001;wifi_stream5.auth_observation.first=0;wifi_stream5.auth_observation.last=UINT64_MAX;wifi_stream5.auth_observation.rollback=true;
 struct mt76_stack5_probe *r=&wifi_stack5_probe_result;r->retries=1;r->setup_tx=11;r->data_tx=75;r->arp=75;r->polls=MT76_STACK5_POLLS;r->services=WIFI_STACK5_POLLS;r->elapsed_us=WIFI_STACK5_TOTAL_US;for(unsigned i=0;i<64;i++)r->ping_pn[i]=12+i;
 (void)wifi_stream5_finish(false);
 /* Final fields are Boolean-width;the production format is emitted here only
  * for sizing after its actual callback/retirement path passed above. */
 console_write("wifi-stack5-final: complete=1 detached=1 provider_retired=1 retained=1 backend_retired=0\n");mt76_stack5_probe_emit(r);
}
static unsigned alternate_sends;
static bool alternate_send(void *c,const uint8_t *p,uint16_t n){(void)c;(void)p;(void)n;alternate_sends++;assert(0);return false;}
static uint64_t alternate_poll(void *c,uint64_t n,bool verbose){(void)c;(void)n;(void)verbose;return 0;}
static bool alternate_link(void *c,struct net_device_link *out){(void)c;*out=(struct net_device_link){.up=true};return true;}
static void probe_setup(unsigned);
/* Real original RX leases through slot63; noise is supplied only while
 * preparing earlier slots. No positive boundary test relies on the old
 * stack_test_progress synthetic beacon-at-boundary escape hatch. */
static void boundary_noise_completion(void){
 assert(wifi_stream5.pending[1]);uint8_t beacon[24]={0x80};
 unsigned bytes=stream_wrap(beacon,sizeof beacon);struct mt76_dma5_lease *l=&wifi_stream5.lease[1];
 uint64_t q=wifi_stream5.posts[1]%wifi_stream5_positions()?wifi_stream5.physical[1]+16*wifi_stream5.enqueue[1]:wifi_stream5.physical[1]+wifi_stream5_positions()*16;
 unsigned cycle=wifi_stream5.posts[1]%wifi_stream5_positions()?(unsigned)wifi_stream5.cycle[1]:l->cycle;
 context_write32(wifi_stream5.d->output_context,9,2,q|cycle);
 struct xhci_trb e={.parameter_low=l->trb,.parameter_high=l->trb>>32,.status=(13u<<24)|(4096-bytes),.control=(32u<<10)|(9u<<16)|((uint32_t)wifi_stream5.slot<<24)};
 stream_enqueue(&e,0);assert(wifi_stream5_dispatch());assert(wifi_stream5_collect(MT76_WPA5_DATA,false));
}
static struct xhci_trb boundary_hid_event;
static unsigned boundary_hid_arrivals;
extern bool http_native_test_current(struct http_network_probe *);
static void boundary_hid_arrive(void){
 if(!wifi_stack5.boundary || wifi_fence5_native.round)return;
 native_boundary_clock_hook=NULL;boundary_hid_arrivals++;
 stream_enqueue(&boundary_hid_event,0);
}
static bool active_stop_exercise(struct mt76_net5 *p){
 unsigned mode=ap.case_id-28;assert(mode<12);
 assert(net_init_bounded_probe() && net_device_selected_is(&p->ops));ap.quiet=true;
 while(wifi_stream5.posts[1]<63){if(wifi_stream5.pending[1])boundary_noise_completion();assert(mt76_net5_service(p));}
 boundary_noise_completion();assert(mt76_net5_service(p) && wifi_fence5_native.round);
 assert(mt76_net5_service(p) && xhci.command_pending);
 struct wifi_fence5_native_command *r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];
 assert(r->id.type==15 && r->id.dci==9 && !r->stop_event && !r->latched);
 struct http_network_probe h={0};
 struct http_network_probe_io io={.context=&wifi_stream5,.current=wifi_stack5_probe_current,.epoch=p->epoch,.start_us=wifi_stack5.start,.deadline_us=wifi_stack5.start+WIFI_STACK5_TOTAL_US};
 h.self=(uintptr_t)&h;h.io=h.io_guard=io;h.source=h.source_guard=&io;
 h.last_us=test_time;h.last_tick=timer_ticks();h.result.phase=HTTP_NET_DRAIN;
 assert(http_native_test_current(&h));
 uint64_t ring=wifi_stream5.physical[1],ptr=mode==1?ring:ring+63*16;
 unsigned cycle=mode==1?(wifi_stream5.dma.cycle[1]^1u):wifi_stream5.dma.cycle[1];
 struct xhci_trb stop={.parameter_low=ptr,.parameter_high=ptr>>32,.status=27u<<24,.control=(32u<<10)|(9u<<16)|((uint32_t)wifi_stream5.slot<<24)};
 struct xhci_trb cc={.parameter_low=r->id.pointer,.parameter_high=r->id.pointer>>32,.status=1u<<24,.control=(33u<<10)|((uint32_t)wifi_stream5.slot<<24)};
 assert((context_read32(wifi_stream5.d->output_context,9,0)&7)==1);
 if(mode==2)dispatch_transfer_event(&cc);
 else{
  if(mode==3)stop.parameter_low-=16;
  if(mode==9)stop.control^=1u<<24;
  if(mode==10)stop.status=26u<<24;
  /* The controller publishes this event before its final context write. */
  dispatch_transfer_event(&stop);
  if(mode==11)dispatch_transfer_event(&stop);
  if(mode<2){assert(r->stop_event && !r->latched && !r->stop_dequeue);assert(http_native_test_current(&h));}
  context_write32(wifi_stream5.d->output_context,9,0,mode==5?1:3);
  uint64_t deq=ptr|cycle;
  if(mode==4)deq^=1;
  if(mode==6)deq=ring+16+cycle;
  if(mode==7)deq|=2;
  context_write32(wifi_stream5.d->output_context,9,2,deq);context_write32(wifi_stream5.d->output_context,9,3,deq>>32);
  if(mode==8)cc.status=2u<<24;
  dispatch_transfer_event(&cc);
 }
 if(mode>=2){
  assert(!http_native_test_current(&h) && h.guard_failed && h.result.phase==HTTP_NET_DRAIN && h.result.reason==HTTP_NET_OWNER);
  assert(!wifi_stack5.failed && !wifi_stream5.failed && wifi_stream5.phase==3 && !wifi_stream5.fault_phase);
  assert(wifi_fence5_native.round && wifi_fence5_native.failed && xhci.command_pending && !wifi_stream5.dma.wraps && wifi_stream5.dma.state==MT76_DMA5_FAULT);
  assert(wifi_stream5.exchange.keys.phase==WPA2_CCMP_ACTIVE);
  return false;
 }
 assert(r->latched && r->stop_dequeue==(ptr|cycle) && http_native_test_current(&h));
 for(unsigned i=0;i<24 && wifi_fence5_native.round;i++){
  assert(wifi_fence5_step());
  if(xhci.command_pending){
   r=&wifi_fence5_native.commands[wifi_fence5_native.count-1];if(r->latched)continue;
   assert(r->id.type==16);context_write32(wifi_stream5.d->output_context,9,0,3);
   context_write32(wifi_stream5.d->output_context,9,2,r->id.dequeue);context_write32(wifi_stream5.d->output_context,9,3,r->id.dequeue>>32);
   cc.parameter_low=r->id.pointer;cc.parameter_high=r->id.pointer>>32;dispatch_transfer_event(&cc);
  }
 }
 assert(!wifi_fence5_native.round && !wifi_fence5_native.failed && wifi_stream5.dma.wraps==1 && http_native_test_current(&h));
 assert(!h.guard_failed && !h.result.reason && wifi_stream5.exchange.keys.phase==WPA2_CCMP_ACTIVE);
 return false; /* No HTTP child was run by this callback-level fixture. */
}
static bool boundary_exercise(void *context,struct mt76_net5 *p){
 assert(context==&ap && p==&wifi_stack5_net);ap.quiet=true;
 if(ap.case_id>=28 && ap.case_id<40)return active_stop_exercise(p);
 if(ap.case_id==24){
  assert(wifi_stack5_probe_current(&wifi_stream5,p->epoch));
  assert(net_init_bounded_probe() && !net_device_selected_is(&p->ops));
  assert(!wifi_stack5_probe_current(&wifi_stream5,p->epoch));
  assert(!wifi_stream5.failed && !wifi_stack5.failed && !p->enqueued && !alternate_sends);
  return false;
 }
 assert(net_init_bounded_probe() && net_device_selected_is(&p->ops));
 while(wifi_stream5.posts[1]<63){
  if(wifi_stream5.pending[1])boundary_noise_completion();
  assert(mt76_net5_service(p));
 }
 assert(wifi_stream5.dma.cursor[1]==63 && wifi_stream5.pending[1]);
 assert(mt76_net5_service(p) && !wifi_stack5.boundary && wifi_stack5_ready(&wifi_stream5,p->epoch));
 if(ap.case_id==40){
  boundary_noise_completion();
  boundary_hid_event=(struct xhci_trb){.parameter_low=xhci.command_ring_phys,.parameter_high=xhci.command_ring_phys>>32,.status=1u<<24,.control=(33u<<10)|((uint32_t)wifi_stream5.slot<<24)};
  boundary_hid_arrivals=0;native_boundary_clock_hook=boundary_hid_arrive;
  uint64_t enq=xhci.command_enqueue;
  assert(mt76_net5_service(p) && boundary_hid_arrivals==1 && wifi_stack5.boundary && !wifi_fence5_native.round);
  assert(!mt76_net5_service(p) && wifi_fence5_native.failed && wifi_fence5_native.fault_event_valid && wifi_stream5.failed);
  assert(wifi_stream5.dma.state==MT76_DMA5_FAULT && !wifi_fence5_native.count && xhci.command_enqueue==enq && !wifi_stream5.dma.wraps);
  assert(!wifi_stream5.retired && !wifi_stack5.retired);
  return false;
 }
 if(ap.case_id==26){
  boundary_noise_completion();
  struct xhci_device *hid=&xhci.devices[1];void *ring=NULL,*buffer=NULL;
  *hid=(struct xhci_device){.present=true,.slot_id=8,.hid_mouse=true,.interrupt_ready=true,.interrupt_cycle=true,.interrupt_dci=3,.interrupt_max_packet=8};
  assert(alloc_frame_zero(&hid->interrupt_ring_phys,&ring));hid->interrupt_ring=ring;
  assert(alloc_frame_zero(&hid->interrupt_buffer_phys,&buffer));hid->interrupt_buffer=buffer;
  submit_interrupt_poll(hid);assert(hid->interrupt_pending);
  boundary_hid_event=(struct xhci_trb){.parameter_low=hid->interrupt_trb,.parameter_high=hid->interrupt_trb>>32,.status=1u<<24,.control=(32u<<10)|(3u<<16)|(8u<<24)};
  boundary_hid_arrivals=0;native_boundary_clock_hook=boundary_hid_arrive;
  uint64_t enq=xhci.command_enqueue,events=xhci.command_events;
  assert(mt76_net5_service(p));
  assert(boundary_hid_arrivals==1 && wifi_stack5.boundary && !wifi_fence5_native.round);
  assert(!wifi_fence5_native.failed && wifi_stream5.dma.state==MT76_DMA5_RUNNING);
  assert(xhci.command_enqueue==enq && xhci.command_events==events && !xhci.command_pending);
  assert(mt76_net5_service(p) && hid->interrupt_success==1);
  for(unsigned i=0;i<2000 && (wifi_stack5.boundary || wifi_fence5_native.round);i++)assert(mt76_net5_service(p));
  assert(!wifi_stack5.boundary && !wifi_fence5_native.round && wifi_stream5.wraps[1]==1);
 }
 if(ap.case_id==27){
  uint64_t start=test_time;
  while(test_time-start<800000){test_time+=1000;assert(mt76_net5_service(p));}
  struct mt76_stack5_observation o={0};
  assert(wifi_stream5.pending[1] && !wifi_stack5.boundary && wifi_stack5_probe_observe(&wifi_stream5,p->epoch,&o) && o.drained);
  assert(wifi_stack5_terminal_rx_handoff(&wifi_stream5,p->epoch));
  assert(wifi_stack5.terminal_pending && p->state==MT76_NET5_CLOSING && !wifi_stream5.failed);
  return false;
 }
 const struct net_dhcp_retry_guard guard={p,stack_guard_current,stack_guard_retry};
 assert(net_dhcp_request_discover_retry(&guard)>=0);
 assert(stack_poll_until_idle(p,2000) && ap.dhcp==2 && p->received==2 && p->submitted==p->completed);
 assert(wifi_stream5.wraps[1]==1 && !wifi_stream5.failed && !wifi_stack5.failed);
 return true;
}
static void boundary_case(unsigned which){
 probe_setup(which);
 if(which==24){struct net_device_ops ops={.name="foreign",.context=&alternate_sends,.priority=1000,.send=alternate_send,.poll=alternate_poll,.link=alternate_link};memcpy(ops.mac,hs_spa,6);assert(net_device_register(&ops));}
 bool good=xhci_wifi_capture_stack5(boundary_exercise,&ap);
 if(which==25 || which==26)assert(good && wifi_stream5.retired && wifi_stack5.retired && wifi_stack5.detached);
 else assert(!good);
 assert(!native_boundary_clock_hook && wifi_stream5.exchange.keys_erased);
 reset_test();
}
static void probe_case(unsigned which){
 probe_setup(which);
 if(which==20){struct net_device_ops ops={.name="mt76-ccmp",.context=&alternate_sends,.priority=1000,.send=alternate_send,.poll=alternate_poll,.link=alternate_link};memcpy(ops.mac,hs_spa,6);assert(net_device_register(&ops));}
 bool good=xhci_wifi_capture_stack5_probe();struct mt76_stack5_probe *r=&wifi_stack5_probe_result;
 if(which>=21 && which<=23){
  const struct mt76_wpa5 *s=&wifi_stream5.exchange;
  assert(!good && s->attempted && s->auth_sent && !s->auth_matched && !s->assoc_sent && !s->assoc_matched);
  assert(s->sends==2 && s->polls && !s->unsafe && !s->ignored && !s->aid && s->status==(which==22?13:0));
  assert(s->keys_erased && !s->keys_installed && !s->keys_activated && !s->leave_sent && !s->handshake.keys_may_live);
  assert(!r->attempted && !r->version && !r->complete && !r->started && !r->matched && !r->private_erased);
  assert(wifi_stream5.posts[0]==1 && !wifi_stream5.data_sends && !wifi_stack5.services && !wifi_stack5_net.enqueued && !ap.dhcp);
  assert(!wifi_stream5.failed && wifi_stream5.restored && !wifi_stream5.retired && !wifi_stack5.retired && !wifi_stack5.detached);
  assert(wifi_stream5.dma.borrowed==wifi_stream5.dma.released && !wifi_stream5.dma.faults);
  assert(wifi_stream5.auth_observation.clocks>=2 && !wifi_stream5.auth_observation.rollback && !wifi_stream5.auth_observation.ok);
  assert(wifi_stream5.auth_observation.present==(which==21?0u:1u) && wifi_stream5.auth_observation.peer==(which==21?0u:1u));
  assert(wifi_stream5.auth_observation.rejected==(which==22) && wifi_stream5.auth_observation.invalid==(which==23));
  reset_test();return;
 }
 if(which==10 || which==11 || which==19){
  if(!good)fprintf(stderr,"probe refused phase%u reason%u net%u pings%u tx%u rx%u native%u fence%u services%llu\n",r->phase,r->reason,wifi_stack5_net.state,r->matched,wifi_stream5.data_sends,(unsigned)wifi_stream5.posts[1],wifi_stream5.failed,wifi_fence5_native.failed,(unsigned long long)wifi_stack5.services);
  assert(good && r->complete && r->matched==64 && r->exact==64 && r->body_bytes==2048 && r->private_erased);
  assert(wifi_stream5.retired && wifi_stack5.retired && wifi_stack5.detached && !wifi_stack5_net.io.context && ap.icmp==64 && !ap.udp && !ap.tcp && ap.dns==1 && ap.dhcp==(which==11?3u:2u));
  assert(r->retries==(which==11) && wifi_stack5.icmp_count==64);
  for(unsigned i=0;i<64;i++)assert(r->ping_pn[i] && (!i || r->ping_pn[i]>r->ping_pn[i-1]));
  if(which==19){unsigned gaps=0;for(unsigned i=1;i<64;i++)gaps+=r->ping_pn[i]>r->ping_pn[i-1]+1;assert(ap.arp_replies==1 && gaps==1);}
 }else{
  assert(!good && !r->complete && r->private_erased && r->reason);
  if(which==12)assert(r->phase==MT76_STACK5_PING && !r->matched && ap.icmp==1);
  if(which==13 || which==17)assert(r->phase==MT76_STACK5_DNS && wifi_stream5.failed && !wifi_stream5.retired);
  if(which==14)assert(r->phase==MT76_STACK5_DNS && wifi_stream5.d->removed && !wifi_stream5.retired);
  if(which==15)assert(r->matched==63 && wifi_stack5_net.pending && wifi_stream5.pending[0] && !wifi_stack5.detached && !wifi_stream5.retired);
  if(which==16)assert(r->reason==MT76_STACK5_CREDIT && !r->started && !ap.icmp);
  if(which==18)assert(r->phase==MT76_STACK5_PING && !r->matched && wifi_stream5.failed && !wifi_stream5.retired);
  if(which==20)assert(r->reason==MT76_STACK5_INITIALIZATION && !alternate_sends && !wifi_stream5.data_sends && !ap.dhcp);
 }
 assert(wifi_stream5.exchange.keys_erased && !wifi_stream5.exchange.handshake.keys_may_live);reset_test();
}
/* Real bounded net.c API through registry/provider/CCMP and the production
 * xHCI dispatcher/fence path. Only the AP reply and USB/MMIO are modeled.
 * Successful DHCP is not a claim to run an HTTP ELF or a prepared worker. */
static struct net_bounded_probe native_dhcp;
static bool native_dhcp_exercise(void *context,struct mt76_net5 *p){
 assert(context==&ap && p==&wifi_stack5_net && net_init_bounded_probe());
 ap.quiet=true;
 /* Force DHCP across a real modeled receive-ring boundary, without synthetic
  * boundary noise: original leases carry beacons before the transaction. */
 while(wifi_stream5.posts[1]<62){
  if(wifi_stream5.pending[1])boundary_noise_completion();
  assert(mt76_net5_service(p));
 }
 struct net_bounded_probe_io io={&wifi_stream5,wifi_stack5_probe_current,wifi_stack5_probe_retry};
 assert(net_bounded_probe_init(&native_dhcp,&io,p->epoch,wifi_stack5.start+WIFI_STACK5_TOTAL_US));
 int64_t result=net_dhcp_request_bounded(&native_dhcp);
 const struct net_dhcp_observation *o=&native_dhcp.dhcp_observation;
 assert(o->version==1 && o->started && o->finished && o->discovers==1 && o->discover_queued==1 &&
  o->requests==1 && o->request_queued==1 && o->accepted_offer==1 && o->have_offer && o->request_sent &&
  !native_dhcp.dhcp_retries && o->polls>0 && o->polls<20000 && o->elapsed_us<=2001000);
 if(ap.case_id==41){
  assert(result==0x0a00020f && o->success && o->configured && o->end_reason==NET_DHCP_SUCCESS &&
   o->frames==2 && o->ipv4==2 && o->udp==2 && o->dhcp==2 && o->offers==1 && o->acks==1 &&
   o->accepted_ack==1 && !o->rejected && !o->reject_mask && o->last_options==63);
 }else{
  assert(result<0 && !o->success && !o->configured && !o->accepted_ack && o->end_reason==NET_DHCP_TIME_LIMIT);
  unsigned reason=0;
  switch(ap.case_id){
   case 42:case 53:assert(o->frames==1 && o->dhcp==1 && o->acks==0);break;
   case 43:reason=NET_DHCP_RX_XID;break;
   case 44:reason=NET_DHCP_RX_ACK_CONFIG;assert(o->acks==1 && o->last_options==31);break;
   case 45:reason=NET_DHCP_RX_UDP_CHECKSUM;assert(o->dhcp==1);break;
   case 46:reason=NET_DHCP_RX_IP_DESTINATION;assert(o->ipv4==2 && o->udp==1);break;
   case 47:reason=NET_DHCP_RX_OFFER_STATE;assert(o->offers==2 && !o->acks);break;
   case 48:reason=NET_DHCP_RX_MESSAGE;assert(o->naks==1 && !o->acks);break;
   case 49:reason=NET_DHCP_RX_ACK_BINDING;assert(o->acks==1);break;
   case 50:reason=NET_DHCP_RX_OPTION_DUPLICATE;assert(o->acks==1);break;
   case 51:reason=NET_DHCP_RX_IP_FRAGMENT;break;
   case 52:reason=NET_DHCP_RX_UDP_LENGTH;break;
   case 54:reason=NET_DHCP_RX_OPTION_SHAPE;assert(!o->last_message && o->last_options==32 && !o->acks);break;
   default:assert(0);
  }
  assert(o->rejected==(reason?1u:0u) && o->first_reject==reason && o->last_reject==reason &&
   o->reject_mask==(reason?1ull<<reason:0));
 }
 struct net_dhcp_observation kept=*o;
 assert(stack_poll_until_idle(p,2000));
 assert(!memcmp(&kept,o,sizeof kept)); /* no observation outside this call */
 bool finished=net_bounded_probe_finish(&native_dhcp);
 assert(finished==(ap.case_id==41) && native_dhcp.closed && !native_dhcp.source);
 assert(!memcmp(&kept,o,sizeof kept));
 assert(wifi_stream5.wraps[1]==(ap.case_id==42?0u:1u) && !wifi_stream5.failed && !wifi_stack5.failed);
 return result>=0;
}
static void native_dhcp_case(unsigned which){
 probe_setup(which);bool good=xhci_wifi_capture_stack5(native_dhcp_exercise,&ap);
 assert(good==(which==41));
 if(good)assert(wifi_stream5.retired && wifi_stack5.retired && wifi_stack5.detached);
 assert(wifi_stream5.exchange.keys_erased && !wifi_stream5.exchange.handshake.keys_may_live);
 if(batch_trace){
  struct http_network_probe emitted={0};
  emitted.bootstrap.dhcp_observation=native_dhcp.dhcp_observation;
  fputs("native-dhcp-fixture: actual_net_provider_crypto=1 modeled_usb=1 http_process=0 acceptance=0\n",stdout);
  http_network_probe_emit(&emitted);
 }
 reset_test();
}
static void reconnect_case(unsigned which){
 assert(which<19);stream_setup();sim.parent_after_fail=which==7;xhci.command_enqueue=which==6?47:41;reconnect_case_id=which;reconnect_hook_count=0;sim.link=which>=16;sim.fault=which==17?40:which==18?41:0;
 if(reconnect_seeded){for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,d?8:9,i,reconnect_seed[d][i]);wifi_cal5.d->slot_id=(uint8_t)reconnect_slot;}
 assert(xhci_wifi_capture_reconnect5_probe()==(which<2 || which==9 || which==10 || which==11 || which==16));
 if(which<2 || which==9 || which==10 || which==11 || which==16){
  assert(wifi_reconnect5.complete && wifi_reconnect5.connections==2 && reconnect_hook_count==2 && reconnect_keys_seen[0] && reconnect_keys_seen[1]);
  assert(wifi_reconnect5.epochs[1]==wifi_reconnect5.epochs[0]+1 && wifi_reconnect5.tokens[1]>wifi_reconnect5.tokens[0]);
  assert(wifi_reconnect5.probe[0].matched==64 && wifi_reconnect5.probe[1].matched==64 && ap.dhcp==2 && ap.dns==1 && ap.icmp==64);
  assert(wifi_stack5_net.state==MT76_NET5_RETIRED && wifi_stack5_net.registry_drained);
  assert(!memcmp(&reconnect_old_stream,&wifi_stream5_slots[0],sizeof reconnect_old_stream));
  assert(!memcmp(&reconnect_old_fence,&wifi_fence5_first,sizeof reconnect_old_fence));
  assert(wifi_stream5_slots[0].exchange.keys_erased && wifi_stream5_slots[1].exchange.keys_erased);
  assert(wifi_stream5_slots[0].data_pn==wifi_stream5_slots[1].data_pn);
  if(which==9){unsigned rejected=0;for(unsigned i=0;i<wifi_stream5.record_count;i++)if(wifi_stream5.records[i].direction && !wifi_stream5.records[i].ethernet && wifi_stream5.records[i].reason==MT76_DATA5_CCMP)rejected++;assert(rejected>=1 && wifi_stream5.data_received==68);}

 }else{assert(!wifi_reconnect5.complete && !wifi_reconnect5.provider_retired && (which==7?wifi_reconnect5.connections==2:wifi_reconnect5.connections<2));if(which==6 || which==8)assert(wifi_reconnect5.connections==1 && !wifi_stream5.posts[0] && !wifi_stream5.posts[1] && !wifi_stream5.commands && !wifi_stream5.physical[0]);}
 if(which==10 || which==11 || which==16){assert(wifi_stream5_slots[0].dma.positions==127 && wifi_stream5.dma.positions==127);assert(wifi_stream5_slots[0].wraps[0]==0 && wifi_stream5.wraps[0]==0 && wifi_stream5_slots[0].wraps[1]==1 && wifi_stream5.wraps[1]==1);assert(wifi_stream5_slots[0].posts[1]>127 && wifi_stream5.posts[1]>127);}
 if(which==12){assert(reconnect_hook_count==1 && !wifi_reconnect5.connections && wifi_stream5.failed && wifi_stream5.posts[1]==WIFI_STREAM5_RX-8 && !wifi_stream5.fault_event_valid && !wifi_fence5_native.round && !xhci.command_pending);assert(wifi_reconnect5.probe[0].dhcp && wifi_reconnect5.probe[0].dns && wifi_reconnect5.probe[0].matched>0 && wifi_reconnect5.probe[0].matched<64);}
 if(which==13 || which==14){assert(reconnect_hook_count==1 && !wifi_reconnect5.connections && wifi_stream5.failed && wifi_stream5.data_sends==0);}

 if(which<2 || which==8 || which==9 || which==10 || which==11 || which==16)for(unsigned i=0;i<10;i++)assert(!memcmp(reconnect_old_physical[i],i<2?(void *)wifi_stream5_slots[0].ring[i]:(void *)wifi_stream5_slots[0].buffer[(i-2)/4][(i-2)%4],4096));
 for(unsigned i=0;i<2;i++)for(unsigned n=0;n<32;n++)assert(!wifi_stream5_slots[i].pmk[n]);
 memset(reconnect_first_tk,0,sizeof reconnect_first_tk);
 assert(!xhci_wifi_capture_reconnect5_probe());
 printf("xhci-reconnect5: PASS case%u completed_connections=%u; no kernel/controller reset between generations; modeled USB/AP; hardware acceptance=0\n",which,wifi_reconnect5.connections);
}
/* Exercise the production connection runner, not the older direct DHCP seam.
 * Only the independent AP peer injects faults; kernel timing/parser are intact. */
static void connection_dhcp_receipt(unsigned fault){
 assert(!fault || (fault>=42 && fault<=54) || fault==56 || fault==57);
 stream_setup();reconnect_case_id=0;reconnect_hook_count=0;connection_dhcp_fault=fault;
 bool good=xhci_wifi_capture_reconnect5_probe();assert(good==!fault);
 assert(reconnect_hook_count==(fault?1u:2u));
 for(unsigned g=0;g<(fault?1u:2u);g++){
  const struct mt76_stack5_probe *r=&wifi_reconnect5.probe[g];const struct net_dhcp_observation *o=&r->dhcp_observation;
  assert(r->attempted && r->private_erased && r->connection_scope && o->version==1 && o->started && o->finished);
  assert(o->success==!fault && o->success==r->dhcp && o->polls<=r->polls && o->elapsed_us<=r->elapsed_us);
  assert(wifi_stream5_slots[g].exchange.keys_erased);
  if(!fault)assert(r->complete && r->disconnected && o->accepted_offer==1 && o->accepted_ack==1 && o->end_reason==NET_DHCP_SUCCESS);
  else{
   assert(!r->complete && r->phase==MT76_STACK5_DHCP && r->reason==MT76_STACK5_DHCP_FAILED && o->end_reason==NET_DHCP_TIME_LIMIT);
   if(fault>=56)assert(o->discovers==2 && o->discover_queued==2 && r->retries==1 && !o->requests && !o->accepted_offer && !o->rejected && o->frames==(fault==57?9u:0u) && !o->ipv4);
   else assert(o->accepted_offer==1 && o->requests==1 && !o->accepted_ack);
  }
 }
 printf("stack-dhcp-fixture: actual_connection_runner=1 fault=%u modeled_usb=1 acceptance=0\n",fault);
}
#ifndef STACK5_TEST_MAIN
#define STACK5_TEST_MAIN main
#endif
int STACK5_TEST_MAIN(int argc,char **argv){
 if(argc==3 && !strcmp(argv[1],"--trace-connection-dhcp")){batch_trace=true;connection_dhcp_receipt((unsigned)strtoul(argv[2],NULL,0));return 0;}
 if((argc==2 || argc==19) && !strcmp(argv[1],"--trace-reconnect")){
  if(argc==19){for(unsigned d=0;d<2;d++)for(unsigned i=0;i<8;i++)reconnect_seed[d][i]=(uint32_t)strtoull(argv[2+d*8+i],NULL,0);reconnect_slot=(unsigned)strtoul(argv[18],NULL,0);assert(reconnect_slot && reconnect_slot<256);reconnect_seeded=true;}
  batch_trace=true;puts("wifi-reconnect5-host: modeled_usb=1 acceptance=0");reconnect_case(10);return 0;
 }
 if(argc==3 && !strcmp(argv[1],"--trace-reconnect-case")){batch_trace=true;puts("wifi-reconnect5-host: modeled_usb=1 acceptance=0");reconnect_case((unsigned)strtoul(argv[2],NULL,0));return 0;}
 if(argc==3 && !strcmp(argv[1],"--reconnect-case")){reconnect_case((unsigned)strtoul(argv[2],NULL,0));return 0;}
 if(argc==3 && !strcmp(argv[1],"--trace-dhcp")){
  unsigned which=(unsigned)strtoul(argv[2],NULL,0);assert(which>=41 && which<55);
  batch_trace=true;native_dhcp_case(which);return 0;
 }
 if(argc==2 && (!strcmp(argv[1],"--trace-auth-loss") || !strcmp(argv[1],"--trace-auth-negative") || !strcmp(argv[1],"--trace-auth-malformed"))){batch_trace=true;probe_case(!strcmp(argv[1],"--trace-auth-loss")?21:!strcmp(argv[1],"--trace-auth-negative")?22:23);return 0;}
 if(argc==2 && !strcmp(argv[1],"--size-projection")){probe_size_projection();return 0;}
 if((argc==2 || argc==11 || argc==19) && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--trace-retry") || !strcmp(argv[1],"--trace-arp"))){
  probe_setup(!strcmp(argv[1],"--trace-retry")?11:!strcmp(argv[1],"--trace-arp")?19:10);
  if(argc==11 || argc==19){for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,9,i,(uint32_t)strtoull(argv[i+2],NULL,0));if(argc==19)for(unsigned i=0;i<8;i++)context_write32(wifi_cal5.d->output_context,8,i,(uint32_t)strtoull(argv[i+10],NULL,0));wifi_cal5.d->slot_id=(uint8_t)strtoul(argv[argc-1],NULL,0);}
  batch_trace=true;assert(xhci_wifi_capture_stack5_probe());return 0;
 }
 if(argc==3 && !strcmp(argv[1],"--case")){unsigned n=(unsigned)strtoul(argv[2],NULL,0);if(n<10)stack_case(n);else if(n<24)probe_case(n);else if(n<41)boundary_case(n);else native_dhcp_case(n);return 0;}assert(argc==1);
 for(unsigned i=0;i<55;i++){pid_t p=fork();assert(p>=0);if(!p){if(i<10)stack_case(i);else if(i<24)probe_case(i);else if(i<41)boundary_case(i);else native_dhcp_case(i);exit(0);}int status=0;assert(waitpid(p,&status,0)==p && WIFEXITED(status) && WEXITSTATUS(status)==0);}
 puts("xhci-stack5: PASS55 cases actual encrypted owner/registry/production IP,quiet RX63/TX liveness,shared HID frontier retry/command-debt refusal,12 native Stop ordering/context cases through real HTTP OWNER check,14 bounded-DHCP observation cases through native encrypted RX plus DHCP/DNS/64exact40-byteICMP/retirement;independent OpenSSL AP,modeled USB/MMIO,no ELF or hardware acceptance");return 0;
}
