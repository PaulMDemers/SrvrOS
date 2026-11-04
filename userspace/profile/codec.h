#ifndef PROFILE_CODEC_H
#define PROFILE_CODEC_H
#include <stddef.h>
void profile_sha256(const void *data, size_t size, char hex[65]);
char *profile_base64_encode(const unsigned char *data, size_t size);
unsigned char *profile_base64_decode(const char *text, size_t expected);
#endif
