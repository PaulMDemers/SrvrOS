#include <srvros/cli.h>

#include <stdlib.h>

int main(int argc, char **argv) {
    int quiet = 0;
    int first = 1;
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: realpath [-q] path [...]\n");
        return 0;
    }
    if (argc > 1 && cli_is_option_terminator(argv[1])) {
        first = 2;
    } else if (argc > 1 && cli_streq(argv[1], "-q")) {
        quiet = 1;
        first = 2;
    }
    if (argc <= first) {
        cli_puts("usage: realpath [-q] path [...]\n");
        return 2;
    }
    const char *pwd = getenv("PWD");
    for (int i = first; i < argc; i++) {
        char normalized[CLI_PATH_MAX];
        struct srv_stat info;
        cli_normalize_path(normalized, sizeof(normalized), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[i]);
        if (srv_stat(normalized, &info) < 0) {
            if (!quiet) {
                cli_puts("realpath: not found: ");
                cli_puts(argv[i]);
                cli_puts("\n");
            }
            status = 1;
            continue;
        }
        cli_puts(normalized);
        cli_puts("\n");
    }
    return status;
}
