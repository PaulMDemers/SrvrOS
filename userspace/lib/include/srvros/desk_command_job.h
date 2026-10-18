#ifndef SRVROS_DESK_COMMAND_JOB_H
#define SRVROS_DESK_COMMAND_JOB_H
#include <stddef.h>
#include <stdint.h>
/* Bounded nonblocking output collection for explicitly chosen, read-only tools.
 * No shell parsing: argv is passed directly. stdin is EOF; stdout/stderr merge.
 * Cancel/timeout kills the child, so never use this for a mutating worker.
 * Owner supplies storage and keeps polling to collect a retiring process. */
struct desk_command_job {
    uint64_t pid,retiring_pid,started_us,timeout_us,exit_status;
    int reader,busy,eof,truncated,timed_out,canceled;
    char *output;
    size_t capacity,length;
};
int desk_command_job_init(struct desk_command_job *job,char *output,size_t capacity);
int desk_command_job_start(struct desk_command_job *job,const char *path,char *const argv[],uint64_t now_us,uint64_t timeout_us);
/* At most 16 KiB per turn. 1: EOF plus exit status; -1: transport/timeout error;
 * 0: pending/idle. A nonzero child status is a completed diagnostic, not a pipe
 * failure. Output is NUL-terminated raw bytes; owner validates before display. */
int desk_command_job_poll(struct desk_command_job *job,uint64_t now_us);
void desk_command_job_cancel(struct desk_command_job *job);
void desk_command_job_close(struct desk_command_job *job);
#endif
