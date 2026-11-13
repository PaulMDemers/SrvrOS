#ifndef SRVROS_KERNEL_FILE_STREAM_H
#define SRVROS_KERNEL_FILE_STREAM_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/file_stream.h>
/* Called only within the namespace service window; owner is the never-reused process instance
 * identity (not PID or slot address). Resource ownership is
 * resource integrity, not account authentication or access-control policy. */
int64_t file_stream_open(uint64_t owner,const char *path,bool create,uint64_t mode);
int64_t file_stream_pread(uint64_t owner,uint64_t token,void *buffer,uint64_t length,uint64_t offset);
/* Admitted byte count, computed before touching the userspace payload. */
int64_t file_stream_append_count(uint64_t owner,uint64_t token,uint64_t length);
int64_t file_stream_append(uint64_t owner,uint64_t token,const void *buffer,uint64_t length);
int64_t file_stream_stat(uint64_t owner,uint64_t token,struct srv_file_stream_stat *info);
int64_t file_stream_sync(uint64_t owner,uint64_t token);
int64_t file_stream_close(uint64_t owner,uint64_t token);
bool file_stream_cleanup(uint64_t owner);
/* Read-only presence snapshot; caller holds IF0 until consuming the result. */
bool file_stream_owned(uint64_t owner);
#endif
