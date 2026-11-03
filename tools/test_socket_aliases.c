/* Reuse the host syscall stubs while keeping each test in a separate process. */
#define main udp_owner_fixture_main
#include "test_posix_udp_owner.c"
#undef main

static int close_during_connect = -1;
static bool connect_succeeds;

int main(void) {
    int first = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    assert(first == POSIX_SOCKET_BASE);
    assert(descriptor_value == SRV_FD_CLOEXEC);
    int alias = __posix_socket_dup(first);
    assert(descriptor_value == 0);
    assert(alias == first + 1);
    assert(__posix_socket_fcntl(first, SRV_F_GETFD, 0) == SRV_FD_CLOEXEC);
    fail_descriptor_write = true;
    assert(__posix_socket_fcntl(alias, SRV_F_SETFD, SRV_FD_CLOEXEC) == -1);
    assert(__posix_socket_fcntl(alias, SRV_F_GETFD, 0) == 0);
    fail_descriptor_write = false;
    fail_status_write = true;
    assert(__posix_socket_fcntl(alias, SRV_F_SETFL, SRV_FD_NONBLOCK) == -1);
    assert(socket_at(first)->fd_flags == 0 && flags_value == 0);
    fail_status_write = false;
    assert(__posix_socket_fcntl(alias, SRV_F_GETFD, 0) == 0);
    assert(__posix_socket_fcntl(alias, SRV_F_SETFD, SRV_FD_CLOEXEC) == 0);
    assert(__posix_socket_fcntl(first, SRV_F_SETFD, 0) == 0);
    assert(__posix_socket_fcntl(alias, SRV_F_GETFD, 0) == SRV_FD_CLOEXEC);

    struct sockaddr_in peer = {.sin_family=AF_INET, .sin_port=htons(8000),
        .sin_addr.s_addr=htonl(0x0a000202)};
    assert(connect(alias, (struct sockaddr *)&peer, sizeof(peer)) == 0);
    struct sockaddr_in actual; socklen_t len = sizeof(actual);
    assert(getpeername(first, (struct sockaddr *)&actual, &len) == 0);
    assert(actual.sin_addr.s_addr == peer.sin_addr.s_addr && actual.sin_port == peer.sin_port);
    int value = 1; len = sizeof(value);
    assert(setsockopt(alias, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) == 0);
    value = 0;
    assert(getsockopt(first, SOL_SOCKET, SO_KEEPALIVE, &value, &len) == 0 && value == 1);
    fail_descriptor_write = true;
    assert(__posix_socket_close(first) == -1);
    assert(socket_at(first) == socket_at(alias));
    assert(__posix_socket_fcntl(first, SRV_F_GETFD, 0) == 0);
    fail_descriptor_write = false;
    assert(__posix_socket_close(first) == 0);
    assert(descriptor_value == SRV_FD_CLOEXEC);
    assert(socket(AF_INET, SOCK_DGRAM, 0) == first);
    len = sizeof(actual);
    assert(getpeername(alias, (struct sockaddr *)&actual, &len) == 0 && actual.sin_port == peer.sin_port);
    assert(__posix_socket_close(alias) == 0);
    assert(__posix_socket_close(first) == 0);

    int stream = socket(AF_INET, SOCK_STREAM, 0);
    int stream_alias = __posix_socket_dup(stream);
    connect_succeeds = true;
    assert(connect(stream_alias, (struct sockaddr *)&peer, sizeof(peer)) == 0);
    assert(socket_at(stream)->connected && socket_at(stream)->listener_fd == 7);
    assert(__posix_socket_close(stream_alias) == 0);
    assert(socket_at(stream)->connected);
    assert(__posix_socket_close(stream) == 0);

    stream = socket(AF_INET, SOCK_STREAM, 0);
    stream_alias = __posix_socket_dup(stream);
    close_during_connect = stream;
    assert(connect(stream, (struct sockaddr *)&peer, sizeof(peer)) == -1 && errno == EBADF);
    assert(socket_at(stream_alias) && !socket_at(stream_alias)->connecting);
    assert(__posix_socket_close(stream_alias) == 0);

    __posix_socket_note_dup(20, 21);
    value = 1; len = sizeof(value);
    assert(setsockopt(20, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) == 0);
    value = 0;
    assert(getsockopt(21, SOL_SOCKET, SO_KEEPALIVE, &value, &len) == 0 && value == 1);
    __posix_socket_note_close(20);
    value = 0; len = sizeof(value);
    assert(getsockopt(21, SOL_SOCKET, SO_KEEPALIVE, &value, &len) == 0 && value == 1);
    __posix_socket_note_close(21);
    for (int fd = 0; fd < 40; fd++) {
        value = 1;
        assert(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) == 0);
    }
    __posix_socket_note_dup(39, SRV_OPEN_MAX - 1);
    value = 0; len = sizeof(value);
    assert(getsockopt(SRV_OPEN_MAX - 1, SOL_SOCKET, SO_KEEPALIVE, &value, &len) == 0 && value == 1);
    value = 0;
    assert(setsockopt(SRV_OPEN_MAX - 1, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) == 0);
    value = 1; len = sizeof(value);
    assert(getsockopt(39, SOL_SOCKET, SO_KEEPALIVE, &value, &len) == 0 && value == 0);
    return 0;
}
long srv_net_connect(uint32_t ip, uint16_t port, uint64_t flags) {
    (void)ip;
    (void)port;
    (void)flags;
    if (connect_succeeds) {
        connect_succeeds = false;
        return 7;
    }
    assert(close_during_connect >= 0);
    assert(__posix_socket_close(close_during_connect) == 0);
    close_during_connect = -1;
    return SRV_ERR_INTR;
}

long srv_unix_connect(const char *path) {
    (void)path;
    assert(0);
    return -1;
}

long srv_net_peername(int fd, uint32_t *ip, uint16_t *port) {
    (void)fd;
    (void)ip;
    (void)port;
    assert(0);
    return -1;
}
