#include <srvros/cli.h>

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: whoami\n");
        return 0;
    }
    cli_puts("root\n");
    return 0;
}
