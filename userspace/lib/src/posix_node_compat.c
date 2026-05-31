#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/epoll.h>
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

#define POSIX_EVENTFD_BASE 3000
#define POSIX_EVENTFD_MAX 16
#define POSIX_EPOLL_MAX 8
#define POSIX_EPOLL_ENTRIES 64

struct posix_eventfd {
    int used;
    uint64_t counter;
    uint64_t fd_flags;
    uint64_t descriptor_flags;
};

struct posix_epoll_entry {
    int used;
    int fd;
    struct epoll_event event;
};

struct posix_epoll {
    int used;
    int fd;
    struct posix_epoll_entry entries[POSIX_EPOLL_ENTRIES];
};

static struct posix_eventfd eventfds[POSIX_EVENTFD_MAX];
static struct posix_epoll epolls[POSIX_EPOLL_MAX];

static int epoll_trace_enabled(void) {
    const char *trace = getenv("SRVROS_EPOLL_TRACE");
    return trace != 0 && trace[0] != '\0' && trace[0] != '0';
}

static void epoll_trace_wait_entry(int epfd,
    int timeout,
    int poll_count,
    int ready,
    struct pollfd *pollfds) {
    if (!epoll_trace_enabled()) {
        return;
    }
    fprintf(stderr,
        "srvros-epoll: wait-enter epfd=%d timeout=%d ready=%d watched=%d\n",
        epfd,
        timeout,
        ready,
        poll_count);
    for (int i = 0; i < poll_count; i++) {
        fprintf(stderr,
            "srvros-epoll: wait-fd fd=%d events=0x%x\n",
            pollfds[i].fd,
            (unsigned int)pollfds[i].events);
    }
}

static struct posix_eventfd *eventfd_slot(int fd) {
    if (fd < POSIX_EVENTFD_BASE || fd >= POSIX_EVENTFD_BASE + POSIX_EVENTFD_MAX) {
        return 0;
    }
    struct posix_eventfd *slot = &eventfds[fd - POSIX_EVENTFD_BASE];
    return slot->used ? slot : 0;
}

int eventfd(unsigned int initval, int flags) {
    for (int i = 0; i < POSIX_EVENTFD_MAX; i++) {
        if (eventfds[i].used) {
            continue;
        }
        eventfds[i].used = 1;
        eventfds[i].counter = initval;
        eventfds[i].fd_flags = (flags & O_NONBLOCK) != 0 ? SRV_FD_NONBLOCK : 0;
        eventfds[i].descriptor_flags = (flags & O_CLOEXEC) != 0 ? SRV_FD_CLOEXEC : 0;
        return POSIX_EVENTFD_BASE + i;
    }
    errno = EMFILE;
    return -1;
}

int __posix_eventfd_is_pseudo(int fd) {
    return eventfd_slot(fd) != 0;
}

ssize_t __posix_eventfd_read(int fd, void *buffer, size_t length) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0 || buffer == 0 || length < sizeof(uint64_t)) {
        errno = slot == 0 ? EBADF : EINVAL;
        return -1;
    }
    if (slot->counter == 0) {
        errno = (slot->fd_flags & SRV_FD_NONBLOCK) != 0 ? EAGAIN : EWOULDBLOCK;
        return -1;
    }
    uint64_t value = slot->counter;
    slot->counter = 0;
    memcpy(buffer, &value, sizeof(value));
    return (ssize_t)sizeof(value);
}

ssize_t __posix_eventfd_write(int fd, const void *buffer, size_t length) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0 || buffer == 0 || length < sizeof(uint64_t)) {
        errno = slot == 0 ? EBADF : EINVAL;
        return -1;
    }
    uint64_t value = 0;
    memcpy(&value, buffer, sizeof(value));
    slot->counter += value;
    return (ssize_t)sizeof(value);
}

int __posix_eventfd_close(int fd) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0) {
        errno = EBADF;
        return -1;
    }
    memset(slot, 0, sizeof(*slot));
    return 0;
}

