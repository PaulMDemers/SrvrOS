#include <srvros/cli.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATCH_MAX_PATH 512

struct text_line {
    char *data;
    size_t length;
};

struct line_vec {
    struct text_line *items;
    size_t count;
    size_t capacity;
};

static void usage(void) {
    cli_puts("usage: patch [-pN] [-i file] [target]\n");
}

static int write_all(int fd, const void *buffer, size_t length) {
    const char *cursor = (const char *)buffer;
    while (length > 0) {
        ssize_t written = write(fd, cursor, length);
        if (written <= 0) {
            return 0;
        }
        cursor += written;
        length -= (size_t)written;
    }
    return 1;
}

static void free_lines(struct line_vec *lines) {
    for (size_t i = 0; i < lines->count; i++) {
        free(lines->items[i].data);
    }
    free(lines->items);
    lines->items = 0;
    lines->count = 0;
    lines->capacity = 0;
}

static int reserve_line(struct line_vec *lines) {
    if (lines->count < lines->capacity) {
        return 1;
    }
    size_t next_capacity = lines->capacity == 0 ? 64 : lines->capacity * 2;
    struct text_line *next = realloc(lines->items, next_capacity * sizeof(struct text_line));
    if (next == 0) {
        return 0;
    }
    lines->items = next;
    lines->capacity = next_capacity;
    return 1;
}

static int append_line_copy(struct line_vec *lines, const char *data, size_t length) {
    if (!reserve_line(lines)) {
        return 0;
    }
    char *copy = malloc(length + 1);
    if (copy == 0) {
        return 0;
    }
    if (length > 0) {
        memcpy(copy, data, length);
    }
    copy[length] = '\0';
    lines->items[lines->count].data = copy;
    lines->items[lines->count].length = length;
    lines->count++;
    return 1;
}

static int read_all(int fd, char **out, size_t *out_length) {
    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = malloc(capacity + 1);
    if (buffer == 0) {
        return 0;
    }
    for (;;) {
        if (used == capacity) {
            capacity *= 2;
            char *next = realloc(buffer, capacity + 1);
            if (next == 0) {
                free(buffer);
                return 0;
            }
            buffer = next;
        }
        ssize_t count = read(fd, buffer + used, capacity - used);
        if (count < 0) {
            free(buffer);
            return 0;
        }
        if (count == 0) {
            break;
        }
        used += (size_t)count;
    }
    buffer[used] = '\0';
    *out = buffer;
    *out_length = used;
    return 1;
}

static int split_lines(const char *data, size_t length, struct line_vec *lines) {
    size_t start = 0;
    for (size_t i = 0; i < length; i++) {
        if (data[i] == '\n') {
            if (!append_line_copy(lines, data + start, i + 1 - start)) {
                return 0;
            }
            start = i + 1;
        }
    }
    if (start < length) {
        return append_line_copy(lines, data + start, length - start);
    }
    return 1;
}

static int read_lines_from_fd(int fd, struct line_vec *lines) {
    char *data = 0;
    size_t length = 0;
    if (!read_all(fd, &data, &length)) {
        return 0;
    }
    int ok = split_lines(data, length, lines);
    free(data);
    return ok;
}

static int read_file_lines(const char *path, struct line_vec *lines) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        cli_puts("patch: cannot open target: ");
        cli_puts(path);
        cli_puts("\n");
        return 0;
    }
    int ok = read_lines_from_fd(fd, lines);
    close(fd);
    if (!ok) {
        cli_puts("patch: cannot read target: ");
        cli_puts(path);
        cli_puts("\n");
    }
    return ok;
}

static int write_file_lines(const char *path, const struct line_vec *lines) {
    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        cli_puts("patch: cannot write target: ");
        cli_puts(path);
        cli_puts("\n");
        return 0;
    }
    for (size_t i = 0; i < lines->count; i++) {
        if (!write_all(fd, lines->items[i].data, lines->items[i].length)) {
            close(fd);
            cli_puts("patch: write failed: ");
            cli_puts(path);
            cli_puts("\n");
            return 0;
        }
    }
    if (close(fd) < 0) {
        cli_puts("patch: close failed: ");
        cli_puts(path);
        cli_puts("\n");
        return 0;
    }
    return 1;
}

static int line_starts(const struct text_line *line, const char *prefix) {
    size_t length = cli_strlen(prefix);
    return line->length >= length && memcmp(line->data, prefix, length) == 0;
}

