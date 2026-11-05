#ifndef PROFILE_STREAM_H
#define PROFILE_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include "codec.h"
#define PROFILE_STREAM_BUFFER 65536u
#define PROFILE_STREAM_META 65536u
#define PROFILE_STREAM_FILE (16u*1024u*1024u)
#define PROFILE_STREAM_TOTAL (64u*1024u*1024u)
#define PROFILE_STREAM_MAGIC "SRVROS-PROFILE2\n"
typedef int64_t profile_stream_handle;
struct profile_stream_info { uint64_t size,inode,mode,nlink,mtime,ctime,dev; };
profile_stream_handle profile_stream_open_read(const char *path);
profile_stream_handle profile_stream_open_new(const char *path);
int64_t profile_stream_pread(profile_stream_handle handle,void *buffer,size_t size,uint64_t offset);
int64_t profile_stream_write(profile_stream_handle handle,const void *buffer,size_t size);
int profile_stream_stat(profile_stream_handle handle,struct profile_stream_info *info);
int profile_stream_sync(profile_stream_handle handle);
int profile_stream_close(profile_stream_handle handle);
int profile_stream_publish(const char *source,const char *destination,const struct profile_stream_info *expected);
int profile_stream_append(profile_stream_handle input,uint64_t offset,uint64_t size,profile_stream_handle output,char hash[65],struct profile_sha256_ctx *whole);
int profile_stream_hash(profile_stream_handle handle,uint64_t offset,uint64_t size,char hash[65]);
int profile_stream_copy(profile_stream_handle input,uint64_t offset,uint64_t size,profile_stream_handle output,char hash[65],profile_stream_handle checkpoint);
#endif
