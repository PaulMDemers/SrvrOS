#include <srvros/cli.h>
#include <srvros/sys.h>

static int touch_path(const char *path) {
    long fd = srv_open_mode(path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_APPEND);
    if (fd < 0) {
        cli_puts("touch: failed: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    srv_close((int)fd);
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: touch <path> [...]\n");
        return 0;
    }
    if (argc < 2) {
        cli_puts("usage: touch <path> [...]\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (touch_path(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
