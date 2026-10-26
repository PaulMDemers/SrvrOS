#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
extern char **environ;
#include <srvros/sys.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: futex line=%d errno=%d failed\n",__LINE__,errno);return 1; } } while(0)
static uint32_t word,ready;
static long result;
static unsigned handled;
static void handler(int signal) { (void)signal;handled++; }
static void *waiter(void *unused) {
    (void)unused;
    __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
    result=srv_futex_wait(&word,0,200);
    if(result==SRV_ERR_INTR) {
        uint64_t signal=0;
        if(srv_signal_poll(&signal)!=SIGUSR2 || signal!=SIGUSR2) return (void *)1;
    }
    return NULL;
}
static void *exit_waiting_process(void *unused) {
    (void)unused;uint64_t start=(uint64_t)srv_ticks();
    while((uint64_t)srv_ticks()-start<150) {
        if(srv_futex_wake(&word,1)==1) _exit(23);
        srv_yield();
    }
    _exit(98);
}
int posix_futex_exit_child(void) {
    pthread_t thread;word=0;
    if(pthread_create(&thread,NULL,exit_waiting_process,NULL)) return 97;
    (void)srv_futex_wait(&word,0,200);
    return 99; /* Process cancellation must take over before syscall return. */
}
int posix_futex_test(void) {
    struct sigaction action={.sa_handler=handler,.sa_flags=SA_RESTART},old;
    sigset_t set,previous;sigemptyset(&set);sigaddset(&set,SIGUSR2);
    CHECK(sigaction(SIGUSR2,&action,&old)==0);
    CHECK(pthread_sigmask(SIG_UNBLOCK,&set,&previous)==0);
    /* A pending caught signal interrupts the raw syscall before parking. */
    word=0;uint64_t signal=0;
    CHECK(srv_thread_signal((uintptr_t)pthread_self(),SIGUSR2)==0);
    CHECK(srv_futex_wait(&word,0,200)==SRV_ERR_INTR);
    CHECK(srv_signal_poll(&signal)==SIGUSR2 && signal==SIGUSR2);
    CHECK(srv_futex_wait(&word,1,200)==SRV_ERR_AGAIN);
    for(unsigned round=0;round<16;round++) for(unsigned release=0;release<2;release++) {
        word=ready=0;result=99;
        pthread_t thread;CHECK(pthread_create(&thread,NULL,waiter,NULL)==0);
        uint64_t start=(uint64_t)srv_ticks();long woke=0;
        /* Wake receipt proves the worker parked; no guessed delay. The raw
         * wait rechecks its unchanged word and may park again before signal. */
        while((uint64_t)srv_ticks()-start<150) {
            if(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)) {
                woke=srv_futex_wake(&word,1);
                if(woke) break;
            }
            srv_yield();
        }
        CHECK(woke==1);
        if(release) {
            __atomic_store_n(&word,1,__ATOMIC_RELEASE);
            CHECK(srv_futex_wake(&word,UINT64_MAX)>=0);
        } else CHECK(srv_thread_signal((uintptr_t)thread,SIGUSR2)==0);
        void *value=(void *)99;CHECK(pthread_join(thread,&value)==0 && !value);
        CHECK(result==(release?0:SRV_ERR_INTR));
        CHECK(srv_futex_wake(&word,UINT64_MAX)==0);
    }
    for(unsigned i=0;i<4;i++) {
        char *args[]={"threadstress","futex-exit-child",NULL};pid_t pid;int status;
        CHECK(posix_spawn(&pid,"/fat/bin/threadstress",NULL,NULL,args,environ)==0);
        CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==23);
    }
    /* More addresses than the kernel table: completed timeouts must recycle. */
    uint32_t churn[96]={0};
    for(unsigned i=0;i<96;i++) CHECK(srv_futex_wait(&churn[i],0,1)==-ETIMEDOUT);
    CHECK(!handled); /* Raw probes consumed their own targeted signals. */
    CHECK(sigaction(SIGUSR2,&old,NULL)==0 && pthread_sigmask(SIG_SETMASK,&previous,NULL)==0);
    puts("threadstress: futex ok");return 0;
}

static uint32_t *mapped_word;
static void *mapping_waiter(void *unused) {
    (void)unused;
    __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
    result=srv_futex_wait(mapped_word,0,200);
    return NULL;
}
int posix_futex_lifetime_test(void) {
    word=0;
    CHECK(srv_futex_wait(&word,0,UINT64_MAX)==SRV_ERR_INVAL);
    for(unsigned round=0;round<4;round++) for(unsigned change=0;change<5;change++) {
        mapped_word=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        CHECK(mapped_word!=MAP_FAILED);*mapped_word=0;ready=0;result=99;
        pthread_t thread;CHECK(pthread_create(&thread,NULL,mapping_waiter,NULL)==0);
        uint64_t start=(uint64_t)srv_ticks();long woke=0;
        while((uint64_t)srv_ticks()-start<150) {
            if(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)) {
                woke=srv_futex_wake(mapped_word,1);if(woke)break;
            }
            srv_yield();
        }
        CHECK(woke==1);
        if(change==0 || change==2) {
            CHECK(munmap(mapped_word,4096)==0);
            if(change==2) CHECK(mmap(mapped_word,4096,PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==mapped_word);
        } else if(change==1 || change==3) {
            CHECK(mprotect(mapped_word,4096,PROT_NONE)==0);
            if(change==3) CHECK(mprotect(mapped_word,4096,PROT_READ|PROT_WRITE)==0);
        } else {
            CHECK(mprotect(mapped_word,4096,PROT_READ)==0);
            CHECK(mprotect(mapped_word,4096,PROT_READ|PROT_WRITE)==0);
            __atomic_store_n(mapped_word,1,__ATOMIC_RELEASE);
            CHECK(srv_futex_wake(mapped_word,1)>=0);
        }
        CHECK(pthread_join(thread,NULL)==0);
        CHECK(result==(change==4?0:SRV_ERR_FAULT));
        if(change==1) CHECK(mprotect(mapped_word,4096,PROT_READ|PROT_WRITE)==0);
        if(change!=0) {
            /* A new wait on restored/replaced storage has an independent life. */
            *mapped_word=0;CHECK(srv_futex_wait(mapped_word,0,1)==-ETIMEDOUT);
            CHECK(munmap(mapped_word,4096)==0);
        }
    }
    puts("threadstress: futex-lifetime ok");return 0;
}
