/* Standalone host regression; compile with userspace/lib/include and shared/include. */
#define main expr_program_main
#include "../userspace/expr/expr.c"
#undef main

long srv_write(int fd, const void *buffer, size_t length) {
    (void)fd; (void)buffer; return (long)length;
}

int main(void) {
    int64_t value;
    if (!parse_i64("-9223372036854775808", &value) || value != INT64_MIN) return 1;
    if (!parse_i64("9223372036854775807", &value) || value != INT64_MAX) return 2;
    if (parse_i64("9223372036854775808", &value) ||
        parse_i64("-9223372036854775809", &value)) return 3;
    if (binary_arithmetic("9223372036854775807", "+", "1") != 2 ||
        binary_arithmetic("-9223372036854775808", "-", "1") != 2 ||
        binary_arithmetic("9223372036854775807", "*", "2") != 2 ||
        binary_arithmetic("-9223372036854775808", "/", "-1") != 2 ||
        binary_arithmetic("-9223372036854775808", "%", "-1") != 2) return 4;
    return 0;
}
