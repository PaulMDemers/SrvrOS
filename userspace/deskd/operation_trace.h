#ifndef DESKD_OPERATION_TRACE_H
#define DESKD_OPERATION_TRACE_H
#include <srvros/desk_capture_job.h>
/* Immutable, allocation-free diagnostic snapshots. UI owns collection; the
 * lifecycle worker owns output. No process pointers or mutable jobs escape. */
struct capture_trace {
    uint64_t pid,retiring,sent,total,received,age_us,progress_age_us,bytes,started_us,finished_us;
    uint64_t phases[CAPTURE_PHASE_COUNT];
    uint64_t cleanup_us,close_us,unmap_us,reap_us;unsigned cleanup_pending;
    struct desk_capture_transfer_timing transfer;
    unsigned id,busy,pending,phase,frames,finished,status;
};
/* Only the first three terminal captures are retained. Further ordinary captures
 * continue normally, but a bounded stress proof must reject dropped history.
 * PID is copied before resource ownership transfer; no live job is retained. */
#define CAPTURE_HISTORY_ROWS 3u
struct capture_completion {
    uint64_t pid,started_us,finished_us,retired_us,phases[CAPTURE_PHASE_COUNT];
    unsigned id,status;
    int result;
    struct desk_capture_transfer_timing transfer;
    unsigned progress_frames;
};
struct capture_history {
    struct capture_completion rows[CAPTURE_HISTORY_ROWS];
    uint64_t completed,dropped,invalid;
    unsigned count,last_id;
};
static inline void capture_history_record(struct capture_history *h,const struct capture_trace *c,
    uint64_t pid,uint64_t retired,int result) {
    if(h->completed==UINT64_MAX)h->invalid=1;else h->completed++;
    if(!c->id || c->id<=h->last_id || !pid || !c->started_us ||
       c->busy || c->retiring || c->cleanup_pending ||
       c->finished_us<c->started_us || retired<c->finished_us ||
       (result!=1 && result!=-1) || c->status>DESK_CAPTURE_EXISTS)h->invalid=1;
    uint64_t total=0;
    for(unsigned i=1;i<CAPTURE_PHASE_COUNT;i++) {
        if(total>UINT64_MAX-c->phases[i])h->invalid=1;
        else total+=c->phases[i];
    }
    if(c->finished_us>=c->started_us && total>c->finished_us-c->started_us)h->invalid=1;
    h->last_id=c->id;
    if(h->count==CAPTURE_HISTORY_ROWS) {
        if(h->dropped==UINT64_MAX)h->invalid=1;else h->dropped++;
        return;
    }
    struct capture_completion *q=&h->rows[h->count++];
    *q=(struct capture_completion){.id=c->id,.pid=pid,.started_us=c->started_us,
        .finished_us=c->finished_us,.retired_us=retired,.status=c->status,.result=result};
    memcpy(q->phases,c->phases,sizeof(q->phases));
    q->transfer=c->transfer;q->progress_frames=c->frames;
}
/* Called by the existing immutable report owner. The UI's small line queue is
 * not used: complete companions must fit the existing atomic console writer. */
