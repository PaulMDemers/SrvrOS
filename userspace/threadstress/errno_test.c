#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <srvros/sys.h>
#include <unistd.h>

#define ERRNO_WORKERS 4
static unsigned ready, release_workers;
static int *addresses[ERRNO_WORKERS];

static void *errno_worker(void *arg) {
    unsigned id=(unsigned)(uintptr_t)arg;
    int initial=errno;
    addresses[id]=&errno;
    errno=100+(int)id;
    __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&release_workers,__ATOMIC_ACQUIRE)) sched_yield();
    int failed=initial!=0;
    for(unsigned i=0;i<128;i++) {
        if(errno!=100+(int)id) failed=1;
        sched_yield();
    }
    return (void *)(uintptr_t)failed;
}

static uint64_t mapped_bytes(void) {
    struct srv_process_info info;long index=0;
    while((index=srv_proc_list(index,&info))>0)
        if(info.pid==(uint64_t)getpid()) return info.user_mapped_bytes;
    return 0;
}
int posix_errno_test(void) {
    /* Reuse pthread slots and verify a fresh zero value, including after the
     * creating thread has a nonzero errno. Check addresses as well as values. */
    uint64_t baseline=0;
    for(unsigned round=0;round<16;round++) {
        pthread_t threads[ERRNO_WORKERS];
        __atomic_store_n(&ready,0,__ATOMIC_RELAXED);
        __atomic_store_n(&release_workers,0,__ATOMIC_RELAXED);
        errno=777;
        for(unsigned i=0;i<ERRNO_WORKERS;i++)
            if(pthread_create(&threads[i],NULL,errno_worker,(void *)(uintptr_t)i)) return -1;
        while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=ERRNO_WORKERS) sched_yield();
        int failed=errno!=777;
        for(unsigned i=0;i<ERRNO_WORKERS;i++) {
            if(addresses[i]==&errno) failed=1;
            for(unsigned j=0;j<i;j++) if(addresses[i]==addresses[j]) failed=1;
        }
        __atomic_store_n(&release_workers,1,__ATOMIC_RELEASE);
        for(unsigned i=0;i<ERRNO_WORKERS;i++) {
            void *result=NULL;
            if(pthread_join(threads[i],&result) || result) failed=1;
        }
        if(failed || errno!=777) return -1;
        uint64_t current=mapped_bytes();
        if(!current) return -1;
        if(!round) baseline=current;
        else if(current!=baseline) return -1;
    }
    puts("threadstress: errno ok");
    return 0;
}
