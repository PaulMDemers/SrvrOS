#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
static unsigned begin;
static uint64_t worker_cpu;
static uint64_t stamp(int id) {
    struct timespec time;if(clock_gettime(id,&time)) return 0;
    return (uint64_t)time.tv_sec*1000000+(uint64_t)time.tv_nsec/1000;
}
static uint64_t cpu(const struct rusage *usage) {
    return (uint64_t)(usage->ru_utime.tv_sec+usage->ru_stime.tv_sec)*1000000+
        (uint64_t)usage->ru_utime.tv_usec+usage->ru_stime.tv_usec;
}
int posix_cpu_child(void) {
    uint64_t end=stamp(CLOCK_MONOTONIC)+100000;
    volatile uint64_t value=1;
    while(stamp(CLOCK_MONOTONIC)<end) for(unsigned i=0;i<5000;i++) value=value*6364136223846793005ull+1;
    return value==0xdeadbeef?1:0;
}
static void *cpu_worker(void *unused) {
    (void)unused;
    while(!__atomic_load_n(&begin,__ATOMIC_ACQUIRE)) sched_yield();
    uint64_t before=stamp(CLOCK_THREAD_CPUTIME_ID);
    posix_cpu_child();worker_cpu=stamp(CLOCK_THREAD_CPUTIME_ID)-before;
    return NULL;
}
#define CHECK(x) do { if(!(x)) { printf("threadstress: usage line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_usage_test(void) {
    struct rusage before,after,child_before,child_after;
    CHECK(getrusage(RUSAGE_SELF,&before)==0);
    uint64_t wall=stamp(CLOCK_MONOTONIC);CHECK(usleep(100000)==0);
    wall=stamp(CLOCK_MONOTONIC)-wall;
    CHECK(getrusage(RUSAGE_SELF,&after)==0 && cpu(&after)>=cpu(&before));
    CHECK(cpu(&after)-cpu(&before)<wall/2+1000);
    uint64_t self=stamp(CLOCK_THREAD_CPUTIME_ID),total=stamp(CLOCK_PROCESS_CPUTIME_ID);
    pthread_t worker;begin=0;CHECK(pthread_create(&worker,NULL,cpu_worker,NULL)==0);
    __atomic_store_n(&begin,1,__ATOMIC_RELEASE);
    CHECK(pthread_join(worker,NULL)==0);
    self=stamp(CLOCK_THREAD_CPUTIME_ID)-self;total=stamp(CLOCK_PROCESS_CPUTIME_ID)-total;
    CHECK(worker_cpu>=20000 && total>=worker_cpu && self<worker_cpu);
    CHECK(getrusage(RUSAGE_SELF,&after)==0 && after.ru_utime.tv_sec>=0 && after.ru_stime.tv_sec>=0);
    uint64_t sampled=stamp(CLOCK_PROCESS_CPUTIME_ID);clock_t ticks=clock();
    CHECK(ticks>=0 && (uint64_t)ticks>=sampled && (uint64_t)ticks-sampled<100000);
    struct timespec resolution;CHECK(clock_getres(CLOCK_THREAD_CPUTIME_ID,&resolution)==0 && resolution.tv_nsec>0);
    CHECK(getrusage(RUSAGE_CHILDREN,&child_before)==0);
    char *argv[]={"threadstress","cpu-child",NULL};pid_t pid;int status=0;
    CHECK(posix_spawn(&pid,"/fat/bin/threadstress",NULL,NULL,argv,environ)==0);
    CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(getrusage(RUSAGE_CHILDREN,&child_after)==0 && cpu(&child_after)>cpu(&child_before)+20000);
    struct tms tm;CHECK(times(&tm)>=0 && tm.tms_cutime+tm.tms_cstime>0);
    CHECK(getrusage(RUSAGE_SELF,&before)==0);
    size_t size=16*1024*1024;
    void *mapping=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    CHECK(mapping!=MAP_FAILED && getrusage(RUSAGE_SELF,&after)==0);
    CHECK(after.ru_maxrss>before.ru_maxrss && after.ru_maxrss>=16384);
    CHECK(munmap(mapping,size)==0 && getrusage(RUSAGE_SELF,&before)==0 && before.ru_maxrss==after.ru_maxrss);
    puts("threadstress: usage ok");return 0;
}
