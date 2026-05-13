#include <srvros/cli.h>
#include <srvros/sys.h>

int main(int argc, char **argv) {
    static char buffer[2048];
    int in_fd;
    int out_fd;

    if (argc != 3) {
        cli_puts("usage: mv <source> <dest>\n");
        return 1;
    }

    if (srv_rename(argv[1], argv[2]) == 0) {
        return 0;
    }

    in_fd = (int)srv_open(argv[1]);
    if (in_fd < 0) {
        cli_puts("mv: cannot open source\n");
        return 2;
    }
    out_fd = (int)srv_open_mode(argv[2], SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (out_fd < 0) {
        srv_close(in_fd);
        cli_puts("mv: cannot open dest\n");
        return 4;
    }

    for (;;) {
        long count = srv_read(in_fd, buffer, sizeof(buffer));
        if (count < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("mv: read failed\n");
            return 3;
        }
        if (count == 0) {
            break;
        }

        long result = srv_write(out_fd, buffer, (size_t)count);
        if (result < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("mv: write failed\n");
            return 4;
        }
    }
    srv_close(in_fd);
    if (srv_close(out_fd) < 0) {
        cli_puts("mv: close failed\n");
        return 6;
    }

    if (srv_unlink(argv[1]) != 0) {
        cli_puts("mv: copied but cannot remove source\n");
        return 5;
    }
    return 0;
}
