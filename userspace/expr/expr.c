#include <srvros/cli.h>

#include <stdint.h>

static int parse_i64(const char *text, int64_t *out) {
    int sign = 1;
    int saw_digit = 0;
    int64_t value = 0;

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

static int string_compare(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        }
        a++;
        b++;
    }
    if (*a == *b) {
        return 0;
    }
    return *a == '\0' ? -1 : 1;
}

static int starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static int truth_string(const char *text) {
    return text != 0 && text[0] != '\0' && !(text[0] == '0' && text[1] == '\0');
}

static int print_i64_result(int64_t value) {
    put_i64(value);
    cli_puts("\n");
    return value == 0 ? 1 : 0;
}

static int print_string_result(const char *text) {
    cli_puts(text);
    cli_puts("\n");
    return truth_string(text) ? 0 : 1;
}

static int binary_compare(const char *left, const char *op, const char *right) {
    int64_t left_number;
    int64_t right_number;
    int compare;

    if (cli_streq(op, "=")) {
        return print_i64_result(cli_streq(left, right) ? 1 : 0);
    }
    if (cli_streq(op, "!=")) {
        return print_i64_result(!cli_streq(left, right) ? 1 : 0);
    }

    if (parse_i64(left, &left_number) && parse_i64(right, &right_number)) {
        if (left_number < right_number) {
            compare = -1;
        } else if (left_number > right_number) {
            compare = 1;
        } else {
            compare = 0;
        }
    } else {
        compare = string_compare(left, right);
    }

    if (cli_streq(op, "<")) {
        return print_i64_result(compare < 0 ? 1 : 0);
    }
    if (cli_streq(op, "<=")) {
        return print_i64_result(compare <= 0 ? 1 : 0);
    }
    if (cli_streq(op, ">")) {
        return print_i64_result(compare > 0 ? 1 : 0);
    }
    if (cli_streq(op, ">=")) {
        return print_i64_result(compare >= 0 ? 1 : 0);
    }
    return 2;
}

static int binary_arithmetic(const char *left, const char *op, const char *right) {
    int64_t a;
    int64_t b;

    if (!parse_i64(left, &a) || !parse_i64(right, &b)) {
        cli_puts("expr: integer expected\n");
        return 2;
    }
    if (cli_streq(op, "+")) {
        return print_i64_result(a + b);
    }
    if (cli_streq(op, "-")) {
        return print_i64_result(a - b);
    }
    if (cli_streq(op, "*")) {
        return print_i64_result(a * b);
    }
    if (cli_streq(op, "/")) {
        if (b == 0) {
            cli_puts("expr: division by zero\n");
            return 2;
        }
        return print_i64_result(a / b);
    }
    if (cli_streq(op, "%")) {
        if (b == 0) {
            cli_puts("expr: division by zero\n");
            return 2;
        }
        return print_i64_result(a % b);
    }
    return 2;
}

static int string_index(const char *text, const char *chars) {
    for (size_t i = 0; text[i] != '\0'; i++) {
        for (size_t j = 0; chars[j] != '\0'; j++) {
            if (text[i] == chars[j]) {
                return print_i64_result((int64_t)i + 1);
            }
        }
    }
    return print_i64_result(0);
}

static int string_substr(const char *text, const char *position_text, const char *length_text) {
    int64_t position;
    int64_t length;
    size_t text_length = cli_strlen(text);
    size_t start;
    size_t out = 0;
    char result[256];

    if (!parse_i64(position_text, &position) || !parse_i64(length_text, &length)) {
        cli_puts("expr: integer expected\n");
        return 2;
    }
    if (position < 1 || length <= 0 || (uint64_t)position > text_length) {
        return print_string_result("");
    }
    start = (size_t)(position - 1);
    for (size_t i = start; text[i] != '\0' && out + 1 < sizeof(result) && length > 0; i++, length--) {
        result[out++] = text[i];
    }
    result[out] = '\0';
    return print_string_result(result);
}

static int literal_prefix_match(const char *text, const char *pattern) {
    if (starts_with(text, pattern)) {
        return print_i64_result((int64_t)cli_strlen(pattern));
    }
    return print_i64_result(0);
}

static int usage(void) {
    cli_puts("usage: expr expression\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return usage();
    }
    if (argc == 2) {
        return print_string_result(argv[1]);
    }
    if (argc == 3 && cli_streq(argv[1], "length")) {
        return print_i64_result((int64_t)cli_strlen(argv[2]));
    }
    if (argc == 4 && cli_streq(argv[2], ":")) {
        return literal_prefix_match(argv[1], argv[3]);
    }
    if (argc == 4 && cli_streq(argv[1], "index")) {
        return string_index(argv[2], argv[3]);
    }
    if (argc == 5 && cli_streq(argv[1], "substr")) {
        return string_substr(argv[2], argv[3], argv[4]);
    }
    if (argc == 4) {
        if (cli_streq(argv[2], "+") || cli_streq(argv[2], "-") || cli_streq(argv[2], "*") ||
            cli_streq(argv[2], "/") || cli_streq(argv[2], "%")) {
            return binary_arithmetic(argv[1], argv[2], argv[3]);
        }
        if (cli_streq(argv[2], "=") || cli_streq(argv[2], "!=") || cli_streq(argv[2], "<") ||
            cli_streq(argv[2], "<=") || cli_streq(argv[2], ">") || cli_streq(argv[2], ">=")) {
            return binary_compare(argv[1], argv[2], argv[3]);
        }
    }
    cli_puts("expr: unsupported expression\n");
    return 2;
}
