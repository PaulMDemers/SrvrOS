#ifndef SRVROS_POSIX_STRING_H
#define SRVROS_POSIX_STRING_H

#include <stddef.h>

void *memset(void *destination, int value, size_t length);
void *memcpy(void *destination, const void *source, size_t length);
void *memmove(void *destination, const void *source, size_t length);
void *memchr(const void *ptr, int value, size_t length);
int memcmp(const void *left, const void *right, size_t length);
size_t strlen(const char *text);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t length);
int strcoll(const char *left, const char *right);
char *strcpy(char *destination, const char *source);
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
char *strdup(const char *text);

#endif
