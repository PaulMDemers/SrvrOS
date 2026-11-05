/* Native CATCH->DEFAULT must terminate before returning to a masked caller. */
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <srvros/sys.h>

static uint64_t target,signum;
static void message(const char *text) { (void)srv_write(1,text,strlen(text)); }
static void *peer(void *unused) {
    (void)unused;
    if(srv_signal_mask(SRV_SIGNAL_UNBLOCK,1ull<<signum,0)<0) srv_syscall1(SYS_EXIT,91);
    __atomic_store_n(&target,(uint64_t)srv_thread_self(),__ATOMIC_RELEASE);
    for(;;) srv_yield();
}
int main(int argc,char **argv) {
    signum=argc>1 && !strcmp(argv[1],"usr2")?SRV_SIGNAL_USR2:SRV_SIGNAL_USR1;
    int shared=argc>2 && !strcmp(argv[2],"shared");
    if(srv_signal_config(signum,SRV_SIGNAL_CATCH)<0 ||
       srv_signal_mask(SRV_SIGNAL_BLOCK,1ull<<signum,0)<0) return 92;
    pthread_t thread;
    if(pthread_create(&thread,0,peer,0)) return 93;
    uint64_t start=srv_monotonic_us(),tid;
    while(!(tid=__atomic_load_n(&target,__ATOMIC_ACQUIRE))) {
        if((uint64_t)srv_monotonic_us()-start>5000000) return 94;
        srv_yield();
    }
    if((shared?srv_kill_signal(srv_getpid(),signum):srv_thread_signal(tid,signum))<0) return 95;
    message("action-default: peer pending before config\n");
    if(srv_signal_config(signum,SRV_SIGNAL_DEFAULT)<0) return 96;
    message("action-default: FORBIDDEN CONFIG SENTINEL\n");
    return 90;
}