int __posix_eventfd_fcntl(int fd, int command, uint64_t flags) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0) {
        errno = EBADF;
        return -1;
    }
    if (command == SRV_F_GETFD) {
        return (int)slot->descriptor_flags;
    }
    if (command == SRV_F_SETFD) {
        slot->descriptor_flags = flags & SRV_FD_CLOEXEC;
        return 0;
    }
    if (command == SRV_F_GETFL) {
        return (int)slot->fd_flags;
    }
    if (command == SRV_F_SETFL) {
        slot->fd_flags = flags & SRV_FD_NONBLOCK;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static short epoll_to_poll_events(uint32_t events) {
    short out = 0;
    if ((events & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) != 0) {
        out |= POLLIN;
    }
    if ((events & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) != 0) {
        out |= POLLOUT;
    }
    return out;
}

static uint32_t poll_to_epoll_events(short revents) {
    uint32_t out = 0;
    if ((revents & POLLIN) != 0) {
        out |= EPOLLIN;
    }
    if ((revents & POLLOUT) != 0) {
        out |= EPOLLOUT;
    }
    if ((revents & POLLERR) != 0) {
        out |= EPOLLERR;
    }
    if ((revents & POLLHUP) != 0) {
        out |= EPOLLHUP;
    }
    return out;
}

static struct posix_epoll *epoll_slot(int fd) {
    for (int i = 0; i < POSIX_EPOLL_MAX; i++) {
        if (epolls[i].used && epolls[i].fd == fd) {
            return &epolls[i];
        }
    }
    return 0;
}

static struct posix_epoll_entry *epoll_entry_for_fd(struct posix_epoll *epoll, int fd) {
    if (epoll == 0) {
        return 0;
    }
    for (int i = 0; i < POSIX_EPOLL_ENTRIES; i++) {
        if (epoll->entries[i].used && epoll->entries[i].fd == fd) {
            return &epoll->entries[i];
        }
    }
    return 0;
}

static struct posix_epoll_entry *epoll_free_entry(struct posix_epoll *epoll) {
    if (epoll == 0) {
        return 0;
    }
    for (int i = 0; i < POSIX_EPOLL_ENTRIES; i++) {
        if (!epoll->entries[i].used) {
            return &epoll->entries[i];
        }
    }
    return 0;
}

static int eventfd_poll_events(int fd, short requested) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    short revents = 0;
    if (slot == 0) {
        return POLLNVAL;
    }
    if ((requested & POLLIN) != 0 && slot->counter != 0) {
        revents |= POLLIN;
    }
    if ((requested & POLLOUT) != 0) {
        revents |= POLLOUT;
    }
    return revents;
}

