#ifndef DESKD_LIFECYCLE_WORKER_H
#define DESKD_LIFECYCLE_WORKER_H
#include <pthread.h>
/* The UI owns admission and publication. One persistent worker owns blocking
 * syscalls. Release/acquire transfers each record; no scene pointers cross the
 * boundary. A discarded allocation keeps its budget until unmapping completes. */
/* Every admitted client buffer is at least 280x240. Metadata must cover
 * the full existing byte budget, including buffers awaiting release. */
#define LIFE_MEMORY_SLOTS (DESK_CACHE_BYTES/(280u*240u*4u)+1)
#define LIFE_LAUNCH_SLOTS 16
#define LIFE_REAP_SLOTS 16
#define LIFE_TERMINATE_SLOTS 16
#define LIFE_LOG_SLOTS 64
#define LIFE_LOG_BYTES 384
#define LIFE_IDLE 0
#define LIFE_QUEUED 1
#define LIFE_DONE 2
#define LIFE_RETIRED 3
#define LIFE_RUNNING 4
struct life_memory {
    unsigned state,discard,width,height;
    uint32_t *pixels;
    uint64_t elapsed;
};
struct life_launch {
    unsigned state;
    int application,child_slot;
    char path[160],home[160];
    long pid;
    uint64_t elapsed,queued_us,begin_us,end_us;
};
struct life_capture { unsigned state;int result;struct desk_capture_job job; };
struct life_prepare { unsigned state,width,height;int result;char path[DESK_CAPTURE_PATH];struct desk_capture_job job; };
struct life_reap { unsigned state;uint64_t pid,status,queued_us,begin_us,end_us;long result; };
struct life_terminate { unsigned state;uint64_t pid,elapsed;long result; };
struct lifecycle_worker {
    /* Single UI producer, single report-thread consumer. Never overwrite a
     * queued or emitting record, and never wait for console/serial pressure. */
    unsigned log_write,log_read;
    uint64_t log_dropped;
    struct {unsigned state;char text[LIFE_LOG_BYTES];} logs[LIFE_LOG_SLOTS];
    pthread_t thread,report_thread;
    unsigned stop,report_state,report_stop,report_started;
    unsigned work_event,report_event;
    struct loop_trace report;
    struct operation_trace operations;
    struct life_capture capture;
    struct life_prepare prepare;
    struct life_memory memory[LIFE_MEMORY_SLOTS];
    struct life_launch launch[LIFE_LAUNCH_SLOTS];
    struct life_reap reap[LIFE_REAP_SLOTS];
    struct life_terminate terminate[LIFE_TERMINATE_SLOTS];
};
static unsigned life_state(const unsigned *state) { return __atomic_load_n(state,__ATOMIC_ACQUIRE); }
static void life_publish(unsigned *state,unsigned value) { __atomic_store_n(state,value,__ATOMIC_RELEASE); }
/* A latched event coalesces producers without losing publication before sleep.
 * Clear before scanning, then wait only while still zero. The kernel's futex
 * compare-and-sleep closes the final producer/wait race. Work records retain
 * their own release/acquire ownership; this word is only a wake hint. */
