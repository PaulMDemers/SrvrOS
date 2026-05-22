#ifndef SRVROS_POSIX_STRINGS_H
#define SRVROS_POSIX_STRINGS_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcmp(const void *left, const void *right, size_t size);
void bcopy(const void *src, void *dest, size_t size);
void bzero(void *dest, size_t size);
char *index(const char *s, int c);
char *rindex(const char *s, int c);
int strcasecmp(const char *left, const char *right);
int strncasecmp(const char *left, const char *right, size_t n);

#ifdef __cplusplus
}
#endif

#endif
