#include <srvros/cli.h>

static void print_basename(const char *path) {
    size_t length = cli_strlen(path);
    while (length > 1 && path[length - 1] == '/') {
        length--;
    }
    size_t start = length;
    while (start > 0 && path[start - 1] != '/') {
        start--;
    }
    if (length == 0) {
        cli_puts("\n");
        return;
    }
    for (size_t i = start; i < length; i++) {
        srv_write(SRV_STDOUT, &path[i], 1);
    }
    cli_puts("\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        cli_puts("usage: basename <path>\n");
        return 1;
    }
    print_basename(argv[1]);
    return 0;
}
