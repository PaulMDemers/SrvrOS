#ifndef DESKD_CAPTURE_WORKER_H
#define DESKD_CAPTURE_WORKER_H
#include <pthread.h>
#include <srvros/desk_capture_job.h>
#include <srvros/sys.h>
#include <string.h>

/* One owner and one UI producer/consumer. The owner never touches the job in
 * PREPARED: that state exclusively lends its snapshot mapping to the UI. Once
 * published, no canvas pointer or mutable job crosses into the input loop. */
enum capture_owner_phase {
    CAPTURE_OWNER_IDLE, CAPTURE_OWNER_PREPARING, CAPTURE_OWNER_PREPARED,
    CAPTURE_OWNER_SNAPSHOTTING, CAPTURE_OWNER_TRANSFERRING, CAPTURE_OWNER_RETIRING, CAPTURE_OWNER_DONE
};
struct capture_worker {
    pthread_t thread;
    unsigned state,event,stop,cancel,view_state,width,height;
    int result;
    uint64_t child_pid;
    char path[DESK_CAPTURE_PATH];
    struct desk_capture_job job,view;
};
static unsigned capture_owner_state(const struct capture_worker *w) {
    return __atomic_load_n(&w->state,__ATOMIC_ACQUIRE);
}
static void capture_owner_publish(struct capture_worker *w,unsigned state) {
    __atomic_store_n(&w->state,state,__ATOMIC_RELEASE);
}
static void capture_owner_notify(struct capture_worker *w) {
    if(!__atomic_exchange_n(&w->event,1,__ATOMIC_RELEASE))
        (void)srv_futex_wake((uint32_t *)&w->event,1);
}
static void capture_owner_view(struct desk_capture_job *out,const struct desk_capture_job *job) {
    *out=*job;
    /* PID fields are diagnostic identities only in the UI view. */
    out->reader=out->writer=-1;out->pixels=NULL;
}
static void capture_owner_report(struct capture_worker *w) {
    if(__atomic_load_n(&w->view_state,__ATOMIC_ACQUIRE))return;
    capture_owner_view(&w->view,&w->job);w->view.busy=1;
    __atomic_store_n(&w->view_state,1,__ATOMIC_RELEASE);
}
static int capture_owner_cancelled(const struct capture_worker *w) {
    return __atomic_load_n(&w->cancel,__ATOMIC_ACQUIRE)!=0;
}
static void capture_owner_abort(struct capture_worker *w) {
    if(!w->job.finished_us) {
        long now=srv_monotonic_us();w->job.finished_us=now>0?(uint64_t)now:0;
    }
    w->result=-1;capture_owner_publish(w,CAPTURE_OWNER_RETIRING);
}
static void capture_owner_step(struct capture_worker *w) {
    unsigned state=capture_owner_state(w);
    if(state==CAPTURE_OWNER_PREPARING) {
        w->result=desk_capture_job_prepare(&w->job,w->width,w->height,w->path);
        w->child_pid=w->job.pid;
        if(w->result || capture_owner_cancelled(w))capture_owner_abort(w);
        else {capture_owner_report(w);capture_owner_publish(w,CAPTURE_OWNER_PREPARED);}
        return;
    }
    if(state==CAPTURE_OWNER_PREPARED) {
        if(capture_owner_cancelled(w)) {
            unsigned expected=CAPTURE_OWNER_PREPARED;
            if(__atomic_compare_exchange_n(&w->state,&expected,CAPTURE_OWNER_RETIRING,0,
                __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))capture_owner_abort(w);
        }
        return;
    }
    if(state==CAPTURE_OWNER_TRANSFERRING) {
        size_t total=sizeof(w->job.request)+desk_capture_bytes(w->job.request.width,w->job.request.height);
        if(capture_owner_cancelled(w) && w->job.sent<total) {capture_owner_abort(w);return;}
        size_t sent=w->job.sent,received=w->job.received;
        unsigned frames=w->job.progress_frames,allocated=w->job.transfer_ready_received;
        int result=desk_capture_job_poll_transfer(&w->job);
        int progressed=w->job.sent!=sent || w->job.received!=received ||
            w->job.progress_frames!=frames || w->job.transfer_ready_received!=allocated;
        capture_owner_report(w);
        if(result) {w->result=result;capture_owner_publish(w,CAPTURE_OWNER_RETIRING);return;}
        struct srv_pollfd fds[2]={{.fd=w->job.reader,.events=SRV_POLLIN}};
        unsigned count=1;
        if(w->job.sent<total)fds[count++]=(struct srv_pollfd){.fd=w->job.writer,.events=SRV_POLLOUT};
        /* One response read and at most four4096-byte writes per admission.
         * Ready pipes can admit another turn without waiting for a UI frame.
         * The bounded wait also observes cancel/stop without an infinite poll. */
        if(!progressed) {
            /* POLLOUT promises some free pipe space, not enough for the next
             * atomic write. A partial header can leave fewer than4096 free
             * bytes, so repeating a ready hint after AGAIN would busy-spin.
             * Probe immediate readiness only after no forward progress. A
             * positive hint backs off one existing tick; a genuinely blocking
             * readiness wake below does not acquire another artificial delay. */
            long immediate=srv_poll(fds,count,0);
            if((immediate<0 && immediate!=SRV_ERR_INTR) || (fds[0].revents&SRV_POLLNVAL) ||
               (count==2 && (fds[1].revents&SRV_POLLNVAL))) {capture_owner_abort(w);return;}
            if(immediate==SRV_ERR_INTR)return;
            if(immediate>0) {srv_sleep_ticks(1);return;}
        }
        long ready=srv_poll(fds,count,10);
        if((ready<0 && ready!=SRV_ERR_INTR) || (fds[0].revents&SRV_POLLNVAL) ||
           (count==2 && (fds[1].revents&SRV_POLLNVAL)))capture_owner_abort(w);
        return;
    }
    if(state==CAPTURE_OWNER_RETIRING) {
        if(w->job.busy || w->job.pixels || w->job.reader>=0 || w->job.writer>=0)
            desk_capture_job_close(&w->job);
        if(w->job.retiring_pid) {
            uint64_t at=(uint64_t)srv_monotonic_us();
            (void)desk_capture_job_poll(&w->job);
            uint64_t elapsed=(uint64_t)srv_monotonic_us()-at;
            w->job.reap_us+=elapsed;w->job.cleanup_us+=elapsed;
        }
        capture_owner_report(w);
        if(!w->job.retiring_pid)capture_owner_publish(w,CAPTURE_OWNER_DONE);
        else srv_sleep_ticks(1);
    }
}
static void *capture_owner_thread(void *context) {
    struct capture_worker *w=context;
    for(;;) {
        (void)__atomic_exchange_n(&w->event,0,__ATOMIC_ACQUIRE);
        unsigned state=capture_owner_state(w);
        int stop=__atomic_load_n(&w->stop,__ATOMIC_ACQUIRE)!=0;
        if(stop && (state==CAPTURE_OWNER_IDLE || state==CAPTURE_OWNER_DONE))return NULL;
        capture_owner_step(w);
        state=capture_owner_state(w);
        if(__atomic_load_n(&w->stop,__ATOMIC_ACQUIRE) &&
           (state==CAPTURE_OWNER_IDLE || state==CAPTURE_OWNER_DONE))return NULL;
        if(state==CAPTURE_OWNER_IDLE || state==CAPTURE_OWNER_PREPARED || state==CAPTURE_OWNER_SNAPSHOTTING || state==CAPTURE_OWNER_DONE) {
            long result=srv_futex_wait((uint32_t *)&w->event,0,0);
            if(result<0 && !__atomic_load_n(&w->event,__ATOMIC_ACQUIRE))srv_sleep_ticks(1);
        }
    }
}
static int capture_owner_start(struct capture_worker *w) {
    *w=(struct capture_worker){0};desk_capture_job_init(&w->job);
    return pthread_create(&w->thread,NULL,capture_owner_thread,w);
}
static int capture_owner_submit(struct capture_worker *w,struct desk_capture_job *view,
    unsigned width,unsigned height,const char *path) {
    if(__atomic_load_n(&w->stop,__ATOMIC_ACQUIRE) || capture_owner_state(w)!=CAPTURE_OWNER_IDLE ||
       view->busy || view->retiring_pid || !path || strlen(path)>=sizeof(w->path))return 0;
    w->job=*view;w->width=width;w->height=height;strcpy(w->path,path);
    w->child_pid=0;w->result=0;
    __atomic_store_n(&w->cancel,0,__ATOMIC_RELEASE);
    __atomic_store_n(&w->view_state,0,__ATOMIC_RELEASE);
    view->busy=1;capture_owner_publish(w,CAPTURE_OWNER_PREPARING);capture_owner_notify(w);return 1;
}
static int capture_owner_snapshot(struct capture_worker *w,struct desk_capture_job *view,
    const uint32_t *pixels,unsigned width,unsigned height) {
    unsigned expected=CAPTURE_OWNER_PREPARED;
    if(!__atomic_compare_exchange_n(&w->state,&expected,CAPTURE_OWNER_SNAPSHOTTING,0,
        __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    int result=desk_capture_job_snapshot(&w->job,pixels,width,height);
    capture_owner_view(view,&w->job);
    if(result)capture_owner_abort(w);
    else capture_owner_publish(w,CAPTURE_OWNER_TRANSFERRING);
    capture_owner_notify(w);return result?-1:1;
}
static int capture_owner_poll(struct capture_worker *w,struct desk_capture_job *view,uint64_t *pid) {
    if(__atomic_load_n(&w->view_state,__ATOMIC_ACQUIRE)) {
        *view=w->view;
        __atomic_store_n(&w->view_state,0,__ATOMIC_RELEASE);
    }
    if(capture_owner_state(w)!=CAPTURE_OWNER_DONE)return 0;
    *view=w->job;*pid=w->child_pid;int result=w->result;
    __atomic_store_n(&w->view_state,0,__ATOMIC_RELEASE);
    capture_owner_publish(w,CAPTURE_OWNER_IDLE);return result;
}
static void capture_owner_cancel(struct capture_worker *w) {
    __atomic_store_n(&w->cancel,1,__ATOMIC_RELEASE);capture_owner_notify(w);
}
static int capture_owner_stop(struct capture_worker *w) {
    __atomic_store_n(&w->stop,1,__ATOMIC_RELEASE);capture_owner_cancel(w);
    return pthread_join(w->thread,NULL);
}
#endif
