#include <srvros/cli.h>
#include <time.h>

int main(void) {
    struct timespec now;
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
