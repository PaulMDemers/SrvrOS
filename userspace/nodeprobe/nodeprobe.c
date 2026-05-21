#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <uv.h>

static pthread_mutex_t probe_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t probe_cond = PTHREAD_COND_INITIALIZER;
static pthread_once_t probe_once = PTHREAD_ONCE_INIT;
static pthread_key_t probe_key;
static int probe_once_count;
static int probe_ready;

static void fail(const char *name) {
    printf("nodeprobe: %s failed errno=%d\n", name, errno);
}

static void once_init(void) {
    probe_once_count++;
}

static void *thread_worker(void *arg) {
    pthread_setspecific(probe_key, arg);
    if (pthread_getspecific(probe_key) != arg) {
        return (void *)0xbad;
    }
    if (pthread_once(&probe_once, once_init) != 0) {
        return (void *)0xbad;
    }
    pthread_mutex_lock(&probe_mutex);
    probe_ready = 1;
    pthread_cond_signal(&probe_cond);
    pthread_mutex_unlock(&probe_mutex);
    return (void *)0x4e;
}

static int check_time_random(void) {
    struct timespec realtime;
    struct timespec monotonic;
    unsigned char random_bytes[16];
    unsigned char combined = 0;
    if (clock_gettime(CLOCK_REALTIME, &realtime) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &monotonic) != 0 ||
        monotonic.tv_nsec < 0 ||
        monotonic.tv_nsec >= 1000000000L) {
        fail("clock_gettime");
        return -1;
    }
    if (getrandom(random_bytes, sizeof(random_bytes), 0) != (ssize_t)sizeof(random_bytes)) {
        fail("getrandom");
        return -1;
    }
    for (size_t i = 0; i < sizeof(random_bytes); i++) {
        combined |= random_bytes[i];
    }
    if (combined == 0) {
        fail("random-data");
        return -1;
    }
    puts("nodeprobe: time/random ok");
    return 0;
}

static int check_mmap(void) {
    const size_t page = 4096;
    unsigned char *anon = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon == MAP_FAILED) {
        fail("mmap-anon");
        return -1;
    }
    for (size_t i = 0; i < page; i++) {
        anon[i] = (unsigned char)(i & 0xff);
    }
    if (mprotect(anon, page, PROT_READ) != 0 || msync(anon, page, MS_SYNC) != 0) {
        fail("mprotect-msync");
        munmap(anon, page);
        return -1;
    }
    if (madvise(anon, page, MADV_DONTNEED) != 0) {
        fail("madvise");
        munmap(anon, page);
        return -1;
    }
    if (munmap(anon, page) != 0) {
        fail("munmap-anon");
        return -1;
    }

    int fd = open("/fat/nodeprobe-map.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) {
        fail("open-map");
        return -1;
    }
    const char *text = "node-map";
    if (write(fd, text, strlen(text)) != (ssize_t)strlen(text)) {
        fail("write-map");
        close(fd);
        return -1;
    }
    void *file_map = mmap(0, page, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_map == MAP_FAILED || memcmp(file_map, text, strlen(text)) != 0) {
        fail("mmap-file");
        close(fd);
        return -1;
    }
    if (munmap(file_map, page) != 0) {
        fail("munmap-file");
        close(fd);
        return -1;
    }
    close(fd);
    puts("nodeprobe: mmap ok");
    return 0;
}

static int check_files(void) {
    char template_path[] = "/fat/nodeprobe-XXXXXX";
    int fd = mkostemp(template_path, O_CLOEXEC);
    if (fd < 0) {
        fail("mkostemp");
        return -1;
    }
    int duped = fcntl(fd, F_DUPFD_CLOEXEC, 16);
    if (duped < 16) {
        fail("fcntl-dupfd-cloexec");
        close(fd);
        return -1;
    }
    struct iovec vec[2];
    vec[0].iov_base = "node";
    vec[0].iov_len = 4;
    vec[1].iov_base = "probe";
    vec[1].iov_len = 5;
    if (writev(duped, vec, 2) != 9) {
        fail("writev");
        close(duped);
        close(fd);
        return -1;
    }
    char resolved[128];
    if (realpath(template_path, resolved) == 0 || strncmp(resolved, "/fat/nodeprobe-", 15) != 0) {
        fail("realpath");
        close(duped);
        close(fd);
        return -1;
    }
    close(duped);
    close(fd);
    unlink(template_path);
    puts("nodeprobe: fs/fd ok");
    return 0;
}

