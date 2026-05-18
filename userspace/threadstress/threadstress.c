#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define WORKERS 4
#define YIELD_ITERS 512
#define MUTEX_ITERS 200
#define FD_ITERS 24
#define STDIO_ITERS 24

static volatile int yield_counts[WORKERS];
static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shared_cond = PTHREAD_COND_INITIALIZER;
static pthread_once_t shared_once = PTHREAD_ONCE_INIT;
static int shared_counter;
static int shared_waiters;
static int shared_phase;
static int shared_seen;
static int shared_once_count;
static int shared_detached_done;
static int shared_fd = -1;
static FILE *shared_stream;
static pthread_key_t shared_key;

static void say(const char *text) {
    write(STDOUT_FILENO, text, strlen(text));
}

static void say_fail(const char *test) {
    say("threadstress: ");
    say(test);
    say(" failed\n");
}

static int join_worker(pthread_t thread, uintptr_t expected) {
    void *value = 0;
    if (pthread_join(thread, &value) != 0) {
        return -1;
    }
    return value == (void *)expected ? 0 : -1;
}

static void *yield_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < YIELD_ITERS; i++) {
        yield_counts[id]++;
        sched_yield();
    }
    return (void *)(uintptr_t)(0x100 + id);
}

static int yield_test(void) {
    pthread_t threads[WORKERS];
    memset((void *)yield_counts, 0, sizeof(yield_counts));
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, yield_worker, (void *)(uintptr_t)i) != 0) {
            return -1;
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x100 + i)) != 0 ||
            yield_counts[i] != YIELD_ITERS) {
            return -1;
        }
    }
    say("threadstress: yield ok\n");
    return 0;
}

static void *mutex_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < MUTEX_ITERS; i++) {
        if (pthread_mutex_lock(&shared_mutex) != 0) {
            return (void *)0xbad;
        }
        shared_counter++;
        if (pthread_mutex_unlock(&shared_mutex) != 0) {
            return (void *)0xbad;
        }
        if (((i + id) & 7) == 0) {
            sched_yield();
        }
    }
    return (void *)(uintptr_t)(0x200 + id);
}

static int mutex_test(void) {
    pthread_t threads[WORKERS];
    shared_counter = 0;
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, mutex_worker, (void *)(uintptr_t)i) != 0) {
            return -1;
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x200 + i)) != 0) {
            return -1;
        }
    }
    if (shared_counter != WORKERS * MUTEX_ITERS) {
        return -1;
    }
    say("threadstress: mutex ok\n");
    return 0;
}

static void *cond_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    if (pthread_mutex_lock(&shared_mutex) != 0) {
        return (void *)0xbad;
    }
    shared_waiters++;
    pthread_cond_signal(&shared_cond);
    while (shared_phase == 0) {
        if (pthread_cond_wait(&shared_cond, &shared_mutex) != 0) {
            pthread_mutex_unlock(&shared_mutex);
            return (void *)0xbad;
        }
    }
    shared_seen++;
    pthread_mutex_unlock(&shared_mutex);
    return (void *)(uintptr_t)(0x300 + id);
}

static int cond_test(void) {
    pthread_t threads[WORKERS];
    shared_waiters = 0;
    shared_phase = 0;
    shared_seen = 0;
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, cond_worker, (void *)(uintptr_t)i) != 0) {
            return -1;
        }
    }
    for (;;) {
        pthread_mutex_lock(&shared_mutex);
        int ready = shared_waiters == WORKERS;
        pthread_mutex_unlock(&shared_mutex);
        if (ready) {
            break;
        }
        sched_yield();
    }
    pthread_mutex_lock(&shared_mutex);
    shared_phase = 1;
    pthread_cond_broadcast(&shared_cond);
    pthread_mutex_unlock(&shared_mutex);
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x300 + i)) != 0) {
            return -1;
        }
    }
    if (shared_seen != WORKERS) {
        return -1;
    }
    say("threadstress: cond ok\n");
    return 0;
}

static void once_init(void) {
    shared_once_count++;
    for (int i = 0; i < 8; i++) {
        sched_yield();
    }
}

static void *once_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < 16; i++) {
        if (pthread_once(&shared_once, once_init) != 0) {
            return (void *)0xbad;
        }
        sched_yield();
    }
    return (void *)(uintptr_t)(0x400 + id);
}

static int once_test(void) {
    pthread_t threads[WORKERS];
    shared_once_count = 0;
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, once_worker, (void *)(uintptr_t)i) != 0) {
            return -1;
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x400 + i)) != 0) {
            return -1;
        }
    }
    if (shared_once_count != 1) {
        return -1;
    }
    say("threadstress: once ok\n");
    return 0;
}

