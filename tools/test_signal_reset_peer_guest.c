/* Opt-in private operation4 pins install+delivery to the selected main/worker. */
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <srvros/sys.h>
#include "../kernel/src/process_signal.h"

static uint64_t target_worker,kind,worker_ready,worker_go,target_tid;
extern void reset_peer_trampoline(void);
static void message(const char *text) { (void)srv_write(1,text,strlen(text)); }
static void handler(uint64_t signal,struct process_signal_private_info *info,
    struct signal_context_snapshot *context) {
    if(signal!=SRV_SIGNAL_USR1 || info->target_tid!=target_tid) srv_syscall1(SYS_EXIT,97);
    if(kind==3) {
        message("reset-peer: delayed handler before private return\n");
        memset(context,0xd3,sizeof(*context));return;
    }
    message("reset-peer: FORBIDDEN HANDLER SENTINEL\n");
    srv_syscall1(SYS_EXIT,90);
}
static void install_and_deliver(void) {
    register uint64_t r8 __asm__("r8")=0;
    uint64_t result=UINT64_C(0x5349475452415031);
    __asm__ volatile("int3" : "+a"(result),"+r"(r8) : "D"(4ull),
        "S"((uint64_t)(uintptr_t)handler),"d"((uint64_t)(uintptr_t)reset_peer_trampoline),
        "c"((uint64_t)SIGNAL_ACTION_RESET) : "r9","r10","r11","cc","memory");
    message("reset-peer: FORBIDDEN RESUME SENTINEL\n");
    srv_syscall1(SYS_EXIT,98);
}
static void *peer(void *unused) {
    (void)unused;
    if(srv_signal_mask(!target_worker && kind==3?SRV_SIGNAL_BLOCK:SRV_SIGNAL_UNBLOCK,
            1ull<<SRV_SIGNAL_USR1,0)<0) srv_syscall1(SYS_EXIT,91);
    __atomic_store_n(&worker_ready,(uint64_t)srv_thread_self(),__ATOMIC_RELEASE);
    if(target_worker) {
        while(!__atomic_load_n(&worker_go,__ATOMIC_ACQUIRE)) srv_yield();
        install_and_deliver();
    }
    for(;;) srv_yield();
}
int main(int argc,char **argv) {
    target_worker=argc>1 && !strcmp(argv[1],"worker");
    kind=argc>2 && !strcmp(argv[2],"shared")?1:
        argc>2 && !strcmp(argv[2],"firstshared")?2:
        argc>2 && !strcmp(argv[2],"currentonly")?3:0;
    if(srv_signal_config(SRV_SIGNAL_USR1,SRV_SIGNAL_CATCH)<0 ||
       srv_signal_mask(target_worker && kind==3?SRV_SIGNAL_BLOCK:SRV_SIGNAL_UNBLOCK,
            1ull<<SRV_SIGNAL_USR1,0)<0) return 92;
    pthread_t thread;if(pthread_create(&thread,0,peer,0)) return 93;
    uint64_t start=srv_monotonic_us(),worker_tid;
    while(!(worker_tid=__atomic_load_n(&worker_ready,__ATOMIC_ACQUIRE))) {
        if((uint64_t)srv_monotonic_us()-start>5000000) return 94;
        srv_yield();
    }
    target_tid=target_worker?worker_tid:1;
    uint64_t peer_tid=target_worker?1:worker_tid;
    if((kind!=2 && srv_thread_signal(target_tid,SRV_SIGNAL_USR1)<0) ||
       ((kind==0 || kind==2) && srv_thread_signal(peer_tid,SRV_SIGNAL_USR1)<0) ||
       (kind!=0 && srv_kill_signal(srv_getpid(),SRV_SIGNAL_USR1)<0)) return 95;
    message("reset-peer: pending prepared before install\n");
    if(target_worker) {
        __atomic_store_n(&worker_go,1,__ATOMIC_RELEASE);
        for(;;) srv_yield();
    }
    install_and_deliver();return 96;
}
