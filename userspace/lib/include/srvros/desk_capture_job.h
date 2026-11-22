#ifndef SRVROS_DESK_CAPTURE_JOB_H
#define SRVROS_DESK_CAPTURE_JOB_H
#include <srvros/desktop_capture.h>
#include <stddef.h>
/* Optional fixed software observations. Clock/counter faults invalidate only
 * these diagnostics; attempt envelopes include AGAIN/INTR/failed calls. */
struct desk_capture_transfer_timing {
    uint64_t enabled,valid,invalid,total,polls,budget_polls;
    uint64_t begin_us,end_us,poll_us,gap_us,gap_max_us;
    uint64_t write_calls,write_ok,write_bytes,short_writes,write_again,write_intr,write_failed;
    uint64_t write_first_us,write_last_us,write_us,write_gap_us,write_gap_max_us;
    uint64_t request_done_us,pixels_done_us;
    uint64_t read_calls,read_bytes,read_again,read_intr,read_failed;
    uint64_t worker_receive_us,worker_allocated_us,worker_read_done_us,worker_final_us;
};
struct desk_capture_job {
    uint64_t pid,retiring_pid;
    int reader,writer,busy,snapshotted;
    uint32_t next_id;
    struct desk_capture_request request;
    struct desk_capture_response response;
    size_t sent,received;
    uint32_t *pixels;
    uint64_t started_us,finished_us,last_progress_us,progress_bytes,phase_us[CAPTURE_PHASE_COUNT];
    unsigned phase,progress_frames;
    uint64_t cleanup_us,close_us,unmap_us,reap_us;
    struct desk_capture_transfer_timing transfer;
    unsigned transfer_ready_received;
};
void desk_capture_job_init(struct desk_capture_job *job);
/* Select before preparation, with no outstanding handles/child/mapping. */
int desk_capture_job_trace(struct desk_capture_job *job,int enabled);
/* Takes an owned snapshot, never retains the caller's canvas. */
int desk_capture_job_start(struct desk_capture_job *job,const uint32_t *pixels,uint32_t width,uint32_t height,const char *path);
/* Worker-side allocation/spawn; the caller owns the returned job. Before any
 * transfer, publish an immutable canvas copy with snapshot(). No canvas pointer
 * is retained while the worker prepares pipes/mappings. */
int desk_capture_job_prepare(struct desk_capture_job *job,uint32_t width,uint32_t height,const char *path);
int desk_capture_job_snapshot(struct desk_capture_job *job,const uint32_t *pixels,uint32_t width,uint32_t height);
/* 1 validated completion (inspect status), 0 pending/idle, -1 transport failure. */
int desk_capture_job_poll(struct desk_capture_job *job);
/* Transfer/validate only. Completion retains busy/resources for an owned cleanup
 * worker; do not poll again until that owner has closed and published the job. */
int desk_capture_job_poll_transfer(struct desk_capture_job *job);
/* Closes pipes without killing a worker that may have accepted a file write. */
void desk_capture_job_close(struct desk_capture_job *job);
#endif
