#include <srvros/cli.h>
#include <srvros/sys.h>

#define SED_LINE_MAX 512
#define SED_PATTERN_MAX 96
#define SED_MAX_COMMANDS 8

struct sed_substitution {
    char from[SED_PATTERN_MAX];
    char to[SED_PATTERN_MAX];
    int global;
};

enum sed_command_type {
    SED_CMD_SUBSTITUTE,
    SED_CMD_PRINT,
    SED_CMD_DELETE
};

struct sed_command {
    enum sed_command_type type;
    uint64_t address_line;
    char address_pattern[SED_PATTERN_MAX];
    struct sed_substitution sub;
};

struct sed_program {
    struct sed_command commands[SED_MAX_COMMANDS];
    size_t count;
    int quiet;
};

static int append_char(char *out, size_t capacity, size_t *length, char c) {
    if (*length + 1 >= capacity) {
        return 0;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
    return 1;
}

static int parse_u64_text(const char *text, size_t *index, uint64_t *out) {
    uint64_t value = 0;
    size_t i = *index;
    if (text[i] < '0' || text[i] > '9') {
        return 0;
    }
    while (text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (uint64_t)(text[i] - '0');
        i++;
    }
    *index = i;
    *out = value;
    return 1;
}

static int parse_delimited(char *out, size_t capacity, const char *text, size_t *index, char delimiter) {
    size_t length = 0;
    size_t i = *index;
    out[0] = '\0';
    while (text[i] != '\0' && text[i] != delimiter) {
        if (text[i] == '\\' && text[i + 1] != '\0') {
            i++;
        }
        if (!append_char(out, capacity, &length, text[i])) {
            return 0;
        }
        i++;
    }
    if (text[i] != delimiter) {
        return 0;
    }
    *index = i + 1;
    return 1;
}

static int parse_substitution_at(const char *expression, size_t *index, struct sed_substitution *sub) {
    char delimiter;
    size_t i = *index;

    if (expression == 0 || expression[i] != 's' || expression[i + 1] == '\0') {
        return 0;
    }
    delimiter = expression[i + 1];
    i += 2;
    if (!parse_delimited(sub->from, sizeof(sub->from), expression, &i, delimiter) ||
        sub->from[0] == '\0') {
        return 0;
    }
    if (!parse_delimited(sub->to, sizeof(sub->to), expression, &i, delimiter)) {
        return 0;
    }
    sub->global = expression[i] == 'g';
    if (expression[i] == 'g') {
        i++;
    }
    *index = i;
    return 1;
}

static int starts_with_at(const char *text, size_t index, const char *needle) {
    for (size_t i = 0; needle[i] != '\0'; i++) {
        if (text[index + i] != needle[i]) {
            return 0;
        }
    }
    return 1;
}

static int contains(const char *line, const char *needle) {
    if (needle[0] == '\0') {
        return 1;
    }
    for (size_t i = 0; line[i] != '\0'; i++) {
        if (starts_with_at(line, i, needle)) {
            return 1;
        }
    }
    return 0;
}

static int command_matches(const struct sed_command *command, const char *line, uint64_t line_number) {
    if (command->address_line != 0) {
        return command->address_line == line_number;
    }
    if (command->address_pattern[0] != '\0') {
        return contains(line, command->address_pattern);
    }
    return 1;
}

static void substitute_line(char *line, size_t capacity, const struct sed_substitution *sub) {
    char out[SED_LINE_MAX];
    size_t from_length = cli_strlen(sub->from);
    size_t out_length = 0;
    int replaced = 0;
    out[0] = '\0';
    for (size_t i = 0; line[i] != '\0'; i++) {
        if ((!replaced || sub->global) && starts_with_at(line, i, sub->from)) {
            for (size_t j = 0; sub->to[j] != '\0'; j++) {
                append_char(out, sizeof(out), &out_length, sub->to[j]);
            }
            i += from_length - 1;
            replaced = 1;
            continue;
        }
        append_char(out, sizeof(out), &out_length, line[i]);
    }
    cli_copy(line, capacity, out);
}

static void print_line(const char *line) {
    cli_puts(line);
    cli_puts("\n");
}

static int parse_command(const char *expression, struct sed_program *program) {
    struct sed_command *command;
    size_t i = 0;
    if (program->count >= SED_MAX_COMMANDS) {
        cli_puts("sed: too many commands\n");
        return 0;
    }
    command = &program->commands[program->count];
    command->type = SED_CMD_SUBSTITUTE;
    command->address_line = 0;
    command->address_pattern[0] = '\0';

    if (expression[i] >= '0' && expression[i] <= '9') {
        if (!parse_u64_text(expression, &i, &command->address_line)) {
            return 0;
        }
    } else if (expression[i] == '/') {
        i++;
        if (!parse_delimited(command->address_pattern, sizeof(command->address_pattern), expression, &i, '/')) {
            return 0;
        }
    }

    if (expression[i] == 's') {
        command->type = SED_CMD_SUBSTITUTE;
        if (!parse_substitution_at(expression, &i, &command->sub)) {
            return 0;
        }
    } else if (expression[i] == 'p') {
        command->type = SED_CMD_PRINT;
        i++;
    } else if (expression[i] == 'd') {
        command->type = SED_CMD_DELETE;
        i++;
    } else {
        return 0;
    }
    if (expression[i] != '\0') {
        return 0;
    }
    program->count++;
    return 1;
}

static void process_line(char *line, const struct sed_program *program, uint64_t line_number) {
    int deleted = 0;
    for (size_t i = 0; i < program->count; i++) {
        const struct sed_command *command = &program->commands[i];
        if (!command_matches(command, line, line_number)) {
            continue;
        }
        if (command->type == SED_CMD_SUBSTITUTE) {
            substitute_line(line, SED_LINE_MAX, &command->sub);
        } else if (command->type == SED_CMD_PRINT) {
            print_line(line);
        } else if (command->type == SED_CMD_DELETE) {
            deleted = 1;
            break;
        }
    }
    if (!deleted && !program->quiet) {
        print_line(line);
    }
}

static int sed_fd(int fd, int close_fd, const struct sed_program *program) {
    char buffer[128];
    char line[SED_LINE_MAX];
    size_t length = 0;
    int status = 0;
    uint64_t line_number = 1;

    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            status = 1;
            break;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n' || length + 1 >= sizeof(line)) {
                line[length] = '\0';
                process_line(line, program, line_number++);
                length = 0;
            } else {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        process_line(line, program, line_number);
    }
    if (close_fd) {
        srv_close(fd);
    }
    return status;
}