static void life_notify(unsigned *event) {
    if(!__atomic_exchange_n(event,1,__ATOMIC_RELEASE))
        (void)srv_futex_wake((uint32_t *)event,1);
}
static void life_clear(unsigned *event) { (void)__atomic_exchange_n(event,0,__ATOMIC_ACQUIRE); }
static void life_wait(unsigned *event) {
    long result=srv_futex_wait((uint32_t *)event,0,0);
    /* Interrupted/refused waits must not become an empty-queue busy loop.
     * Retry with the old cadence only on failure without a latched producer. */
    if(result<0 && !life_state(event))srv_sleep_ticks(1);
}
static void life_queue(struct lifecycle_worker *w,unsigned *state,unsigned value) {
    life_publish(state,value);life_notify(&w->work_event);
}
static int life_terminate_pending(struct lifecycle_worker *w,uint64_t pid) {
    for(unsigned n=0;n<LIFE_TERMINATE_SLOTS;n++)
        if(life_state(&w->terminate[n].state)!=LIFE_IDLE && w->terminate[n].pid==pid)return 1;
    return 0;
}
static int life_terminate_submit(struct lifecycle_worker *w,uint64_t pid) {
    if(!pid || life_state(&w->stop))return 0;
    if(life_terminate_pending(w,pid))return 1;
    for(unsigned n=0;n<LIFE_TERMINATE_SLOTS;n++)if(life_state(&w->terminate[n].state)==LIFE_IDLE) {
        w->terminate[n].pid=pid;w->terminate[n].result=0;w->terminate[n].elapsed=0;
        life_queue(w,&w->terminate[n].state,LIFE_QUEUED);return 1;
    }
    return 0; /* UI retains the client and retries; never fall back to kill. */
}
static int life_reap_pending(struct lifecycle_worker *w,uint64_t pid) {
    for(unsigned n=0;n<LIFE_REAP_SLOTS;n++)if(life_state(&w->reap[n].state)!=LIFE_IDLE && w->reap[n].pid==pid)return 1;
    return 0;
}
/* Admit only after read-only process metadata reports exit. That is not proof
 * of final kernel retirement: the worker's blocking wait owns that check/reap.
 * An occupied slot retains its PID until the UI consumes the result once. */
static int life_reap_submit(struct lifecycle_worker *w,uint64_t pid) {
    if(!pid || life_state(&w->stop) || life_reap_pending(w,pid))return 0;
    for(unsigned n=0;n<LIFE_REAP_SLOTS;n++)if(life_state(&w->reap[n].state)==LIFE_IDLE) {
        w->reap[n].pid=pid;w->reap[n].status=0;w->reap[n].result=0;
        w->reap[n].queued_us=(uint64_t)srv_monotonic_us();w->reap[n].begin_us=w->reap[n].end_us=0;
        life_queue(w,&w->reap[n].state,LIFE_QUEUED);return 1;
    }
    return 0;
}
static int life_reap_take(struct lifecycle_worker *w,uint64_t pid,long *result,uint64_t *status) {
    for(unsigned n=0;n<LIFE_REAP_SLOTS;n++)if(life_state(&w->reap[n].state)==LIFE_DONE && w->reap[n].pid==pid) {
        *result=w->reap[n].result;*status=w->reap[n].status;
        life_publish(&w->reap[n].state,LIFE_IDLE);return 1;
    }
    return 0;
}
static int life_prepare_submit(struct lifecycle_worker *w,struct desk_capture_job *j,unsigned width,unsigned height,const char *path) {
    if(life_state(&w->stop) || life_state(&w->prepare.state)!=LIFE_IDLE ||
       life_state(&w->capture.state)!=LIFE_IDLE || j->busy || j->retiring_pid || !path || strlen(path)>=sizeof(w->prepare.path))return 0;
    w->prepare.job=*j;w->prepare.width=width;w->prepare.height=height;
    strcpy(w->prepare.path,path);w->prepare.result=-1;
    /* The reserved UI record owns no handles/pixels until take publishes the
     * worker result. Never poll, clean up or reuse this placeholder. */
    j->busy=1;life_queue(w,&w->prepare.state,LIFE_QUEUED);return 1;
}
static int life_prepare_take(struct lifecycle_worker *w,struct desk_capture_job *j,int *result) {
    if(life_state(&w->prepare.state)!=LIFE_DONE)return 0;
    *j=w->prepare.job;*result=w->prepare.result;
    life_publish(&w->prepare.state,LIFE_IDLE);return 1;
}
/* One fixed slot: never overwrite a snapshot while the worker is emitting it.
 * A busy slot causes a retry next loop; only the UI owns the live accumulator. */
