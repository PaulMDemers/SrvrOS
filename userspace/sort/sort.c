#include <srvros/cli.h>
#include <srvros/sys.h>

#define MAX_LINES 512
#define MAX_LINE_LENGTH 256

static char lines[MAX_LINES][MAX_LINE_LENGTH];
static uint64_t line_count;

static int line_compare(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        }
        a++;
        b++;
    }
    if (*a == *b) {
        return 0;
    }
    return *a == '\0' ? -1 : 1;
}

static void copy_line(char *destination, const char *source) {
    uint64_t i = 0;
    while (source[i] != '\0' && i + 1 < MAX_LINE_LENGTH) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static int push_line(const char *line) {
    if (line_count >= MAX_LINES) {
        cli_puts("sort: too many lines\n");
        return 1;
    }
    copy_line(lines[line_count++], line);
    return 0;
}

static int read_fd(int fd, int close_fd) {
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
                if (push_line(line) != 0) {
                    status = 1;
                }
                length = 0;
            } else {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        if (push_line(line) != 0) {
            status = 1;
        }
    }
    if (close_fd) {
        srv_close(fd);
    }
    return status;
}

static int read_file(const char *path) {
    if (cli_streq(path, "-")) {
        return read_fd(SRV_STDIN, 0);
    }
    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("sort: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return read_fd(fd, 1);
}

static void sort_lines(void) {
    for (uint64_t i = 1; i < line_count; i++) {
        char current[MAX_LINE_LENGTH];
        copy_line(current, lines[i]);
        uint64_t j = i;
        while (j > 0 && line_compare(lines[j - 1], current) > 0) {
            copy_line(lines[j], lines[j - 1]);
            j--;
        }
        copy_line(lines[j], current);
    }
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc == 1) {
        status = read_fd(SRV_STDIN, 0);
    } else {
        for (int i = 1; i < argc; i++) {
            if (read_file(argv[i]) != 0) {
                status = 1;
            }
        }
    }
    if (status != 0) {
        return status;
    }
    sort_lines();
    for (uint64_t i = 0; i < line_count; i++) {
        cli_puts(lines[i]);
        cli_puts("\n");
    }
    return 0;
}