int epoll_create1(int flags) {
    if ((flags & ~EPOLL_CLOEXEC) != 0) {
        errno = EINVAL;
        return -1;
    }
    int fds[2];
    if (pipe(fds) < 0) {
        return -1;
    }
    close(fds[1]);
    for (int i = 0; i < POSIX_EPOLL_MAX; i++) {
        if (epolls[i].used) {
            continue;
        }
        memset(&epolls[i], 0, sizeof(epolls[i]));
        epolls[i].used = 1;
        epolls[i].fd = fds[0];
        if ((flags & EPOLL_CLOEXEC) != 0) {
            (void)fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        }
        return fds[0];
    }
    close(fds[0]);
    errno = EMFILE;
    return -1;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    struct posix_epoll *epoll = epoll_slot(epfd);
    if (epoll == 0 || fd < 0 || (op != EPOLL_CTL_DEL && event == 0)) {
        errno = EINVAL;
        return -1;
    }

    struct posix_epoll_entry *entry = epoll_entry_for_fd(epoll, fd);
    if (op == EPOLL_CTL_DEL) {
        if (epoll_trace_enabled()) {
            fprintf(stderr, "srvros-epoll: del epfd=%d fd=%d\n", epfd, fd);
        }
        if (entry == 0) {
            errno = ENOENT;
            return -1;
        }
        memset(entry, 0, sizeof(*entry));
        return 0;
    }
    if (op == EPOLL_CTL_MOD) {
        if (epoll_trace_enabled()) {
            fprintf(stderr,
                "srvros-epoll: mod epfd=%d fd=%d events=0x%x\n",
                epfd,
                fd,
                event->events);
        }
        if (entry == 0) {
            errno = ENOENT;
            return -1;
        }
        entry->event = *event;
        return 0;
    }
    if (op != EPOLL_CTL_ADD) {
        errno = EINVAL;
        return -1;
    }
    if (entry != 0) {
        errno = EEXIST;
        return -1;
    }
    entry = epoll_free_entry(epoll);
    if (entry == 0) {
        errno = ENOSPC;
        return -1;
    }
    memset(entry, 0, sizeof(*entry));
    entry->used = 1;
    entry->fd = fd;
    entry->event = *event;
    if (epoll_trace_enabled()) {
        fprintf(stderr,
            "srvros-epoll: add epfd=%d fd=%d events=0x%x\n",
            epfd,
            fd,
            event->events);
    }
    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    struct posix_epoll *epoll = epoll_slot(epfd);
    struct pollfd pollfds[POSIX_EPOLL_ENTRIES];
    int map[POSIX_EPOLL_ENTRIES];
    int poll_count = 0;
    int ready = 0;

    if (epoll == 0 || events == 0 || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    for (int i = 0; i < POSIX_EPOLL_ENTRIES && ready < maxevents; i++) {
        struct posix_epoll_entry *entry = &epoll->entries[i];
        if (!entry->used) {
            continue;
        }
        short requested = epoll_to_poll_events(entry->event.events);
        if (requested == 0) {
            continue;
        }
        if (__posix_eventfd_is_pseudo(entry->fd)) {
            uint32_t revents = poll_to_epoll_events(eventfd_poll_events(entry->fd, requested));
            if (revents != 0) {
                events[ready] = entry->event;
                events[ready].events = revents;
                ready++;
            }
            continue;
        }
        pollfds[poll_count].fd = entry->fd;
        pollfds[poll_count].events = requested;
        pollfds[poll_count].revents = 0;
        map[poll_count] = i;
        poll_count++;
    }

    epoll_trace_wait_entry(epfd, timeout, poll_count, ready, pollfds);

    if (poll_count == 0) {
        if (ready == 0 && timeout > 0) {
            usleep((unsigned int)timeout * 1000u);
        } else if (ready == 0 && timeout < 0) {
            usleep(10000);
        }
        return ready;
    }

    int effective_timeout = timeout == 0 ? 10 : timeout;
    int result = poll(pollfds, (nfds_t)poll_count, effective_timeout);
    if (epoll_trace_enabled()) {
        fprintf(stderr,
            "srvros-epoll: poll-result epfd=%d result=%d errno=%d\n",
            epfd,
            result,
            errno);
        for (int i = 0; i < poll_count; i++) {
            fprintf(stderr,
                "srvros-epoll: poll-fd fd=%d revents=0x%x\n",
                pollfds[i].fd,
                (unsigned int)pollfds[i].revents);
        }
    }
    if (result < 0) {
        return -1;
    }
    for (int i = 0; i < poll_count && ready < maxevents; i++) {
        struct posix_epoll_entry *entry = &epoll->entries[map[i]];
        uint32_t revents = poll_to_epoll_events(pollfds[i].revents);
        if (revents == 0) {
            continue;
        }
        events[ready] = entry->event;
        events[ready].events = revents;
        if (epoll_trace_enabled()) {
            fprintf(stderr,
                "srvros-epoll: event fd=%d revents=0x%x data=0x%llx\n",
                entry->fd,
                revents,
                (unsigned long long)entry->event.data.u64);
        }
        ready++;
    }
    if (ready != 0 && epoll_trace_enabled()) {
        fprintf(stderr,
            "srvros-epoll: wait epfd=%d timeout=%d ready=%d watched=%d\n",
            epfd,
            timeout,
            ready,
            poll_count);
    }
    return ready;
}

int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const void *sigmask) {
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

int openat(int dirfd, const char *path, int flags, ...) {
    (void)dirfd;
    mode_t mode = 0;
    if ((flags & O_CREAT) != 0) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (dirfd != AT_FDCWD) {
        errno = ENOSYS;
        return -1;
    }
    return open(path, flags, mode);
}

int unlinkat(int dirfd, const char *path, int flags) {
    if (dirfd != AT_FDCWD || (flags & ~AT_REMOVEDIR) != 0) {
        errno = ENOSYS;
        return -1;
    }
    return (flags & AT_REMOVEDIR) != 0 ? rmdir(path) : unlink(path);
}

DIR *fdopendir(int fd) {
    (void)fd;
    errno = ENOSYS;
    return 0;
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
