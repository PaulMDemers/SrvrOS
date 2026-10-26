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
#include <sys/eventfd.h>
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
#define POSIX_EVENTFD_COUNTER_MAX (UINT64_MAX - 1)
#define POSIX_EPOLL_MAX 8
#define POSIX_EPOLL_ENTRIES 64

struct posix_eventfd {
    int used;
    int semaphore;
    uint64_t counter;
    uint64_t fd_flags;
    uint64_t descriptor_flags;
};

struct posix_epoll_entry {
    int used;
    int fd;
    uint64_t identity, revision;
    struct epoll_event event;
};

struct posix_epoll {
    int used;
    uint64_t identity;
    struct posix_epoll_entry entries[POSIX_EPOLL_ENTRIES];
};

static struct posix_eventfd eventfds[POSIX_EVENTFD_MAX];
static struct posix_epoll epolls[POSIX_EPOLL_MAX];

/* Libc-owned identities cover the native table and existing pseudo namespaces.
 * All public close/dup variants serialize their syscall plus bookkeeping here.
 * Direct srv_* descriptor mutations are outside this adapter contract. */
#define EVENT_NATIVE_FDS 67
#define EVENT_SOCKET_BASE 1000
#define EVENT_SOCKET_FDS 8
#define EVENT_FDS (EVENT_NATIVE_FDS + EVENT_SOCKET_FDS + POSIX_EVENTFD_MAX)
static uint64_t event_identities[EVENT_FDS];
static uint64_t event_serial;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
int __posix_socket_is_pseudo(int fd);
void __posix_event_lock(void) { (void)pthread_mutex_lock(&event_mutex); }
void __posix_event_unlock(void) { (void)pthread_mutex_unlock(&event_mutex); }
static int event_index(int fd) {
    if (fd >= 0 && fd < EVENT_NATIVE_FDS) return fd;
    if (fd >= EVENT_SOCKET_BASE && fd < EVENT_SOCKET_BASE + EVENT_SOCKET_FDS)
        return EVENT_NATIVE_FDS + fd - EVENT_SOCKET_BASE;
    if (fd >= POSIX_EVENTFD_BASE && fd < POSIX_EVENTFD_BASE + POSIX_EVENTFD_MAX)
        return EVENT_NATIVE_FDS + EVENT_SOCKET_FDS + fd - POSIX_EVENTFD_BASE;
    return -1;
}
static int event_fd(int index) {
    if (index < EVENT_NATIVE_FDS) return index;
    if (index < EVENT_NATIVE_FDS + EVENT_SOCKET_FDS)
        return EVENT_SOCKET_BASE + index - EVENT_NATIVE_FDS;
    return POSIX_EVENTFD_BASE + index - EVENT_NATIVE_FDS - EVENT_SOCKET_FDS;
}
static uint64_t event_next_serial(void) {
    if (event_serial == UINT64_MAX) { errno = EOVERFLOW; return 0; }
    return ++event_serial;
}
static uint64_t event_identity(int fd, int create) {
    int index = event_index(fd);
    if (index < 0) { errno = EBADF; return 0; }
    if (!event_identities[index] && create) {
        int valid = fd < EVENT_NATIVE_FDS ? srv_fcntl(fd, SRV_F_GETFD, 0) >= 0 :
            fd < POSIX_EVENTFD_BASE ? __posix_socket_is_pseudo(fd) :
            eventfds[fd - POSIX_EVENTFD_BASE].used;
        if (!valid) { errno = EBADF; return 0; }
        event_identities[index] = event_next_serial();
    }
    return event_identities[index];
}
/* Called with the lifecycle lock held; aliases can predate the first watch. */
int __posix_event_prepare_dup(int fd) {
    if (fd >= POSIX_EVENTFD_BASE && fd < POSIX_EVENTFD_BASE + POSIX_EVENTFD_MAX &&
        eventfds[fd - POSIX_EVENTFD_BASE].used) { errno = EOPNOTSUPP; return 0; }
    return event_identity(fd, 1) != 0;
}
static int event_alias(uint64_t identity) {
    if (identity) for (int i = 0; i < EVENT_FDS; i++)
        if (event_identities[i] == identity) return event_fd(i);
    return -1;
}
void __posix_event_note_close(int fd) {
    int index = event_index(fd);
    if (index < 0) return;
    uint64_t identity = event_identities[index];
    event_identities[index] = 0;
    if (!identity || event_alias(identity) >= 0) return;
    for (int i = 0; i < POSIX_EPOLL_MAX; i++) {
        if (epolls[i].used && epolls[i].identity == identity)
            memset(&epolls[i], 0, sizeof(epolls[i]));
        else for (int j = 0; j < POSIX_EPOLL_ENTRIES; j++)
            if (epolls[i].entries[j].used && epolls[i].entries[j].identity == identity)
                memset(&epolls[i].entries[j], 0, sizeof(epolls[i].entries[j]));
    }
}
void __posix_event_note_dup(int old_fd, int new_fd) {
    if (old_fd == new_fd) return;
    uint64_t identity = event_identity(old_fd, 0);
    __posix_event_note_close(new_fd);
    int index = event_index(new_fd);
    if (index >= 0) event_identities[index] = identity;
}

