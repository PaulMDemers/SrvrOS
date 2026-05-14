#include <srvros/cli.h>
#include <srvros/sys.h>

static uint64_t parse_number(const char *text, uint64_t fallback) {
    uint64_t value = 0;
    if (text == 0 || text[0] == '\0') {
        return fallback;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return fallback;
        }
        value = value * 10 + (uint64_t)(text[i] - '0');
    }
    return value;
}

static int head_fd(int fd, uint64_t limit, int close_fd) {
    char buffer[128];
    uint64_t lines = 0;
    while (lines < limit) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (close_fd) {
                srv_close(fd);
            }
            return 1;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count && lines < limit; i++) {
            srv_write(SRV_STDOUT, &buffer[i], 1);
            if (buffer[i] == '\n') {
                lines++;
            }
        }
    }
    if (close_fd) {
        srv_close(fd);
    }
    return 0;
}

static int head_file(const char *path, uint64_t limit) {
    if (cli_streq(path, "-")) {
        return head_fd(SRV_STDIN, limit, 0);
    }

    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("head: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return head_fd(fd, limit, 1);
}

int main(int argc, char **argv) {
    uint64_t lines = 10;
    int first_file = 1;
    int status = 0;
    if (argc >= 3 && cli_streq(argv[1], "-n")) {
        lines = parse_number(argv[2], 10);
        first_file = 3;
    }
    if (argc <= first_file) {
        return head_fd(SRV_STDIN, lines, 0);
    }
    for (int i = first_file; i < argc; i++) {
        if (head_file(argv[i], lines) != 0) {
            status = 1;
        }
    }
    return status;
}
