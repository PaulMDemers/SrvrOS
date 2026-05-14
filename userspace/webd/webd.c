#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_OPEN 3
#define SYS_READ 4
#define SYS_CLOSE 5
#define SYS_NET_LISTEN 8
#define SYS_NET_ACCEPT 9
#define SYS_STAT 28
#define SYS_TICKS 42
#define SYS_POLL 50
#define SYS_FCNTL 51

#define STDOUT 1
#define REQUEST_CAPACITY 4096
#define FILE_BUFFER_CAPACITY 1024
#define MAX_PATH 160
#define MAX_CLIENTS 4
#define POLL_TIMEOUT_MS 250
#define CLIENT_IDLE_TICKS 1000

#define POLLIN 0x0001
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020
#define SRV_F_SETFL 4
#define SRV_FD_NONBLOCK 0x01
#define SRV_ERR_AGAIN -11

struct pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

struct client {
    int used;
    long fd;
    size_t request_used;
    uint64_t last_activity;
    char request[REQUEST_CAPACITY];
};

struct stat_info {
    uint64_t size;
    uint64_t type;
};

static char web_root[MAX_PATH] = "/fat/www";

static long syscall0(long number) {
    __asm__ volatile ("int $0x80" : "+a"(number) : : "memory");
    return number;
}

static long syscall1(long number, long arg0) {
    __asm__ volatile ("int $0x80" : "+a"(number) : "D"(arg0) : "memory");
    return number;
}

static long syscall2(long number, long arg0, long arg1) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1)
        : "memory");
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

static void set_nonblocking(long fd) {
    (void)syscall3(SYS_FCNTL, fd, SRV_F_SETFL, SRV_FD_NONBLOCK);
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

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
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

    if (url_length < 1 || url[0] != '/') {
        return 0;
    }

    size_t out = prefix_length;
    int last_was_slash = 1;
    int segment_has_char = 0;
    int segment_has_only_dots = 1;
    for (size_t i = 1; i < url_length; i++) {
        char c = url[i];
        if (c == '?' || c == '#') {
            break;
        }
        if (c == '%') {
            if (i + 2 >= url_length) {
                return 0;
            }
            int high = hex_value(url[i + 1]);
            int low = hex_value(url[i + 2]);
            if (high < 0 || low < 0) {
                return 0;
            }
            c = (char)((high << 4) | low);
            i += 2;
        }
        if (c == '\\' || (!safe_url_char(c) && c != '/')) {
            return 0;
        }
        if (c == '/') {
            if (last_was_slash || !segment_has_char || segment_has_only_dots) {
                return 0;
            }
            last_was_slash = 1;
            segment_has_char = 0;
            segment_has_only_dots = 1;
        } else {
            last_was_slash = 0;
            segment_has_char = 1;
            if (c != '.') {
                segment_has_only_dots = 0;
            }
        }
        if (out + 1 >= path_capacity) {
            return 0;
        }
        path[out++] = c;
    }

    if (out == prefix_length && !last_was_slash) {
        return 0;
    }
    if (segment_has_char && segment_has_only_dots) {
        return 0;
    }

    if (last_was_slash) {
        const char index[] = "index.html";
        if (out + sizeof(index) > path_capacity) {
            return 0;
        }
        for (size_t i = 0; i < sizeof(index); i++) {
            path[out + i] = index[i];
        }
        return 1;
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
    if (string_ends_with_ci(path, ".css")) {
        return "text/css; charset=utf-8";
    }
    if (string_ends_with_ci(path, ".js")) {
        return "application/javascript; charset=utf-8";
    }
    if (string_ends_with_ci(path, ".json")) {
        return "application/json; charset=utf-8";
    }
    if (string_ends_with_ci(path, ".svg")) {
        return "image/svg+xml";
    }
    if (string_ends_with_ci(path, ".png")) {
        return "image/png";
    }
    if (string_ends_with_ci(path, ".jpg") || string_ends_with_ci(path, ".jpeg")) {
        return "image/jpeg";
    }
    if (string_ends_with_ci(path, ".txt") || string_ends_with_ci(path, ".log")) {
        return "text/plain; charset=utf-8";
    }
    return "application/octet-stream";
}

static const char *cache_control_for_path(const char *path) {
    if (string_ends_with_ci(path, ".html") || string_ends_with_ci(path, ".htm")) {
        return "no-cache";
    }
    return "public, max-age=300";
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

static int write_u64(long fd, uint64_t value) {
    char digits[21];
    size_t count = 0;
    if (value == 0) {
        return write_all(fd, "0", 1) == 1;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        char c = digits[--count];
        if (write_all(fd, &c, 1) != 1) {
            return 0;
        }
    }
    return 1;
}

static int send_simple_response(long connection,
    const char *status,
    const char *content_type,
    const char *body,
    int is_head) {
    size_t body_length = strlen(body);
    if (write_cstr(connection, "HTTP/1.1 ") < 0 ||
        write_cstr(connection, status) < 0 ||
        write_cstr(connection, "\r\nServer: srvros-webd") < 0 ||
        write_cstr(connection, "\r\nContent-Type: ") < 0 ||
        write_cstr(connection, content_type) < 0 ||
        write_cstr(connection, "\r\nContent-Length: ") < 0 ||
        !write_u64(connection, body_length) ||
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
    struct stat_info info;

    if (syscall2(SYS_STAT, (long)path, (long)&info) < 0 || info.type != 0) {
        return send_simple_response(connection,
            "404 Not Found",
            "text/plain; charset=utf-8",
            "srvros webd: not found\n",
            is_head);
    }

    long fd = syscall1(SYS_OPEN, (long)path);
    if (fd < 0) {
        return send_simple_response(connection,
            "404 Not Found",
            "text/plain; charset=utf-8",
            "srvros webd: not found\n",
            is_head);
    }

    if (write_cstr(connection, "HTTP/1.1 200 OK\r\nServer: srvros-webd\r\nContent-Type: ") < 0 ||
        write_cstr(connection, mime_type_for_path(path)) < 0 ||
        write_cstr(connection, "\r\nContent-Length: ") < 0 ||
        !write_u64(connection, info.size) ||
        write_cstr(connection, "\r\nCache-Control: ") < 0 ||
        write_cstr(connection, cache_control_for_path(path)) < 0 ||
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

static int handle_request(long connection, const char *request, size_t header_end, char *path) {
    int is_head = 0;
    int status = parse_request_line(request, header_end, path, MAX_PATH, &is_head);
    if (status == 200) {
        return send_file_response(connection, path, is_head);
    }
    if (status == 405) {
        return send_simple_response(connection,
            "405 Method Not Allowed",
            "text/plain; charset=utf-8",
            "srvros webd: method not allowed\n",
            is_head);
    }
    return send_simple_response(connection,
        "400 Bad Request",
        "text/plain; charset=utf-8",
        "srvros webd: bad request\n",
        0);
}

static void close_client(struct client *client) {
    if (client == 0 || !client->used) {
        return;
    }
    syscall1(SYS_CLOSE, client->fd);
    client->used = 0;
    client->fd = -1;
    client->request_used = 0;
    client->last_activity = 0;
}

static struct client *find_free_client(struct client *clients) {
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].used) {
            return &clients[i];
        }
    }
    return 0;
}

static void accept_ready_client(long listener, struct client *clients) {
    uint64_t request_length = 0;
    long connection = syscall4(SYS_NET_ACCEPT, listener, 0, 0, (long)&request_length);
    if (connection < 0) {
        if (connection == SRV_ERR_AGAIN) {
            return;
        }
        write_text("webd: accept failed\n");
        return;
    }

    struct client *client = find_free_client(clients);
    if (client == 0) {
        send_simple_response(connection,
            "503 Service Unavailable",
            "text/plain; charset=utf-8",
            "srvros webd: busy\n",
            0);
        syscall1(SYS_CLOSE, connection);
        write_text("webd: busy\n");
        return;
    }

    (void)request_length;
    client->used = 1;
    client->fd = connection;
    client->request_used = 0;
    client->last_activity = (uint64_t)syscall0(SYS_TICKS);
    set_nonblocking(connection);
}

static void read_ready_client(struct client *client, char *path) {
    if (client == 0 || !client->used) {
        return;
    }

    if (client->request_used == sizeof(client->request)) {
        send_simple_response(client->fd,
            "413 Payload Too Large",
            "text/plain; charset=utf-8",
            "srvros webd: request too large\n",
            0);
        close_client(client);
        write_text("webd: request too large\n");
        return;
    }

    long read_count = syscall3(SYS_READ,
        client->fd,
            (long)(client->request + client->request_used),
            sizeof(client->request) - client->request_used);
    if (read_count <= 0) {
        if (read_count == SRV_ERR_AGAIN) {
            return;
        }
        close_client(client);
        write_text("webd: read closed\n");
        return;
    }

    client->request_used += (size_t)read_count;
    client->last_activity = (uint64_t)syscall0(SYS_TICKS);
    size_t header_end = find_header_end(client->request, client->request_used);
    if (header_end == 0) {
        return;
    }

    int sent = handle_request(client->fd, client->request, header_end, path);
    close_client(client);
    if (!sent) {
        write_text("webd: respond failed\n");
        return;
    }
    write_text("webd: response sent\n");
}

static void close_idle_clients(struct client *clients) {
    uint64_t now = (uint64_t)syscall0(SYS_TICKS);
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].used && now - clients[i].last_activity > CLIENT_IDLE_TICKS) {
            close_client(&clients[i]);
            write_text("webd: idle closed\n");
        }
    }
}

