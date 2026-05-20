#include <stdio.h>
#include <stdlib.h>
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
