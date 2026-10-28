/* Real TCP state/refcount code with a captured Ethernet sink and controlled clock. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SRVROS_NET_LIFETIME_SOURCE
#define SRVROS_NET_LIFETIME_SOURCE "../kernel/src/net.c"
#endif
#include SRVROS_NET_LIFETIME_SOURCE

static uint64_t ticks=100;
static unsigned frames,payload_frames;
static unsigned read_wakes,write_wakes;
static uint8_t last_frame[1600];
static uint16_t last_length;
static bool fail_send;
static struct process *caller;
static unsigned char creator_token,child_token;
static struct process *creator(void) { return (struct process *)&creator_token; }
static struct process *child(void) { return (struct process *)&child_token; }
uint64_t timer_ticks(void) { return ticks; }
void scheduler_wake_all(struct scheduler_wait_queue *q) {
    if(q==&read_wait_queue)read_wakes++;
    if(q==&write_wait_queue)write_wakes++;
}
void process_file_poll_wake(void) {}
void kfree(void *p) { free(p); }
void *kmalloc(size_t n) { return malloc(n); }
struct process *process_current(void) { return caller; }
bool process_should_exit_current(void) { return false; }
bool process_signal_pending_current(void) { return false; }
uint64_t net_device_poll(uint64_t budget,bool verbose) { (void)budget;(void)verbose;return 0; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) {
    (void)q;(void)fn;(void)arg;assert(0);return false;
}
void scheduler_yield(void) {} /* Successful sends yield even when nonblocking. */
bool net_device_send(const uint8_t *frame,uint16_t length) {
    assert(length<=sizeof(last_frame));frames++;last_length=length;memcpy(last_frame,frame,length);
    if(length>54) payload_frames++;
    return !fail_send;
}
static void reset(void) {
    for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++) if(tcp_connections[i].state) close_connection(&tcp_connections[i]);
    memset(tcp_connections,0,sizeof(tcp_connections));memset(listeners,0,sizeof(listeners));
    memset(pending_requests,0,sizeof(pending_requests));memset(udp_sockets,0,sizeof(udp_sockets));
    static const uint8_t mac[6]={2,0,0,0,0,1};local_mac=mac;
    initialized=worker_started=true;caller=creator();ticks=100;
    frames=payload_frames=read_wakes=write_wakes=0;fail_send=false;
}
static struct tcp_connection *connected(void) {
    reset();struct tcp_connection *c=&tcp_connections[0];
    *c=(struct tcp_connection){.state=TCP_STATE_ESTABLISHED,.accepted=true,.outbound=true,
        .owner=creator(),.id=42,.refs=1,.local_seq=1000,.remote_next=2000,
        .local_port=50000,.remote_port=80,.remote_ip=0x0a000202,.peer_window=65535,.last_activity_tick=ticks};
    return c;
}
static uint64_t handle(struct tcp_connection *c) { return make_handle(NET_HANDLE_CONNECTION,c->id); }
static void final_close(bool full) {
    struct tcp_connection *c=connected();uint64_t h=handle(c);
    char payload[TCP_TX_WINDOW_BYTES];memset(payload,'x',sizeof(payload));
    unsigned size=full?sizeof(payload):5;
    unsigned sent=0;
    while(sent<size) { int64_t n=net_respond(h,payload+sent,size-sent,true);assert(n>0);sent+=(unsigned)n; }
    uint32_t end=c->local_seq;unsigned before=frames;
    assert(!net_close(h) && !c->refs);
    assert(c->fin_pending==full && c->local_write_closed);
    net_process_cleanup(creator());
    assert(c->state!=TCP_STATE_EMPTY && !c->owner); /* Exit must retain retransmits. */
    unsigned data_before=payload_frames;
    ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();assert(frames>before && payload_frames>data_before);
    assert(last_length>=54);
    if(full) {
        assert(!(last_frame[47]&TCP_FIN));
        tcp_handle_ack(c,end);assert(!c->fin_pending && c->state==TCP_STATE_FIN_WAIT_1);
        assert(c->local_seq==end+1 && (last_frame[47]&TCP_FIN));
    }
    tcp_handle_ack(c,end+1);assert(c->state==TCP_STATE_FIN_WAIT_2);
    tcp_handle_fin(c,true);assert(c->state==TCP_STATE_TIME_WAIT);
    ticks=c->close_deadline;net_timer_tick();assert(c->state==TCP_STATE_EMPTY);
    assert(net_hold(h)<0 && net_close(h)<0);
}
static void inherited_connection(void) {
    for(unsigned parent_first=0;parent_first<2;parent_first++) {
        struct tcp_connection *c=connected();uint64_t h=handle(c);
        assert(!net_hold(h) && c->refs==2);
        assert(!net_close(h) && c->refs==1 && !frames);
        net_process_cleanup(parent_first?creator():child());
        assert(c->state==TCP_STATE_ESTABLISHED && c->refs==1);
        assert(net_respond(h,"survives",8,true)==8);
        assert(!net_close(h));net_process_cleanup(parent_first?child():creator());
        assert(c->state==TCP_STATE_FIN_WAIT_1 && !c->refs && !c->owner);
        ticks+=TCP_RETRANSMIT_TICKS;unsigned before=frames;net_timer_tick();assert(frames>before);
        ticks=c->close_deadline;net_timer_tick();assert(!c->state);
    }
}
static void inherited_backlog(void) {
    reset();uint64_t listener=net_listen(8080);assert((int64_t)listener>0);
    assert(!net_hold(listener));
    uint8_t mac[6]={2,0,0,0,0,2};
    struct tcp_connection *c=alloc_connection(8080,0x0a000202,40000,mac);assert(c);
    c->state=TCP_STATE_ESTABLISHED;assert(ensure_pending_connection(c,8080));
    struct tcp_connection *handshake=alloc_connection(8080,0x0a000202,40001,mac);assert(handshake);
    assert(!net_close(listener));net_process_cleanup(creator());
    assert(c->state==TCP_STATE_ESTABLISHED && !c->owner && pending_requests[0].used);
    assert(!pending_requests[0].owner);
    assert(handshake->state==TCP_STATE_SYN_RECEIVED && !handshake->owner);
    caller=child();uint64_t h=net_accept(listener,NULL,0,NULL,true);assert(h==handle(c) && c->refs==1);
    assert(!net_close(listener) && c->state==TCP_STATE_ESTABLISHED && !handshake->state);
    assert(!net_close(h));net_process_cleanup(child());
    assert(c->state==TCP_STATE_FIN_WAIT_1);ticks=c->close_deadline;net_timer_tick();assert(!c->state);
}
static void abort_controls(void) {
    struct tcp_connection *c=connected();c->state=TCP_STATE_SYN_SENT;
    assert(!net_close(handle(c)) && !c->state);net_process_cleanup(creator());
    c=connected();c->reset=true;assert(!net_close(handle(c)) && !c->state);
    c=connected();fail_send=true;assert(!net_close(handle(c)) && !c->state);
    reset();uint64_t listener=net_listen(8080);
    uint8_t mac[6]={0};c=alloc_connection(8080,0x0a000202,40000,mac);assert(c);
    assert(!net_close(listener) && !c->state);net_process_cleanup(creator());
}
static void deferred_fin(bool send_failure) {
    struct tcp_connection *c=connected();uint64_t h=handle(c);
    for(unsigned i=0;i<TCP_TX_HISTORY;i++) assert(net_respond(h,"x",1,true)==1);
    assert(!net_shutdown(h,1) && c->fin_pending && c->refs==1);
    uint32_t end=c->local_seq;
    if(send_failure) {
        fail_send=true;tcp_handle_ack(c,end);
        assert(c->reset && !c->fin_pending && c->error==SRV_NETERR_RESET);
        assert(net_poll_events(h,SRV_POLLOUT)&SRV_POLLERR);
        assert(net_respond(h,"x",1,true)<0);
    } else {
        assert(c->close_deadline==ticks+TCP_CLOSE_TIMEOUT_TICKS);
    }
    ticks=c->close_deadline;net_timer_tick();
    assert(c->refs==1 && net_error(h)==(send_failure?SRV_NETERR_RESET:SRV_NETERR_TIMEDOUT));
    assert(!net_close(h) && !c->state);
}
static void simultaneous_close(void) {
    for(unsigned passive=0;passive<2;passive++) {
        struct tcp_connection *c=connected();
        if(passive) { c->state=TCP_STATE_CLOSE_WAIT;c->peer_closed=true; }
        assert(!net_close(handle(c)));
        uint8_t waiting=passive?TCP_STATE_LAST_ACK:TCP_STATE_FIN_WAIT_1;
        assert(c->state==waiting);
        tcp_handle_fin(c,true); /* Peer FIN is not an ACK of our FIN. */
        assert(c->state==waiting);
        ticks+=TCP_TIME_WAIT_TICKS;net_timer_tick();assert(c->state==waiting);
        tcp_handle_ack(c,c->local_seq-1);assert(c->state==waiting);
        tcp_handle_ack(c,c->local_seq);assert(c->state==TCP_STATE_TIME_WAIT);
        ticks=c->close_deadline;net_timer_tick();assert(!c->state);
    }
}
static void incoming(struct tcp_connection *c,uint32_t seq,uint32_t ack,uint8_t flags,
    uint16_t window,const char *data) {
    uint8_t frame[128]={0},*ip=frame+14,*tcp=ip+20;
    size_t n=data?strlen(data):0;assert(n<=64);
    frame[6]=2;frame[11]=2;
    write_be32(ip+12,c->remote_ip);write_be32(ip+16,0x0a00020f);
    write_be16(tcp,c->remote_port);write_be16(tcp+2,c->local_port);
    write_be32(tcp+4,seq);write_be32(tcp+8,ack);
    tcp[12]=5<<4;tcp[13]=flags;write_be16(tcp+14,window);
    if(n) memcpy(tcp+20,data,n);
    write_be16(tcp+16,tcp_checksum(c->remote_ip,0x0a00020f,tcp,20+n));
    handle_tcp(frame,54+n,ip,20,20+n,c->remote_ip);
}
static void future_ack(void) {
    for(unsigned wrap=0;wrap<2;wrap++) for(unsigned closing=0;closing<2;closing++) {
        struct tcp_connection *c=connected();if(wrap)c->local_seq=UINT32_MAX-2;
        assert(net_respond(handle(c),"abcd",4,true)==4);
        if(closing) assert(!net_shutdown(handle(c),1));
        struct tcp_tx_record before[TCP_TX_HISTORY];memcpy(before,c->tx_history,sizeof(before));
        uint8_t state=c->state;uint64_t deadline=c->close_deadline;
        incoming(c,c->remote_next,c->local_seq+1,TCP_ACK|TCP_FIN,0,"bad");
        assert(!memcmp(before,c->tx_history,sizeof(before)) && c->state==state);
        assert(c->peer_window==65535 && !c->peer_closed && !c->rx_length);
        assert(c->close_deadline==deadline && last_frame[47]==TCP_ACK);
        tcp_handle_ack(c,c->local_seq+1); /* Defensive helper admission too. */
        assert(!memcmp(before,c->tx_history,sizeof(before)) && c->state==state);
        incoming(c,c->remote_next,c->local_seq,TCP_ACK,65535,NULL);
        assert(!tcp_tx_outstanding_bytes(c));
    }
}
static void wrap_ack(void) {
    struct tcp_connection *c=connected();c->local_seq=UINT32_MAX-3;
    assert(net_respond(handle(c),"abc",3,true)==3);
    assert(net_respond(handle(c),"def",3,true)==3 && c->local_seq==2);
    incoming(c,c->remote_next,UINT32_MAX-4,TCP_ACK,65535,NULL);
    assert(tcp_tx_outstanding_bytes(c)==6);
    incoming(c,c->remote_next,UINT32_MAX,TCP_ACK,65535,NULL);
    assert(tcp_tx_outstanding_bytes(c)==3);
    incoming(c,c->remote_next,UINT32_MAX,TCP_ACK,65535,NULL);
    assert(tcp_tx_outstanding_bytes(c)==3);
    incoming(c,c->remote_next,1,TCP_ACK,65535,NULL);assert(tcp_tx_outstanding_bytes(c)==3);
    incoming(c,c->remote_next,2,TCP_ACK,65535,NULL);assert(!tcp_tx_outstanding_bytes(c));
    assert(!net_shutdown(handle(c),1));
    incoming(c,c->remote_next,3,TCP_ACK,65535,NULL);assert(c->state==TCP_STATE_FIN_WAIT_2);
}
static void syn_ack(void) {
    for(unsigned wrap=0;wrap<2;wrap++) {
        reset();uint8_t mac[6]={0};struct tcp_connection *c=alloc_client_connection(0x0a000202,80,mac);assert(c);
        if(wrap)c->local_seq=UINT32_MAX;
        assert(send_tcp_segment(c,c->local_port,TCP_SYN,NULL,0));
        uint32_t next=c->local_seq;
        incoming(c,123,next+1,TCP_SYN|TCP_ACK,0,NULL);
        assert(c->state==TCP_STATE_SYN_SENT && !c->remote_next && c->peer_window==TCP_RX_WINDOW);
        assert(tcp_tx_outstanding_bytes(c)==1 && (last_frame[47]&TCP_RST));
        incoming(c,123,next-1,TCP_SYN|TCP_ACK,0,NULL);assert(c->state==TCP_STATE_SYN_SENT);
        incoming(c,123,next,TCP_SYN|TCP_ACK,8192,NULL);
        assert(c->state==TCP_STATE_ESTABLISHED && c->remote_next==124 && c->peer_window==8192);
        assert(!tcp_tx_outstanding_bytes(c));
    }
}
static void stale_handle(bool io) {
    struct tcp_connection *c=connected();uint64_t h=handle(c);
    assert(append_connection_data(c,(const uint8_t *)"secret",6));
    assert(!net_close(h) && !c->refs && c->state==TCP_STATE_FIN_WAIT_1);
    if(!io) { assert(net_hold(h)<0 && !c->refs);return; }
    uint32_t ip=99;uint16_t port=99;char data[8];
    assert(net_read(h,data,1,true)<0 && net_peek(h,data,1,true)<0);
    assert(net_read(h,NULL,0,true)<0 && net_respond(h,NULL,0,true)<0);
    assert(net_respond(h,"x",1,true)<0 && net_shutdown(h,0)<0);
    assert(net_error(h)<0 && net_sockname(h,&ip,&port)<0 && net_peername(h,&ip,&port)<0);
    assert(ip==99 && port==99 && net_poll_events(h,SRV_POLLIN|SRV_POLLOUT)==SRV_POLLNVAL);
    unsigned before=frames;ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();assert(frames>before);
}
static void reuse_handle(void) {
    reset();uint8_t mac[6]={0};struct tcp_connection *c=alloc_client_connection(0x0a000202,80,mac);assert(c);
    uint64_t old=handle(c);assert(!net_close(old) && !c->state);
    struct tcp_connection *next=alloc_client_connection(0x0a000202,80,mac);assert(next==c && handle(next)!=old);
    assert(net_hold(old)<0 && net_close(old)<0 && net_poll_events(old,SRV_POLLIN)==SRV_POLLNVAL);
    uint64_t h=handle(next);next->refs=UINT32_MAX;assert(net_hold(h)<0 && next->refs==UINT32_MAX);
    next->refs=1;assert(!net_hold(h) && next->refs==2);assert(!net_close(h) && next->refs==1);
    assert(!net_close(h) && !next->state);
}
static void stale_wait(void) {
    struct tcp_connection *c=connected();c->peer_window=0;
    struct read_wait_arg r={.connection_id=c->id};
    struct write_wait_arg w={.connection_id=c->id};
    assert(!read_ready(&r) && !write_ready(&w));
    assert(!net_close(handle(c)));
    assert(read_wakes && write_wakes && read_ready(&r) && write_ready(&w));
}
static void reset_window(void) {
    for(unsigned wrap=0;wrap<2;wrap++) {
        struct tcp_connection *c=connected();if(wrap)c->remote_next=UINT32_MAX-3;
        uint32_t seq=c->remote_next;unsigned before=frames;
        incoming(c,seq-1,c->local_seq,TCP_RST|TCP_ACK,0,NULL);
        assert(!c->reset && frames==before);
        incoming(c,seq+NET_CONNECTION_RX_MAX_SIZE,c->local_seq,TCP_RST|TCP_ACK,0,NULL);
        assert(!c->reset && frames==before);
        incoming(c,seq+1,c->local_seq,TCP_RST|TCP_ACK,0,NULL);
        assert(!c->reset && frames==before+1 && last_frame[47]==TCP_ACK);
        incoming(c,seq,c->local_seq,TCP_RST|TCP_ACK,0,NULL);
        assert(c->reset && net_error(handle(c))==SRV_NETERR_RESET);
    }
}
static void reset_connect(void) {
    reset();uint8_t mac[6]={0};struct tcp_connection *c=alloc_client_connection(0x0a000202,80,mac);assert(c);
    assert(send_tcp_segment(c,c->local_port,TCP_SYN,NULL,0));
    incoming(c,0,c->local_seq,TCP_RST,0,NULL);assert(!c->reset);
    incoming(c,0,c->local_seq+1,TCP_RST|TCP_ACK,0,NULL);assert(!c->reset);
    incoming(c,0,c->local_seq,TCP_RST|TCP_ACK,0,NULL);
    assert(c->reset && net_error(handle(c))==SRV_NETERR_REFUSED);
}
static void receive_window(void) {
    struct tcp_connection *c=connected();uint64_t h=handle(c);
    assert(net_respond(h,"keep",4,true)==4);
    incoming(c,c->remote_next+NET_CONNECTION_RX_MAX_SIZE,c->local_seq,TCP_ACK,0,NULL);
    assert(tcp_tx_outstanding_bytes(c)==4 && c->peer_window==65535);
    incoming(c,c->remote_next,c->local_seq,TCP_PSH|TCP_FIN,0,"bad");
    assert(!c->rx_length && !c->peer_closed && c->peer_window==65535);
    incoming(c,c->remote_next+1,c->local_seq,TCP_ACK|TCP_PSH,0,"later");
    assert(!c->rx_length && c->peer_window==65535 && tcp_tx_outstanding_bytes(c)==4);
    c->rx_data=malloc(NET_CONNECTION_RX_MAX_SIZE);assert(c->rx_data);
    c->rx_capacity=NET_CONNECTION_RX_MAX_SIZE;c->rx_length=NET_CONNECTION_RX_MAX_SIZE-1;
    incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,"xx");
    assert(c->rx_length==NET_CONNECTION_RX_MAX_SIZE-1 && !c->reset && c->peer_window==65535);
    incoming(c,c->remote_next,c->local_seq,TCP_ACK,1234,"x");
    assert(c->rx_length==NET_CONNECTION_RX_MAX_SIZE && c->peer_window==1234);
}
static void receive_duplicate(void) {
    struct tcp_connection *c=connected();uint32_t first=c->remote_next;
    incoming(c,first,c->local_seq,TCP_ACK,65535,"abc");assert(c->rx_length==3);
    incoming(c,first,c->local_seq,TCP_ACK,65535,"abc");assert(c->rx_length==3);
    incoming(c,first+2,c->local_seq,TCP_ACK,65535,"cde");assert(c->rx_length==3);
    incoming(c,first+3,c->local_seq,TCP_ACK|TCP_FIN,65535,"de");
    assert(c->rx_length==5 && c->peer_closed && c->remote_next==first+6);
    char data[5];assert(net_read(handle(c),data,5,true)==5 && !memcmp(data,"abcde",5));
    assert(net_read(handle(c),data,1,true)==0);
}
static void terminal_error(void) {
    for(unsigned failure=0;failure<3;failure++) {
        struct tcp_connection *c=connected();uint64_t h=handle(c);assert(!net_hold(h));
        uint16_t error=failure==0?SRV_NETERR_RESET:SRV_NETERR_TIMEDOUT;
        if(failure<2) tcp_fail_connection(c,error,ticks);
        else assert(!net_shutdown(h,1));
        ticks=c->close_deadline;net_timer_tick();
        assert(c->refs==2 && net_error(h)==error);
        assert(net_poll_events(h,SRV_POLLIN|SRV_POLLOUT)&SRV_POLLERR);
        char data;assert(net_read(h,&data,1,true)<0 && net_respond(h,"x",1,true)<0);
        unsigned before=frames;ticks+=TCP_IDLE_TIMEOUT_TICKS*2;net_timer_tick();
        assert(frames==before && net_error(h)==error && c->refs==2);
        assert(!net_close(h) && c->refs==1 && net_error(h)==error);
        assert(!net_close(h) && !c->state && net_error(h)<0);
    }
}
static void terminal_eof(void) {
    struct tcp_connection *c=connected();uint64_t h=handle(c);
    assert(append_connection_data(c,(const uint8_t *)"unread",6));
    assert(!net_shutdown(h,1));tcp_handle_ack(c,c->local_seq);tcp_handle_fin(c,true);
    ticks=c->close_deadline;net_timer_tick();
    assert(c->refs==1 && net_error(h)==SRV_NETERR_NONE && net_shutdown(h,0)<0);
    char data[6];assert(net_read(h,data,6,true)==6 && !memcmp(data,"unread",6));
    assert(net_read(h,data,1,true)==0 && (net_poll_events(h,SRV_POLLIN)&SRV_POLLHUP));
    assert(!net_close(h) && !c->state);
}
static void timewait_pressure(void) {
    struct tcp_connection *c=connected();uint64_t h=handle(c);
    assert(!net_shutdown(h,1));tcp_handle_ack(c,c->local_seq);tcp_handle_fin(c,true);
    assert(c->state==TCP_STATE_TIME_WAIT && c->refs==1);
    assert(!tcp_reclaim_oldest_time_wait(ticks) && c->refs==1);
    assert(!net_close(h));assert(tcp_reclaim_oldest_time_wait(ticks) && !c->state);
}
static void repeat_syn(void) {
    reset();uint64_t listener=net_listen(8080);assert((int64_t)listener>0);
    uint8_t mac[6]={0};struct tcp_connection *c=alloc_connection(8080,0x0a000202,40000,mac);assert(c);
    c->remote_next=501;assert(send_tcp_segment(c,c->local_port,TCP_SYN|TCP_ACK,NULL,0));
    uint32_t seq=c->local_seq;
    incoming(c,500,0,TCP_SYN,1000,NULL);
    assert(c->local_seq==seq && c->remote_next==501 && tcp_tx_outstanding_bytes(c)==1);
    incoming(c,501,seq,TCP_ACK,1000,NULL);assert(c->state==TCP_STATE_ESTABLISHED);
    incoming(c,900,0,TCP_SYN,0,NULL);
    assert(c->state==TCP_STATE_ESTABLISHED && c->remote_next==501 && c->local_seq==seq && c->peer_window==1000);
    assert(!net_close(listener));
    reset();listener=net_listen(8080);assert((int64_t)listener>0);
    c=alloc_connection(8080,0x0a000202,40000,mac);assert(c);c->remote_next=501;
    seq=c->local_seq;fail_send=true;
    assert(!send_tcp_segment(c,c->local_port,TCP_SYN|TCP_ACK,NULL,0));fail_send=false;
    incoming(c,500,0,TCP_SYN,1000,NULL);
    assert(c->local_seq==seq+1 && tcp_tx_outstanding_bytes(c)==1);
    incoming(c,501,c->local_seq,TCP_ACK,1000,NULL);assert(c->state==TCP_STATE_ESTABLISHED);
    assert(!net_close(listener));
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"close")) final_close(false);
    else if(!strcmp(argv[1],"pending")) final_close(true);
    else if(!strcmp(argv[1],"inherited")) inherited_connection();
    else if(!strcmp(argv[1],"backlog")) inherited_backlog();
    else if(!strcmp(argv[1],"abort")) abort_controls();
    else if(!strcmp(argv[1],"deadline")) deferred_fin(false);
    else if(!strcmp(argv[1],"fin-failure")) deferred_fin(true);
    else if(!strcmp(argv[1],"simultaneous")) simultaneous_close();
    else if(!strcmp(argv[1],"future-ack")) future_ack();
    else if(!strcmp(argv[1],"wrap-ack")) wrap_ack();
    else if(!strcmp(argv[1],"syn-ack")) syn_ack();
    else if(!strcmp(argv[1],"stale-hold")) stale_handle(false);
    else if(!strcmp(argv[1],"stale-io")) stale_handle(true);
    else if(!strcmp(argv[1],"reuse")) reuse_handle();
    else if(!strcmp(argv[1],"stale-wait")) stale_wait();
    else if(!strcmp(argv[1],"rst-window")) reset_window();
    else if(!strcmp(argv[1],"rst-connect")) reset_connect();
    else if(!strcmp(argv[1],"receive-window")) receive_window();
    else if(!strcmp(argv[1],"receive-duplicate")) receive_duplicate();
    else if(!strcmp(argv[1],"terminal-error")) terminal_error();
    else if(!strcmp(argv[1],"terminal-eof")) terminal_eof();
    else if(!strcmp(argv[1],"timewait-pressure")) timewait_pressure();
    else if(!strcmp(argv[1],"repeat-syn")) repeat_syn();
    else assert(0);
    reset();printf("net-lifetime: %s PASS\n",argv[1]);
}
