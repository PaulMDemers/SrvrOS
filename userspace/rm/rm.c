#include <srvros/cli.h>
#include <srvros/sys.h>

static int remove_path(const char *path) {
    if (srv_unlink(path) == 0) {
        return 0;
    }

    cli_puts("rm: cannot remove ");
    cli_puts(path);
    cli_puts("\n");
    return 1;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc < 2) {
        cli_puts("usage: rm <path> [...]\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (remove_path(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
