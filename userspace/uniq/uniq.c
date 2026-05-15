#include <srvros/cli.h>
#include <srvros/sys.h>

#define MAX_LINE_LENGTH 256

static int read_fd(int fd, int close_fd) {
    char buffer[128];
    char line[MAX_LINE_LENGTH];
    char previous[MAX_LINE_LENGTH];
    uint64_t length = 0;
    int have_previous = 0;
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
                if (!have_previous || !cli_streq(line, previous)) {
                    cli_puts(line);
                    cli_puts("\n");
                    cli_copy(previous, sizeof(previous), line);
                    have_previous = 1;
                }
                length = 0;
            } else {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        if (!have_previous || !cli_streq(line, previous)) {
            cli_puts(line);
            cli_puts("\n");
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
        cli_puts("uniq: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return read_fd(fd, 1);
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc == 1) {
        return read_fd(SRV_STDIN, 0);
    }
    for (int i = 1; i < argc; i++) {
        if (read_file(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
