#include <srvros/cli.h>
#include <srvros/sys.h>

static void print_mode(uint64_t mode) {
    cli_putn((mode >> 6) & 7);
    cli_putn((mode >> 3) & 7);
    cli_putn(mode & 7);
}

static int stat_path(const char *target) {
    struct srv_stat info;
    if (srv_stat(target, &info) == 0) {
        cli_puts(target);
        cli_puts(": ");
        cli_putn(info.size);
        cli_puts(" bytes");
        if (info.type == 0) {
            cli_puts(" file");
        } else if (info.type == 1) {
            cli_puts(" directory");
        }
        cli_puts(" mode ");
        print_mode(info.mode);
        cli_puts(" inode ");
        cli_putn(info.inode);
        cli_puts("\n");
        return 0;
    }
    cli_puts("stat: not found: ");
    cli_puts(target);
    cli_puts("\n");
    return 1;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc < 2) {
        cli_puts("usage: stat <path> [...]\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (stat_path(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
