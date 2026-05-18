#include <string.h>
#include <unistd.h>

static int parse_fd(const char *text) {
    int value = 0;
    if (text == 0 || text[0] == '\0') {
        return 3;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

int main(int argc, char **argv) {
    char byte;
    int fd = argc > 2 ? parse_fd(argv[2]) : 3;
    if (fd < 0) {
        return 5;
    }
    ssize_t n = read(fd, &byte, 1);
    if (argc > 1 && strcmp(argv[1], "closed") == 0) {
        return n < 0 ? 0 : 2;
    }
    if (argc > 1 && strcmp(argv[1], "open") == 0) {
        return n == 1 ? 0 : 3;
    }
    return 4;
}
