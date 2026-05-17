#include <srvros/cli.h>

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

static void usage(void) {
    cli_puts("usage: cmp [-s] file1 file2\n");
}

int main(int argc, char **argv) {
    int silent = 0;
    int first = 1;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        usage();
        return 0;
    }
    if (argc > 1 && cli_is_option_terminator(argv[1])) {
        first = 2;
    } else if (argc > 1 && cli_streq(argv[1], "-s")) {
        silent = 1;
        first = 2;
    }
    if (argc != first + 2) {
        usage();
        return 2;
    }

    int left = open(argv[first], O_RDONLY);
    if (left < 0) {
        if (!silent) {
            cli_puts("cmp: cannot open: ");
            cli_puts(argv[first]);
            cli_puts("\n");
        }
        return 2;
    }
    int right = open(argv[first + 1], O_RDONLY);
    if (right < 0) {
        close(left);
        if (!silent) {
            cli_puts("cmp: cannot open: ");
            cli_puts(argv[first + 1]);
            cli_puts("\n");
        }
        return 2;
    }

    uint8_t left_buffer[256];
    uint8_t right_buffer[256];
    for (;;) {
        ssize_t left_read = read(left, left_buffer, sizeof(left_buffer));
        ssize_t right_read = read(right, right_buffer, sizeof(right_buffer));
        if (left_read < 0 || right_read < 0) {
            close(left);
            close(right);
            if (!silent) {
                cli_puts("cmp: read failed\n");
            }
            return 2;
        }
        if (left_read != right_read) {
            break;
        }
        if (left_read == 0) {
            close(left);
            close(right);
            return 0;
        }
        for (ssize_t i = 0; i < left_read; i++) {
            if (left_buffer[i] != right_buffer[i]) {
                close(left);
                close(right);
                if (!silent) {
                    cli_puts(argv[first]);
                    cli_puts(" ");
                    cli_puts(argv[first + 1]);
                    cli_puts(" differ\n");
                }
                return 1;
            }
        }
    }

    close(left);
    close(right);
    if (!silent) {
        cli_puts(argv[first]);
        cli_puts(" ");
        cli_puts(argv[first + 1]);
        cli_puts(" differ\n");
    }
    return 1;
}
