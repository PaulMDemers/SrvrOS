/* Execute the production syscall dispatcher helper; transport stubs assert
 * nonblocking dispatch and no mutation of the shared file status flags. */
#include <assert.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static struct process_file file;
static unsigned calls;
static uint64_t bad_address;
static bool expected_writable;
struct process *process_current(void) { return (struct process *)1; }
struct process_file *process_file_at(struct process *p,uint64_t fd) { assert(p==(struct process *)1);return fd==7?&file:NULL; }
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(length>0);if(address==bad_address)return false;
    if(length==3)assert(writable==expected_writable);
    return true;
}
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
int64_t process_file_pipe_read_mode(struct process *p,uint64_t fd,uint8_t *b,uint64_t n,bool nonblock) {
    (void)b;assert(p==(struct process *)1 && fd==7);return invoked(42,n,nonblock);
}
int64_t process_file_pipe_write_mode(struct process *p,uint64_t fd,const uint8_t *b,uint64_t n,bool nonblock) {
    (void)b;assert(p==(struct process *)1 && fd==7);return invoked(42,n,nonblock);
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
    assert(syscall_socket_try_io(7,6,buffer,3,&peer)==SRV_ERR_INVAL);
    assert(syscall_socket_try_io(7,5,NULL,3,&peer)==SRV_ERR_FAULT);
    assert(syscall_socket_try_io(7,5,buffer,3,NULL)==SRV_ERR_FAULT);
    peer.reserved=1;assert(syscall_socket_try_io(7,5,buffer,3,&peer)==SRV_ERR_INVAL);peer.reserved=0;
    bad_address=(uint64_t)&peer;assert(syscall_socket_try_io(7,5,buffer,3,&peer)==SRV_ERR_FAULT);
    expected_writable=true;assert(syscall_socket_try_io(7,4,buffer,3,&peer)==SRV_ERR_FAULT);
    assert(calls==before);bad_address=0;peer.ip=0;peer.port=0;
    assert(syscall_socket_try_io(7,4,buffer,3,&peer)==3 && peer.ip==0x01020304 && peer.port==53 && !peer.reserved);
    puts("socket-try-io: PASS types, operations, user access, peer copy, flags and nonblocking dispatch");
}
