#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include <assert.h>
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
#define bind audit_bind
#define recvmsg audit_recvmsg
#define recvfrom audit_recvfrom
#ifndef SRVROS_POSIX_DATAGRAM_SOURCE
#define SRVROS_POSIX_DATAGRAM_SOURCE "../userspace/lib/src/posix_socket.c"
#endif
#include SRVROS_POSIX_DATAGRAM_SOURCE
static const char *pending;static size_t pending_length;static unsigned receives;
int __posix_signal_dispatch_pending(void) { return 0; }
int __posix_signal_dispatch_pending_restartable(void) { return 0; }
long srv_fcntl(int fd,int command,uint64_t arg) { (void)fd;(void)command;(void)arg;return 0; }
long srv_unix_bind(const char *path) { (void)path;assert(0);return -1; }
long srv_close(int fd) { (void)fd;assert(0);return -1; }
long srv_net_udp_bind(int fd, uint16_t port) { assert(fd == 7 && port == 9001);return -1; }
long srv_net_udp_recvfrom(int fd,void *out,size_t cap,uint32_t *ip,uint16_t *port) {
    assert(fd==7);if(!pending)return SRV_ERR_AGAIN;
    size_t n=cap<pending_length?cap:pending_length;if(n)memcpy(out,pending,n);
    *ip=0x0a000202;*port=8000;pending=NULL;receives++;return (long)n;
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
    assert(argc==2);sockets[0]=(struct posix_socket){.used=1,.type=SOCK_DGRAM,.listener_fd=7};
    char a[2]={0},b[3]={0};struct iovec iov[]={{a,2},{b,3}};
    struct msghdr msg={.msg_iov=iov,.msg_iovlen=2};pending="abcdefg";pending_length=7;
    if(!strcmp(argv[1],"bind-cache")) {
        sockets[0].port=9000;
        struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(9001)};
        assert(bind(POSIX_SOCKET_BASE,(struct sockaddr *)&address,sizeof(address))<0);
        assert(errno==EADDRINUSE && sockets[0].port==9000);
    } else if(!strcmp(argv[1],"truncate")) {
        assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)==5 && (msg.msg_flags&MSG_TRUNC));
        assert(!memcmp(a,"ab",2) && !memcmp(b,"cde",3) && receives==1 && !pending);
    } else if(!strcmp(argv[1],"zero")) {
        msg.msg_iov=NULL;msg.msg_iovlen=0;
        assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)==0 && receives==1 && !pending && (msg.msg_flags&MSG_TRUNC));
    } else if(!strcmp(argv[1],"empty")) {
        pending="";pending_length=0;
        assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)==0 && receives==1 && !(msg.msg_flags&MSG_TRUNC));
    } else if(!strcmp(argv[1],"address")) {
        unsigned char addr[3]={0xa5,0xa5,0xa5};msg.msg_name=addr+1;msg.msg_namelen=1;
        assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)==5 && msg.msg_namelen==sizeof(struct sockaddr_in));
        assert(addr[0]==0xa5 && addr[2]==0xa5);
    } else if(!strcmp(argv[1],"oom")) {
        fail_alloc=1;errno=0;assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)<0 && errno==ENOMEM);
        assert(!receives && pending);fail_alloc=0;
        assert(recvmsg(POSIX_SOCKET_BASE,&msg,0)==5 && receives==1);
    } else assert(0);
    puts("posix-datagram: PASS");
}
