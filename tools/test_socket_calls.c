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
#include <stdlib.h>
static int fail_alloc;
static void *test_malloc(size_t n) { return fail_alloc?NULL:malloc(n); }
#define malloc test_malloc
#define close audit_close
#define poll audit_poll
#define connect audit_connect
#define accept audit_accept
#define read audit_read
#define write audit_write
#define send audit_send
#define socket audit_socket
#define recv audit_recv
#define bind audit_bind
#define recvmsg audit_recvmsg
#define recvfrom audit_recvfrom
#ifndef SRVROS_SOCKET_CALL_SOURCE
#define SRVROS_SOCKET_CALL_SOURCE "../userspace/lib/src/posix_socket.c"
#endif
#include "posix_socket_host_names.h"
#include SRVROS_SOCKET_CALL_SOURCE
static unsigned connects,accepts,closed_result,flag_writes,tries;
static uint64_t flags_value;
static int retire,restart,do_accept,raw_mode,dup_fail;
static unsigned held_closed,signal_dispatches;
static long try_result=SRV_ERR_AGAIN;
static const char *dns_mode;
static bool unix_control;
static unsigned dns_sends,dns_receives;
static unsigned char saved_query[512];static size_t query_length;
int audit_close(int fd) { return __posix_socket_close(fd); }
int audit_poll(struct pollfd *fds,nfds_t count,int timeout) { (void)fds;assert(count==1 && timeout==2000);return strcmp(dns_mode,"dns-busy")!=0; }
long srv_net_status_info(struct srv_net_status_info *info) { memset(info,0,sizeof(*info));info->dns_ip=0x0a000203;return 0; }
long srv_net_dns(const char *name,uint32_t *out) { (void)out;assert(!strcmp(name,"outer.test") && !strcmp(dns_mode,"dns-busy"));return SRV_ERR_AGAIN; }
long srv_pipe_recv_rights(int fd,int *fds,size_t capacity,uint64_t *flags) { (void)fd;(void)fds;(void)capacity;*flags=0;return 0; }