static int compat_test(void) {
    pthread_mutexattr_t mattr;
    pthread_mutex_t recursive;
    pthread_mutex_t errorcheck;
    pthread_cond_t cond;
    pthread_attr_t attr;
    pthread_condattr_t cattr;
    void *stackaddr = (void *)1;
    size_t stacksize = 0;
    int type = -1;
    struct timespec timeout;
    char token;

    if (pthread_key_create(&shared_key, 0) != 0 ||
        pthread_setspecific(shared_key, &token) != 0 ||
        pthread_getspecific(shared_key) != &token ||
        pthread_key_delete(shared_key) != 0) {
        return -1;
    }

    if (pthread_attr_init(&attr) != 0 ||
        pthread_attr_setstacksize(&attr, 32768) != 0 ||
        pthread_attr_getstacksize(&attr, &stacksize) != 0 ||
        stacksize != 32768 ||
        pthread_attr_getstack(&attr, &stackaddr, &stacksize) != 0 ||
        stackaddr != 0 ||
        stacksize != 32768 ||
        pthread_attr_destroy(&attr) != 0) {
        return -1;
    }

    if (pthread_mutexattr_init(&mattr) != 0 ||
        pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE) != 0 ||
        pthread_mutexattr_gettype(&mattr, &type) != 0 ||
        type != PTHREAD_MUTEX_RECURSIVE ||
        pthread_mutex_init(&recursive, &mattr) != 0 ||
        pthread_mutex_lock(&recursive) != 0 ||
        pthread_mutex_lock(&recursive) != 0 ||
        pthread_mutex_unlock(&recursive) != 0 ||
        pthread_mutex_unlock(&recursive) != 0 ||
        pthread_mutex_destroy(&recursive) != 0 ||
        pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK) != 0 ||
        pthread_mutex_init(&errorcheck, &mattr) != 0 ||
        pthread_mutex_lock(&errorcheck) != 0 ||
        pthread_mutex_lock(&errorcheck) != EDEADLK ||
        pthread_mutex_unlock(&errorcheck) != 0 ||
        pthread_mutex_destroy(&errorcheck) != 0 ||
        pthread_mutexattr_destroy(&mattr) != 0) {
        return -1;
    }

    if (pthread_condattr_init(&cattr) != 0 ||
        pthread_cond_init(&cond, &cattr) != 0 ||
        pthread_condattr_destroy(&cattr) != 0 ||
        clock_gettime(CLOCK_REALTIME, &timeout) != 0) {
        return -1;
    }
    timeout.tv_nsec += 20000000L;
    if (timeout.tv_nsec >= 1000000000L) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000L;
    }
    if (pthread_mutex_lock(&shared_mutex) != 0) {
        return -1;
    }
    int timed = pthread_cond_timedwait(&cond, &shared_mutex, &timeout);
    pthread_mutex_unlock(&shared_mutex);
    if (timed != ETIMEDOUT || pthread_cond_destroy(&cond) != 0) {
        return -1;
    }

    say("threadstress: compat ok\n");
    return 0;
}

static void *heap_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < 64; i++) {
        size_t size = 32 + (size_t)(((id + 1) * (i + 3) * 11) % 128);
        unsigned char *buffer = malloc(size);
        if (buffer == 0) {
            return (void *)0xbad;
        }
        memset(buffer, id + i, size);
        unsigned char *grown = realloc(buffer, size + 31);
        if (grown == 0) {
            free(buffer);
            return (void *)0xbad;
        }
        for (size_t j = 0; j < size; j++) {
            if (grown[j] != (unsigned char)(id + i)) {
                free(grown);
                return (void *)0xbad;
            }
        }
        unsigned char *zeroed = calloc(8, 8);
        if (zeroed == 0) {
            free(grown);
            return (void *)0xbad;
        }
        for (int j = 0; j < 64; j++) {
            if (zeroed[j] != 0) {
                free(zeroed);
                free(grown);
                return (void *)0xbad;
            }
        }
        free(zeroed);
        free(grown);
        if ((i & 3) == 0) {
            sched_yield();
        }
    }
    return (void *)(uintptr_t)(0x500 + id);
}

static int heap_test(void) {
    pthread_t threads[WORKERS];
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, heap_worker, (void *)(uintptr_t)i) != 0) {
            return -1;
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x500 + i)) != 0) {
            return -1;
        }
    }
    say("threadstress: heap ok\n");
    return 0;
}

static void *detached_worker(void *arg) {
    (void)arg;
    pthread_mutex_lock(&shared_mutex);
    shared_detached_done++;
    pthread_cond_signal(&shared_cond);
    pthread_mutex_unlock(&shared_mutex);
    return (void *)0x600;
}

