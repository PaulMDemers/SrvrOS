#include <stdint.h>
#include "../ports/srvros/uv.h"

static int callback_called;
int __posix_socket_real_fd(int fd) { return fd; }
static void connect_cb(uv_connect_t *request, int status) {
    (void)request; (void)status; callback_called++;
}

int main(void) {
    uv_tcp_t tcp = {0}; uv_write_t request = {0};
    char byte = 0;
    uv_buf_t buffers[2] = {{&byte, SIZE_MAX}, {&byte, 1}};
    tcp.handle.type = UV_TCP; tcp.handle.fd = 1;
    if (uv_write(&request, (uv_stream_t *)&tcp, buffers, 2, NULL) != UV_EINVAL) return 1;
    buffers[0].base = NULL; buffers[0].len = 1;
    if (uv_write(&request, (uv_stream_t *)&tcp, buffers, 1, NULL) != UV_EINVAL) return 2;
    tcp.write_queue_size = SIZE_MAX; buffers[0].base = &byte;
    if (uv_write(&request, (uv_stream_t *)&tcp, buffers, 1, NULL) != UV_EINVAL) return 3;
    uv_pipe_connect(NULL, NULL, "name", connect_cb);
    return callback_called ? 4 : 0;
}
