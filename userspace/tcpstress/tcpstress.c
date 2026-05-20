#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(const char *step) {
    printf("tcpstress: fail %s errno=%d\n", step, errno);
    return 1;
}

static int wait_fd(int fd, short events, int timeout_ms, const char *step) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    int result = poll(&pfd, 1, timeout_ms);
    if (result != 1) {
        printf("tcpstress: fail %s poll=%d revents=0x%x errno=%d\n", step, result, pfd.revents, errno);
        return -1;
    }
    if ((pfd.revents & (events | POLLERR | POLLHUP)) == 0) {
        printf("tcpstress: fail %s revents=0x%x\n", step, pfd.revents);
        return -1;
    }
    return pfd.revents;
}

static int poll_once(int fd, short events, int timeout_ms, short *revents_out, const char *step) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    int result = poll(&pfd, 1, timeout_ms);
    if (result < 0) {
        printf("tcpstress: fail %s poll=%d errno=%d\n", step, result, errno);
        return -1;
    }
    if (result > 1) {
        printf("tcpstress: fail %s poll=%d\n", step, result);
        return -1;
    }
    *revents_out = pfd.revents;
    return result;
}

static int expect_poll(int fd, short events, int timeout_ms, short required, short forbidden, const char *step) {
    short revents = 0;
    int result = poll_once(fd, events, timeout_ms, &revents, step);
    if (result <= 0) {
        printf("tcpstress: fail %s ready=%d revents=0x%x\n", step, result, revents);
        return -1;
    }
    if ((revents & required) != required || (revents & forbidden) != 0) {
        printf("tcpstress: fail %s revents=0x%x required=0x%x forbidden=0x%x\n",
            step,
            revents,
            required,
            forbidden);
        return -1;
    }
    return 0;
}

static int expect_no_poll(int fd, short events, const char *step) {
    short revents = 0;
    int result = poll_once(fd, events, 0, &revents, step);
    if (result != 0 || revents != 0) {
        printf("tcpstress: fail %s ready=%d revents=0x%x\n", step, result, revents);
        return -1;
    }
    return 0;
}

static int write_all(int fd, const char *buffer, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = send(fd, buffer + offset, length - offset, MSG_NOSIGNAL);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (wait_fd(fd, POLLOUT, 3000, "send-poll") < 0) {
                return -1;
            }
            continue;
        }
        if (count <= 0) {
            return -1;
        }
        offset += (size_t)count;
    }
    return 0;
}

static int check_socket_metadata(int fd, int expect_listener) {
    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISSOCK(st.st_mode)) {
        return fail(expect_listener ? "listener-stat" : "conn-stat");
    }

    int type = 0;
    socklen_t type_len = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &type_len) < 0 || type != SOCK_STREAM) {
        return fail(expect_listener ? "listener-type" : "conn-type");
    }

    int accepting = -1;
    socklen_t accepting_len = sizeof(accepting);
    if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &accepting, &accepting_len) < 0 ||
        (expect_listener && accepting == 0) ||
        (!expect_listener && accepting != 0)) {
        return fail(expect_listener ? "listener-acceptconn" : "conn-acceptconn");
    }

    return 0;
}

static int read_expected(int fd, const char *expected) {
    char buffer[128];
    size_t expected_len = strlen(expected);
    size_t offset = 0;
    while (offset < expected_len) {
        ssize_t count = recv(fd, buffer + offset, sizeof(buffer) - 1 - offset, 0);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (wait_fd(fd, POLLIN, 5000, "recv-poll") < 0) {
                return -1;
            }
            continue;
        }
        if (count <= 0) {
            return -1;
        }
        offset += (size_t)count;
    }
    buffer[offset] = '\0';
    if (offset != expected_len || memcmp(buffer, expected, expected_len) != 0) {
        printf("tcpstress: fail payload got=%s expected=%s\n", buffer, expected);
        return -1;
    }
    return 0;
}

static int check_eof(int fd) {
    char byte = 0;
    for (int tries = 0; tries < 8; tries++) {
        ssize_t count = recv(fd, &byte, 1, 0);
        if (count == 0) {
            return 0;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            int revents = wait_fd(fd, POLLIN | POLLHUP | POLLERR, 5000, "eof-poll");
            if (revents < 0) {
                return -1;
            }
            continue;
        }
        if (count < 0) {
            return -1;
        }
    }
    return -1;
}