static int check_threads(void) {
    pthread_attr_t attr;
    pthread_t thread;
    void *value = 0;
    size_t stack_size = 0;
    if (pthread_key_create(&probe_key, 0) != 0 ||
        pthread_attr_init(&attr) != 0 ||
        pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN * 2) != 0 ||
        pthread_attr_getstacksize(&attr, &stack_size) != 0 ||
        stack_size < PTHREAD_STACK_MIN * 2) {
        fail("pthread-setup");
        return -1;
    }
    pthread_attr_t self_attr;
    if (pthread_getattr_np(pthread_self(), &self_attr) != 0 ||
        pthread_attr_getstacksize(&self_attr, &stack_size) != 0 ||
        stack_size < PTHREAD_STACK_MIN) {
        fail("pthread-getattr");
        return -1;
    }
    if (pthread_create(&thread, &attr, thread_worker, (void *)0x1234) != 0) {
        fail("pthread-create");
        return -1;
    }
    pthread_mutex_lock(&probe_mutex);
    while (!probe_ready) {
        pthread_cond_wait(&probe_cond, &probe_mutex);
    }
    pthread_mutex_unlock(&probe_mutex);
    if (pthread_join(thread, &value) != 0 || value != (void *)0x4e || probe_once_count != 1) {
        fail("pthread-join");
        return -1;
    }
    pthread_attr_destroy(&attr);
    pthread_key_delete(probe_key);
    puts("nodeprobe: pthread ok");
    return 0;
}

static int check_resource_limits(void) {
    struct rlimit limit;
    struct rusage usage;
    struct sysinfo system_info;
    cpu_set_t cpu_set;
    char thread_name[16];
    CPU_ZERO(&cpu_set);
    if (sysconf(_SC_NPROCESSORS_CONF) < 1 ||
        sysconf(_SC_HOST_NAME_MAX) < 1 ||
        getauxval(AT_PAGESZ) != 4096 ||
        getauxval(AT_SECURE) != 0 ||
        sysinfo(&system_info) != 0 ||
        system_info.totalram == 0 ||
        system_info.mem_unit == 0 ||
        sched_getaffinity(0, sizeof(cpu_set), &cpu_set) != 0 ||
        CPU_COUNT(&cpu_set) != 1 ||
        !CPU_ISSET(0, &cpu_set) ||
        sched_setaffinity(0, sizeof(cpu_set), &cpu_set) != 0 ||
        prctl(PR_SET_NAME, "nodeprobe") != 0 ||
        prctl(PR_GET_NAME, thread_name) != 0 ||
        syscall(SYS_gettid) <= 0 ||
        getrlimit(RLIMIT_NOFILE, &limit) != 0 ||
        limit.rlim_cur < 16 ||
        limit.rlim_cur > limit.rlim_max ||
        getrlimit(RLIMIT_STACK, &limit) != 0 ||
        limit.rlim_cur < PTHREAD_STACK_MIN ||
        getrusage(RUSAGE_SELF, &usage) != 0 ||
        usage.ru_utime.tv_usec < 0 ||
        usage.ru_utime.tv_usec >= 1000000 ||
        getrusage(RUSAGE_THREAD, &usage) != 0 ||
        getrusage(RUSAGE_CHILDREN, &usage) != 0) {
        fail("resource");
        return -1;
    }
    if (MAXHOSTNAMELEN < 16 || MAXPATHLEN < 64) {
        fail("param");
        return -1;
    }
    puts("nodeprobe: resource ok");
    return 0;
}

static int check_sockets(void) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) != 0) {
        fail("socketpair");
        return -1;
    }
    if (send(fds[0], "n", 1, 0) != 1) {
        fail("socketpair-send");
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    struct pollfd pfd = {.fd = fds[1], .events = POLLIN, .revents = 0};
    if (poll(&pfd, 1, 100) != 1 || (pfd.revents & POLLIN) == 0) {
        fail("socketpair-poll");
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    char c = 0;
    if (recv(fds[1], &c, 1, 0) != 1 || c != 'n') {
        fail("socketpair-recv");
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    close(fds[0]);
    close(fds[1]);

    struct addrinfo hints;
    struct addrinfo *info = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("127.0.0.1", "80", &hints, &info) != 0 || info == 0) {
        fail("getaddrinfo");
        return -1;
    }
    freeaddrinfo(info);
    puts("nodeprobe: socket/dns ok");
    return 0;
}

static int check_uv_and_stubs(void) {
    void *frames[4];
    if (uv_version() == 0 || strcmp(uv_version_string(), "1.52.1") != 0) {
        fail("uv-version");
        return -1;
    }
    if (backtrace(frames, 4) != 0) {
        fail("backtrace-stub");
        return -1;
    }
    if (dlopen("/fat/lib/nope.so", RTLD_NOW) != 0 || dlerror() == 0) {
        fail("dlopen-stub");
        return -1;
    }
    puts("nodeprobe: uv/diagnostic stubs ok");
    return 0;
}

int main(void) {
    puts("nodeprobe: start");
    if (check_time_random() != 0 ||
        check_mmap() != 0 ||
        check_files() != 0 ||
        check_threads() != 0 ||
        check_resource_limits() != 0 ||
        check_sockets() != 0 ||
        check_uv_and_stubs() != 0) {
        return 1;
    }
    puts("nodeprobe: ok");
    return 0;
}
