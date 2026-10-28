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
static uint8_t last_frame[1600];
static uint16_t last_length;
static bool fail_send;
static struct process *caller;
static unsigned char creator_token,child_token;
static struct process *creator(void) { return (struct process *)&creator_token; }
static struct process *child(void) { return (struct process *)&child_token; }
uint64_t timer_ticks(void) { return ticks; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
void process_file_poll_wake(void) {}
void kfree(void *p) { free(p); }
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
    initialized=worker_started=true;caller=creator();ticks=100;frames=payload_frames=0;fail_send=false;
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
    ticks=c->close_deadline;net_timer_tick();assert(!c->state);
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
    else assert(0);
    reset();printf("net-lifetime: %s PASS\n",argv[1]);
}
