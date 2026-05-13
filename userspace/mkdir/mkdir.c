#include <srvros/cli.h>
#include <srvros/sys.h>

int main(int argc, char **argv) {
    int status = 0;
    if (argc < 2) {
        cli_puts("usage: mkdir <path> [...]\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (srv_mkdir(argv[i]) < 0) {
            cli_puts("mkdir: failed: ");
            cli_puts(argv[i]);
            cli_puts("\n");
            status = 1;
        }
    }
    return status;
}
