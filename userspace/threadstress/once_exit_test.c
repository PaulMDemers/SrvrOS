#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <srvros/sys.h>

extern char **environ;
#define CHECK(x) do { if (!(x)) { printf("threadstress: once-exit line=%d failed\n",__LINE__);exit(1); } } while (0)
static pthread_once_t outer,inner,completed;
static pthread_key_t key;
static uint32_t entered,release_owner;
static unsigned outer_calls,inner_calls,completed_calls,destructors;
static int payload;
static void completed_init(void) { completed_calls++; }
static void inner_init(void) {
    if (++inner_calls==1) {
        __atomic_store_n(&entered,1,__ATOMIC_RELEASE);
        while (!__atomic_load_n(&release_owner,__ATOMIC_ACQUIRE)) srv_yield();
        pthread_exit((void *)(uintptr_t)0x5678);
    }
}
static void outer_init(void) {
    if (++outer_calls==1) CHECK(pthread_once(&inner,inner_init)==0);
    payload=42;
}
static void destructor(void *value) {
    CHECK(value==(void *)1);
    /* Must be retryable before destructors, even for nested initialization. */
    CHECK(pthread_once(&inner,inner_init)==0);
    __atomic_add_fetch(&destructors,1,__ATOMIC_RELEASE);
}
static void *owner(void *unused) {
    (void)unused;
    CHECK(pthread_setspecific(key,(void *)1)==0);
    CHECK(pthread_once(&completed,completed_init)==0);
    CHECK(pthread_once(&outer,outer_init)==0);
    CHECK(0);return NULL;
}
static void *contender(void *unused) {
    (void)unused;
    CHECK(pthread_once(&outer,outer_init)==0 && payload==42);
    return NULL;
}
static void reset(void) {
    outer=inner=completed=PTHREAD_ONCE_INIT;entered=release_owner=0;
    outer_calls=inner_calls=completed_calls=destructors=0;payload=0;
    CHECK(pthread_key_create(&key,destructor)==0);
}
static void check_result(void) {
    CHECK(outer_calls==2 && inner_calls==2 && completed_calls==1 && destructors==1);
    CHECK(pthread_once(&completed,completed_init)==0 && completed_calls==1);
    CHECK(pthread_once(&outer,outer_init)==0 && outer_calls==2 && payload==42);
    CHECK(pthread_key_delete(key)==0);
}
static void *initial_peer(void *unused) {
    (void)unused;
    while (!__atomic_load_n(&entered,__ATOMIC_ACQUIRE)) srv_yield();
    __atomic_store_n(&release_owner,1,__ATOMIC_RELEASE);
    contender(NULL);
    void *value=NULL;
    CHECK(pthread_join((pthread_t)(uintptr_t)1,&value)==0 && value==(void *)(uintptr_t)0x5678);
    check_result();return NULL;
}
int posix_once_exit_child(void) {
    reset();pthread_t peer;CHECK(pthread_create(&peer,NULL,initial_peer,NULL)==0);
    owner(NULL);return 1;
}
int posix_once_exit_test(void) {
    for(unsigned round=0;round<4;round++) {
        reset();pthread_t first,peers[3];void *value=NULL;
        CHECK(pthread_create(&first,NULL,owner,NULL)==0);
        while (!__atomic_load_n(&entered,__ATOMIC_ACQUIRE)) srv_yield();
        for(unsigned i=0;i<3;i++) CHECK(pthread_create(&peers[i],NULL,contender,NULL)==0);
        /* Prove a contender parked before releasing the exiting initializer. */
        uint64_t start=(uint64_t)srv_ticks();long woke=0;
        while((uint64_t)srv_ticks()-start<100) {
            woke=srv_futex_wake((uint32_t *)&outer,1);if(woke)break;srv_yield();
        }
        CHECK(woke==1);__atomic_store_n(&release_owner,1,__ATOMIC_RELEASE);
        CHECK(pthread_join(first,&value)==0 && value==(void *)(uintptr_t)0x5678);
        for(unsigned i=0;i<3;i++) CHECK(pthread_join(peers[i],NULL)==0);
        check_result();
        pid_t child;int status=0;
        char *args[]={"threadstress","once-exit-child",NULL};
        CHECK(posix_spawn(&child,"/fat/bin/threadstress",NULL,NULL,args,environ)==0);
        CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    puts("threadstress: once-exit ok");return 0;
}
