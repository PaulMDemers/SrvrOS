#include <srvros/cli.h>
#include <srvros/sys.h>

int main(int argc, char **argv) {
    uint64_t index = 0;
    struct srv_process_info info;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: ps\n");
        return 0;
    }
    (void)argv;
    cli_puts("PID STATE      NAME\n");
    for (;;) {
        long next = srv_proc_list(index, &info);
        if (next <= 0) {
            break;
        }
        cli_putn(info.pid);
        cli_puts("   ");
        cli_puts(info.state);
        cli_puts(" ");
        cli_puts(info.name);
        cli_puts("\n");
        index = (uint64_t)next;
    }
    return 0;
}
