#ifdef TEST_DNS_LIBC
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <limits.h>
#undef htons
#undef ntohs
#undef htonl
#undef ntohl
uint16_t htons(uint16_t value);
uint16_t ntohs(uint16_t value);
uint32_t htonl(uint32_t value);
uint32_t ntohl(uint32_t value);
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif
#ifdef __APPLE__
/* Darwin msghdr uses signed msg_iovlen; the guest ABI uses size_t. */
#pragma clang diagnostic ignored "-Wsign-compare"
#endif
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0x40000000
#endif
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0x800
#define SOCK_CLOEXEC 0x80000
#endif

#ifndef DNS_SOURCE
#define DNS_SOURCE "../userspace/lib/src/posix_socket.c"
#endif
#include DNS_SOURCE
#else
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DNS_SOURCE
#define DNS_SOURCE "../kernel/src/net.c"
#endif
#include DNS_SOURCE
#endif
static void put16(unsigned char *p,unsigned n) { p[0]=n>>8;p[1]=n; }
static size_t question(unsigned char *p) {
    memset(p,0,512);put16(p,0x7372);put16(p+2,0x8180);put16(p+4,1);put16(p+6,1);
    memcpy(p+12,"\005outer\004test",12);put16(p+24,1);put16(p+26,1);return 28;
}
static size_t answer(unsigned char *p,size_t n,unsigned pointer,unsigned ip) {
    p[n++]=0xc0|(pointer>>8);p[n++]=pointer;put16(p+n,1);put16(p+n+2,1);put16(p+n+8,4);n+=10;
    p[n++]=ip>>24;p[n++]=ip>>16;p[n++]=ip>>8;p[n++]=ip;return n;
}
static int parse(const unsigned char *p,size_t n,uint32_t *ip) {
#ifdef TEST_DNS_LIBC
#ifdef SRVROS_DNS_WIRE_H
    return dns_parse_response(p,n,0x7372,"outer.test",ip)==0;
#else
    return dns_parse_response(p,n,0x7372,ip)==0;
#endif
#else
    dns_waiting=true;dns_answer_ready=false;dns_response_done=false;dns_query_id=0x7372;
#ifdef SRVROS_DNS_WIRE_H
    memcpy(dns_question_name,"outer.test",11);
#endif
    handle_dns_packet(p,(uint16_t)n);
    if(dns_answer_ready)*ip=dns_answer_ip;
    return dns_answer_ready;
#endif
}
int main(int argc,char **argv) {
    assert(argc==2);unsigned char packet[512];size_t start=question(packet),n=answer(packet,start,12,0x01020304);
    bool good=false;
    if(!strcmp(argv[1],"valid"))good=true;
    else if(!strcmp(argv[1],"case")) { packet[13]='O';packet[19]='T';good=true; }
    else if(!strcmp(argv[1],"question"))packet[13]='x';
    else if(!strcmp(argv[1],"qtype"))put16(packet+24,28);
    else if(!strcmp(argv[1],"qclass"))put16(packet+26,3);
    else if(!strcmp(argv[1],"qr"))put16(packet+2,0x0180);
    else if(!strcmp(argv[1],"opcode"))put16(packet+2,0x8980);
    else if(!strcmp(argv[1],"tc"))put16(packet+2,0x8380);
    else if(!strcmp(argv[1],"rcode"))put16(packet+2,0x8183);
    else if(!strcmp(argv[1],"self-pointer"))packet[start+1]=(unsigned char)start;
    else if(!strcmp(argv[1],"forward-pointer"))packet[start+1]=(unsigned char)(start+2);
    else if(!strcmp(argv[1],"bad-pointer")) { packet[start]=0xff;packet[start+1]=0xff; }
    else if(!strcmp(argv[1],"interior-pointer"))packet[start+1]=13;
    else if(!strcmp(argv[1],"header-pointer"))packet[start+1]=4;
    else if(!strcmp(argv[1],"unrelated-first")) { put16(packet+6,2);n=answer(packet,start,18,0x09090909);n=answer(packet,n,12,0x01020304);good=true; }
    else if(!strcmp(argv[1],"cname-cycle")) { n=start;packet[n++]=0xc0;packet[n++]=12;put16(packet+n,5);put16(packet+n+2,1);put16(packet+n+8,2);n+=10;packet[n++]=0xc0;packet[n++]=12; }
    else if(!strcmp(argv[1],"unrelated"))packet[start+1]=18; /* owner=test, question=outer.test */
    else if(!strcmp(argv[1],"trailing-answer"))put16(packet+6,2);
    else if(!strcmp(argv[1],"trailing-additional"))put16(packet+10,1);
    else if(!strcmp(argv[1],"bounds")) {
        for(size_t len=0;len<n;len++) { unsigned char *short_packet=malloc(len?len:1);assert(short_packet);memcpy(short_packet,packet,len);uint32_t ip=0xdeadbeef;assert(!parse(short_packet,len,&ip) && ip==0xdeadbeef);free(short_packet); }
        good=true;
    } else if((!strcmp(argv[1],"cname") || !strcmp(argv[1],"cname-length") || !strcmp(argv[1],"cname-conflict"))) {
        put16(packet+6,2);n=start;packet[n++]=0xc0;packet[n++]=12;put16(packet+n,5);put16(packet+n+2,1);put16(packet+n+8,8);n+=10;
        size_t alias=n;memcpy(packet+n,"\005alias\300\022",8);n+=8;n=answer(packet,n,(unsigned)alias,0x01020304);good=true;
        if(!strcmp(argv[1],"cname-length")) { put16(packet+start+10,7);good=false; }
        if(!strcmp(argv[1],"cname-conflict")) { put16(packet+6,3);n=answer(packet,n,12,0x01020304);good=false; }
    } else assert(0);
    uint32_t ip=0xdeadbeef;assert(parse(packet,n,&ip)==good);
#ifdef TEST_DNS_LIBC
    if(good)assert(ntohl(ip)==0x01020304);
#else
    if(good)assert(ip==0x01020304);
#endif
    if(!good)assert(ip==0xdeadbeef);
    printf("dns-response: %s PASS\n",argv[1]);return 0;
}