static int life_report_submit(struct lifecycle_worker *w,const struct loop_trace *trace,const struct operation_trace *operations) {
    if(!trace->samples || life_state(&w->report_state)!=LIFE_IDLE)return 0;
    w->report=*trace;w->operations=*operations;life_publish(&w->report_state,LIFE_QUEUED);life_notify(&w->report_event);return 1;
}
static int life_capture_submit(struct lifecycle_worker *w,struct desk_capture_job *j,int result) {
    if(!result || life_state(&w->capture.state)!=LIFE_IDLE)return 0;
    w->capture.job=*j;w->capture.result=result;
    /* Transfer every resource exactly once; UI retains diagnostic scalars only.
     * Busy stays set until the worker publishes full resource retirement. */
    j->reader=j->writer=-1;j->pixels=NULL;j->retiring_pid=j->pid?j->pid:j->retiring_pid;j->pid=0;
    life_queue(w,&w->capture.state,LIFE_QUEUED);return 1;
}
static int life_capture_take(struct lifecycle_worker *w,struct desk_capture_job *j) {
    if(life_state(&w->capture.state)!=LIFE_DONE)return 0;
    *j=w->capture.job;int result=w->capture.result;
    life_publish(&w->capture.state,LIFE_IDLE);return result;
}
static void life_capture_work(struct lifecycle_worker *w) {
    struct life_capture *c=&w->capture;
    if(life_state(&c->state)!=LIFE_QUEUED)return;
    if(c->job.busy)desk_capture_job_close(&c->job);
    if(c->job.retiring_pid) {
        uint64_t at=(uint64_t)srv_monotonic_us();
        (void)desk_capture_job_poll(&c->job);
        uint64_t elapsed=(uint64_t)srv_monotonic_us()-at;
        c->job.reap_us+=elapsed;c->job.cleanup_us+=elapsed;
    }
    if(!c->job.retiring_pid)life_publish(&c->state,LIFE_DONE);
}
static int life_reserved(const struct lifecycle_worker *w,int slot) {
    if(w)for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++)if(life_state(&w->launch[n].state) && w->launch[n].child_slot==slot)return 1;
    return 0;
}
static int life_log_submit(struct lifecycle_worker *w,const char *text) {
    unsigned at=w->log_write;
    if(life_state(&w->logs[at].state)!=LIFE_IDLE || strlen(text)>=LIFE_LOG_BYTES) {
        w->log_dropped++;return 0;
    }
    strcpy(w->logs[at].text,text);life_publish(&w->logs[at].state,LIFE_QUEUED);
    w->log_write=(at+1)%LIFE_LOG_SLOTS;life_notify(&w->report_event);return 1;
}
static void life_log_work(struct lifecycle_worker *w) {
    for(unsigned n=0;n<LIFE_LOG_SLOTS;n++) {
        unsigned at=w->log_read;
        if(life_state(&w->logs[at].state)!=LIFE_QUEUED)break;
        srv_puts(w->logs[at].text);
        life_publish(&w->logs[at].state,LIFE_IDLE);w->log_read=(at+1)%LIFE_LOG_SLOTS;
    }
}
static void life_report_work(struct lifecycle_worker *w) {
    life_log_work(w);
    if(life_state(&w->report_state)==LIFE_QUEUED) {
        loop_trace_report(&w->report,0);operation_trace_report(&w->operations,0);life_publish(&w->report_state,LIFE_IDLE);
    }
}
static void life_work(struct lifecycle_worker *w) {
    for(unsigned n=0;n<LIFE_TERMINATE_SLOTS;n++)if(life_state(&w->terminate[n].state)==LIFE_QUEUED) {
        struct life_terminate *t=&w->terminate[n];
        uint64_t begin=(uint64_t)srv_monotonic_us();t->result=srv_kill(t->pid);
        t->elapsed=(uint64_t)srv_monotonic_us()-begin;
        life_terminate_log(t->pid,t->elapsed,t->result);
        life_publish(&t->state,LIFE_DONE);
    }
    for(unsigned n=0;n<LIFE_REAP_SLOTS;n++)if(life_state(&w->reap[n].state)==LIFE_QUEUED) {
        struct life_reap *r=&w->reap[n];
        r->begin_us=(uint64_t)srv_monotonic_us();life_publish(&r->state,LIFE_RUNNING);
        r->result=srv_wait(r->pid,&r->status,0);
        r->end_us=(uint64_t)srv_monotonic_us();
        life_publish(&r->state,LIFE_DONE);
    }
    if(life_state(&w->prepare.state)==LIFE_QUEUED) {
        struct life_prepare *p=&w->prepare;
        p->result=desk_capture_job_prepare(&p->job,p->width,p->height,p->path);
        life_publish(&p->state,LIFE_DONE);
    }
    life_capture_work(w);
    for(unsigned n=0;n<LIFE_MEMORY_SLOTS;n++) {
        struct life_memory *m=&w->memory[n];
        if(life_state(&m->state)==LIFE_QUEUED) {
            uint64_t begin=(uint64_t)srv_monotonic_us();
            m->pixels=desk_pixels_alloc(m->width,m->height);
            m->elapsed=(uint64_t)srv_monotonic_us()-begin;
            life_publish(&m->state,LIFE_DONE);
        }
        if(life_state(&m->state)==LIFE_DONE && life_state(&m->discard)) {
            desk_pixels_free(m->pixels,m->width,m->height);m->pixels=NULL;
            life_publish(&m->state,LIFE_RETIRED);
        }
    }
    for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++) {
        struct life_launch *j=&w->launch[n];
        if(life_state(&j->state)!=LIFE_QUEUED)continue;
        const struct desk_application_info *info=desk_application_info(j->application);
        char *argv[]={(char *)info->program,(char *)info->role,j->path,NULL};
        char home[192],desktop_home[192];
        snprintf(home,sizeof(home),"HOME=%s",j->home);
        snprintf(desktop_home,sizeof(desktop_home),"SRVROS_DESKTOP_HOME=%s",j->home);
        char *env[]={"PATH=/fat/bin:/:/fat",home,desktop_home,NULL};
        struct srv_exec_request r={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
            .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
        j->begin_us=(uint64_t)srv_monotonic_us();j->pid=srv_exec(&r);
        j->end_us=(uint64_t)srv_monotonic_us();j->elapsed=j->end_us-j->begin_us;
        /* Live UART backpressure must stay off the compositor input loop.
         * Keep ownership until this immutable launch receipt is written. */
        life_launch_log(j->application,j->pid,j->elapsed);
        life_publish(&j->state,LIFE_DONE);
    }
}
static void *life_thread(void *arg) {
    struct lifecycle_worker *w=arg;
    for(;;) {
        life_clear(&w->work_event);
        if(life_state(&w->stop))break;
        life_work(w);
        /* An admitted screenshot still owns its child/resources. Preserve the
         * existing retry cadence until retirement; empty queues need no timer. */
        if(life_state(&w->capture.state)==LIFE_QUEUED)srv_sleep_ticks(1);
        else life_wait(&w->work_event);
    }
    /* Shutdown joins before stack/session storage or mappings are released. */
    do {life_work(w);if(life_state(&w->capture.state)==LIFE_QUEUED)srv_sleep_ticks(1);}
    while(life_state(&w->capture.state)==LIFE_QUEUED);
    return NULL;
}
static void *life_report_thread(void *arg) {
    struct lifecycle_worker *w=arg;
    for(;;) {
        life_clear(&w->report_event);
        if(life_state(&w->report_stop))break;
        life_report_work(w);life_wait(&w->report_event);
    }
    life_report_work(w);return NULL;
}
static int life_start(struct lifecycle_worker *w,int diagnostics) {
    if(diagnostics) {
        if(pthread_create(&w->report_thread,NULL,life_report_thread,w))return -1;
        w->report_started=1;
    }
    int result=pthread_create(&w->thread,NULL,life_thread,w);
    if(result && w->report_started){life_publish(&w->report_stop,1);life_notify(&w->report_event);pthread_join(w->report_thread,NULL);w->report_started=0;}
    return result;
}
static int life_stop(struct lifecycle_worker *w) {
    life_queue(w,&w->stop,1);int result=pthread_join(w->thread,NULL);if(result)return result;
    if(w->report_started){life_publish(&w->report_stop,1);life_notify(&w->report_event);return pthread_join(w->report_thread,NULL);}
    return 0;
}
#endif
