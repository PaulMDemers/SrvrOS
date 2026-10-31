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
#define socket audit_socket
#define recv audit_recv
#define bind audit_bind
#define recvmsg audit_recvmsg
#define recvfrom audit_recvfrom
#ifndef SRVROS_POSIX_UDP_OWNER_SOURCE
#define SRVROS_POSIX_UDP_OWNER_SOURCE "../userspace/lib/src/posix_socket.c"
#endif
#include "posix_socket_host_names.h"
#include SRVROS_POSIX_UDP_OWNER_SOURCE
static unsigned receives,flag_writes,replacement_writes;
static uint64_t flags_value;
static bool replaced,close_only,restart_close,close_on_receive,interrupt_once,empty_after_foreign;
static int receive_fd=POSIX_SOCKET_BASE;
static unsigned foreign=2;
static void replace_socket(void) {
    assert(!__posix_socket_close(receive_fd));replaced=true;flags_value=0;
    if(!close_only) {
        assert(socket(AF_INET,SOCK_DGRAM,0)==receive_fd);
        struct posix_socket *s=socket_at(receive_fd);s->connected=1;s->peer_ip=0x0a000202;s->peer_port=8000;
    }
}
int __posix_signal_dispatch_pending(void) { return 0; }
int __posix_signal_dispatch_pending_restartable(void) {
    if(restart_close) { restart_close=false;replace_socket(); }return 1;
}
long srv_net_udp_open(void) { return 7; }
long srv_net_sockname(int fd,uint32_t *ip,uint16_t *port) { (void)fd;*ip=0x0a00020f;*port=9000;return 0; }
long srv_dup(int fd) { assert(fd==7 || fd==8);return 8; }
long srv_close(int fd) { assert(fd==7 || fd==8);return 0; }
long srv_fcntl(int fd,int command,uint64_t arg) {
    assert(fd==7 || fd==8);(void)arg;
    if(command==SRV_F_SETFL) { flags_value=arg;flag_writes++;if(replaced)replacement_writes++; }
    return command==SRV_F_GETFL?(long)flags_value:0;
}
long srv_net_udp_recvfrom(int fd,void *out,size_t cap,uint32_t *ip,uint16_t *port) {
    assert(fd==7 || fd==8);receives++;assert(receives<5);
    if(interrupt_once) { interrupt_once=false;return SRV_ERR_INTR; }
    if(restart_close)return SRV_ERR_INTR;
    if(empty_after_foreign && !foreign)return SRV_ERR_AGAIN;
    if(close_on_receive) { close_on_receive=false;replace_socket(); }
    *ip=foreign==2?0x0a000204:0x0a000202;*port=foreign==1?8001:8000;
    if(cap)((char *)out)[0]=foreign?'x':'y';
    if(foreign)foreign--;
    return cap?1:0;
}
long srv_socket_try_io(int fd,uint64_t operation,void *buffer,size_t length,struct srv_socket_peer *peer) {
    assert(operation==SRV_SOCKET_UDP_RECV);
    return srv_net_udp_recvfrom(fd,buffer,length,&peer->ip,&peer->port);
}
long srv_pipe_recv_rights(int fd,int *fds,size_t capacity,uint64_t *flags) {
    (void)fd;(void)fds;(void)capacity;*flags=0;return 0;
}
long srv_net_error(int fd) { (void)fd;assert(0);return -1; }
long srv_net_peek(int fd,void *buffer,size_t length) { (void)fd;(void)buffer;(void)length;assert(0);return -1; }
int main(int argc,char **argv) {
    assert(argc==2);assert(socket(AF_INET,SOCK_DGRAM,0)==POSIX_SOCKET_BASE);
    sockets[0].connected=1;sockets[0].peer_ip=0x0a000202;sockets[0].peer_port=8000;
    char byte=0;struct sockaddr_in peer; socklen_t len=sizeof(peer);
    if(!strcmp(argv[1],"filter-from")) {
        assert(recvfrom(POSIX_SOCKET_BASE,&byte,1,0,(struct sockaddr *)&peer,&len)==1);
        assert(byte=='y' && receives==3 && ntohs(peer.sin_port)==8000);
    } else if(!strcmp(argv[1],"filter-msg") || !strcmp(argv[1],"filter-zero")) {
        struct iovec iov={&byte,1};struct msghdr msg={.msg_iov=&iov,.msg_iovlen=1};
        if(!strcmp(argv[1],"filter-zero"))msg.msg_iovlen=0;
        assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)==(msg.msg_iovlen?1:0));
        assert(receives==3 && (msg.msg_iovlen?byte=='y':!!(msg.msg_flags&MSG_TRUNC)));
    } else if(!strcmp(argv[1],"filter-recv")) {
        assert(recv(POSIX_SOCKET_BASE,&byte,1,0)==1 && receives==3 && byte=='y');
    } else if(!strcmp(argv[1],"retry-control")) {
        interrupt_once=true;assert(recv(POSIX_SOCKET_BASE,&byte,1,0)==1 && receives==4 && byte=='y');
    } else if(!strcmp(argv[1],"dontwait-control")) {
        empty_after_foreign=true;
        assert(recv(POSIX_SOCKET_BASE,&byte,1,MSG_DONTWAIT)<0 && errno==EAGAIN && receives==3 && flags_value==0);
    } else if(!strcmp(argv[1],"unconnected")) {
        sockets[0].connected=0;assert(recv(POSIX_SOCKET_BASE,&byte,1,0)==1 && receives==1 && byte=='x');
    } else {
        int flags=0;
        if(!strcmp(argv[1],"dup-reuse")) {
            receive_fd=__posix_socket_dup(POSIX_SOCKET_BASE);assert(receive_fd==POSIX_SOCKET_BASE+1);
        }
        if(!strcmp(argv[1],"reuse-restart"))restart_close=true;
        else {
            close_on_receive=true;
            if(!strcmp(argv[1],"reuse-flags"))flags=MSG_DONTWAIT;
            else if(!strcmp(argv[1],"close-only"))close_only=true;
            else assert(!strcmp(argv[1],"reuse-filter") || !strcmp(argv[1],"dup-reuse"));
        }
        errno=0;assert(recv(receive_fd,&byte,1,flags)==-1 && errno==EBADF);
        assert(receives==1 && !replacement_writes);
        if(flags)assert(flag_writes==0);
    }
    printf("posix-udp-owner: %s PASS\n",argv[1]);return 0;
}