static int sed_file(const char *path, const struct sed_program *program) {
    if (cli_streq(path, "-")) {
        return sed_fd(SRV_STDIN, 0, program);
    }
    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("sed: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return sed_fd(fd, 1, program);
}

int main(int argc, char **argv) {
    struct sed_program program = {0};
    int status = 0;
    int first_file = 1;

    if (argc < 2) {
        cli_puts("usage: sed [-n] [-e script] script [file ...]\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (cli_streq(argv[i], "-n")) {
            program.quiet = 1;
            first_file = i + 1;
        } else if (cli_streq(argv[i], "-e")) {
            if (i + 1 >= argc || !parse_command(argv[i + 1], &program)) {
                cli_puts("sed: bad script\n");
                return 1;
            }
            i++;
            first_file = i + 1;
        } else if (program.count == 0) {
            if (!parse_command(argv[i], &program)) {
                cli_puts("sed: bad script\n");
                return 1;
            }
            first_file = i + 1;
        } else {
            first_file = i;
            break;
        }
    }
    if (program.count == 0) {
        cli_puts("usage: sed [-n] [-e script] script [file ...]\n");
        return 1;
    }
    if (argc <= first_file) {
        return sed_fd(SRV_STDIN, 0, &program);
    }
    for (int i = first_file; i < argc; i++) {
        if (sed_file(argv[i], &program) != 0) {
            status = 1;
        }
    }
    return status;
}
