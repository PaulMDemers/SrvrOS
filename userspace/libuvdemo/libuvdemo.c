#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <uv.h>

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
    poll_seen = 0;
    if (uv_loop_init(&poll_loop) < 0 ||
        pipe(poll_fds) < 0 ||
        uv_poll_init(&poll_loop, &poll_handle, poll_fds[0]) < 0 ||
        uv_poll_start(&poll_handle, UV_READABLE, poll_cb) < 0) {
        puts("libuvdemo: poll setup failed");
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

int main(void) {
    puts("libuvdemo: srvros libuv port staging");
    if (timer_test() != 0 ||
        fs_test() != 0 ||
        work_test() != 0 ||
        poll_test() != 0) {
        puts("libuvdemo: failed");
        return 1;
    }
    puts("libuvdemo: ok");
    return 0;
}
