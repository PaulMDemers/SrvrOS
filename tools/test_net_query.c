/* Deterministic overlapping callers through production ARP/DNS wait paths. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SRVROS_NET_QUERY_SOURCE
#define SRVROS_NET_QUERY_SOURCE "../kernel/src/net.c"
#endif
#include SRVROS_NET_QUERY_SOURCE
static uint64_t ticks=100;
static void (*poll_hook)(void);
static void (*wait_hook)(void);
static uint64_t reply_tick;
static bool fail_send,exiting;
static unsigned sends;
static uint8_t last_frame[1600];
uint64_t timer_ticks(void) { return ticks; }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
void console_printf(const char *fmt,...) { (void)fmt; }
bool process_should_exit_current(void) { return exiting; }
bool process_signal_pending_current(void) { return false; }
void scheduler_yield(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
void process_file_poll_wake(void) {}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t deadline) {
    (void)q;(void)fn;(void)arg;
    ticks=deadline;
    if (wait_hook && ticks == reply_tick) {
        wait_hook();
        wait_hook=NULL;
    }
    return false;
}
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    (void)budget;(void)verbose;if(poll_hook) { void (*fn)(void)=poll_hook;poll_hook=NULL;fn(); }return 0;
}
bool net_device_send(const uint8_t *frame,uint16_t length) {
    assert(length<=sizeof(last_frame));memcpy(last_frame,frame,length);sends++;return !fail_send;
}
static void setup(void) {
    static const uint8_t mac[6]={2,0,0,0,0,1};
    local_mac=mac;local_ip=0x0a00020f;initialized=worker_started=true;
}
static void arp_reply(uint32_t ip,uint8_t mac) {
    uint8_t frame[42]={0};write_be16(frame+14,1);write_be16(frame+16,ETH_TYPE_IPV4);
    frame[18]=6;frame[19]=4;write_be16(frame+20,2);memset(frame+22,mac,6);
    write_be32(frame+28,ip);write_be32(frame+38,local_ip);handle_arp(frame,sizeof(frame));
}
static void arp_inner_reply(void) { arp_reply(0x0a000203,3); }
static void arp_outer_reply(void) { arp_reply(0x0a000202,2); }
static void arp_overlap(void) {
    uint8_t inner[6]={0};poll_hook=arp_inner_reply;
    assert(resolve_ipv4_mac(0x0a000203,inner) && inner[0]==3);
    poll_hook=arp_outer_reply;
}
static void dns_reply_value(uint32_t server,uint16_t id,uint32_t answer) {
    uint8_t ip[128]={0},*udp=ip+20,*dns=udp+8;
    write_be32(ip+12,server);write_be32(ip+16,local_ip);
    write_be16(udp,DNS_PORT);write_be16(udp+2,DNS_CLIENT_PORT);
    write_be16(dns,id);write_be16(dns+2,0x8180);write_be16(dns+4,1);write_be16(dns+6,1);
    uint64_t n=12;assert(dns_encode_name(dns,&n,100,"outer.test"));write_be16(dns+n,1);write_be16(dns+n+2,1);n+=4;
    dns[n++]=0xc0;dns[n++]=12;write_be16(dns+n,1);write_be16(dns+n+2,1);write_be16(dns+n+8,4);n+=10;
    write_be32(dns+n,answer);n+=4;write_be16(udp+4,(uint16_t)n+8);
    handle_udp(ip,20,(uint16_t)n+8);
}
static void dns_reply_id(uint32_t server,uint16_t id) { dns_reply_value(server,id,0x01020304); }
static void dns_reply_from(uint32_t server) { dns_reply_id(server,dns_query_id); }
static void dns_inner_reply(void) { dns_reply_value(0x0a000203,dns_query_id,0x05060708); }
static void dns_reply(void) { dns_reply_from(0x0a000203); }
static void poll_until_reply_tick(void) {
    if (ticks == reply_tick) {
        dns_reply();
    } else {
        poll_hook=poll_until_reply_tick;
    }
}
static void dns_nested(void) {
    uint32_t answer=0xdeadbeef;poll_hook=dns_inner_reply;
    int64_t result=net_dns_resolve("other.test",&answer);
    /* The old code completes this nested call, then wrongly returns its answer
     * to the outer caller. The fixed single-owner resolver refuses overlap. */
    assert((result==0 && answer==0x05060708) || (result==SRV_ERR_AGAIN && answer==0xdeadbeef));
    poll_hook=dns_reply;
}
static void dns_nested_during_arp(void) {
    dns_nested();arp_reply(0x0a000203,3);
}
static void dns_wrong_source(void) {
    dns_reply_from(0x0a000204);assert(dns_waiting && !dns_answer_ready);poll_hook=dns_reply;
}
static void dns_completed_overlap(void) { dns_reply();dns_nested(); }
static void request_exit(void) { exiting=true; }
static void dns_wrong_id(void) {
    dns_reply_id(0x0a000203,(uint16_t)(dns_query_id-1));assert(dns_waiting && !dns_answer_ready);poll_hook=dns_reply;
}
int main(int argc,char **argv) {
    assert(argc==2);setup();uint8_t mac[6]={0xa5};uint32_t answer=0xdeadbeef;
    if(!strcmp(argv[1],"arp-overlap")) {
        poll_hook=arp_overlap;assert(resolve_ipv4_mac(0x0a000202,mac) && mac[0]==2);
    } else if(!strcmp(argv[1],"arp-timeout")) {
        assert(!resolve_ipv4_mac(0x0a000202,mac) && mac[0]==0xa5 && ticks==200);
        poll_hook=arp_outer_reply;assert(resolve_ipv4_mac(0x0a000202,mac) && mac[0]==2);
    } else if(!strcmp(argv[1],"arp-exit")) {
        poll_hook=request_exit;assert(!resolve_ipv4_mac(0x0a000202,mac) && mac[0]==0xa5 && ticks<=101);
        exiting=false;poll_hook=arp_outer_reply;assert(resolve_ipv4_mac(0x0a000202,mac) && mac[0]==2);
    } else if(!strcmp(argv[1],"arp-opcode")) {
        uint8_t frame[42]={0};write_be16(frame+14,1);write_be16(frame+16,ETH_TYPE_IPV4);
        frame[18]=6;frame[19]=4;write_be16(frame+20,3);memset(frame+22,3,6);write_be32(frame+28,0x0a000202);
        handle_arp(frame,sizeof(frame));assert(!arp_cache_lookup(0x0a000202,mac));
    } else if(!strcmp(argv[1],"arp-cache")) {
        arp_reply(0x0a000202,2);assert(resolve_ipv4_mac(0x0a000202,mac) && mac[0]==2 && sends==0);
    } else {
        if(strcmp(argv[1],"dns-arp-overlap"))arp_reply(0x0a000203,3);
        if(!strcmp(argv[1],"dns-overlap"))poll_hook=dns_nested;
        else if(!strcmp(argv[1],"dns-arp-overlap"))poll_hook=dns_nested_during_arp;
        else if(!strcmp(argv[1],"dns-last-poll")) {
            reply_tick=ticks+DNS_TIMEOUT_TICKS-1;
            poll_hook=poll_until_reply_tick;
        } else if(!strcmp(argv[1],"dns-deadline-wake")) {
            reply_tick=ticks+DNS_TIMEOUT_TICKS;
            wait_hook=dns_reply;
        }
        else if(!strcmp(argv[1],"dns-source"))poll_hook=dns_wrong_source;
        else if(!strcmp(argv[1],"dns-id"))poll_hook=dns_wrong_id;
        else if(!strcmp(argv[1],"dns-completed-overlap"))poll_hook=dns_completed_overlap;
        else if(!strcmp(argv[1],"dns-exit")) {
            poll_hook=request_exit;assert(net_dns_resolve("outer.test",&answer)<0 && answer==0xdeadbeef && ticks<=101);
            exiting=false;poll_hook=dns_reply;
        }
        else if(!strcmp(argv[1],"dns-failure-retry")) {
            fail_send=true;assert(net_dns_resolve("outer.test",&answer)<0 && answer==0xdeadbeef);fail_send=false;
            assert(net_dns_resolve(".",&answer)<0 && answer==0xdeadbeef);poll_hook=dns_reply;
        } else if(!strcmp(argv[1],"dns-timeout")) {
            uint64_t start=ticks;
            assert(net_dns_resolve("outer.test",&answer)<0 && answer==0xdeadbeef);
            assert(sends==DNS_RETRIES && ticks-start==DNS_RETRIES*DNS_TIMEOUT_TICKS);
            poll_hook=dns_reply;
        } else assert(0);
        assert(!net_dns_resolve("outer.test",&answer) && answer==0x01020304);
        if (reply_tick) assert(sends==1 && !dns_query_active);
    }
    printf("net-query: %s PASS\n",argv[1]);return 0;
}
