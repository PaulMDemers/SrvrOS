#include <srvros/cli.h>
#include <srvros/sys.h>

int main(void) {
    uint64_t index = 0;
    struct srv_process_info info;
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
