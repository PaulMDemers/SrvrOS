#include <srvros/cli.h>
#include <time.h>

int main(int argc, char **argv) {
    struct timespec now;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: date\n");
        return 0;
    }
    (void)argv;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        cli_puts("date: failed\n");
        return 1;
    }
    cli_puts("uptime ");
    cli_putn((uint64_t)now.tv_sec);
    cli_puts(".");
    long centiseconds = now.tv_nsec / 10000000;
    if (centiseconds < 10) {
        cli_puts("0");
    }
    cli_putn((uint64_t)centiseconds);
    cli_puts("s\n");
    return 0;
}
