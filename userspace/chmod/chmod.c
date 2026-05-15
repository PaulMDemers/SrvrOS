#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

static int parse_mode(const char *text, mode_t *mode_out) {
    mode_t mode = 0;
    if (text == 0 || text[0] == '\0' || mode_out == 0) {
        return -1;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '7') {
            return -1;
        }
        mode = (mode_t)((mode << 3) | (mode_t)(text[i] - '0'));
    }
    *mode_out = mode;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: chmod <octal-mode> <path>\n", stderr);
        return 2;
    }

    mode_t mode = 0;
    if (parse_mode(argv[1], &mode) < 0) {
        fputs("chmod: invalid mode\n", stderr);
        return 2;
    }

    if (chmod(argv[2], mode) < 0) {
        perror("chmod");
        return errno != 0 ? errno : 1;
    }
    return 0;
}
