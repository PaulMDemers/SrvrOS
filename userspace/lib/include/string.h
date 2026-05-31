#ifndef SRVROS_POSIX_STRING_H
#define SRVROS_POSIX_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *destination, int value, size_t length);
void *memcpy(void *destination, const void *source, size_t length);
void *memmove(void *destination, const void *source, size_t length);
void *mempcpy(void *destination, const void *source, size_t length);
void *memchr(const void *ptr, int value, size_t length);
void *memmem(const void *haystack, size_t haystack_length, const void *needle, size_t needle_length);
void *memrchr(const void *ptr, int value, size_t length);
int memcmp(const void *left, const void *right, size_t length);
size_t strlen(const char *text);
size_t strnlen(const char *text, size_t maxlen);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t length);
int strcoll(const char *left, const char *right);
size_t strxfrm(char *destination, const char *source, size_t length);
char *strcpy(char *destination, const char *source);
char *stpcpy(char *destination, const char *source);
char *stpncpy(char *destination, const char *source, size_t length);
char *strncpy(char *destination, const char *source, size_t length);
char *strcat(char *destination, const char *source);
char *strncat(char *destination, const char *source, size_t length);
char *strchr(const char *text, int c);
char *strrchr(const char *text, int c);
char *strpbrk(const char *text, const char *accept);
char *strstr(const char *haystack, const char *needle);
size_t strspn(const char *text, const char *accept);
size_t strcspn(const char *text, const char *reject);
char *strerror(int error);
int strerror_r(int error, char *buffer, size_t length);
char *strdup(const char *text);
char *strndup(const char *text, size_t length);
int strcasecmp(const char *left, const char *right);
int strncasecmp(const char *left, const char *right, size_t length);

typedef struct srvros_locale *locale_t;

int strcoll_l(const char *left, const char *right, locale_t locale);
size_t strxfrm_l(char *destination, const char *source, size_t length, locale_t locale);

#ifdef __cplusplus
}
#endif

#endif
