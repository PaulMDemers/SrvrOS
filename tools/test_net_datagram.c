/* Production UDP/ICMP paths with exact packet buffers and controlled wait hooks. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SRVROS_NET_DATAGRAM_SOURCE
#define SRVROS_NET_DATAGRAM_SOURCE "../kernel/src/net.c"
#endif
#include SRVROS_NET_DATAGRAM_SOURCE
static uint64_t ticks=100;
static unsigned frames,udp_wakes,poll_wakes;
static uint8_t last_frame[1600];static uint16_t last_length;
static void (*poll_hook)(void),(*wait_hook)(void);
static unsigned char parent_token,child_token;
static struct process *caller;
static struct process *parent(void) { return (struct process *)&parent_token; }
static struct process *child(void) { return (struct process *)&child_token; }
uint64_t timer_ticks(void) { return ticks; }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
void console_printf(const char *fmt,...) { (void)fmt; }
struct process *process_current(void) { return caller; }
bool process_should_exit_current(void) { return false; }
bool process_signal_pending_current(void) { return false; }
void scheduler_yield(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *q) { if(q==&udp_wait_queue)udp_wakes++; }
void process_file_poll_wake(void) { poll_wakes++; }
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    (void)budget;(void)verbose;if(poll_hook) { void (*fn)(void)=poll_hook;poll_hook=NULL;fn(); }return 0;
}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) {
    assert(q==&udp_wait_queue && wait_hook);void (*hook)(void)=wait_hook;wait_hook=NULL;hook();assert(fn(arg));return true;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t deadline) {
    (void)fn;(void)arg;assert(q==&rx_wait_queue);ticks=deadline;return false;
}
bool net_device_send(const uint8_t *frame,uint16_t length) {
    assert(length<=sizeof(last_frame));frames++;memcpy(last_frame,frame,length);last_length=length;return true;
}
static void reset(void) {
    memset(udp_sockets,0,sizeof(udp_sockets));memset(arp_cache,0,sizeof(arp_cache));
    static const uint8_t mac[6]={2,0,0,0,0,1};local_mac=mac;local_ip=0x0a00020f;
    initialized=worker_started=true;caller=parent();ticks=100;frames=udp_wakes=poll_wakes=0;
    poll_hook=wait_hook=NULL;icmp_ping_waiting=icmp_ping_reply=false;
}
static uint64_t endpoint(uint16_t port) {
    int64_t h=net_udp_open();assert(h>0 && !net_udp_bind((uint64_t)h,port));return (uint64_t)h;
}
static uint16_t ip_packet(uint8_t *frame,uint8_t protocol,const uint8_t *payload,uint16_t size) {
    memset(frame,0,1600);memcpy(frame,local_mac,6);frame[6]=2;frame[11]=2;
    write_be16(frame+12,ETH_TYPE_IPV4);uint8_t *ip=frame+14;ip[0]=0x45;ip[8]=64;ip[9]=protocol;
    write_be16(ip+2,20+size);write_be16(ip+6,0x4000);write_be32(ip+12,0x0a000202);write_be32(ip+16,local_ip);
    write_be16(ip+10,checksum_bytes(ip,20,0));memcpy(ip+20,payload,size);return 34+size;
}
static void icmp_packet(uint8_t type,uint8_t code,bool corrupt,uint16_t size) {
    uint8_t payload[1480]={0},frame[1600];assert(size<=sizeof(payload));payload[0]=type;payload[1]=code;
    write_be16(payload+4,ICMP_PING_ID);write_be16(payload+6,77);memcpy(payload+8,"hello",5);
    write_be16(payload+2,checksum_bytes(payload,size,0));if(corrupt)payload[2]^=1;
    handle_ipv4(frame,ip_packet(frame,IP_PROTO_ICMP,payload,size));
}
static void icmp_checksum(void) {
    reset();icmp_packet(8,0,true,13);assert(!frames);
    icmp_packet(8,0,false,13);assert(frames==1 && last_length==47);
    assert(last_frame[34]==0 && last_frame[35]==0 && !checksum_bytes(last_frame+34,13,0));
    assert(!memcmp(last_frame+42,"hello",5));
}
static void icmp_code(void) {
    reset();icmp_packet(8,1,false,13);assert(!frames);
    for(unsigned n=0;n<8;n++) { icmp_packet(8,0,false,n);assert(!frames); }
    icmp_packet(8,0,false,8);assert(frames==1);
}
static void icmp_reply(void) {
    reset();icmp_ping_waiting=true;icmp_ping_ip=0x0a000202;icmp_ping_sequence=77;
    icmp_packet(0,1,false,13);assert(icmp_ping_waiting && !icmp_ping_reply);
    icmp_packet(0,0,true,13);assert(icmp_ping_waiting && !icmp_ping_reply);
    icmp_ping_sequence=78;icmp_packet(0,0,false,13);assert(icmp_ping_waiting);
    icmp_ping_sequence=77;icmp_packet(0,0,false,13);assert(!icmp_ping_waiting && icmp_ping_reply && !frames);
}
static void udp_wire(void) {
    reset();uint64_t h=endpoint(9000);uint8_t udp[1482]={0},frame[1600],data[16];
    write_be16(udp,8000);write_be16(udp+2,9000);write_be16(udp+4,13);memcpy(udp+8,"hello",5);
    write_be16(udp+6,udp_checksum(0x0a000202,local_ip,udp,13));
    uint16_t n=ip_packet(frame,IP_PROTO_UDP,udp,13);
    for(unsigned len=0;len<n;len++) {
        uint8_t *short_frame=malloc(len?len:1);assert(short_frame);memcpy(short_frame,frame,len);
        handle_ipv4(short_frame,len);free(short_frame);assert(!find_udp_socket(h)->count);
    }
    frame[n-1]^=1;handle_ipv4(frame,n);assert(!find_udp_socket(h)->count);frame[n-1]^=1;
    handle_ipv4(frame,n);uint32_t ip;uint16_t port;
    assert(net_udp_recvfrom(h,data,sizeof(data),&ip,&port,true)==5 && !memcmp(data,"hello",5));
    assert(ip==0x0a000202 && port==8000);
    for(unsigned len=0;len<16;len++) if(len<8 || len>13) {
        write_be16(udp+4,(uint16_t)len);write_be16(udp+6,0);
        handle_ipv4(frame,ip_packet(frame,IP_PROTO_UDP,udp,13));assert(!find_udp_socket(h)->count);
    }
    write_be16(udp,0);write_be16(udp+4,8); /* No source port/checksum and empty payload are valid. */
    handle_ipv4(frame,ip_packet(frame,IP_PROTO_UDP,udp,8));
    assert(net_poll_events(h,SRV_POLLIN)&SRV_POLLIN);
    assert(net_udp_recvfrom(h,NULL,0,&ip,&port,true)==0 && port==0);
    write_be16(udp+4,1481);handle_ipv4(frame,ip_packet(frame,IP_PROTO_UDP,udp,1481));
    assert(!find_udp_socket(h)->count); /* Oversize cannot evict a queued entry. */
    write_be16(udp+4,13);handle_ipv4(frame,ip_packet(frame,IP_PROTO_UDP,udp,16));
    assert(net_udp_recvfrom(h,data,sizeof(data),NULL,NULL,true)==5); /* IP tail is not UDP data. */
}
static void udp_pressure(void) {
    reset();uint64_t h=endpoint(9000);uint8_t byte=0,out;
    for(unsigned round=0;round<50;round++) {
        for(unsigned i=0;i<6;i++) { byte=(uint8_t)i;udp_deliver(9000,0x0a000202,8000+i,&byte,1); }
        assert(find_udp_socket(h)->count==UDP_RX_QUEUE);
        uint8_t huge[UDP_PAYLOAD_MAX+1]={0};udp_deliver(9000,0,0,huge,sizeof(huge));
        for(unsigned i=2;i<6;i++) {
            uint16_t port;assert(net_udp_recvfrom(h,&out,1,NULL,&port,true)==1 && out==i && port==8000+i);
        }
        assert(net_udp_recvfrom(h,&out,1,NULL,NULL,true)==SRV_ERR_AGAIN && !(net_poll_events(h,SRV_POLLIN)&SRV_POLLIN));
    }
}
static void udp_truncate(void) {
    reset();uint64_t h=endpoint(9000);uint8_t out[8];
    udp_deliver(9000,1,2,(const uint8_t *)"abcdef",6);udp_deliver(9000,3,4,(const uint8_t *)"next",4);
    assert(net_udp_recvfrom(h,out,2,NULL,NULL,true)==2 && !memcmp(out,"ab",2));
    assert(net_udp_recvfrom(h,out,8,NULL,NULL,true)==4 && !memcmp(out,"next",4));
    udp_deliver(9000,1,2,(const uint8_t *)"discard",7);
    assert(net_udp_recvfrom(h,NULL,0,NULL,NULL,true)==0 && !find_udp_socket(h)->count);
    udp_deliver(9000,1,2,NULL,0);assert(net_udp_recvfrom(h,out,8,NULL,NULL,true)==0);
}
static uint64_t old_handle,new_handle;static bool keep_alias;
static void close_reuse(void) {
    assert(!net_close(old_handle));
    if(!keep_alias) { new_handle=endpoint(9001);assert(new_handle!=old_handle); }
    uint8_t reply[42]={0};write_be16(reply+14,1);write_be16(reply+16,ETH_TYPE_IPV4);
    reply[18]=6;reply[19]=4;write_be16(reply+20,2);memset(reply+22,2,6);
    write_be32(reply+28,0x0a000202);handle_arp(reply,sizeof(reply));
}
static void udp_send_reuse(void) {
    for(unsigned alias=0;alias<2;alias++) {
        reset();old_handle=endpoint(9000);keep_alias=alias!=0;if(alias)assert(!net_hold(old_handle));
        poll_hook=close_reuse;
        int64_t result=net_udp_sendto(old_handle,0x0a000202,8000,(const uint8_t *)"abc",3);
        if(alias) assert(result==3 && last_frame[23]==IP_PROTO_UDP && read_be16(last_frame+34)==9000);
        else assert(result<0 && last_frame[12]==0x08 && last_frame[13]==0x06 && !find_udp_socket(new_handle)->count);
    }
}
static void udp_close_wait(void) {
    reset();old_handle=endpoint(9000);keep_alias=false;wait_hook=close_reuse;uint8_t byte;
    struct udp_wait_arg wait={.socket_id=handle_value(old_handle)};
    assert(net_udp_recvfrom(old_handle,&byte,1,NULL,NULL,false)<0 && udp_recv_ready(&wait));
    assert(net_poll_events(old_handle,SRV_POLLIN)==SRV_POLLNVAL && net_hold(old_handle)<0 && net_close(old_handle)<0);
    assert(!find_udp_socket(new_handle)->count && udp_wakes && poll_wakes);
}
static void udp_inherited(void) {
    for(unsigned parent_first=0;parent_first<2;parent_first++) {
        reset();uint64_t h=endpoint(9000);assert(!net_hold(h));
        udp_deliver(9000,1,2,(const uint8_t *)"ok",2);assert(!net_close(h));
        net_process_cleanup(parent_first?parent():child());caller=parent_first?child():parent();
        uint8_t out[2];assert(net_udp_recvfrom(h,out,2,NULL,NULL,true)==2 && !memcmp(out,"ok",2));
        assert(!net_close(h));net_process_cleanup(caller);assert(!find_udp_socket(h));
    }
}
static void udp_bind_identity(void) {
    reset();uint64_t h=endpoint(9000),other=endpoint(9001);uint32_t ip;uint16_t port;
    udp_deliver(9000,1,2,(const uint8_t *)"old",3);
    assert(net_udp_bind(h,9000)<0 && net_udp_bind(h,9002)<0 && net_udp_bind(h,0)<0);
    assert(!net_sockname(h,&ip,&port) && port==9000 && find_udp_socket(h)->count==1);
    assert(net_udp_bind(other,9000)<0);assert(!net_close(h));
    assert(endpoint(9000)!=h);
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"icmp-checksum"))icmp_checksum();
    else if(!strcmp(argv[1],"icmp-code"))icmp_code();
    else if(!strcmp(argv[1],"icmp-reply"))icmp_reply();
    else if(!strcmp(argv[1],"udp-wire"))udp_wire();
    else if(!strcmp(argv[1],"udp-pressure"))udp_pressure();
    else if(!strcmp(argv[1],"udp-truncate"))udp_truncate();
    else if(!strcmp(argv[1],"udp-send-reuse"))udp_send_reuse();
    else if(!strcmp(argv[1],"udp-close-wait"))udp_close_wait();
    else if(!strcmp(argv[1],"udp-inherited"))udp_inherited();
    else if(!strcmp(argv[1],"udp-bind"))udp_bind_identity();
    else assert(0);
    reset();printf("net-datagram: %s PASS\n",argv[1]);
}
