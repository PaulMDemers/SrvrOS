#ifndef SRVROS_POSIX_SYS_MMAN_H
#define SRVROS_POSIX_SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#include <srvros/syscall_numbers.h>

#define PROT_NONE SRV_PROT_NONE
#define PROT_READ SRV_PROT_READ
#define PROT_WRITE SRV_PROT_WRITE
#define PROT_EXEC SRV_PROT_EXEC

#define MAP_SHARED SRV_MAP_SHARED
#define MAP_PRIVATE SRV_MAP_PRIVATE
#define MAP_FIXED SRV_MAP_FIXED
#define MAP_ANONYMOUS SRV_MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void *)-1)

#define MS_ASYNC SRV_MS_ASYNC
#define MS_SYNC SRV_MS_SYNC
#define MS_INVALIDATE SRV_MS_INVALIDATE

void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset);
int munmap(void *address, size_t length);
int mprotect(void *address, size_t length, int protection);
int msync(void *address, size_t length, int flags);

#endif
