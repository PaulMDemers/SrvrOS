#ifndef SRVROS_DESK_CAPTURE_JOB_H
#define SRVROS_DESK_CAPTURE_JOB_H
#include <srvros/desktop_capture.h>
#include <stddef.h>
struct desk_capture_job {
    uint64_t pid,retiring_pid;
    int reader,writer,busy;
    uint32_t next_id;
    struct desk_capture_request request;
    struct desk_capture_response response;
    size_t sent,received;
    uint32_t *pixels;
    uint64_t started_us,finished_us,last_progress_us,progress_bytes,phase_us[CAPTURE_PHASE_COUNT];
    unsigned phase,progress_frames;
    uint64_t cleanup_us,close_us,unmap_us,reap_us;
};
void desk_capture_job_init(struct desk_capture_job *job);
/* Takes an owned snapshot, never retains the caller's canvas. */
int desk_capture_job_start(struct desk_capture_job *job,const uint32_t *pixels,uint32_t width,uint32_t height,const char *path);
/* 1 validated completion (inspect status), 0 pending/idle, -1 transport failure. */
int desk_capture_job_poll(struct desk_capture_job *job);
/* Transfer/validate only. Completion retains busy/resources for an owned cleanup
 * worker; do not poll again until that owner has closed and published the job. */
int desk_capture_job_poll_transfer(struct desk_capture_job *job);
/* Closes pipes without killing a worker that may have accepted a file write. */
void desk_capture_job_close(struct desk_capture_job *job);
#endif
