#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(void) {
    puts("usage: httpget host [path]");
}

static int write_all(int fd, const char *buffer, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = send(fd, buffer + offset, length - offset, 0);
        if (written <= 0) {
            return -1;
        }
        offset += (size_t)written;
    }
    return 0;
}

static int request(int fd, const char *host, const char *path) {
    if (write_all(fd, "GET ", 4) < 0 ||
        write_all(fd, path, strlen(path)) < 0 ||
        write_all(fd, " HTTP/1.0\r\nHost: ", 17) < 0 ||
        write_all(fd, host, strlen(host)) < 0 ||
        write_all(fd, "\r\nConnection: close\r\n\r\n", 23) < 0) {
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *host;
    const char *path;
    struct addrinfo hints;
    struct addrinfo *info = 0;
    int fd = -1;
    int gai;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage();
        return 0;
    }
    if (argc < 2 || argc > 3) {
        usage();
        return 2;
    }
    host = argv[1];
    path = argc >= 3 ? argv[2] : "/";
    if (path[0] == '\0') {
        path = "/";
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    gai = getaddrinfo(host, "80", &hints, &info);
    if (gai != 0) {
        printf("httpget: resolve failed: %s\n", gai_strerror(gai));
        return 1;
    }

    fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd < 0) {
        perror("httpget: socket");
        freeaddrinfo(info);
        return 1;
    }
    if (connect(fd, info->ai_addr, info->ai_addrlen) < 0) {
        perror("httpget: connect");
        close(fd);
        freeaddrinfo(info);
        return 1;
    }
    freeaddrinfo(info);

    if (request(fd, host, path) < 0) {
        perror("httpget: write");
        close(fd);
        return 1;
    }

    for (;;) {
        char buffer[512];
        ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count < 0) {
            perror("httpget: read");
            close(fd);
            return 1;
        }
        if (count == 0) {
            break;
        }
        if (write_all(1, buffer, (size_t)count) < 0) {
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}
