#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

#include <srvros/sys.h>

void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset) {
    if (length == 0 ||
        (protection & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if ((flags & MAP_SHARED) != 0 ||
        (flags & MAP_PRIVATE) == 0 ||
        (flags & ~(MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)) != 0) {
        errno = ENOSYS;
        return MAP_FAILED;
    }
    if ((flags & MAP_ANONYMOUS) != 0) {
        if (fd != -1 || offset != 0) {
            errno = EINVAL;
            return MAP_FAILED;
        }
    } else if (fd < 0 || offset < 0 || (((uint64_t)offset) & 0xfff) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    long result = srv_mmap(address, length, protection, flags, fd, offset);
    if (result < 0) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    return (void *)(uintptr_t)result;
}

int munmap(void *address, size_t length) {
    if (address == 0 || length == 0 || (((uintptr_t)address) & 0xfff) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_munmap(address, length) < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int mprotect(void *address, size_t length, int protection) {
    if (address == 0 ||
        length == 0 ||
        (((uintptr_t)address) & 0xfff) != 0 ||
        (protection & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_mprotect(address, length, protection) < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}
