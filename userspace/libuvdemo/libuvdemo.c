#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <uv.h>

static int version_test(void) {
    unsigned int expected = (UV_VERSION_MAJOR << 16) | (UV_VERSION_MINOR << 8) | UV_VERSION_PATCH;
    if (uv_version() != expected || strcmp(uv_version_string(), "1.52.1") != 0) {
        puts("libuvdemo: version failed");
        return 1;
    }
    printf("libuvdemo: upstream version %s\n", uv_version_string());
    return 0;
}

static int error_test(void) {
    char name[32];
    char message[64];
    if (UV_EINVAL >= 0 || uv_translate_sys_error(EINVAL) != UV_EINVAL) {
        puts("libuvdemo: error translate failed");
        return 1;
    }
    if (strcmp(uv_err_name(UV_EINVAL), "EINVAL") != 0 ||
        strcmp(uv_strerror(UV_EINVAL), "invalid argument") != 0 ||
        strcmp(uv_strerror(UV_EOF), "end of file") != 0) {
        puts("libuvdemo: error strings failed");
        return 1;
    }
    if (uv_err_name_r(UV_EINVAL, name, sizeof(name)) != name ||
        uv_strerror_r(UV_EINVAL, message, sizeof(message)) != message ||
        strcmp(name, "EINVAL") != 0 ||
        strcmp(message, "invalid argument") != 0) {
        puts("libuvdemo: error reentrant failed");
        return 1;
    }
    puts("libuvdemo: errors ok");
    return 0;
}

static int core_timer_seen;
static int core_close_seen;
static int core_walk_seen;
static int core_walk_arg_seen;

static void core_timer_cb(uv_timer_t *timer) {
    core_timer_seen++;
    uv_timer_stop(timer);
}

static void core_close_cb(uv_handle_t *handle) {
    if (uv_is_closing(handle)) {
        core_close_seen = 1;
    }
}

static void core_walk_cb(uv_handle_t *handle, void *arg) {
    int *token = arg;
    if (uv_handle_get_type(handle) == UV_TIMER && token != 0 && *token == 7) {
        core_walk_seen++;
        core_walk_arg_seen = 1;
    }
}

