#include <srvros/cli.h>

#include <stdlib.h>

static void print_i64(int64_t value) {
    uint64_t magnitude;
    if (value < 0) {
        cli_puts("-");
        magnitude = (uint64_t)(-(value + 1)) + 1;
    } else {
        magnitude = (uint64_t)value;
    }
    cli_putn(magnitude);
}

int main(int argc, char **argv) {
    int64_t first = 1;
    int64_t increment = 1;
    int64_t last;
    char *end = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: seq [first [increment]] last\n");
        return 0;
    }
    if (argc == 2) {
        last = strtoll(argv[1], &end, 10);
    } else if (argc == 3) {
        first = strtoll(argv[1], &end, 10);
        if (end == 0 || *end != '\0') {
            cli_puts("usage: seq [first [increment]] last\n");
            return 2;
        }
        last = strtoll(argv[2], &end, 10);
    } else if (argc == 4) {
        first = strtoll(argv[1], &end, 10);
        if (end == 0 || *end != '\0') {
            cli_puts("usage: seq [first [increment]] last\n");
            return 2;
        }
        increment = strtoll(argv[2], &end, 10);
        if (end == 0 || *end != '\0' || increment == 0) {
            cli_puts("usage: seq [first [increment]] last\n");
            return 2;
        }
        last = strtoll(argv[3], &end, 10);
    } else {
        cli_puts("usage: seq [first [increment]] last\n");
        return 2;
    }
    if (end == 0 || *end != '\0') {
        cli_puts("usage: seq [first [increment]] last\n");
        return 2;
    }

    for (int64_t value = first;;) {
        if ((increment > 0 && value > last) || (increment < 0 && value < last)) {
            break;
        }
        print_i64(value);
        cli_puts("\n");
        if ((increment > 0 && value > INT64_MAX - increment) || (increment < 0 && value < INT64_MIN - increment)) {
            break;
        }
        value += increment;
    }
    return 0;
}
