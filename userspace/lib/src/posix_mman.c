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
        (flags & ~(MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE)) != 0) {
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
    int kernel_flags = flags & ~MAP_NORESERVE;
    long result = srv_mmap(address, length, protection, kernel_flags, fd, offset);
    if (result < 0) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    return (void *)(uintptr_t)result;
}

void *mmap64(void *address, size_t length, int protection, int flags, int fd, off_t offset) {
    return mmap(address, length, protection, flags, fd, offset);
}

void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...) {
    (void)old_address;
    (void)old_size;
    (void)new_size;
    (void)flags;
    errno = ENOSYS;
    return MAP_FAILED;
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

int msync(void *address, size_t length, int flags) {
    if (address == 0 ||
        length == 0 ||
        (((uintptr_t)address) & 0xfff) != 0 ||
        (flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
        ((flags & MS_ASYNC) != 0 && (flags & MS_SYNC) != 0)) {
        errno = EINVAL;
        return -1;
    }
    if (srv_msync(address, length, flags) < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int madvise(void *address, size_t length, int advice) {
    if (address == 0 || length == 0 || (((uintptr_t)address) & 0xfff) != 0) {
        errno = EINVAL;
        return -1;
    }
    switch (advice) {
    case MADV_NORMAL:
    case MADV_RANDOM:
    case MADV_SEQUENTIAL:
    case MADV_WILLNEED:
    case MADV_DONTNEED:
    case MADV_FREE:
    case MADV_REMOVE:
    case MADV_DONTFORK:
    case MADV_DOFORK:
    case MADV_MERGEABLE:
    case MADV_UNMERGEABLE:
    case MADV_HUGEPAGE:
    case MADV_NOHUGEPAGE:
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}