int main(int argc, char **argv) {
    static char path[MAX_PATH];
    static struct client clients[MAX_CLIENTS];
    struct pollfd fds[MAX_CLIENTS + 1];
    int indexes[MAX_CLIENTS + 1];

    configure_root(argc, argv);
    long listener = syscall1(SYS_NET_LISTEN, 80);
    if (listener < 0) {
        write_text("webd: listen failed\n");
        return 1;
    }
    set_nonblocking(listener);
    write_text("webd: serving ");
    write_text(web_root);
    write_text(" on 10.0.2.15:80\n");

    for (;;) {
        size_t poll_count = 1;
        fds[0].fd = (int32_t)listener;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        indexes[0] = -1;

        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].used) {
                continue;
            }
            fds[poll_count].fd = (int32_t)clients[i].fd;
            fds[poll_count].events = POLLIN;
            fds[poll_count].revents = 0;
            indexes[poll_count] = (int)i;
            poll_count++;
        }

        long ready = syscall3(SYS_POLL, (long)fds, (long)poll_count, POLL_TIMEOUT_MS);
        if (ready < 0) {
            write_text("webd: poll failed\n");
            return 2;
        }
        if (ready == 0) {
            close_idle_clients(clients);
            continue;
        }

        for (size_t i = 0; i < poll_count; i++) {
            if (fds[i].revents == 0) {
                continue;
            }
            if (indexes[i] < 0) {
                if ((fds[i].revents & POLLIN) != 0) {
                    accept_ready_client(listener, clients);
                }
                continue;
            }

            struct client *client = &clients[indexes[i]];
            if ((fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) {
                close_client(client);
                continue;
            }
            if ((fds[i].revents & POLLIN) != 0) {
                read_ready_client(client, path);
            }
        }
        close_idle_clients(clients);
    }
}
