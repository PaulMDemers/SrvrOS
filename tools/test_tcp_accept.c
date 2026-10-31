#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/net.c"

struct process { int unused; };
static struct process owner;
static unsigned accept_wakes, poll_wakes, sent;
static uint8_t last_flags;
uint64_t timer_ticks(void) { return 100; }
void *kmalloc(size_t size) { return malloc(size); }
void kfree(void *p) { free(p); }
struct process *process_current(void) { return &owner; }
bool process_should_exit_current(void) { return false; }
bool process_signal_pending_current(void) { return false; }
void process_file_poll_wake(void) { poll_wakes++; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { if(q==&accept_wait_queue)accept_wakes++; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) {
    (void)q;(void)fn;(void)arg;assert(!"unexpected blocking wait");return false;
}
void scheduler_yield(void) { assert(!"unexpected yield"); }
uint64_t net_device_poll(uint64_t budget,bool verbose) { (void)budget;(void)verbose;return 0; }
const uint8_t *net_device_mac(void) { static const uint8_t mac[6]={2,0,0,0,0,1};return mac; }
bool net_device_send(const uint8_t *frame,uint16_t size) {
    assert(size>=54);last_flags=frame[47];sent++;return true;
}

static void packet(uint32_t seq,uint32_t ack,uint8_t flags,const char *data) {
    uint8_t frame[128]={0};uint8_t *ip=frame+14,*tcp=ip+20;
    size_t n=data?strlen(data):0;
    frame[6]=2;frame[11]=2;
    write_be32(ip+12,0x0a000202);write_be32(ip+16,0x0a00020f);
    write_be16(tcp,40000);write_be16(tcp+2,7122);
    write_be32(tcp+4,seq);write_be32(tcp+8,ack);
    tcp[12]=5<<4;tcp[13]=flags;write_be16(tcp+14,32768);
    if(n)memcpy(tcp+20,data,n);
    write_be16(tcp+16,tcp_checksum(0x0a000202,0x0a00020f,tcp,20+n));
    handle_tcp(frame,54+n,ip,20,20+n,0x0a000202);
}
static struct tcp_connection *start(void) {
    for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++)if(tcp_connections[i].rx_data)free(tcp_connections[i].rx_data);
    memset(tcp_connections,0,sizeof(tcp_connections));memset(pending_requests,0,sizeof(pending_requests));
    memset(listeners,0,sizeof(listeners));
    initialized=true;listeners[0]=(struct net_listener){.used=true,.id=1,.refs=1,.owner=&owner,.port=7122};
    accept_wakes=poll_wakes=sent=0;
    packet(1000,0,TCP_SYN,NULL);
    struct tcp_connection *c=find_connection(7122,0x0a000202,40000);
    assert(c && c->state==TCP_STATE_SYN_RECEIVED && !c->pending && !accept_wakes);
    assert(net_poll_events(make_handle(NET_HANDLE_LISTENER,1),SRV_POLLIN)==0);
    return c;
}
static unsigned queued(void) {
    unsigned n=0;for(unsigned i=0;i<NET_PENDING_REQUESTS;i++)n+=pending_requests[i].used;return n;
}
int main(void) {
    struct tcp_connection *c=start();uint32_t ack=c->local_seq;
    packet(1001,ack,TCP_ACK,NULL);
    assert(c->state==TCP_STATE_ESTABLISHED && c->pending && queued()==1);
    assert(accept_wakes==1 && poll_wakes && c->rx_length==0);
    uint64_t listener=make_handle(NET_HANDLE_LISTENER,1);
    assert(net_poll_events(listener,SRV_POLLIN)==SRV_POLLIN);
    assert(net_poll_events(listener,SRV_POLLIN)==SRV_POLLIN);
    packet(1001,ack,TCP_ACK,NULL);assert(queued()==1 && accept_wakes==1);
    uint64_t length=99;int64_t client=net_accept(listener,NULL,0,&length,true);
    assert(client>0 && length==0 && c->accepted && !c->pending && !queued());
    assert(net_accept(listener,NULL,0,NULL,true)==SRV_ERR_AGAIN);
    assert(net_poll_events(client,SRV_POLLIN|SRV_POLLOUT)==SRV_POLLOUT);
    packet(1001,ack,TCP_ACK|TCP_PSH,"hello");
    assert(c->rx_length==5 && !memcmp(c->rx_data,"hello",5));
    assert(net_poll_events(client,SRV_POLLIN)==SRV_POLLIN && !queued());

    c=start();ack=c->local_seq;
    packet(1001,ack-1,TCP_ACK,NULL);assert(c->state==TCP_STATE_SYN_RECEIVED && !queued());
    packet(1001,ack+1,TCP_ACK,NULL);assert(c->state==TCP_STATE_SYN_RECEIVED && !queued());
    packet(1002,ack,TCP_ACK,NULL);assert(c->state==TCP_STATE_SYN_RECEIVED && !queued());
    packet(1001,ack,TCP_ACK|TCP_PSH,"first");
    assert(c->pending && queued()==1 && c->rx_length==5);
    packet(1006,ack,TCP_ACK|TCP_FIN,NULL);
    assert(c->peer_closed && queued()==1);
    client=net_accept(listener,NULL,0,&length,true);assert(client>0 && length==5);
    assert((net_poll_events(client,SRV_POLLIN)&(SRV_POLLIN|SRV_POLLHUP))==(SRV_POLLIN|SRV_POLLHUP));

    c=start();ack=c->local_seq;
    for(unsigned i=0;i<NET_PENDING_REQUESTS;i++)pending_requests[i].used=true;
    packet(1001,ack,TCP_ACK,NULL);
    assert(c->state==TCP_STATE_EMPTY && (last_flags&TCP_RST));
    assert(queued()==NET_PENDING_REQUESTS && accept_wakes==1);
    puts("tcp-accept: PASS empty handshake readiness, one queue entry, drain/write/read/FIN, invalid ACK/sequence and full queue");
}
