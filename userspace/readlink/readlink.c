#include <srvros/cli.h>

#include <stdlib.h>

int main(int argc, char **argv) {
    int canonical = 0;
    int first = 1;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: readlink [-f|-e] path\n");
        return 0;
    }
    if (argc > 1 && cli_is_option_terminator(argv[1])) {
        first = 2;
    } else if (argc > 1 && (cli_streq(argv[1], "-f") || cli_streq(argv[1], "-e"))) {
        canonical = 1;
        first = 2;
    }
    if (argc != first + 1) {
        cli_puts("usage: readlink [-f|-e] path\n");
        return 2;
    }

    if (!canonical) {
        cli_puts("readlink: not a symbolic link: ");
        cli_puts(argv[first]);
        cli_puts("\n");
        return 1;
    }

    const char *pwd = getenv("PWD");
    char normalized[CLI_PATH_MAX];
    struct srv_stat info;
    cli_normalize_path(normalized, sizeof(normalized), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[first]);
    if (srv_stat(normalized, &info) < 0) {
        cli_puts("readlink: not found: ");
        cli_puts(argv[first]);
        cli_puts("\n");
        return 1;
    }
    cli_puts(normalized);
    cli_puts("\n");
    return 0;
}
