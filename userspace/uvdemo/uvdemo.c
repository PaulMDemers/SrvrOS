#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

static int timer_ticks;

static void timer_cb(uv_timer_t *timer) {
    timer_ticks++;
    printf("uvdemo: timer tick %d\n", timer_ticks);
    if (timer_ticks == 3) {
        uv_timer_stop(timer);
    }
}

static int timer_test(void) {
    uv_loop_t loop;
    uv_timer_t timer;
    timer_ticks = 0;
    if (uv_loop_init(&loop) < 0 || uv_timer_init(&loop, &timer) < 0 ||
        uv_timer_start(&timer, timer_cb, 1, 1) < 0) {
        puts("uvdemo: timer setup failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    if (timer_ticks != 3) {
        puts("uvdemo: timer failed");
        return 1;
    }
    puts("uvdemo: timer ok");
    return 0;
}

static int fs_test(void) {
    uv_loop_t loop;
    uv_fs_t request;
    const char *path = "/fat/uvdemo.txt";
    char content[] = "uv-fs-ok";
    char readback[32];
    uv_buf_t buffer;
    int fd;

    if (uv_loop_init(&loop) < 0) {
        return 1;
    }
    (void)uv_fs_unlink(&loop, &request, path, 0);
    if (uv_fs_open(&loop, &request, path, O_CREAT | O_TRUNC | O_RDWR, 0644, 0) < 0) {
        puts("uvdemo: fs open failed");
        return 1;
    }
    fd = (int)request.result;
    buffer = uv_buf_init(content, (unsigned int)strlen(content));
    if (uv_fs_write(&loop, &request, fd, &buffer, 1, 0, 0) < 0 ||
        request.result != (ssize_t)strlen(content)) {
        puts("uvdemo: fs write failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    if (uv_fs_close(&loop, &request, fd, 0) < 0) {
        puts("uvdemo: fs close failed");
        return 1;
    }
    if (uv_fs_open(&loop, &request, path, O_RDONLY, 0, 0) < 0) {
        puts("uvdemo: fs reopen failed");
        return 1;
    }
    fd = (int)request.result;
    memset(readback, 0, sizeof(readback));
    buffer = uv_buf_init(readback, sizeof(readback) - 1);
    if (uv_fs_read(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("uvdemo: fs read failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    (void)uv_fs_close(&loop, &request, fd, 0);
    if (strcmp(readback, content) != 0) {
        printf("uvdemo: fs mismatch %s\n", readback);
        return 1;
    }
    if (uv_fs_stat(&loop, &request, path, 0) < 0 || request.statbuf.st_size != (off_t)strlen(content)) {
        puts("uvdemo: fs stat failed");
        return 1;
    }
    puts("uvdemo: fs ok");
    return 0;
}

static char udp_storage[128];
static int udp_got_reply;

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
    (void)handle;
    (void)suggested_size;
    buffer->base = udp_storage;
    buffer->len = sizeof(udp_storage) - 1;
}

static void udp_recv_cb(uv_udp_t *handle,
    ssize_t nread,
    const uv_buf_t *buffer,
    const struct sockaddr *addr,
    unsigned flags) {
    (void)addr;
    (void)flags;
    if (nread < 0) {
        printf("uvdemo: udp recv failed %s\n", uv_strerror((int)nread));
        uv_udp_recv_stop(handle);
        return;
    }
    buffer->base[nread] = '\0';
    if (strcmp(buffer->base, "uv-udp-ok") == 0) {
        udp_got_reply = 1;
    }
    uv_udp_recv_stop(handle);
}

static void udp_timeout_cb(uv_timer_t *timer) {
    uv_stop(timer->handle.loop);
}

static int udp_test(void) {
    uv_loop_t loop;
    uv_udp_t server;
    uv_udp_t client;
    uv_udp_send_t send_request;
    uv_timer_t timeout;
    struct sockaddr_in server_addr;
    uv_buf_t buffer;
    char message[] = "uv-udp-ok";

    udp_got_reply = 0;
    memset(udp_storage, 0, sizeof(udp_storage));
    if (uv_loop_init(&loop) < 0 ||
        uv_udp_init(&loop, &server) < 0 ||
        uv_udp_init(&loop, &client) < 0 ||
        uv_ip4_addr("0.0.0.0", 7017, &server_addr) < 0 ||
        uv_udp_bind(&server, (const struct sockaddr *)&server_addr, 0) < 0 ||
        uv_udp_recv_start(&server, alloc_cb, udp_recv_cb) < 0 ||
        uv_timer_init(&loop, &timeout) < 0 ||
        uv_timer_start(&timeout, udp_timeout_cb, 50, 0) < 0) {
        puts("uvdemo: udp setup failed");
        return 1;
    }

    if (uv_ip4_addr("10.0.2.15", 7017, &server_addr) < 0) {
        puts("uvdemo: udp addr failed");
        return 1;
    }
    buffer = uv_buf_init(message, (unsigned int)strlen(message));
    if (uv_udp_send(&send_request, &client, &buffer, 1, (const struct sockaddr *)&server_addr, 0) < 0) {
        puts("uvdemo: udp send failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t *)&server, 0);
    uv_close((uv_handle_t *)&client, 0);
    if (!udp_got_reply) {
        puts("uvdemo: udp failed");
        return 1;
    }
    puts("uvdemo: udp ok");
    return 0;
}

static void usage(void) {
    puts("usage: uvdemo [basic|udp]");
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "basic";
    if (strcmp(mode, "-h") == 0 || strcmp(mode, "--help") == 0) {
        usage();
        return 0;
    }
    if (strcmp(mode, "basic") == 0) {
        if (timer_test() != 0 || fs_test() != 0) {
            return 1;
        }
        puts("uvdemo: basic ok");
        return 0;
    }
    if (strcmp(mode, "udp") == 0) {
        return udp_test();
    }
    usage();
    return 2;
}
