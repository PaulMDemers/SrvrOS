#include <srvros/cli.h>

#include <unistd.h>

static int write_all(const char *text, size_t length) {
    while (length > 0) {
        long written = srv_write(SRV_STDOUT, text, length);
        if (written <= 0) {
            return 0;
        }
        text += written;
        length -= (size_t)written;
    }
    return 1;
}

int main(int argc, char **argv) {
    char line[160];
    size_t length = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: yes [string ...]\n");
        return 0;
    }

    if (argc <= 1) {
        cli_copy(line, sizeof(line), "y\n");
    } else {
        line[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (i > 1 && length + 1 < sizeof(line)) {
                line[length++] = ' ';
            }
            for (size_t j = 0; argv[i][j] != '\0' && length + 1 < sizeof(line); j++) {
                line[length++] = argv[i][j];
            }
        }
        if (length + 1 < sizeof(line)) {
            line[length++] = '\n';
        }
        line[length] = '\0';
    }
    length = cli_strlen(line);
    while (write_all(line, length)) {
    }
    return 0;
}
