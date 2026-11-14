/* Actual isolated init body + SDK wrappers, HOST raw-call contract only. */
#include <srvros/sys.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../userspace/init/network_probe.h"
static jmp_buf stop;
static unsigned ready,waits,sleeps;static int fail_ready,flood;
long srv_syscall0(long op) {assert(op==SYS_PROC_READY && !ready++);return fail_ready?-1:0;}
long srv_syscall1(long op,long ticks) {
    assert(op==SYS_SLEEP_TICKS && ticks==100 && ready==1);
    assert(waits==(flood?64u:1u)*(sleeps+1));
    if(++sleeps==2)longjmp(stop,1);
    return 0;
}
long srv_syscall3(long op,long pid,long status,long flags) {
    assert(op==SYS_WAIT && pid==0 && status && flags==SRV_WAIT_NOHANG && ready==1);
    waits++;*(uint64_t *)status=0;return flood?7:0;
}
int main(void) {
    fail_ready=1;assert(init_network_probe_main()==1 && ready==1 && !waits && !sleeps);
    for(flood=0;flood<2;flood++) {
        fail_ready=0;ready=waits=sleeps=0;
        if(!setjmp(stop))(void)init_network_probe_main();
        assert(ready==1 && sleeps==2 && waits==(flood?128u:2u));
    }
    puts("init-network-probe: PASS READY-error, parked idle, bounded orphan flood; HOST calls, no guest acceptance");
}
