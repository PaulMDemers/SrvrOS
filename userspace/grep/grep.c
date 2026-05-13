#include <srvros/cli.h>
#include <srvros/sys.h>

static int contains(const char *line, const char *needle) {
    if (needle[0] == '\0') {
        return 1;
    }
    for (size_t i = 0; line[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' && line[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

static int grep_file(const char *needle, const char *path) {
    int fd = (int)srv_open(path);
    char buffer[128];
    char line[256];
    size_t line_len = 0;
    int matched = 0;
    if (fd < 0) {
        cli_puts("grep: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 2;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            srv_close(fd);
            return 2;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n' || line_len + 1 >= sizeof(line)) {
                line[line_len] = '\0';
                if (contains(line, needle)) {
                    cli_puts(line);
                    cli_puts("\n");
                    matched = 1;
                }
                line_len = 0;
            } else {
                line[line_len++] = c;
            }
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        if (contains(line, needle)) {
            cli_puts(line);
            cli_puts("\n");
            matched = 1;
        }
    }
    srv_close(fd);
    return matched ? 0 : 1;
}

int main(int argc, char **argv) {
    int status = 1;
    if (argc < 3) {
        cli_puts("usage: grep <text> <file> [...]\n");
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        int result = grep_file(argv[1], argv[i]);
        if (result == 0) {
            status = 0;
        } else if (result > status) {
            status = result;
        }
    }
    return status;
}
