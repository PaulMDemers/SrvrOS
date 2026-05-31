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
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE 0x8000

#define MAP_FAILED ((void *)-1)

#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED 2

#define MS_ASYNC SRV_MS_ASYNC
#define MS_SYNC SRV_MS_SYNC
#define MS_INVALIDATE SRV_MS_INVALIDATE

#define MADV_NORMAL 0
#define MADV_RANDOM 1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_FREE 8
#define MADV_REMOVE 9
#define MADV_DONTFORK 10
#define MADV_DOFORK 11
#define MADV_MERGEABLE 12
#define MADV_UNMERGEABLE 13
#define MADV_HUGEPAGE 14
#define MADV_NOHUGEPAGE 15

#ifdef __cplusplus
extern "C" {
#endif

void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset);
void *mmap64(void *address, size_t length, int protection, int flags, int fd, off_t offset);
int munmap(void *address, size_t length);
void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);
int mprotect(void *address, size_t length, int protection);
int msync(void *address, size_t length, int flags);
int madvise(void *address, size_t length, int advice);

#ifdef __cplusplus
}
#endif

#endif
