#ifndef DESKD_OPERATION_TRACE_H
#define DESKD_OPERATION_TRACE_H
/* Immutable, allocation-free diagnostic snapshots. UI owns collection; the
 * lifecycle worker owns output. No process pointers or mutable jobs escape. */
struct capture_trace {
    uint64_t pid,retiring,sent,total,received,age_us,progress_age_us,bytes,started_us,finished_us;
    uint64_t phases[CAPTURE_PHASE_COUNT];
    uint64_t cleanup_us,close_us,unmap_us,reap_us;unsigned cleanup_pending;
    unsigned id,busy,pending,phase,frames,finished,status;
};
struct operation_trace {
    uint64_t at_us,launch_age_us,exit_age_us,close_age_us,close_pid,close_window;
    uint64_t requests,started,refused,last_pid,last_exec_us;
    unsigned queued,done,memory,pending_opens,live,awaiting,closing,close_requested,exiting,preferences;
    unsigned oldest_app,oldest_state;
    struct capture_trace capture,slowest;
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
    desk_log("desktop-operations: version=1 at_us=%llu requests=%llu started=%llu refused=%llu last_pid=%llu last_exec_us=%llu queued=%u done=%u oldest_app=%u oldest_state=%u launch_age_us=%llu memory=%u pending_opens=%u live=%u awaiting=%u closing=%u close_requested=%u exiting=%u exit_age_us=%llu close_pid=%llu close_window=%llu close_age_us=%llu preferences=%u final=%u\n",
        (unsigned long long)t->at_us,(unsigned long long)t->requests,(unsigned long long)t->started,(unsigned long long)t->refused,
        (unsigned long long)t->last_pid,(unsigned long long)t->last_exec_us,t->queued,t->done,t->oldest_app,t->oldest_state,
        (unsigned long long)t->launch_age_us,t->memory,t->pending_opens,t->live,t->awaiting,t->closing,t->close_requested,t->exiting,
        (unsigned long long)t->exit_age_us,(unsigned long long)t->close_pid,(unsigned long long)t->close_window,(unsigned long long)t->close_age_us,t->preferences,final);
    capture_trace_report(&t->capture,"current",t->at_us,final);capture_trace_report(&t->slowest,"slowest",t->at_us,final);
}
#endif