static int serve_client(int listener, int index) {
    struct pollfd listener_poll;
    listener_poll.fd = listener;
    listener_poll.events = POLLIN;
    listener_poll.revents = 0;
    if (poll(&listener_poll, 1, 8000) != 1 || (listener_poll.revents & POLLIN) == 0) {
        return fail("listen-poll");
    }

    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    int accepted = accept4(listener, (struct sockaddr *)&peer, &peer_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (accepted < 0) {
        return fail("accept4");
    }

    int result = 1;
    int duplicate = dup(accepted);
    if (duplicate < 0) {
        close(accepted);
        return fail("dup-accepted");
    }
    close(accepted);

    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (check_socket_metadata(duplicate, 0) != 0 ||
        getsockname(duplicate, (struct sockaddr *)&local, &local_len) < 0 ||
        getpeername(duplicate, (struct sockaddr *)&peer, &peer_len) < 0) {
        goto done;
    }

    char expected[32];
    snprintf(expected, sizeof(expected), "tcpstress-%d", index);
    if (read_expected(duplicate, expected) < 0) {
        (void)fail("read");
        goto done;
    }

    char response[48];
    snprintf(response, sizeof(response), "tcpstress-reply-%d", index);
    if (write_all(duplicate, response, strlen(response)) < 0) {
        (void)fail("write");
        goto done;
    }

    if (shutdown(duplicate, SHUT_WR) < 0) {
        (void)fail("shutdown");
        goto done;
    }
    if (check_eof(duplicate) < 0) {
        (void)fail("eof");
        goto done;
    }

    printf("tcpstress: client %d ok\n", index);
    result = 0;

done:
    close(duplicate);
    return result;
}

static int run_server(int port, int clients) {
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (listener < 0) {
        return fail("socket");
    }

    int on = 1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = INADDR_ANY;
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(listener);
        return fail("bind");
    }
    if (listen(listener, 8) < 0) {
        close(listener);
        return fail("listen");
    }
    if (check_socket_metadata(listener, 1) != 0) {
        close(listener);
        return 1;
    }

    int live_listener = dup(listener);
    if (live_listener < 0) {
        close(listener);
        return fail("dup-listener");
    }
    close(listener);
    if (check_socket_metadata(live_listener, 1) != 0) {
        close(live_listener);
        return 1;
    }

    printf("tcpstress: listening %d clients=%d\n", port, clients);
    for (int i = 1; i <= clients; i++) {
        if (serve_client(live_listener, i) != 0) {
            close(live_listener);
            return 1;
        }
    }
    close(live_listener);
    printf("tcpstress: ok\n");
    return 0;
}

static int run_ready_server(int port) {
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (listener < 0) {
        return fail("ready-socket");
    }

    int on = 1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = INADDR_ANY;
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(listener, 4) < 0 ||
        check_socket_metadata(listener, 1) != 0) {
        close(listener);
        return fail("ready-listen");
    }

    if (expect_no_poll(listener, POLLIN, "ready-listener-empty") < 0) {
        close(listener);
        return 1;
    }

    printf("tcpstress: ready listening %d\n", port);
    if (expect_poll(listener, POLLIN, 10000, POLLIN, POLLERR | POLLHUP, "ready-listener") < 0 ||
        expect_poll(listener, POLLIN, 0, POLLIN, POLLERR | POLLHUP, "ready-listener-repeat") < 0) {
        close(listener);
        return 1;
    }

    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    int client = accept4(listener, (struct sockaddr *)&peer, &peer_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client < 0) {
        close(listener);
        return fail("ready-accept");
    }
    if (expect_no_poll(listener, POLLIN, "ready-listener-drained") < 0 ||
        expect_poll(client, POLLOUT, 3000, POLLOUT, POLLERR, "ready-client-writable") < 0) {
        close(client);
        close(listener);
        return 1;
    }
    printf("tcpstress: accepted writable ok\n");

    if (expect_poll(client, POLLIN, 10000, POLLIN, POLLERR, "ready-client-readable") < 0 ||
        expect_poll(client, POLLIN, 0, POLLIN, POLLERR, "ready-client-readable-repeat") < 0) {
        close(client);
        close(listener);
        return 1;
    }

    char buffer[32];
    ssize_t count = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (count <= 0) {
        close(client);
        close(listener);
        return fail("ready-recv");
    }
    buffer[count] = '\0';
    if (strcmp(buffer, "ready-data") != 0) {
        printf("tcpstress: fail ready-payload got=%s\n", buffer);
        close(client);
        close(listener);
        return 1;
    }

    if (write_all(client, "ready-reply", strlen("ready-reply")) < 0) {
        close(client);
        close(listener);
        return fail("ready-write");
    }
    printf("tcpstress: payload readiness ok\n");

    close(client);
    close(listener);
    printf("tcpstress: ready cleanup ok\n");
    printf("tcpstress: readiness ok\n");
    return 0;
}

int main(int argc, char **argv) {
    int port = 7121;
    int clients = 3;
    if (argc > 1 && strcmp(argv[1], "ready") == 0) {
        if (argc > 2) {
            port = atoi(argv[2]);
        }
        if (port <= 0 || port > 65535) {
            puts("usage: tcpstress ready [port]");
            return 1;
        }
        return run_ready_server(port);
    }
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    if (argc > 2) {
        clients = atoi(argv[2]);
    }
    if (port <= 0 || port > 65535 || clients <= 0 || clients > 16) {
        puts("usage: tcpstress [port] [clients]");
        return 1;
    }
    return run_server(port, clients);
}
