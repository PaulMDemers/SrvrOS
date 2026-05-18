#include <srvros/cli.h>
#include <srvros/sys.h>

int main(int argc, char **argv) {
    int append = 0;
    int first_path = 1;
    int fds[8];
    int fd_count = 0;
    char buffer[256];
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: tee [-a] <path> [...]\n");
        return 0;
    }
    while (first_path < argc && argv[first_path][0] == '-' && argv[first_path][1] != '\0') {
        if (cli_is_option_terminator(argv[first_path])) {
            first_path++;
            break;
        }
        if (cli_streq(argv[first_path], "-a") || cli_streq(argv[first_path], "--append")) {
            append = 1;
            first_path++;
            continue;
        }
        cli_puts("usage: tee [-a] <path> [...]\n");
        return 1;
    }
    if (argc <= first_path) {
        cli_puts("usage: tee [-a] <path> [...]\n");
        return 1;
    }

    for (int i = first_path; i < argc && fd_count < (int)(sizeof(fds) / sizeof(fds[0])); i++) {
        long fd = srv_open_mode(argv[i],
            SRV_OPEN_WRITE | SRV_OPEN_CREATE | (append ? SRV_OPEN_APPEND : SRV_OPEN_TRUNC));
        if (fd < 0) {
            cli_puts("tee: cannot open ");
            cli_puts(argv[i]);
            cli_puts("\n");
            status = 1;
            continue;
        }
        fds[fd_count++] = (int)fd;
    }

    for (;;) {
        long count = srv_read(SRV_STDIN, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("tee: read failed\n");
            status = 1;
            break;
        }
        if (count == 0) {
            break;
        }
        srv_write(SRV_STDOUT, buffer, (size_t)count);
        for (int i = 0; i < fd_count; i++) {
            if (srv_write(fds[i], buffer, (size_t)count) < 0) {
                status = 1;
            }
        }
    }

    for (int i = 0; i < fd_count; i++) {
        srv_close(fds[i]);
    }
    return status;
}
