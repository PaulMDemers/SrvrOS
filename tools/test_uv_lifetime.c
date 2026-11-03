/* Include the actual bridge so this test can gate its private wakeup path. */
#include <sys/types.h>
#include <stddef.h>
ssize_t uv_test_write(int fd, const void *data, size_t size);
int uv_test_pipe2(int fds[2], int flags);
#define write uv_test_write
#define pipe2 uv_test_pipe2
#include "../ports/srvros/uv.c"
#undef write
#undef pipe2

static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static int wake_entered, wake_release, callback_count;
static int pipe_flags;
int uv_test_pipe2(int fds[2], int flags) { pipe_flags = flags; fds[0] = 123; fds[1] = 124; return 0; }

ssize_t uv_test_write(int fd, const void *data, size_t size) {
    (void)fd; (void)data;
    pthread_mutex_lock(&gate_mutex);
    wake_entered = 1;
    pthread_cond_broadcast(&gate_cond);
    while (!wake_release) pthread_cond_wait(&gate_cond, &gate_mutex);
    pthread_mutex_unlock(&gate_mutex);
    return (ssize_t)size;
}
static void work_cb(uv_work_t *work) { (void)work; }
static void after_cb(uv_work_t *work, int status) {
    (void)work; if (!status) callback_count++;
}
static int async_count;
static void async_cb(uv_async_t *async) {
    async_count++;
    if (async_count == 1) uv_async_send(async);
}
static void *worker(void *arg) { work_thread_main(arg); return NULL; }
static void *async_sender(void *arg) { uv_async_send(arg); return NULL; }
struct fs_args { uv_loop_t *loop; uv_fs_t *request; };
static void *fs_worker(void *arg) {
    struct fs_args *args = arg; fs_queue_done(args->loop, args->request); return NULL;
}
int main(void) {
    uv_loop_t initialized;
    if (uv_loop_init(&initialized) || initialized.wake_read_fd != 123 ||
        initialized.wake_write_fd != 124 || !(pipe_flags & O_NONBLOCK)) return 10;
    pthread_mutex_destroy(&initialized.queue_mutex);
    uv_loop_t loop = {0}; uv_work_t request = {0}; pthread_t thread;
    if (pthread_mutex_init(&loop.queue_mutex, NULL)) return 1;
    loop.wake_write_fd = 1;
    request.loop = &loop; request.work_cb = work_cb; request.after_work_cb = after_cb;
    loop.work_queue = &request;
    if (pthread_create(&thread, NULL, worker, &request)) return 2;
    pthread_mutex_lock(&gate_mutex);
    while (!wake_entered) pthread_cond_wait(&gate_cond, &gate_mutex);
    pthread_mutex_unlock(&gate_mutex);
    /* Completion must remain protected until the worker finishes waking the
     * loop, otherwise a callback can free the request or close the loop. */
    int held = pthread_mutex_trylock(&loop.queue_mutex);
    if (!held) pthread_mutex_unlock(&loop.queue_mutex);
    pthread_mutex_lock(&gate_mutex); wake_release = 1;
    pthread_cond_broadcast(&gate_cond); pthread_mutex_unlock(&gate_mutex);
    pthread_join(thread, NULL);
    if (held != EBUSY) return 3;
    run_done_work(&loop);
    if (callback_count != 1 || loop.work_queue) return 4;
    uv_fs_t fs = {0}; struct fs_args args = {&loop, &fs};
    pthread_mutex_lock(&gate_mutex); wake_entered = wake_release = 0; pthread_mutex_unlock(&gate_mutex);
    if (pthread_create(&thread, NULL, fs_worker, &args)) return 5;
    pthread_mutex_lock(&gate_mutex);
    while (!wake_entered) pthread_cond_wait(&gate_cond, &gate_mutex);
    pthread_mutex_unlock(&gate_mutex);
    held = pthread_mutex_trylock(&loop.queue_mutex);
    if (!held) pthread_mutex_unlock(&loop.queue_mutex);
    pthread_mutex_lock(&gate_mutex); wake_release = 1;
    pthread_cond_broadcast(&gate_cond); pthread_mutex_unlock(&gate_mutex);
    pthread_join(thread, NULL);
    if (held != EBUSY || loop.fs_queue != &fs) return 6;
    pthread_mutex_destroy(&loop.queue_mutex);
    uv_loop_t async_loop = {0}; uv_async_t async;
    if (pthread_mutex_init(&async_loop.queue_mutex, NULL)) return 11;
    async_loop.wake_write_fd = 1;
    if (uv_async_init(&async_loop, &async, async_cb) || !async.handle.active) return 7;
    pthread_mutex_lock(&gate_mutex); wake_entered = wake_release = 0; pthread_mutex_unlock(&gate_mutex);
    if (pthread_create(&thread, NULL, async_sender, &async)) return 8;
    pthread_mutex_lock(&gate_mutex);
    while (!wake_entered) pthread_cond_wait(&gate_cond, &gate_mutex);
    pthread_mutex_unlock(&gate_mutex);
    held = pthread_mutex_trylock(&async_loop.queue_mutex);
    if (!held) pthread_mutex_unlock(&async_loop.queue_mutex);
    pthread_mutex_lock(&gate_mutex); wake_release = 1;
    pthread_cond_broadcast(&gate_cond); pthread_mutex_unlock(&gate_mutex);
    pthread_join(thread, NULL);
    if (held != EBUSY) return 12;
    async_loop.wake_write_fd = -1;
    run_pending_async(&async_loop);
    run_pending_async(&async_loop);
    if (async_count != 2) return 9;
    pthread_mutex_destroy(&async_loop.queue_mutex);
    return 0;
}