static void capture_transfer_completion_report(const struct capture_completion *c,
    unsigned index,uint64_t at,unsigned final) {
    const struct desk_capture_transfer_timing *t=&c->transfer;
    /* Scalars remain immutable in history until final publication. Repeating
     * three rows per capture in every checkpoint needlessly evicts app events. */
    if(!t->enabled || !final)return;
    char text[1024];int length;
#define CAPTURE_TRANSFER_KEYS "version=1 snapshot_us=%llu index=%u id=%u pid=%llu started_us=%llu finished_us=%llu retired_us=%llu"
#define CAPTURE_TRANSFER_ARGS (unsigned long long)at,index,c->id,(unsigned long long)c->pid,(unsigned long long)c->started_us,(unsigned long long)c->finished_us,(unsigned long long)c->retired_us
#define CAPTURE_TRANSFER_U(v) (unsigned long long)(v)
    length=snprintf(text,sizeof(text),"desktop-capture-transfer: " CAPTURE_TRANSFER_KEYS " enabled=%llu valid=%llu invalid=%llu total=%llu polls=%llu budget_polls=%llu begin_us=%llu end_us=%llu poll_us=%llu gap_us=%llu gap_max_us=%llu final=%u\n",
        CAPTURE_TRANSFER_ARGS,CAPTURE_TRANSFER_U(t->enabled),CAPTURE_TRANSFER_U(t->valid),CAPTURE_TRANSFER_U(t->invalid),CAPTURE_TRANSFER_U(t->total),CAPTURE_TRANSFER_U(t->polls),CAPTURE_TRANSFER_U(t->budget_polls),CAPTURE_TRANSFER_U(t->begin_us),CAPTURE_TRANSFER_U(t->end_us),CAPTURE_TRANSFER_U(t->poll_us),CAPTURE_TRANSFER_U(t->gap_us),CAPTURE_TRANSFER_U(t->gap_max_us),final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
    length=snprintf(text,sizeof(text),"desktop-capture-transfer-write: " CAPTURE_TRANSFER_KEYS " write_calls=%llu write_ok=%llu write_bytes=%llu short_writes=%llu write_again=%llu write_intr=%llu write_failed=%llu write_first_us=%llu write_last_us=%llu write_us=%llu write_gap_us=%llu write_gap_max_us=%llu request_done_us=%llu pixels_done_us=%llu final=%u\n",
        CAPTURE_TRANSFER_ARGS,CAPTURE_TRANSFER_U(t->write_calls),CAPTURE_TRANSFER_U(t->write_ok),CAPTURE_TRANSFER_U(t->write_bytes),CAPTURE_TRANSFER_U(t->short_writes),CAPTURE_TRANSFER_U(t->write_again),CAPTURE_TRANSFER_U(t->write_intr),CAPTURE_TRANSFER_U(t->write_failed),CAPTURE_TRANSFER_U(t->write_first_us),CAPTURE_TRANSFER_U(t->write_last_us),CAPTURE_TRANSFER_U(t->write_us),CAPTURE_TRANSFER_U(t->write_gap_us),CAPTURE_TRANSFER_U(t->write_gap_max_us),CAPTURE_TRANSFER_U(t->request_done_us),CAPTURE_TRANSFER_U(t->pixels_done_us),final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
    length=snprintf(text,sizeof(text),"desktop-capture-transfer-read: " CAPTURE_TRANSFER_KEYS " read_calls=%llu read_bytes=%llu read_again=%llu read_intr=%llu read_failed=%llu progress_frames=%u worker_receive_us=%llu worker_allocated_us=%llu worker_read_done_us=%llu worker_final_us=%llu final=%u\n",
        CAPTURE_TRANSFER_ARGS,CAPTURE_TRANSFER_U(t->read_calls),CAPTURE_TRANSFER_U(t->read_bytes),CAPTURE_TRANSFER_U(t->read_again),CAPTURE_TRANSFER_U(t->read_intr),CAPTURE_TRANSFER_U(t->read_failed),c->progress_frames,CAPTURE_TRANSFER_U(t->worker_receive_us),CAPTURE_TRANSFER_U(t->worker_allocated_us),CAPTURE_TRANSFER_U(t->worker_read_done_us),CAPTURE_TRANSFER_U(t->worker_final_us),final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
#undef CAPTURE_TRANSFER_U
#undef CAPTURE_TRANSFER_ARGS
#undef CAPTURE_TRANSFER_KEYS
}
static void capture_history_report(const struct capture_history *h,uint64_t at,unsigned final) {
    desk_log("desktop-capture-history: version=1 snapshot_us=%llu completed=%llu count=%u dropped=%llu invalid=%llu final=%u\n",
        (unsigned long long)at,(unsigned long long)h->completed,h->count,
        (unsigned long long)h->dropped,(unsigned long long)h->invalid,final);
    for(unsigned i=0;i<h->count && i<CAPTURE_HISTORY_ROWS;i++) {
        const struct capture_completion *c=&h->rows[i];
        desk_log("desktop-capture-completion: version=1 snapshot_us=%llu index=%u id=%u pid=%llu started_us=%llu finished_us=%llu retired_us=%llu result=%d status=%u p1_us=%llu p2_us=%llu p3_us=%llu p4_us=%llu p5_us=%llu p6_us=%llu p7_us=%llu p8_us=%llu p9_us=%llu p10_us=%llu p11_us=%llu p12_us=%llu final=%u\n",
            (unsigned long long)at,i,c->id,(unsigned long long)c->pid,
            (unsigned long long)c->started_us,(unsigned long long)c->finished_us,(unsigned long long)c->retired_us,c->result,c->status,
            (unsigned long long)c->phases[1],(unsigned long long)c->phases[2],(unsigned long long)c->phases[3],
            (unsigned long long)c->phases[4],(unsigned long long)c->phases[5],(unsigned long long)c->phases[6],
            (unsigned long long)c->phases[7],(unsigned long long)c->phases[8],(unsigned long long)c->phases[9],
            (unsigned long long)c->phases[10],(unsigned long long)c->phases[11],(unsigned long long)c->phases[12],final);
        capture_transfer_completion_report(c,i,at,final);
    }
}
/* Absolute observation clocks; zero means not observed, never zero cost.
 * UI retains only the current cycle and two immutable worst-case copies. */
struct lifetime_trace {
    uint64_t pid,cycle,begin_us,launch_begin_us,launch_end_us,ready_us;
    uint64_t close_request_us,visual_us,exit_seen_us,reap_queued_us,reap_begin_us,reap_end_us,closed_us;
};
static void lifetime_trace_report(const struct lifetime_trace *q,const char *kind,uint64_t snapshot,unsigned final) {
    if(!q->pid)return;
    char text[1024];
    int length=snprintf(text,sizeof(text),"desktop-life-worst: version=1 snapshot_us=%llu kind=%s pid=%llu cycle=%llu begin_us=%llu launch_begin_us=%llu launch_end_us=%llu ready_us=%llu close_request_us=%llu visual_us=%llu exit_seen_us=%llu reap_queued_us=%llu reap_begin_us=%llu reap_end_us=%llu closed_us=%llu final=%u\n",
        (unsigned long long)snapshot,kind,(unsigned long long)q->pid,(unsigned long long)q->cycle,
        (unsigned long long)q->begin_us,(unsigned long long)q->launch_begin_us,(unsigned long long)q->launch_end_us,(unsigned long long)q->ready_us,
        (unsigned long long)q->close_request_us,(unsigned long long)q->visual_us,(unsigned long long)q->exit_seen_us,
        (unsigned long long)q->reap_queued_us,(unsigned long long)q->reap_begin_us,(unsigned long long)q->reap_end_us,(unsigned long long)q->closed_us,final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
}
struct message_trace {
    uint64_t begin,received,end,pid,type;
    int result;
    struct srv_sched_trace scheduler;
};
struct operation_trace {
    struct message_trace message;
    struct lifetime_trace ready_worst,close_worst;
    uint64_t at_us,launch_age_us,exit_age_us,close_age_us,close_pid,close_window;
    uint64_t requests,started,refused,last_pid,last_exec_us;
    uint64_t cache_bytes,cache_retained_bytes,cache_reuses;
    unsigned cache_retained;
    uint64_t reap_pid,reap_queue_age_us,reap_run_age_us,reap_completed_us;
    unsigned reap_queued,reap_running,reap_done,reap_state;
    unsigned queued,done,memory,pending_opens,live,awaiting,closing,close_requested,exiting,preferences;
    unsigned oldest_app,oldest_state;
    struct capture_trace capture,slowest;
    struct capture_history capture_history;
};
static void capture_trace_report(const struct capture_trace *c,const char *kind,uint64_t at,unsigned final) {
    desk_log("desktop-capture-state: version=1 snapshot_us=%llu kind=%s id=%u pid=%llu retiring=%llu busy=%u pending=%u sent=%llu total=%llu received=%llu age_us=%llu started_us=%llu finished_us=%llu phase=%u phase_age_us=%llu bytes=%llu frames=%u finished=%u status=%u final=%u\n",
        (unsigned long long)at,kind,c->id,(unsigned long long)c->pid,(unsigned long long)c->retiring,c->busy,c->pending,
        (unsigned long long)c->sent,(unsigned long long)c->total,(unsigned long long)c->received,
        (unsigned long long)c->age_us,(unsigned long long)c->started_us,(unsigned long long)c->finished_us,c->phase,(unsigned long long)c->progress_age_us,(unsigned long long)c->bytes,c->frames,c->finished,c->status,final);
    if(c->id)desk_log("desktop-capture-cleanup: snapshot_us=%llu kind=%s id=%u pending=%u total_us=%llu close_us=%llu unmap_us=%llu reap_us=%llu final=%u\n",(unsigned long long)at,kind,c->id,c->cleanup_pending,(unsigned long long)c->cleanup_us,(unsigned long long)c->close_us,(unsigned long long)c->unmap_us,(unsigned long long)c->reap_us,final);
    if(c->id)desk_log("desktop-capture-phases: snapshot_us=%llu kind=%s id=%u p1_us=%llu p2_us=%llu p3_us=%llu p4_us=%llu p5_us=%llu p6_us=%llu p7_us=%llu p8_us=%llu p9_us=%llu p10_us=%llu p11_us=%llu p12_us=%llu final=%u\n",(unsigned long long)at,kind,c->id,(unsigned long long)c->phases[1],(unsigned long long)c->phases[2],(unsigned long long)c->phases[3],(unsigned long long)c->phases[4],(unsigned long long)c->phases[5],(unsigned long long)c->phases[6],(unsigned long long)c->phases[7],(unsigned long long)c->phases[8],(unsigned long long)c->phases[9],(unsigned long long)c->phases[10],(unsigned long long)c->phases[11],(unsigned long long)c->phases[12],final);
}
static void operation_trace_report(const struct operation_trace *t,unsigned final) {
    if(!t->at_us)return;
    if(t->message.begin) {
        const struct message_trace *m=&t->message;const struct srv_sched_trace *q=&m->scheduler;
        char text[1024];
        int length=snprintf(text,sizeof(text),"desktop-message-worst: version=1 snapshot_us=%llu begin_us=%llu received_us=%llu end_us=%llu pid=%llu type=%llu result=%d blocked_us=%llu ready_us=%llu dispatches=%llu exclusions=%llu invalid=%llu final=%u\n",
            (unsigned long long)t->at_us,(unsigned long long)m->begin,(unsigned long long)m->received,(unsigned long long)m->end,
            (unsigned long long)m->pid,(unsigned long long)m->type,m->result,(unsigned long long)q->blocked_us,
            (unsigned long long)q->ready_us,(unsigned long long)q->dispatches,(unsigned long long)q->exclusions,(unsigned long long)q->invalid,final);
        if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
    }
    lifetime_trace_report(&t->ready_worst,"ready",t->at_us,final);
    lifetime_trace_report(&t->close_worst,"close",t->at_us,final);
    desk_log("desktop-reap-state: version=1 at_us=%llu queued=%u running=%u done=%u pid=%llu state=%u queue_age_us=%llu run_age_us=%llu completed_us=%llu final=%u\n",
        (unsigned long long)t->at_us,t->reap_queued,t->reap_running,t->reap_done,(unsigned long long)t->reap_pid,t->reap_state,
        (unsigned long long)t->reap_queue_age_us,(unsigned long long)t->reap_run_age_us,(unsigned long long)t->reap_completed_us,final);
    desk_log("desktop-cache: version=1 at_us=%llu bytes=%llu retained=%u retained_bytes=%llu reuses=%llu final=%u\n",
        (unsigned long long)t->at_us,(unsigned long long)t->cache_bytes,t->cache_retained,
        (unsigned long long)t->cache_retained_bytes,(unsigned long long)t->cache_reuses,final);
    desk_log("desktop-operations: version=1 at_us=%llu requests=%llu started=%llu refused=%llu last_pid=%llu last_exec_us=%llu queued=%u done=%u oldest_app=%u oldest_state=%u launch_age_us=%llu memory=%u pending_opens=%u live=%u awaiting=%u closing=%u close_requested=%u exiting=%u exit_age_us=%llu close_pid=%llu close_window=%llu close_age_us=%llu preferences=%u final=%u\n",
        (unsigned long long)t->at_us,(unsigned long long)t->requests,(unsigned long long)t->started,(unsigned long long)t->refused,
        (unsigned long long)t->last_pid,(unsigned long long)t->last_exec_us,t->queued,t->done,t->oldest_app,t->oldest_state,
        (unsigned long long)t->launch_age_us,t->memory,t->pending_opens,t->live,t->awaiting,t->closing,t->close_requested,t->exiting,
        (unsigned long long)t->exit_age_us,(unsigned long long)t->close_pid,(unsigned long long)t->close_window,(unsigned long long)t->close_age_us,t->preferences,final);
    capture_trace_report(&t->capture,"current",t->at_us,final);capture_trace_report(&t->slowest,"slowest",t->at_us,final);
    capture_history_report(&t->capture_history,t->at_us,final);
}
#endif
