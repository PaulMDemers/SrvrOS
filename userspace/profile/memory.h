#ifndef PROFILE_MEMORY_H
#define PROFILE_MEMORY_H
#include <stddef.h>
void profile_memory_init(void);
void profile_memory_report(const char *phase);
#ifdef SRVROS_PROFILE_HOST_TEST
void *profile_memory_alloc(size_t size);
void profile_memory_free(void *ptr);
#define malloc profile_memory_alloc
#define free profile_memory_free
#endif
#endif
