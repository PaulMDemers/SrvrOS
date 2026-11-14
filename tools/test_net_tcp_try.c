/* Actual dormant finite TCP APIs. Process tokens below are labeled HOST stubs;
 * they do not establish a native process/scheduler lifetime or TCP acceptance. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/net.c"

static unsigned char creator_token, child_token;
static struct process *caller;
static bool exiting, identity_valid, refuse_output;
static unsigned process_checks, exit_checks, identity_checks, attempts, accepted;
static uint8_t last_frame[1514], before_connections[sizeof tcp_connections];
static uint16_t last_length;
static void (*identity_hook)(void), (*output_hook)(void);
static const uint8_t station[6]={2,0,0,0,0,1}, router_mac[6]={2,0,0,0,0,2};

struct process *process_current(void) { process_checks++;return caller; }
bool process_should_exit_current(void) { exit_checks++;return exiting; }
bool process_signal_pending_current(void) { assert(!"unexpected signal check");return false; }
uint64_t timer_ticks(void) { return 100; } /* Frozen clock: finite paths still return. */
uint64_t timer_monotonic_us(void) { assert(!"unexpected probe clock callback");return 0; }
const uint8_t *net_device_mac(void) { assert(!"unexpected provider MAC callback");return NULL; }
bool net_device_link(struct net_device_link *out) { (void)out;assert(!"unexpected provider link callback");return false; }
bool timer_delay_us(uint64_t us) { (void)us;assert(!"finite API delayed");return false; }
void scheduler_yield(void) { assert(!"finite API yielded"); }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) {
    (void)q;(void)fn;(void)arg;assert(!"finite API waited");return false;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t end) {
    (void)q;(void)fn;(void)arg;(void)end;assert(!"finite API timed wait");return false;
}
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    (void)budget;(void)verbose;assert(!"finite API polled");return 0;
}
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
void process_file_poll_wake(void) {}
void console_printf(const char *fmt,...) { (void)fmt; }
void *kmalloc(size_t size) { return malloc(size); }
void kfree(void *p) { free(p); }
bool net_device_send(const uint8_t *frame,uint16_t length) {
    assert(length<=sizeof last_frame);attempts++;last_length=length;memcpy(last_frame,frame,length);
    if(output_hook) { void (*hook)(void)=output_hook;output_hook=NULL;hook(); }
    if(refuse_output)return false;
    accepted++;return true;
}
static bool fd_current(void *context) {
    assert(context==&identity_valid);identity_checks++;
    if(identity_hook) { void (*hook)(void)=identity_hook;identity_hook=NULL;hook(); }
    return identity_valid;
}
static const struct net_transfer transfer={.context=&identity_valid,.current=fd_current};
static uint64_t handle(void) { return make_handle(NET_HANDLE_CONNECTION,42); }
static void reset(void) {
    for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++)free(tcp_connections[i].rx_data);
    memset(tcp_connections,0,sizeof tcp_connections);memset(pending_requests,0,sizeof pending_requests);
    memset(listeners,0,sizeof listeners);memset(udp_sockets,0,sizeof udp_sockets);memset(arp_cache,0,sizeof arp_cache);
    initialized=true;worker_started=false;local_mac=station;
    bounded_poll_owner=NULL;
    const uint8_t ip[4]={10,0,2,15};set_local_ip_bytes(ip);memset(dhcp_router_ip_bytes,0,sizeof dhcp_router_ip_bytes);
    next_client_port=TCP_CLIENT_PORT_FIRST;next_connection_id=1;ip_ident=1;
    caller=(struct process *)&creator_token;exiting=refuse_output=false;identity_valid=true;
    process_checks=exit_checks=identity_checks=attempts=accepted=0;last_length=0;
    identity_hook=output_hook=NULL;
}
static struct tcp_connection *connected(void) {
    reset();struct tcp_connection *c=&tcp_connections[0];
    *c=(struct tcp_connection){.state=TCP_STATE_ESTABLISHED,.accepted=true,.outbound=true,
        .owner=caller,.id=42,.refs=1,.local_seq=1000,.remote_next=2000,
        .local_port=50000,.remote_port=80,.remote_ip=0x0a000202,.peer_window=65535};
    memcpy(c->remote_mac,router_mac,6);return c;
}
/* Independent wire checksum verifier; never calls the production checksum. */
static uint16_t wire_sum(const uint8_t *bytes,unsigned n,uint32_t sum) {
    for(unsigned i=0;i<n;i+=2)sum+=(uint32_t)bytes[i]*256+(i+1<n?bytes[i+1]:0);
    while(sum>65535)sum=(sum&65535)+(sum>>16);
    return (uint16_t)~sum;
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)(256u*p[0]+p[1]); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)u16(p)*65536+u16(p+2); }
static void wire(uint32_t remote,uint16_t port,uint32_t seq,uint8_t flags,const char *data,unsigned length) {
    unsigned header=(flags&TCP_SYN)?24:20;
    assert(attempts==1 && accepted==1 && last_length==14+20+header+length);
    assert(!memcmp(last_frame,router_mac,6) && !memcmp(last_frame+6,station,6) && u16(last_frame+12)==0x800);
    const uint8_t *ip=last_frame+14,*tcp=ip+20;
    assert(ip[0]==0x45 && ip[9]==6 && u16(ip+2)==20+header+length && wire_sum(ip,20,0)==0);
    assert(u32(ip+12)==0x0a00020f && u32(ip+16)==remote);
    assert(u16(tcp+2)==port && u32(tcp+4)==seq && tcp[12]==header/4*16 && tcp[13]==flags);
    uint32_t pseudo=u16(ip+12)+u16(ip+14)+u16(ip+16)+u16(ip+18)+6+header+length;
    assert(wire_sum(tcp,header+length,pseudo)==0);
    if(flags&TCP_SYN)assert(tcp[20]==2 && tcp[21]==4 && u16(tcp+22)==1400);
    if(length)assert(!memcmp(tcp+header,data,length));
}
static void cache_miss(void) {
    reset();memcpy(before_connections,tcp_connections,sizeof tcp_connections);
    struct arp_cache_entry before_arp[ARP_CACHE_ENTRIES];memcpy(before_arp,arp_cache,sizeof arp_cache);
    assert(net_connect_try(0x0a000202,80)==SRV_ERR_AGAIN);
    assert(!attempts && !accepted && next_client_port==TCP_CLIENT_PORT_FIRST && next_connection_id==1 && ip_ident==1);
    assert(!memcmp(before_connections,tcp_connections,sizeof tcp_connections) && !memcmp(before_arp,arp_cache,sizeof arp_cache));
    assert(process_checks==1 && exit_checks==1);
    /* An off-subnet destination cache entry cannot replace the route's MAC. */
    arp_cache_update(0xc0000214,router_mac);memcpy(before_arp,arp_cache,sizeof arp_cache);
    assert(net_connect_try(0xc0000214,80)==SRV_ERR_AGAIN && !attempts);
    assert(next_client_port==TCP_CLIENT_PORT_FIRST && next_connection_id==1 && ip_ident==1 &&
        !memcmp(before_arp,arp_cache,sizeof arp_cache) && !memcmp(before_connections,tcp_connections,sizeof tcp_connections));
}
static void connect_inputs(void) {
    for(unsigned kind=0;kind<6;kind++) {
        reset();arp_cache_update(0x0a000202,router_mac);uint32_t ip=0x0a000202;uint16_t port=80;
        if(kind==0)caller=NULL;
        if(kind==1)initialized=false;
        if(kind==2)local_ip=0;
        if(kind==3)local_mac=NULL;
        if(kind==4)ip=0;
        if(kind==5)port=0;
        assert(net_connect_try(ip,port)<0 && !attempts && next_connection_id==1 && next_client_port==TCP_CLIENT_PORT_FIRST);
        for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++)assert(!tcp_connections[i].state && !tcp_connections[i].refs);
    }
    reset();exiting=true;arp_cache_update(0x0a000202,router_mac);
    assert(net_connect_try(0x0a000202,80)<0 && !attempts && next_client_port==TCP_CLIENT_PORT_FIRST);
}
static void connect_cached(void) {
    for(unsigned routed=0;routed<2;routed++) {
        reset();uint32_t remote=routed?0xc0000214:0x0a000202;
        if(routed) { const uint8_t router[4]={10,0,2,99};memcpy(dhcp_router_ip_bytes,router,4); }
        arp_cache_update(routed?0x0a000263:remote,router_mac);
        int64_t h=net_connect_try(remote,80);assert(h>0);
        struct tcp_connection *c=find_referenced_connection_by_id(handle_value((uint64_t)h));assert(c);
        assert(c->state==TCP_STATE_SYN_SENT && c->owner==caller && c->refs==1 && c->outbound && c->accepted);
        assert(c->local_port==TCP_CLIENT_PORT_FIRST && c->remote_ip==remote && c->remote_port==80);
        uint32_t initial=0x63720000u+TCP_CLIENT_PORT_FIRST;assert(c->local_seq==initial+1);
        assert(c->tx_history[0].used && c->tx_history[0].flags==TCP_SYN && c->tx_history[0].seq==initial);
        assert(process_checks==2 && exit_checks==1);wire(remote,80,initial,TCP_SYN,NULL,0);
        assert(u16(last_frame+34)==TCP_CLIENT_PORT_FIRST);
        assert(net_respond_try_checked((uint64_t)h,"x",1,&transfer)<0 && attempts==1);
    }
    reset();refuse_output=true;arp_cache_update(0x0a000202,router_mac);
    assert(net_connect_try(0x0a000202,80)==-1 && attempts==1 && !accepted);
    for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++)assert(!tcp_connections[i].state && !tcp_connections[i].refs && !tcp_connections[i].tx_history[0].used);
    reset();arp_cache_update(0x0a000202,router_mac);
    for(unsigned i=0;i<TCP_MAX_CONNECTIONS;i++)tcp_connections[i]=(struct tcp_connection){.state=TCP_STATE_ESTABLISHED,.refs=1,.id=100+i};
    memcpy(before_connections,tcp_connections,sizeof tcp_connections);
    assert(net_connect_try(0x0a000202,80)<0 && !attempts && !memcmp(before_connections,tcp_connections,sizeof tcp_connections));
}
static void payloads(void) {
    static char data[1600];for(unsigned i=0;i<sizeof data;i++)data[i]=(char)(i*17+3);
    const unsigned limits[]={0,536,1400,2000};
    for(unsigned i=0;i<4;i++) {
        struct tcp_connection *c=connected();c->peer_mss=limits[i];unsigned size=i<2?536:1400;
        assert(net_respond_try_checked(handle(),data,UINT64_MAX,&transfer)==(int64_t)size);
        assert(identity_checks==1 && exit_checks==1 && c->local_seq==1000+size);
        assert(c->tx_history[0].used && c->tx_history[0].payload_length==size && !memcmp(c->tx_history[0].payload,data,size));
        wire(c->remote_ip,80,1000,TCP_ACK|TCP_PSH,data,size);assert(u32(last_frame+42)==2000);
    }
    struct tcp_connection *c=connected();c->peer_window=7;
    assert(net_respond_try_checked(handle(),data,20,&transfer)==7 && c->local_seq==1007);
    wire(c->remote_ip,80,1000,TCP_ACK|TCP_PSH,data,7);
    c=connected();c->tx_history[0]=(struct tcp_tx_record){.used=true,.flags=TCP_ACK,.payload_length=20};c->peer_window=27;
    assert(net_respond_try_checked(handle(),data,20,NULL)==7 && c->tx_history[1].payload_length==7);
    c=connected();c->state=TCP_STATE_CLOSE_WAIT;c->peer_closed=true;
    assert(net_respond_try_checked(handle(),data,3,&transfer)==3);
    wire(c->remote_ip,80,1000,TCP_ACK|TCP_PSH,data,3);
}
static void no_credit(void) {
    for(unsigned kind=0;kind<3;kind++) {
        struct tcp_connection *c=connected();
        if(kind==0)c->peer_window=0;
        if(kind==1)for(unsigned i=0;i<TCP_TX_HISTORY;i++)c->tx_history[i]=(struct tcp_tx_record){.used=true,.payload_length=1};
        if(kind==2)for(unsigned i=0;i<4;i++)c->tx_history[i]=(struct tcp_tx_record){.used=true,.payload_length=1400};
        memcpy(before_connections,tcp_connections,sizeof tcp_connections);
        assert(net_respond_try_checked(handle(),"x",1,&transfer)==SRV_ERR_AGAIN && !attempts && identity_checks==1);
        assert(!memcmp(before_connections,tcp_connections,sizeof tcp_connections));
    }
}
static void invalidate(void) { identity_valid=false; }
static void close_from_identity(void) { close_connection(&tcp_connections[0]); }
static void replace_from_identity(void) { tcp_connections[0].id=43; }
static void callback_and_refusals(void) {
    struct tcp_connection *c=connected();c->refs=2;identity_valid=false;
    assert(net_respond_try_checked(handle(),"x",1,&transfer)==SRV_ERR_BADF && !attempts && c->refs==2);
    connected();identity_hook=close_from_identity;
    assert(net_respond_try_checked(handle(),"x",1,&transfer)<0 && !attempts && !tcp_connections[0].state);
    connected();identity_hook=replace_from_identity;
    assert(net_respond_try_checked(handle(),"x",1,&transfer)<0 && !attempts && tcp_connections[0].id==43);
    c=connected();caller=(struct process *)&child_token;c->refs=2;
    assert(net_respond_try_checked(handle(),"shared",6,&transfer)==6 && c->owner!=(struct process *)&child_token && c->refs==2);
    /* The provider invalidates an FD token, not the TCP table: arbitrary TCP
     * reentrancy is outside the same contract as the ordinary output path. */
    c=connected();output_hook=invalidate;
    assert(net_respond_try_checked(handle(),"admitted",8,&transfer)==8 && !identity_valid && identity_checks==1);
    assert(c->local_seq==1008 && c->tx_history[0].used && c->tx_history[0].payload_length==8);
    assert(!memcmp(c->tx_history[0].payload,"admitted",8));
    assert(net_respond_try_checked(handle(),"again",5,&transfer)==SRV_ERR_BADF && attempts==1);
    c=connected();refuse_output=true;memcpy(before_connections,tcp_connections,sizeof tcp_connections);
    assert(net_respond_try_checked(handle(),"x",1,&transfer)==-1 && attempts==1 && !accepted);
    assert(!memcmp(before_connections,tcp_connections,sizeof tcp_connections));
    for(unsigned kind=0;kind<6;kind++) {
        c=connected();
        if(kind==0)c->local_write_closed=true;
        if(kind==1)c->reset=true;
        if(kind==2)c->state=TCP_STATE_CLOSED;
        if(kind==3)c->refs=0;
        if(kind==4)c->state=TCP_STATE_SYN_SENT;
        if(kind==5)exiting=true;
        assert(net_respond_try_checked(handle(),"x",1,&transfer)<0 && !attempts);
    }
    connected();identity_valid=false;
    assert(net_respond_try_checked(handle(),NULL,0,&transfer)==0 && !identity_checks && !attempts);
    assert(net_respond_try_checked(handle(),NULL,1,&transfer)<0 && !identity_checks && !attempts);
    assert(net_respond_try_checked(make_handle(NET_HANDLE_LISTENER,42),"x",1,&transfer)<0 && !identity_checks);
    assert(net_respond_try_checked(make_handle(NET_HANDLE_CONNECTION,43),"x",1,&transfer)<0 && !identity_checks);
    initialized=false;assert(net_respond_try_checked(handle(),"x",1,&transfer)<0 && !identity_checks && !attempts);
}
int main(void) {
    cache_miss();connect_inputs();connect_cached();payloads();no_credit();callback_and_refusals();reset();
    puts("net-tcp-try: PASS actual finite cache-only connect and one-segment checked send; route/MSS/window/history/refusal/callback/shared-FD wire checks; no polls/waits/yields/delays; HOST process stubs only, no native acceptance");
    return 0;
}