static int __posix_node_fork_ready_locked(void) {
    for(unsigned i=0;i<POSIX_EVENTFD_MAX;i++) if(eventfds[i].used) return 0;
    for(unsigned i=0;i<POSIX_EPOLL_MAX;i++) if(epolls[i].used) return 0;
    return 1;
}
int __posix_node_fork_ready(void) {
    __posix_event_lock();
    int result = __posix_node_fork_ready_locked();
    __posix_event_unlock();
    return result;
}


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
    return __atomic_load_n(&slot->used, __ATOMIC_ACQUIRE) ? slot : 0;
}

static int eventfd_locked(unsigned int initval, int flags) {
    if (flags & ~(EFD_SEMAPHORE | EFD_NONBLOCK | EFD_CLOEXEC)) {
        errno = EINVAL;
        return -1;
    }
    for (int i = 0; i < POSIX_EVENTFD_MAX; i++) {
        if (eventfds[i].used) {
            continue;
        }
        eventfds[i].counter = initval;
        eventfds[i].semaphore = (flags & EFD_SEMAPHORE) != 0;
        eventfds[i].fd_flags = (flags & EFD_NONBLOCK) != 0 ? SRV_FD_NONBLOCK : 0;
        eventfds[i].descriptor_flags = (flags & EFD_CLOEXEC) != 0 ? SRV_FD_CLOEXEC : 0;
        __atomic_store_n(&eventfds[i].used, 1, __ATOMIC_RELEASE);
        return POSIX_EVENTFD_BASE + i;
    }
    errno = EMFILE;
    return -1;
}
int eventfd(unsigned int initval, int flags) {
    __posix_event_lock();
    int result = eventfd_locked(initval, flags);
    __posix_event_unlock();
    return result;
}


int __posix_eventfd_is_pseudo(int fd) {
    return eventfd_slot(fd) != 0;
}

static ssize_t __posix_eventfd_read_locked(int fd, void *buffer, size_t length) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0 || buffer == 0 || length < sizeof(uint64_t)) {
        errno = slot == 0 ? EBADF : EINVAL;
        return -1;
    }
    if (slot->counter == 0) {
        errno = (slot->fd_flags & SRV_FD_NONBLOCK) != 0 ? EAGAIN : EWOULDBLOCK;
        return -1;
    }
    uint64_t value = slot->semaphore ? 1 : slot->counter;
    slot->counter -= value;
    memcpy(buffer, &value, sizeof(value));
    return (ssize_t)sizeof(value);
}
ssize_t __posix_eventfd_read(int fd, void *buffer, size_t length) {
    __posix_event_lock();
    ssize_t result = __posix_eventfd_read_locked(fd, buffer, length);
    __posix_event_unlock();
    return result;
}


static ssize_t __posix_eventfd_write_locked(int fd, const void *buffer, size_t length) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0 || buffer == 0 || length < sizeof(uint64_t)) {
        errno = slot == 0 ? EBADF : EINVAL;
        return -1;
    }
    uint64_t value = 0;
    memcpy(&value, buffer, sizeof(value));
    if (value == UINT64_MAX) { errno = EINVAL; return -1; }
    /* Test capacity before addition, under the same lock as drain/readiness.
     * Blocking I/O is not implemented by this compatibility adapter yet. */
    if (value > POSIX_EVENTFD_COUNTER_MAX - slot->counter) {
        errno = EAGAIN; return -1;
    }
    slot->counter += value;
    return (ssize_t)sizeof(value);
}
ssize_t __posix_eventfd_write(int fd, const void *buffer, size_t length) {
    __posix_event_lock();
    ssize_t result = __posix_eventfd_write_locked(fd, buffer, length);
    __posix_event_unlock();
    return result;
}


