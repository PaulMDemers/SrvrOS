#include <srvros/cli.h>
#include <srvros/sys.h>

int main(int argc, char **argv) {
    static char buffer[2048];
    int in_fd;
    int out_fd;

    if (argc != 3) {
        cli_puts("usage: cp <source> <dest>\n");
        return 1;
    }

    in_fd = (int)srv_open(argv[1]);
    if (in_fd < 0) {
        cli_puts("cp: cannot open source\n");
        return 2;
    }
    out_fd = (int)srv_open_mode(argv[2], SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (out_fd < 0) {
        srv_close(in_fd);
        cli_puts("cp: cannot open dest\n");
        return 4;
    }

    for (;;) {
        long count = srv_read(in_fd, buffer, sizeof(buffer));
        if (count < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("cp: read failed\n");
            return 3;
        }
        if (count == 0) {
            break;
        }
        long written = srv_write(out_fd, buffer, (size_t)count);
        if (written < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("cp: write failed\n");
            return 5;
        }
    }
    srv_close(in_fd);
    if (srv_close(out_fd) < 0) {
        cli_puts("cp: close failed\n");
        return 6;
    }

    return 0;
}