static void replace(void) {
    assert(!__posix_socket_close(POSIX_SOCKET_BASE));
    assert(socket(AF_INET,SOCK_STREAM,0)==POSIX_SOCKET_BASE);
}
int __posix_signal_dispatch_pending(void) { signal_dispatches++;errno=EDOM;return 1; }
int __posix_signal_dispatch_pending_restartable(void) { if(restart) { restart=0;replace(); }return 1; }
long srv_net_udp_open(void) { return 7; }
long srv_close(int fd) { if(fd==8)closed_result++;if(fd==9)held_closed++;return 0; }
long srv_fcntl(int fd,int cmd,uint64_t arg) { (void)fd;if(cmd==SRV_F_DUPFD_CLOEXEC)return dup_fail?dup_fail:9;if(cmd==SRV_F_SETFL) { flags_value=arg;flag_writes++; }return cmd==SRV_F_GETFL?(long)flags_value:0; }
long srv_net_connect(uint32_t ip,uint16_t port,uint64_t flags) {
    (void)flags;assert(ip==0x0a000202 && port==8000);connects++;
    if(restart)return SRV_ERR_INTR;
    if(retire)replace();
    return 8;
}
long srv_net_accept(int fd,char *buffer,size_t cap,uint64_t *length) {
    if(raw_mode) {
        assert(fd==9);accepts++;*length=0;
        if(accepts==1) { real_options_for_fd(7,0)->so_keepalive=0;return SRV_ERR_INTR; }
        return 8;
    }
    assert(fd==7);(void)buffer;(void)cap;*length=0;accepts++;
    if(restart)return SRV_ERR_INTR;
    if(retire)replace();
    return 8;
}
long srv_unix_connect(const char *p) {
    if(!unix_control)return -1;
    assert(strlen(p)==SRV_UNIX_PATH_MAX-1);return 8;
}
int __posix_event_export_allowed(int fd) { assert(fd==3 || fd==4);return 1; }
long srv_unix_accept(int fd) { (void)fd;return -1; }
long srv_net_sockname(int fd,uint32_t *ip,uint16_t *port) { (void)fd;*ip=0x0a00020f;*port=9000;return 0; }
long srv_net_peername(int fd,uint32_t *ip,uint16_t *port) { (void)fd;*ip=0x0a000202;*port=8000;return 0; }
long srv_net_error(int fd) { (void)fd;return 0; }
ssize_t audit_read(int fd,void *buf,size_t n) { (void)fd;(void)buf;(void)n;assert(flags_value==0);errno=EAGAIN;return -1; }
ssize_t audit_write(int fd,const void *buf,size_t n) { (void)fd;(void)buf;(void)n;assert(flags_value==0);errno=EAGAIN;return -1; }
long srv_net_peek(int fd,void *buf,size_t n) { (void)fd;(void)buf;(void)n;assert(flags_value==0);return SRV_ERR_AGAIN; }
long srv_net_udp_recvfrom(int fd,void *buf,size_t n,uint32_t *ip,uint16_t *port) {
    (void)fd;assert(flags_value==0);if(!dns_mode)return SRV_ERR_AGAIN;
    unsigned char response[513]={0};memcpy(response,saved_query,query_length);response[2]=0x81;response[3]=0x80;response[7]=1;
    const unsigned char answer[]={0xc0,12,0,1,0,1,0,0,0,0,0,4,1,2,3,4};memcpy(response+query_length,answer,sizeof(answer));
    size_t length=query_length+sizeof(answer);*ip=0x0a000203;*port=53;dns_receives++;
    if(dns_receives==1) {
        if(!strcmp(dns_mode,"dns-retry"))response[0]^=1;
        if(!strcmp(dns_mode,"dns-truncate"))length=513;
        if(!strcmp(dns_mode,"dns-source")) { *ip=0x0a000204;response[query_length+15]=9; }
    }
    if(n>length)n=length;
    memcpy(buf,response,n);return (long)n;
}
long srv_net_udp_sendto(int fd,uint32_t ip,uint16_t port,const void *buf,size_t n) {
    (void)fd;if(dns_mode) {
        assert(ip==0x0a000203 && port==53 && n<=sizeof(saved_query));
        if(!dns_sends) { memcpy(saved_query,buf,n);query_length=n; }
        else assert(n==query_length && !memcmp(saved_query,buf,n));
        dns_sends++;
    }return (long)n;
}
#ifdef SRV_SOCKET_RECV
long srv_socket_try_io(int fd,uint64_t operation,void *buf,size_t n,struct srv_socket_peer *peer) {
    assert(flags_value==0);tries++;
    if(dns_mode) { assert(operation==SRV_SOCKET_UDP_RECV);return srv_net_udp_recvfrom(fd,buf,n,&peer->ip,&peer->port); }
    return try_result;
}
#endif
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"unix-copy")) {
        unix_control=true;assert(socket(AF_UNIX,SOCK_STREAM,0)==POSIX_SOCKET_BASE);
        struct sockaddr_un peer={.sun_family=AF_UNIX};
        memset(peer.sun_path,'p',SRV_UNIX_PATH_MAX-1);peer.sun_path[SRV_UNIX_PATH_MAX-1]=0;
        assert(connect(POSIX_SOCKET_BASE,(struct sockaddr *)&peer,sizeof(peer))==0);
        assert(!memcmp(sockets[0].unix_path,peer.sun_path,SRV_UNIX_PATH_MAX));
        puts("socket-calls: unix-copy PASS");return 0;
    }
    if(!strcmp(argv[1],"rights-const")) {
        union { struct cmsghdr align; unsigned char bytes[2*CMSG_SPACE(sizeof(int))]; } control={0};
        struct msghdr message={.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
        struct cmsghdr *c=CMSG_FIRSTHDR(&message);
        for(int fd=3;fd<=4;fd++) {
            assert(c);c->cmsg_len=CMSG_LEN(sizeof(int));c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;
            memcpy(CMSG_DATA(c),&fd,sizeof(fd));c=CMSG_NXTHDR(&message,c);
        }
        unsigned char before[sizeof(control.bytes)];memcpy(before,control.bytes,sizeof(before));
        int rights[SRV_RIGHTS_MAX];size_t count=0;
        assert(!message_extract_rights(&message,rights,&count) && count==2 && rights[0]==3 && rights[1]==4);
        assert(message.msg_control==control.bytes && message.msg_controllen==sizeof(control.bytes));
        assert(!memcmp(before,control.bytes,sizeof(before)));
        CMSG_FIRSTHDR(&message)->cmsg_len=CMSG_LEN(0)-1;
        assert(message_extract_rights(&message,rights,&count)<0 && errno==EINVAL && !count);
        puts("socket-calls: rights-const PASS");return 0;
    }
    if(!strncmp(argv[1],"dns-",4)) {
        dns_mode=argv[1];struct addrinfo *info=NULL;int result=getaddrinfo("outer.test",NULL,NULL,&info);
        if(!strcmp(dns_mode,"dns-busy"))assert(result==EAI_AGAIN && !info && dns_sends==3);
        else { assert(result==0 && info && ntohl(((struct sockaddr_in *)info->ai_addr)->sin_addr.s_addr)==0x01020304);assert(dns_receives==2);assert(dns_sends==(!strcmp(dns_mode,"dns-source")?1u:2u));freeaddrinfo(info); }
        printf("socket-calls: %s PASS\n",argv[1]);return 0;
    }
    int datagram=strstr(argv[1],"udp")!=NULL;
    assert(socket(AF_INET,datagram?SOCK_DGRAM:SOCK_STREAM,0)==POSIX_SOCKET_BASE);
    struct sockaddr_in peer={.sin_family=AF_INET,.sin_port=htons(8000),.sin_addr.s_addr=htonl(0x0a000202)};
    if(!strncmp(argv[1],"raw-",4)) {
        raw_mode=1;dup_fail=strstr(argv[1],"full")?SRV_ERR_MFILE:0;
        real_options_for_fd(7,1)->so_keepalive=1;
        int result=accept(7,NULL,NULL);
        if(dup_fail) {
            assert(result<0 && errno==EMFILE && !accepts && !held_closed);
            dup_fail=SRV_ERR_BADF;assert(accept(7,NULL,NULL)<0 && errno==EBADF);
            dup_fail=SRV_ERR_NOMEM;assert(accept(7,NULL,NULL)<0 && errno==ENOMEM);
            assert(!accepts && !held_closed);
        }
        else assert(result==8 && accepts==2 && held_closed==1 && real_options_for_fd(8,0)->so_keepalive==1);
    } else if(!strncmp(argv[1],"flags-",6)) {
        sockets[0].listener_fd=7;sockets[0].connected=1;sockets[0].peer_ip=0x0a000202;sockets[0].peer_port=8000;
        char byte=0;ssize_t result;
        if(strstr(argv[1],"send"))result=send(POSIX_SOCKET_BASE,&byte,1,MSG_DONTWAIT);
        else result=recv(POSIX_SOCKET_BASE,&byte,1,MSG_DONTWAIT|(strstr(argv[1],"peek")?MSG_PEEK:0));
        assert(result<0 && errno==EAGAIN && !flag_writes && tries==1 && signal_dispatches==1);
        if(!datagram && !strstr(argv[1],"peek")) {
            try_result=1;errno=0;
            result=strstr(argv[1],"send")?send(POSIX_SOCKET_BASE,&byte,1,MSG_DONTWAIT):recv(POSIX_SOCKET_BASE,&byte,1,MSG_DONTWAIT);
            assert(result==1 && signal_dispatches==2 && errno==0 && !flag_writes);
            if(strstr(argv[1],"send")) {
                try_result=SRV_ERR_PIPE;
                assert(send(POSIX_SOCKET_BASE,&byte,1,MSG_DONTWAIT)==-1 && errno==EPIPE && signal_dispatches==3 && !flag_writes);
            }
        }
    } else {
        do_accept=strstr(argv[1],"accept")!=NULL;
        retire=strstr(argv[1],"reuse")!=NULL;restart=strstr(argv[1],"restart")!=NULL;
        int fail=retire || restart;
        if(do_accept) { sockets[0].listener_fd=7;sockets[0].listening=1; }
        int result=do_accept?accept(POSIX_SOCKET_BASE,NULL,NULL):connect(POSIX_SOCKET_BASE,(struct sockaddr *)&peer,sizeof(peer));
        if(fail) { assert(result==-1 && errno==EBADF && sockets[0].listener_fd==-1 && !sockets[0].connected);assert(closed_result==(retire?1u:0u)); }
        else assert(result==(do_accept?8:0));
        assert((do_accept?accepts:connects)==1);
    }
    printf("socket-calls: %s PASS\n",argv[1]);return 0;
}
