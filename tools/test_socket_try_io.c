/* Execute the production syscall dispatcher helper; transport stubs assert
 * nonblocking dispatch and no mutation of the shared file status flags. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct process_file file;
static unsigned calls;
static uint64_t bad_address;
static bool expected_writable;
static uint64_t test_generation=1;
static uint64_t revoke_at_transport;
static bool replace_at_transport;
static uint8_t *mutate_source;
struct process *process_current(void) { return (struct process *)1; }
struct process_file *process_file_at(struct process *p,uint64_t fd) { assert(p==(struct process *)1);return fd==7?&file:NULL; }
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(length>0);if(address==bad_address)return false;
    if(length==3)assert(writable==expected_writable);
    return true;
}
uint64_t process_fd_generation(struct process *p,uint64_t fd) { assert(p==(struct process *)1 && fd==7);return test_generation; }
int64_t process_handle_alloc(struct process *p,enum process_file_type type,uint64_t h) { (void)p;(void)type;(void)h;assert(0);return -1; }
int64_t net_close(uint64_t h) { (void)h;assert(0);return -1; }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
static int64_t invoked(uint64_t h,uint64_t length,bool nonblock) {
    assert(h==42 && length==3 && nonblock && file.flags==SRV_FD_APPEND);calls++;return 3;
}
int64_t net_read(uint64_t h,char *b,uint64_t n,bool nonblock) { (void)b;return invoked(h,n,nonblock); }
int64_t net_peek(uint64_t h,char *b,uint64_t n,bool nonblock) { (void)b;return invoked(h,n,nonblock); }
int64_t net_respond(uint64_t h,const char *b,uint64_t n,bool nonblock) { (void)b;return invoked(h,n,nonblock); }
int64_t net_udp_sendto_mode(uint64_t h,uint32_t ip,uint16_t port,const uint8_t *b,uint64_t n,bool nonblock) {
    (void)b;assert(ip==0x01020304 && port==53);return invoked(h,n,nonblock);
}
int64_t net_udp_recvfrom(uint64_t h,uint8_t *b,uint64_t n,uint32_t *ip,uint16_t *port,bool nonblock) {
    (void)b;*ip=0x01020304;*port=53;return invoked(h,n,nonblock);
}
int64_t net_accept_checked(uint64_t h,uint64_t n,bool nonblock,const struct net_transfer *t) {
    (void)h;(void)n;(void)nonblock;(void)t;assert(0);return -1;
}
int64_t net_read_checked(uint64_t h,uint64_t n,bool nonblock,bool peek,const struct net_transfer *t) {
    (void)peek;
    if(revoke_at_transport) bad_address=revoke_at_transport;
    if(replace_at_transport) test_generation++;
    if(!t->current(t->context)) return SRV_ERR_BADF;
    int64_t result=t->copyout(t->context,(const uint8_t *)"abc",3,0,0,0);
    return result<0?result:invoked(h,n,nonblock);
}
int64_t net_udp_recv_checked(uint64_t h,uint64_t n,bool nonblock,const struct net_transfer *t) {
    if(revoke_at_transport) bad_address=revoke_at_transport;
    if(replace_at_transport) test_generation++;
    if(!t->current(t->context)) return SRV_ERR_BADF;
    int64_t result=t->copyout(t->context,(const uint8_t *)"abc",3,0,0x01020304,53);
    return result<0?result:invoked(h,n,nonblock);
}
int64_t net_respond_try_checked(uint64_t h,const char *b,uint64_t n,const struct net_transfer *t) {
    (void)h;(void)b;(void)n;(void)t;assert(0);return -1;
}
int64_t net_respond_checked(uint64_t h,const char *b,uint64_t n,bool nonblock,const struct net_transfer *t) {
    assert(t->current(t->context) && b && n==3);
    if(mutate_source) { memset(mutate_source,'z',3);assert(!memcmp(b,"abc",3)); }
    return invoked(h,n,nonblock);
}
int64_t net_udp_send_checked(uint64_t h,uint32_t ip,uint16_t port,const uint8_t *b,uint64_t n,bool nonblock,const struct net_transfer *t) {
    assert(t->current(t->context) && b && ip==0x01020304 && port==53);
    if(mutate_source) { memset(mutate_source,'z',3);assert(!memcmp(b,"abc",3)); }
    return invoked(h,n,nonblock);
}
int64_t process_file_pipe_read_checked(struct process *p,uint64_t fd,uint64_t n,bool nonblock,
    process_pipe_copyout copyout,void *context) {
    assert(copyout && context && p==(struct process *)1 && fd==7);return invoked(42,n,nonblock);
}
int64_t process_file_pipe_write_checked(struct process *p,uint64_t fd,uint64_t n,bool nonblock,
    process_pipe_copyin copyin,void *context) {
    assert(copyin && context && p==(struct process *)1 && fd==7);return invoked(42,n,nonblock);
}
int main(void) {
    uint8_t buffer[3];struct srv_socket_peer peer={.ip=0x01020304,.port=53};
    const enum process_file_type types[]={PROCESS_FILE_NET_CONNECTION,PROCESS_FILE_NET_UDP,PROCESS_FILE_PIPE_READ,PROCESS_FILE_PIPE_WRITE,PROCESS_FILE_PIPE_DUPLEX,PROCESS_FILE_VFS};
    for(unsigned t=0;t<sizeof(types)/sizeof(*types);t++)for(unsigned op=1;op<=5;op++) {
        file=(struct process_file){.type=types[t],.handle=42,.flags=SRV_FD_APPEND};
        expected_writable=op!=SRV_SOCKET_SEND && op!=SRV_SOCKET_UDP_SEND;
        bool valid=(t==0 && op<=3) || (t==1 && op>=4) || (t==2 && op==1) || (t==3 && op==2) || (t==4 && op<=2);
        unsigned before=calls;long result=syscall_socket_try_io(7,op,buffer,3,&peer);
        assert(valid?result==3:result<0);assert(calls==before+(valid?1:0));assert(file.flags==SRV_FD_APPEND);
    }
    file.type=PROCESS_FILE_NET_UDP;expected_writable=false;
    unsigned before=calls;
    assert(syscall_socket_try_io(99,5,buffer,3,&peer)==SRV_ERR_BADF);
    assert(syscall_socket_try_io(7,0,buffer,3,&peer)==SRV_ERR_INVAL);
    assert(syscall_socket_try_io(7,7,buffer,3,&peer)==SRV_ERR_INVAL);
    assert(syscall_socket_try_io(7,5,NULL,3,&peer)==SRV_ERR_FAULT);
    assert(syscall_socket_try_io(7,5,buffer,3,NULL)==SRV_ERR_FAULT);
    peer.reserved=1;assert(syscall_socket_try_io(7,5,buffer,3,&peer)==SRV_ERR_INVAL);peer.reserved=0;
    bad_address=(uint64_t)&peer;assert(syscall_socket_try_io(7,5,buffer,3,&peer)==SRV_ERR_FAULT);
    expected_writable=true;assert(syscall_socket_try_io(7,4,buffer,3,&peer)==SRV_ERR_FAULT);
    assert(calls==before);bad_address=0;peer.ip=0;peer.port=0;
    assert(syscall_socket_try_io(7,4,buffer,3,&peer)==3 && peer.ip==0x01020304 && peer.port==53 && !peer.reserved);
    assert(!memcmp(buffer,"abc",3));
    /* Mapping or descriptor identity can change after initial validation but
     * before transport commit. Neither payload nor peer may be published. */
    memset(buffer,0xa5,sizeof(buffer));memset(&peer,0xa5,sizeof(peer));
    struct srv_socket_peer sentinel=peer;
    revoke_at_transport=(uint64_t)buffer;expected_writable=true;
    assert(syscall_socket_try_io(7,SRV_SOCKET_UDP_RECV,buffer,3,&peer)==SRV_ERR_FAULT);
    assert(buffer[0]==0xa5 && buffer[1]==0xa5 && buffer[2]==0xa5 && !memcmp(&peer,&sentinel,sizeof(peer)));
    bad_address=0;revoke_at_transport=(uint64_t)&peer;
    assert(syscall_socket_try_io(7,SRV_SOCKET_UDP_RECV,buffer,3,&peer)==SRV_ERR_FAULT);
    assert(buffer[0]==0xa5 && buffer[1]==0xa5 && buffer[2]==0xa5 && !memcmp(&peer,&sentinel,sizeof(peer)));
    bad_address=0;revoke_at_transport=0;replace_at_transport=true;
    assert(syscall_socket_try_io(7,SRV_SOCKET_UDP_RECV,buffer,3,&peer)==SRV_ERR_BADF);
    assert(buffer[0]==0xa5 && buffer[1]==0xa5 && buffer[2]==0xa5 && !memcmp(&peer,&sentinel,sizeof(peer)));
    replace_at_transport=false;
    file.type=PROCESS_FILE_NET_CONNECTION;expected_writable=false;memcpy(buffer,"abc",3);mutate_source=buffer;
    assert(syscall_socket_try_io(7,SRV_SOCKET_SEND,buffer,3,NULL)==3 && !memcmp(buffer,"zzz",3));
    file.type=PROCESS_FILE_NET_UDP;peer=(struct srv_socket_peer){.ip=0x01020304,.port=53};memcpy(buffer,"abc",3);
    assert(syscall_socket_try_io(7,SRV_SOCKET_UDP_SEND,buffer,3,&peer)==3 && !memcmp(buffer,"zzz",3));
    puts("socket-try-io: PASS types, flags, nonblocking dispatch, mapping/peer/fd revalidation and send snapshots");
}
