/* Observe browser-relevant libc semantics without mapping host/guest memory.
 * Compile the real libc adapter with private names. The modeled kernel boundary
 * always fails; only adapter rejection, flag forwarding and no-op advice are
 * accepted here. This is not a guest memory or JSC allocator test. */
#define mmap browser_mmap
#define mmap64 browser_mmap64
#define mremap browser_mremap
#define munmap browser_munmap
#define mprotect browser_mprotect
#define msync browser_msync
#define madvise browser_madvise
#define __errno_location browser_errno_location
#include "../userspace/lib/src/posix_mman.c"
#include <stdio.h>

static int observed_errno;
static unsigned mmap_calls;
static int observed_flags;

int *browser_errno_location(void) { return &observed_errno; }

long srv_mmap(void *address, size_t length, int protection, int flags,
              int fd, off_t offset) {
    (void)address; (void)length; (void)protection; (void)fd; (void)offset;
    ++mmap_calls;
    observed_flags = flags;
    return -1;
}
long srv_munmap(void *address, size_t length) {
    (void)address; (void)length; return -1;
}
long srv_mprotect(void *address, size_t length, int protection) {
    (void)address; (void)length; (void)protection; return -1;
}
long srv_msync(void *address, size_t length, int flags) {
    (void)address; (void)length; (void)flags; return -1;
}

int main(void) {
    _Alignas(4096) static unsigned char page[4096];
    if (browser_mmap(0, sizeof(page), PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
        observed_errno != ENOSYS || mmap_calls != 0) return 1;

    for (size_t i = 0; i < sizeof(page); ++i) page[i] = 0x5a;
    if (browser_madvise(page, sizeof(page), MADV_DONTNEED) != 0) return 2;
    for (size_t i = 0; i < sizeof(page); ++i)
        if (page[i] != 0x5a) return 3;
    if (browser_madvise(page, sizeof(page), -1) != -1 ||
        observed_errno != EINVAL) return 4;

    if (browser_mmap(0, sizeof(page), PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)
        != MAP_FAILED || observed_errno != ENOMEM || mmap_calls != 1 ||
        observed_flags != (MAP_PRIVATE | MAP_ANONYMOUS)) return 5;
    puts("browser-memory: shared rejected; discard retains bytes; noreserve stripped");
    return 0;
}
