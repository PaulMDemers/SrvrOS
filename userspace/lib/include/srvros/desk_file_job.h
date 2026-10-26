#ifndef SRVROS_DESK_FILE_JOB_H
#define SRVROS_DESK_FILE_JOB_H
#include <srvros/desktop_file.h>
#include <srvros/gui_wait.h>
#include <stddef.h>
#define DESK_FILE_POOL_MAX_CHANNELS 32u
struct desk_file_pool;
struct desk_file_channel;
struct desk_file_job {
    uint64_t pid;
    int reader,writer,busy,broken;
    uint32_t next_id;
    struct desk_file_request request;
    struct desk_file_response response;
    size_t sent,received;
    unsigned char outgoing[DESK_FILE_BYTES],incoming[DESK_FILE_BYTES];
    struct desk_file_pool *pool;
    struct desk_file_channel *channel;
    struct desk_file_job *queued_next;
};
/* Single-dispatcher, caller-owned service. Channels own workers; bound jobs own
 * immutable requests/results. Do not move/free either while bound. No threads,
 * callbacks or allocation; spawn only when a request needs an idle transport. */
struct desk_file_channel {
    uint64_t pid;
    int reader,writer,retiring;
    struct desk_file_job *owner;
};
struct desk_file_pool {
    struct desk_file_channel *channels;
    unsigned count,bound;
    uint32_t next_id;
    int closing;
    struct desk_file_job *head,*tail;
    uint64_t spawned,reaped,live,peak;
};
/* Channel count is an explicit concurrency budget (1..32), independent of jobs. */
int desk_file_pool_init(struct desk_file_pool *pool,struct desk_file_channel *channels,unsigned count);
/* Job must be zero-initialized or detached/closed, and idle. */
int desk_file_job_bind(struct desk_file_job *job,struct desk_file_pool *pool);
/* Refuse while accepted work is pending. Does not terminate a worker. */
int desk_file_job_detach(struct desk_file_job *job);
/* -1 bound jobs remain; 0 waiting for child exit; 1 fully retired. No kill. */
int desk_file_pool_shutdown(struct desk_file_pool *pool);
/* 1 safe to wait on the returned interests, 0 dispatch/retirement needs a short
 * retry, -1 invalid/capacity. No I/O or worker polling. Sets only count/fds. */
int desk_file_pool_interests(const struct desk_file_pool *pool,struct srv_gui_wait *request);
int desk_file_job_init(struct desk_file_job *job);
int desk_file_job_start(struct desk_file_job *job,unsigned operation,const char *path,
    const void *data,size_t length,unsigned flags,uint32_t expected_crc);
/* Advance request transmission without consuming a reply. 1 fully sent,
 * 0 pending/idle, -1 failure. The job stays busy until poll accepts its reply. */
int desk_file_job_submit(struct desk_file_job *job);
/* 1 completed response, 0 pending/idle, -1 worker or protocol failure. */
int desk_file_job_poll(struct desk_file_job *job);
/* Legacy close is best-effort; explicit application lifetimes should retire.
 * For pooled jobs, busy close is refused by retaining the binding. Prefer the
 * checked detach API before releasing caller storage. */
void desk_file_job_close(struct desk_file_job *job);
/* Explicit idle-worker retirement: 1 drained/reaped,0 pending,-1 invalid.
 * Retains PID across nonblocking waits; never kills an accepted mutation.
 * Keep polling accepted work before retirement; do not reuse the job meanwhile. */
int desk_file_job_retire(struct desk_file_job *job);
#endif
