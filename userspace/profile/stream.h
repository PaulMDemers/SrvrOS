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
int profile_stream_append(int input,uint64_t offset,uint64_t size,int output,char hash[65],struct profile_sha256_ctx *whole);
int profile_stream_sync(int fd);
int profile_stream_hash(int fd,uint64_t offset,uint64_t size,char hash[65]);
int profile_stream_copy(int input,uint64_t offset,uint64_t size,int output,char hash[65],int checkpoint_fd);
#endif
