#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <srvros/sys.h>

struct mmsghdr {
    struct msghdr msg_hdr;
    unsigned int msg_len;
};

struct ifaddrs {
    struct ifaddrs *ifa_next;
    char *ifa_name;
    unsigned int ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    union {
        struct sockaddr *ifu_broadaddr;
        struct sockaddr *ifu_dstaddr;
    } ifa_ifu;
    void *ifa_data;
};

struct dl_phdr_info {
    uintptr_t dlpi_addr;
    const char *dlpi_name;
    const void *dlpi_phdr;
    unsigned short dlpi_phnum;
};

int eventfd(unsigned int initval, int flags) {
    (void)initval;
    (void)flags;
    errno = ENOSYS;
    return -1;
}

int epoll_create1(int flags) {
    (void)flags;
    errno = ENOSYS;
    return -1;
}

int epoll_ctl(int epfd, int op, int fd, void *event) {
    (void)epfd;
    (void)op;
    (void)fd;
    (void)event;
    errno = ENOSYS;
    return -1;
}

int epoll_wait(int epfd, void *events, int maxevents, int timeout) {
    (void)epfd;
    (void)events;
    (void)maxevents;
    (void)timeout;
    errno = ENOSYS;
    return -1;
}

int epoll_pwait(int epfd, void *events, int maxevents, int timeout, const void *sigmask) {
    (void)sigmask;
    return epoll_wait(epfd, events, maxevents, timeout);
}

int inotify_init1(int flags) {
    (void)flags;
    errno = ENOSYS;
    return -1;
}

int inotify_add_watch(int fd, const char *path, uint32_t mask) {
    (void)fd;
    (void)path;
    (void)mask;
    errno = ENOSYS;
    return -1;
}

int inotify_rm_watch(int fd, int wd) {
    (void)fd;
    (void)wd;
    errno = ENOSYS;
    return -1;
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
    (void)prepare;
    (void)parent;
    (void)child;
    return 0;
}

int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t *mask) {
    return sched_getaffinity((pid_t)thread, cpusetsize, mask) == 0 ? 0 : errno;
}

int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *mask) {
    return sched_setaffinity((pid_t)thread, cpusetsize, mask) == 0 ? 0 : errno;
}

int pthread_getschedparam(pthread_t thread, int *policy, void *param) {
    (void)thread;
    (void)param;
    if (policy != 0) {
        *policy = 0;
    }
    return 0;
}

int pthread_setschedparam(pthread_t thread, int policy, const void *param) {
    (void)thread;
    (void)policy;
    (void)param;
    return 0;
}

int fork(void) {
    errno = ENOSYS;
    return -1;
}

ssize_t readlink(const char *path, char *buffer, size_t size) {
    (void)path;
    (void)buffer;
    (void)size;
    errno = ENOSYS;
    return -1;
}

int ttyname_r(int fd, char *buffer, size_t size) {
    if (!isatty(fd)) {
        return ENOTTY;
    }
    const char *name = "/dev/tty";
    if (buffer == 0 || size <= strlen(name)) {
        return ERANGE;
    }
    strcpy(buffer, name);
    return 0;
}

ssize_t sendfile64(int out_fd, int in_fd, off_t *offset, size_t count) {
    char buffer[1024];
    size_t total = 0;
    if (offset != 0 && lseek(in_fd, *offset, SEEK_SET) < 0) {
        return -1;
    }
    while (total < count) {
        size_t chunk = count - total < sizeof(buffer) ? count - total : sizeof(buffer);
        ssize_t got = read(in_fd, buffer, chunk);
        if (got <= 0) {
            break;
        }
        ssize_t sent = write(out_fd, buffer, (size_t)got);
        if (sent < 0) {
            return total != 0 ? (ssize_t)total : -1;
        }
        total += (size_t)sent;
        if (offset != 0) {
            *offset += sent;
        }
        if (sent < got) {
            break;
        }
    }
    return (ssize_t)total;
}

int getifaddrs(struct ifaddrs **ifap) {
    if (ifap == 0) {
        errno = EINVAL;
        return -1;
    }
    *ifap = 0;
    return 0;
}

void freeifaddrs(struct ifaddrs *ifa) {
    (void)ifa;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data) {
    if (callback == 0) {
        return 0;
    }
    struct dl_phdr_info info = {
        .dlpi_addr = 0,
        .dlpi_name = "srvros",
        .dlpi_phdr = 0,
        .dlpi_phnum = 0,
    };
    return callback(&info, sizeof(info), data);
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    (void)abs_timeout;
    return sem_wait(sem);
}

int recvmmsg(int fd, struct mmsghdr *messages, unsigned int vlen, int flags, struct timespec *timeout) {
    (void)timeout;
    if (messages == 0 && vlen != 0) {
        errno = EINVAL;
        return -1;
    }
    unsigned int count = 0;
    for (; count < vlen; count++) {
        ssize_t got = recvmsg(fd, &messages[count].msg_hdr, flags);
        if (got < 0) {
            return count != 0 ? (int)count : -1;
        }
        messages[count].msg_len = (unsigned int)got;
    }
    return (int)count;
}

int sendmmsg(int fd, struct mmsghdr *messages, unsigned int vlen, int flags) {
    if (messages == 0 && vlen != 0) {
        errno = EINVAL;
        return -1;
    }
    unsigned int count = 0;
    for (; count < vlen; count++) {
        ssize_t sent = sendmsg(fd, &messages[count].msg_hdr, flags);
        if (sent < 0) {
            return count != 0 ? (int)count : -1;
        }
        messages[count].msg_len = (unsigned int)sent;
    }
    return (int)count;
}

int __sched_cpucount(size_t setsize, const cpu_set_t *set) {
    (void)setsize;
    return __sched_cpu_count(set);
}