static int core_api_test(void) {
    uv_loop_t loop;
    uv_timer_t timer;
    uv_fs_t request;
    int loop_token = 1;
    int handle_token = 2;
    int req_token = 3;

    core_timer_seen = 0;
    core_close_seen = 0;
    core_walk_seen = 0;
    core_walk_arg_seen = 0;
    if (uv_loop_init(&loop) < 0 || uv_loop_close(&loop) != 0) {
        puts("libuvdemo: loop close failed");
        return 1;
    }
    if (uv_loop_init(&loop) < 0 || uv_timer_init(&loop, &timer) < 0) {
        puts("libuvdemo: core setup failed");
        return 1;
    }
    uv_loop_set_data(&loop, &loop_token);
    uv_handle_set_data((uv_handle_t *)&timer, &handle_token);
    if (uv_loop_get_data(&loop) != &loop_token ||
        uv_handle_get_data((uv_handle_t *)&timer) != &handle_token ||
        uv_handle_get_loop((uv_handle_t *)&timer) != &loop ||
        uv_handle_get_type((uv_handle_t *)&timer) != UV_TIMER ||
        strcmp(uv_handle_type_name(UV_TIMER), "timer") != 0 ||
        strcmp(uv_handle_type_name(UV_PREPARE), "prepare") != 0 ||
        strcmp(uv_handle_type_name(UV_CHECK), "check") != 0 ||
        strcmp(uv_handle_type_name(UV_IDLE), "idle") != 0 ||
        uv_handle_size(UV_TIMER) != sizeof(uv_timer_t) ||
        uv_handle_size(UV_PREPARE) != sizeof(uv_prepare_t) ||
        uv_handle_size(UV_CHECK) != sizeof(uv_check_t) ||
        uv_handle_size(UV_IDLE) != sizeof(uv_idle_t) ||
        uv_req_size(UV_FS) != sizeof(uv_fs_t) ||
        uv_req_size(UV_GETADDRINFO) != sizeof(uv_getaddrinfo_t) ||
        strcmp(uv_req_type_name(UV_FS), "fs") != 0 ||
        strcmp(uv_req_type_name(UV_GETADDRINFO), "getaddrinfo") != 0 ||
        uv_backend_fd(&loop) != UV_ENOSYS ||
        uv_is_active((uv_handle_t *)&timer) ||
        uv_is_closing((uv_handle_t *)&timer) ||
        !uv_has_ref((uv_handle_t *)&timer)) {
        puts("libuvdemo: core helpers failed");
        return 1;
    }
    if (uv_timer_start(&timer, core_timer_cb, 50, 0) < 0 ||
        !uv_is_active((uv_handle_t *)&timer) ||
        uv_loop_close(&loop) != UV_EBUSY ||
        uv_backend_timeout(&loop) < 0 ||
        uv_timer_get_due_in(&timer) > 50) {
        puts("libuvdemo: active helpers failed");
        return 1;
    }
    int walk_token = 7;
    uv_walk(&loop, core_walk_cb, &walk_token);
    uv_unref((uv_handle_t *)&timer);
    if (uv_loop_alive(&loop) || uv_has_ref((uv_handle_t *)&timer)) {
        puts("libuvdemo: unref failed");
        return 1;
    }
    uv_ref((uv_handle_t *)&timer);
    if (!uv_loop_alive(&loop) || !uv_has_ref((uv_handle_t *)&timer) ||
        core_walk_seen != 1 || !core_walk_arg_seen ||
        uv_fileno((uv_handle_t *)&timer, 0) != UV_EBADF) {
        puts("libuvdemo: ref/walk failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    if (core_timer_seen != 1 || uv_is_active((uv_handle_t *)&timer)) {
        puts("libuvdemo: active state failed");
        return 1;
    }
    uv_close((uv_handle_t *)&timer, core_close_cb);
    if (!core_close_seen || !uv_is_closing((uv_handle_t *)&timer)) {
        puts("libuvdemo: close state failed");
        return 1;
    }
    if (uv_fs_stat(&loop, &request, "/fat", 0) < 0 ||
        uv_req_get_type((uv_req_t *)&request) != UV_FS) {
        puts("libuvdemo: request type failed");
        return 1;
    }
    uv_req_set_data((uv_req_t *)&request, &req_token);
    if (uv_req_get_data((uv_req_t *)&request) != &req_token) {
        puts("libuvdemo: request data failed");
        return 1;
    }
    puts("libuvdemo: core api ok");
    return 0;
}

static int timer_seen;

static void timer_cb(uv_timer_t *timer) {
    timer_seen++;
    uv_timer_stop(timer);
}

static int timer_test(void) {
    uv_loop_t loop;
    uv_timer_t timer;
    timer_seen = 0;
    if (uv_loop_init(&loop) < 0 ||
        uv_timer_init(&loop, &timer) < 0 ||
        uv_timer_start(&timer, timer_cb, 1, 0) < 0) {
        puts("libuvdemo: timer setup failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    if (timer_seen != 1) {
        puts("libuvdemo: timer failed");
        return 1;
    }
    puts("libuvdemo: timer ok");
    return 0;
}

static uv_loop_t phase_loop;
static uv_prepare_t phase_prepare;
static uv_check_t phase_check;
static uv_idle_t phase_idle;
static int phase_prepare_seen;
static int phase_check_seen;
static int phase_idle_seen;

static void phase_idle_cb(uv_idle_t *handle) {
    phase_idle_seen++;
    if (phase_idle_seen >= 2) {
        uv_idle_stop(handle);
    }
}

static void phase_prepare_cb(uv_prepare_t *handle) {
    phase_prepare_seen++;
    if (phase_prepare_seen >= 2) {
        uv_prepare_stop(handle);
    }
}

static void phase_check_cb(uv_check_t *handle) {
    phase_check_seen++;
    if (phase_check_seen >= 2) {
        uv_check_stop(handle);
        uv_stop(&phase_loop);
    }
}

static int phase_test(void) {
    phase_prepare_seen = 0;
    phase_check_seen = 0;
    phase_idle_seen = 0;
    if (uv_loop_init(&phase_loop) < 0 ||
        uv_prepare_init(&phase_loop, &phase_prepare) < 0 ||
        uv_check_init(&phase_loop, &phase_check) < 0 ||
        uv_idle_init(&phase_loop, &phase_idle) < 0 ||
        uv_prepare_start(&phase_prepare, phase_prepare_cb) < 0 ||
        uv_check_start(&phase_check, phase_check_cb) < 0 ||
        uv_idle_start(&phase_idle, phase_idle_cb) < 0) {
        puts("libuvdemo: phase setup failed");
        return 1;
    }
    (void)uv_run(&phase_loop, UV_RUN_DEFAULT);
    if (phase_prepare_seen < 2 || phase_check_seen < 2 || phase_idle_seen < 2 ||
        uv_is_active((uv_handle_t *)&phase_prepare) ||
        uv_is_active((uv_handle_t *)&phase_check) ||
        uv_is_active((uv_handle_t *)&phase_idle)) {
        puts("libuvdemo: phase failed");
        return 1;
    }
    puts("libuvdemo: phases ok");
    return 0;
}

static uv_fs_t *fs_async_expected;
static int fs_async_seen;

static void fs_async_cb(uv_fs_t *async_req) {
    if (async_req == fs_async_expected &&
        uv_fs_get_type(async_req) == UV_FS_STAT &&
        uv_fs_get_result(async_req) == 0) {
        fs_async_seen = 1;
    }
    uv_stop(async_req->loop);
}

static int fs_test(void) {
    uv_loop_t loop;
    uv_fs_t request;
    uv_fs_t async_request;
    const char *path = "/fat/libuvdemo.txt";
    const char *renamed = "/fat/libuvdemo-renamed.txt";
    const char *dir = "/fat/libuvdemo-dir";
    char payload[] = "libuv-fs-ok";
    char readback[32];
    uv_buf_t buffer;
    int fd;
    int saw_bin = 0;
    int saw_echo = 0;
    fs_async_expected = 0;
    fs_async_seen = 0;

    if (uv_loop_init(&loop) < 0) {
        return 1;
    }
    (void)uv_fs_unlink(&loop, &request, path, 0);
    uv_fs_req_cleanup(&request);
    (void)uv_fs_unlink(&loop, &request, renamed, 0);
    uv_fs_req_cleanup(&request);
    (void)uv_fs_rmdir(&loop, &request, dir, 0);
    uv_fs_req_cleanup(&request);

    if (uv_fs_open(&loop, &request, path, O_CREAT | O_TRUNC | O_RDWR, 0644, 0) < 0) {
        puts("libuvdemo: fs open failed");
        return 1;
    }
    fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    buffer = uv_buf_init(payload, (unsigned int)strlen(payload));
    if (uv_fs_write(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("libuvdemo: fs write failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, fd, 0);
    uv_fs_req_cleanup(&request);

    if (uv_fs_open(&loop, &request, path, O_RDONLY, 0, 0) < 0) {
        puts("libuvdemo: fs reopen failed");
        return 1;
    }
    fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    memset(readback, 0, sizeof(readback));
    buffer = uv_buf_init(readback, sizeof(readback) - 1);
    if (uv_fs_read(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("libuvdemo: fs read failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, fd, 0);
    uv_fs_req_cleanup(&request);
    if (strcmp(readback, payload) != 0) {
        puts("libuvdemo: fs mismatch");
        return 1;
    }

    if (uv_fs_mkdir(&loop, &request, dir, 0755, 0) < 0) {
        puts("libuvdemo: fs metadata failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_rename(&loop, &request, path, renamed, 0) < 0) {
        puts("libuvdemo: fs metadata failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_stat(&loop, &request, renamed, 0) < 0 ||
        uv_fs_get_type(&request) != UV_FS_STAT ||
        uv_fs_get_result(&request) != 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload)) {
        puts("libuvdemo: fs stat failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_open(&loop, &request, renamed, O_RDONLY, 0, 0) < 0) {
        puts("libuvdemo: fs fstat open failed");
        return 1;
    }
    fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    if (uv_fs_fstat(&loop, &request, fd, 0) < 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload)) {
        puts("libuvdemo: fs fstat failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, fd, 0);
    uv_fs_req_cleanup(&request);
    if (uv_fs_lstat(&loop, &request, renamed, 0) < 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload)) {
        puts("libuvdemo: fs lstat failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_access(&loop, &request, renamed, F_OK | R_OK, 0) < 0) {
        puts("libuvdemo: fs access failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_realpath(&loop, &request, renamed, 0) < 0 ||
        strcmp((const char *)uv_fs_get_ptr(&request), renamed) != 0 ||
        strcmp(uv_fs_get_path(&request), renamed) != 0) {
        puts("libuvdemo: fs realpath failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_scandir(&loop, &request, "/fat/bin", 0, 0) < 0 ||
        uv_fs_get_result(&request) <= 0) {
        puts("libuvdemo: fs scandir failed");
        return 1;
    }
    uv_dirent_t entry;
    while (uv_fs_scandir_next(&request, &entry) == 0) {
        if (strcmp(entry.name, "echo") == 0 && entry.type == UV_DIRENT_FILE) {
            saw_echo = 1;
        }
        if (strcmp(entry.name, "sh") == 0 && entry.type == UV_DIRENT_FILE) {
            saw_bin = 1;
        }
    }
    uv_fs_req_cleanup(&request);
    if (!saw_bin || !saw_echo) {
        puts("libuvdemo: fs scandir contents failed");
        return 1;
    }
    memset(&async_request, 0, sizeof(async_request));
    fs_async_expected = &async_request;
    if (uv_fs_stat(&loop, &async_request, renamed, fs_async_cb) < 0) {
        puts("libuvdemo: fs async setup failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    uv_fs_req_cleanup(&async_request);
    if (!fs_async_seen) {
        puts("libuvdemo: fs async failed");
        return 1;
    }
    if (uv_fs_unlink(&loop, &request, renamed, 0) < 0) {
        puts("libuvdemo: fs cleanup failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_rmdir(&loop, &request, dir, 0) < 0) {
        puts("libuvdemo: fs cleanup failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    puts("libuvdemo: fs ok");
    return 0;
}

static uv_loop_t work_loop;
static uv_async_t async_handle;
static uv_work_t work_request;
static int work_value;
static int async_seen;
static int work_seen;

static void async_cb(uv_async_t *handle) {
    async_seen = 1;
    uv_close((uv_handle_t *)handle, 0);
}

static void work_cb(uv_work_t *request) {
    (void)request;
    work_value = 21 * 2;
    (void)uv_async_send(&async_handle);
}

static void after_work_cb(uv_work_t *request, int status) {
    (void)request;
    work_seen = status == 0 && work_value == 42;
    uv_stop(&work_loop);
}

static int work_test(void) {
    work_value = 0;
    async_seen = 0;
    work_seen = 0;
    if (uv_loop_init(&work_loop) < 0 ||
        uv_async_init(&work_loop, &async_handle, async_cb) < 0 ||
        uv_queue_work(&work_loop, &work_request, work_cb, after_work_cb) < 0) {
        puts("libuvdemo: work setup failed");
        return 1;
    }
    (void)uv_run(&work_loop, UV_RUN_DEFAULT);
    if (!async_seen || !work_seen) {
        puts("libuvdemo: work failed");
        return 1;
    }
    puts("libuvdemo: async ok");
    puts("libuvdemo: work ok");
    return 0;
}

static uv_mutex_t thread_mutex;
static uv_mutex_t thread_cond_mutex;
static uv_cond_t thread_cond;
static uv_sem_t thread_sem;
static uv_rwlock_t thread_rwlock;
static uv_barrier_t thread_barrier;
static uv_once_t thread_once_guard = UV_ONCE_INIT;
static uv_key_t thread_key;
static int thread_value;
static int thread_ready;
static int thread_go;
static int thread_once_count;
static int thread_barrier_serial_count;
static int thread_detached_value;

static void thread_once_cb(void) {
    thread_once_count++;
}

static void thread_worker(void *arg) {
    uv_thread_t self = uv_thread_self();
    if (!uv_thread_equal(&self, &self)) {
        thread_value = -100;
        return;
    }
    uv_key_set(&thread_key, arg);
    uv_mutex_lock(&thread_mutex);
    thread_value += (int)(uintptr_t)uv_key_get(&thread_key);
    uv_mutex_unlock(&thread_mutex);

    uv_mutex_lock(&thread_cond_mutex);
    thread_ready = 1;
    uv_cond_signal(&thread_cond);
    while (!thread_go) {
        uv_cond_wait(&thread_cond, &thread_cond_mutex);
    }
    uv_mutex_unlock(&thread_cond_mutex);
}

static void barrier_worker(void *arg) {
    (void)arg;
    if (uv_barrier_wait(&thread_barrier) != 0) {
        thread_barrier_serial_count++;
    }
}

static void detached_worker(void *arg) {
    thread_detached_value = (int)(uintptr_t)arg;
    uv_sem_post(&thread_sem);
}

static int thread_test(void) {
    uv_thread_t thread;
    uv_thread_t barrier_thread;
    uv_thread_t detached_thread;
    uv_thread_options_t options = {
        .flags = UV_THREAD_HAS_STACK_SIZE,
        .stack_size = PTHREAD_STACK_MIN,
    };
    thread_value = 0;
    thread_ready = 0;
    thread_go = 0;
    thread_once_count = 0;
    thread_barrier_serial_count = 0;
    thread_detached_value = 0;

    if (uv_mutex_init(&thread_mutex) < 0 ||
        uv_mutex_init(&thread_cond_mutex) < 0 ||
        uv_cond_init(&thread_cond) < 0 ||
        uv_sem_init(&thread_sem, 0) < 0 ||
        uv_rwlock_init(&thread_rwlock) < 0 ||
        uv_key_create(&thread_key) < 0) {
        puts("libuvdemo: thread setup failed");
        return 1;
    }

    uv_once(&thread_once_guard, thread_once_cb);
    uv_once(&thread_once_guard, thread_once_cb);
    if (thread_once_count != 1) {
        puts("libuvdemo: once failed");
        return 1;
    }

    uv_mutex_lock(&thread_mutex);
    if (uv_mutex_trylock(&thread_mutex) != UV_EBUSY) {
        puts("libuvdemo: mutex try failed");
        return 1;
    }
    uv_mutex_unlock(&thread_mutex);

    if (uv_sem_trywait(&thread_sem) != UV_EAGAIN) {
        puts("libuvdemo: sem empty failed");
        return 1;
    }
    uv_sem_post(&thread_sem);
    uv_sem_wait(&thread_sem);
    if (uv_sem_trywait(&thread_sem) != UV_EAGAIN) {
        puts("libuvdemo: sem wait failed");
        return 1;
    }

    uv_rwlock_rdlock(&thread_rwlock);
    if (uv_rwlock_trywrlock(&thread_rwlock) != UV_EBUSY) {
        puts("libuvdemo: rwlock read failed");
        return 1;
    }
    uv_rwlock_rdunlock(&thread_rwlock);
    uv_rwlock_wrlock(&thread_rwlock);
    if (uv_rwlock_tryrdlock(&thread_rwlock) != UV_EBUSY) {
        puts("libuvdemo: rwlock write failed");
        return 1;
    }
    uv_rwlock_wrunlock(&thread_rwlock);

    if (uv_thread_create(&thread, thread_worker, (void *)(uintptr_t)37) < 0) {
        puts("libuvdemo: thread create failed");
        return 1;
    }
    uv_mutex_lock(&thread_cond_mutex);
    while (!thread_ready) {
        uv_cond_wait(&thread_cond, &thread_cond_mutex);
    }
    thread_go = 1;
    uv_cond_signal(&thread_cond);
    uv_mutex_unlock(&thread_cond_mutex);
    if (uv_thread_join(&thread) < 0 || thread_value != 37) {
        puts("libuvdemo: thread join failed");
        return 1;
    }

    if (uv_thread_create_ex(&detached_thread, &options, detached_worker, (void *)(uintptr_t)11) < 0 ||
        uv_thread_detach(&detached_thread) < 0) {
        puts("libuvdemo: detached thread failed");
        return 1;
    }
    uv_sem_wait(&thread_sem);
    if (thread_detached_value != 11) {
        puts("libuvdemo: detached thread failed");
        return 1;
    }

    if (uv_barrier_init(&thread_barrier, 2) < 0 ||
        uv_thread_create(&barrier_thread, barrier_worker, 0) < 0) {
        puts("libuvdemo: barrier setup failed");
        return 1;
    }
    if (uv_barrier_wait(&thread_barrier) != 0) {
        thread_barrier_serial_count++;
    }
    if (uv_thread_join(&barrier_thread) < 0 || thread_barrier_serial_count != 1) {
        puts("libuvdemo: barrier failed");
        return 1;
    }

    uv_barrier_destroy(&thread_barrier);
    uv_key_delete(&thread_key);
    uv_rwlock_destroy(&thread_rwlock);
    uv_sem_destroy(&thread_sem);
    uv_cond_destroy(&thread_cond);
    uv_mutex_destroy(&thread_cond_mutex);
    uv_mutex_destroy(&thread_mutex);
    puts("libuvdemo: thread ok");
    return 0;
}

static uv_loop_t poll_loop;
static uv_poll_t poll_handle;
static int poll_fds[2] = {-1, -1};
static int poll_seen;

static void poll_cb(uv_poll_t *handle, int status, int events) {
    char buffer[32];
    ssize_t count;
    if (status < 0 || (events & UV_READABLE) == 0) {
        puts("libuvdemo: poll event failed");
        uv_stop(&poll_loop);
        return;
    }
    count = read(poll_fds[0], buffer, sizeof(buffer) - 1);
    if (count < 0) {
        puts("libuvdemo: poll read failed");
        uv_stop(&poll_loop);
        return;
    }
    buffer[count] = '\0';
    poll_seen = strcmp(buffer, "libuv-poll-ok") == 0;
    uv_poll_stop(handle);
    uv_stop(&poll_loop);
}

static int poll_test(void) {
    char message[] = "libuv-poll-ok";
    uv_os_fd_t fd = -1;
    poll_seen = 0;
    if (uv_loop_init(&poll_loop) < 0 ||
        pipe(poll_fds) < 0 ||
        uv_poll_init(&poll_loop, &poll_handle, poll_fds[0]) < 0 ||
        uv_poll_start(&poll_handle, UV_READABLE, poll_cb) < 0) {
        puts("libuvdemo: poll setup failed");
        return 1;
    }
    if (uv_fileno((uv_handle_t *)&poll_handle, &fd) < 0 || fd != poll_fds[0]) {
        puts("libuvdemo: poll fileno failed");
        uv_poll_stop(&poll_handle);
        close(poll_fds[0]);
        close(poll_fds[1]);
        return 1;
    }
    if (write(poll_fds[1], message, strlen(message)) != (ssize_t)strlen(message)) {
        puts("libuvdemo: poll write failed");
        uv_poll_stop(&poll_handle);
        close(poll_fds[0]);
        close(poll_fds[1]);
        return 1;
    }
    (void)uv_run(&poll_loop, UV_RUN_DEFAULT);
    close(poll_fds[0]);
    close(poll_fds[1]);
    if (!poll_seen) {
        puts("libuvdemo: poll failed");
        return 1;
    }
    puts("libuvdemo: poll ok");
    return 0;
}

static uv_loop_t pipe_loop;
static uv_pipe_t pipe_read_handle;
static uv_pipe_t pipe_write_handle;
static uv_write_t pipe_write_request;
static char pipe_output[64];
static int pipe_write_seen;
static int pipe_read_eof;
static int pipe_failed;

static void maybe_stop_pipe_loop(void) {
    if (pipe_write_seen && pipe_read_eof) {
        uv_stop(&pipe_loop);
    }
}

static void pipe_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
    (void)handle;
    (void)suggested_size;
    size_t used = strlen(pipe_output);
    buffer->base = pipe_output + used;
    buffer->len = sizeof(pipe_output) - used - 1;
}

static void pipe_write_cb(uv_write_t *request, int status) {
    if (request != &pipe_write_request || status < 0) {
        pipe_failed = 1;
    }
    pipe_write_seen = 1;
    uv_close((uv_handle_t *)&pipe_write_handle, 0);
    maybe_stop_pipe_loop();
}

static void pipe_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buffer) {
    (void)buffer;
    if (nread > 0) {
        size_t offset = (size_t)(buffer->base - pipe_output);
        pipe_output[offset + (size_t)nread] = '\0';
        return;
    }
    if (nread == UV_EOF) {
        pipe_read_eof = 1;
        uv_close((uv_handle_t *)stream, 0);
        maybe_stop_pipe_loop();
        return;
    }
    pipe_failed = 1;
    uv_stop(&pipe_loop);
}

static int pipe_stream_test(void) {
    uv_file fds[2] = {-1, -1};
    uv_buf_t buffer = uv_buf_init("libuv-pipe-ok", 13);
    memset(pipe_output, 0, sizeof(pipe_output));
    pipe_write_seen = 0;
    pipe_read_eof = 0;
    pipe_failed = 0;
    if (uv_loop_init(&pipe_loop) < 0 ||
        uv_pipe(fds, UV_NONBLOCK_PIPE, UV_NONBLOCK_PIPE) < 0 ||
        uv_pipe_init(&pipe_loop, &pipe_read_handle, 0) < 0 ||
        uv_pipe_init(&pipe_loop, &pipe_write_handle, 0) < 0 ||
        uv_pipe_open(&pipe_read_handle, fds[0]) < 0 ||
        uv_pipe_open(&pipe_write_handle, fds[1]) < 0 ||
        uv_read_start((uv_stream_t *)&pipe_read_handle, pipe_alloc_cb, pipe_read_cb) < 0 ||
        uv_write(&pipe_write_request, (uv_stream_t *)&pipe_write_handle, &buffer, 1, pipe_write_cb) < 0) {
        puts("libuvdemo: pipe setup failed");
        if (fds[0] >= 0) {
            close(fds[0]);
        }
        if (fds[1] >= 0) {
            close(fds[1]);
        }
        return 1;
    }
    (void)uv_run(&pipe_loop, UV_RUN_DEFAULT);
    if (pipe_failed || !pipe_write_seen || !pipe_read_eof ||
        strcmp(pipe_output, "libuv-pipe-ok") != 0) {
        puts("libuvdemo: pipe failed");
        return 1;
    }
    puts("libuvdemo: pipe stream ok");
    return 0;
}

static uv_loop_t gai_loop;
static int gai_seen;
static int gai_failed;

static void gai_cb(uv_getaddrinfo_t *request, int status, struct addrinfo *result) {
    struct sockaddr_in *addr;
    if (uv_req_get_type((uv_req_t *)request) != UV_GETADDRINFO ||
        status != 0 || result == 0 || result->ai_addr == 0) {
        gai_failed = 1;
    } else {
        addr = (struct sockaddr_in *)result->ai_addr;
        gai_seen = result->ai_family == AF_INET &&
            result->ai_socktype == SOCK_STREAM &&
            ntohs(addr->sin_port) == 80;
    }
    uv_freeaddrinfo(result);
    uv_stop(&gai_loop);
}

static int getaddrinfo_test(void) {
    uv_getaddrinfo_t request;
    struct addrinfo hints;
    gai_seen = 0;
    gai_failed = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    if (uv_loop_init(&gai_loop) < 0 ||
        uv_getaddrinfo(&gai_loop, &request, gai_cb, "10.0.2.15", "80", &hints) < 0) {
        puts("libuvdemo: getaddrinfo setup failed");
        return 1;
    }
    if (uv_req_get_data((uv_req_t *)&request) != 0) {
        puts("libuvdemo: getaddrinfo request data failed");
        return 1;
    }
    (void)uv_run(&gai_loop, UV_RUN_DEFAULT);
    if (gai_failed || !gai_seen) {
        puts("libuvdemo: getaddrinfo failed");
        return 1;
    }
    puts("libuvdemo: getaddrinfo ok");
    return 0;
}

static uv_loop_t process_loop;
static uv_pipe_t process_stdout_pipe;
static uv_process_t process_handle;
static char process_output[128];
static int process_exit_seen;
static int process_stdout_eof;
static int process_failed;

static void process_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
    (void)handle;
    (void)suggested_size;
    size_t used = strlen(process_output);
    buffer->base = process_output + used;
    buffer->len = sizeof(process_output) - used - 1;
}

static void maybe_stop_process_loop(void) {
    if (process_exit_seen && process_stdout_eof) {
        uv_stop(&process_loop);
    }
}

static void process_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buffer) {
    (void)buffer;
    if (nread > 0) {
        size_t offset = (size_t)(buffer->base - process_output);
        process_output[offset + (size_t)nread] = '\0';
        return;
    }
    if (nread == UV_EOF) {
        process_stdout_eof = 1;
        uv_close((uv_handle_t *)stream, 0);
        maybe_stop_process_loop();
        return;
    }
    process_failed = 1;
    uv_stop(&process_loop);
}

static void process_exit_cb(uv_process_t *process, int64_t exit_status, int term_signal) {
    if (process != &process_handle || exit_status != 0 || term_signal != 0) {
        process_failed = 1;
    }
    process_exit_seen = 1;
    maybe_stop_process_loop();
}

static int process_test(void) {
    char *argv[] = {"echo", "uv-process-ok", 0};
    uv_stdio_container_t stdio[3];
    uv_process_options_t options;
    memset(process_output, 0, sizeof(process_output));
    memset(stdio, 0, sizeof(stdio));
    memset(&options, 0, sizeof(options));
    process_exit_seen = 0;
    process_stdout_eof = 0;
    process_failed = 0;
    if (uv_loop_init(&process_loop) < 0 ||
        uv_pipe_init(&process_loop, &process_stdout_pipe, 0) < 0) {
        puts("libuvdemo: process setup failed");
        return 1;
    }
    stdio[0].flags = UV_IGNORE;
    stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    stdio[1].data.stream = (uv_stream_t *)&process_stdout_pipe;
    stdio[2].flags = UV_IGNORE;
    options.exit_cb = process_exit_cb;
    options.file = "echo";
    options.args = argv;
    options.stdio_count = 3;
    options.stdio = stdio;
    if (uv_spawn(&process_loop, &process_handle, &options) < 0 ||
        uv_process_get_pid(&process_handle) <= 0 ||
        uv_read_start((uv_stream_t *)&process_stdout_pipe, process_alloc_cb, process_read_cb) < 0) {
        puts("libuvdemo: process spawn failed");
        return 1;
    }
    (void)uv_run(&process_loop, UV_RUN_DEFAULT);
    if (process_failed || !process_exit_seen || !process_stdout_eof ||
        strstr(process_output, "uv-process-ok") == 0) {
        puts("libuvdemo: process failed");
        return 1;
    }
    uv_close((uv_handle_t *)&process_handle, 0);
    puts("libuvdemo: pipe ok");
    puts("libuvdemo: process ok");
    return 0;
}

int main(void) {
    puts("libuvdemo: srvros libuv port staging");
    if (version_test() != 0 ||
        error_test() != 0 ||
        core_api_test() != 0 ||
        timer_test() != 0 ||
        phase_test() != 0 ||
        fs_test() != 0 ||
        work_test() != 0 ||
        thread_test() != 0 ||
        poll_test() != 0 ||
        pipe_stream_test() != 0 ||
        getaddrinfo_test() != 0 ||
        process_test() != 0) {
        puts("libuvdemo: failed");
        return 1;
    }
    puts("libuvdemo: ok");
    return 0;
}
