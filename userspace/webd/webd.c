#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_OPEN 3
#define SYS_READ 4
#define SYS_CLOSE 5
#define SYS_NET_LISTEN 8
#define SYS_NET_ACCEPT 9

#define STDOUT 1
#define REQUEST_CAPACITY 4096
#define FILE_BUFFER_CAPACITY 1024
#define MAX_PATH 160

static char web_root[MAX_PATH] = "/fat/www";

static long syscall1(long number, long arg0) {
    __asm__ volatile ("int $0x80" : "+a"(number) : "D"(arg0) : "memory");
    return number;
}

static long syscall3(long number, long arg0, long arg1, long arg2) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2)
        : "memory");
    return number;
}

static long syscall4(long number, long arg0, long arg1, long arg2, long arg3) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2), "c"(arg3)
        : "memory");
    return number;
}

static size_t strlen(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void write_text(const char *text) {
    syscall3(SYS_WRITE, STDOUT, (long)text, (long)strlen(text));
}

static int ascii_lower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}

static int string_equals_ci_n(const char *left, const char *right, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (right[i] == '\0' || ascii_lower(left[i]) != ascii_lower(right[i])) {
            return 0;
        }
    }
    return right[length] == '\0';
}

static int string_ends_with_ci(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    if (suffix_length > text_length) {
        return 0;
    }
    return string_equals_ci_n(text + text_length - suffix_length, suffix, suffix_length);
}

static size_t find_header_end(const char *buffer, size_t length) {
    for (size_t i = 0; i + 3 < length; i++) {
        if (buffer[i] == '\r' &&
            buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' &&
            buffer[i + 3] == '\n') {
            return i + 4;
        }
    }
    return 0;
}

static int safe_url_char(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '.' ||
        c == '-' ||
        c == '_';
}

static int copy_static_path(const char *url, size_t url_length, char *path, size_t path_capacity) {
    size_t prefix_length = strlen(web_root);

    if (path_capacity <= prefix_length + 1) {
        return 0;
    }
    for (size_t i = 0; i < prefix_length; i++) {
        path[i] = web_root[i];
    }
    if (prefix_length > 1 && path[prefix_length - 1] == '/') {
        prefix_length--;
    } else {
        path[prefix_length++] = '/';
    }

    if (url_length == 1 && url[0] == '/') {
        const char index[] = "index.html";
        if (prefix_length + sizeof(index) > path_capacity) {
            return 0;
        }
        for (size_t i = 0; i < sizeof(index); i++) {
            path[prefix_length + i] = index[i];
        }
        return 1;
    }

    if (url_length < 2 || url[0] != '/') {
        return 0;
    }

    size_t out = prefix_length;
    for (size_t i = 1; i < url_length; i++) {
        char c = url[i];
        if (c == '?' || c == '#') {
            break;
        }
        if (c == '/' || c == '\\' || !safe_url_char(c)) {
            return 0;
        }
        if (c == '.' && i + 1 < url_length && url[i + 1] == '.') {
            return 0;
        }
        if (out + 1 >= path_capacity) {
            return 0;
        }
        path[out++] = c;
    }

    if (out == prefix_length) {
        return 0;
    }
    path[out] = '\0';
    return 1;
}

static void configure_root(int argc, char **argv) {
    if (argc < 2 || argv[1] == 0 || argv[1][0] == '\0') {
        return;
    }
    size_t length = strlen(argv[1]);
    if (length >= sizeof(web_root)) {
        return;
    }
    for (size_t i = 0; i <= length; i++) {
        web_root[i] = argv[1][i];
    }
    while (length > 1 && web_root[length - 1] == '/') {
        web_root[--length] = '\0';
    }
}

static int parse_request_line(const char *request,
    size_t request_length,
    char *path,
    size_t path_capacity,
    int *is_head) {
    size_t line_end = 0;
    while (line_end < request_length && request[line_end] != '\n') {
        line_end++;
    }
    if (line_end == request_length) {
        return 400;
    }
    if (line_end > 0 && request[line_end - 1] == '\r') {
        line_end--;
    }

    size_t method_end = 0;
    while (method_end < line_end && request[method_end] != ' ') {
        method_end++;
    }
    if (method_end == line_end) {
        return 400;
    }

    *is_head = 0;
    if (string_equals_ci_n(request, "GET", method_end)) {
        *is_head = 0;
    } else if (string_equals_ci_n(request, "HEAD", method_end)) {
        *is_head = 1;
    } else {
        return 405;
    }

    size_t url_start = method_end + 1;
    while (url_start < line_end && request[url_start] == ' ') {
        url_start++;
    }
    size_t url_end = url_start;
    while (url_end < line_end && request[url_end] != ' ') {
        url_end++;
    }
    if (url_end == url_start) {
        return 400;
    }

    if (!copy_static_path(request + url_start, url_end - url_start, path, path_capacity)) {
        return 400;
    }
    return 200;
}

