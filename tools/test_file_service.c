#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/mman.h>
#include <errno.h>
#include "../userspace/lib/src/desk_file_job.c"
static pthread_t ui;
static atomic_uint hold_spawn,hold_unmap,spawn_calls,unmap_calls,fail_spawn,fail_unmap;
static unsigned fail_create,fail_join;
static void *expected_stack;
static int next_fd=10,closed[1024],children,reaped,eof[64],writers[64];
static void nap(void) {struct timespec t={0,1000000};nanosleep(&t,NULL);}
long srv_signal_config(uint64_t signal,uint64_t action) {assert(!pthread_equal(pthread_self(),ui) && signal==SIGPIPE && action==SRV_SIGNAL_IGNORE);return 0;}
long srv_pipe(int fds[2]) {assert(!pthread_equal(pthread_self(),ui));fds[0]=next_fd++;fds[1]=next_fd++;return 0;}
long srv_fcntl(int fd,int command,uint64_t flags) {assert(!pthread_equal(pthread_self(),ui) && fd>=10 && command==SRV_F_SETFL && flags==SRV_FD_NONBLOCK);return 0;}
long srv_spawn_bg_args_fds(const char *path,const char *args,int input,int output) {
    assert(!pthread_equal(pthread_self(),ui) && !strcmp(path,"/fat/bin/deskfs") && !args[0] && output>input);
    atomic_fetch_add(&spawn_calls,1);while(atomic_load(&hold_spawn))nap();
    if(atomic_exchange(&fail_spawn,0))return -1;
    writers[children]=input+1;children++;return 1000+children;
}
long srv_close(int fd) {
    assert(fd>=10 && fd<next_fd && !closed[fd]);closed[fd]=1;
    for(int i=0;i<children;i++)if(writers[i]==fd)eof[i]=1;
    return 0;
}
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    assert(pid>1000 && pid<=1000+(unsigned)children && !flags && eof[pid-1001]);*status=0;reaped++;return pid;
}
long srv_monotonic_us(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long)t.tv_sec*1000000+t.tv_nsec/1000;}
long srv_getpid(void) {return 123;}
long srv_sleep_ticks(uint64_t ticks) {(void)ticks;nap();return 0;}
static pthread_mutex_t event_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event_cond=PTHREAD_COND_INITIALIZER;
long srv_futex_wait(uint32_t *word,uint32_t expected,uint64_t ticks) {
    assert(!ticks);pthread_mutex_lock(&event_lock);
    while(__atomic_load_n(word,__ATOMIC_ACQUIRE)==expected)pthread_cond_wait(&event_cond,&event_lock);
    pthread_mutex_unlock(&event_lock);return 0;
}
long srv_futex_wake(uint32_t *word,uint64_t count) {
    (void)word;assert(count==1);pthread_mutex_lock(&event_lock);pthread_cond_broadcast(&event_cond);pthread_mutex_unlock(&event_lock);return 1;
}
static int create_thread(pthread_t *thread,const pthread_attr_t *attr,void *(*start)(void *),void *arg) {
    void *stack=NULL;size_t size=0;assert(!pthread_attr_getstack(attr,&stack,&size) && stack==expected_stack && size==64*1024);
    assert(pthread_equal(pthread_self(),ui));if(fail_create){fail_create=0;return EAGAIN;}return pthread_create(thread,attr,start,arg);
}
static int join_thread(pthread_t thread,void **value) {if(fail_join){fail_join=0;return EBUSY;}return pthread_join(thread,value);}
static int retire_mapping(void *address,size_t length) {
    assert(!pthread_equal(pthread_self(),ui));atomic_fetch_add(&unmap_calls,1);while(atomic_load(&hold_unmap))nap();
    return atomic_exchange(&fail_unmap,0)?-1:munmap(address,length);
}
static void desk_log(const char *format,...) {(void)format;}
struct tool_phase_trace {int value;};
static _Thread_local unsigned tool_timing_lane;
static uint64_t tool_timing_begin(int role,uint64_t window,const char *stage) {(void)role;(void)window;(void)stage;return srv_monotonic_us();}
static void tool_timing_end(int role,uint64_t window,const char *stage,uint64_t at) {(void)role;(void)window;(void)stage;(void)at;}
static void tool_phase_begin(struct tool_phase_trace *phase) {phase->value=1;}
static void tool_phase_end(struct tool_phase_trace *phase,int role,uint64_t window,const char *stage) {(void)role;(void)window;(void)stage;assert(phase->value);}
#define pthread_create create_thread
#define pthread_join join_thread
#define munmap retire_mapping
#include "../userspace/desktools/file_service.h"
#undef pthread_create
#undef pthread_join
#undef munmap
static void await_count(atomic_uint *value,unsigned count) {for(unsigned n=0;n<5000;n++){if(atomic_load(value)>=count)return;nap();}assert(0);}
static void await_state(unsigned *value,unsigned state) {for(unsigned n=0;n<5000;n++){if(file_service_state(value)==state)return;nap();}assert(0);}
static void drain(struct file_service *s) {for(unsigned n=0;n<5000;n++){if(!file_service_poll(s))return;nap();}assert(0);}
static void clean_fds(void) {for(int i=10;i<next_fd;i++)assert(closed[i]);assert(reaped==children);}
int main(void) {
    ui=pthread_self();struct desk_file_pool pool;struct desk_file_channel channels[2];struct file_service service;
    expected_stack=service.stack;
    assert(!desk_file_pool_init(&pool,channels,2));fail_create=1;
    assert(file_service_start(&service,&pool,1)==-1 && !service.started && !pool.opener && !children);
    assert(!file_service_start(&service,&pool,1));
    atomic_store(&hold_spawn,1);assert(!pool.opener(pool.opener_context,&channels[0]));await_count(&spawn_calls,1);
    for(unsigned i=0;i<1000;i++)assert(!pool.opener(pool.opener_context,&channels[0]) && !channels[0].pid && service.requested==1);
    atomic_store(&hold_spawn,0);await_state(&service.starts[0].state,FILE_SERVICE_DONE);
    assert(pool.opener(pool.opener_context,&channels[0])==1 && channels[0].pid==1001 && service.opened==1);
    pool.spawned=pool.live=1;
    atomic_store(&hold_unmap,1);void *maps[FILE_SERVICE_RETIRES+1];
    struct file_service_release release={.service=&service,.window=42};
    for(unsigned i=0;i<FILE_SERVICE_RETIRES+1;i++){maps[i]=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(maps[i]!=MAP_FAILED);*(unsigned *)maps[i]=123;}
    for(unsigned i=0;i<FILE_SERVICE_RETIRES;i++)assert(!file_service_retire(&release,maps[i],4096));
    await_count(&unmap_calls,1);assert(!file_service_capacity(&service) && file_service_poll(&service));
    assert(file_service_retire(&release,maps[FILE_SERVICE_RETIRES],4096)==-1 && *(unsigned *)maps[FILE_SERVICE_RETIRES]==123);
    atomic_store(&hold_unmap,0);drain(&service);assert(service.retired==FILE_SERVICE_RETIRES && file_service_capacity(&service));
    assert(!file_service_retire(&release,maps[FILE_SERVICE_RETIRES],4096));drain(&service);
    assert(!file_service_finish(&service) && !service.started && service.retired==FILE_SERVICE_RETIRES+1);
    assert(pool.opener(pool.opener_context,&channels[1])==-1);
    assert(desk_file_pool_finish(&pool)==1);clean_fds();
    /* Failed startup publishes an error once, closes all construction fds,
     * and does not create a child or fall back on the UI. */
    assert(!desk_file_pool_init(&pool,channels,2) && !file_service_start(&service,&pool,1));
    atomic_store(&fail_spawn,1);assert(!file_service_open(&service,&channels[0]));await_state(&service.starts[0].state,FILE_SERVICE_DONE);
    assert(file_service_open(&service,&channels[0])==-1 && service.open_failed==1 && !channels[0].pid);
    assert(!file_service_finish(&service));clean_fds();
    /* Stop drains an admitted, unconsumed startup and naturally reaps it. */
    assert(!desk_file_pool_init(&pool,channels,2) && !file_service_start(&service,&pool,2));
    assert(!file_service_open(&service,&channels[1]));assert(!file_service_finish(&service) && service.abandoned==1 && !channels[1].pid);clean_fds();
    /* Failed unmap retains the address in a nonreusable failure record; join
     * failure cannot let service storage escape before actual thread return. */
    assert(!desk_file_pool_init(&pool,channels,2) && !file_service_start(&service,&pool,1));
    void *held=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(held!=MAP_FAILED);*(unsigned *)held=987;
    release.service=&service;atomic_store(&fail_unmap,1);assert(!file_service_retire(&release,held,4096));drain(&service);
    assert(service.retire_failed==1 && service.retires[0].address==held && *(unsigned *)held==987 && !file_service_capacity(&service));
    fail_join=1;assert(file_service_finish(&service)==-1 && !service.started && !fail_join);assert(!munmap(held,4096));clean_fds();
    puts("file-service: PASS held startup/unmap keeps UI admission nonblocking, bounded backpressure, error ownership, orphan drain, child reap and verified join");
}
