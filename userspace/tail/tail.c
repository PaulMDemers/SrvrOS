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

static int parse_line_option(const char *arg, uint64_t *lines) {
    if (arg == 0 || arg[0] == '\0') {
        return 0;
    }
    if (arg[0] == '-' && arg[1] == 'n' && arg[2] != '\0') {
        *lines = parse_number(arg + 2, *lines);
        return 1;
    }
    if (arg[0] == '-' && arg[1] >= '0' && arg[1] <= '9') {
        *lines = parse_number(arg + 1, *lines);
        return 1;
    }
    return 0;
}

static int tail_fd(int fd, uint64_t limit, int close_fd) {
    char data[4096];
    size_t length = 0;
    size_t start = 0;
    char buffer[128];
    uint64_t lines = 0;

    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("tail: read failed\n");
            if (close_fd) {
                srv_close(fd);
            }
            return 1;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            if (length < sizeof(data)) {
                data[length++] = buffer[i];
            } else {
                for (size_t j = 1; j < sizeof(data); j++) {
                    data[j - 1] = data[j];
                }
                data[sizeof(data) - 1] = buffer[i];
            }
        }
    }

    for (size_t i = length; i > 0; i--) {
        if (data[i - 1] == '\n' && i < length) {
            lines++;
            if (lines >= limit) {
                start = i;
                break;
            }
        }
    }
    srv_write(SRV_STDOUT, data + start, length - start);
    if (close_fd) {
        srv_close(fd);
    }
    return 0;
}

static int tail_file(const char *path, uint64_t limit) {
    if (cli_streq(path, "-")) {
        return tail_fd(SRV_STDIN, limit, 0);
    }

    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("tail: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return tail_fd(fd, limit, 1);
}

int main(int argc, char **argv) {
    uint64_t lines = 10;
    int first_file = 1;
    int status = 0;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: tail [-n count|-count] [file ...]\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (cli_is_option_terminator(argv[i])) {
            first_file = i + 1;
            break;
        } else if (cli_streq(argv[i], "-n")) {
            if (i + 1 >= argc) {
                cli_puts("usage: tail [-n count|-count] [file ...]\n");
                return 1;
            }
            lines = parse_number(argv[i + 1], lines);
            i++;
            first_file = i + 1;
        } else if (cli_streq(argv[i], "--lines")) {
            if (i + 1 >= argc) {
                cli_puts("usage: tail [-n count|-count] [file ...]\n");
                return 1;
            }
            lines = parse_number(argv[i + 1], lines);
            i++;
            first_file = i + 1;
        } else if (cli_starts_with(argv[i], "--lines=")) {
            lines = parse_number(argv[i] + 8, lines);
            first_file = i + 1;
        } else if (parse_line_option(argv[i], &lines)) {
            first_file = i + 1;
        } else {
            first_file = i;
            break;
        }
    }
    if (argc <= first_file) {
        return tail_fd(SRV_STDIN, lines, 0);
    }
    for (int i = first_file; i < argc; i++) {
        if (tail_file(argv[i], lines) != 0) {
            status = 1;
        }
    }
    return status;
}
