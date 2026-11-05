#ifndef SRVROS_FILE_STREAM_H
#define SRVROS_FILE_STREAM_H
#include <stdint.h>
#define SRV_FILE_STREAM_VERSION 1u
#define SRV_FILE_STREAM_CHUNK 65536u
#define SRV_FILE_STREAM_TYPE_FILE 0u
#define SRV_FILE_STREAM_PROCESS_MAX 65u
#define SRV_FILE_STREAM_GLOBAL_MAX 256u
enum srv_file_stream_operation {
    SRV_FILE_STREAM_OPEN_READ=1, SRV_FILE_STREAM_CREATE_APPEND,
    SRV_FILE_STREAM_PREAD, SRV_FILE_STREAM_APPEND, SRV_FILE_STREAM_STAT,
    SRV_FILE_STREAM_SYNC, SRV_FILE_STREAM_CLOSE
};
/* Process-local opaque resources; never pass a token to POSIX fd functions.
 * Unknown/unused fields must be zero. The complete operation holds existing
 * namespace I/O admission; no callback retains a userspace pointer. */
struct srv_file_stream_request {
    uint32_t version,size,operation,flags;
    uint64_t handle,path,buffer,length,offset,mode,reserved[2];
};
/* Same fixed-width information as the ordinary stat ABI, separate operation. */
struct srv_file_stream_stat {
    uint64_t abi_version,struct_size,size,type,inode,mode,nlink,uid,gid;
    uint64_t atime,mtime,ctime,blksize,blocks;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_file_stream_request)==80,"file stream request ABI");
static_assert(sizeof(struct srv_file_stream_stat)==112,"file stream stat ABI");
#else
_Static_assert(sizeof(struct srv_file_stream_request)==80,"file stream request ABI");
_Static_assert(sizeof(struct srv_file_stream_stat)==112,"file stream stat ABI");
#endif
#endif
