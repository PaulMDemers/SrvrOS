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
static bool fail_send,fail_alloc;
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
void console_printf(const char *fmt,...) { (void)fmt; }
void kfree(void *p) { free(p); }
void *kmalloc(size_t n) { return fail_alloc?NULL:malloc(n); }
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
    frames=payload_frames=read_wakes=write_wakes=0;fail_send=fail_alloc=false;
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
    incoming(c,c->remote_next,1,TCP_ACK,65535,NULL);assert(tcp_tx_outstanding_bytes(c)==1);
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
    incoming(c,first+2,c->local_seq,TCP_ACK,65535,"cde");assert(c->rx_length==5);
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

static void receive_allocation(unsigned kind) {
    for(unsigned growth=0;growth<2;growth++) {
        struct tcp_connection *c=connected();uint64_t h=handle(c),listener=0;
        if(kind) c->outbound=false;
        if(kind==2) {
            listener=net_listen(8080);assert((int64_t)listener>0);
            c->local_port=8080;c->accepted=false;c->refs=0;
            assert(ensure_pending_connection(c,8080));
        } else assert(!net_hold(h));
        char old[NET_CONNECTION_RX_INITIAL_SIZE];memset(old,'x',sizeof(old));
        if(growth) {
            assert(append_connection_data(c,(uint8_t *)old,sizeof(old)));
            c->rx_offset=16; /* Consumed prefix forces compaction before failed growth. */
        }
        uint32_t seq=c->remote_next;uint64_t unread=c->rx_length-c->rx_offset;
        unsigned before=payload_frames;fail_alloc=true;
        incoming(c,seq,c->local_seq,TCP_ACK|TCP_FIN,65535,"0123456789abcdefghijklmnopqrstuv");
        assert(c->state==TCP_STATE_ESTABLISHED && !c->reset && !c->peer_closed);
        assert(c->remote_next==seq && c->rx_length-c->rx_offset==unread);
        assert(c->refs==(kind==2?0:2) && !c->error);
        if(kind!=2) assert(!net_error(h));
        assert(payload_frames==before && last_length==54 && last_frame[47]==TCP_ACK);
        assert(read_be32(last_frame+42)==seq);
        if(growth) assert(!memcmp(c->rx_data+c->rx_offset,old,unread));
        if(kind==2) assert(c->pending && pending_requests[0].used);
        fail_alloc=false;
        incoming(c,seq,c->local_seq,TCP_ACK|TCP_FIN,65535,"0123456789abcdefghijklmnopqrstuv");
        assert(c->remote_next==seq+33 && c->peer_closed);
        if(kind==2) assert(net_accept(listener,NULL,0,NULL,true)==(int64_t)h);
        char data[NET_CONNECTION_RX_INITIAL_SIZE+32];
        assert(net_read(h,data,sizeof(data),true)==(int64_t)(unread+32));
        assert(!memcmp(data,old,unread) && !memcmp(data+unread,"0123456789abcdefghijklmnopqrstuv",32));
        assert(net_read(h,data,1,true)==0);
        if(kind==2) assert(!net_close(listener));
    }
}
static void partial_ack(void) {
    for(unsigned wrap=0;wrap<2;wrap++) {
        struct tcp_connection *c=connected();if(wrap)c->local_seq=UINT32_MAX-2;
        uint32_t seq=c->local_seq;
        assert(net_respond(handle(c),"abcdef",6,true)==6);unsigned wakes=write_wakes;
        incoming(c,c->remote_next,seq+3,TCP_ACK,65535,NULL);
        assert(tcp_tx_outstanding_bytes(c)==3 && c->local_seq==seq+6 && write_wakes>wakes);
        struct tcp_tx_record saved[TCP_TX_HISTORY];memcpy(saved,c->tx_history,sizeof(saved));
        incoming(c,c->remote_next,seq+3,TCP_ACK,65535,NULL);
        incoming(c,c->remote_next,seq+2,TCP_ACK,65535,NULL);
        assert(!memcmp(saved,c->tx_history,sizeof(saved)));
        ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();
        assert(last_length==57 && read_be32(last_frame+38)==seq+3 && !memcmp(last_frame+54,"def",3));
        incoming(c,c->remote_next,seq+6,TCP_ACK,65535,NULL);assert(!tcp_tx_outstanding_bytes(c));
    }
}
static void partial_fin(void) {
    struct tcp_connection *c=connected();uint32_t seq=c->local_seq;
    assert(send_tcp_segment(c,c->local_port,TCP_ACK|TCP_FIN,(const uint8_t *)"abc",3));
    c->local_write_closed=true;c->state=TCP_STATE_FIN_WAIT_1;
    incoming(c,c->remote_next,seq+2,TCP_ACK,65535,NULL);
    assert(c->state==TCP_STATE_FIN_WAIT_1 && tcp_tx_outstanding_bytes(c)==2);
    ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();
    assert(last_length==55 && last_frame[54]=='c' && (last_frame[47]&TCP_FIN) && read_be32(last_frame+38)==seq+2);
    incoming(c,c->remote_next,seq+3,TCP_ACK,65535,NULL);
    assert(c->state==TCP_STATE_FIN_WAIT_1 && tcp_tx_outstanding_bytes(c)==1);
    ticks+=TCP_RETRANSMIT_TICKS*2;net_timer_tick();
    assert(last_length==54 && (last_frame[47]&TCP_FIN) && read_be32(last_frame+38)==seq+3);
    incoming(c,c->remote_next,seq+4,TCP_ACK,65535,NULL);assert(c->state==TCP_STATE_FIN_WAIT_2);
}
static void partial_window(void) {
    struct tcp_connection *c=connected();uint32_t seq=c->local_seq;
    char data[TCP_SEGMENT_PAYLOAD_MAX];memset(data,'x',sizeof(data));
    unsigned sent=0;
    while(sent<TCP_TX_WINDOW_BYTES) {
        unsigned n=TCP_TX_WINDOW_BYTES-sent;if(n>sizeof(data))n=sizeof(data);
        int64_t wrote=net_respond(handle(c),data,n,true);assert(wrote>0);sent+=(unsigned)wrote;
    }
    assert(!net_shutdown(handle(c),1) && c->fin_pending);
    incoming(c,c->remote_next,seq+1,TCP_ACK,65535,NULL);
    assert(!c->fin_pending && c->state==TCP_STATE_FIN_WAIT_1);
    assert((last_frame[47]&TCP_FIN) && c->local_seq==seq+TCP_TX_WINDOW_BYTES+1);
}
static void overlap_wrap(void) {
    struct tcp_connection *c=connected();c->remote_next=UINT32_MAX-2;uint32_t first=c->remote_next;
    incoming(c,first,c->local_seq,TCP_ACK,65535,"abc");assert(c->remote_next==0);
    incoming(c,first+1,c->local_seq+1,TCP_ACK|TCP_FIN,0,"bcde");
    assert(c->remote_next==0 && c->rx_length==3 && !c->peer_closed && c->peer_window==65535);
    incoming(c,first+1,c->local_seq,TCP_ACK|TCP_FIN,65535,"bcde");
    assert(c->remote_next==3 && c->rx_length==5 && c->peer_closed);
    unsigned wakes=read_wakes;
    incoming(c,first+1,c->local_seq,TCP_ACK|TCP_FIN,0,"bcde");
    assert(c->remote_next==3 && c->rx_length==5 && c->peer_window==65535 && wakes==read_wakes);
    char data[5];assert(net_read(handle(c),data,5,true)==5 && !memcmp(data,"abcde",5));
    c=connected();char full[NET_CONNECTION_RX_MAX_SIZE-1];memset(full,'x',sizeof(full));
    assert(append_connection_data(c,(const uint8_t *)full,sizeof(full)));
    first=c->remote_next;
    incoming(c,first-2,c->local_seq,TCP_ACK,0,"abxy");
    assert(c->remote_next==first && c->rx_length==sizeof(full) && c->peer_window==65535);
    incoming(c,first-2,c->local_seq,TCP_ACK,65535,"abx");
    assert(c->remote_next==first+1 && c->rx_length==NET_CONNECTION_RX_MAX_SIZE);
}


