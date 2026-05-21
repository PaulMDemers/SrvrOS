#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

struct line_buffer {
    char **lines;
    size_t count;
    size_t capacity;
    size_t current;
    int modified;
};

static char filename[160];
static int quiet;

static void usage(void) {
    puts("usage: ed [-s] [file]");
}

static char *dup_line(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy != 0) {
        memcpy(copy, text, length);
    }
    return copy;
}

static void free_lines(struct line_buffer *buffer) {
    for (size_t i = 0; i < buffer->count; i++) {
        free(buffer->lines[i]);
    }
    free(buffer->lines);
    buffer->lines = 0;
    buffer->count = 0;
    buffer->capacity = 0;
    buffer->current = 0;
    buffer->modified = 0;
}

static int ensure_capacity(struct line_buffer *buffer, size_t needed) {
    if (needed <= buffer->capacity) {
        return 0;
    }
    size_t next = buffer->capacity == 0 ? 16 : buffer->capacity * 2;
    while (next < needed) {
        next *= 2;
    }
    char **lines = realloc(buffer->lines, next * sizeof(*lines));
    if (lines == 0) {
        puts("?");
        return -1;
    }
    buffer->lines = lines;
    buffer->capacity = next;
    return 0;
}

static int insert_line(struct line_buffer *buffer, size_t index, const char *text) {
    if (index > buffer->count || ensure_capacity(buffer, buffer->count + 1) < 0) {
        return -1;
    }
    char *copy = dup_line(text);
    if (copy == 0) {
        puts("?");
        return -1;
    }
    for (size_t i = buffer->count; i > index; i--) {
        buffer->lines[i] = buffer->lines[i - 1];
    }
    buffer->lines[index] = copy;
    buffer->count++;
    buffer->current = index + 1;
    buffer->modified = 1;
    return 0;
}

static int append_input(struct line_buffer *buffer, size_t after) {
    char input[512];
    size_t index = after;
    while (fgets(input, sizeof(input), stdin) != 0) {
        if (strcmp(input, ".\n") == 0 || strcmp(input, ".") == 0) {
            return 0;
        }
        if (insert_line(buffer, index, input) < 0) {
            return -1;
        }
        index++;
    }
    return 0;
}

static int delete_range(struct line_buffer *buffer, size_t first, size_t last) {
    if (first == 0 || last < first || last > buffer->count) {
        puts("?");
        return -1;
    }
    for (size_t i = first - 1; i < last; i++) {
        free(buffer->lines[i]);
    }
    size_t removed = last - first + 1;
    for (size_t i = first - 1; i + removed < buffer->count; i++) {
        buffer->lines[i] = buffer->lines[i + removed];
    }
    buffer->count -= removed;
    buffer->current = first <= buffer->count ? first : buffer->count;
    buffer->modified = 1;
    return 0;
}

static int load_file(struct line_buffer *buffer, const char *path) {
    FILE *file = fopen(path, "r");
    if (file == 0) {
        return -1;
    }
    free_lines(buffer);
    char input[512];
    while (fgets(input, sizeof(input), file) != 0) {
        if (insert_line(buffer, buffer->count, input) < 0) {
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    buffer->modified = 0;
    buffer->current = buffer->count;
    return 0;
}

static int write_file(struct line_buffer *buffer, const char *path) {
    FILE *file = fopen(path, "w");
    if (file == 0) {
        puts("?");
        return -1;
    }
    size_t bytes = 0;
    for (size_t i = 0; i < buffer->count; i++) {
        fputs(buffer->lines[i], file);
        bytes += strlen(buffer->lines[i]);
    }
    if (fclose(file) != 0) {
        puts("?");
        return -1;
    }
    buffer->modified = 0;
    printf("%u\n", (unsigned)bytes);
    return 0;
}

static int parse_delimited(char *out, size_t capacity, const char **cursor, char delimiter) {
    const char *text = *cursor;
    size_t length = 0;
    if (*text != delimiter) {
        return 0;
    }
    text++;
    while (*text != '\0' && *text != delimiter && *text != '\n' && length + 1 < capacity) {
        if (*text == '\\' && text[1] != '\0') {
            text++;
            if (*text != delimiter && length + 1 < capacity) {
                out[length++] = '\\';
            }
        }
        out[length++] = *text++;
    }
    if (*text != delimiter) {
        return 0;
    }
    out[length] = '\0';
    *cursor = text + 1;
    return 1;
}

static int parse_until_delimiter(char *out, size_t capacity, const char **cursor, char delimiter) {
    const char *text = *cursor;
    size_t length = 0;
    while (*text != '\0' && *text != delimiter && *text != '\n' && length + 1 < capacity) {
        if (*text == '\\' && text[1] != '\0') {
            text++;
            if (*text != delimiter && length + 1 < capacity) {
                out[length++] = '\\';
            }
        }
        out[length++] = *text++;
    }
    if (*text != delimiter) {
        return 0;
    }
    out[length] = '\0';
    *cursor = text + 1;
    return 1;
}

static size_t find_regex_address(const struct line_buffer *buffer, const char *pattern, size_t fallback) {
    regex_t regex;
    if (buffer->count == 0 || regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
        puts("?");
        return 0;
    }
    size_t start = fallback == 0 || fallback > buffer->count ? 1 : fallback + 1;
    for (size_t step = 0; step < buffer->count; step++) {
        size_t index = ((start - 1 + step) % buffer->count) + 1;
        if (regexec(&regex, buffer->lines[index - 1], 0, 0, 0) == 0) {
            regfree(&regex);
            return index;
        }
    }
    regfree(&regex);
    puts("?");
    return 0;
}

static size_t parse_address(const char **cursor, const struct line_buffer *buffer, size_t fallback) {
    const char *text = *cursor;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == '.') {
        *cursor = text + 1;
        return buffer->current;
    }
    if (*text == '$') {
        *cursor = text + 1;
        return buffer->count;
    }
    if (*text >= '0' && *text <= '9') {
        size_t value = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10 + (size_t)(*text - '0');
            text++;
        }
        *cursor = text;
        return value;
    }
    if (*text == '/') {
        char pattern[160];
        const char *pattern_cursor = text;
        if (!parse_delimited(pattern, sizeof(pattern), &pattern_cursor, '/')) {
            puts("?");
            return 0;
        }
        *cursor = pattern_cursor;
        return find_regex_address(buffer, pattern, fallback);
    }
    *cursor = text;
    return fallback;
}

