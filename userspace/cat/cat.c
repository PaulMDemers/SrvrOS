#include <srvros/cli.h>
#include <srvros/sys.h>

static int cat_file(const char *path) {
    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("cat: open failed: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }

    char buffer[128];
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("cat: read failed\n");
            srv_close(fd);
            return 2;
        }
        if (count == 0) {
            break;
        }
        srv_write(SRV_STDOUT, buffer, (size_t)count);
    }

    srv_close(fd);
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc < 2) {
        return cat_file("/fat/notes");
    }
    for (int i = 1; i < argc; i++) {
        int result = cat_file(argv[i]);
        if (result != 0) {
            status = result;
        }
    }
    return status;
}