static int detached_test(void) {
    pthread_attr_t attr;
    shared_detached_done = 0;
    if (pthread_attr_init(&attr) != 0 ||
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        return -1;
    }
    for (int i = 0; i < WORKERS; i++) {
        pthread_t thread;
        if (pthread_create(&thread, &attr, detached_worker, 0) != 0) {
            pthread_attr_destroy(&attr);
            return -1;
        }
    }
    pthread_attr_destroy(&attr);
    pthread_mutex_lock(&shared_mutex);
    while (shared_detached_done != WORKERS) {
        pthread_cond_wait(&shared_cond, &shared_mutex);
    }
    pthread_mutex_unlock(&shared_mutex);
    say("threadstress: detached ok\n");
    return 0;
}

static void *fd_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < FD_ITERS; i++) {
        char line[32];
        int len = snprintf(line, sizeof(line), "T%d:%02d\n", id, i);
        if (len <= 0 || len >= (int)sizeof(line)) {
            return (void *)0xbad;
        }
        ssize_t written = write(shared_fd, line, (size_t)len);
        if (written != len) {
            return (void *)0xbad;
        }
        if ((i & 3) == 0) {
            sched_yield();
        }
    }
    return (void *)(uintptr_t)(0x700 + id);
}

static int fd_test(void) {
    pthread_t threads[WORKERS];
    char buffer[WORKERS * FD_ITERS * 8];
    unlink("/fat/threadstress-fd.txt");
    shared_fd = open("/fat/threadstress-fd.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (shared_fd < 0) {
        return -1;
    }
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, fd_worker, (void *)(uintptr_t)i) != 0) {
            close(shared_fd);
            return -1;
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x700 + i)) != 0) {
            close(shared_fd);
            return -1;
        }
    }
    if (lseek(shared_fd, 0, SEEK_SET) < 0) {
        close(shared_fd);
        return -1;
    }
    ssize_t bytes = read(shared_fd, buffer, sizeof(buffer));
    close(shared_fd);
    unlink("/fat/threadstress-fd.txt");
    shared_fd = -1;
    if (bytes != WORKERS * FD_ITERS * 6) {
        return -1;
    }
    say("threadstress: fd ok\n");
    return 0;
}

static void *stdio_worker(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < STDIO_ITERS; i++) {
        if (fprintf(shared_stream, "S%d:%02d\n", id, i) != 6) {
            return (void *)0xbad;
        }
        if ((i & 3) == 0) {
            sched_yield();
        }
    }
    return (void *)(uintptr_t)(0x800 + id);
}

static int stdio_test(void) {
    pthread_t threads[WORKERS];
    char buffer[WORKERS * STDIO_ITERS * 8];
    unlink("/fat/threadstress-stdio.txt");
    shared_stream = fopen("/fat/threadstress-stdio.txt", "w+");
    if (shared_stream == 0) {
        return -1;
    }
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&threads[i], 0, stdio_worker, (void *)(uintptr_t)i) != 0) {
            fclose(shared_stream);
            return -1;
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (join_worker(threads[i], (uintptr_t)(0x800 + i)) != 0) {
            fclose(shared_stream);
            return -1;
        }
    }
    if (fflush(shared_stream) != 0 ||
        fseek(shared_stream, 0, SEEK_SET) != 0) {
        fclose(shared_stream);
        return -1;
    }
    size_t bytes = fread(buffer, 1, sizeof(buffer), shared_stream);
    fclose(shared_stream);
    unlink("/fat/threadstress-stdio.txt");
    shared_stream = 0;
    if (bytes != WORKERS * STDIO_ITERS * 6) {
        return -1;
    }
    for (size_t i = 0; i < bytes; i += 6) {
        if (buffer[i] != 'S' ||
            buffer[i + 1] < '0' || buffer[i + 1] > '0' + WORKERS - 1 ||
            buffer[i + 2] != ':' ||
            buffer[i + 3] < '0' || buffer[i + 3] > '9' ||
            buffer[i + 4] < '0' || buffer[i + 4] > '9' ||
            buffer[i + 5] != '\n') {
            return -1;
        }
    }
    say("threadstress: stdio ok\n");
    return 0;
}

int main(int argc, char **argv) {
    const char *only = argc > 1 ? argv[1] : "all";
    say("threadstress: start\n");

#define RUN_TEST(name, fn) \
    do { \
        if ((strcmp(only, "all") == 0 || strcmp(only, name) == 0) && fn() != 0) { \
            say_fail(name); \
            return 1; \
        } \
    } while (0)

    RUN_TEST("yield", yield_test);
    RUN_TEST("mutex", mutex_test);
    RUN_TEST("cond", cond_test);
    RUN_TEST("once", once_test);
    RUN_TEST("compat", compat_test);
    RUN_TEST("heap", heap_test);
    RUN_TEST("detached", detached_test);
    RUN_TEST("fd", fd_test);
    RUN_TEST("stdio", stdio_test);

#undef RUN_TEST

    say("threadstress: ok\n");
    return 0;
}