static int same_line(const struct text_line *line, const char *data, size_t length) {
    return line->length == length && memcmp(line->data, data, length) == 0;
}

static const char *skip_spaces(const char *cursor) {
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    return cursor;
}

static int parse_uint(const char **cursor, int *value) {
    int parsed = 0;
    const char *p = *cursor;
    if (*p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        parsed = parsed * 10 + (*p - '0');
        p++;
    }
    *cursor = p;
    *value = parsed;
    return 1;
}

static int parse_decimal_arg(const char *text, int *value) {
    const char *cursor = text;
    int parsed = 0;
    if (!parse_uint(&cursor, &parsed) || *cursor != '\0') {
        return 0;
    }
    *value = parsed;
    return 1;
}

static int parse_hunk_header(const struct text_line *line, int *old_start) {
    const char *cursor = line->data;
    int old_count = 1;
    int new_start = 0;
    int new_count = 1;
    if (!line_starts(line, "@@ -")) {
        return 0;
    }
    cursor += 4;
    if (!parse_uint(&cursor, old_start)) {
        return 0;
    }
    if (*cursor == ',') {
        cursor++;
        if (!parse_uint(&cursor, &old_count)) {
            return 0;
        }
    }
    cursor = skip_spaces(cursor);
    if (*cursor != '+') {
        return 0;
    }
    cursor++;
    if (!parse_uint(&cursor, &new_start)) {
        return 0;
    }
    if (*cursor == ',') {
        cursor++;
        if (!parse_uint(&cursor, &new_count)) {
            return 0;
        }
    }
    (void)old_count;
    (void)new_start;
    (void)new_count;
    return 1;
}

static int extract_path(const struct text_line *line, int strip_components, char *out, size_t capacity) {
    const char *cursor = line->data + 4;
    char raw[PATCH_MAX_PATH];
    size_t used = 0;
    cursor = skip_spaces(cursor);
    while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r' && *cursor != ' ' && *cursor != '\t') {
        if (used + 1 >= sizeof(raw)) {
            return 0;
        }
        raw[used++] = *cursor++;
    }
    raw[used] = '\0';
    if (raw[0] == '\0' || cli_streq(raw, "/dev/null")) {
        return 0;
    }
    const char *path = raw;
    for (int i = 0; i < strip_components; i++) {
        const char *slash = strchr(path, '/');
        if (slash == 0 || slash[1] == '\0') {
            return 0;
        }
        path = slash + 1;
    }
    while (path[0] == '.' && path[1] == '/') {
        path += 2;
    }
    if (path[0] == '\0' || cli_strlen(path) + 1 > capacity) {
        return 0;
    }
    cli_copy(out, capacity, path);
    return 1;
}

static int append_original_range(struct line_vec *out, const struct line_vec *original, size_t *index, size_t until) {
    while (*index < until && *index < original->count) {
        if (!append_line_copy(out, original->items[*index].data, original->items[*index].length)) {
            return 0;
        }
        (*index)++;
    }
    return 1;
}

