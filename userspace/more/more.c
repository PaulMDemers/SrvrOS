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
    return value == 0 ? fallback : value;
}

static void usage(void) {
    cli_puts("usage: more [--plain] [-n lines|-lines] [file ...]\n");
}

static int prompt_more(void) {
    char key = 0;
    cli_puts("--More--");
    long count = srv_read(SRV_STDIN, &key, 1);
    cli_puts("\r        \r");
    if (count <= 0 || key == 'q' || key == 'Q') {
        return -1;
    }
    if (key == '\n' || key == '\r') {
        return 1;
    }
    return 0;
}

static int more_fd(int fd, int close_fd, uint64_t page_lines, int plain) {
    char buffer[128];
    uint64_t line_count = 0;
    int status = 0;
    int at_line_start = 1;

    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("more: read failed\n");
            status = 2;
            break;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            srv_write(SRV_STDOUT, &buffer[i], 1);
            if (buffer[i] == '\n') {
                line_count++;
                at_line_start = 1;
            } else {
                at_line_start = 0;
            }
            if (!plain && line_count >= page_lines) {
                int action = prompt_more();
                if (action < 0) {
                    if (close_fd) {
                        srv_close(fd);
                    }
                    return status;
                }
                line_count = action == 1 ? page_lines - 1 : 0;
            }
        }
    }

    if (!at_line_start) {
        cli_puts("\n");
    }
    if (close_fd) {
        srv_close(fd);
    }
    return status;
}

static int more_file(const char *path, uint64_t page_lines, int plain) {
    if (cli_streq(path, "-")) {
        return more_fd(SRV_STDIN, 0, page_lines, plain);
    }

    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("more: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return more_fd(fd, 1, page_lines, plain);
}

static int parse_line_option(const char *arg, uint64_t *lines) {
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

int main(int argc, char **argv) {
    uint64_t page_lines = 24;
    int plain = 0;
    int first_file = 1;
    int status = 0;

    for (int i = 1; i < argc; i++) {
        if (cli_streq(argv[i], "--plain")) {
            plain = 1;
            first_file = i + 1;
        } else if (cli_streq(argv[i], "-n")) {
            if (i + 1 >= argc) {
                usage();
                return 1;
            }
            page_lines = parse_number(argv[i + 1], page_lines);
            i++;
            first_file = i + 1;
        } else if (parse_line_option(argv[i], &page_lines)) {
            first_file = i + 1;
        } else {
            first_file = i;
            break;
        }
    }

    if (argc <= first_file) {
        return more_fd(SRV_STDIN, 0, page_lines, plain);
    }
    for (int i = first_file; i < argc; i++) {
        int result = more_file(argv[i], page_lines, plain);
        if (result != 0) {
            status = result;
        }
    }
    return status;
}
