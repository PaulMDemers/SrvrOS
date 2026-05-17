#include <srvros/cli.h>

#include <stdint.h>

static int parse_i64(const char *text, int64_t *out) {
    int sign = 1;
    int64_t value = 0;
    int saw_digit = 0;
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    if (*text == '-') {
        sign = -1;
        text++;
    } else if (*text == '+') {
        text++;
    }
    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }
        saw_digit = 1;
        value = value * 10 + (int64_t)(*text - '0');
        text++;
    }
    if (!saw_digit) {
        return 0;
    }
    *out = sign < 0 ? -value : value;
    return 1;
}

static void put_i64(int64_t value) {
    uint64_t magnitude;
    if (value < 0) {
        cli_puts("-");
        magnitude = (uint64_t)(-(value + 1)) + 1;
    } else {
        magnitude = (uint64_t)value;
    }
    cli_putn(magnitude);
}

static void put_hex(uint64_t value) {
    char digits[16];
    size_t count = 0;
    if (value == 0) {
        cli_puts("0");
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        uint64_t digit = value & 0xf;
        digits[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value >>= 4;
    }
    while (count > 0) {
        srv_write(SRV_STDOUT, &digits[--count], 1);
    }
}

static char escaped_char(char c) {
    if (c == 'n') {
        return '\n';
    }
    if (c == 'r') {
        return '\r';
    }
    if (c == 't') {
        return '\t';
    }
    if (c == 'b') {
        return '\b';
    }
    if (c == 'f') {
        return '\f';
    }
    if (c == 'v') {
        return '\v';
    }
    return c;
}

static int format_consumes_args(const char *format) {
    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] == '\\' && format[i + 1] != '\0') {
            i++;
            continue;
        }
        if (format[i] == '%' && format[i + 1] != '\0' && format[i + 1] != '%') {
            return 1;
        }
        if (format[i] == '%' && format[i + 1] == '%') {
            i++;
        }
    }
    return 0;
}

static int put_format(const char *format, int argc, char **argv, int arg) {
    for (size_t i = 0; format[i] != '\0'; i++) {
        char c = format[i];
        if (c == '\\' && format[i + 1] != '\0') {
            char out = escaped_char(format[++i]);
            srv_write(SRV_STDOUT, &out, 1);
            continue;
        }
        if (c != '%' || format[i + 1] == '\0') {
            srv_write(SRV_STDOUT, &c, 1);
            continue;
        }
        c = format[++i];
        if (c == '%') {
            srv_write(SRV_STDOUT, &c, 1);
        } else if (c == 's') {
            cli_puts(arg < argc ? argv[arg++] : "");
        } else if (c == 'c') {
            char out = (arg < argc && argv[arg][0] != '\0') ? argv[arg][0] : '\0';
            if (arg < argc) {
                arg++;
            }
            srv_write(SRV_STDOUT, &out, 1);
        } else if (c == 'd' || c == 'i') {
            int64_t value = 0;
            if (arg < argc) {
                (void)parse_i64(argv[arg++], &value);
            }
            put_i64(value);
        } else if (c == 'u') {
            int64_t value = 0;
            if (arg < argc) {
                (void)parse_i64(argv[arg++], &value);
            }
            cli_putn((uint64_t)value);
        } else if (c == 'x') {
            int64_t value = 0;
            if (arg < argc) {
                (void)parse_i64(argv[arg++], &value);
            }
            put_hex((uint64_t)value);
        } else {
            srv_write(SRV_STDOUT, &c, 1);
        }
    }
    return arg;
}

int main(int argc, char **argv) {
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: printf format [args...]\n");
        return 0;
    }
    if (argc < 2) {
        cli_puts("usage: printf format [args...]\n");
        return 1;
    }
    if (!format_consumes_args(argv[1]) || argc == 2) {
        (void)put_format(argv[1], argc, argv, 2);
        return 0;
    }
    for (int arg = 2; arg < argc;) {
        int next = put_format(argv[1], argc, argv, arg);
        if (next <= arg) {
            break;
        }
        arg = next;
    }
    return 0;
}
