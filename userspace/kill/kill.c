#include <srvros/cli.h>
#include <srvros/sys.h>

static uint64_t parse_u64(const char *text) {
    uint64_t value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (uint64_t)(*text - '0');
        text++;
    }
    return value;
}

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: kill <pid>\n");
        return 0;
    }
    if (argc < 2) {
        cli_puts("usage: kill <pid>\n");
        return 1;
    }
    uint64_t pid = parse_u64(argv[1]);
    if (pid == 0 || srv_kill(pid) < 0) {
        cli_puts("kill: failed\n");
        return 2;
    }
    return 0;
}
