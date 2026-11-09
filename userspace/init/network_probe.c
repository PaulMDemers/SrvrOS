/* Explicit isolated kernel network owner: no init.sh, service tree, log or
 * shell. Parent-zero kernel lifetime results remain owned by their supervisor. */
#include "network_probe.h"
#include <srvros/sys.h>
int init_network_probe_main(void) {
    if(srv_syscall0(SYS_PROC_READY)!=0)return 1;
    for(;;) {
        uint64_t status=0;
        for(unsigned i=0;i<64 && srv_wait(0,&status,SRV_WAIT_NOHANG)>0;i++) {}
        (void)srv_sleep_ticks(100);
    }
}
