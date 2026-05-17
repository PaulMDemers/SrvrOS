#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    const char *text = "usage: tap [-a] <copy-path> [input-path]\n";
    write(STDERR_FILENO, text, strlen(text));
}

static int write_all(int fd, const char *buffer, size_t length) {
    size_t done = 0;
    while (done < length) {
        ssize_t n = write(fd, buffer + done, length - done);
        if (n <= 0) {
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

int main(int argc, char **argv) {
    int append = 0;
    int arg = 1;
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-a") == 0) {
        append = 1;
        arg++;
    }
    if (argc < arg + 1 || argc > arg + 2) {
        usage();
        return 1;
    }

    int input = STDIN_FILENO;
    int should_close_input = 0;
    if (argc == arg + 2) {
        input = open(argv[arg + 1], O_RDONLY);
        if (input < 0) {
            const char *text = "tap: cannot open input\n";
            write(STDERR_FILENO, text, strlen(text));
            return 2;
        }
        should_close_input = 1;
    }

    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    int copy = open(argv[arg], flags);
    if (copy < 0) {
        const char *text = "tap: cannot open copy path\n";
        write(STDERR_FILENO, text, strlen(text));
        if (should_close_input) {
            close(input);
        }
        return 3;
    }

    char buffer[512];
    for (;;) {
        ssize_t n = read(input, buffer, sizeof(buffer));
        if (n < 0) {
            const char *text = "tap: read failed\n";
            write(STDERR_FILENO, text, strlen(text));
            close(copy);
            if (should_close_input) {
                close(input);
            }
            return 4;
        }
        if (n == 0) {
            break;
        }
        if (write_all(STDOUT_FILENO, buffer, (size_t)n) < 0 ||
            write_all(copy, buffer, (size_t)n) < 0) {
            const char *text = "tap: write failed\n";
            write(STDERR_FILENO, text, strlen(text));
            close(copy);
            if (should_close_input) {
                close(input);
            }
            return 5;
        }
    }

    close(copy);
    if (should_close_input) {
        close(input);
    }
    return 0;
}