static int apply_file_patch(const char *target, const struct line_vec *patch, size_t *cursor) {
    struct line_vec original = {0};
    struct line_vec output = {0};
    if (!read_file_lines(target, &original)) {
        return 0;
    }

    size_t original_index = 0;
    int saw_hunk = 0;
    while (*cursor < patch->count) {
        const struct text_line *header = &patch->items[*cursor];
        if (line_starts(header, "--- ")) {
            break;
        }
        if (!line_starts(header, "@@ ")) {
            (*cursor)++;
            continue;
        }

        int old_start = 0;
        if (!parse_hunk_header(header, &old_start)) {
            cli_puts("patch: bad hunk header\n");
            free_lines(&original);
            free_lines(&output);
            return 0;
        }
        saw_hunk = 1;
        size_t hunk_index = old_start > 0 ? (size_t)(old_start - 1) : 0;
        if (hunk_index < original_index || !append_original_range(&output, &original, &original_index, hunk_index)) {
            cli_puts("patch: hunk out of order\n");
            free_lines(&original);
            free_lines(&output);
            return 0;
        }

        (*cursor)++;
        while (*cursor < patch->count &&
               !line_starts(&patch->items[*cursor], "@@ ") &&
               !line_starts(&patch->items[*cursor], "--- ")) {
            const struct text_line *line = &patch->items[*cursor];
            if (line->length == 0) {
                (*cursor)++;
                continue;
            }
            char marker = line->data[0];
            const char *content = line->data + 1;
            size_t content_length = line->length > 0 ? line->length - 1 : 0;
            if (marker == ' ' || marker == '-') {
                if (original_index >= original.count ||
                    !same_line(&original.items[original_index], content, content_length)) {
                    cli_puts("patch: hunk failed: ");
                    cli_puts(target);
                    cli_puts("\n");
                    free_lines(&original);
                    free_lines(&output);
                    return 0;
                }
                if (marker == ' ') {
                    if (!append_line_copy(&output,
                                          original.items[original_index].data,
                                          original.items[original_index].length)) {
                        free_lines(&original);
                        free_lines(&output);
                        return 0;
                    }
                }
                original_index++;
            } else if (marker == '+') {
                if (!append_line_copy(&output, content, content_length)) {
                    free_lines(&original);
                    free_lines(&output);
                    return 0;
                }
            } else if (marker == '\\') {
                (void)0;
            } else {
                cli_puts("patch: bad hunk line\n");
                free_lines(&original);
                free_lines(&output);
                return 0;
            }
            (*cursor)++;
        }
    }

    if (!saw_hunk) {
        cli_puts("patch: no hunks for target: ");
        cli_puts(target);
        cli_puts("\n");
        free_lines(&original);
        free_lines(&output);
        return 0;
    }
    if (!append_original_range(&output, &original, &original_index, original.count) ||
        !write_file_lines(target, &output)) {
        free_lines(&original);
        free_lines(&output);
        return 0;
    }

    cli_puts("patch: ");
    cli_puts(target);
    cli_puts("\n");
    free_lines(&original);
    free_lines(&output);
    return 1;
}

int main(int argc, char **argv) {
    int strip_components = 0;
    const char *patch_path = 0;
    const char *target_override = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (cli_is_help_arg(arg)) {
            usage();
            return 0;
        }
        if (cli_streq(arg, "-i")) {
            if (i + 1 >= argc) {
                usage();
                return 2;
            }
            patch_path = argv[++i];
        } else if (arg[0] == '-' && arg[1] == 'i' && arg[2] != '\0') {
            patch_path = arg + 2;
        } else if (cli_starts_with(arg, "-p")) {
            const char *number = arg + 2;
            if (*number == '\0') {
                if (i + 1 >= argc) {
                    usage();
                    return 2;
                }
                number = argv[++i];
            }
            if (!parse_decimal_arg(number, &strip_components)) {
                usage();
                return 2;
            }
        } else if (cli_is_option_terminator(arg)) {
            if (i + 1 < argc) {
                target_override = argv[++i];
            }
            break;
        } else if (arg[0] == '-') {
            usage();
            return 2;
        } else if (target_override == 0) {
            target_override = arg;
        } else {
            usage();
            return 2;
        }
    }

    int input_fd = SRV_STDIN;
    if (patch_path != 0) {
        input_fd = open(patch_path, O_RDONLY);
        if (input_fd < 0) {
            cli_puts("patch: cannot open patch: ");
            cli_puts(patch_path);
            cli_puts("\n");
            return 1;
        }
    }

    struct line_vec patch_lines = {0};
    int ok = read_lines_from_fd(input_fd, &patch_lines);
    if (patch_path != 0) {
        close(input_fd);
    }
    if (!ok) {
        cli_puts("patch: cannot read patch\n");
        return 1;
    }

    int files = 0;
    int status = 0;
    size_t cursor = 0;
    while (cursor < patch_lines.count) {
        if (!line_starts(&patch_lines.items[cursor], "--- ")) {
            cursor++;
            continue;
        }
        if (cursor + 1 >= patch_lines.count || !line_starts(&patch_lines.items[cursor + 1], "+++ ")) {
            cli_puts("patch: missing +++ header\n");
            status = 1;
            break;
        }
        char target[PATCH_MAX_PATH];
        if (target_override != 0) {
            cli_copy(target, sizeof(target), target_override);
        } else if (!extract_path(&patch_lines.items[cursor + 1], strip_components, target, sizeof(target))) {
            cli_puts("patch: cannot determine target path\n");
            status = 1;
            break;
        }
        cursor += 2;
        files++;
        if (!apply_file_patch(target, &patch_lines, &cursor)) {
            status = 1;
            break;
        }
    }

    if (files == 0 && status == 0) {
        cli_puts("patch: no file headers found\n");
        status = 1;
    }
    free_lines(&patch_lines);
    return status;
}
