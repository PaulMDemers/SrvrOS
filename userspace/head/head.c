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

static int head_file(const char *path, uint64_t limit) {
    int fd = (int)srv_open(path);
    char buffer[128];
    uint64_t lines = 0;
    if (fd < 0) {
        cli_puts("head: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    while (lines < limit) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            srv_close(fd);
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
    srv_close(fd);
    return 0;
}

int main(int argc, char **argv) {
    uint64_t lines = 10;
    int first_file = 1;
    int status = 0;
    if (argc < 2) {
        cli_puts("usage: head [-n count] <file> [...]\n");
        return 1;
    }
    if (argc >= 4 && cli_streq(argv[1], "-n")) {
        lines = parse_number(argv[2], 10);
        first_file = 3;
    }
    for (int i = first_file; i < argc; i++) {
        if (head_file(argv[i], lines) != 0) {
            status = 1;
        }
    }
    return status;
}
