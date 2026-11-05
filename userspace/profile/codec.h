#ifndef PROFILE_CODEC_H
#define PROFILE_CODEC_H
#include <stddef.h>
#include <stdint.h>
struct profile_sha256_ctx { uint32_t h[8]; uint64_t total; unsigned used; unsigned char tail[64]; };
void profile_sha256_init(struct profile_sha256_ctx *c);
void profile_sha256_update(struct profile_sha256_ctx *c,const void *data,size_t size);
void profile_sha256_final(struct profile_sha256_ctx *c,char hex[65]);
void profile_sha256(const void *data, size_t size, char hex[65]);
char *profile_base64_encode(const unsigned char *data, size_t size);
unsigned char *profile_base64_decode(const char *text, size_t expected);
#endif
