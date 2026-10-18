#ifndef SRVROS_DESK_IMAGE_JOB_H
#define SRVROS_DESK_IMAGE_JOB_H
#include <srvros/desktop_image.h>
struct desk_image_job {
    uint64_t pid,retiring_pid,started;
    int reader,writer,busy,ready;
    uint32_t next_id,hash;
    struct desk_image_request request;
    struct desk_image_response response;
    size_t sent,received;
    uint32_t *pixels;
};
/* Initialize without spawning. Each request snapshots its path, and launches a
 * read-only worker lazily. Poll drains at most 64 KiB per turn. */
void desk_image_job_init(struct desk_image_job *job);
int desk_image_job_start(struct desk_image_job *job,const char *path,uint64_t now);
/* 1 complete (inspect response.status), 0 idle/pending, -1 transport/timeout.
 * Keep polling while idle to reap a worker canceled in an earlier turn. */
int desk_image_job_poll(struct desk_image_job *job,uint64_t now);
/* After successful completion, transfer ownership of the pixels to the caller,
 * which releases them with desk_image_pixels_release(). NULL for failure/no completed request. */
uint32_t *desk_image_job_take(struct desk_image_job *job);
/* Release with the dimensions from the completed response. */
void desk_image_pixels_release(uint32_t *pixels,uint32_t width,uint32_t height);
void desk_image_job_cancel(struct desk_image_job *job);
void desk_image_job_close(struct desk_image_job *job);
#endif
