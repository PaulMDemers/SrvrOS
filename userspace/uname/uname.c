#include <srvros/cli.h>

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: uname [-a]\n");
        return 0;
    }
    if (argc > 1 && cli_streq(argv[1], "-a")) {
        cli_puts("srvros srvros 0.1 x86_64\n");
        return 0;
    }
    cli_puts("srvros\n");
    return 0;
}
