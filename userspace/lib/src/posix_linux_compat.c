#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <srvros/sys.h>

int sysinfo(struct sysinfo *info) {
    if (info == 0) {
        errno = EINVAL;
        return -1;
    }
    memset(info, 0, sizeof(*info));
    struct srv_meminfo meminfo;
    if (srv_meminfo(&meminfo) < 0) {
        errno = EIO;
        return -1;
    }
    long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) {
        ticks_per_second = 100;
    }
    info->uptime = (long)(srv_ticks() / (uint64_t)ticks_per_second);
    info->totalram = (unsigned long)meminfo.total_bytes;
    info->freeram = (unsigned long)meminfo.free_bytes;
    info->mem_unit = 1;
    info->procs = 1;
    return 0;
}

unsigned long getauxval(unsigned long type) {
    switch (type) {
    case AT_PAGESZ:
        return (unsigned long)getpagesize();
    case AT_CLKTCK:
        return (unsigned long)sysconf(_SC_CLK_TCK);
    case AT_SECURE:
    case AT_HWCAP:
    case AT_HWCAP2:
    case AT_SYSINFO_EHDR:
        return 0;
    default:
        errno = ENOENT;
        return 0;
    }
}

int prctl(int option, ...) {
    switch (option) {
    case PR_SET_NAME:
    case PR_SET_VMA:
        return 0;
    case PR_GET_NAME: {
        va_list args;
        va_start(args, option);
        char *buffer = va_arg(args, char *);
        va_end(args);
        if (buffer == 0) {
            errno = EINVAL;
            return -1;
        }
        strcpy(buffer, "srvros");
        return 0;
    }
    default:
        errno = EINVAL;
        return -1;
    }
}

long syscall(long number, ...) {
    va_list args;
    va_start(args, number);
    long result = -1;
    switch (number) {
    case SYS_read: {
        int fd = va_arg(args, int);
        void *buffer = va_arg(args, void *);
        size_t length = va_arg(args, size_t);
        result = read(fd, buffer, length);
        break;
    }
    case SYS_write: {
        int fd = va_arg(args, int);
        const void *buffer = va_arg(args, const void *);
        size_t length = va_arg(args, size_t);
        result = write(fd, buffer, length);
        break;
    }
    case SYS_close: {
        int fd = va_arg(args, int);
        result = close(fd);
        break;
    }
    case SYS_mmap: {
        void *address = va_arg(args, void *);
        size_t length = va_arg(args, size_t);
        int protection = va_arg(args, int);
        int flags = va_arg(args, int);
        int fd = va_arg(args, int);
        off_t offset = va_arg(args, off_t);
        void *mapping = mmap(address, length, protection, flags, fd, offset);
        result = mapping == MAP_FAILED ? -1 : (long)(uintptr_t)mapping;
        break;
    }
    case SYS_mprotect: {
        void *address = va_arg(args, void *);
        size_t length = va_arg(args, size_t);
        int protection = va_arg(args, int);
        result = mprotect(address, length, protection);
        break;
    }
    case SYS_munmap: {
        void *address = va_arg(args, void *);
        size_t length = va_arg(args, size_t);
        result = munmap(address, length);
        break;
    }
    case SYS_getpid:
        result = getpid();
        break;
    case SYS_gettid:
        result = srv_thread_self();
        if (result <= 0) {
            result = getpid();
        }
        break;
    case SYS_getrandom: {
        void *buffer = va_arg(args, void *);
        size_t length = va_arg(args, size_t);
        unsigned int flags = va_arg(args, unsigned int);
        result = getrandom(buffer, length, flags);
        break;
    }
    default:
        errno = ENOSYS;
        result = -1;
        break;
    }
    va_end(args);
    return result;
}

ssize_t __read_chk(int fd, void *buffer, size_t length, size_t buffer_length) {
    if (length > buffer_length) {
        errno = EINVAL;
        return -1;
    }
    return read(fd, buffer, length);
}

void *__memmove_chk(void *destination, const void *source, size_t length, size_t destination_length) {
    if (length > destination_length) {
        errno = EINVAL;
        return 0;
    }
    return memmove(destination, source, length);
}

void *__memcpy_chk(void *destination, const void *source, size_t length, size_t destination_length) {
    if (length > destination_length) {
        errno = EINVAL;
        return 0;
    }
    return memcpy(destination, source, length);
}

void *__memset_chk(void *destination, int value, size_t length, size_t destination_length) {
    if (length > destination_length) {
        errno = EINVAL;
        return 0;
    }
    return memset(destination, value, length);
}

int __open64_2(const char *path, int flags) {
    return open64(path, flags);
}

char *__realpath_chk(const char *path, char *resolved_path, size_t resolved_length) {
    if (resolved_path != 0 && resolved_length < 160) {
        errno = EINVAL;
        return 0;
    }
    return realpath(path, resolved_path);
}

long __fdelt_chk(long fd) {
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }
    return fd / (long)(8 * sizeof(long));
}

long __sysconf(int name) {
    return sysconf(name);
}

const char *gnu_get_libc_version(void) {
    return "srvros-libc";
}
