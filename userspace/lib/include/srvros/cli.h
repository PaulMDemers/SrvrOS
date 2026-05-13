#ifndef SRVROS_USER_CLI_H
#define SRVROS_USER_CLI_H

#include <srvros/sys.h>

#include <stddef.h>
#include <stdint.h>

#define CLI_PATH_MAX 160
#define CLI_UNUSED __attribute__((unused))

static CLI_UNUSED int cli_streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static CLI_UNUSED int cli_starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static CLI_UNUSED void cli_copy(char *destination, size_t capacity, const char *source) {
    size_t i = 0;
    if (capacity == 0) {
        return;
    }
    while (source != 0 && source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static CLI_UNUSED size_t cli_strlen(const char *text) {
    size_t length = 0;
    while (text != 0 && text[length] != '\0') {
        length++;
    }
    return length;
}

static CLI_UNUSED void cli_puts(const char *text) {
    srv_write(SRV_STDOUT, text, cli_strlen(text));
}

static CLI_UNUSED void cli_putn(uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    if (value == 0) {
        cli_puts("0");
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        srv_write(SRV_STDOUT, &digits[--count], 1);
    }
}

static CLI_UNUSED char *cli_trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    end = text;
    while (*end != '\0') {
        end++;
    }
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';
    return text;
}

static CLI_UNUSED int cli_contains_slash(const char *text) {
    while (*text != '\0') {
        if (*text == '/') {
            return 1;
        }
        text++;
    }
    return 0;
}

static CLI_UNUSED int cli_join_path(char *destination, size_t capacity, const char *directory, const char *name) {
    size_t out = 0;
    if (capacity == 0 || directory == 0 || directory[0] == '\0') {
        return 0;
    }
    while (directory[out] != '\0' && out + 1 < capacity) {
        destination[out] = directory[out];
        out++;
    }
    if (out == 0 || out + 1 >= capacity) {
        return 0;
    }
    if (destination[out - 1] != '/') {
        destination[out++] = '/';
    }
    for (size_t i = 0; name != 0 && name[i] != '\0'; i++) {
        if (out + 1 >= capacity) {
            return 0;
        }
        destination[out++] = name[i];
    }
    destination[out] = '\0';
    return 1;
}

static CLI_UNUSED void cli_basename(char *destination, size_t capacity, const char *path) {
    const char *base = path;
    for (const char *p = path; p != 0 && *p != '\0'; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    cli_copy(destination, capacity, base);
}

static CLI_UNUSED void cli_normalize_path(char *destination, size_t capacity, const char *cwd, const char *path) {
    if (path == 0 || path[0] == '\0') {
        cli_copy(destination, capacity, cwd);
    } else if (path[0] == '/') {
        cli_copy(destination, capacity, path);
    } else if (cli_streq(cwd, "/")) {
        cli_join_path(destination, capacity, "/", path);
    } else {
        cli_join_path(destination, capacity, cwd, path);
    }
}

#endif