static void window_order(void) {
    for(unsigned wrap=0;wrap<2;wrap++) {
        struct tcp_connection *c=connected();
        if(wrap) { c->local_seq=UINT32_MAX-2;c->remote_next=UINT32_MAX-2; }
        uint32_t seq=c->local_seq,remote=c->remote_next;
        assert(net_respond(handle(c),"abcdef",6,true)==6);
        incoming(c,remote,seq+3,TCP_ACK,20,NULL);assert(c->peer_window==20);
        incoming(c,remote,seq+2,TCP_ACK,0,NULL);
        assert(c->peer_window==20 && !c->zero_window_deadline && tcp_tx_outstanding_bytes(c)==3);
        incoming(c,remote+2,seq+3,TCP_ACK,0,NULL);assert(!c->peer_window);
        uint64_t deadline=c->zero_window_deadline;unsigned wakes=write_wakes;
        c->zero_window_probes=2;
        incoming(c,remote+1,seq+4,TCP_ACK,60,NULL);
        assert(!c->peer_window && c->zero_window_deadline==deadline && tcp_tx_outstanding_bytes(c)==2);
        assert(c->zero_window_probes==2);
        assert(!(net_poll_events(handle(c),SRV_POLLOUT)&SRV_POLLOUT));
        /* New sequence cannot legitimize an ACK older than SND.UNA. */
        incoming(c,remote+3,seq+2,TCP_ACK,60,NULL);assert(!c->peer_window);
        incoming(c,remote+2,seq+4,TCP_ACK,60,NULL);
        assert(c->peer_window==60 && !c->zero_window_deadline && write_wakes>wakes);
        /* Identical sequence/ACK can legitimately reopen a closed window. */
        incoming(c,remote+2,seq+4,TCP_ACK,0,NULL);assert(!c->peer_window);
        incoming(c,remote+2,seq+4,TCP_ACK,60,NULL);assert(c->peer_window==60);
    }
}
static void window_overlap(void) {
    struct tcp_connection *c=connected();uint32_t remote=c->remote_next;
    incoming(c,remote,c->local_seq,TCP_ACK,60,"abc");
    incoming(c,remote+3,c->local_seq,TCP_ACK,0,NULL);assert(!c->peer_window);
    incoming(c,remote+1,c->local_seq,TCP_ACK,60,"bcde");
    assert(c->rx_length==5 && c->remote_next==remote+5 && !c->peer_window);
    char data[5];assert(net_read(handle(c),data,5,true)==5 && !memcmp(data,"abcde",5));
}
static void window_handshake(void) {
    reset();uint64_t listener=net_listen(8080);assert((int64_t)listener>0);
    uint8_t mac[6]={0};struct tcp_connection *c=alloc_connection(8080,0x0a000202,40000,mac);assert(c);
    c->remote_next=501;assert(send_tcp_segment(c,c->local_port,TCP_SYN|TCP_ACK,NULL,0));
    uint16_t initial=c->peer_window;uint32_t seq=c->local_seq;
    incoming(c,501,seq+1,TCP_ACK,0,NULL);
    assert(c->state==TCP_STATE_SYN_RECEIVED && c->peer_window==initial && !c->zero_window_deadline);
    incoming(c,502,seq,TCP_ACK,0,NULL);assert(c->peer_window==initial);
    incoming(c,501,seq,TCP_ACK,0,NULL);assert(c->state==TCP_STATE_ESTABLISHED && !c->peer_window);
    incoming(c,501,seq,TCP_ACK,20,NULL);assert(c->peer_window==20);
    assert(!net_close(listener));
}
/* Feed captured wire fields through the other endpoint's real packet parser. */
static void deliver_last(struct tcp_connection *to) {
    uint8_t packet[1600];uint16_t n=last_length;memcpy(packet,last_frame,n);
    assert(n==54 || n==55);char data[2]={0};if(n==55)data[0]=(char)packet[54];
    incoming(to,read_be32(packet+38),read_be32(packet+42),packet[47],read_be16(packet+48),n==55?data:NULL);
}
static void persist_reopen(void) {
    for(unsigned wrap=0;wrap<2;wrap++) for(unsigned queued=0;queued<2;queued++) {
        struct tcp_connection *c=connected();if(wrap)c->local_seq=0;
        uint32_t seq=c->local_seq;
        struct tcp_connection *peer=&tcp_connections[1];*peer=*c;
        peer->id=43;peer->local_port=80;peer->remote_port=50000;peer->remote_ip=0x0a000203;
        peer->local_seq=c->remote_next;peer->remote_next=c->local_seq;
        peer->rx_data=malloc(NET_CONNECTION_RX_MAX_SIZE);assert(peer->rx_data);
        memset(peer->rx_data,'x',NET_CONNECTION_RX_MAX_SIZE);
        peer->rx_length=peer->rx_capacity=NET_CONNECTION_RX_MAX_SIZE;
        if(queued) assert(net_respond(handle(c),"Z",1,true)==1); /* Data not delivered. */
        incoming(c,c->remote_next,seq,TCP_ACK,0,NULL);
        assert(net_respond(handle(c),"Z",1,true)==SRV_ERR_AGAIN);
        char byte;assert(net_read(handle(peer),&byte,1,true)==1); /* Drop window-open ACK. */
        fail_send=true;ticks=c->zero_window_deadline;net_timer_tick(); /* Refused first probe. */
        assert(c->local_seq==seq+queued && tcp_tx_outstanding_bytes(c)==queued && !c->reset);
        fail_send=false;ticks=c->zero_window_deadline;net_timer_tick();
        unsigned before=frames;deliver_last(peer);
        assert(frames==before+1); /* A probe must elicit the lost window update. */
        deliver_last(c);assert(!tcp_tx_outstanding_bytes(c));
        if(!queued) {
            assert(c->peer_window==1 && !c->zero_window_deadline && c->local_seq==seq);
            assert(net_poll_events(handle(c),SRV_POLLOUT)&SRV_POLLOUT);
            assert(net_respond(handle(c),"Z",1,true)==1);deliver_last(peer);
        } else {
            /* A one-byte probe was accepted and ACKed; do not send it twice. */
            assert(c->local_seq==seq+1 && !c->peer_window);
            assert(net_read(handle(peer),&byte,1,true)==1);deliver_last(c);
            assert(c->peer_window==1 && !c->zero_window_deadline);
        }
        assert(peer->remote_next==seq+1 && peer->rx_length-peer->rx_offset==NET_CONNECTION_RX_MAX_SIZE-queued);
        assert(peer->rx_data[peer->rx_length-1]=='Z');
    }
}
static void persist_responsive(bool data) {
    struct tcp_connection *c=connected();uint32_t seq=c->local_seq;
    if(data) {
        assert(net_respond(handle(c),"abcdef",6,true)==6);
        for(unsigned i=0;i<TCP_TX_HISTORY;i++) if(c->tx_history[i].used)c->tx_history[i].retries=TCP_RETRANSMIT_MAX;
    }
    incoming(c,c->remote_next,seq,TCP_ACK,0,NULL);
    uint32_t end=c->local_seq;uint64_t previous_delay=0;
    for(unsigned i=0;i<50;i++) {
        ticks=c->zero_window_deadline;unsigned before=frames;net_timer_tick();
        assert(!c->reset && c->state==TCP_STATE_ESTABLISHED && frames==before+1);
        if(data) assert(last_length==55 && last_frame[54]=='a' && read_be32(last_frame+38)==seq);
        else assert(last_length==54);
        assert(c->local_seq==end && tcp_tx_outstanding_bytes(c)==(data?6:0));
        uint64_t delay=c->zero_window_deadline-ticks;
        assert(delay>=previous_delay && delay<=TCP_CLOSE_TIMEOUT_TICKS);previous_delay=delay;
        incoming(c,c->remote_next,seq,TCP_ACK,0,NULL);
    }
    assert(ticks>TCP_IDLE_TIMEOUT_TICKS && !c->reset && !net_error(handle(c)));
    incoming(c,c->remote_next,seq,TCP_ACK,64,NULL);assert(!c->zero_window_deadline);
    if(data) {
        ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();
        assert(last_length==60 && !memcmp(last_frame+54,"abcdef",6));
        incoming(c,c->remote_next,end,TCP_ACK,64,NULL);assert(!tcp_tx_outstanding_bytes(c));
    }
    assert(net_respond(handle(c),"z",1,true)==1);
}
static void persist_silent(void) {
    for(unsigned fail=0;fail<2;fail++) {
        struct tcp_connection *c=connected();incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,NULL);
        fail_send=fail!=0;
        for(unsigned i=0;i<=TCP_ZERO_WINDOW_MAX;i++) { ticks=c->zero_window_deadline;net_timer_tick(); }
        assert(c->reset && net_error(handle(c))==SRV_NETERR_TIMEDOUT && c->refs==1);
        ticks=c->close_deadline;net_timer_tick();assert(c->state==TCP_STATE_CLOSED);
        assert(!net_close(handle(c)) && !c->state);
    }
}
static void persist_close(void) {
    struct tcp_connection *c=connected();
    incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,NULL);
    assert(!net_shutdown(handle(c),1));uint64_t deadline=c->close_deadline;
    ticks=deadline;net_timer_tick();
    assert(c->state==TCP_STATE_CLOSED && net_error(handle(c))==SRV_NETERR_TIMEDOUT);
}


