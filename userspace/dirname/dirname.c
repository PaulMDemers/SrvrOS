#include <srvros/cli.h>

static void print_dirname(const char *path) {
    size_t length = cli_strlen(path);
    while (length > 1 && path[length - 1] == '/') {
        length--;
    }
    while (length > 0 && path[length - 1] != '/') {
        length--;
    }
    while (length > 1 && path[length - 1] == '/') {
        length--;
    }
    if (length == 0) {
        cli_puts(".\n");
        return;
    }
    for (size_t i = 0; i < length; i++) {
        srv_write(SRV_STDOUT, &path[i], 1);
    }
    cli_puts("\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        cli_puts("usage: dirname <path>\n");
        return 1;
    }
    print_dirname(argv[1]);
    return 0;
}
