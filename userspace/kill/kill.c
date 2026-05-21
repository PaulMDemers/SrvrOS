#include <srvros/cli.h>
#include <srvros/sys.h>

static int parse_i64(const char *text, int64_t *value_out) {
    int sign = 1;
    int64_t value = 0;
    if (*text == '-') {
        sign = -1;
        text++;
    }
    if (*text < '0' || *text > '9') {
        return 0;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (int64_t)(*text - '0');
        text++;
    }
    if (*text != '\0') {
        return 0;
    }
    *value_out = value * sign;
    return 1;
}

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: kill [-signal] <pid> [...]\n");
        return 0;
    }
    if (argc < 2) {
        cli_puts("usage: kill [-signal] <pid> [...]\n");
        return 1;
    }
    int signal = (int)SRV_SIGNAL_TERM;
    int first_target = 1;
    if (cli_streq(argv[1], "--")) {
        first_target = 2;
    } else if (argv[1][0] == '-' && argv[1][1] != '\0') {
        int64_t parsed_signal = 0;
        if (!parse_i64(argv[1] + 1, &parsed_signal) ||
            parsed_signal < 0 ||
            parsed_signal >= 64) {
            cli_puts("kill: invalid signal\n");
            return 2;
        }
        signal = (int)parsed_signal;
        first_target = 2;
    }
    if (first_target >= argc) {
        cli_puts("usage: kill [-signal] <pid> [...]\n");
        return 1;
    }
    int failures = 0;
    for (int i = first_target; i < argc; i++) {
        if (cli_streq(argv[i], "--")) {
            continue;
        }
        int64_t pid = 0;
        if (!parse_i64(argv[i], &pid)) {
            cli_puts("kill: invalid pid\n");
            failures++;
            continue;
        }
        if (srv_kill_signal(pid, (uint64_t)signal) < 0) {
            cli_puts("kill: failed\n");
            failures++;
        }
    }
    return failures == 0 ? 0 : 2;
}
