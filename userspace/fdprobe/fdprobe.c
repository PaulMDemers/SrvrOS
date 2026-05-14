#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char byte;
    ssize_t n = read(3, &byte, 1);
    if (argc > 1 && strcmp(argv[1], "closed") == 0) {
        return n < 0 ? 0 : 2;
    }
    if (argc > 1 && strcmp(argv[1], "open") == 0) {
        return n == 1 ? 0 : 3;
    }
    return 4;
}
