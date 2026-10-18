#ifndef SRVROS_DESK_FILE_JOB_H
#define SRVROS_DESK_FILE_JOB_H
#include <srvros/desktop_file.h>
#include <stddef.h>
struct desk_file_job {
    uint64_t pid;
    int reader,writer,busy,broken;
    uint32_t next_id;
    struct desk_file_request request;
    struct desk_file_response response;
    size_t sent,received;
    unsigned char outgoing[DESK_FILE_BYTES],incoming[DESK_FILE_BYTES];
};
int desk_file_job_init(struct desk_file_job *job);
int desk_file_job_start(struct desk_file_job *job,unsigned operation,const char *path,
    const void *data,size_t length,unsigned flags,uint32_t expected_crc);
/* 1 completed response, 0 pending/idle, -1 worker or protocol failure. */
int desk_file_job_poll(struct desk_file_job *job);
void desk_file_job_close(struct desk_file_job *job);
#endif
