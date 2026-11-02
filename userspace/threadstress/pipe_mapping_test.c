#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <srvros/sys.h>

#define CHECK(x) do { if (!(x)) { printf("threadstress: pipe-mapping line=%d errno=%d\n", __LINE__, errno); return -1; } } while (0)

struct pipe_transfer {
    int fd;
    char *mapping;
    int vector;
    unsigned entered;
    char first;
    ssize_t count;
    int error;
    struct srv_sched_trace trace;
};

static void *blocked_read(void *arg) {
    struct pipe_transfer *transfer = arg;
    struct iovec iov[] = {{&transfer->first, 1}, {transfer->mapping, 1}};
    if (srv_sched_trace(SRV_SCHED_TRACE_BEGIN, &transfer->trace)) return (void *)1;
    __atomic_store_n(&transfer->entered, 1, __ATOMIC_RELEASE);
    transfer->count = transfer->vector ? readv(transfer->fd, iov, 2) : read(transfer->fd, transfer->mapping, 2);
    transfer->error = errno;
    return srv_sched_trace(SRV_SCHED_TRACE_END, &transfer->trace) ? (void *)1 : NULL;
}

static int check_read_mapping(int vector) {
    int fds[2];
    CHECK(pipe(fds) == 0);
    struct pipe_transfer transfer = {.fd = fds[0], .vector = vector, .first = '?'};
    transfer.mapping = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    CHECK(transfer.mapping != MAP_FAILED);
    pthread_t reader;
    CHECK(pthread_create(&reader, NULL, blocked_read, &transfer) == 0);
    while (!__atomic_load_n(&transfer.entered, __ATOMIC_ACQUIRE)) sched_yield();
    /* Give the reader a turn. Its trace below must confirm it actually blocked. */
    usleep(30000);
    CHECK(munmap(transfer.mapping, 4096) == 0 && write(fds[1], "AB", 2) == 2);
    void *result;
    CHECK(pthread_join(reader, &result) == 0 && !result && transfer.trace.blocks > 0);
    char bytes[2] = {0};
    CHECK(close(fds[1]) == 0);
    ssize_t recovered = read(fds[0], bytes, 2);
    if (transfer.count != -1 || transfer.error != EFAULT || transfer.first != '?' ||
        recovered != 2 || memcmp(bytes, "AB", 2)) {
        printf("threadstress: pipe-mapping %s returned=%ld errno=%d first=%c recovered=%ld\n",
            vector ? "readv" : "read", (long)transfer.count, transfer.error, transfer.first, (long)recovered);
        return -1;
    }
    CHECK(close(fds[0]) == 0);
    return 0;
}

static void *blocked_write(void *arg) {
    struct pipe_transfer *transfer = arg;
    transfer->count = write(transfer->fd, transfer->mapping, PIPE_BUF + 1);
    return NULL;
}

static int check_write_mapping(void) {
    int fds[2];
    CHECK(pipe2(fds, O_NONBLOCK) == 0);
    char drain[PIPE_BUF] = {0};
    ssize_t n;
    size_t queued = 0;
    while ((n = write(fds[1], drain, sizeof(drain))) > 0) queued += (size_t)n;
    CHECK(n == -1 && errno == EAGAIN && queued >= sizeof(drain));
    CHECK(read(fds[0], drain, 1) == 1 && fcntl(fds[1], F_SETFL, 0) == 0);

    size_t size = PIPE_BUF + 1;
    char *mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    CHECK(mapping != MAP_FAILED);
    memset(mapping, 'P', size);
    struct pipe_transfer transfer = {.fd = fds[1], .mapping = mapping};
    pthread_t writer;
    CHECK(pthread_create(&writer, NULL, blocked_write, &transfer) == 0);
    /* The newly full pipe proves the syscall has already transferred one byte. */
    struct pollfd pfd = {.fd = fds[1], .events = POLLOUT};
    long start = srv_ticks();
    int ready;
    while ((ready = poll(&pfd, 1, 0)) > 0 && srv_ticks() - start < 150) sched_yield();
    CHECK(ready == 0 && munmap(mapping, size) == 0);
    CHECK(read(fds[0], drain, sizeof(drain)) == sizeof(drain));
    CHECK(pthread_join(writer, NULL) == 0 && transfer.count == 1);
    CHECK(close(fds[1]) == 0);
    size_t remaining = queued - sizeof(drain);
    char last = 0;
    while ((n = read(fds[0], drain, sizeof(drain))) > 0) {
        CHECK((size_t)n <= remaining);
        remaining -= (size_t)n;
        last = drain[n - 1];
    }
    CHECK(n == 0 && remaining == 0 && last == 'P' && close(fds[0]) == 0);
    return 0;
}

int posix_pipe_mapping_test(void) {
    CHECK(check_read_mapping(1) == 0);
    CHECK(check_read_mapping(0) == 0);
    CHECK(check_write_mapping() == 0);
    puts("threadstress: pipe-mapping ok");
    return 0;
}
