/* Exercise the actual shell arithmetic parser and formatting. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern char **environ;

#define main srv_shell_main
#include "../userspace/sh/sh.c"
#undef main

static void valid(const char *expression, int64_t expected) {
    int64_t value = 42;
    assert(evaluate_arithmetic(expression, &value));
    assert(value == expected);
}

static void invalid(const char *expression) {
    int64_t value = 42;
    assert(!evaluate_arithmetic(expression, &value));
    assert(value == 42);
}

int main(void) {
    valid("9223372036854775807", INT64_MAX);
    valid("-9223372036854775808", INT64_MIN);
    valid("-0x8000000000000000", INT64_MIN);
    valid("9223372036854775806 + 1", INT64_MAX);
    valid("-9223372036854775807 - 1", INT64_MIN);
    valid("-4611686018427387904 * 2", INT64_MIN);
    valid("-9223372036854775808 / 2", INT64_MIN / 2);
    valid("-9223372036854775808 % 3", -2);
    valid("-(-9223372036854775807)", INT64_MAX);
    valid("1 && 2", 1);
    valid("0 || 2", 1);
    invalid("9223372036854775808");
    invalid("-9223372036854775809");
    invalid("0x");
    invalid("9223372036854775807 + 1");
    invalid("-9223372036854775808 - 1");
    invalid("3037000500 * 3037000500");
    invalid("-(-9223372036854775808)");
    invalid("-9223372036854775808 / -1");
    invalid("-9223372036854775808 % -1");
    invalid("1 / 0");
    valid("0 && 1", 0);
    valid("1 || 2", 1);
    valid("0 && 1 / 0", 0);
    valid("1 || -9223372036854775808 / -1", 1);
    valid("0 && (1 || (9223372036854775808 * 2))", 0);
    invalid("1 && 1 / 0");
    invalid("0 || 9223372036854775807 + 1");
    invalid("1 || (0 && 0x)");
    assert(setenv("SRV_ARITH_TEST", "9223372036854775808", 1) == 0);
    invalid("SRV_ARITH_TEST");
    valid("1 || SRV_ARITH_TEST", 1);
    assert(setenv("SRV_ARITH_TEST", "-9223372036854775808", 1) == 0);
    valid("SRV_ARITH_TEST", INT64_MIN);
    assert(setenv("SRV_ARITH_TEST", "not-a-number", 1) == 0);
    valid("SRV_ARITH_TEST", 0);
    char output[32] = {0};
    size_t length = 0;
    append_i64(output, sizeof(output), &length, INT64_MIN);
    assert(strcmp(output, "-9223372036854775808") == 0);
    puts("shell arithmetic tests passed");
    return 0;
}