static const char *mime_type_for_path(const char *path) {
    if (string_ends_with_ci(path, ".html") || string_ends_with_ci(path, ".htm")) {
        return "text/html; charset=utf-8";
    }
    if (string_ends_with_ci(path, ".txt") || string_ends_with_ci(path, ".log")) {
        return "text/plain; charset=utf-8";
    }
    return "application/octet-stream";
}

static long write_all(long fd, const char *buffer, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        size_t chunk = length - sent;
        if (chunk > 1024) {
            chunk = 1024;
        }
        long result = syscall3(SYS_WRITE, fd, (long)(buffer + sent), (long)chunk);
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }
    return (long)sent;
}

static long write_cstr(long fd, const char *text) {
    return write_all(fd, text, strlen(text));
}

static int send_simple_response(long connection,
    const char *status,
    const char *content_type,
    const char *body,
    int is_head) {
    if (write_cstr(connection, "HTTP/1.1 ") < 0 ||
        write_cstr(connection, status) < 0 ||
        write_cstr(connection, "\r\nContent-Type: ") < 0 ||
        write_cstr(connection, content_type) < 0 ||
        write_cstr(connection, "\r\nConnection: close\r\n\r\n") < 0) {
        return 0;
    }
    if (!is_head && write_cstr(connection, body) < 0) {
        return 0;
    }
    return 1;
}

static int send_file_response(long connection, const char *path, int is_head) {
    static char file_buffer[FILE_BUFFER_CAPACITY];

    long fd = syscall1(SYS_OPEN, (long)path);
    if (fd < 0) {
        return send_simple_response(connection,
            "404 Not Found",
            "text/plain; charset=utf-8",
            "srvros webd: not found\n",
            is_head);
    }

    if (write_cstr(connection, "HTTP/1.1 200 OK\r\nContent-Type: ") < 0 ||
        write_cstr(connection, mime_type_for_path(path)) < 0 ||
        write_cstr(connection, "\r\nConnection: close\r\n\r\n") < 0) {
        syscall1(SYS_CLOSE, fd);
        return 0;
    }

    if (!is_head) {
        for (;;) {
            long count = syscall3(SYS_READ, fd, (long)file_buffer, sizeof(file_buffer));
            if (count < 0) {
                syscall1(SYS_CLOSE, fd);
                return 0;
            }
            if (count == 0) {
                break;
            }
            if (write_all(connection, file_buffer, (size_t)count) < 0) {
                syscall1(SYS_CLOSE, fd);
                return 0;
            }
        }
    }

    syscall1(SYS_CLOSE, fd);
    return 1;
}

int main(int argc, char **argv) {
    static char request[REQUEST_CAPACITY];
    static char path[MAX_PATH];

    configure_root(argc, argv);
    long listener = syscall1(SYS_NET_LISTEN, 80);
    if (listener < 0) {
        write_text("webd: listen failed\n");
        return 1;
    }
    write_text("webd: serving ");
    write_text(web_root);
    write_text(" on 10.0.2.15:80\n");

    for (;;) {
        uint64_t request_length = 0;
        long connection = syscall4(SYS_NET_ACCEPT, listener, 0, 0, (long)&request_length);
        if (connection < 0) {
            write_text("webd: accept failed\n");
            return 2;
        }

        (void)request_length;
        size_t request_used = 0;
        size_t header_end = 0;
        for (;;) {
            if (request_used == sizeof(request)) {
                send_simple_response(connection,
                    "413 Payload Too Large",
                    "text/plain; charset=utf-8",
                    "srvros webd: request too large\n",
                    0);
                syscall1(SYS_CLOSE, connection);
                write_text("webd: request too large\n");
                break;
            }

            long read_count = syscall3(SYS_READ,
                connection,
                (long)(request + request_used),
                sizeof(request) - request_used);
            if (read_count <= 0) {
                syscall1(SYS_CLOSE, connection);
                write_text("webd: read failed\n");
                return 3;
            }

            request_used += (size_t)read_count;
            header_end = find_header_end(request, request_used);
            if (header_end != 0) {
                int is_head = 0;
                int status = parse_request_line(request, header_end, path, sizeof(path), &is_head);
                int sent = 0;
                if (status == 200) {
                    sent = send_file_response(connection, path, is_head);
                } else if (status == 405) {
                    sent = send_simple_response(connection,
                        "405 Method Not Allowed",
                        "text/plain; charset=utf-8",
                        "srvros webd: method not allowed\n",
                        is_head);
                } else {
                    sent = send_simple_response(connection,
                        "400 Bad Request",
                        "text/plain; charset=utf-8",
                        "srvros webd: bad request\n",
                        0);
                }

                syscall1(SYS_CLOSE, connection);
                if (!sent) {
                    write_text("webd: respond failed\n");
                    return 4;
                }
                write_text("webd: response sent\n");
                break;
            }
        }
    }
}