static uint16_t wire_packet(uint8_t *frame,struct tcp_connection *c,uint32_t seq,uint32_t ack,
    uint8_t flags,uint16_t fragments,const uint8_t *options,unsigned option_size,const char *data,unsigned ip_extra) {
    assert(option_size<=40 && !(option_size%4) && (ip_extra==0 || ip_extra==4));
    size_t n=data?strlen(data):0;assert(n<=64);memset(frame,0,256);
    memcpy(frame,local_mac,6);frame[6]=2;frame[11]=2;write_be16(frame+12,ETH_TYPE_IPV4);
    uint8_t *ip=frame+14,*tcp=ip+20+ip_extra;unsigned h=20+option_size;
    ip[0]=(uint8_t)(0x45+ip_extra/4);ip[8]=64;ip[9]=IP_PROTO_TCP;
    write_be16(ip+2,20+ip_extra+h+n);write_be16(ip+6,fragments);
    write_be32(ip+12,c->remote_ip);write_be32(ip+16,local_ip);
    write_be16(tcp,c->remote_port);write_be16(tcp+2,c->local_port);
    write_be32(tcp+4,seq);write_be32(tcp+8,ack);tcp[12]=(uint8_t)((h/4)<<4);tcp[13]=flags;
    write_be16(tcp+14,65535);if(option_size)memcpy(tcp+20,options,option_size);
    if(n)memcpy(tcp+h,data,n);
    write_be16(tcp+16,tcp_checksum(c->remote_ip,local_ip,tcp,h+n));
    write_be16(ip+10,checksum_bytes(ip,20+ip_extra,0));return 14+20+ip_extra+h+n;
}
static void wire_incoming(struct tcp_connection *c,uint32_t seq,uint32_t ack,uint8_t flags,
    uint16_t fragments,const uint8_t *options,unsigned option_size,const char *data) {
    uint8_t frame[256];uint16_t n=wire_packet(frame,c,seq,ack,flags,fragments,options,option_size,data,0);
    handle_ipv4(frame,n);
}
static void ipv4_fragments(void) {
    const uint16_t bad[]={0x2000,1,0x2001,0x8000,0x6000,0x1fff};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        struct tcp_connection *c=connected();uint32_t seq=c->remote_next;
        assert(net_respond(handle(c),"x",1,true)==1);unsigned before=frames;
        wire_incoming(c,seq,c->local_seq,TCP_ACK|TCP_FIN,bad[i],NULL,0,"bad");
        assert(!c->rx_length && !c->peer_closed && c->remote_next==seq && frames==before);
        assert(tcp_tx_outstanding_bytes(c)==1);
        wire_incoming(c,seq,c->local_seq,TCP_RST,bad[i],NULL,0,NULL);assert(!c->reset);
        wire_incoming(c,seq,c->local_seq,TCP_ACK,0x4000,NULL,0,"ok");
        assert(c->rx_length==2 && !tcp_tx_outstanding_bytes(c));
    }
}
static void fragment_protocols(void) {
    for(unsigned protocol=1;protocol<=17;protocol+=16) {
        struct tcp_connection *c=connected();uint8_t frame[256];
        (void)wire_packet(frame,c,c->remote_next,c->local_seq,TCP_ACK,0x2000,NULL,0,NULL,0);
        uint8_t *ip=frame+14,*payload=ip+20;ip[9]=(uint8_t)protocol;
        write_be16(ip+2,32);memset(payload,0,12);
        if(protocol==17) { write_be16(payload,40000);write_be16(payload+2,5000);write_be16(payload+4,12); }
        else { payload[0]=8;write_be16(payload+2,checksum_bytes(payload,12,0)); }
        write_be16(ip+10,0);write_be16(ip+10,checksum_bytes(ip,20,0));
        uint64_t before=protocol==17?udp_packets:icmp_packets;
        handle_ipv4(frame,46);assert((protocol==17?udp_packets:icmp_packets)==before && !frames);
        write_be16(ip+6,0x4000);write_be16(ip+10,0);write_be16(ip+10,checksum_bytes(ip,20,0));
        handle_ipv4(frame,46);assert((protocol==17?udp_packets:icmp_packets)==before+1);
    }
}
static void packet_bounds(void) {
    for(unsigned extra=0;extra<=4;extra+=4) {
        struct tcp_connection *c=connected();uint8_t frame[256];
        uint16_t n=wire_packet(frame,c,c->remote_next,c->local_seq,TCP_ACK,0,NULL,0,"abc",extra);
        for(unsigned len=0;len<n;len++) {
            uint8_t *short_frame=malloc(len?len:1);assert(short_frame);memcpy(short_frame,frame,len);
            handle_ipv4(short_frame,len);free(short_frame);assert(!c->rx_length && !frames);
        }
        frame[24]^=1;handle_ipv4(frame,n);assert(!c->rx_length);frame[24]^=1;
        frame[n-1]^=1;handle_ipv4(frame,n);assert(!c->rx_length);frame[n-1]^=1;
        uint8_t *tcp=frame+34+extra;
        for(unsigned words=0;words<16;words++) if(words<5 || words>5) {
            tcp[12]=(uint8_t)(words<<4);write_be16(tcp+16,0);
            write_be16(tcp+16,tcp_checksum(c->remote_ip,local_ip,tcp,23));
            handle_ipv4(frame,n);assert(!c->rx_length && !frames);
        }
        tcp[12]=0x50;write_be16(tcp+16,0);write_be16(tcp+16,tcp_checksum(c->remote_ip,local_ip,tcp,23));
        memset(frame+n,0xa5,8);handle_ipv4(frame,n+8); /* Ethernet padding is not TCP data. */
        assert(c->rx_length==3 && !memcmp(c->rx_data,"abc",3));
    }
}
static void option_invalid(void) {
    static const uint8_t bad[][8]={{30,0,0,0},{30,1,0,0},{30,5,0,0},{1,1,1,30},
        {2,3,1,0},{2,4,0,0},{2,4,0,64,2,4,0,32}};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        struct tcp_connection *c=connected();c->state=TCP_STATE_SYN_SENT;
        assert(send_tcp_segment(c,c->local_port,TCP_SYN,NULL,0));unsigned before=frames;
        wire_incoming(c,500,c->local_seq,TCP_SYN|TCP_ACK,0,bad[i],i==6?8:4,NULL);
        assert(c->state==TCP_STATE_SYN_SENT && frames==before && tcp_tx_outstanding_bytes(c)==1);
    }
    for(unsigned i=0;i<5;i++) {
        struct tcp_connection *c=connected();assert(net_respond(handle(c),"x",1,true)==1);unsigned before=frames;
        wire_incoming(c,c->remote_next,c->local_seq,TCP_RST,0,bad[i],4,NULL);
        assert(!c->reset && frames==before);
        wire_incoming(c,c->remote_next,c->local_seq,TCP_ACK|TCP_FIN,0,bad[i],4,"bad");
        assert(!c->rx_length && !c->peer_closed && frames==before && tcp_tx_outstanding_bytes(c)==1);
    }
    for(unsigned length=0;length<256;length++) if(length<2 || length>4) {
        const uint8_t options[]={30,(uint8_t)length,0,0};struct tcp_connection *c=connected();
        wire_incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,options,4,"bad");
        assert(!c->rx_length && !frames);
    }
}
static void option_valid(void) {
    const uint8_t options[]={1,30,3,99,0,0,0,0}; /* Unaligned unknown option, NOP, EOL/padding. */
    struct tcp_connection *c=connected();
    wire_incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,options,sizeof(options),"ok");
    assert(c->rx_length==2 && !memcmp(c->rx_data,"ok",2));
    for(unsigned length=2;length<=40;length++) {
        uint8_t list[40]={30,(uint8_t)length};c=connected();
        wire_incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,list,(length+3)&~3u,"ok");
        assert(c->rx_length==2 && !memcmp(c->rx_data,"ok",2));
    }
}
static void mss_connection(bool active,bool absent) {
    const uint16_t values[]={64,1,1460,65535};
    for(unsigned i=0;i<(absent?1:4);i++) {
        reset();uint8_t mac[6]={0};uint64_t listener=0;
        struct tcp_connection *c;
        uint8_t options[]={1,2,4,(uint8_t)(values[i]>>8),(uint8_t)values[i],0,0,0};
        if(active) {
            c=alloc_client_connection(0x0a000202,80,mac);assert(c);
            assert(send_tcp_segment(c,c->local_port,TCP_SYN,NULL,0));
            wire_incoming(c,500,c->local_seq,TCP_SYN|TCP_ACK,0,absent?NULL:options,absent?0:8,NULL);
        } else {
            listener=net_listen(8080);assert((int64_t)listener>0);
            struct tcp_connection address={.local_port=8080,.remote_port=40000,.remote_ip=0x0a000202};
            wire_incoming(&address,500,0,TCP_SYN,0,absent?NULL:options,absent?0:8,NULL);
            c=find_connection(8080,0x0a000202,40000);assert(c);
            /* Duplicate SYN cannot renegotiate the already retained peer limit. */
            const uint8_t changed[]={2,4,0,2};
            wire_incoming(c,500,0,TCP_SYN,0,changed,4,NULL);
            wire_incoming(c,501,c->local_seq,TCP_ACK,0,NULL,0,NULL);
            assert(net_accept(listener,NULL,0,NULL,true)==(int64_t)handle(c));
        }
        assert(c->state==TCP_STATE_ESTABLISHED);
        const uint8_t later[]={2,4,0,2};
        wire_incoming(c,c->remote_next,c->local_seq,TCP_ACK,0,later,4,NULL); /* No late MSS renegotiation. */
        unsigned limit=absent?536:(values[i]>1400?1400:values[i]);
        char data[1400];memset(data,'m',sizeof(data));
        assert(net_respond(handle(c),data,sizeof(data),true)==(int64_t)limit);
        assert(last_length==54+limit && read_be16(last_frame+16)==40+limit);
        assert(tcp_checksum(local_ip,c->remote_ip,last_frame+34,20+limit)==0);
        ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();assert(last_length==54+limit);
        incoming(c,c->remote_next,c->local_seq,TCP_ACK,65535,NULL);assert(!tcp_tx_outstanding_bytes(c));
        if(listener)assert(!net_close(listener));
    }
}
static void mss_syn_wire(void) {
    reset();uint8_t mac[6]={0};struct tcp_connection *c=alloc_client_connection(0x0a000202,80,mac);assert(c);
    assert(send_tcp_segment(c,c->local_port,TCP_SYN,NULL,0));
    uint32_t seq=c->local_seq;
    assert(last_length==58 && last_frame[46]==0x60 && last_frame[54]==2 && last_frame[55]==4);
    assert(read_be16(last_frame+56)==1400 && tcp_checksum(local_ip,c->remote_ip,last_frame+34,24)==0);
    ticks+=TCP_RETRANSMIT_TICKS;net_timer_tick();
    assert(last_length==58 && c->local_seq==seq && read_be16(last_frame+56)==1400);
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
    else if(!strcmp(argv[1],"allocation-outbound")) receive_allocation(0);
    else if(!strcmp(argv[1],"allocation-inbound")) receive_allocation(1);
    else if(!strcmp(argv[1],"allocation-backlog")) receive_allocation(2);
    else if(!strcmp(argv[1],"partial-ack")) partial_ack();
    else if(!strcmp(argv[1],"partial-fin")) partial_fin();
    else if(!strcmp(argv[1],"partial-window")) partial_window();
    else if(!strcmp(argv[1],"overlap-wrap")) overlap_wrap();
    else if(!strcmp(argv[1],"window-order")) window_order();
    else if(!strcmp(argv[1],"window-overlap")) window_overlap();
    else if(!strcmp(argv[1],"window-handshake")) window_handshake();
    else if(!strcmp(argv[1],"persist-reopen")) persist_reopen();
    else if(!strcmp(argv[1],"persist-responsive")) persist_responsive(false);
    else if(!strcmp(argv[1],"persist-data")) persist_responsive(true);
    else if(!strcmp(argv[1],"persist-silent")) persist_silent();
    else if(!strcmp(argv[1],"persist-close")) persist_close();
    else if(!strcmp(argv[1],"ipv4-fragments")) {ipv4_fragments();fragment_protocols();}
    else if(!strcmp(argv[1],"packet-bounds")) packet_bounds();
    else if(!strcmp(argv[1],"option-invalid")) option_invalid();
    else if(!strcmp(argv[1],"option-valid")) option_valid();
    else if(!strcmp(argv[1],"mss-passive")) mss_connection(false,false);
    else if(!strcmp(argv[1],"mss-active")) mss_connection(true,false);
    else if(!strcmp(argv[1],"mss-default")) {mss_connection(false,true);mss_connection(true,true);}
    else if(!strcmp(argv[1],"mss-syn-wire")) mss_syn_wire();
    else assert(0);
    reset();printf("net-lifetime: %s PASS\n",argv[1]);
}
