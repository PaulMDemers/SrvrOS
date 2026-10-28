#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#define DESK_CACHE_BYTES (280u*240u*4u)
#define SRV_EXEC_BACKGROUND 1
#define DESK_CAPTURE_PATH 160
struct loop_trace { unsigned samples; };
struct operation_trace { unsigned value; };
struct desk_capture_job { int busy,reader,writer;uint64_t pid,retiring_pid,reap_us,cleanup_us;uint32_t *pixels; };
struct desk_application_info { const char *program,*role; };
struct srv_exec_request { const char *path;char **argv,**envp;unsigned flags;int stdin_fd,stdout_fd,stderr_fd; };
static atomic_uint waits,wakes,sleeps,allocations,frees,launches,reports,closes,polls,hold_child;
static unsigned create_calls,fail_create;
static uint32_t storage[4];
static pthread_t ui_thread;
static atomic_uint launch_logs,hold_launch_log;
static atomic_uint reap_calls,hold_reap;
static atomic_uint prepare_calls,hold_prepare;
static void nap(void);
static void life_launch_log(int application,int64_t pid,uint64_t elapsed) {
    (void)elapsed;
    assert(application==1 && pid==42 && !pthread_equal(pthread_self(),ui_thread));
    atomic_fetch_add(&launch_logs,1);
    while(atomic_load(&hold_launch_log))nap();
}
static void nap(void) { struct timespec t={0,1000000};nanosleep(&t,NULL); }
static long srv_wait(uint64_t pid,uint64_t *status,unsigned flags) {
    assert(pid==77 && !flags && !pthread_equal(pthread_self(),ui_thread));
    atomic_fetch_add(&reap_calls,1);
    while(atomic_load(&hold_reap))nap();
    *status=9;return (long)pid;
}
static int desk_capture_job_prepare(struct desk_capture_job *j,uint32_t width,uint32_t height,const char *path) {
    assert(!j->busy && width==280 && height==240 && !strcmp(path,"/test.bmp"));
    assert(!pthread_equal(pthread_self(),ui_thread));atomic_fetch_add(&prepare_calls,1);
    while(atomic_load(&hold_prepare))nap();
    *j=(struct desk_capture_job){.busy=1,.reader=3,.writer=4,.pid=9,.pixels=storage};return 0;
}
static uint64_t srv_monotonic_us(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
static void srv_sleep_ticks(unsigned ticks) { assert(ticks==1);atomic_fetch_add(&sleeps,1);nap(); }
static uint32_t *desk_pixels_alloc(unsigned w,unsigned h) { assert(w==280 && h==240);atomic_fetch_add(&allocations,1);return storage; }
static void desk_pixels_free(uint32_t *p,unsigned w,unsigned h) { assert(p==storage && w==280 && h==240);atomic_fetch_add(&frees,1); }
static const struct desk_application_info *desk_application_info(int app) { static const struct desk_application_info info={"/app","text"};assert(app==1);return &info; }
static long srv_exec(struct srv_exec_request *r) { assert(!strcmp(r->argv[2],"test"));atomic_fetch_add(&launches,1);return 42; }
static void loop_trace_report(struct loop_trace *t,int final) { assert(t->samples==1 && !final);atomic_fetch_add(&reports,1); }
static void operation_trace_report(struct operation_trace *t,int final) { assert(t->value==7 && !final); }
static void desk_capture_job_close(struct desk_capture_job *j) { atomic_fetch_add(&closes,1);j->busy=0;j->retiring_pid=j->pid;j->pid=0;j->pixels=NULL;j->reader=j->writer=-1; }
static int desk_capture_job_poll(struct desk_capture_job *j) { atomic_fetch_add(&polls,1);if(!atomic_load(&hold_child))j->retiring_pid=0;return 0; }
static int test_create(pthread_t *t,const pthread_attr_t *a,void *(*fn)(void *),void *p) { if(++create_calls==fail_create)return EAGAIN;return pthread_create(t,a,fn,p); }
static long srv_futex_wait(uint32_t *,uint32_t,uint64_t);
static pthread_mutex_t futex_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t futex_ready=PTHREAD_COND_INITIALIZER;
/* Portable host adapter for atomic compare-and-sleep. Guest VMs exercise the
 * real process-scoped futex syscall and its separate kernel regressions. */
static long srv_futex_wake(uint32_t *p,uint64_t count) {
    (void)p;assert(count==1);atomic_fetch_add(&wakes,1);
    pthread_mutex_lock(&futex_lock);pthread_cond_broadcast(&futex_ready);pthread_mutex_unlock(&futex_lock);return 0;
}
#define pthread_create test_create
#include "../userspace/deskd/lifecycle_worker.h"
#undef pthread_create
static struct lifecycle_worker *race_worker;
static atomic_uint inject;
static long srv_futex_wait(uint32_t *p,uint32_t expected,uint64_t ticks) {
    assert(!expected && !ticks);atomic_fetch_add(&waits,1);
    /* Publish between the worker's empty scan and kernel compare-and-sleep. */
    unsigned action=atomic_exchange(&inject,0);
    if(action==4)return -EINTR;
    if(action==1)life_queue(race_worker,&race_worker->launch[0].state,LIFE_QUEUED);
    if(action==2)life_queue(race_worker,&race_worker->stop,1);
    if(action==3) {life_publish(&race_worker->report_stop,1);life_notify(&race_worker->report_event);}
    pthread_mutex_lock(&futex_lock);
    if(__atomic_load_n(p,__ATOMIC_ACQUIRE)!=expected){pthread_mutex_unlock(&futex_lock);return -EAGAIN;}
    while(__atomic_load_n(p,__ATOMIC_ACQUIRE)==expected)pthread_cond_wait(&futex_ready,&futex_lock);
    pthread_mutex_unlock(&futex_lock);return 0;
}
static void await_state(unsigned *p,unsigned value) { for(unsigned n=0;n<5000;n++){if(life_state(p)==value)return;nap();}assert(!"state deadline"); }
static void await_count(atomic_uint *p,unsigned value) { for(unsigned n=0;n<5000;n++){if(atomic_load(p)>=value)return;nap();}assert(!"counter deadline"); }
static void launch_record(struct lifecycle_worker *w) { w->launch[0].application=1;w->launch[0].child_slot=3;strcpy(w->launch[0].path,"test");strcpy(w->launch[0].home,"/home"); }
static atomic_uint joined;
static void *stop_worker(void *p) {assert(!life_stop(p));atomic_store(&joined,1);return NULL;}
int main(void) {
    ui_thread=pthread_self();
    alarm(15);
    struct lifecycle_worker w={0};unsigned before=atomic_load(&waits);
    assert(!life_start(&w,1));await_count(&waits,before+2);
    before=atomic_load(&waits);unsigned slept=atomic_load(&sleeps);
    for(unsigned n=0;n<30;n++)nap();
    assert(atomic_load(&waits)==before && atomic_load(&sleeps)==slept); /* no idle timer */
    launch_record(&w);assert(!life_reserved(&w,3));atomic_store(&hold_launch_log,1);
    life_queue(&w,&w.launch[0].state,LIFE_QUEUED);await_count(&launch_logs,1);
    /* A stalled diagnostic writer retains its job; the UI side keeps running. */
    assert(life_reserved(&w,3) && life_state(&w.launch[0].state)==LIFE_QUEUED);
    atomic_store(&hold_launch_log,0);
    await_state(&w.launch[0].state,LIFE_DONE);assert(life_reserved(&w,3));assert(w.launch[0].pid==42 && atomic_load(&launches)==1 && atomic_load(&launch_logs)==1);
    w.memory[0].width=280;w.memory[0].height=240;life_queue(&w,&w.memory[0].state,LIFE_QUEUED);
    await_state(&w.memory[0].state,LIFE_DONE);assert(atomic_load(&allocations)==1 && !atomic_load(&frees));
    life_queue(&w,&w.memory[0].discard,1);await_state(&w.memory[0].state,LIFE_RETIRED);assert(atomic_load(&frees)==1);
    struct loop_trace trace={1};struct operation_trace op={7};assert(life_report_submit(&w,&trace,&op));
    await_count(&reports,1);await_state(&w.report_state,LIFE_IDLE);
    atomic_store(&hold_reap,1);assert(life_reap_submit(&w,77));await_count(&reap_calls,1);
    long reaped=0;uint64_t status=0;
    for(unsigned n=0;n<100;n++) {
        assert(life_reap_pending(&w,77) && !life_reap_submit(&w,77));
        assert(!life_reap_take(&w,77,&reaped,&status));
    }
    atomic_store(&hold_reap,0);await_state(&w.reap[0].state,LIFE_DONE);
    assert(life_reap_take(&w,77,&reaped,&status) && reaped==77 && status==9);
    assert(!life_reap_pending(&w,77) && !life_reap_take(&w,77,&reaped,&status));
    assert(atomic_load(&reap_calls)==1);
    atomic_store(&hold_child,1);struct desk_capture_job j={.busy=1,.reader=3,.writer=4,.pid=9,.pixels=storage};
    assert(life_capture_submit(&w,&j,1) && !j.pixels && j.busy && j.reader==-1);
    await_count(&polls,3);assert(life_state(&w.capture.state)==LIFE_QUEUED && atomic_load(&closes)==1 && !life_capture_take(&w,&j));
    assert(atomic_load(&sleeps)>slept);atomic_store(&hold_child,0);
    await_state(&w.capture.state,LIFE_DONE);assert(life_capture_take(&w,&j)==1 && !j.retiring_pid);
    atomic_store(&hold_prepare,1);
    assert(life_prepare_submit(&w,&j,280,240,"/test.bmp"));await_count(&prepare_calls,1);
    int prepared=1;
    for(unsigned n=0;n<100;n++) {
        assert(j.busy && !j.pixels && !j.pid);
        assert(!life_prepare_submit(&w,&j,280,240,"/test.bmp"));
        assert(!life_prepare_take(&w,&j,&prepared));
    }
    atomic_store(&hold_prepare,0);await_state(&w.prepare.state,LIFE_DONE);
    assert(life_prepare_take(&w,&j,&prepared) && !prepared && j.pixels==storage && j.pid==9);
    assert(!life_prepare_take(&w,&j,&prepared));
    assert(life_capture_submit(&w,&j,-1));await_state(&w.capture.state,LIFE_DONE);
    assert(life_capture_take(&w,&j)==-1 && !j.pixels && !j.retiring_pid);
    /* Stop cannot release an admitted screenshot child before it retires. */
    atomic_store(&hold_child,1);j=(struct desk_capture_job){.busy=1,.reader=3,.writer=4,.pid=10,.pixels=storage};
    unsigned prior_polls=atomic_load(&polls);assert(life_capture_submit(&w,&j,1));await_count(&polls,prior_polls+2);
    pthread_t stopper;assert(!pthread_create(&stopper,NULL,stop_worker,&w));
    for(unsigned n=0;n<10;n++)nap();
    assert(!atomic_load(&joined) && life_state(&w.capture.state)==LIFE_QUEUED);
    atomic_store(&hold_child,0);assert(!pthread_join(stopper,NULL) && atomic_load(&joined));
    assert(life_state(&w.capture.state)==LIFE_DONE);
    /* Work already queued before thread creation is not lost. */
    w=(struct lifecycle_worker){0};launch_record(&w);before=atomic_load(&wakes);
    life_queue(&w,&w.launch[0].state,LIFE_QUEUED);
    w.memory[0].width=280;w.memory[0].height=240;
    life_queue(&w,&w.memory[0].state,LIFE_QUEUED);life_queue(&w,&w.memory[0].discard,1);
    assert(atomic_load(&wakes)==before+1); /* Coalescing retains all jobs. */
    assert(!life_start(&w,0));await_state(&w.launch[0].state,LIFE_DONE);await_state(&w.memory[0].state,LIFE_RETIRED);
    assert(atomic_load(&allocations)==2 && atomic_load(&frees)==2);assert(!life_stop(&w));
    /* Work and stop racing immediately before futex wait cannot be lost. */
    w=(struct lifecycle_worker){0};launch_record(&w);race_worker=&w;atomic_store(&inject,1);
    assert(!life_start(&w,0));await_state(&w.launch[0].state,LIFE_DONE);assert(!life_stop(&w));
    w=(struct lifecycle_worker){0};race_worker=&w;atomic_store(&inject,2);
    assert(!life_start(&w,0));assert(!pthread_join(w.thread,NULL));assert(life_state(&w.stop));
    w=(struct lifecycle_worker){0};race_worker=&w;atomic_store(&inject,3);
    assert(!pthread_create(&w.report_thread,NULL,life_report_thread,&w));assert(!pthread_join(w.report_thread,NULL));
    /* Refused/interrupted waits retain the old retry cadence, not a spin. */
    w=(struct lifecycle_worker){0};race_worker=&w;atomic_store(&inject,4);slept=atomic_load(&sleeps);before=atomic_load(&waits);
    assert(!life_start(&w,0));await_count(&waits,before+2);assert(atomic_load(&sleeps)==slept+1);assert(!life_stop(&w));
    /* A failed second thread creation must wake/join its sleeping reporter. */
    w=(struct lifecycle_worker){0};fail_create=create_calls+2;assert(life_start(&w,1)==EAGAIN && !w.report_started);
    puts("lifecycle-wait: PASS idle blocking, launch/allocation/discard/report notifications, capture retry/ownership, prewait publication/stop races and creation-failure join");
}
