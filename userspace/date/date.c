#include <srvros/cli.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: date [-u] [+FORMAT]\nDisplay the hardware-seeded UTC clock using C-locale calendar formats.\n");
        return 0;
    }
    int index=1;if(index<argc && !strcmp(argv[index],"-u")) index++;
    const char *format="%F %T UTC";
    if(index<argc && argv[index][0]=='+') format=argv[index++]+1;
    if(index!=argc) { cli_puts("usage: date [-u] [+FORMAT]\n");return 1; }
    time_t now = time(NULL);
    struct tm utc;
    if (now == (time_t)-1 || !gmtime_r(&now,&utc)) {
        cli_puts("date: UTC clock unavailable\n"); return 1;
    }
    char text[512];
    if(!strftime(text,sizeof(text),format,&utc) && *format) {
        cli_puts("date: unsupported format or output too long\n");return 1;
    }
    cli_puts(text);cli_puts("\n");return 0;
}
