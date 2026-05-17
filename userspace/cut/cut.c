#include <srvros/cli.h>
#include <srvros/sys.h>

#define MAX_LINE_LENGTH 512

static int parse_uint(const char *text, uint64_t *out) {
    uint64_t value = 0;
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (uint64_t)(text[i] - '0');
    }
    if (value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

static void print_field(const char *line, char delimiter, uint64_t field) {
    uint64_t current = 1;
    uint64_t start = 0;
    uint64_t i = 0;
    for (;; i++) {
        if (line[i] == delimiter || line[i] == '\0') {
            if (current == field) {
                srv_write(SRV_STDOUT, line + start, i - start);
                cli_puts("\n");
                return;
            }
            if (line[i] == '\0') {
                break;
            }
            current++;
            start = i + 1;
        }
    }
    cli_puts("\n");
}

static int cut_fd(int fd, int close_fd, char delimiter, uint64_t field) {
    char buffer[128];
    char line[MAX_LINE_LENGTH];
    uint64_t length = 0;
    int status = 0;
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            status = 1;
            break;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n' || length + 1 >= sizeof(line)) {
                line[length] = '\0';
                print_field(line, delimiter, field);
                length = 0;
            } else {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        print_field(line, delimiter, field);
    }
    if (close_fd) {
        srv_close(fd);
    }
    return status;
}

static int cut_file(const char *path, char delimiter, uint64_t field) {
    if (cli_streq(path, "-")) {
        return cut_fd(SRV_STDIN, 0, delimiter, field);
    }
    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("cut: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return cut_fd(fd, 1, delimiter, field);
}

int main(int argc, char **argv) {
    char delimiter = '\t';
    uint64_t field = 0;
    int first_file = 1;
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: cut [-d delimiter] -f field [file ...]\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (cli_streq(argv[i], "-d") && i + 1 < argc) {
            delimiter = argv[++i][0];
            first_file = i + 1;
        } else if (cli_streq(argv[i], "-f") && i + 1 < argc && parse_uint(argv[i + 1], &field)) {
            i++;
            first_file = i + 1;
        } else {
            break;
        }
    }

    if (field == 0) {
        cli_puts("usage: cut [-d delimiter] -f field [file ...]\n");
        return 1;
    }
    if (argc <= first_file) {
        return cut_fd(SRV_STDIN, 0, delimiter, field);
    }
    for (int i = first_file; i < argc; i++) {
        if (cut_file(argv[i], delimiter, field) != 0) {
            status = 1;
        }
    }
    return status;
}
