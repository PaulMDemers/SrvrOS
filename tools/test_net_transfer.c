/* Real queue ownership with deterministic copy refusal and descriptor changes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/net.c"
static bool current=true, refuse;
static unsigned copies, wakes, frames;
static uint8_t output[16];
static uint64_t remaining;
static void (*wait_hook)(void);
static struct process *owner(void) { return (struct process *)1; }
struct process *process_current(void) { return owner(); }
bool process_should_exit_current(void) { return false; }
bool process_signal_pending_current(void) { return false; }
uint64_t timer_ticks(void) { return 100; }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
void console_printf(const char *s,...) { (void)s; }
void process_file_poll_wake(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; wakes++; }
void scheduler_yield(void) {}
uint64_t net_device_poll(uint64_t n,bool verbose) { (void)n;(void)verbose;return 0; }
bool net_device_send(const uint8_t *p,uint16_t n) { (void)p;(void)n;frames++;return true; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) {
    (void)q;assert(wait_hook);void (*hook)(void)=wait_hook;wait_hook=NULL;hook();assert(fn(arg));return true;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t end) {
    (void)q;(void)fn;(void)arg;(void)end;assert(0);return false;
}
static bool identity(void *p) { assert(p==output);return current; }
static int64_t copyout(void *p,const uint8_t *data,uint64_t n,uint64_t left,uint32_t ip,uint16_t port) {
    (void)ip;(void)port;assert(p==output && n<=sizeof(output));copies++;
    if(refuse)return SRV_ERR_FAULT;
    if(n) memcpy(output,data,n);
    remaining=left;return 0;
}
static const struct net_transfer transfer={output,identity,copyout};
static void replaced(void) { current=false; }
static void reset(void) {
    for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++)free(tcp_connections[i].rx_data);
    memset(tcp_connections,0,sizeof(tcp_connections));memset(pending_requests,0,sizeof(pending_requests));
    memset(listeners,0,sizeof(listeners));memset(udp_sockets,0,sizeof(udp_sockets));
    initialized=worker_started=true;current=true;refuse=false;copies=wakes=frames=0;wait_hook=NULL;
    memset(output,'?',sizeof(output));remaining=99;
    static const uint8_t mac[6]={2,0,0,0,0,1};local_mac=mac;local_ip=0x0a00020f;
}
static struct tcp_connection *connection(void) {
    struct tcp_connection *c=&tcp_connections[0];
    *c=(struct tcp_connection){.state=TCP_STATE_ESTABLISHED,.id=42,.refs=1,.owner=owner(),
        .accepted=true,.local_port=9000,.remote_port=1234,.peer_window=65535,.remote_ip=0x0a000202};
    c->rx_data=malloc(16);assert(c->rx_data);memcpy(c->rx_data,"abcdef",6);c->rx_length=6;
    return c;
}
int main(void) {
    reset();int64_t udp=net_udp_open();assert(udp>0 && !net_udp_bind(udp,9000));
    udp_deliver(9000,123,456,(const uint8_t *)"abc",3);
    refuse=true;assert(net_udp_recv_checked(udp,2,true,&transfer)==SRV_ERR_FAULT);
    assert(find_udp_socket(udp)->count==1 && output[0]=='?');
    refuse=false;assert(net_udp_recv_checked(udp,2,true,&transfer)==2 && !memcmp(output,"ab",2));
    assert(!find_udp_socket(udp)->count);
    assert(!net_hold(udp));unsigned before=wakes;assert(!net_close(udp) && wakes>before);
    wait_hook=replaced;assert(net_udp_recv_checked(udp,2,false,&transfer)==SRV_ERR_BADF);
    assert(find_udp_socket(udp));

    reset();struct tcp_connection *c=connection();uint64_t h=make_handle(NET_HANDLE_CONNECTION,42);
    refuse=true;assert(net_read_checked(h,2,true,false,&transfer)==SRV_ERR_FAULT && !c->rx_offset && !frames);
    refuse=false;assert(net_read_checked(h,2,true,true,&transfer)==2 && !c->rx_offset && !frames);
    assert(net_read_checked(h,2,true,false,&transfer)==2 && c->rx_offset==2 && remaining==4);
    c->rx_offset=c->rx_length;wait_hook=replaced;
    assert(net_read_checked(h,2,false,false,&transfer)==SRV_ERR_BADF && c->refs==1);
    current=true;c->peer_window=0;wait_hook=replaced;before=frames;
    assert(net_respond_checked(h,"x",1,false,&transfer)==SRV_ERR_BADF && frames==before);

    reset();c=connection();c->accepted=false;c->pending=true;c->refs=0;c->rx_offset=1;
    listeners[0]=(struct net_listener){.used=true,.refs=1,.id=1,.port=9000,.owner=owner()};
    pending_requests[0]=(struct pending_request){.used=true,.port=9000,.connection_index=0,.connection_id=42,.owner=owner()};
    uint64_t listener=make_handle(NET_HANDLE_LISTENER,1);
    refuse=true;assert(net_accept_checked(listener,2,true,&transfer)==SRV_ERR_FAULT);
    assert(c->pending && !c->accepted && !c->refs && c->rx_offset==1 && pending_requests[0].used);
    refuse=false;assert(net_accept_checked(listener,2,true,&transfer)==(int64_t)h);
    assert(!memcmp(output,"bc",2) && remaining==3 && c->rx_offset==3 && c->refs==1);
    assert(!pending_requests[0].used && c->accepted && !c->pending);
    wait_hook=replaced;assert(net_accept_checked(listener,0,false,&transfer)==SRV_ERR_BADF);
    current=true;wait_hook=replaced; /* Raw callers also refind a closed listener. */
    struct accept_wait_arg wait={listener,NULL};listeners[0].id=2;
    assert(accept_ready(&wait));assert(net_accept(listener,NULL,0,NULL,true)<0);
    reset();puts("net-transfer: PASS refused copies preserve queues, peer/length output, peek, accept offsets and replaced wait identities");
}
