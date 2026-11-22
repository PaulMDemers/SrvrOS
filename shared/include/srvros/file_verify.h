#ifndef SRVROS_FILE_VERIFY_H
#define SRVROS_FILE_VERIFY_H
#include <stdint.h>

/* A separate read-only proof limit; existing read/write/copy limits do not
 * change. The caller keeps expected bytes immutable and mapped until return.
 * Expected bytes are checked in bounded chunks, not atomically snapshotted. */
#define SRV_FILE_VERIFY_MAX (8u * 1024u * 1024u)
#define SRV_FILE_VERIFY_CHUNK 4096u
struct srv_file_verify_request {
    uint64_t abi_version,struct_size;
    const char *path;
    const void *data;
    uint64_t length,expected_inode,expected_mtime,reserved;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_file_verify_request)==64,"file verify ABI");
#else
_Static_assert(sizeof(struct srv_file_verify_request)==64,"file verify ABI");
#endif
#endif
