#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
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

static int fs_test(void) {
    uv_loop_t loop;
    uv_fs_t request;
    const char *path = "/fat/libuvdemo.txt";
    const char *renamed = "/fat/libuvdemo-renamed.txt";
    const char *dir = "/fat/libuvdemo-dir";
    char payload[] = "libuv-fs-ok";
    char readback[32];
    uv_buf_t buffer;
    int fd;

    if (uv_loop_init(&loop) < 0) {
        return 1;
    }
    (void)uv_fs_unlink(&loop, &request, path, 0);
    (void)uv_fs_unlink(&loop, &request, renamed, 0);
    (void)uv_fs_rmdir(&loop, &request, dir, 0);

    if (uv_fs_open(&loop, &request, path, O_CREAT | O_TRUNC | O_RDWR, 0644, 0) < 0) {
        puts("libuvdemo: fs open failed");
        return 1;
    }
    fd = (int)request.result;
    buffer = uv_buf_init(payload, (unsigned int)strlen(payload));
    if (uv_fs_write(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("libuvdemo: fs write failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    (void)uv_fs_close(&loop, &request, fd, 0);

    if (uv_fs_open(&loop, &request, path, O_RDONLY, 0, 0) < 0) {
        puts("libuvdemo: fs reopen failed");
        return 1;
    }
    fd = (int)request.result;
    memset(readback, 0, sizeof(readback));
    buffer = uv_buf_init(readback, sizeof(readback) - 1);
    if (uv_fs_read(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("libuvdemo: fs read failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    (void)uv_fs_close(&loop, &request, fd, 0);
    if (strcmp(readback, payload) != 0) {
        puts("libuvdemo: fs mismatch");
        return 1;
    }

    if (uv_fs_mkdir(&loop, &request, dir, 0755, 0) < 0 ||
        uv_fs_rename(&loop, &request, path, renamed, 0) < 0 ||
        uv_fs_stat(&loop, &request, renamed, 0) < 0 ||
        request.statbuf.st_size != (off_t)strlen(payload) ||
        uv_fs_unlink(&loop, &request, renamed, 0) < 0 ||
        uv_fs_rmdir(&loop, &request, dir, 0) < 0) {
        puts("libuvdemo: fs metadata failed");
        return 1;
    }
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

int main(void) {
    puts("libuvdemo: srvros libuv port staging");
    if (version_test() != 0 ||
        error_test() != 0 ||
        core_api_test() != 0 ||
        timer_test() != 0 ||
        phase_test() != 0 ||
        fs_test() != 0 ||
        work_test() != 0 ||
        poll_test() != 0 ||
        getaddrinfo_test() != 0) {
        puts("libuvdemo: failed");
        return 1;
    }
    puts("libuvdemo: ok");
    return 0;
}