static void parse_range(const char **cursor, const struct line_buffer *buffer, size_t *first, size_t *last) {
    size_t fallback = buffer->current == 0 ? buffer->count : buffer->current;
    *first = parse_address(cursor, buffer, fallback);
    *last = *first;
    const char *text = *cursor;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == ',') {
        text++;
        *cursor = text;
        *last = parse_address(cursor, buffer, buffer->count);
    } else {
        *cursor = text;
    }
}

static const char *skip_spaces(const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

static char *trim_arg(char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 &&
           (text[length - 1] == '\n' || text[length - 1] == '\r' ||
            text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[--length] = '\0';
    }
    return text;
}

static void append_text_range(char *out, size_t capacity, size_t *length,
    const char *text, size_t start, size_t end) {
    for (size_t i = start; i < end && *length + 1 < capacity; i++) {
        out[(*length)++] = text[i];
        out[*length] = '\0';
    }
}

static void append_replacement(char *out, size_t capacity, size_t *length,
    const char *line, const char *replacement, regmatch_t *matches, size_t match_count) {
    for (size_t i = 0; replacement[i] != '\0' && *length + 1 < capacity; i++) {
        if (replacement[i] == '&') {
            append_text_range(out, capacity, length, line,
                (size_t)matches[0].rm_so, (size_t)matches[0].rm_eo);
        } else if (replacement[i] == '\\' && replacement[i + 1] >= '0' && replacement[i + 1] <= '9') {
            size_t index = (size_t)(replacement[++i] - '0');
            if (index < match_count && matches[index].rm_so >= 0 && matches[index].rm_eo >= matches[index].rm_so) {
                append_text_range(out, capacity, length, line,
                    (size_t)matches[index].rm_so, (size_t)matches[index].rm_eo);
            }
        } else if (replacement[i] == '\\' && replacement[i + 1] != '\0') {
            out[(*length)++] = replacement[++i];
            out[*length] = '\0';
        } else {
            out[(*length)++] = replacement[i];
            out[*length] = '\0';
        }
    }
}

static int substitute_one_line(struct line_buffer *buffer, size_t index, regex_t *regex,
    const char *replacement, int global) {
    char line_storage[512];
    const char *source = buffer->lines[index - 1];
    size_t source_length = strlen(source);
    int had_newline = source_length > 0 && source[source_length - 1] == '\n';
    if (source_length >= sizeof(line_storage)) {
        source_length = sizeof(line_storage) - 1;
    }
    memcpy(line_storage, source, source_length);
    line_storage[source_length] = '\0';
    if (had_newline && source_length > 0) {
        line_storage[source_length - 1] = '\0';
    }
    const char *line = line_storage;
    char out[512];
    size_t out_length = 0;
    size_t offset = 0;
    int changed = 0;
    out[0] = '\0';
    while (line[offset] != '\0') {
        regmatch_t matches[10];
        if (regexec(regex, line + offset, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
            append_text_range(out, sizeof(out), &out_length, line, offset, strlen(line));
            break;
        }
        for (size_t i = 0; i < sizeof(matches) / sizeof(matches[0]); i++) {
            if (matches[i].rm_so >= 0) {
                matches[i].rm_so += (regoff_t)offset;
                matches[i].rm_eo += (regoff_t)offset;
            }
        }
        append_text_range(out, sizeof(out), &out_length, line, offset, (size_t)matches[0].rm_so);
        append_replacement(out, sizeof(out), &out_length, line, replacement, matches,
            sizeof(matches) / sizeof(matches[0]));
        offset = matches[0].rm_eo > matches[0].rm_so ? (size_t)matches[0].rm_eo : (size_t)matches[0].rm_eo + 1;
        changed = 1;
        if (!global) {
            append_text_range(out, sizeof(out), &out_length, line, offset, strlen(line));
            break;
        }
    }
    if (!changed) {
        return 0;
    }
    if (had_newline && out_length + 1 < sizeof(out)) {
        out[out_length++] = '\n';
        out[out_length] = '\0';
    }
    char *copy = dup_line(out);
    if (copy == 0) {
        puts("?");
        return -1;
    }
    free(buffer->lines[index - 1]);
    buffer->lines[index - 1] = copy;
    buffer->current = index;
    buffer->modified = 1;
    return 1;
}

static int substitute_range(struct line_buffer *buffer, size_t first, size_t last, const char *script) {
    const char *cursor = script;
    char pattern[160];
    char replacement[160];
    int global = 0;
    regex_t regex;
    if (*cursor != 's') {
        return -1;
    }
    cursor++;
    char delimiter = *cursor;
    if (delimiter == '\0' || delimiter == '\n') {
        puts("?");
        return -1;
    }
    if (!parse_delimited(pattern, sizeof(pattern), &cursor, delimiter) ||
        !parse_until_delimiter(replacement, sizeof(replacement), &cursor, delimiter)) {
        puts("?");
        return -1;
    }
    cursor = skip_spaces(cursor);
    if (*cursor == 'g') {
        global = 1;
        cursor++;
    }
    cursor = skip_spaces(cursor);
    if (*cursor != '\0' && *cursor != '\n') {
        puts("?");
        return -1;
    }
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        puts("?");
        return -1;
    }
    int any = 0;
    for (size_t i = first; i <= last; i++) {
        int result = substitute_one_line(buffer, i, &regex, replacement, global);
        if (result < 0) {
            regfree(&regex);
            return -1;
        }
        any = any || result > 0;
    }
    regfree(&regex);
    if (!any) {
        puts("?");
        return -1;
    }
    return 0;
}

static int run_command(struct line_buffer *buffer, char *command) {
    const char *cursor = command;
    size_t first = 0;
    size_t last = 0;
    parse_range(&cursor, buffer, &first, &last);
    cursor = skip_spaces(cursor);
    char op = *cursor == '\0' || *cursor == '\n' ? 'p' : *cursor++;
    cursor = skip_spaces(cursor);

    switch (op) {
    case 'a':
        return append_input(buffer, last);
    case 'i':
        return append_input(buffer, first == 0 ? 0 : first - 1);
    case 'c':
        if (buffer->count != 0 && delete_range(buffer, first, last) < 0) {
            return -1;
        }
        return append_input(buffer, first == 0 ? 0 : first - 1);
    case 'd':
        return delete_range(buffer, first, last);
    case 'p':
        if (first == 0 || last > buffer->count || last < first) {
            puts("?");
            return -1;
        }
        for (size_t i = first; i <= last; i++) {
            fputs(buffer->lines[i - 1], stdout);
        }
        buffer->current = last;
        return 0;
    case 's':
        if (first == 0 || last > buffer->count || last < first) {
            puts("?");
            return -1;
        }
        {
            char script[512];
            script[0] = 's';
            strncpy(script + 1, cursor, sizeof(script) - 2);
            script[sizeof(script) - 1] = '\0';
            return substitute_range(buffer, first, last, script);
        }
    case 'e':
        {
            char *path = trim_arg((char *)cursor);
            if (*path == '\0') {
                path = filename;
            }
            if (*path == '\0' || load_file(buffer, path) < 0) {
                puts("?");
                return -1;
            }
            strncpy(filename, path, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
        return 0;
    case 'r':
        cursor = trim_arg((char *)cursor);
        if (*cursor == '\0') {
            puts("?");
            return -1;
        }
        {
            struct line_buffer incoming = {0};
            if (load_file(&incoming, cursor) < 0) {
                puts("?");
                return -1;
            }
            for (size_t i = 0; i < incoming.count; i++) {
                if (insert_line(buffer, last + i, incoming.lines[i]) < 0) {
                    free_lines(&incoming);
                    return -1;
                }
            }
            free_lines(&incoming);
        }
        return 0;
    case 'w':
        {
            char *path = trim_arg((char *)cursor);
            if (*path == '\0') {
                path = filename;
            }
            if (*path == '\0') {
                puts("?");
                return -1;
            }
            strncpy(filename, path, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
        return write_file(buffer, filename);
    case 'q':
        return 1;
    case 'Q':
        buffer->modified = 0;
        return 1;
    case 'H':
        return 0;
    default:
        puts("?");
        return -1;
    }
}

int main(int argc, char **argv) {
    struct line_buffer buffer = {0};
    char command[512];
    int argi = 1;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        quiet = 1;
        argi++;
    }
    if (argc > argi) {
        strncpy(filename, argv[argi], sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        (void)load_file(&buffer, filename);
    }

    while (fgets(command, sizeof(command), stdin) != 0) {
        if (!quiet) {
            fflush(stdout);
        }
        int result = run_command(&buffer, command);
        if (result > 0) {
            break;
        }
    }
    free_lines(&buffer);
    return 0;
}
