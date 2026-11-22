/* Actual SDK wrapper bodies from sys.c; only raw int80 entry is replaced by
 * strong HOST calls. This is old/new ABI negotiation and argument evidence,
 * not kernel, real process/ISR or native networking acceptance. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <srvros/sys.h>

static long version=SRV_SOCKET_TCP_TRY_ABI_VERSION, connect_result=7, send_result=3;
static unsigned queries,connects,sends;
static uint64_t expected_flags=SRV_FD_NONBLOCK|SRV_NET_CONNECT_CACHE_ONLY;
static int expected_fd;
static void *expected_buffer;
static size_t expected_length;
long srv_syscall3(long number,long ip,long port,long flags) {
    assert(number==SYS_NET_CONNECT && (uint32_t)ip==0xc0000201 && port==80);
    assert((uint64_t)flags==expected_flags);connects++;return connect_result;
}
long srv_syscall5(long number,long fd,long operation,long buffer,long length,long peer) {
    assert(number==SYS_SOCKET_TRY_IO && operation==SRV_SOCKET_TCP_SEND_TRY && !peer);
    if(fd==-1) {
        assert(!buffer && !length);queries++;return version;
    }
    assert(fd==expected_fd && (void *)buffer==expected_buffer && (size_t)length==expected_length);
    sends++;return send_result;
}
int main(void) {
    const long unsupported[]={SRV_ERR_INVAL,SRV_ERR_NOTSUP,SRV_ERR_BADF,0,2};
    for(unsigned i=0;i<sizeof unsupported/sizeof unsupported[0];i++) {
        version=unsupported[i];unsigned before=connects;
        assert(srv_net_connect_try(0xc0000201,80)==SRV_ERR_NOTSUP && connects==before);
    }
    assert(queries==5);
    version=SRV_SOCKET_TCP_TRY_ABI_VERSION;
    const long results[]={0,7,SRV_ERR_AGAIN,SRV_ERR_BADF,-1};
    for(unsigned i=0;i<sizeof results/sizeof results[0];i++) {
        connect_result=results[i];unsigned before=connects;
        assert(srv_net_connect_try(0xc0000201,80)==results[i] && connects==before+1);
    }
    assert(queries==10);
    /* Raw old API is preserved and does not silently gain negotiation. */
    expected_flags=0;connect_result=11;
    assert(srv_net_connect(0xc0000201,80,0)==11 && queries==10);
    char data[8]={0};expected_fd=3;expected_buffer=data;expected_length=sizeof data;
    const long sent[]={3,SRV_ERR_AGAIN,SRV_ERR_INVAL,-1};
    for(unsigned i=0;i<sizeof sent/sizeof sent[0];i++) {
        send_result=sent[i];assert(srv_net_respond_try(3,data,sizeof data)==sent[i]);
    }
    expected_buffer=NULL;expected_length=0;send_result=0;
    assert(srv_net_respond_try(3,NULL,0)==0 && sends==5);
    expected_buffer=data;expected_length=SIZE_MAX;send_result=1400;
    assert(srv_net_respond_try(3,data,SIZE_MAX)==1400 && sends==6);
    puts("net-tcp-frontend-sdk: PASS actual wrappers and explicit version refusal before old-kernel legacy connect; HOST raw-call model only");
    return 0;
}
