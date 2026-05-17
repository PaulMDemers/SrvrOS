#include <srvros/cli.h>

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: hostname\n");
        return 0;
    }
    (void)argv;
    cli_puts("srvros\n");
    return 0;
}
