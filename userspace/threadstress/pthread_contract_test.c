#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>

static pthread_key_t key;
static unsigned ready,release_workers,destructor_calls[4],bad_destructor;
static _Thread_local unsigned passes;
static void destructor(void *value) {
    unsigned id=(unsigned)(uintptr_t)value-1;
    if(id>=4 || pthread_getspecific(key)!=NULL) { __atomic_store_n(&bad_destructor,1,__ATOMIC_RELEASE);return; }
    destructor_calls[id]++;passes++;
    if(id==3 || passes<3) if(pthread_setspecific(key,value)) __atomic_store_n(&bad_destructor,1,__ATOMIC_RELEASE);
}
static void *key_worker(void *arg) {
    if(pthread_getspecific(key)!=NULL || pthread_setspecific(key,arg)) return (void *)1;
    __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&release_workers,__ATOMIC_ACQUIRE)) sched_yield();
    return pthread_getspecific(key)==arg?NULL:(void *)2;
}
static void *reuse_worker(void *unused) {
    (void)unused;
    if(pthread_setspecific(key,(void *)55)) return (void *)1;
    __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&release_workers,__ATOMIC_ACQUIRE)) sched_yield();
    if(pthread_getspecific(key)!=NULL) return (void *)2;
    return pthread_setspecific(key,(void *)66)?(void *)3:NULL;
}
static pthread_rwlock_t rwlock=PTHREAD_RWLOCK_INITIALIZER;
static unsigned readers,release_readers,writer_attempt,writer_entered;
static void *reader(void *unused) {
    (void)unused;
    if(pthread_rwlock_rdlock(&rwlock)) return (void *)1;
    __atomic_add_fetch(&readers,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&release_readers,__ATOMIC_ACQUIRE)) sched_yield();
    return pthread_rwlock_unlock(&rwlock)?(void *)2:NULL;
}
static void *writer(void *unused) {
    (void)unused;__atomic_store_n(&writer_attempt,1,__ATOMIC_RELEASE);
    if(pthread_rwlock_wrlock(&rwlock)) return (void *)1;
    __atomic_store_n(&writer_entered,1,__ATOMIC_RELEASE);
    return pthread_rwlock_unlock(&rwlock)?(void *)2:NULL;
}
#define CHECK(x) do { if(!(x)) { printf("threadstress: pthread contracts line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_pthread_contract_test(void) {
    pthread_t workers[4];ready=release_workers=bad_destructor=0;
    CHECK(pthread_key_create(&key,destructor)==0 && pthread_setspecific(key,(void *)99)==0);
    for(unsigned i=0;i<4;i++) { destructor_calls[i]=0;CHECK(pthread_create(&workers[i],NULL,key_worker,(void *)(uintptr_t)(i+1))==0); }
    while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=4) sched_yield();
    CHECK(pthread_getspecific(key)==(void *)99);
    __atomic_store_n(&release_workers,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<4;i++) {
        void *result=NULL;CHECK(pthread_join(workers[i],&result)==0 && !result);
        CHECK(destructor_calls[i]==(i==3?PTHREAD_DESTRUCTOR_ITERATIONS:3));
    }
    CHECK(!__atomic_load_n(&bad_destructor,__ATOMIC_ACQUIRE));
    pthread_key_t original=key;CHECK(pthread_key_delete(key)==0 && pthread_key_create(&key,NULL)==0 && key==original);
    CHECK(pthread_getspecific(key)==NULL);
    ready=release_workers=0;CHECK(pthread_create(&workers[0],NULL,reuse_worker,NULL)==0);
    while(!__atomic_load_n(&ready,__ATOMIC_ACQUIRE)) sched_yield();
    CHECK(pthread_key_delete(key)==0 && pthread_key_create(&key,NULL)==0 && key==original);
    __atomic_store_n(&release_workers,1,__ATOMIC_RELEASE);
    void *result=NULL;CHECK(pthread_join(workers[0],&result)==0 && !result && pthread_key_delete(key)==0);
    CHECK(pthread_rwlock_rdlock(&rwlock)==0 && pthread_rwlock_tryrdlock(&rwlock)==0);
    CHECK(pthread_rwlock_trywrlock(&rwlock)==EBUSY && pthread_rwlock_destroy(&rwlock)==EBUSY);
    CHECK(pthread_rwlock_unlock(&rwlock)==0 && pthread_rwlock_trywrlock(&rwlock)==EBUSY);
    CHECK(pthread_rwlock_unlock(&rwlock)==0);
    readers=release_readers=writer_attempt=writer_entered=0;
    CHECK(pthread_create(&workers[0],NULL,reader,NULL)==0 && pthread_create(&workers[1],NULL,reader,NULL)==0);
    while(__atomic_load_n(&readers,__ATOMIC_ACQUIRE)!=2) sched_yield();
    CHECK(pthread_create(&workers[2],NULL,writer,NULL)==0);
    while(!__atomic_load_n(&writer_attempt,__ATOMIC_ACQUIRE)) sched_yield();
    for(unsigned i=0;i<16;i++) sched_yield();
    CHECK(!__atomic_load_n(&writer_entered,__ATOMIC_ACQUIRE));
    __atomic_store_n(&release_readers,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<3;i++) { result=NULL;CHECK(pthread_join(workers[i],&result)==0 && !result); }
    CHECK(__atomic_load_n(&writer_entered,__ATOMIC_ACQUIRE) && pthread_rwlock_destroy(&rwlock)==0);
    struct sched_param param={.sched_priority=123};int policy=-1;errno=EDOM;
    CHECK(pthread_getschedparam(pthread_self(),&policy,&param)==0 && policy==SCHED_OTHER && !param.sched_priority && errno==EDOM);
    CHECK(pthread_setschedparam(pthread_self(),SCHED_OTHER,&param)==0);
    param.sched_priority=1;CHECK(pthread_setschedparam(pthread_self(),SCHED_OTHER,&param)==EINVAL);
    CHECK(pthread_setschedparam(pthread_self(),SCHED_FIFO,&param)==EINVAL);
    CHECK(pthread_getschedparam((pthread_t)(uintptr_t)0x123456,&policy,&param)==ESRCH);
    CHECK(sched_get_priority_min(SCHED_OTHER)==0 && sched_get_priority_max(SCHED_OTHER)==0);
    CHECK(sched_get_priority_min(SCHED_RR)==-1 && errno==EINVAL);
    puts("threadstress: pthread contracts ok");return 0;
}
