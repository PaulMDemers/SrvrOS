#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <srvros/sys.h>

static unsigned ready, received[2], leave;
static pthread_t main_thread, handled_on;
static volatile sig_atomic_t handlers;
static void handler(int signal) {
    if(signal==SIGUSR1) { handlers++;handled_on=pthread_self(); }
}
static void *wait_worker(void *arg) {
    unsigned id=(unsigned)(uintptr_t)arg;
    sigset_t set;sigemptyset(&set);sigaddset(&set,SIGUSR1);
    __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    int signal=0;
    if(sigwait(&set,&signal) || signal!=SIGUSR1) return (void *)1;
    __atomic_store_n(&received[id],1,__ATOMIC_RELEASE);
    if(id==1 && pthread_kill(main_thread,SIGUSR2)) return (void *)2;
    while(!__atomic_load_n(&leave,__ATOMIC_ACQUIRE)) sched_yield();
    return NULL;
}
static void *join_signal_worker(void *unused) {
    (void)unused;
    return pthread_kill(main_thread,SIGUSR1)?(void *)1:NULL;
}
#define CHECK(x) do { if(!(x)) { printf("threadstress: targeted signal line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_targeted_signal_test(void) {
    struct sigaction action={.sa_handler=handler},old1,old2;
    CHECK(sigaction(SIGUSR1,&action,&old1)==0 && sigaction(SIGUSR2,&action,&old2)==0);
    sigset_t both,previous,pending;sigemptyset(&both);sigaddset(&both,SIGUSR1);sigaddset(&both,SIGUSR2);
    CHECK(pthread_sigmask(SIG_BLOCK,&both,&previous)==0);
    main_thread=pthread_self();handlers=0;handled_on=NULL;
    CHECK(raise(SIGUSR1)==0 && handlers==0);
    CHECK(sigpending(&pending)==0 && sigismember(&pending,SIGUSR1));
    CHECK(pthread_sigmask(SIG_UNBLOCK,&both,NULL)==0 && handlers==1 && handled_on==main_thread);
    errno=EDOM;CHECK(pthread_kill(main_thread,SIGUSR1)==0 && handlers==2 && errno==EDOM);
    CHECK(pthread_sigmask(SIG_BLOCK,&both,NULL)==0);
    pthread_t workers[2];ready=leave=received[0]=received[1]=0;
    for(unsigned i=0;i<2;i++) CHECK(pthread_create(&workers[i],NULL,wait_worker,(void *)(uintptr_t)i)==0);
    while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=2) sched_yield();
    errno=EDOM;CHECK(pthread_kill(workers[1],SIGUSR1)==0 && errno==EDOM);
    sigset_t reply;sigemptyset(&reply);sigaddset(&reply,SIGUSR2);int signal=0;
    CHECK(sigwait(&reply,&signal)==0 && signal==SIGUSR2);
    CHECK(__atomic_load_n(&received[1],__ATOMIC_ACQUIRE)==1 && !__atomic_load_n(&received[0],__ATOMIC_ACQUIRE));
    CHECK(sigpending(&pending)==0 && !sigismember(&pending,SIGUSR1));
    CHECK(pthread_kill(workers[0],SIGUSR1)==0);
    while(!__atomic_load_n(&received[0],__ATOMIC_ACQUIRE)) sched_yield();
    __atomic_store_n(&leave,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<2;i++) {
        while(srv_thread_status((uint64_t)(uintptr_t)workers[i])>0) sched_yield();
        CHECK(pthread_kill(workers[i],0)==0 && pthread_kill(workers[i],SIGUSR1)==0);
        void *result=NULL;CHECK(pthread_join(workers[i],&result)==0 && !result);
        CHECK(pthread_kill(workers[i],0)==ESRCH);
    }
    CHECK(handlers==2);
    CHECK(pthread_sigmask(SIG_UNBLOCK,&both,NULL)==0);
    pthread_t finished;
    CHECK(pthread_create(&finished,NULL,join_signal_worker,NULL)==0);
    /* Raw scheduler calls intentionally do not dispatch libc handlers. Force
     * a pending signal and an already-finished join target, without a race
     * between a short condition timeout and the worker's signal delivery. */
    uint64_t deadline=(uint64_t)srv_monotonic_us()+5000000;
    while(srv_thread_status((uint64_t)(uintptr_t)finished)>0 &&
          (uint64_t)srv_monotonic_us()<deadline) srv_yield();
    CHECK(srv_thread_status((uint64_t)(uintptr_t)finished)==0 && handlers==2);
    void *joined=NULL;
    CHECK(pthread_join(finished,&joined)==0 && !joined);
    CHECK(handlers==3 && handled_on==main_thread);
    CHECK(sigaction(SIGUSR1,&old1,NULL)==0 && sigaction(SIGUSR2,&old2,NULL)==0);
    CHECK(pthread_sigmask(SIG_SETMASK,&previous,NULL)==0);
    puts("threadstress: targeted-signal ok");return 0;
}
