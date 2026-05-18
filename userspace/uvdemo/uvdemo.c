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

static char tcp_storage[256];
static uv_loop_t tcp_loop;
static uv_tcp_t tcp_server;
static uv_tcp_t tcp_client;
static uv_write_t tcp_write_request;
static int tcp_ok;

static void udp_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
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
        uv_udp_recv_start(&server, udp_alloc_cb, udp_recv_cb) < 0 ||
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

static void tcp_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
    (void)handle;
    (void)suggested_size;
    buffer->base = tcp_storage;
    buffer->len = sizeof(tcp_storage) - 1;
}

static void tcp_write_cb(uv_write_t *request, int status) {
    (void)request;
    if (status < 0) {
        printf("uvdemo: tcp write failed %s\n", uv_strerror(status));
    } else {
        tcp_ok = 1;
        puts("uvdemo: tcp ok");
    }
    uv_close((uv_handle_t *)&tcp_client, 0);
    uv_stop(&tcp_loop);
}

static void tcp_read_cb(uv_tcp_t *stream, ssize_t nread, const uv_buf_t *buffer) {
    (void)stream;
    if (nread < 0) {
        printf("uvdemo: tcp read failed %s\n", uv_strerror((int)nread));
        uv_stop(&tcp_loop);
        return;
    }
    buffer->base[nread] = '\0';
    printf("uvdemo: tcp read %ld\n", (long)nread);
    char response[320];
    snprintf(response, sizeof(response), "uv-tcp-ok:%s", buffer->base);
    uv_buf_t out = uv_buf_init(response, (unsigned int)strlen(response));
    if (uv_write(&tcp_write_request, &tcp_client, &out, 1, tcp_write_cb) < 0) {
        puts("uvdemo: tcp write start failed");
        uv_stop(&tcp_loop);
    }
}

static void tcp_connection_cb(uv_tcp_t *server, int status) {
    if (status < 0) {
        printf("uvdemo: tcp accept event failed %s\n", uv_strerror(status));
        uv_stop(&tcp_loop);
        return;
    }
    if (uv_tcp_init(&tcp_loop, &tcp_client) < 0 ||
        uv_accept(server, &tcp_client) < 0 ||
        uv_read_start(&tcp_client, tcp_alloc_cb, tcp_read_cb) < 0) {
        puts("uvdemo: tcp accept failed");
        uv_stop(&tcp_loop);
        return;
    }
    uv_close((uv_handle_t *)server, 0);
    puts("uvdemo: tcp accepted");
}

static void tcp_timeout_cb(uv_timer_t *timer) {
    (void)timer;
    puts("uvdemo: tcp timeout");
    uv_stop(&tcp_loop);
}

static int tcp_test(void) {
    uv_timer_t timeout;
    struct sockaddr_in addr;
    tcp_ok = 0;
    memset(tcp_storage, 0, sizeof(tcp_storage));
    if (uv_loop_init(&tcp_loop) < 0 ||
        uv_tcp_init(&tcp_loop, &tcp_server) < 0 ||
        uv_ip4_addr("0.0.0.0", 7018, &addr) < 0 ||
        uv_tcp_bind(&tcp_server, (const struct sockaddr *)&addr, 0) < 0 ||
        uv_listen(&tcp_server, 4, tcp_connection_cb) < 0 ||
        uv_timer_init(&tcp_loop, &timeout) < 0 ||
        uv_timer_start(&timeout, tcp_timeout_cb, 5000, 0) < 0) {
        puts("uvdemo: tcp setup failed");
        return 1;
    }
    puts("uvdemo: tcp listening 7018");
    (void)uv_run(&tcp_loop, UV_RUN_DEFAULT);
    uv_timer_stop(&timeout);
    uv_close((uv_handle_t *)&tcp_server, 0);
    if (!tcp_ok) {
        puts("uvdemo: tcp failed");
        return 1;
    }
    return 0;
}

static void usage(void) {
    puts("usage: uvdemo [basic|udp|tcp]");
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
    if (strcmp(mode, "tcp") == 0) {
        return tcp_test();
    }
    usage();
    return 2;
}