static int __posix_eventfd_close_locked(int fd) {
    struct posix_eventfd *slot = eventfd_slot(fd);
    if (slot == 0) {
        errno = EBADF;
        return -1;
    }
    __posix_event_note_close(fd);
    slot->counter = slot->fd_flags = slot->descriptor_flags = 0;
    slot->semaphore = 0;
    __atomic_store_n(&slot->used, 0, __ATOMIC_RELEASE);
    return 0;
}
int __posix_eventfd_close(int fd) {
    __posix_event_lock();
    int result = __posix_eventfd_close_locked(fd);
    __posix_event_unlock();
    return result;
}


static int eventfd_fcntl_locked(int fd, int command, uint64_t flags) {
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

int __posix_eventfd_fcntl(int fd, int command, uint64_t flags) {
    __posix_event_lock();
    int result = eventfd_fcntl_locked(fd, command, flags);
    __posix_event_unlock();
    return result;
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
        if (epolls[i].used && epolls[i].identity == event_identity(fd, 0)) {
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
        if (epoll->entries[i].used && epoll->entries[i].fd == fd &&
            epoll->entries[i].identity == event_identity(fd, 0)) {
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
    if ((requested & POLLOUT) != 0 && slot->counter < POSIX_EVENTFD_COUNTER_MAX) {
        revents |= POLLOUT;
    }
    return revents;
}

static int epoll_create1_locked(int flags) {
    if ((flags & ~EPOLL_CLOEXEC) != 0) {
        errno = EINVAL;
        return -1;
    }
    int fds[2];
    if (pipe(fds) < 0) {
        return -1;
    }
    (void)srv_close(fds[1]);
    for (int i = 0; i < POSIX_EPOLL_MAX; i++) {
        if (epolls[i].used) {
            continue;
        }
        memset(&epolls[i], 0, sizeof(epolls[i]));
        uint64_t identity = event_identity(fds[0], 1);
        if (!identity) { (void)srv_close(fds[0]); return -1; }
        epolls[i].used = 1;
        epolls[i].identity = identity;
        if ((flags & EPOLL_CLOEXEC) != 0) {
            if (srv_fcntl(fds[0], SRV_F_SETFD, SRV_FD_CLOEXEC) < 0) {
                __posix_event_note_close(fds[0]);
                (void)srv_close(fds[0]);
                errno = EIO;
                return -1;
            }
        }
        return fds[0];
    }
    (void)srv_close(fds[0]);
    errno = EMFILE;
    return -1;
}

int epoll_create1(int flags) {
    __posix_event_lock();
    int result = epoll_create1_locked(flags);
    __posix_event_unlock();
    return result;
}

static int epoll_ctl_locked(int epfd, int op, int fd, struct epoll_event *event) {
    struct posix_epoll *epoll = epoll_slot(epfd);
    if (epoll == 0 || fd < 0 || (op != EPOLL_CTL_DEL && event == 0)) {
        errno = EINVAL;
        return -1;
    }

    uint64_t identity = event_identity(fd, 1);
    if (!identity) return -1;
    /* Nested epoll cannot be represented by polling its placeholder pipe. */
    if (epoll_slot(fd)) { errno = identity == epoll->identity ? EINVAL : EOPNOTSUPP; return -1; }
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
        uint64_t revision = event_next_serial();
        if (!revision) return -1;
        entry->event = *event;
        entry->revision = revision;
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
    uint64_t revision = event_next_serial();
    if (!revision) return -1;
    memset(entry, 0, sizeof(*entry));
    entry->identity = identity;
    entry->revision = revision;
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

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    __posix_event_lock();
    int result = epoll_ctl_locked(epfd, op, fd, event);
    __posix_event_unlock();
    return result;
}

/* Synthetic counters/control changes have no kernel poll notification. Bound
 * each blocking poll to 10 ms, then rebuild the snapshot. This is a level-
 * triggered compatibility adapter, not a kernel epoll notification queue. */
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    struct pollfd pollfds[POSIX_EPOLL_ENTRIES];
    struct posix_epoll_entry snapshots[POSIX_EPOLL_ENTRIES];
    int map[POSIX_EPOLL_ENTRIES], poll_map[POSIX_EPOLL_ENTRIES];
    struct timespec deadline = {0};
    if (!events || maxevents <= 0) { errno = EINVAL; return -1; }
    __posix_event_lock();
    struct posix_epoll *epoll = epoll_slot(epfd);
    if (!epoll) { __posix_event_unlock(); errno = EBADF; return -1; }
    uint64_t identity = epoll->identity;
    __posix_event_unlock();
    if (timeout > 0) {
        if (clock_gettime(CLOCK_MONOTONIC, &deadline) < 0) return -1;
        if (deadline.tv_sec > INT64_MAX - timeout / 1000 - 1) {
            errno = EOVERFLOW; return -1;
        }
        deadline.tv_sec += timeout / 1000;
        deadline.tv_nsec += (long)(timeout % 1000) * 1000000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++; deadline.tv_nsec -= 1000000000;
        }
    }
    for (;;) {
        uint32_t observed[POSIX_EPOLL_ENTRIES] = {0};
        int count = 0, poll_count = 0, ready = 0;
        __posix_event_lock();
        if (!epoll->used || epoll->identity != identity) {
            __posix_event_unlock(); errno = EBADF; return -1;
        }
        for (int i = 0; i < POSIX_EPOLL_ENTRIES && ready < maxevents; i++) {
            struct posix_epoll_entry *entry = &epoll->entries[i];
            if (!entry->used) continue;
            int fd = event_alias(entry->identity);
            short requested = epoll_to_poll_events(entry->event.events);
            if (fd < 0 || !requested) continue;
            snapshots[count] = *entry;
            map[count] = i;
            if (__posix_eventfd_is_pseudo(fd)) {
                observed[count] = poll_to_epoll_events(eventfd_poll_events(fd, requested));
                ready += observed[count] != 0;
            } else {
                pollfds[poll_count] = (struct pollfd){.fd=fd,.events=requested};
                poll_map[poll_count++] = count;
            }
            count++;
        }
        __posix_event_unlock();
        int wait_ms = ready || timeout == 0 ? 0 : 10;
        int expired = 0;
        if (timeout > 0) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
            if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
                expired = 1; wait_ms = 0;
            } else if (deadline.tv_sec - now.tv_sec <= 1) {
                int64_t ns = (deadline.tv_sec - now.tv_sec) * 1000000000 +
                    deadline.tv_nsec - now.tv_nsec;
                int64_t remaining = (ns + 999999) / 1000000;
                if (remaining < wait_ms) wait_ms = (int)remaining;
            }
        }
        epoll_trace_wait_entry(epfd, wait_ms, poll_count, ready, pollfds);
        /* poll with zero descriptors is an interruptible wait too. Never hold
         * the lifecycle lock across poll or its pending-signal dispatch. */
        int result = poll_count || wait_ms ? poll(pollfds, (nfds_t)poll_count, wait_ms) : 0;
        int saved_errno = errno;
        __posix_event_lock();
        if (!epoll->used || epoll->identity != identity) {
            __posix_event_unlock(); errno = EBADF; return -1;
        }
        if (result < 0) { __posix_event_unlock(); errno = saved_errno; return -1; }
        for (int i = 0; i < poll_count; i++) {
            int n = poll_map[i];
            /* Never attach recycled-number readiness to the old watch. */
            if (event_identity(pollfds[i].fd, 0) == snapshots[n].identity)
                observed[n] = poll_to_epoll_events(pollfds[i].revents);
        }
        ready = 0;
        for (int i = 0; i < count && ready < maxevents; i++) {
            struct posix_epoll_entry *entry = &epoll->entries[map[i]];
            if (!entry->used || entry->revision != snapshots[i].revision ||
                entry->identity != snapshots[i].identity) continue;
            int fd = event_alias(entry->identity);
            if (__posix_eventfd_is_pseudo(fd))
                observed[i] = poll_to_epoll_events(eventfd_poll_events(fd,
                    epoll_to_poll_events(entry->event.events)));
            if (!observed[i]) continue;
            events[ready] = snapshots[i].event;
            events[ready++].events = observed[i];
        }
        __posix_event_unlock();
        if (ready || timeout == 0 || expired) return ready;
        /* Spurious wakes and retired snapshots must not end an infinite wait
         * or restart a finite timeout. Rebuild against the original deadline. */
    }
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


int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t *mask) {
    return sched_getaffinity((pid_t)thread, cpusetsize, mask) == 0 ? 0 : errno;
}

int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *mask) {
    return sched_setaffinity((pid_t)thread, cpusetsize, mask) == 0 ? 0 : errno;
}

int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param) {
    if(!policy || !param) return EINVAL;
    if(srv_thread_signal((uint64_t)(uintptr_t)thread,0)<0) return ESRCH;
    *policy=SCHED_OTHER;param->sched_priority=0;return 0;
}
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param) {
    if(!param || policy!=SCHED_OTHER || param->sched_priority!=0) return EINVAL;
    return srv_thread_signal((uint64_t)(uintptr_t)thread,0)<0?ESRCH:0;
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
