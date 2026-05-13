#ifndef SRVROS_POSIX_STDLIB_H
#define SRVROS_POSIX_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
int atoi(const char *text);
long atol(const char *text);
char *getenv(const char *name);
void exit(int status) __attribute__((noreturn));

#endif
