#ifndef SRVROS_EXFAT_STREAM_H
#define SRVROS_EXFAT_STREAM_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/file_stream.h>
/* Internal stable backend leases; caller holds the existing storage service
 * admission. Backend data copies/bounces never exceed the chunk bound. */
int64_t exfat_stream_open_read(const char *path);
int64_t exfat_stream_create(const char *path,uint64_t mode);
int64_t exfat_stream_pread(uint64_t handle,void *buffer,uint64_t length,uint64_t offset);
int64_t exfat_stream_append(uint64_t handle,const void *buffer,uint64_t length);
int64_t exfat_stream_stat(uint64_t handle,struct srv_file_stream_stat *info);
int64_t exfat_stream_sync(uint64_t handle);
int64_t exfat_stream_close(uint64_t handle);
/* Exact file, or any leased descendant when subtree is true; case-insensitive
 * canonical storage identity. Does not acquire the service gate recursively. */
bool exfat_stream_busy(const char *path,bool subtree);
#endif
