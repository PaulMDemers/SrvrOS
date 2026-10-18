#include <srvros/cli.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: date [-u]\nDisplay the hardware-seeded UTC clock.\n");
        return 0;
    }
    if (argc > 2 || (argc == 2 && strcmp(argv[1],"-u") != 0)) {
        cli_puts("usage: date [-u]\n"); return 1;
    }
    time_t now = time(NULL);
    struct tm utc;
    if (now == (time_t)-1 || !gmtime_r(&now,&utc)) {
        cli_puts("date: UTC clock unavailable\n"); return 1;
    }
    char text[64];
    snprintf(text,sizeof(text),"%04d-%02d-%02d %02d:%02d:%02d UTC\n",
        utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec);
    cli_puts(text);
    return 0;
}
