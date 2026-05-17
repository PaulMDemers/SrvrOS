#include <srvros/cli.h>
#include <srvros/conio.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>

#include "linenoise.h"

int linenoiseHistoryClear(void);
size_t linenoiseHistoryLen(void);
const char *linenoiseHistoryGet(size_t index);

#define LINE_MAX 256
#define EXPANDED_LINE_MAX 512
#define ARG_EXPANDED_MAX 512
#define PATH_MAX_ENTRIES 8
#define PIPELINE_MAX_COMMANDS 6
#define SHELL_MAX_ALIASES 16
#define SHELL_MAX_FUNCTIONS 16
#define SHELL_MAX_FUNCTION_ARGS 16
#define SHELL_MAX_JOBS 32

static char path_entries[PATH_MAX_ENTRIES][CLI_PATH_MAX] = {
    "/fat/bin",
    "/",
    "/fat",
};
static size_t path_count = 3;
static uint64_t last_status = 0;
static uint64_t shell_pid = 0;
static uint64_t substitution_counter = 0;
static int shell_argc = 1;
static char **shell_argv = 0;
static int exit_on_error = 0;
static int function_depth = 0;
static int return_requested = 0;
static uint64_t return_status = 0;
static int loop_depth = 0;
static uint64_t break_requested = 0;
static uint64_t continue_requested = 0;
static uint64_t last_background_pid = 0;
static char completion_cwd[CLI_PATH_MAX] = "/";
static const char *active_script_path;
static uint64_t active_script_line;

static const char *shell_builtins[] = {
    "help", "man", "apropos", "exit", "exec", "return", "shift", "set", "source", ".", "path", "cd", "pwd", "clear",
    "echo", "env", "export", "unset", "alias", "history", "type", "which", "command", "test", "[",
    "break", "continue",
    "jobs", "wait", "fg", "bg", "kill", "service", "dhcp", "net", "dns", "rmdir", "read", ":",
};

struct shell_alias {
    char name[32];
    char value[LINE_MAX];
};

struct shell_function {
    char name[32];
    char body[EXPANDED_LINE_MAX];
};

struct shell_job {
    int used;
    uint64_t id;
    uint64_t group;
    size_t count;
    long pids[PIPELINE_MAX_COMMANDS];
    char command[LINE_MAX];
};

static struct shell_alias aliases[SHELL_MAX_ALIASES];
static struct shell_function functions[SHELL_MAX_FUNCTIONS];
static struct shell_job jobs[SHELL_MAX_JOBS];
static uint64_t next_job_id = 1;
static uint64_t current_job_id = 0;
static uint64_t previous_job_id = 0;

static uint64_t run_line(char *line, char *cwd);
static uint64_t run_command_impl(char *line, char *cwd, int background, int bypass_alias_functions);
static int expand_variables(const char *input, char *out, size_t capacity, char *cwd);
static int resolve_command(char *out, size_t capacity, const char *command);
static int split_words(char *text, char **argv, size_t capacity);
static int valid_shell_name(const char *name);
static void set_path_list(const char *value);
static void wait_pipeline_pids(long *pids, size_t count, uint64_t *last_status);
static void print_script_context(void);
static void shell_error(const char *message);

struct command_redirection {
    int stdin_set;
    int stdout_set;
    int stderr_set;
    int stderr_to_stdout;
    int stdout_append;
    int stderr_append;
    char stdin_path[CLI_PATH_MAX];
    char stdout_path[CLI_PATH_MAX];
    char stderr_path[CLI_PATH_MAX];
};

struct if_parts {
    size_t condition_start;
    size_t condition_end;
    size_t then_start;
    size_t then_end;
    size_t else_start;
    size_t else_end;
    size_t command_end;
    int has_else;
};

struct for_parts {
    size_t name_start;
    size_t name_end;
    size_t words_start;
    size_t words_end;
    size_t body_start;
    size_t body_end;
    size_t command_end;
};

struct while_parts {
    size_t condition_start;
    size_t condition_end;
    size_t body_start;
    size_t body_end;
    size_t command_end;
};

struct case_parts {
    size_t word_start;
    size_t word_end;
    size_t arms_start;
    size_t arms_end;
    size_t command_end;
};

static int find_if_parts(const char *line, struct if_parts *parts);
static int find_for_parts(const char *line, struct for_parts *parts);
static int find_while_parts(const char *line, struct while_parts *parts);
static int find_case_parts(const char *line, struct case_parts *parts);
static int find_group_parts(const char *line, size_t *body_start, size_t *body_end, size_t *command_end);
static int find_function_definition(const char *line, char *name, size_t name_capacity, char *body, size_t body_capacity);
static int is_function_start(const char *line);

static int shell_is_name_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int shell_is_name_char(char c) {
    return shell_is_name_start(c) || (c >= '0' && c <= '9');
}

static int shell_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int shell_is_separator(char c) {
    return c == '\0' || c == ' ' || c == '\t' || c == ';' || c == '\r' || c == '\n' || c == '(' || c == ')';
}

static int shell_keyword_at(const char *text, size_t index, const char *keyword) {
    size_t i = 0;
    if (index > 0 && !shell_is_separator(text[index - 1])) {
        return 0;
    }
    while (keyword[i] != '\0') {
        if (text[index + i] != keyword[i]) {
            return 0;
        }
        i++;
    }
    return shell_is_separator(text[index + i]);
}

static void append_char(char *out, size_t capacity, size_t *length, char c) {
    if (*length + 1 >= capacity) {
        return;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
}

static void append_text(char *out, size_t capacity, size_t *length, const char *text) {
    while (text != 0 && *text != '\0') {
        append_char(out, capacity, length, *text++);
    }
}

static void append_number(char *out, size_t capacity, size_t *length, uint64_t value) {
    char digits[21];
    size_t count = 0;
    if (value == 0) {
        append_char(out, capacity, length, '0');
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        append_char(out, capacity, length, digits[--count]);
    }
}

static const char *positional_parameter(int index) {
    if (shell_argv == 0 || index < 0 || index >= shell_argc || shell_argv[index] == 0) {
        return "";
    }
    return shell_argv[index];
}

static void append_positional_all(char *out, size_t capacity, size_t *length) {
    for (int i = 1; i < shell_argc; i++) {
        if (i > 1) {
            append_char(out, capacity, length, ' ');
        }
        append_text(out, capacity, length, positional_parameter(i));
    }
}

static size_t find_command_substitution_end(const char *input, size_t open_index) {
    size_t depth = 1;
    char quote = '\0';
    for (size_t i = open_index + 2; input[i] != '\0'; i++) {
        char c = input[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && input[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && input[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == '$' && input[i + 1] == '(') {
            depth++;
            i++;
            continue;
        }
        if (c == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return 0;
}

static void make_substitution_path(char *path, size_t capacity) {
    size_t length = 0;
    path[0] = '\0';
    append_text(path, capacity, &length, "/fat/.srvsh_subst_");
    append_number(path, capacity, &length, shell_pid);
    append_char(path, capacity, &length, '_');
    append_number(path, capacity, &length, substitution_counter++);
}

static int append_command_substitution(const char *command_text,
    size_t command_length,
    char *cwd,
    char *out,
    size_t capacity,
    size_t *length) {
    char temp_path[CLI_PATH_MAX];
    char command[EXPANDED_LINE_MAX];
    size_t command_out = 0;
    int fd;
    char buffer[128];
    char captured[EXPANDED_LINE_MAX];
    size_t captured_length = 0;

    make_substitution_path(temp_path, sizeof(temp_path));
    if (command_length + cli_strlen(temp_path) + 4 >= sizeof(command)) {
        cli_puts("sh: command substitution too long\n");
        return 0;
    }
    for (size_t i = 0; i < command_length && command_out + 1 < sizeof(command); i++) {
        command[command_out++] = command_text[i];
    }
    command[command_out] = '\0';
    append_text(command, sizeof(command), &command_out, " > ");
    append_text(command, sizeof(command), &command_out, temp_path);

    (void)srv_unlink(temp_path);
    (void)run_line(command, cwd);
    fd = (int)srv_open(temp_path);
    if (fd < 0) {
        return 1;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        for (long i = 0; i < count && captured_length + 1 < sizeof(captured); i++) {
            char c = buffer[i];
            captured[captured_length++] = (c == '\r' || c == '\n') ? ' ' : c;
        }
    }
    srv_close(fd);
    (void)srv_unlink(temp_path);
    while (captured_length > 0 && captured[captured_length - 1] == ' ') {
        captured_length--;
    }
    captured[captured_length] = '\0';
    append_text(out, capacity, length, captured);
    return 1;
}

static int parameter_pattern_match(const char *pattern, const char *text) {
    if (*pattern == '\0') {
        return *text == '\0';
    }
    if (*pattern == '*') {
        while (pattern[1] == '*') {
            pattern++;
        }
        if (parameter_pattern_match(pattern + 1, text)) {
            return 1;
        }
        return *text != '\0' && parameter_pattern_match(pattern, text + 1);
    }
    if (*pattern == '?') {
        return *text != '\0' && parameter_pattern_match(pattern + 1, text + 1);
    }
    return *pattern == *text && parameter_pattern_match(pattern + 1, text + 1);
}

static void copy_slice(char *out, size_t capacity, const char *text, size_t start, size_t end) {
    size_t length = 0;
    if (capacity == 0) {
        return;
    }
    for (size_t i = start; i < end && length + 1 < capacity; i++) {
        out[length++] = text[i];
    }
    out[length] = '\0';
}

static void shell_parameter_value(const char *name, char *out, size_t capacity, int *is_set) {
    const char *value = 0;
    char number[32];
    size_t length = 0;
    number[0] = '\0';
    if (capacity != 0) {
        out[0] = '\0';
    }
    *is_set = 1;
    if (cli_streq(name, "?")) {
        append_number(number, sizeof(number), &length, last_status);
        value = number;
    } else if (cli_streq(name, "$")) {
        append_number(number, sizeof(number), &length, shell_pid);
        value = number;
    } else if (cli_streq(name, "!")) {
        if (last_background_pid != 0) {
            append_number(number, sizeof(number), &length, last_background_pid);
        }
        value = number;
    } else if (cli_streq(name, "#")) {
        append_number(number, sizeof(number), &length, shell_argc > 0 ? (uint64_t)(shell_argc - 1) : 0);
        value = number;
    } else if (cli_streq(name, "@")) {
        size_t out_length = 0;
        out[0] = '\0';
        append_positional_all(out, capacity, &out_length);
        return;
    } else if (name[0] >= '0' && name[0] <= '9' && name[1] == '\0') {
        value = positional_parameter(name[0] - '0');
    } else {
        value = getenv(name);
        *is_set = value != 0;
    }
    cli_copy(out, capacity, value != 0 ? value : "");
}

static int valid_assignable_parameter(const char *name) {
    return valid_shell_name(name);
}

static void append_trimmed_prefix(char *out,
    size_t capacity,
    size_t *length,
    const char *value,
    const char *pattern,
    int longest) {
    size_t value_length = cli_strlen(value);
    if (longest) {
        for (size_t cut = value_length; cut > 0; cut--) {
            char prefix[ARG_EXPANDED_MAX];
            copy_slice(prefix, sizeof(prefix), value, 0, cut);
            if (parameter_pattern_match(pattern, prefix)) {
                append_text(out, capacity, length, value + cut);
                return;
            }
        }
    } else {
        for (size_t cut = 0; cut <= value_length; cut++) {
            char prefix[ARG_EXPANDED_MAX];
            copy_slice(prefix, sizeof(prefix), value, 0, cut);
            if (parameter_pattern_match(pattern, prefix)) {
                append_text(out, capacity, length, value + cut);
                return;
            }
        }
    }
    append_text(out, capacity, length, value);
}

static void append_trimmed_suffix(char *out,
    size_t capacity,
    size_t *length,
    const char *value,
    const char *pattern,
    int longest) {
    size_t value_length = cli_strlen(value);
    if (longest) {
        for (size_t cut = 0; cut < value_length; cut++) {
            char suffix[ARG_EXPANDED_MAX];
            copy_slice(suffix, sizeof(suffix), value, cut, value_length);
            if (parameter_pattern_match(pattern, suffix)) {
                copy_slice(suffix, sizeof(suffix), value, 0, cut);
                append_text(out, capacity, length, suffix);
                return;
            }
        }
    } else {
        for (size_t cut = value_length; cut > 0; cut--) {
            char suffix[ARG_EXPANDED_MAX];
            copy_slice(suffix, sizeof(suffix), value, cut, value_length);
            if (parameter_pattern_match(pattern, suffix)) {
                copy_slice(suffix, sizeof(suffix), value, 0, cut);
                append_text(out, capacity, length, suffix);
                return;
            }
        }
        if (parameter_pattern_match(pattern, "")) {
            append_text(out, capacity, length, value);
            return;
        }
    }
    append_text(out, capacity, length, value);
}

static int append_parameter_expansion(const char *body,
    char *cwd,
    char *out,
    size_t capacity,
    size_t *length) {
    char name[64];
    char value[ARG_EXPANDED_MAX];
    char word[ARG_EXPANDED_MAX];
    char expanded_word[ARG_EXPANDED_MAX];
    size_t cursor = 0;
    size_t name_length = 0;
    int is_set = 0;
    int colon = 0;
    char op = '\0';
    int doubled = 0;

    if (body[0] == '#') {
        if (body[1] == '\0') {
            append_number(out, capacity, length, shell_argc > 0 ? (uint64_t)(shell_argc - 1) : 0);
            return 1;
        }
        cursor = 1;
        if (shell_is_name_start(body[cursor])) {
            while (shell_is_name_char(body[cursor]) && name_length + 1 < sizeof(name)) {
                name[name_length++] = body[cursor++];
            }
        } else if ((body[cursor] >= '0' && body[cursor] <= '9') || body[cursor] == '@' ||
            body[cursor] == '?' || body[cursor] == '!' || body[cursor] == '$') {
            name[name_length++] = body[cursor++];
        } else {
            cli_puts("sh: bad substitution\n");
            return 0;
        }
        if (body[cursor] != '\0') {
            cli_puts("sh: bad substitution\n");
            return 0;
        }
        name[name_length] = '\0';
        shell_parameter_value(name, value, sizeof(value), &is_set);
        append_number(out, capacity, length, cli_strlen(value));
        return 1;
    }

    if (shell_is_name_start(body[cursor])) {
        while (shell_is_name_char(body[cursor]) && name_length + 1 < sizeof(name)) {
            name[name_length++] = body[cursor++];
        }
    } else if ((body[cursor] >= '0' && body[cursor] <= '9') || body[cursor] == '@' ||
        body[cursor] == '?' || body[cursor] == '!' || body[cursor] == '$' || body[cursor] == '#') {
        name[name_length++] = body[cursor++];
    } else {
        cli_puts("sh: bad substitution\n");
        return 0;
    }
    name[name_length] = '\0';
    shell_parameter_value(name, value, sizeof(value), &is_set);

    if (body[cursor] == '\0') {
        append_text(out, capacity, length, value);
        return 1;
    }
    if (body[cursor] == ':' && body[cursor + 1] != '\0') {
        colon = 1;
        cursor++;
    }
    op = body[cursor++];
    if ((op == '#' || op == '%') && body[cursor] == op) {
        doubled = 1;
        cursor++;
    }
    cli_copy(word, sizeof(word), body + cursor);
    if (!expand_variables(word, expanded_word, sizeof(expanded_word), cwd)) {
        return 0;
    }

    int use_word = !is_set || (colon && value[0] == '\0');
    if (op == '-') {
        append_text(out, capacity, length, use_word ? expanded_word : value);
        return 1;
    }
    if (op == '=') {
        if (use_word) {
            if (!valid_assignable_parameter(name) || setenv(name, expanded_word, 1) < 0) {
                cli_puts("sh: cannot assign parameter\n");
                return 0;
            }
            if (cli_streq(name, "PATH")) {
                set_path_list(expanded_word);
            }
            append_text(out, capacity, length, expanded_word);
        } else {
            append_text(out, capacity, length, value);
        }
        return 1;
    }
    if (op == '+') {
        if (!use_word) {
            append_text(out, capacity, length, expanded_word);
        }
        return 1;
    }
    if (op == '?') {
        if (use_word) {
            cli_puts("sh: ");
            cli_puts(name);
            cli_puts(": ");
            cli_puts(expanded_word[0] != '\0' ? expanded_word : "parameter not set");
            cli_puts("\n");
            return 0;
        }
        append_text(out, capacity, length, value);
        return 1;
    }
    if (op == '#') {
        append_trimmed_prefix(out, capacity, length, value, expanded_word, doubled);
        return 1;
    }
    if (op == '%') {
        append_trimmed_suffix(out, capacity, length, value, expanded_word, doubled);
        return 1;
    }
    cli_puts("sh: bad substitution\n");
    return 0;
}

static int expand_variables(const char *input, char *out, size_t capacity, char *cwd) {
    size_t length = 0;
    char quote = '\0';
    out[0] = '\0';
    for (size_t i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if (quote == '\'') {
            append_char(out, capacity, &length, c);
            if (c == '\'') {
                quote = '\0';
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            if (quote == c) {
                quote = '\0';
            } else if (quote == '\0') {
                quote = c;
            }
            append_char(out, capacity, &length, c);
            continue;
        }
        if (c == '\\' && input[i + 1] != '\0') {
            append_char(out, capacity, &length, c);
            append_char(out, capacity, &length, input[++i]);
            continue;
        }
        if (c != '$') {
            append_char(out, capacity, &length, c);
            continue;
        }
        if (input[i + 1] == '?') {
            append_number(out, capacity, &length, last_status);
            i++;
            continue;
        }
        if (input[i + 1] == '$') {
            append_number(out, capacity, &length, shell_pid);
            i++;
            continue;
        }
        if (input[i + 1] == '!') {
            if (last_background_pid != 0) {
                append_number(out, capacity, &length, last_background_pid);
            }
            i++;
            continue;
        }
        if (input[i + 1] == '#') {
            append_number(out, capacity, &length, shell_argc > 0 ? (uint64_t)(shell_argc - 1) : 0);
            i++;
            continue;
        }
        if (input[i + 1] == '@') {
            append_positional_all(out, capacity, &length);
            i++;
            continue;
        }
        if (input[i + 1] >= '0' && input[i + 1] <= '9') {
            append_text(out, capacity, &length, positional_parameter(input[i + 1] - '0'));
            i++;
            continue;
        }
        if (input[i + 1] == '(') {
            size_t end = find_command_substitution_end(input, i);
            if (end == 0) {
                shell_error("sh: unterminated command substitution\n");
                return 0;
            }
            if (!append_command_substitution(input + i + 2, end - i - 2, cwd, out, capacity, &length)) {
                return 0;
            }
            i = end;
            continue;
        }
        if (input[i + 1] == '{') {
            char body[ARG_EXPANDED_MAX];
            size_t body_length = 0;
            size_t cursor = i + 2;
            while (input[cursor] != '\0' && input[cursor] != '}' && body_length + 1 < sizeof(body)) {
                body[body_length++] = input[cursor++];
            }
            if (input[cursor] != '}') {
                cli_puts("sh: bad substitution\n");
                return 0;
            }
            body[body_length] = '\0';
            if (!append_parameter_expansion(body, cwd, out, capacity, &length)) {
                return 0;
            }
            i = cursor;
            continue;
        }
        if (shell_is_name_start(input[i + 1])) {
            char name[64];
            size_t name_length = 0;
            size_t cursor = i + 1;
            while (shell_is_name_char(input[cursor]) && name_length + 1 < sizeof(name)) {
                name[name_length++] = input[cursor++];
            }
            name[name_length] = '\0';
            append_text(out, capacity, &length, getenv(name));
            i = cursor - 1;
            continue;
        }
        append_char(out, capacity, &length, c);
    }
    return 1;
}

static int resolve_command(char *out, size_t capacity, const char *command) {
    if (cli_contains_slash(command)) {
        cli_copy(out, capacity, command);
        return 1;
    }
    for (size_t i = 0; i < path_count; i++) {
        if (!cli_join_path(out, capacity, path_entries[i], command)) {
            continue;
        }
        for (uint64_t index = 0;; index++) {
            char listed[CLI_PATH_MAX];
            uint64_t size = 0;
            long result = srv_list(index, listed, sizeof(listed), &size);
            if (result <= 0) {
                break;
            }
            if (cli_streq(listed, out)) {
                (void)size;
                return 1;
            }
        }
        long fd = srv_open(out);
        if (fd >= 0) {
            srv_close((int)fd);
            return 1;
        }
    }
    return 0;
}

static int glob_match(const char *pattern, const char *text) {
    if (*pattern == '\0') {
        return *text == '\0';
    }
    if (*pattern == '*') {
        while (pattern[1] == '*') {
            pattern++;
        }
        if (glob_match(pattern + 1, text)) {
            return 1;
        }
        return *text != '\0' && glob_match(pattern, text + 1);
    }
    if (*pattern == '?') {
        return *text != '\0' && glob_match(pattern + 1, text + 1);
    }
    return *pattern == *text && glob_match(pattern + 1, text + 1);
}

static int token_has_glob(const char *token) {
    for (size_t i = 0; token[i] != '\0'; i++) {
        if (token[i] == '*' || token[i] == '?') {
            return 1;
        }
    }
    return 0;
}

static void split_glob_token(const char *cwd,
    const char *token,
    char *directory,
    size_t directory_capacity,
    char *pattern,
    size_t pattern_capacity) {
    size_t slash = 0;
    int has_slash = 0;
    for (size_t i = 0; token[i] != '\0'; i++) {
        if (token[i] == '/') {
            slash = i;
            has_slash = 1;
        }
    }
    if (!has_slash) {
        cli_copy(directory, directory_capacity, cwd);
        cli_copy(pattern, pattern_capacity, token);
        return;
    }
    if (slash == 0) {
        cli_copy(directory, directory_capacity, "/");
    } else {
        size_t out = 0;
        while (out < slash && out + 1 < directory_capacity) {
            directory[out] = token[out];
            out++;
        }
        directory[out] = '\0';
    }
    char normalized[CLI_PATH_MAX];
    cli_normalize_path(normalized, sizeof(normalized), cwd, directory);
    cli_copy(directory, directory_capacity, normalized);
    cli_copy(pattern, pattern_capacity, token + slash + 1);
}

static int path_is_immediate_child(const char *path, const char *directory, const char **name_out) {
    size_t dir_length = cli_strlen(directory);
    const char *name;
    if (cli_streq(directory, "/")) {
        if (path[0] != '/' || path[1] == '\0') {
            return 0;
        }
        name = path + 1;
    } else {
        if (!cli_starts_with(path, directory) || path[dir_length] != '/' || path[dir_length + 1] == '\0') {
            return 0;
        }
        name = path + dir_length + 1;
    }
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            return 0;
        }
    }
    *name_out = name;
    return 1;
}

static int shell_path_is_directory(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.type == 1;
}

static int completion_exists(const linenoiseCompletions *completions, const char *text) {
    for (size_t i = 0; i < completions->len; i++) {
        if (completions->cvec[i] != 0 && cli_streq(completions->cvec[i], text)) {
            return 1;
        }
    }
    return 0;
}

static void add_completion_unique(linenoiseCompletions *completions, const char *text) {
    if (!completion_exists(completions, text)) {
        linenoiseAddCompletion(completions, text);
    }
}

static int token_is_command_position(const char *line, size_t token_start) {
    size_t segment_start = 0;
    char quote = '\0';
    for (size_t i = 0; i < token_start; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == ';' || c == '|' || c == '&') {
            segment_start = i + 1;
        }
    }
    while (segment_start < token_start && (line[segment_start] == ' ' || line[segment_start] == '\t')) {
        segment_start++;
    }
    return segment_start == token_start;
}

static void completion_add_replacement(linenoiseCompletions *completions,
    const char *line,
    size_t token_start,
    size_t token_end,
    const char *replacement,
    int append_space) {
    char completed[LINE_MAX];
    size_t out = 0;
    for (size_t i = 0; i < token_start && out + 1 < sizeof(completed); i++) {
        completed[out++] = line[i];
    }
    for (size_t i = 0; replacement[i] != '\0' && out + 1 < sizeof(completed); i++) {
        completed[out++] = replacement[i];
    }
    if (append_space && out + 1 < sizeof(completed)) {
        completed[out++] = ' ';
    }
    for (size_t i = token_end; line[i] != '\0' && out + 1 < sizeof(completed); i++) {
        completed[out++] = line[i];
    }
    completed[out] = '\0';
    add_completion_unique(completions, completed);
}

static void complete_path_token(const char *line,
    size_t token_start,
    size_t token_end,
    const char *token,
    linenoiseCompletions *completions) {
    char directory[CLI_PATH_MAX];
    char visible_prefix[CLI_PATH_MAX];
    char partial[CLI_PATH_MAX];
    char candidate_path[CLI_PATH_MAX];
    char replacement[CLI_PATH_MAX];
    size_t slash = 0;
    int has_slash = 0;

    for (size_t i = 0; token[i] != '\0'; i++) {
        if (token[i] == '/') {
            slash = i;
            has_slash = 1;
        }
    }

    if (!has_slash) {
        cli_copy(directory, sizeof(directory), completion_cwd);
        visible_prefix[0] = '\0';
        cli_copy(partial, sizeof(partial), token);
    } else {
        size_t out = 0;
        for (size_t i = 0; i <= slash && out + 1 < sizeof(visible_prefix); i++) {
            visible_prefix[out++] = token[i];
        }
        visible_prefix[out] = '\0';
        if (slash == 0) {
            cli_copy(directory, sizeof(directory), "/");
        } else {
            out = 0;
            for (size_t i = 0; i < slash && out + 1 < sizeof(directory); i++) {
                directory[out++] = token[i];
            }
            directory[out] = '\0';
            if (directory[0] != '/') {
                char normalized[CLI_PATH_MAX];
                cli_normalize_path(normalized, sizeof(normalized), completion_cwd, directory);
                cli_copy(directory, sizeof(directory), normalized);
            }
        }
        cli_copy(partial, sizeof(partial), token + slash + 1);
    }

    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        const char *name = 0;
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        (void)size;
        if (result <= 0) {
            break;
        }
        if (!path_is_immediate_child(listed, directory, &name) || !cli_starts_with(name, partial)) {
            continue;
        }
        cli_join_path(candidate_path, sizeof(candidate_path), directory, name);
        cli_copy(replacement, sizeof(replacement), visible_prefix);
        size_t length = cli_strlen(replacement);
        append_text(replacement, sizeof(replacement), &length, name);
        if (shell_path_is_directory(candidate_path)) {
            append_char(replacement, sizeof(replacement), &length, '/');
        }
        completion_add_replacement(completions, line, token_start, token_end, replacement,
            !shell_path_is_directory(candidate_path));
    }
}

static void complete_command_token(const char *line,
    size_t token_start,
    size_t token_end,
    const char *token,
    linenoiseCompletions *completions) {
    char replacement[CLI_PATH_MAX];
    for (size_t i = 0; i < sizeof(shell_builtins) / sizeof(shell_builtins[0]); i++) {
        if (cli_starts_with(shell_builtins[i], token)) {
            completion_add_replacement(completions, line, token_start, token_end, shell_builtins[i], 1);
        }
    }
    for (size_t i = 0; i < SHELL_MAX_ALIASES; i++) {
        if (aliases[i].name[0] != '\0' && cli_starts_with(aliases[i].name, token)) {
            completion_add_replacement(completions, line, token_start, token_end, aliases[i].name, 1);
        }
    }
    for (size_t i = 0; i < SHELL_MAX_FUNCTIONS; i++) {
        if (functions[i].name[0] != '\0' && cli_starts_with(functions[i].name, token)) {
            completion_add_replacement(completions, line, token_start, token_end, functions[i].name, 1);
        }
    }
    for (size_t path = 0; path < path_count; path++) {
        for (uint64_t index = 0;; index++) {
            char listed[CLI_PATH_MAX];
            const char *name = 0;
            uint64_t size = 0;
            long result = srv_list(index, listed, sizeof(listed), &size);
            (void)size;
            if (result <= 0) {
                break;
            }
            if (!path_is_immediate_child(listed, path_entries[path], &name) || !cli_starts_with(name, token)) {
                continue;
            }
            cli_copy(replacement, sizeof(replacement), name);
            completion_add_replacement(completions, line, token_start, token_end, replacement, 1);
        }
    }
}

static int copy_first_token(const char *line, char *out, size_t capacity) {
    size_t cursor = 0;
    size_t length = 0;
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    while (line[cursor] != '\0' && line[cursor] != ' ' && line[cursor] != '\t' && length + 1 < capacity) {
        out[length++] = line[cursor++];
    }
    out[length] = '\0';
    return length != 0;
}

static void strip_txt_suffix(char *text) {
    size_t length = cli_strlen(text);
    if (length > 4 && cli_streq(text + length - 4, ".txt")) {
        text[length - 4] = '\0';
    }
}

static void complete_help_topic(const char *line,
    size_t token_start,
    size_t token_end,
    const char *token,
    linenoiseCompletions *completions) {
    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        char topic[CLI_PATH_MAX];
        const char *name = 0;
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        (void)size;
        if (result <= 0) {
            break;
        }
        if (!path_is_immediate_child(listed, "/fat/share/help", &name)) {
            continue;
        }
        cli_copy(topic, sizeof(topic), name);
        strip_txt_suffix(topic);
        if (cli_starts_with(topic, token)) {
            completion_add_replacement(completions, line, token_start, token_end, topic, 1);
        }
    }
}

static int service_config_name_from_path(char *out, size_t capacity, const char *path) {
    const char *name = 0;
    size_t length;
    if (!path_is_immediate_child(path, "/fat/etc/services", &name)) {
        return 0;
    }
    length = cli_strlen(name);
    if (length <= 4 || !cli_streq(name + length - 4, ".svc")) {
        return 0;
    }
    cli_copy(out, capacity, name);
    out[length - 4] = '\0';
    return 1;
}

static void complete_service_token(const char *line,
    size_t token_start,
    size_t token_end,
    const char *token,
    linenoiseCompletions *completions) {
    static const char *actions[] = {
        "list", "status", "reload", "enable", "disable", "restart", "set", "unset",
        "start-enabled", "supervise", "start", "stop", "check", "check-config", "log",
        "tail", "rotate-log",
    };
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        if (cli_starts_with(actions[i], token)) {
            completion_add_replacement(completions, line, token_start, token_end, actions[i], 1);
        }
    }
    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        char name[CLI_PATH_MAX];
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        (void)size;
        if (result <= 0) {
            break;
        }
        if (service_config_name_from_path(name, sizeof(name), listed) && cli_starts_with(name, token)) {
            completion_add_replacement(completions, line, token_start, token_end, name, 1);
        }
    }
}

static void shell_completion_callback(const char *line, linenoiseCompletions *completions) {
    size_t length = cli_strlen(line);
    size_t token_start = length;
    char token[CLI_PATH_MAX];
    char first[32];
    size_t token_length = 0;
    int command_position;

    while (token_start > 0 &&
        line[token_start - 1] != ' ' &&
        line[token_start - 1] != '\t' &&
        line[token_start - 1] != ';' &&
        line[token_start - 1] != '|' &&
        line[token_start - 1] != '&') {
        token_start--;
    }
    for (size_t i = token_start; i < length && token_length + 1 < sizeof(token); i++) {
        token[token_length++] = line[i];
    }
    token[token_length] = '\0';

    command_position = token_is_command_position(line, token_start);
    if (!command_position && copy_first_token(line, first, sizeof(first)) &&
        (cli_streq(first, "help") || cli_streq(first, "man") || cli_streq(first, "apropos"))) {
        complete_help_topic(line, token_start, length, token, completions);
    } else if (!command_position && copy_first_token(line, first, sizeof(first)) && cli_streq(first, "service")) {
        complete_service_token(line, token_start, length, token, completions);
    } else if (command_position && !cli_contains_slash(token)) {
        complete_command_token(line, token_start, length, token, completions);
    } else {
        complete_path_token(line, token_start, length, token, completions);
    }
}

static int append_glob_matches(char *out, size_t capacity, size_t *out_length, const char *cwd, const char *token) {
    char directory[CLI_PATH_MAX];
    char pattern[CLI_PATH_MAX];
    int matches = 0;
    split_glob_token(cwd, token, directory, sizeof(directory), pattern, sizeof(pattern));
    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        const char *name = 0;
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        if (result <= 0) {
            break;
        }
        (void)size;
        if (!path_is_immediate_child(listed, directory, &name) || !glob_match(pattern, name)) {
            continue;
        }
        if (*out_length != 0) {
            append_char(out, capacity, out_length, ' ');
        }
        append_text(out, capacity, out_length, listed);
        matches++;
    }
    return matches;
}

static void expand_globs(const char *args, const char *cwd, char *out, size_t capacity) {
    size_t out_length = 0;
    const char *cursor = args;
    out[0] = '\0';
    while (*cursor != '\0') {
        char token[CLI_PATH_MAX];
        char raw_token[CLI_PATH_MAX];
        size_t token_length = 0;
        size_t raw_length = 0;
        int quoted = 0;
        int escaped = 0;
        char quote = '\0';
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        while (*cursor != '\0') {
            char current = *cursor;
            if (quote != '\0') {
                if (raw_length + 1 < sizeof(raw_token)) {
                    raw_token[raw_length++] = current;
                }
                if (current == quote) {
                    quote = '\0';
                    cursor++;
                    continue;
                }
                if (current == '\\' && cursor[1] != '\0') {
                    escaped = 1;
                    cursor++;
                    if (raw_length + 1 < sizeof(raw_token)) {
                        raw_token[raw_length++] = *cursor;
                    }
                    if (token_length + 1 < sizeof(token)) {
                        token[token_length++] = *cursor;
                    }
                    cursor++;
                    continue;
                }
            } else {
                if (current == ' ' || current == '\t') {
                    break;
                }
                if (raw_length + 1 < sizeof(raw_token)) {
                    raw_token[raw_length++] = current;
                }
                if (current == '\'' || current == '"') {
                    quote = current;
                    quoted = 1;
                    cursor++;
                    continue;
                }
                if (current == '\\' && cursor[1] != '\0') {
                    escaped = 1;
                    cursor++;
                    if (raw_length + 1 < sizeof(raw_token)) {
                        raw_token[raw_length++] = *cursor;
                    }
                    if (token_length + 1 < sizeof(token)) {
                        token[token_length++] = *cursor;
                    }
                    cursor++;
                    continue;
                }
            }
            if (token_length + 1 < sizeof(token)) {
                token[token_length++] = current;
            }
            cursor++;
        }
        token[token_length] = '\0';
        raw_token[raw_length] = '\0';
        if (!quoted && token_has_glob(token)) {
            size_t before = out_length;
            int matches = append_glob_matches(out, capacity, &out_length, cwd, token);
            if (matches != 0) {
                continue;
            }
            out_length = before;
        }
        if (out_length != 0) {
            append_char(out, capacity, &out_length, ' ');
        }
        append_text(out, capacity, &out_length, (quoted || escaped) ? raw_token : token);
    }
}

static int help_topic_valid(const char *topic) {
    if (topic == 0 || topic[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; topic[i] != '\0'; i++) {
        char c = topic[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            continue;
        }
        return 0;
    }
    return 1;
}

static int print_file_to_stdout(const char *path) {
    char buffer[192];
    int fd = (int)srv_open(path);
    if (fd < 0) {
        return 0;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            srv_close(fd);
            return 0;
        }
        if (count == 0) {
            break;
        }
        srv_write(SRV_STDOUT, buffer, (size_t)count);
    }
    srv_close(fd);
    return 1;
}

static void print_help(void) {
    cli_puts("builtins: help man apropos exit exec return shift set source . path cd pwd clear echo env export unset alias history type which command test [ break continue jobs wait fg bg kill service dhcp net dns rmdir read :\n");
    cli_puts("commands: ls cat more write cp rm mkdir mv tap wc grep head tail tee find du df sort uniq cut xargs seq realpath id whoami readlink cmp yes install diff tar gzip gunzip minizip miniunz patch make byacc sed expr printf tr stat chmod ps kill which env pwd true false sleep date touch mktemp basename dirname uname hostname uptime hello svscan webd httpget udpdns udpecho netstat ifconfig route arp ping host netcheck netabi sysabi spin fpdemo desktop calcgui notesgui textedit imgedit posixdemo ttydemo jsondemo inidemo linedemo sqlitedemo zlibdemo lua\n");
    cli_puts("syntax: sh [--login] [-c command|script] [args], command [args], { commands; }, name() { commands; }, if/then/else/fi, for/in/do/done, while/do/done, case/in/esac, use ;, &&, ||, append & for background\n");
    cli_puts("expansion: $VAR ${VAR} $? $$ $! $0 $1 $# $@ $(command), command-local NAME=value, unquoted * and ? globs\n");
    cli_puts("redirection: command < file, command > file, command >> file, command 2> file, command 2>> file, command 2>&1\n");
    cli_puts("pipeline: command | command [...]\n");
    cli_puts("topics: help -l, help <topic>, man <topic>, apropos <word>\n");
}

static void print_help_topics(void) {
    int printed = 0;
    cli_puts("topics:");
    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        char topic[CLI_PATH_MAX];
        const char *name = 0;
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        (void)size;
        if (result <= 0) {
            break;
        }
        if (!path_is_immediate_child(listed, "/fat/share/help", &name)) {
            continue;
        }
        cli_copy(topic, sizeof(topic), name);
        strip_txt_suffix(topic);
        cli_puts(printed ? " " : " ");
        cli_puts(topic);
        printed = 1;
    }
    if (!printed) {
        cli_puts(" none");
    }
    cli_puts("\n");
}

static void print_help_topic(const char *topic) {
    char path[CLI_PATH_MAX];
    size_t out = 0;

    if (topic == 0 || topic[0] == '\0') {
        print_help();
        return;
    }
    topic = cli_trim((char *)topic);
    if (topic[0] == '\0' || cli_is_help_arg(topic)) {
        print_help();
        return;
    }
    if (cli_streq(topic, "-l") || cli_streq(topic, "--list")) {
        print_help_topics();
        return;
    }
    if (!help_topic_valid(topic)) {
        cli_puts("help: bad topic: ");
        cli_puts(topic);
        cli_puts("\n");
        return;
    }
    path[0] = '\0';
    append_text(path, sizeof(path), &out, "/fat/share/help/");
    append_text(path, sizeof(path), &out, topic);
    append_text(path, sizeof(path), &out, ".txt");
    if (!print_file_to_stdout(path)) {
        cli_puts("help: no topic: ");
        cli_puts(topic);
        cli_puts("\n");
    }
}

static char lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int contains_casefold(const char *text, const char *needle) {
    if (needle == 0 || needle[0] == '\0') {
        return 1;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' && lower_ascii(text[i + j]) == lower_ascii(needle[j])) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

static int file_contains_casefold(const char *path, const char *needle) {
    char buffer[256];
    int fd = (int)srv_open(path);
    if (fd < 0) {
        return 0;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer) - 1);
        if (count <= 0) {
            break;
        }
        buffer[count] = '\0';
        if (contains_casefold(buffer, needle)) {
            srv_close(fd);
            return 1;
        }
    }
    srv_close(fd);
    return 0;
}

static uint64_t apropos_command(const char *args) {
    char keyword[64];
    char *argv[2];
    int argc;
    int matches = 0;

    cli_copy(keyword, sizeof(keyword), args);
    argc = split_words(keyword, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc != 1 || cli_is_help_arg(argv[0])) {
        cli_puts("usage: apropos <word>\n");
        return argc == 1 && cli_is_help_arg(argv[0]) ? 0 : 2;
    }
    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        char topic[CLI_PATH_MAX];
        const char *name = 0;
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        (void)size;
        if (result <= 0) {
            break;
        }
        if (!path_is_immediate_child(listed, "/fat/share/help", &name)) {
            continue;
        }
        cli_copy(topic, sizeof(topic), name);
        strip_txt_suffix(topic);
        if (contains_casefold(topic, argv[0]) || file_contains_casefold(listed, argv[0])) {
            cli_puts(topic);
            cli_puts("\n");
            matches++;
        }
    }
    if (!matches) {
        cli_puts("apropos: nothing appropriate for ");
        cli_puts(argv[0]);
        cli_puts("\n");
        return 1;
    }
    return 0;
}

static void print_script_context(void) {
    if (active_script_path == 0 || active_script_path[0] == '\0' || active_script_line == 0) {
        return;
    }
    cli_puts(active_script_path);
    cli_puts(":");
    cli_putn(active_script_line);
    cli_puts(": ");
}

static void shell_error(const char *message) {
    print_script_context();
    cli_puts(message);
}

static void print_ipv4(uint64_t ip) {
    cli_putn((ip >> 24) & 0xff);
    cli_puts(".");
    cli_putn((ip >> 16) & 0xff);
    cli_puts(".");
    cli_putn((ip >> 8) & 0xff);
    cli_puts(".");
    cli_putn(ip & 0xff);
}

static void dhcp_command(void) {
    long ip = srv_net_dhcp();
    if (ip < 0) {
        cli_puts("dhcp: failed\n");
        return;
    }
    cli_puts("dhcp: ");
    print_ipv4((uint64_t)ip);
    cli_puts("\n");
}

static void net_command(void) {
    if (srv_net_status() < 0) {
        cli_puts("net: failed\n");
    }
}

static void dns_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    uint32_t ip = 0;
    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        cli_puts("usage: dns <name>\n");
        return;
    }
    if (srv_net_dns(name, &ip) < 0) {
        cli_puts("dns: failed: ");
        cli_puts(name);
        cli_puts("\n");
        return;
    }
    cli_puts(name);
    cli_puts(" ");
    print_ipv4(ip);
    cli_puts("\n");
}

static void rmdir_command(const char *args) {
    char work[LINE_MAX];
    char *path;
    cli_copy(work, sizeof(work), args);
    path = cli_trim(work);
    if (path[0] == '\0') {
        cli_puts("usage: rmdir <path>\n");
        return;
    }
    if (srv_rmdir(path) < 0) {
        cli_puts("rmdir: failed: ");
        cli_puts(path);
        cli_puts("\n");
    }
}

static uint64_t parse_u64(const char *text) {
    uint64_t value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (uint64_t)(*text - '0');
        text++;
    }
    return value;
}

static uint64_t parse_job_reference(const char *text) {
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    if (text[0] == '%') {
        text++;
        if (text[0] == '+' || text[0] == '\0') {
            return current_job_id;
        }
        if (text[0] == '-') {
            return previous_job_id;
        }
    }
    return parse_u64(text);
}

static int shell_flow_requested(void) {
    return return_requested || break_requested != 0 || continue_requested != 0;
}

static int parse_i64(const char *text, int64_t *value_out) {
    int sign = 1;
    int64_t value = 0;
    if (*text == '-') {
        sign = -1;
        text++;
    }
    if (!shell_is_digit(*text)) {
        return 0;
    }
    while (shell_is_digit(*text)) {
        value = value * 10 + (int64_t)(*text - '0');
        text++;
    }
    if (*text != '\0') {
        return 0;
    }
    *value_out = value * sign;
    return 1;
}

static int split_words(char *text, char **argv, size_t capacity) {
    int argc = 0;
    char *read = text;
    char *write = text;
    while (*read != '\0') {
        char quote = '\0';
        if (*read == ' ' || *read == '\t') {
            read++;
            continue;
        }
        if ((size_t)argc >= capacity) {
            return -1;
        }
        argv[argc++] = write;
        while (*read != '\0') {
            if (quote != '\0') {
                if (*read == quote) {
                    quote = '\0';
                    read++;
                    continue;
                }
                if (*read == '\\' && read[1] != '\0') {
                    read++;
                }
                *write++ = *read++;
                continue;
            }
            if (*read == '\'' || *read == '"') {
                quote = *read++;
                continue;
            }
            if (*read == '\\' && read[1] != '\0') {
                read++;
                *write++ = *read++;
                continue;
            }
            if (*read == ' ' || *read == '\t') {
                break;
            }
            *write++ = *read++;
        }
        if (*read == ' ' || *read == '\t') {
            read++;
        }
        *write++ = '\0';
    }
    return argc;
}

static int history_size_from_env(void) {
    const char *text = getenv("HISTSIZE");
    int64_t parsed = 64;
    if (text != 0 && text[0] != '\0' && (!parse_i64(text, &parsed) || parsed < 1)) {
        parsed = 64;
    }
    if (parsed > 512) {
        parsed = 512;
    }
    return (int)parsed;
}

static const char *history_file_from_env(void) {
    const char *file = getenv("HISTFILE");
    return file != 0 && file[0] != '\0' ? file : "/fat/.srvsh_history";
}

static void configure_history(void) {
    linenoiseHistorySetMaxLen(history_size_from_env());
}

static void save_history(void) {
    const char *file = history_file_from_env();
    if (file[0] != '\0') {
        linenoiseHistorySave(file);
    }
}

static uint64_t history_command(const char *args) {
    char work[LINE_MAX];
    char *argv[3];
    int argc;
    size_t start = 0;
    size_t count = linenoiseHistoryLen();

    cli_copy(work, sizeof(work), args);
    argc = split_words(work, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0) {
        cli_puts("usage: history [-c|-r [file]|-w [file]|count]\n");
        return 2;
    }
    if (argc > 0 && cli_streq(argv[0], "-c")) {
        if (argc != 1) {
            cli_puts("usage: history -c\n");
            return 2;
        }
        linenoiseHistoryClear();
        configure_history();
        save_history();
        return 0;
    }
    if (argc > 0 && (cli_streq(argv[0], "-r") || cli_streq(argv[0], "-w"))) {
        const char *file = argc > 1 ? argv[1] : history_file_from_env();
        if (argc > 2) {
            cli_puts(cli_streq(argv[0], "-r") ? "usage: history -r [file]\n" : "usage: history -w [file]\n");
            return 2;
        }
        if (cli_streq(argv[0], "-r")) {
            configure_history();
            if (linenoiseHistoryLoad(file) != 0) {
                cli_puts("history: read failed: ");
                cli_puts(file);
                cli_puts("\n");
                return 1;
            }
        } else if (linenoiseHistorySave(file) != 0) {
            cli_puts("history: write failed: ");
            cli_puts(file);
            cli_puts("\n");
            return 1;
        }
        return 0;
    }
    if (argc > 0) {
        int64_t parsed = 0;
        if (argc != 1 || !parse_i64(argv[0], &parsed) || parsed < 0) {
            cli_puts("usage: history [-c|-r [file]|-w [file]|count]\n");
            return 2;
        }
        if ((uint64_t)parsed < count) {
            start = count - (size_t)parsed;
        }
    }
    for (size_t i = start; i < count; i++) {
        const char *line = linenoiseHistoryGet(i);
        cli_putn(i + 1);
        cli_puts("  ");
        cli_puts(line != 0 ? line : "");
        cli_puts("\n");
    }
    return 0;
}

static int path_type_matches(const char *path, int expected_type) {
    struct srv_stat info;
    if (srv_stat(path, &info) < 0) {
        return 0;
    }
    return expected_type < 0 || (int)info.type == expected_type;
}

static int path_size_nonzero(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.size > 0;
}

static int path_mode_has(const char *path, uint64_t bits) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && (info.mode & bits) != 0;
}

static int path_stat(const char *path, struct srv_stat *info) {
    return path != 0 && srv_stat(path, info) == 0;
}

static uint64_t test_eval_simple(int argc, char **argv) {
    if (argc == 0) {
        return 1;
    }
    if (argc == 1) {
        return argv[0][0] != '\0' ? 0 : 1;
    }
    if (argc == 2) {
        if (cli_streq(argv[0], "!")) {
            return test_eval_simple(1, argv + 1) == 0 ? 1 : 0;
        }
        if (cli_streq(argv[0], "-n")) {
            return argv[1][0] != '\0' ? 0 : 1;
        }
        if (cli_streq(argv[0], "-z")) {
            return argv[1][0] == '\0' ? 0 : 1;
        }
        if (cli_streq(argv[0], "-a") || cli_streq(argv[0], "-e")) {
            return path_type_matches(argv[1], -1) ? 0 : 1;
        }
        if (cli_streq(argv[0], "-f")) {
            return path_type_matches(argv[1], 0) ? 0 : 1;
        }
        if (cli_streq(argv[0], "-d")) {
            return path_type_matches(argv[1], 1) ? 0 : 1;
        }
        if (cli_streq(argv[0], "-s")) {
            return path_size_nonzero(argv[1]) ? 0 : 1;
        }
        if (cli_streq(argv[0], "-r")) {
            return path_mode_has(argv[1], 0444) ? 0 : 1;
        }
        if (cli_streq(argv[0], "-w")) {
            return path_mode_has(argv[1], 0222) ? 0 : 1;
        }
        if (cli_streq(argv[0], "-x")) {
            return path_mode_has(argv[1], 0111) ? 0 : 1;
        }
        return 2;
    }
    if (argc == 3) {
        if (cli_streq(argv[1], "=")) {
            return cli_streq(argv[0], argv[2]) ? 0 : 1;
        }
        if (cli_streq(argv[1], "==")) {
            return cli_streq(argv[0], argv[2]) ? 0 : 1;
        }
        if (cli_streq(argv[1], "!=")) {
            return !cli_streq(argv[0], argv[2]) ? 0 : 1;
        }
        if (cli_streq(argv[1], "<")) {
            return strcmp(argv[0], argv[2]) < 0 ? 0 : 1;
        }
        if (cli_streq(argv[1], ">")) {
            return strcmp(argv[0], argv[2]) > 0 ? 0 : 1;
        }
        if (cli_streq(argv[1], "-nt") || cli_streq(argv[1], "-ot") || cli_streq(argv[1], "-ef")) {
            struct srv_stat left_info;
            struct srv_stat right_info;
            int left_exists = path_stat(argv[0], &left_info);
            int right_exists = path_stat(argv[2], &right_info);
            if (cli_streq(argv[1], "-nt")) {
                return left_exists && (!right_exists || left_info.mtime > right_info.mtime) ? 0 : 1;
            }
            if (cli_streq(argv[1], "-ot")) {
                return right_exists && (!left_exists || left_info.mtime < right_info.mtime) ? 0 : 1;
            }
            return left_exists && right_exists && left_info.inode == right_info.inode ? 0 : 1;
        }
        if (argv[1][0] == '-' && argv[1][1] != '\0') {
            int64_t left = 0;
            int64_t right = 0;
            if (!parse_i64(argv[0], &left) || !parse_i64(argv[2], &right)) {
                return 2;
            }
            if (cli_streq(argv[1], "-eq")) {
                return left == right ? 0 : 1;
            }
            if (cli_streq(argv[1], "-ne")) {
                return left != right ? 0 : 1;
            }
            if (cli_streq(argv[1], "-gt")) {
                return left > right ? 0 : 1;
            }
            if (cli_streq(argv[1], "-ge")) {
                return left >= right ? 0 : 1;
            }
            if (cli_streq(argv[1], "-lt")) {
                return left < right ? 0 : 1;
            }
            if (cli_streq(argv[1], "-le")) {
                return left <= right ? 0 : 1;
            }
        }
    }
    return 2;
}

static uint64_t test_eval_range(int start, int end, char **argv) {
    int depth;
    if (start >= end) {
        return 1;
    }
    if (cli_streq(argv[start], "!")) {
        uint64_t status = test_eval_range(start + 1, end, argv);
        return status == 2 ? 2 : (status == 0 ? 1 : 0);
    }
    while (end - start >= 2 && cli_streq(argv[start], "(") && cli_streq(argv[end - 1], ")")) {
        depth = 0;
        int wraps = 1;
        for (int i = start; i < end; i++) {
            if (cli_streq(argv[i], "(")) {
                depth++;
            } else if (cli_streq(argv[i], ")")) {
                depth--;
                if (depth == 0 && i != end - 1) {
                    wraps = 0;
                    break;
                }
                if (depth < 0) {
                    return 2;
                }
            }
        }
        if (depth != 0) {
            return 2;
        }
        if (!wraps) {
            break;
        }
        start++;
        end--;
    }
    depth = 0;
    for (int i = end - 1; i >= start; i--) {
        if (cli_streq(argv[i], ")")) {
            depth++;
        } else if (cli_streq(argv[i], "(")) {
            depth--;
            if (depth < 0) {
                return 2;
            }
        } else if (depth == 0 && cli_streq(argv[i], "-o")) {
            uint64_t left = test_eval_range(start, i, argv);
            uint64_t right = test_eval_range(i + 1, end, argv);
            if (left == 2 || right == 2) {
                return 2;
            }
            return left == 0 || right == 0 ? 0 : 1;
        }
    }
    if (depth != 0) {
        return 2;
    }
    depth = 0;
    for (int i = end - 1; i >= start; i--) {
        if (cli_streq(argv[i], ")")) {
            depth++;
        } else if (cli_streq(argv[i], "(")) {
            depth--;
            if (depth < 0) {
                return 2;
            }
        } else if (depth == 0 && cli_streq(argv[i], "-a")) {
            uint64_t left = test_eval_range(start, i, argv);
            uint64_t right = test_eval_range(i + 1, end, argv);
            if (left == 2 || right == 2) {
                return 2;
            }
            return left == 0 && right == 0 ? 0 : 1;
        }
    }
    return test_eval_simple(end - start, argv + start);
}

static uint64_t test_eval(int argc, char **argv) {
    return test_eval_range(0, argc, argv);
}

static uint64_t test_command(const char *args, int bracket_form) {
    char work[ARG_EXPANDED_MAX];
    char *argv[32];
    int argc;
    cli_copy(work, sizeof(work), args);
    argc = split_words(work, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0) {
        cli_puts("test: too many arguments\n");
        return 2;
    }
    if (bracket_form) {
        if (argc == 0) {
            cli_puts("[: missing expression\n");
            return 2;
        }
        if (!cli_streq(argv[argc - 1], "]")) {
            cli_puts("[: missing ]\n");
            return 2;
        }
        argc--;
    }
    uint64_t status = test_eval(argc, argv);
    if (status == 2) {
        cli_puts(bracket_form ? "[: unsupported expression\n" : "test: unsupported expression\n");
    }
    return status;
}

static void remember_current_job(uint64_t id) {
    if (id == 0 || id == current_job_id) {
        return;
    }
    previous_job_id = current_job_id;
    current_job_id = id;
}

static void forget_job_id(uint64_t id) {
    if (id == 0) {
        return;
    }
    if (current_job_id == id) {
        current_job_id = previous_job_id;
        previous_job_id = 0;
    } else if (previous_job_id == id) {
        previous_job_id = 0;
    }
}

static void print_jobs(const char *args) {
    uint64_t index = 0;
    struct srv_process_info info;
    char options[32];
    int long_format;
    cli_copy(options, sizeof(options), args != 0 ? args : "");
    long_format = cli_streq(cli_trim(options), "-l");
    for (size_t i = 0; i < SHELL_MAX_JOBS; i++) {
        if (!jobs[i].used) {
            continue;
        }
        cli_puts("[");
        cli_putn(jobs[i].id);
        cli_puts("] group ");
        cli_putn(jobs[i].group);
        cli_puts(" ");
        cli_puts(jobs[i].command);
        cli_puts("\n");
        if (long_format) {
            for (size_t j = 0; j < jobs[i].count; j++) {
                if (jobs[i].pids[j] < 0) {
                    continue;
                }
                cli_puts("  pid ");
                cli_putn((uint64_t)jobs[i].pids[j]);
                cli_puts("\n");
            }
        }
    }
    cli_puts("PID STATE      NAME\n");
    for (;;) {
        long next = srv_proc_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (!cli_streq(info.state, "foreground")) {
            cli_putn(info.pid);
            cli_puts("   ");
            cli_puts(info.state);
            cli_puts(" ");
            cli_puts(info.name);
            cli_puts("\n");
        }
        index = (uint64_t)next;
    }
}

static struct shell_job *find_job(uint64_t id_or_pid) {
    if (id_or_pid == 0) {
        return 0;
    }
    for (size_t i = 0; i < SHELL_MAX_JOBS; i++) {
        if (!jobs[i].used) {
            continue;
        }
        if (jobs[i].id == id_or_pid || jobs[i].group == id_or_pid) {
            return &jobs[i];
        }
        for (size_t j = 0; j < jobs[i].count; j++) {
            if (jobs[i].pids[j] >= 0 && (uint64_t)jobs[i].pids[j] == id_or_pid) {
                return &jobs[i];
            }
        }
    }
    return 0;
}

static struct shell_job *add_job(uint64_t group, const long *pids, size_t count, const char *command) {
    if (group == 0 || pids == 0 || count == 0) {
        return 0;
    }
    for (size_t i = 0; i < SHELL_MAX_JOBS; i++) {
        if (!jobs[i].used) {
            jobs[i].used = 1;
            jobs[i].id = next_job_id++;
            jobs[i].group = group;
            jobs[i].count = count;
            for (size_t j = 0; j < PIPELINE_MAX_COMMANDS; j++) {
                jobs[i].pids[j] = j < count ? pids[j] : -1;
            }
            cli_copy(jobs[i].command, sizeof(jobs[i].command), command != 0 ? command : "");
            remember_current_job(jobs[i].id);
            return &jobs[i];
        }
    }
    return 0;
}

static uint64_t wait_job_pids(long *pids, size_t count) {
    uint64_t status = 0;
    wait_pipeline_pids(pids, count, &status);
    return status;
}

static uint64_t wait_shell_job(struct shell_job *job, int foreground) {
    uint64_t status;
    if (job == 0) {
        return 127;
    }
    if (foreground) {
        (void)srv_proc_group(0, job->group, 1);
    }
    status = wait_job_pids(job->pids, job->count);
    if (foreground) {
        (void)srv_proc_group(0, 0, 1);
    }
    forget_job_id(job->id);
    job->used = 0;
    return status;
}

static int find_process_by_pid(uint64_t pid, struct srv_process_info *out) {
    uint64_t index = 0;
    struct srv_process_info info;
    if (pid == 0) {
        return 0;
    }
    for (;;) {
        long next = srv_proc_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (info.pid == pid) {
            if (out != 0) {
                *out = info;
            }
            return 1;
        }
        index = (uint64_t)next;
    }
    return 0;
}

static void print_wait_result(const char *prefix, long waited, uint64_t status) {
    cli_puts(prefix);
    cli_puts(" pid ");
    cli_putn((uint64_t)waited);
    cli_puts(" status ");
    cli_putn(status);
    cli_puts("\n");
}

static uint64_t wait_for_job(const char *args) {
    uint64_t status = 0;
    uint64_t pid = 0;
    long waited;
    struct shell_job *job;

    if (args != 0 && args[0] != '\0') {
        pid = parse_job_reference(args);
        if (pid == 0) {
            cli_puts("usage: wait [pid]\n");
            return 2;
        }
        job = find_job(pid);
        if (job != 0) {
            status = wait_shell_job(job, 0);
            print_wait_result("[done]", (long)job->group, status);
            return status;
        }
    } else {
        int waited_any = 0;
        for (;;) {
            job = 0;
            for (size_t i = 0; i < SHELL_MAX_JOBS; i++) {
                if (jobs[i].used) {
                    job = &jobs[i];
                    break;
                }
            }
            if (job == 0) {
                break;
            }
            uint64_t group = job->group;
            status = wait_shell_job(job, 0);
            print_wait_result("[done]", (long)group, status);
            waited_any = 1;
        }
        if (waited_any) {
            return status;
        }
        return 0;
    }

    waited = srv_wait(pid, &status, 0);
    if (waited < 0) {
        cli_puts("wait: no matching background process\n");
        return 127;
    }
    print_wait_result("[done]", waited, status);
    return status;
}

static uint64_t fg_command(const char *args) {
    uint64_t pid = args != 0 && args[0] != '\0' ? parse_job_reference(args) : last_background_pid;
    uint64_t status = 0;
    long waited;
    struct srv_process_info info;
    struct shell_job *job;

    if (pid == 0) {
        cli_puts("fg: no current background job\n");
        return 1;
    }
    job = find_job(pid);
    if (job != 0) {
        cli_puts("[fg] pid ");
        cli_putn(job->group);
        cli_puts(" ");
        cli_puts(job->command);
        cli_puts("\n");
        status = wait_shell_job(job, 1);
        print_wait_result("[done]", (long)job->group, status);
        return status;
    }
    if (!find_process_by_pid(pid, &info)) {
        cli_puts("fg: no such job\n");
        return 127;
    }
    if (cli_streq(info.state, "foreground")) {
        cli_puts("fg: process is already foreground\n");
        return 1;
    }
    (void)srv_proc_group(pid, pid, 1);
    cli_puts("[fg] pid ");
    cli_putn(pid);
    cli_puts(" ");
    cli_puts(info.name);
    cli_puts("\n");
    waited = srv_wait(pid, &status, 0);
    if (waited < 0) {
        cli_puts("fg: wait failed\n");
        (void)srv_proc_group(0, 0, 1);
        return 127;
    }
    (void)srv_proc_group(0, 0, 1);
    print_wait_result("[done]", waited, status);
    return status;
}

static uint64_t bg_command(const char *args) {
    uint64_t pid = args != 0 && args[0] != '\0' ? parse_job_reference(args) : last_background_pid;
    struct srv_process_info info;
    struct shell_job *job;

    if (pid == 0) {
        cli_puts("bg: no current background job\n");
        return 1;
    }
    job = find_job(pid);
    if (job != 0) {
        cli_puts("[bg] pid ");
        cli_putn(job->group);
        cli_puts(" background ");
        cli_puts(job->command);
        cli_puts("\n");
        return 0;
    }
    if (!find_process_by_pid(pid, &info)) {
        cli_puts("bg: no such job\n");
        return 127;
    }
    if (cli_streq(info.state, "foreground")) {
        cli_puts("bg: process is foreground\n");
        return 1;
    }
    cli_puts("[bg] pid ");
    cli_putn(pid);
    cli_puts(" ");
    cli_puts(info.state);
    cli_puts(" ");
    cli_puts(info.name);
    cli_puts("\n");
    return 0;
}

static uint64_t kill_command(const char *args) {
    char work[ARG_EXPANDED_MAX];
    char *argv[8];
    int argc;
    uint64_t failures = 0;

    cli_copy(work, sizeof(work), args);
    argc = split_words(work, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0) {
        cli_puts("kill: too many arguments\n");
        return 2;
    }
    if (argc == 0) {
        cli_puts("usage: kill <pid|%job> [...]\n");
        return 2;
    }
    if (argc == 1 && cli_is_help_arg(argv[0])) {
        cli_puts("usage: kill <pid|%job> [...]\n");
        return 0;
    }

    for (int i = 0; i < argc; i++) {
        uint64_t target = parse_job_reference(argv[i]);
        struct shell_job *job = argv[i][0] == '%' ? find_job(target) : 0;
        if (target == 0) {
            cli_puts("kill: invalid target: ");
            cli_puts(argv[i]);
            cli_puts("\n");
            failures++;
            continue;
        }
        if (job != 0) {
            for (size_t j = 0; j < job->count; j++) {
                if (job->pids[j] >= 0 && srv_kill((uint64_t)job->pids[j]) < 0) {
                    failures++;
                }
            }
            continue;
        }
        if (srv_kill(target) < 0) {
            cli_puts("kill: no such pid: ");
            cli_putn(target);
            cli_puts("\n");
            failures++;
        }
    }
    return failures == 0 ? 0 : 1;
}

static int process_name_matches(const struct srv_process_info *info, const char *name) {
    char base[CLI_PATH_MAX];
    cli_basename(base, sizeof(base), info->name);
    return cli_streq(info->name, name) || cli_streq(base, name);
}

static int find_service_process(const char *name, struct srv_process_info *out, int include_exited) {
    uint64_t index = 0;
    struct srv_process_info info;
    for (;;) {
        long next = srv_proc_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (process_name_matches(&info, name) &&
            (include_exited || !cli_streq(info.state, "exited"))) {
            *out = info;
            return 1;
        }
        index = (uint64_t)next;
    }
    return 0;
}

struct service_config {
    char name[32];
    char command[CLI_PATH_MAX];
    char args[LINE_MAX];
    char process_name[32];
    char log_path[CLI_PATH_MAX];
    char requires[32];
    char health[32];
    int enabled;
    char restart[16];
    uint64_t max_log_size;
};

static const char *service_reload_path = "/fat/run/svscan.reload";

static void service_default_config(struct service_config *config, const char *name) {
    size_t out = 0;
    memset(config, 0, sizeof(*config));
    cli_copy(config->name, sizeof(config->name), name);
    cli_copy(config->process_name, sizeof(config->process_name), name);
    cli_copy(config->restart, sizeof(config->restart), "never");
    config->command[0] = '\0';
    append_text(config->command, sizeof(config->command), &out, "/fat/bin/");
    append_text(config->command, sizeof(config->command), &out, name);
    if (cli_streq(name, "webd")) {
        cli_copy(config->command, sizeof(config->command), "/fat/bin/webd");
        cli_copy(config->args, sizeof(config->args), "/fat/www");
        cli_copy(config->log_path, sizeof(config->log_path), "/fat/var/log/webd.log");
        config->enabled = 1;
    }
}

static void service_config_path(char *out, size_t capacity, const char *name) {
    size_t length = 0;
    out[0] = '\0';
    append_text(out, capacity, &length, "/fat/etc/services/");
    append_text(out, capacity, &length, name);
    append_text(out, capacity, &length, ".svc");
}

static void service_apply_config_line(struct service_config *config, char *line) {
    char *trimmed = cli_trim(line);
    char *equals;
    if (trimmed[0] == '\0' || trimmed[0] == '#') {
        return;
    }
    equals = strchr(trimmed, '=');
    if (equals == 0 || equals == trimmed) {
        return;
    }
    *equals++ = '\0';
    char *key = cli_trim(trimmed);
    char *value = cli_trim(equals);
    if (cli_streq(key, "command") || cli_streq(key, "path")) {
        cli_copy(config->command, sizeof(config->command), value);
    } else if (cli_streq(key, "args")) {
        cli_copy(config->args, sizeof(config->args), value);
    } else if (cli_streq(key, "process") || cli_streq(key, "name")) {
        cli_copy(config->process_name, sizeof(config->process_name), value);
    } else if (cli_streq(key, "log") || cli_streq(key, "stdout")) {
        cli_copy(config->log_path, sizeof(config->log_path), value);
    } else if (cli_streq(key, "requires") || cli_streq(key, "after")) {
        cli_copy(config->requires, sizeof(config->requires), value);
    } else if (cli_streq(key, "health")) {
        cli_copy(config->health, sizeof(config->health), value);
    } else if (cli_streq(key, "max_log") || cli_streq(key, "log_max")) {
        config->max_log_size = parse_u64(value);
    } else if (cli_streq(key, "enabled")) {
        config->enabled =
            cli_streq(value, "1") ||
            cli_streq(value, "yes") ||
            cli_streq(value, "true") ||
            cli_streq(value, "on");
    } else if (cli_streq(key, "restart")) {
        cli_copy(config->restart, sizeof(config->restart), value);
    }
}

static int service_load_config(struct service_config *config, const char *name) {
    char path[CLI_PATH_MAX];
    char data[512];
    char line[LINE_MAX];
    size_t line_length = 0;
    long fd;
    long count;

    service_default_config(config, name);
    service_config_path(path, sizeof(path), name);
    fd = srv_open(path);
    if (fd < 0) {
        long command_fd = srv_open(config->command);
        if (command_fd >= 0) {
            srv_close((int)command_fd);
            return 1;
        }
        return cli_streq(name, "webd") ? 1 : 0;
    }
    count = srv_read((int)fd, data, sizeof(data) - 1);
    srv_close((int)fd);
    if (count < 0) {
        return 0;
    }
    data[count] = '\0';
    for (long i = 0; i <= count; i++) {
        char c = data[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n' || c == '\0') {
            line[line_length] = '\0';
            service_apply_config_line(config, line);
            line_length = 0;
            continue;
        }
        if (line_length + 1 < sizeof(line)) {
            line[line_length++] = c;
        }
    }
    if (config->command[0] == '\0' || config->process_name[0] == '\0') {
        return 0;
    }
    return 1;
}

static int service_append_config_line(char *out, size_t capacity, size_t *length, const char *key, const char *value) {
    if (*length + cli_strlen(key) + cli_strlen(value) + 2 >= capacity) {
        return 0;
    }
    append_text(out, capacity, length, key);
    append_text(out, capacity, length, "=");
    append_text(out, capacity, length, value);
    append_text(out, capacity, length, "\n");
    return 1;
}

static int service_save_config(const struct service_config *config) {
    char path[CLI_PATH_MAX];
    char data[512];
    size_t length = 0;
    service_config_path(path, sizeof(path), config->name);
    data[0] = '\0';
    if (!service_append_config_line(data, sizeof(data), &length, "command", config->command) ||
        !service_append_config_line(data, sizeof(data), &length, "args", config->args) ||
        !service_append_config_line(data, sizeof(data), &length, "process", config->process_name) ||
        !service_append_config_line(data, sizeof(data), &length, "log", config->log_path) ||
        !service_append_config_line(data, sizeof(data), &length, "requires", config->requires) ||
        !service_append_config_line(data, sizeof(data), &length, "health", config->health) ||
        !service_append_config_line(data, sizeof(data), &length, "enabled", config->enabled ? "true" : "false") ||
        !service_append_config_line(data, sizeof(data), &length, "restart", config->restart)) {
        cli_puts("service: config too large\n");
        return 0;
    }
    if (config->max_log_size != 0) {
        char value[32];
        size_t used = 0;
        value[0] = '\0';
        append_number(value, sizeof(value), &used, config->max_log_size);
        if (!service_append_config_line(data, sizeof(data), &length, "max_log", value)) {
            cli_puts("service: config too large\n");
            return 0;
        }
    }
    if (srv_fs_write(path, data, length) < 0) {
        cli_puts("service: config write failed: ");
        cli_puts(path);
        cli_puts("\n");
        return 0;
    }
    return 1;
}

static int service_request_reload(void) {
    (void)srv_mkdir("/fat/run");
    if (srv_fs_write(service_reload_path, "reload\n", 7) < 0) {
        cli_puts("service: reload request failed\n");
        return 0;
    }
    cli_puts("service: reload requested\n");
    return 1;
}

static void service_set_enabled(struct service_config *config, int enabled) {
    config->enabled = enabled;
    if (!service_save_config(config)) {
        return;
    }
    cli_puts(config->name);
    cli_puts(enabled ? " enabled\n" : " disabled\n");
    (void)service_request_reload();
}

static int service_name_from_path(char *out, size_t capacity, const char *path) {
    const char *name;
    size_t length;
    if (!path_is_immediate_child(path, "/fat/etc/services", &name)) {
        return 0;
    }
    length = cli_strlen(name);
    if (length <= 4 ||
        name[length - 4] != '.' ||
        name[length - 3] != 's' ||
        name[length - 2] != 'v' ||
        name[length - 1] != 'c' ||
        length - 4 >= capacity) {
        return 0;
    }
    for (size_t i = 0; i < length - 4; i++) {
        out[i] = name[i];
    }
    out[length - 4] = '\0';
    return 1;
}

static void reap_exited_service(const char *name) {
    struct srv_process_info info;
    uint64_t status = 0;
    while (find_service_process(name, &info, 1) && cli_streq(info.state, "exited")) {
        if (srv_wait(info.pid, &status, SRV_WAIT_NOHANG) <= 0) {
            return;
        }
    }
}

static int service_stop_config(const struct service_config *config) {
    struct srv_process_info info;
    uint64_t status = 0;
    if (!find_service_process(config->process_name, &info, 0)) {
        cli_puts(config->name);
        cli_puts(" stopped\n");
        return 0;
    }
    if (srv_kill(info.pid) < 0) {
        cli_puts(config->name);
        cli_puts(" stop failed\n");
        return 1;
    }
    long waited = srv_wait(info.pid, &status, 0);
    cli_puts(config->name);
    cli_puts(" stopped pid ");
    cli_putn(info.pid);
    if (waited > 0) {
        cli_puts(" status ");
        cli_putn(status);
    }
    cli_puts("\n");
    return 0;
}

static void service_print_status(const struct service_config *config) {
    struct srv_process_info info;
    if (find_service_process(config->process_name, &info, 1)) {
        cli_puts(config->name);
        cli_puts(" ");
        cli_puts(info.state);
        cli_puts(" pid ");
        cli_putn(info.pid);
    } else {
        cli_puts(config->name);
        cli_puts(" stopped");
    }
    cli_puts(" enabled=");
    cli_puts(config->enabled ? "true" : "false");
    cli_puts(" restart=");
    cli_puts(config->restart);
    if (config->requires[0] != '\0') {
        cli_puts(" requires=");
        cli_puts(config->requires);
    }
    if (config->health[0] != '\0') {
        cli_puts(" health=");
        cli_puts(config->health);
    }
    if (config->max_log_size != 0) {
        cli_puts(" max_log=");
        cli_putn(config->max_log_size);
    }
    if (config->log_path[0] != '\0') {
        cli_puts(" log ");
        cli_puts(config->log_path);
    }
    cli_puts("\n");
}

static int service_parse_listen_health(const char *health, uint16_t *port_out) {
    const char *prefix = "listen:";
    uint64_t port;
    for (size_t i = 0; prefix[i] != '\0'; i++) {
        if (health[i] != prefix[i]) {
            return 0;
        }
    }
    if (health[7] == '\0') {
        return 0;
    }
    for (const char *cursor = health + 7; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
    }
    port = parse_u64(health + 7);
    if (port == 0 || port > 65535) {
        return 0;
    }
    *port_out = (uint16_t)port;
    return 1;
}

static int service_dependency_declared_valid(const char *requires) {
    if (requires[0] == '\0' || cli_streq(requires, "network")) {
        return 1;
    }
    for (const char *cursor = requires; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == ' ' || *cursor == '\t') {
            return 0;
        }
    }
    return 1;
}

static int service_log_parent_path(char *out, size_t capacity, const char *path) {
    size_t length = cli_strlen(path);
    if (path[0] == '\0' || length >= capacity) {
        return 0;
    }
    cli_copy(out, capacity, path);
    while (length > 0 && out[length - 1] != '/') {
        length--;
    }
    if (length == 0) {
        return 0;
    }
    if (length == 1) {
        out[1] = '\0';
    } else {
        out[length - 1] = '\0';
    }
    return 1;
}

static int service_check_config_file(const struct service_config *config) {
    int errors = 0;
    struct srv_stat stat;
    char parent[CLI_PATH_MAX];
    uint16_t port = 0;

    cli_puts(config->name);
    cli_puts(" config ");

    if (srv_stat(config->command, &stat) < 0) {
        if (errors == 0) {
            cli_puts("bad");
        }
        cli_puts(" command-missing=");
        cli_puts(config->command);
        errors++;
    }
    if (config->log_path[0] != '\0') {
        if (!service_log_parent_path(parent, sizeof(parent), config->log_path) ||
            srv_stat(parent, &stat) < 0) {
            if (errors == 0) {
                cli_puts("bad");
            }
            cli_puts(" log-dir-missing=");
            cli_puts(config->log_path);
            errors++;
        }
    }
    if (config->requires[0] != '\0' && !service_dependency_declared_valid(config->requires)) {
        if (errors == 0) {
            cli_puts("bad");
        }
        cli_puts(" bad-requires=");
        cli_puts(config->requires);
        errors++;
    }
    if (config->health[0] != '\0' && !service_parse_listen_health(config->health, &port)) {
        if (errors == 0) {
            cli_puts("bad");
        }
        cli_puts(" bad-health=");
        cli_puts(config->health);
        errors++;
    }
    if (!cli_streq(config->restart, "always") && !cli_streq(config->restart, "never")) {
        if (errors == 0) {
            cli_puts("bad");
        }
        cli_puts(" bad-restart=");
        cli_puts(config->restart);
        errors++;
    }
    if (errors == 0) {
        cli_puts("ok");
    }
    cli_puts("\n");
    return errors == 0 ? 0 : 1;
}

static int service_set_config_field(struct service_config *config, const char *key, const char *value) {
    if (cli_streq(key, "command") || cli_streq(key, "path")) {
        cli_copy(config->command, sizeof(config->command), value);
    } else if (cli_streq(key, "args")) {
        cli_copy(config->args, sizeof(config->args), value);
    } else if (cli_streq(key, "process") || cli_streq(key, "name")) {
        cli_copy(config->process_name, sizeof(config->process_name), value);
    } else if (cli_streq(key, "log") || cli_streq(key, "stdout")) {
        cli_copy(config->log_path, sizeof(config->log_path), value);
    } else if (cli_streq(key, "requires") || cli_streq(key, "after")) {
        cli_copy(config->requires, sizeof(config->requires), value);
    } else if (cli_streq(key, "health")) {
        cli_copy(config->health, sizeof(config->health), value);
    } else if (cli_streq(key, "max_log") || cli_streq(key, "log_max")) {
        config->max_log_size = parse_u64(value);
    } else if (cli_streq(key, "enabled")) {
        config->enabled =
            cli_streq(value, "1") ||
            cli_streq(value, "yes") ||
            cli_streq(value, "true") ||
            cli_streq(value, "on");
    } else if (cli_streq(key, "restart")) {
        cli_copy(config->restart, sizeof(config->restart), value);
    } else {
        return 0;
    }
    return 1;
}

static int service_unset_config_field(struct service_config *config, const char *key) {
    if (cli_streq(key, "args")) {
        config->args[0] = '\0';
    } else if (cli_streq(key, "log") || cli_streq(key, "stdout")) {
        config->log_path[0] = '\0';
    } else if (cli_streq(key, "requires") || cli_streq(key, "after")) {
        config->requires[0] = '\0';
    } else if (cli_streq(key, "health")) {
        config->health[0] = '\0';
    } else if (cli_streq(key, "max_log") || cli_streq(key, "log_max")) {
        config->max_log_size = 0;
    } else {
        return 0;
    }
    return 1;
}

static int service_health_ok(const struct service_config *config, const struct srv_process_info *process) {
    uint16_t port = 0;
    struct srv_net_info info;
    if (config->health[0] == '\0') {
        return 1;
    }
    if (!service_parse_listen_health(config->health, &port)) {
        return 1;
    }
    for (uint64_t index = 0;;) {
        long next = srv_net_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (info.kind == SRV_NET_KIND_TCP_LISTENER &&
            info.local_port == port &&
            (process == 0 || info.pid == process->pid)) {
            return 1;
        }
        index = (uint64_t)next;
    }
    return 0;
}

static void service_check_config(const struct service_config *config) {
    struct srv_process_info info;
    if (!find_service_process(config->process_name, &info, 0)) {
        cli_puts(config->name);
        cli_puts(" check stopped\n");
        return;
    }
    cli_puts(config->name);
    cli_puts(service_health_ok(config, &info) ? " check ok pid " : " check failed pid ");
    cli_putn(info.pid);
    cli_puts("\n");
}

static int service_wait_healthy(const struct service_config *config, uint64_t timeout_ticks) {
    uint64_t start = srv_ticks();
    struct srv_process_info info;
    for (;;) {
        reap_exited_service(config->process_name);
        if (find_service_process(config->process_name, &info, 0) &&
            service_health_ok(config, &info)) {
            cli_puts(config->name);
            cli_puts(" healthy pid ");
            cli_putn(info.pid);
            cli_puts("\n");
            return 1;
        }
        if (srv_ticks() - start >= timeout_ticks) {
            cli_puts(config->name);
            cli_puts(" wait timeout\n");
            return 0;
        }
        srv_sleep_ticks(10);
    }
}

static void service_rotated_path(char *out, size_t capacity, const char *path) {
    size_t length = 0;
    out[0] = '\0';
    append_text(out, capacity, &length, path);
    append_text(out, capacity, &length, ".1");
}

static int service_copy_truncate_log(const char *path, const char *rotated) {
    char buffer[256];
    int in_fd = (int)srv_open(path);
    if (in_fd < 0) {
        return 0;
    }
    int out_fd = (int)srv_open_mode(rotated, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (out_fd < 0) {
        srv_close(in_fd);
        return 0;
    }
    for (;;) {
        long count = srv_read(in_fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        if (srv_write(out_fd, buffer, (size_t)count) != count) {
            srv_close(in_fd);
            srv_close(out_fd);
            return 0;
        }
    }
    srv_close(in_fd);
    srv_close(out_fd);

    int trunc_fd = (int)srv_open_mode(path, SRV_OPEN_WRITE);
    if (trunc_fd < 0) {
        return 0;
    }
    int ok = srv_ftruncate(trunc_fd, 0) >= 0;
    srv_close(trunc_fd);
    return ok;
}

static void service_rotate_log(const struct service_config *config) {
    char rotated[CLI_PATH_MAX];
    int fd;
    if (config->log_path[0] == '\0') {
        cli_puts(config->name);
        cli_puts(" has no log\n");
        return;
    }
    service_rotated_path(rotated, sizeof(rotated), config->log_path);
    (void)srv_unlink(rotated);
    if (srv_rename(config->log_path, rotated) < 0 &&
        !service_copy_truncate_log(config->log_path, rotated)) {
        cli_puts(config->name);
        cli_puts(" log rotate failed\n");
        return;
    }
    fd = (int)srv_open_mode(config->log_path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (fd >= 0) {
        srv_close(fd);
    }
    cli_puts(config->name);
    cli_puts(" log rotated ");
    cli_puts(rotated);
    cli_puts("\n");
}

static void service_start_config(const struct service_config *config, const char *override_args) {
    struct srv_process_info info;
    const char *start_args = override_args != 0 && override_args[0] != '\0' ? override_args : config->args;
    int log_fd = -1;
    long pid;

    if (find_service_process(config->process_name, &info, 0)) {
        cli_puts(config->name);
        cli_puts(" already running pid ");
        cli_putn(info.pid);
        cli_puts("\n");
        return;
    }
    if (config->log_path[0] != '\0') {
        log_fd = (int)srv_open_mode(config->log_path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_APPEND);
        if (log_fd < 0) {
            cli_puts(config->name);
            cli_puts(" log open failed: ");
            cli_puts(config->log_path);
            cli_puts("\n");
            return;
        }
    }
    pid = log_fd >= 0 ?
        srv_spawn_bg_args_fds(config->command, start_args, -1, log_fd) :
        srv_spawn_bg_args(config->command, start_args);
    if (log_fd >= 0) {
        srv_close(log_fd);
    }
    if (pid < 0) {
        cli_puts(config->name);
        cli_puts(" start failed\n");
        return;
    }
    last_background_pid = (uint64_t)pid;
    cli_puts(config->name);
    cli_puts(" started pid ");
    cli_putn((uint64_t)pid);
    if (config->log_path[0] != '\0') {
        cli_puts(" log ");
        cli_puts(config->log_path);
    }
    cli_puts("\n");
}

static void service_print_log(const struct service_config *config) {
    char buffer[256];
    int fd;
    if (config->log_path[0] == '\0') {
        cli_puts(config->name);
        cli_puts(" has no log\n");
        return;
    }
    fd = (int)srv_open(config->log_path);
    if (fd < 0) {
        cli_puts(config->name);
        cli_puts(" log not found: ");
        cli_puts(config->log_path);
        cli_puts("\n");
        return;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        (void)srv_write(SRV_STDOUT, buffer, (size_t)count);
    }
    srv_close(fd);
}

static void service_tail_log(const struct service_config *config, uint64_t lines) {
    char buffer[256];
    char tail[2048];
    size_t used = 0;
    int fd;
    if (lines == 0) {
        lines = 20;
    }
    if (config->log_path[0] == '\0') {
        cli_puts(config->name);
        cli_puts(" has no log\n");
        return;
    }
    fd = (int)srv_open(config->log_path);
    if (fd < 0) {
        cli_puts(config->name);
        cli_puts(" log not found: ");
        cli_puts(config->log_path);
        cli_puts("\n");
        return;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            if (used == sizeof(tail)) {
                for (size_t j = 1; j < sizeof(tail); j++) {
                    tail[j - 1] = tail[j];
                }
                used--;
            }
            tail[used++] = buffer[i];
        }
    }
    srv_close(fd);

    size_t start = used;
    uint64_t seen = 0;
    while (start > 0 && seen < lines) {
        start--;
        if (tail[start] == '\n' && start + 1 < used) {
            seen++;
        }
    }
    if (seen == lines && start + 1 < used) {
        start++;
    } else {
        start = 0;
    }
    if (start < used) {
        (void)srv_write(SRV_STDOUT, tail + start, used - start);
    }
}

static void service_list_command(int start_enabled) {
    uint64_t size = 0;
    char path[CLI_PATH_MAX];
    char name[32];
    struct service_config config;
    int found = 0;

    for (uint64_t index = 0;; index++) {
        long next = srv_list(index, path, sizeof(path), &size);
        if (next <= 0) {
            break;
        }
        if (!service_name_from_path(name, sizeof(name), path) ||
            !service_load_config(&config, name)) {
            continue;
        }
        found = 1;
        reap_exited_service(config.process_name);
        if (start_enabled) {
            if (config.enabled) {
                service_start_config(&config, "");
            }
        } else {
            service_print_status(&config);
        }
    }

    if (!found && !start_enabled) {
        cli_puts("service: no configs in /fat/etc/services\n");
    }
}

static void service_supervise_command(uint64_t cycles) {
    if (cycles == 0) {
        cycles = 1;
    }
    for (uint64_t cycle = 0; cycle < cycles; cycle++) {
        uint64_t size = 0;
        char path[CLI_PATH_MAX];
        char name[32];
        struct service_config config;
        for (uint64_t index = 0;; index++) {
            long next = srv_list(index, path, sizeof(path), &size);
            if (next <= 0) {
                break;
            }
            if (!service_name_from_path(name, sizeof(name), path) ||
                !service_load_config(&config, name) ||
                !config.enabled ||
                !cli_streq(config.restart, "always")) {
                continue;
            }
            reap_exited_service(config.process_name);
            struct srv_process_info info;
            if (!find_service_process(config.process_name, &info, 0)) {
                cli_puts(config.name);
                cli_puts(" supervisor restarting\n");
                service_start_config(&config, "");
            }
        }
        if (cycle + 1 < cycles) {
            srv_sleep_ticks(50);
        }
    }
}

static void service_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    char *action;
    char *action_args;
    struct service_config config;

    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0' || cli_is_help_arg(name)) {
        cli_puts("usage: service list|status --all|reload|enable <name>|disable <name>|restart <name> [--wait]|set <name> <key> <value>|unset <name> <key>|start-enabled|supervise [cycles]|<name> [enable|disable|start [args]|stop|restart [--wait]|status|check|check-config|log|tail [lines]|rotate-log]\n");
        cli_puts("known: webd, /fat/bin/<name>, /fat/etc/services/<name>.svc\n");
        return;
    }
    action = name;
    while (*action != '\0' && *action != ' ' && *action != '\t') {
        action++;
    }
    if (*action != '\0') {
        *action++ = '\0';
        action = cli_trim(action);
    }

    if (cli_streq(name, "list")) {
        service_list_command(0);
        return;
    }
    if (cli_streq(name, "status") && cli_streq(action, "--all")) {
        service_list_command(0);
        return;
    }
    if (cli_streq(name, "reload")) {
        (void)service_request_reload();
        return;
    }
    if (cli_streq(name, "enable") || cli_streq(name, "disable")) {
        int enable = cli_streq(name, "enable");
        char target[32];
        cli_copy(target, sizeof(target), action);
        if (target[0] == '\0') {
            cli_puts(enable ? "usage: service enable <name>\n" : "usage: service disable <name>\n");
            return;
        }
        if (!service_load_config(&config, target)) {
            cli_puts("service: not found: ");
            cli_puts(target);
            cli_puts("\n");
            return;
        }
        service_set_enabled(&config, enable);
        return;
    }
    if (cli_streq(name, "restart")) {
        char target[32];
        char *restart_args = action;
        while (*restart_args != '\0' && *restart_args != ' ' && *restart_args != '\t') {
            restart_args++;
        }
        if (*restart_args != '\0') {
            *restart_args++ = '\0';
            restart_args = cli_trim(restart_args);
        }
        cli_copy(target, sizeof(target), action);
        if (target[0] == '\0') {
            cli_puts("usage: service restart <name> [--wait]\n");
            return;
        }
        if (!service_load_config(&config, target)) {
            cli_puts("service: not found: ");
            cli_puts(target);
            cli_puts("\n");
            return;
        }
        (void)service_stop_config(&config);
        if (config.enabled && cli_streq(config.restart, "always")) {
            (void)service_request_reload();
        } else {
            service_start_config(&config, "");
        }
        if (cli_streq(restart_args, "--wait")) {
            (void)service_wait_healthy(&config, 300);
        }
        return;
    }
    if (cli_streq(name, "set") || cli_streq(name, "unset")) {
        int set_mode = cli_streq(name, "set");
        char target[32];
        char key[32];
        char *cursor = action;
        char *key_start;
        char *value;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
            cursor = cli_trim(cursor);
        }
        cli_copy(target, sizeof(target), action);
        key_start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
            cursor = cli_trim(cursor);
        }
        cli_copy(key, sizeof(key), key_start);
        value = cursor;
        if (target[0] == '\0' || key[0] == '\0' || (set_mode && value[0] == '\0')) {
            cli_puts(set_mode ? "usage: service set <name> <key> <value>\n" : "usage: service unset <name> <key>\n");
            return;
        }
        if (!service_load_config(&config, target)) {
            cli_puts("service: not found: ");
            cli_puts(target);
            cli_puts("\n");
            return;
        }
        if (set_mode ? !service_set_config_field(&config, key, value) : !service_unset_config_field(&config, key)) {
            cli_puts("service: unknown field: ");
            cli_puts(key);
            cli_puts("\n");
            return;
        }
        if (!service_save_config(&config)) {
            return;
        }
        cli_puts(config.name);
        cli_puts(set_mode ? " set " : " unset ");
        cli_puts(key);
        cli_puts("\n");
        (void)service_request_reload();
        return;
    }
    if (cli_streq(name, "start-enabled")) {
        service_list_command(1);
        return;
    }
    if (cli_streq(name, "supervise")) {
        service_supervise_command(action[0] != '\0' ? parse_u64(action) : 1);
        return;
    }

    if (!service_load_config(&config, name)) {
        cli_puts("service: not found: ");
        cli_puts(name);
        cli_puts(" (expected /fat/etc/services/");
        cli_puts(name);
        cli_puts(".svc)\n");
        return;
    }
    if (action[0] == '\0') {
        action = "status";
    }
    action_args = action;
    while (*action_args != '\0' && *action_args != ' ' && *action_args != '\t') {
        action_args++;
    }
    if (*action_args != '\0') {
        *action_args++ = '\0';
        action_args = cli_trim(action_args);
    }

    reap_exited_service(config.process_name);
    if (cli_streq(action, "status")) {
        service_print_status(&config);
        return;
    }

    if (cli_streq(action, "check")) {
        service_check_config(&config);
        return;
    }

    if (cli_streq(action, "check-config")) {
        (void)service_check_config_file(&config);
        return;
    }

    if (cli_streq(action, "enable")) {
        service_set_enabled(&config, 1);
        return;
    }

    if (cli_streq(action, "disable")) {
        service_set_enabled(&config, 0);
        return;
    }

    if (cli_streq(action, "start")) {
        service_start_config(&config, action_args);
        return;
    }

    if (cli_streq(action, "stop")) {
        (void)service_stop_config(&config);
        return;
    }

    if (cli_streq(action, "restart")) {
        (void)service_stop_config(&config);
        if (config.enabled &&
            cli_streq(config.restart, "always") &&
            (action_args[0] == '\0' || cli_streq(action_args, "--wait"))) {
            (void)service_request_reload();
        } else {
            service_start_config(&config, action_args);
        }
        if (cli_streq(action_args, "--wait")) {
            (void)service_wait_healthy(&config, 300);
        }
        return;
    }

    if (cli_streq(action, "log")) {
        service_print_log(&config);
        return;
    }

    if (cli_streq(action, "tail")) {
        service_tail_log(&config, action_args[0] != '\0' ? parse_u64(action_args) : 20);
        return;
    }

    if (cli_streq(action, "rotate-log")) {
        service_rotate_log(&config);
        return;
    }

    cli_puts("usage: service list|status --all|reload|enable <name>|disable <name>|restart <name> [--wait]|set <name> <key> <value>|unset <name> <key>|start-enabled|supervise [cycles]|<name> [enable|disable|start [args]|stop|restart [--wait]|status|check|check-config|log|tail [lines]|rotate-log]\n");
}

static void print_path(void) {
    for (size_t i = 0; i < path_count; i++) {
        cli_puts(path_entries[i]);
        cli_puts("\n");
    }
}

static uint64_t add_path(const char *directory) {
    if (directory == 0 || directory[0] == '\0') {
        cli_puts("usage: path [add <dir>]\n");
        return 2;
    }
    if (path_count >= PATH_MAX_ENTRIES) {
        cli_puts("path: full\n");
        return 1;
    }
    cli_copy(path_entries[path_count++], CLI_PATH_MAX, directory);
    return 0;
}

static void clear_path(void) {
    path_count = 0;
}

static void set_path_list(const char *value) {
    char work[LINE_MAX];
    char *entry = work;
    clear_path();
    cli_copy(work, sizeof(work), value);
    for (char *cursor = work; ; cursor++) {
        if (*cursor == ':' || *cursor == '\0') {
            char done = *cursor;
            *cursor = '\0';
            entry = cli_trim(entry);
            if (entry[0] != '\0' && path_count < PATH_MAX_ENTRIES) {
                cli_copy(path_entries[path_count++], CLI_PATH_MAX, entry);
            }
            if (done == '\0') {
                break;
            }
            entry = cursor + 1;
        }
    }
    if (path_count == 0) {
        cli_copy(path_entries[path_count++], CLI_PATH_MAX, "/fat/bin");
    }
}

static void print_env(void) {
    for (size_t i = 0; environ[i] != 0; i++) {
        cli_puts(environ[i]);
        cli_puts("\n");
    }
}

static int is_builtin(const char *name) {
    for (size_t i = 0; i < sizeof(shell_builtins) / sizeof(shell_builtins[0]); i++) {
        if (cli_streq(name, shell_builtins[i])) {
            return 1;
        }
    }
    return 0;
}

static struct shell_alias *find_alias(const char *name) {
    for (size_t i = 0; i < SHELL_MAX_ALIASES; i++) {
        if (aliases[i].name[0] != '\0' && cli_streq(aliases[i].name, name)) {
            return &aliases[i];
        }
    }
    return 0;
}

static void print_aliases(void) {
    for (size_t i = 0; i < SHELL_MAX_ALIASES; i++) {
        if (aliases[i].name[0] != '\0') {
            cli_puts("alias ");
            cli_puts(aliases[i].name);
            cli_puts("='");
            cli_puts(aliases[i].value);
            cli_puts("'\n");
        }
    }
}

static uint64_t alias_command(const char *args) {
    char work[LINE_MAX];
    char *text;
    char *equals;
    struct shell_alias *slot = 0;

    cli_copy(work, sizeof(work), args);
    text = cli_trim(work);
    if (text[0] == '\0') {
        print_aliases();
        return 0;
    }
    equals = strchr(text, '=');
    if (equals == 0 || equals == text) {
        slot = find_alias(text);
        if (slot == 0) {
            cli_puts("alias: not found: ");
            cli_puts(text);
            cli_puts("\n");
            return 1;
        }
        cli_puts("alias ");
        cli_puts(slot->name);
        cli_puts("='");
        cli_puts(slot->value);
        cli_puts("'\n");
        return 0;
    }
    *equals++ = '\0';
    text = cli_trim(text);
    if (!shell_is_name_start(text[0])) {
        cli_puts("alias: bad name\n");
        return 2;
    }
    for (char *cursor = text; *cursor != '\0'; cursor++) {
        if (!shell_is_name_char(*cursor)) {
            cli_puts("alias: bad name\n");
            return 2;
        }
    }
    slot = find_alias(text);
    if (slot == 0) {
        for (size_t i = 0; i < SHELL_MAX_ALIASES; i++) {
            if (aliases[i].name[0] == '\0') {
                slot = &aliases[i];
                break;
            }
        }
    }
    if (slot == 0) {
        cli_puts("alias: full\n");
        return 1;
    }
    cli_copy(slot->name, sizeof(slot->name), text);
    char *value = cli_trim(equals);
    size_t value_length = cli_strlen(value);
    if (value_length >= 2 &&
        ((value[0] == '\'' && value[value_length - 1] == '\'') ||
            (value[0] == '"' && value[value_length - 1] == '"'))) {
        value[value_length - 1] = '\0';
        value++;
    }
    cli_copy(slot->value, sizeof(slot->value), value);
    return 0;
}

static struct shell_function *find_function(const char *name) {
    for (size_t i = 0; i < SHELL_MAX_FUNCTIONS; i++) {
        if (functions[i].name[0] != '\0' && cli_streq(functions[i].name, name)) {
            return &functions[i];
        }
    }
    return 0;
}

static int valid_shell_name(const char *name) {
    if (!shell_is_name_start(name[0])) {
        return 0;
    }
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        if (!shell_is_name_char(*cursor)) {
            return 0;
        }
    }
    return 1;
}

static uint64_t define_function(const char *name, const char *body) {
    struct shell_function *slot = find_function(name);
    if (!valid_shell_name(name) || is_builtin(name)) {
        cli_puts("function: bad name\n");
        return 2;
    }
    if (slot == 0) {
        for (size_t i = 0; i < SHELL_MAX_FUNCTIONS; i++) {
            if (functions[i].name[0] == '\0') {
                slot = &functions[i];
                break;
            }
        }
    }
    if (slot == 0) {
        cli_puts("function: table full\n");
        return 1;
    }
    cli_copy(slot->name, sizeof(slot->name), name);
    cli_copy(slot->body, sizeof(slot->body), body);
    return 0;
}

static uint64_t return_command(const char *args) {
    if (function_depth == 0) {
        cli_puts("return: not in function\n");
        return 2;
    }
    return_status = args[0] != '\0' ? parse_u64(args) : last_status;
    return_requested = 1;
    return return_status;
}

static uint64_t shift_command(const char *args) {
    char work[LINE_MAX];
    char *argv[2];
    int argc;
    int64_t parsed = 1;
    uint64_t count;

    cli_copy(work, sizeof(work), args);
    argc = split_words(work, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0 || argc > 1 || (argc == 1 && (!parse_i64(argv[0], &parsed) || parsed < 0))) {
        cli_puts("usage: shift [count]\n");
        return 2;
    }
    count = (uint64_t)parsed;
    if (count > (uint64_t)(shell_argc > 0 ? shell_argc - 1 : 0)) {
        cli_puts("shift: count exceeds positional parameters\n");
        return 1;
    }
    for (int i = 1; i + (int)count < shell_argc; i++) {
        shell_argv[i] = shell_argv[i + count];
    }
    shell_argc -= (int)count;
    shell_argv[shell_argc] = 0;
    return 0;
}

static uint64_t run_function(struct shell_function *function, const char *args, char *cwd) {
    char body[EXPANDED_LINE_MAX];
    char args_copy[ARG_EXPANDED_MAX];
    char arg_storage[SHELL_MAX_FUNCTION_ARGS][64];
    char *argv[SHELL_MAX_FUNCTION_ARGS + 1];
    char **saved_argv = shell_argv;
    int saved_argc = shell_argc;
    int saved_return_requested = return_requested;
    uint64_t saved_return_status = return_status;
    int argc = 1;
    uint64_t status;

    cli_copy(body, sizeof(body), function->body);
    cli_copy(args_copy, sizeof(args_copy), args);
    cli_copy(arg_storage[0], sizeof(arg_storage[0]), function->name);
    argv[0] = arg_storage[0];

    char *cursor = cli_trim(args_copy);
    while (*cursor != '\0' && argc < SHELL_MAX_FUNCTION_ARGS) {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        char saved = *cursor;
        *cursor = '\0';
        cli_copy(arg_storage[argc], sizeof(arg_storage[argc]), start);
        argv[argc] = arg_storage[argc];
        argc++;
        if (saved == '\0') {
            break;
        }
        *cursor++ = saved;
    }
    argv[argc] = 0;

    shell_argv = argv;
    shell_argc = argc;
    function_depth++;
    return_requested = 0;
    return_status = 0;
    status = run_line(body, cwd);
    if (return_requested) {
        status = return_status;
    }
    function_depth--;
    shell_argv = saved_argv;
    shell_argc = saved_argc;
    return_requested = saved_return_requested;
    return_status = saved_return_status;
    return status;
}

static uint64_t export_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    char *equals;
    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        print_env();
        return 0;
    }
    equals = strchr(name, '=');
    if (equals == name) {
        cli_puts("usage: export NAME[=value]\n");
        return 2;
    }
    if (equals != 0) {
        *equals++ = '\0';
    }
    if (setenv(name, equals != 0 ? equals : (getenv(name) != 0 ? getenv(name) : ""), 1) < 0) {
        cli_puts("export: failed\n");
        return 1;
    }
    if (cli_streq(name, "PATH") && equals != 0) {
        set_path_list(equals);
    } else if (cli_streq(name, "HISTSIZE") && equals != 0) {
        configure_history();
    }
    return 0;
}

static uint64_t unset_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        cli_puts("usage: unset NAME\n");
        return 2;
    }
    while (*name != '\0') {
        char *next = name;
        while (*next != '\0' && *next != ' ' && *next != '\t') {
            next++;
        }
        if (*next != '\0') {
            *next++ = '\0';
        }
        unsetenv(name);
        if (cli_streq(name, "PATH")) {
            set_path_list("");
        } else if (cli_streq(name, "HISTSIZE")) {
            configure_history();
        }
        name = cli_trim(next);
    }
    return 0;
}

static uint64_t which_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    int found = 0;
    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0' || cli_is_help_arg(name)) {
        cli_puts("usage: which <command> [...]\n");
        return name[0] == '\0' ? 2 : 0;
    }
    while (*name != '\0') {
        char *next = name;
        char path[CLI_PATH_MAX];
        while (*next != '\0' && *next != ' ' && *next != '\t') {
            next++;
        }
        if (*next != '\0') {
            *next++ = '\0';
        }
        if (resolve_command(path, sizeof(path), name)) {
            cli_puts(path);
            cli_puts("\n");
            found = 1;
        }
        name = cli_trim(next);
    }
    if (!found) {
        cli_puts("which: not found\n");
        return 1;
    }
    return 0;
}

static uint64_t type_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    uint64_t result = 0;
    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        cli_puts("usage: type <name> [...]\n");
        return 2;
    }
    while (*name != '\0') {
        char *next = name;
        char path[CLI_PATH_MAX];
        struct shell_alias *alias;
        struct shell_function *function;
        while (*next != '\0' && *next != ' ' && *next != '\t') {
            next++;
        }
        if (*next != '\0') {
            *next++ = '\0';
        }
        alias = find_alias(name);
        function = find_function(name);
        if (alias != 0) {
            cli_puts(name);
            cli_puts(" is aliased to '");
            cli_puts(alias->value);
            cli_puts("'\n");
            result = 0;
        } else if (function != 0) {
            cli_puts(name);
            cli_puts(" is a shell function\n");
            result = 0;
        } else if (is_builtin(name)) {
            cli_puts(name);
            cli_puts(" is a shell builtin\n");
            result = 0;
        } else if (resolve_command(path, sizeof(path), name)) {
            cli_puts(name);
            cli_puts(" is ");
            cli_puts(path);
            cli_puts("\n");
            result = 0;
        } else {
            cli_puts(name);
            cli_puts(" not found\n");
            result = 1;
        }
        name = cli_trim(next);
    }
    return result;
}

static uint64_t command_lookup(const char *args, int verbose) {
    char work[LINE_MAX];
    char *name;
    uint64_t result = 0;

    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        return 0;
    }
    while (*name != '\0') {
        char *next = name;
        char path[CLI_PATH_MAX];
        struct shell_alias *alias;
        struct shell_function *function;
        while (*next != '\0' && *next != ' ' && *next != '\t') {
            next++;
        }
        if (*next != '\0') {
            *next++ = '\0';
        }
        alias = find_alias(name);
        function = find_function(name);
        if (verbose && alias != 0) {
            cli_puts(name);
            cli_puts(" is aliased to '");
            cli_puts(alias->value);
            cli_puts("'\n");
        } else if (verbose && function != 0) {
            cli_puts(name);
            cli_puts(" is a shell function\n");
        } else if (verbose && is_builtin(name)) {
            cli_puts(name);
            cli_puts(" is a shell builtin\n");
        } else if (verbose && resolve_command(path, sizeof(path), name)) {
            cli_puts(name);
            cli_puts(" is ");
            cli_puts(path);
            cli_puts("\n");
        } else if (!verbose && alias != 0) {
            cli_puts("alias ");
            cli_puts(name);
            cli_puts("='");
            cli_puts(alias->value);
            cli_puts("'\n");
        } else if (!verbose && (function != 0 || is_builtin(name))) {
            cli_puts(name);
            cli_puts("\n");
        } else if (!verbose && resolve_command(path, sizeof(path), name)) {
            cli_puts(path);
            cli_puts("\n");
        } else {
            result = 1;
        }
        name = cli_trim(next);
    }
    return result;
}

static uint64_t command_command(const char *args, char *cwd, int background) {
    char work[ARG_EXPANDED_MAX];
    char command_line[ARG_EXPANDED_MAX];
    char *argv[32];
    int argc;
    int index = 0;
    int lookup = 0;
    int verbose = 0;
    size_t out = 0;

    cli_copy(work, sizeof(work), args);
    argc = split_words(work, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0) {
        cli_puts("command: too many arguments\n");
        return 2;
    }
    if (argc == 0) {
        return 0;
    }
    while (index < argc) {
        if (cli_streq(argv[index], "--")) {
            index++;
            break;
        }
        if (cli_streq(argv[index], "-p")) {
            index++;
            continue;
        }
        if (cli_streq(argv[index], "-v")) {
            lookup = 1;
            verbose = 0;
            index++;
            continue;
        }
        if (cli_streq(argv[index], "-V")) {
            lookup = 1;
            verbose = 1;
            index++;
            continue;
        }
        if (argv[index][0] == '-') {
            cli_puts("usage: command [-p] [-v|-V] command [args]\n");
            return 2;
        }
        break;
    }
    if (index >= argc) {
        return 0;
    }
    command_line[0] = '\0';
    for (int i = index; i < argc; i++) {
        if (i > index) {
            append_char(command_line, sizeof(command_line), &out, ' ');
        }
        append_text(command_line, sizeof(command_line), &out, argv[i]);
    }
    if (lookup) {
        return command_lookup(command_line, verbose);
    }
    return run_command_impl(command_line, cwd, background, 1);
}

static uint64_t loop_control_command(const char *args, int is_continue) {
    char work[LINE_MAX];
    char *argv[2];
    int argc;
    int64_t parsed = 1;

    if (loop_depth == 0) {
        cli_puts(is_continue ? "continue: not in loop\n" : "break: not in loop\n");
        return 2;
    }
    cli_copy(work, sizeof(work), args);
    argc = split_words(work, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0 || argc > 1 || (argc == 1 && (!parse_i64(argv[0], &parsed) || parsed <= 0))) {
        cli_puts(is_continue ? "usage: continue [count]\n" : "usage: break [count]\n");
        return 2;
    }
    if (is_continue) {
        continue_requested = (uint64_t)parsed;
    } else {
        break_requested = (uint64_t)parsed;
    }
    return 0;
}

static uint64_t set_command(const char *args) {
    char work[LINE_MAX];
    char *text;
    cli_copy(work, sizeof(work), args);
    text = cli_trim(work);
    if (cli_streq(text, "-e")) {
        exit_on_error = 1;
        return 0;
    }
    if (cli_streq(text, "+e")) {
        exit_on_error = 0;
        return 0;
    }
    if (text[0] == '\0') {
        print_env();
        return 0;
    }
    cli_puts("usage: set [-e|+e]\n");
    return 2;
}

static uint64_t read_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    char value[LINE_MAX];
    size_t length = 0;

    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        cli_puts("usage: read NAME\n");
        return 2;
    }
    for (char *cursor = name; *cursor != '\0'; cursor++) {
        if (!shell_is_name_char(*cursor)) {
            *cursor = '\0';
            break;
        }
    }
    if (!shell_is_name_start(name[0])) {
        cli_puts("read: bad name\n");
        return 2;
    }
    while (length + 1 < sizeof(value)) {
        char c;
        long count = srv_read(SRV_STDIN, &c, 1);
        if (count <= 0) {
            break;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            break;
        }
        value[length++] = c;
    }
    value[length] = '\0';
    if (setenv(name, value, 1) < 0) {
        cli_puts("read: failed\n");
        return 1;
    }
    return 0;
}

static int apply_assignment(const char *name, const char *value) {
    char decoded[ARG_EXPANDED_MAX];
    char decode_input[ARG_EXPANDED_MAX];
    char *argv[2];
    int argc;
    if (!shell_is_name_start(name[0])) {
        return 0;
    }
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        if (!shell_is_name_char(*cursor)) {
            return 0;
        }
    }
    cli_copy(decode_input, sizeof(decode_input), value);
    argc = split_words(decode_input, argv, 1);
    if (argc == 1) {
        cli_copy(decoded, sizeof(decoded), argv[0]);
    } else if (value[0] == '\0') {
        decoded[0] = '\0';
    } else {
        return 0;
    }
    if (setenv(name, decoded, 1) < 0) {
        return 0;
    }
    if (cli_streq(name, "PATH")) {
        set_path_list(decoded);
    }
    return 1;
}

static int apply_decoded_assignment(const char *name, const char *value) {
    if (!valid_shell_name(name)) {
        return 0;
    }
    if (setenv(name, value, 1) < 0) {
        return 0;
    }
    if (cli_streq(name, "PATH")) {
        set_path_list(value);
    }
    return 1;
}

struct temporary_assignment {
    char name[64];
    char value[ARG_EXPANDED_MAX];
    char old_value[ARG_EXPANDED_MAX];
    int had_old_value;
};

static int parse_assignment_token(const char *token, char *name, size_t name_capacity, char *value, size_t value_capacity) {
    char decoded[ARG_EXPANDED_MAX];
    char decoded_name[64];
    char decoded_value[ARG_EXPANDED_MAX];
    char *equals;
    char *argv[2];
    int argc;

    cli_copy(decoded, sizeof(decoded), token);
    argc = split_words(decoded, argv, 1);
    if (argc != 1) {
        return 0;
    }
    equals = strchr(argv[0], '=');
    if (equals == 0 || equals == argv[0]) {
        return 0;
    }
    *equals++ = '\0';
    cli_copy(decoded_name, sizeof(decoded_name), argv[0]);
    cli_copy(decoded_value, sizeof(decoded_value), equals);
    if (!valid_shell_name(decoded_name)) {
        return 0;
    }
    cli_copy(name, name_capacity, decoded_name);
    cli_copy(value, value_capacity, decoded_value);
    return 1;
}

static char *skip_command_word(char *cursor) {
    char quote = '\0';
    while (*cursor != '\0') {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
            } else if (*cursor == '\\' && cursor[1] != '\0') {
                cursor++;
            }
            cursor++;
            continue;
        }
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor++;
            continue;
        }
        if (*cursor == ' ' || *cursor == '\t') {
            break;
        }
        cursor++;
    }
    return cursor;
}

static size_t collect_leading_assignments(char *command,
    struct temporary_assignment *assignments,
    size_t capacity,
    char **rest_out) {
    char *cursor = command;
    size_t count = 0;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    while (*cursor != '\0') {
        char token[ARG_EXPANDED_MAX];
        char *end = skip_command_word(cursor);
        char saved = *end;
        size_t token_length = 0;
        if ((size_t)(end - cursor) + 1 >= sizeof(token)) {
            break;
        }
        for (char *read = cursor; read < end && token_length + 1 < sizeof(token); read++) {
            token[token_length++] = *read;
        }
        token[token_length] = '\0';
        if (count >= capacity ||
            !parse_assignment_token(token,
                assignments[count].name,
                sizeof(assignments[count].name),
                assignments[count].value,
                sizeof(assignments[count].value))) {
            break;
        }
        count++;
        if (saved == '\0') {
            cursor = end;
            break;
        }
        cursor = end + 1;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
    }
    *rest_out = cursor;
    return count;
}

static int apply_temporary_assignments(struct temporary_assignment *assignments, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const char *old = getenv(assignments[i].name);
        assignments[i].had_old_value = old != 0;
        if (old != 0) {
            cli_copy(assignments[i].old_value, sizeof(assignments[i].old_value), old);
        } else {
            assignments[i].old_value[0] = '\0';
        }
        if (setenv(assignments[i].name, assignments[i].value, 1) < 0) {
            return 0;
        }
        if (cli_streq(assignments[i].name, "PATH")) {
            set_path_list(assignments[i].value);
        }
    }
    return 1;
}

static void restore_temporary_assignments(struct temporary_assignment *assignments, size_t count) {
    for (size_t i = count; i > 0; i--) {
        struct temporary_assignment *assignment = &assignments[i - 1];
        if (assignment->had_old_value) {
            setenv(assignment->name, assignment->old_value, 1);
            if (cli_streq(assignment->name, "PATH")) {
                set_path_list(assignment->old_value);
            }
        } else {
            unsetenv(assignment->name);
            if (cli_streq(assignment->name, "PATH")) {
                set_path_list("");
            }
        }
    }
}

static char *find_argument_tail(char *command) {
    char quote = '\0';
    while (*command != '\0') {
        if (quote != '\0') {
            if (*command == quote) {
                quote = '\0';
            } else if (*command == '\\' && command[1] != '\0') {
                command++;
            }
            command++;
            continue;
        }
        if (*command == '\'' || *command == '"') {
            quote = *command++;
            continue;
        }
        if (*command == ' ' || *command == '\t') {
            break;
        }
        command++;
    }
    return command;
}

static int rewrite_heredoc_command(const char *line,
    const char *temp_path,
    char *rewritten,
    size_t rewritten_capacity,
    char *delimiter,
    size_t delimiter_capacity) {
    char quote = '\0';
    size_t out = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == '<' && line[i + 1] == '<') {
            size_t cursor = i + 2;
            char raw[64];
            size_t raw_length = 0;
            char *argv[2];
            char decoded[64];
            int argc;
            while (line[cursor] == ' ' || line[cursor] == '\t') {
                cursor++;
            }
            while (line[cursor] != '\0' &&
                line[cursor] != ' ' &&
                line[cursor] != '\t' &&
                line[cursor] != ';' &&
                raw_length + 1 < sizeof(raw)) {
                raw[raw_length++] = line[cursor++];
            }
            raw[raw_length] = '\0';
            if (raw[0] == '\0') {
                return 0;
            }
            cli_copy(decoded, sizeof(decoded), raw);
            argc = split_words(decoded, argv, 1);
            if (argc != 1 || argv[0][0] == '\0') {
                return 0;
            }
            cli_copy(delimiter, delimiter_capacity, argv[0]);
            for (size_t copy = 0; copy < i && out + 1 < rewritten_capacity; copy++) {
                rewritten[out++] = line[copy];
            }
            rewritten[out] = '\0';
            append_text(rewritten, rewritten_capacity, &out, " < ");
            append_text(rewritten, rewritten_capacity, &out, temp_path);
            append_text(rewritten, rewritten_capacity, &out, line + cursor);
            return 1;
        }
    }
    return 0;
}

static uint64_t run_script_command_line(char *line, char *cwd, uint64_t line_number) {
    char *trimmed = cli_trim(line);
    if (trimmed[0] == '\0' || trimmed[0] == '#') {
        return last_status;
    }
    active_script_line = line_number;
    return run_line(trimmed, cwd);
}

static uint64_t run_script(const char *path, char *cwd) {
    int fd = (int)srv_open(path);
    char buffer[128];
    char line[LINE_MAX];
    char block[EXPANDED_LINE_MAX];
    char heredoc_command[LINE_MAX];
    char heredoc_delimiter[64];
    char heredoc_temp[CLI_PATH_MAX];
    char heredoc_body[EXPANDED_LINE_MAX];
    size_t length = 0;
    size_t block_length = 0;
    size_t heredoc_body_length = 0;
    uint64_t line_number = 1;
    uint64_t block_start_line = 0;
    uint64_t heredoc_start_line = 0;
    int collecting_block = 0;
    int collecting_heredoc = 0;
    char block_end_keyword[8];
    int stop = 0;
    uint64_t status = 0;
    const char *saved_script_path = active_script_path;
    uint64_t saved_script_line = active_script_line;
    if (fd < 0) {
        cli_puts("source: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    active_script_path = path;
    active_script_line = 0;
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            shell_error("source: read failed\n");
            break;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count && !stop; i++) {
            char c = buffer[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                uint64_t current_line = line_number++;
                if (!collecting_heredoc && length > 0 && line[length - 1] == '\\') {
                    length--;
                    continue;
                }
                line[length] = '\0';
                if (collecting_heredoc) {
                    char compare[LINE_MAX];
                    cli_copy(compare, sizeof(compare), line);
                    if (cli_streq(cli_trim(compare), heredoc_delimiter)) {
                        if (srv_fs_write(heredoc_temp, heredoc_body, heredoc_body_length) < 0) {
                            active_script_line = heredoc_start_line;
                            shell_error("source: heredoc write failed\n");
                            status = 1;
                        } else {
                            status = run_script_command_line(heredoc_command, cwd, heredoc_start_line);
                        }
                        (void)srv_unlink(heredoc_temp);
                        if (exit_on_error && status != 0) {
                            stop = 1;
                        }
                        collecting_heredoc = 0;
                        heredoc_body_length = 0;
                        heredoc_body[0] = '\0';
                    } else {
                        append_text(heredoc_body, sizeof(heredoc_body), &heredoc_body_length, line);
                        append_char(heredoc_body, sizeof(heredoc_body), &heredoc_body_length, '\n');
                    }
                    length = 0;
                    continue;
                }
                char *trimmed = cli_trim(line);
                if (collecting_block) {
                    append_text(block, sizeof(block), &block_length, "; ");
                    append_text(block, sizeof(block), &block_length, trimmed);
                    if ((block_end_keyword[0] == '}' && cli_streq(trimmed, "}")) ||
                        (block_end_keyword[0] != '}' && shell_keyword_at(trimmed, 0, block_end_keyword))) {
                        status = run_line(block, cwd);
                        if (exit_on_error && status != 0) {
                            stop = 1;
                        }
                        collecting_block = 0;
                        block_length = 0;
                        block[0] = '\0';
                    }
                } else if (shell_keyword_at(trimmed, 0, "if") && !find_if_parts(trimmed, &(struct if_parts){0})) {
                    collecting_block = 1;
                    block_start_line = current_line;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "fi");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (shell_keyword_at(trimmed, 0, "for") && !find_for_parts(trimmed, &(struct for_parts){0})) {
                    collecting_block = 1;
                    block_start_line = current_line;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "done");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (shell_keyword_at(trimmed, 0, "while") && !find_while_parts(trimmed, &(struct while_parts){0})) {
                    collecting_block = 1;
                    block_start_line = current_line;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "done");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (shell_keyword_at(trimmed, 0, "case") && !find_case_parts(trimmed, &(struct case_parts){0})) {
                    collecting_block = 1;
                    block_start_line = current_line;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "esac");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (is_function_start(trimmed) &&
                    !find_function_definition(trimmed, (char[32]){0}, 32, (char[EXPANDED_LINE_MAX]){0}, EXPANDED_LINE_MAX)) {
                    collecting_block = 1;
                    block_start_line = current_line;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "}");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else {
                    make_substitution_path(heredoc_temp, sizeof(heredoc_temp));
                    if (rewrite_heredoc_command(trimmed,
                            heredoc_temp,
                            heredoc_command,
                            sizeof(heredoc_command),
                            heredoc_delimiter,
                            sizeof(heredoc_delimiter))) {
                        collecting_heredoc = 1;
                        heredoc_start_line = current_line;
                        heredoc_body_length = 0;
                        heredoc_body[0] = '\0';
                    } else {
                        status = run_script_command_line(line, cwd, current_line);
                        if (exit_on_error && status != 0) {
                            stop = 1;
                        }
                    }
                }
                length = 0;
            } else if (length + 1 < sizeof(line)) {
                line[length++] = c;
            }
        }
        if (stop) {
            break;
        }
    }
    if (!stop && collecting_heredoc) {
        if (length > 0) {
            line[length] = '\0';
            char compare[LINE_MAX];
            cli_copy(compare, sizeof(compare), line);
            if (cli_streq(cli_trim(compare), heredoc_delimiter)) {
                if (srv_fs_write(heredoc_temp, heredoc_body, heredoc_body_length) < 0) {
                    active_script_line = heredoc_start_line;
                    shell_error("source: heredoc write failed\n");
                    status = 1;
                } else {
                    status = run_script_command_line(heredoc_command, cwd, heredoc_start_line);
                }
                (void)srv_unlink(heredoc_temp);
                collecting_heredoc = 0;
                length = 0;
            }
        }
        if (collecting_heredoc) {
            active_script_line = heredoc_start_line;
            shell_error("source: unterminated heredoc\n");
            (void)srv_unlink(heredoc_temp);
            status = 2;
        }
    } else if (!stop && collecting_block) {
        active_script_line = block_start_line;
        shell_error("source: unterminated block\n");
        status = 2;
    } else if (!stop && length > 0) {
        line[length] = '\0';
        status = run_script_command_line(line, cwd, line_number);
    }
    srv_close(fd);
    active_script_path = saved_script_path;
    active_script_line = saved_script_line;
    return status;
}

static uint64_t run_optional_script(const char *path, char *cwd) {
    struct srv_stat info;
    if (srv_stat(path, &info) < 0 || info.type != 0) {
        return 0;
    }
    return run_script(path, cwd);
}

static int ends_with(const char *text, const char *suffix) {
    size_t text_length = cli_strlen(text);
    size_t suffix_length = cli_strlen(suffix);
    if (suffix_length > text_length) {
        return 0;
    }
    return cli_streq(text + text_length - suffix_length, suffix);
}

static uint64_t run_optional_script_directory(const char *directory, char *cwd) {
    uint64_t status = 0;
    for (uint64_t index = 0;; index++) {
        char path[CLI_PATH_MAX];
        const char *name = 0;
        uint64_t size = 0;
        struct srv_stat info;
        long result = srv_list(index, path, sizeof(path), &size);
        (void)size;
        if (result <= 0) {
            break;
        }
        if (!path_is_immediate_child(path, directory, &name) || !ends_with(name, ".sh")) {
            continue;
        }
        if (srv_stat(path, &info) == 0 && info.type == 0) {
            status = run_script(path, cwd);
            last_status = status;
            if (exit_on_error && status != 0) {
                break;
            }
        }
    }
    return status;
}

static void normalize_command_text(const char *input, char *out, size_t capacity) {
    size_t length = 0;
    out[0] = '\0';
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] == '\\' && input[i + 1] == '\n') {
            i++;
            continue;
        }
        if (input[i] == '\r') {
            continue;
        }
        if (input[i] == '\n') {
            append_char(out, capacity, &length, ';');
            continue;
        }
        append_char(out, capacity, &length, input[i]);
    }
}

static int text_contains_heredoc(const char *text) {
    char quote = '\0';
    for (size_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && text[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && text[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == '<' && text[i + 1] == '<') {
            return 1;
        }
    }
    return 0;
}

static void init_redirection(struct command_redirection *redirection) {
    redirection->stdin_set = 0;
    redirection->stdout_set = 0;
    redirection->stderr_set = 0;
    redirection->stderr_to_stdout = 0;
    redirection->stdout_append = 0;
    redirection->stderr_append = 0;
    redirection->stdin_path[0] = '\0';
    redirection->stdout_path[0] = '\0';
    redirection->stderr_path[0] = '\0';
}

static char *copy_redirection_path(char *cursor, char *destination, size_t capacity) {
    size_t out = 0;
    char quote = '\0';
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    while (*cursor != '\0') {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
                cursor++;
                continue;
            }
            if (*cursor == '\\' && cursor[1] != '\0') {
                cursor++;
            }
        } else {
            if (*cursor == '\'' || *cursor == '"') {
                quote = *cursor++;
                continue;
            }
            if (*cursor == ' ' || *cursor == '\t') {
                break;
            }
            if (*cursor == '\\' && cursor[1] != '\0') {
                cursor++;
            }
        }
        if (out + 1 < capacity) {
            destination[out++] = *cursor;
        }
        cursor++;
    }
    destination[out] = '\0';
    return cursor;
}

static void split_redirections(char *args, struct command_redirection *redirection) {
    char quote = '\0';
    char *read = args;
    char *write = args;
    init_redirection(redirection);
    while (*read != '\0') {
        if (quote != '\0') {
            if (*read == quote) {
                quote = '\0';
            } else if (*read == '\\' && read[1] != '\0') {
                *write++ = *read++;
            }
            *write++ = *read++;
            continue;
        }
        if (*read == '\'' || *read == '"') {
            quote = *read;
            *write++ = *read++;
            continue;
        }
        if (*read == '2' && read[1] == '>') {
            read += 2;
            redirection->stderr_append = 0;
            if (*read == '>') {
                redirection->stderr_append = 1;
                read++;
            }
            if (*read == '&' && read[1] == '1') {
                read += 2;
                redirection->stderr_set = 0;
                redirection->stderr_to_stdout = 1;
                redirection->stderr_path[0] = '\0';
                continue;
            }
            read = copy_redirection_path(read, redirection->stderr_path, sizeof(redirection->stderr_path));
            redirection->stderr_set = redirection->stderr_path[0] != '\0';
            redirection->stderr_to_stdout = 0;
            continue;
        }
        if (*read == '>') {
            read++;
            redirection->stdout_append = 0;
            if (*read == '>') {
                redirection->stdout_append = 1;
                read++;
            }
            read = copy_redirection_path(read, redirection->stdout_path, sizeof(redirection->stdout_path));
            redirection->stdout_set = redirection->stdout_path[0] != '\0';
            continue;
        }
        if (*read == '<') {
            read++;
            read = copy_redirection_path(read, redirection->stdin_path, sizeof(redirection->stdin_path));
            redirection->stdin_set = redirection->stdin_path[0] != '\0';
            continue;
        }
        *write++ = *read++;
    }
    *write = '\0';
}

static int split_pipeline_segments(char *line, char **segments, size_t capacity) {
    char quote = '\0';
    size_t count = 1;
    segments[0] = cli_trim(line);
    for (char *cursor = line; *cursor != '\0'; cursor++) {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
            } else if (*cursor == '\\' && cursor[1] != '\0') {
                cursor++;
            }
            continue;
        }
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor;
            continue;
        }
        if (*cursor == '|') {
            if (count >= capacity) {
                return -1;
            }
            *cursor = '\0';
            segments[count++] = cli_trim(cursor + 1);
        }
    }
    return (int)count;
}

static int prepare_external_command(char *line,
    char *path,
    size_t path_capacity,
    char **args_out,
    struct command_redirection *redirection,
    int allow_redirect) {
    char *command = cli_trim(line);
    char *args = find_argument_tail(command);

    init_redirection(redirection);
    if (*command == '\0' || *command == '#') {
        return 0;
    }
    if (*args != '\0') {
        *args++ = '\0';
        args = cli_trim(args);
    }
    split_redirections(args, redirection);
    if (!allow_redirect &&
        (redirection->stdin_set ||
            redirection->stdout_set ||
            redirection->stderr_set ||
            redirection->stderr_to_stdout)) {
        cli_puts("sh: pipeline redirection unsupported\n");
        return 0;
    }
    args = cli_trim(args);
    if (!resolve_command(path, path_capacity, command)) {
        print_script_context();
        cli_puts("sh: command not found: ");
        cli_puts(command);
        cli_puts(" (try 'path' or 'which ");
        cli_puts(command);
        cli_puts("')\n");
        return 0;
    }
    *args_out = args;
    return 1;
}

struct pipeline_command {
    char path[CLI_PATH_MAX];
    char *args;
    char args_expanded[ARG_EXPANDED_MAX];
    struct command_redirection redirection;
};

static void close_pipeline_fds(int pipes[][2], size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (pipes[i][0] >= 0) {
            srv_close(pipes[i][0]);
            pipes[i][0] = -1;
        }
        if (pipes[i][1] >= 0) {
            srv_close(pipes[i][1]);
            pipes[i][1] = -1;
        }
    }
}

static void wait_pipeline_pids(long *pids, size_t count, uint64_t *last_status) {
    uint64_t interrupted_status = 0;
    uint64_t final_status = 0;
    for (size_t i = 0; i < count; i++) {
        uint64_t status = 0;
        if (pids[i] >= 0) {
            (void)srv_wait((uint64_t)pids[i], &status, 0);
            if (status >= 128 && interrupted_status == 0) {
                interrupted_status = status;
            }
            final_status = status;
        }
    }
    if (last_status != 0) {
        *last_status = interrupted_status != 0 ? interrupted_status : final_status;
    }
}

static int open_redirection_input(const char *path) {
    long fd = srv_open(path);
    if (fd < 0) {
        print_script_context();
        cli_puts("sh: input redirect failed: ");
        cli_puts(path);
        cli_puts("\n");
    }
    return (int)fd;
}

static int open_redirection_output(const char *path, int append, const char *label) {
    uint64_t flags = SRV_OPEN_WRITE | SRV_OPEN_CREATE | (append ? SRV_OPEN_APPEND : SRV_OPEN_TRUNC);
    long fd = srv_open_mode(path, flags);
    if (fd < 0) {
        print_script_context();
        cli_puts("sh: ");
        cli_puts(label);
        cli_puts(" redirect failed: ");
        cli_puts(path);
        cli_puts("\n");
    }
    return (int)fd;
}

static void close_redirection_fds(int input_fd, int output_fd, int error_fd) {
    if (input_fd >= 0) {
        srv_close(input_fd);
    }
    if (output_fd >= 0) {
        srv_close(output_fd);
    }
    if (error_fd >= 0 && error_fd != output_fd) {
        srv_close(error_fd);
    }
}

static long exec_external_command(const char *path,
    const char *args,
    int background,
    int stdin_fd,
    int stdout_fd,
    int stderr_fd,
    uint64_t process_group,
    int foreground) {
    char args_copy[ARG_EXPANDED_MAX];
    char *argv[18];
    int argc;
    cli_copy(args_copy, sizeof(args_copy), args);
    argv[0] = (char *)path;
    argc = split_words(args_copy, argv + 1, 16);
    if (argc < 0) {
        cli_puts("sh: too many arguments\n");
        return -1;
    }
    argv[argc + 1] = 0;

    struct srv_exec_request request = {
        .path = path,
        .argv = argv,
        .envp = environ,
        .flags = background ? SRV_EXEC_BACKGROUND : 0,
        .stdin_fd = stdin_fd,
        .stdout_fd = stdout_fd,
        .stderr_fd = stderr_fd,
        .process_group = process_group,
        .foreground = foreground ? 1 : 0,
    };
    return srv_exec(&request);
}

static uint64_t exec_replace_command(const char *args,
    const struct command_redirection *redirection) {
    char args_copy[ARG_EXPANDED_MAX];
    char path[CLI_PATH_MAX];
    char *words[18];
    char *argv[18];
    int argc;
    int input_fd = -1;
    int output_fd = -1;
    int error_fd = -1;

    cli_copy(args_copy, sizeof(args_copy), args);
    argc = split_words(args_copy, words, sizeof(words) / sizeof(words[0]) - 1);
    if (argc < 0) {
        cli_puts("exec: too many arguments\n");
        return 2;
    }
    if (argc == 0) {
        cli_puts("usage: exec <command> [args]\n");
        return 2;
    }
    if (!resolve_command(path, sizeof(path), words[0])) {
        cli_puts("exec: command not found: ");
        cli_puts(words[0]);
        cli_puts("\n");
        return 127;
    }

    argv[0] = path;
    for (int i = 1; i < argc; i++) {
        argv[i] = words[i];
    }
    argv[argc] = 0;

    if (redirection->stdin_set) {
        input_fd = open_redirection_input(redirection->stdin_path);
        if (input_fd < 0) {
            return 1;
        }
    }
    if (redirection->stdout_set) {
        output_fd = open_redirection_output(redirection->stdout_path, redirection->stdout_append, "output");
        if (output_fd < 0) {
            close_redirection_fds(input_fd, -1, -1);
            return 1;
        }
    }
    if (redirection->stderr_set) {
        error_fd = open_redirection_output(redirection->stderr_path, redirection->stderr_append, "stderr");
        if (error_fd < 0) {
            close_redirection_fds(input_fd, output_fd, -1);
            return 1;
        }
    } else if (redirection->stderr_to_stdout) {
        error_fd = output_fd >= 0 ? output_fd : 1;
    }

    struct srv_exec_request request = {
        .path = path,
        .argv = argv,
        .envp = environ,
        .flags = SRV_EXEC_REPLACE,
        .stdin_fd = input_fd,
        .stdout_fd = output_fd,
        .stderr_fd = error_fd,
    };
    if (srv_exec(&request) < 0) {
        cli_puts("exec: failed\n");
        close_redirection_fds(input_fd, output_fd, error_fd);
        return 126;
    }
    return 126;
}

static uint64_t run_pipeline(char **segments, size_t segment_count, const char *cwd, int background, const char *command_line) {
    struct pipeline_command commands[PIPELINE_MAX_COMMANDS];
    int pipes[PIPELINE_MAX_COMMANDS - 1][2];
    long pids[PIPELINE_MAX_COMMANDS];
    uint64_t final_status = 0;
    int input_fd = -1;
    int output_fd = -1;
    int error_fd = -1;

    if (segment_count < 2 || segment_count > PIPELINE_MAX_COMMANDS) {
        cli_puts("sh: pipeline too long\n");
        return 2;
    }

    for (size_t i = 0; i < PIPELINE_MAX_COMMANDS; i++) {
        pids[i] = -1;
        if (i + 1 < PIPELINE_MAX_COMMANDS) {
            pipes[i][0] = -1;
            pipes[i][1] = -1;
        }
    }

    for (size_t i = 0; i < segment_count; i++) {
        if (!prepare_external_command(segments[i],
                commands[i].path,
                sizeof(commands[i].path),
                &commands[i].args,
                &commands[i].redirection,
                i == 0 || i + 1 == segment_count)) {
            return 127;
        }
        expand_globs(commands[i].args, cwd, commands[i].args_expanded, sizeof(commands[i].args_expanded));
        commands[i].args = commands[i].args_expanded;
    }

    if (commands[0].redirection.stdin_set) {
        input_fd = open_redirection_input(commands[0].redirection.stdin_path);
        if (input_fd < 0) {
            return 1;
        }
    }
    if (commands[segment_count - 1].redirection.stdout_set) {
        output_fd = open_redirection_output(commands[segment_count - 1].redirection.stdout_path,
            commands[segment_count - 1].redirection.stdout_append,
            "output");
        if (output_fd < 0) {
            if (input_fd >= 0) {
                srv_close(input_fd);
            }
            return 1;
        }
    }
    if (commands[segment_count - 1].redirection.stderr_set) {
        error_fd = open_redirection_output(commands[segment_count - 1].redirection.stderr_path,
            commands[segment_count - 1].redirection.stderr_append,
            "stderr");
        if (error_fd < 0) {
            close_redirection_fds(input_fd, output_fd, -1);
            return 1;
        }
    } else if (commands[segment_count - 1].redirection.stderr_to_stdout) {
        error_fd = output_fd >= 0 ? output_fd : 1;
    }

    for (size_t i = 0; i + 1 < segment_count; i++) {
        if (srv_pipe(pipes[i]) < 0) {
            cli_puts("sh: pipe failed\n");
            close_pipeline_fds(pipes, segment_count - 1);
            close_redirection_fds(input_fd, output_fd, error_fd);
            return 1;
        }
    }

    for (size_t i = 0; i < segment_count; i++) {
        int stdin_fd = i == 0 ? input_fd : pipes[i - 1][0];
        int stdout_fd = i + 1 == segment_count ? output_fd : pipes[i][1];
        int stderr_fd = i + 1 == segment_count ? error_fd : -1;
        uint64_t group = i == 0 ? SRV_EXEC_GROUP_SELF : (uint64_t)pids[0];
        pids[i] = exec_external_command(commands[i].path,
            commands[i].args,
            1,
            stdin_fd,
            stdout_fd,
            stderr_fd,
            group,
            background ? 0 : 1);
        if (pids[i] < 0) {
            cli_puts("sh: pipeline spawn failed: ");
            cli_puts(commands[i].path);
            cli_puts("\n");
            close_pipeline_fds(pipes, segment_count - 1);
            close_redirection_fds(input_fd, output_fd, error_fd);
            wait_pipeline_pids(pids, i, 0);
            (void)srv_proc_group(0, 0, 1);
            return 126;
        }
    }

    close_pipeline_fds(pipes, segment_count - 1);
    close_redirection_fds(input_fd, output_fd, error_fd);
    if (background) {
        uint64_t group = pids[0] >= 0 ? (uint64_t)pids[0] : 0;
        struct shell_job *job = add_job(group, pids, segment_count, command_line);
        last_background_pid = group;
        cli_puts("[bg] pid ");
        cli_putn(group);
        if (job != 0) {
            cli_puts(" job ");
            cli_putn(job->id);
        }
        cli_puts("\n");
        return 0;
    }
    wait_pipeline_pids(pids, segment_count, &final_status);
    (void)srv_proc_group(0, 0, 1);
    cli_puts("status ");
    cli_putn(final_status);
    cli_puts("\n");
    return final_status;
}

static void echo_text(const char *text, const char *redirect_path, int append) {
    char buffer[LINE_MAX];
    size_t out = 0;
    for (size_t i = 0; text[i] != '\0' && out + 1 < sizeof(buffer); i++) {
        if (text[i] == '\'' || text[i] == '"') {
            continue;
        }
        if (text[i] == '\\' && text[i + 1] != '\0') {
            i++;
        }
        buffer[out++] = text[i];
    }
    buffer[out++] = '\n';
    if (redirect_path != 0 && redirect_path[0] != '\0') {
        long result = append ?
            srv_fs_append(redirect_path, buffer, out) :
            srv_fs_write(redirect_path, buffer, out);
        if (result < 0) {
            cli_puts("echo: redirect failed\n");
        }
        return;
    }
    srv_write(SRV_STDOUT, buffer, out);
}

static void build_prompt(char *out, size_t capacity, const char *cwd) {
    const char *pattern = getenv("PS1");
    size_t length = 0;
    if (pattern == 0 || pattern[0] == '\0') {
        pattern = "\\w $ ";
    }
    out[0] = '\0';
    for (size_t i = 0; pattern[i] != '\0'; i++) {
        if (pattern[i] == '\\' && pattern[i + 1] != '\0') {
            i++;
            if (pattern[i] == 'w') {
                append_text(out, capacity, &length, cwd);
            } else if (pattern[i] == '$') {
                append_char(out, capacity, &length, '$');
            } else if (pattern[i] == '\\') {
                append_char(out, capacity, &length, '\\');
            } else {
                append_char(out, capacity, &length, pattern[i]);
            }
            continue;
        }
        append_char(out, capacity, &length, pattern[i]);
    }
}

static uint64_t run_command_impl(char *line, char *cwd, int background, int bypass_alias_functions) {
    char *command = cli_trim(line);
    char *args = command;
    char *pipeline_segments[PIPELINE_MAX_COMMANDS];
    int pipeline_count;
    char command_text[LINE_MAX];
    char path[CLI_PATH_MAX];
    struct command_redirection redirection;
    char expanded_args[ARG_EXPANDED_MAX];
    long status;
    int input_fd = -1;
    int output_fd = -1;
    int error_fd = -1;

    if (*command == '\0' || *command == '#') {
        return last_status;
    }
    struct temporary_assignment assignments[8];
    char *assignment_rest = command;
    size_t assignment_count = collect_leading_assignments(command,
        assignments,
        sizeof(assignments) / sizeof(assignments[0]),
        &assignment_rest);
    if (assignment_count != 0) {
        uint64_t assignment_status;
        assignment_rest = cli_trim(assignment_rest);
        if (assignment_rest[0] == '\0') {
            for (size_t i = 0; i < assignment_count; i++) {
                if (!apply_decoded_assignment(assignments[i].name, assignments[i].value)) {
                    cli_puts("sh: bad assignment\n");
                    return 2;
                }
            }
            return 0;
        }
        if (!apply_temporary_assignments(assignments, assignment_count)) {
            cli_puts("sh: bad assignment\n");
            restore_temporary_assignments(assignments, assignment_count);
            return 2;
        }
        assignment_status = run_command_impl(assignment_rest, cwd, background, bypass_alias_functions);
        restore_temporary_assignments(assignments, assignment_count);
        return assignment_status;
    }
    cli_copy(command_text, sizeof(command_text), command);

    pipeline_count = split_pipeline_segments(command, pipeline_segments, PIPELINE_MAX_COMMANDS);
    if (pipeline_count < 0) {
        cli_puts("sh: pipeline too long\n");
        return 2;
    }
    if (pipeline_count > 1) {
        return run_pipeline(pipeline_segments, (size_t)pipeline_count, cwd, background, command_text);
    }

    args = find_argument_tail(command);
    if (*args != '\0') {
        *args++ = '\0';
        args = cli_trim(args);
    }
    struct shell_alias *alias = bypass_alias_functions ? 0 : find_alias(command);
    if (alias != 0) {
        char alias_line[EXPANDED_LINE_MAX];
        size_t alias_length = 0;
        alias_line[0] = '\0';
        append_text(alias_line, sizeof(alias_line), &alias_length, alias->value);
        if (args[0] != '\0') {
            append_char(alias_line, sizeof(alias_line), &alias_length, ' ');
            append_text(alias_line, sizeof(alias_line), &alias_length, args);
        }
        return run_command_impl(alias_line, cwd, background, 0);
    }
    struct shell_function *function = bypass_alias_functions ? 0 : find_function(command);
    if (function != 0) {
        if (background) {
            cli_puts("function: background unsupported\n");
            return 2;
        }
        return run_function(function, args, cwd);
    }
    char *equals = strchr(command, '=');
    if (equals != 0 && args[0] == '\0') {
        *equals++ = '\0';
        if (!apply_assignment(command, equals)) {
            cli_puts("sh: bad assignment\n");
            return 2;
        }
        return 0;
    }
    split_redirections(args, &redirection);
    args = cli_trim(args);
    expand_globs(args, cwd, expanded_args, sizeof(expanded_args));
    args = expanded_args;

    if (cli_streq(command, "help")) {
        print_help_topic(args);
        return 0;
    }
    if (cli_streq(command, "man")) {
        if (args[0] == '\0' || cli_is_help_arg(args)) {
            cli_puts("usage: man <topic>\n");
            return args[0] == '\0' ? 2 : 0;
        }
        print_help_topic(args);
        return 0;
    }
    if (cli_streq(command, "apropos")) {
        return apropos_command(args);
    }
    if (cli_streq(command, "exit")) {
        srv_exit(args[0] != '\0' ? (int)parse_u64(args) : (int)last_status);
    }
    if (cli_streq(command, "exec")) {
        if (background) {
            cli_puts("exec: background unsupported\n");
            return 2;
        }
        return exec_replace_command(args, &redirection);
    }
    if (cli_streq(command, "return")) {
        return return_command(args);
    }
    if (cli_streq(command, "shift")) {
        return shift_command(args);
    }
    if (cli_streq(command, "set")) {
        return set_command(args);
    }
    if (cli_streq(command, ":")) {
        return 0;
    }
    if (cli_streq(command, "path")) {
        if (cli_starts_with(args, "add ")) {
            return add_path(cli_trim(args + 4));
        } else {
            print_path();
            return 0;
        }
    }
    if (cli_streq(command, "env")) {
        if (cli_is_help_arg(args)) {
            cli_puts("usage: env [-i] [-u NAME] [NAME=value ...] [command [arg ...]]\n");
            return 0;
        }
        if (args[0] != '\0') {
            char env_line[EXPANDED_LINE_MAX];
            size_t env_length = 0;
            env_line[0] = '\0';
            append_text(env_line, sizeof(env_line), &env_length, "/fat/bin/env");
            append_char(env_line, sizeof(env_line), &env_length, ' ');
            append_text(env_line, sizeof(env_line), &env_length, args);
            return run_command_impl(env_line, cwd, background, 1);
        }
        print_env();
        return 0;
    }
    if (cli_streq(command, "export")) {
        return export_command(args);
    }
    if (cli_streq(command, "unset")) {
        return unset_command(args);
    }
    if (cli_streq(command, "alias")) {
        return alias_command(args);
    }
    if (cli_streq(command, "history")) {
        return history_command(args);
    }
    if (cli_streq(command, "type")) {
        return type_command(args);
    }
    if (cli_streq(command, "which")) {
        return which_command(args);
    }
    if (cli_streq(command, "command")) {
        return command_command(args, cwd, background);
    }
    if (cli_streq(command, "test")) {
        return test_command(args, 0);
    }
    if (cli_streq(command, "[")) {
        return test_command(args, 1);
    }
    if (cli_streq(command, "break")) {
        return loop_control_command(args, 0);
    }
    if (cli_streq(command, "continue")) {
        return loop_control_command(args, 1);
    }
    if (cli_streq(command, "source") || cli_streq(command, ".")) {
        if (args[0] == '\0') {
            cli_puts("usage: source <file>\n");
            return 2;
        } else {
            return run_script(args, cwd);
        }
    }
    if (cli_streq(command, "pwd")) {
        if (cli_is_help_arg(args)) {
            cli_puts("usage: pwd\n");
            return 0;
        }
        cli_puts(cwd);
        cli_puts("\n");
        return 0;
    }
    if (cli_streq(command, "cd")) {
        char next[CLI_PATH_MAX];
        struct srv_stat info;
        const char *target = args[0] != '\0' ? args : "/";
        if (cli_streq(target, "-")) {
            target = getenv("OLDPWD");
            if (target == 0 || target[0] == '\0') {
                cli_puts("cd: OLDPWD not set\n");
                return 1;
            }
        }
        cli_normalize_path(next, sizeof(next), cwd, target);
        if (!cli_streq(next, "/") && (srv_stat(next, &info) < 0 || info.type != 1)) {
            cli_puts("cd: not a directory: ");
            cli_puts(next);
            cli_puts("\n");
            return 1;
        }
        setenv("OLDPWD", cwd, 1);
        cli_copy(cwd, CLI_PATH_MAX, next);
        setenv("PWD", cwd, 1);
        if (cli_streq(args, "-")) {
            cli_puts(cwd);
            cli_puts("\n");
        }
        return 0;
    }
    if (cli_streq(command, "clear")) {
        clrscr();
        return 0;
    }
    if (cli_streq(command, "echo")) {
        echo_text(args,
            redirection.stdout_set ? redirection.stdout_path : 0,
            redirection.stdout_append);
        return 0;
    }
    if (cli_streq(command, "jobs")) {
        print_jobs(args);
        return 0;
    }
    if (cli_streq(command, "wait")) {
        return wait_for_job(args);
    }
    if (cli_streq(command, "fg")) {
        return fg_command(args);
    }
    if (cli_streq(command, "bg")) {
        return bg_command(args);
    }
    if (cli_streq(command, "kill")) {
        return kill_command(args);
    }
    if (cli_streq(command, "service")) {
        service_command(args);
        return 0;
    }
    if (cli_streq(command, "dhcp")) {
        dhcp_command();
        return 0;
    }
    if (cli_streq(command, "net")) {
        net_command();
        return 0;
    }
    if (cli_streq(command, "dns")) {
        dns_command(args);
        return 0;
    }
    if (cli_streq(command, "rmdir")) {
        rmdir_command(args);
        return 0;
    }
    if (cli_streq(command, "read")) {
        return read_command(args);
    }
    if (!resolve_command(path, sizeof(path), command)) {
        print_script_context();
        cli_puts("sh: command not found: ");
        cli_puts(command);
        cli_puts(" (try 'path' or 'which ");
        cli_puts(command);
        cli_puts("')\n");
        return 127;
    }

    if (redirection.stdin_set) {
        input_fd = open_redirection_input(redirection.stdin_path);
        if (input_fd < 0) {
            return 1;
        }
    }
    if (redirection.stdout_set) {
        output_fd = open_redirection_output(redirection.stdout_path, redirection.stdout_append, "output");
        if (output_fd < 0) {
            if (input_fd >= 0) {
                srv_close(input_fd);
            }
            return 1;
        }
    }
    if (redirection.stderr_set) {
        error_fd = open_redirection_output(redirection.stderr_path, redirection.stderr_append, "stderr");
        if (error_fd < 0) {
            close_redirection_fds(input_fd, output_fd, -1);
            return 1;
        }
    } else if (redirection.stderr_to_stdout) {
        error_fd = output_fd >= 0 ? output_fd : 1;
    }

    if (background) {
        status = exec_external_command(path, args, 1, input_fd, output_fd, error_fd, 0, 0);
        if (status < 0) {
            cli_puts("sh: background spawn failed\n");
            close_redirection_fds(input_fd, output_fd, error_fd);
            return 126;
        } else {
            long pids[1];
            pids[0] = status;
            last_background_pid = (uint64_t)status;
            (void)add_job((uint64_t)status, pids, 1, command_text);
            cli_puts("[bg] pid ");
            cli_putn((uint64_t)status);
            cli_puts("\n");
        }
        close_redirection_fds(input_fd, output_fd, error_fd);
        return 0;
    }

    status = exec_external_command(path, args, 0, input_fd, output_fd, error_fd, 0, 0);
    close_redirection_fds(input_fd, output_fd, error_fd);
    if (status < 0) {
        cli_puts("sh: exec failed\n");
        return 126;
    }
    cli_puts("status ");
    cli_putn((uint64_t)status);
    cli_puts("\n");
    return (uint64_t)status;
}

static uint64_t run_command(char *line, char *cwd, int background) {
    return run_command_impl(line, cwd, background, 0);
}

enum shell_control {
    SHELL_CONTROL_ALWAYS,
    SHELL_CONTROL_AND,
    SHELL_CONTROL_OR,
};

static int find_if_parts(const char *line, struct if_parts *parts) {
    char quote = '\0';
    int depth = 1;
    int saw_then = 0;
    size_t then_keyword = 0;
    size_t else_keyword = 0;

    parts->condition_start = 2;
    parts->condition_end = 0;
    parts->then_start = 0;
    parts->then_end = 0;
    parts->else_start = 0;
    parts->else_end = 0;
    parts->command_end = 0;
    parts->has_else = 0;

    for (size_t i = 2; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "if")) {
            depth++;
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "then") && depth == 1 && !saw_then) {
            then_keyword = i;
            parts->condition_end = i;
            parts->then_start = i + 4;
            saw_then = 1;
            i += 3;
            continue;
        }
        if (shell_keyword_at(line, i, "else") && depth == 1 && saw_then && !parts->has_else) {
            else_keyword = i;
            parts->then_end = i;
            parts->else_start = i + 4;
            parts->has_else = 1;
            i += 3;
            continue;
        }
        if (shell_keyword_at(line, i, "fi")) {
            depth--;
            if (depth == 0) {
                if (!saw_then) {
                    return 0;
                }
                if (parts->has_else) {
                    parts->else_end = i;
                } else {
                    parts->then_end = i;
                }
                parts->command_end = i + 2;
                (void)then_keyword;
                (void)else_keyword;
                return 1;
            }
            i++;
        }
    }
    return 0;
}

static void copy_slice_trimmed(char *destination, size_t capacity, const char *source, size_t start, size_t end) {
    size_t out = 0;
    while (start < end && (source[start] == ' ' || source[start] == '\t' || source[start] == ';')) {
        start++;
    }
    while (end > start && (source[end - 1] == ' ' || source[end - 1] == '\t' || source[end - 1] == ';')) {
        end--;
    }
    while (start < end && out + 1 < capacity) {
        destination[out++] = source[start++];
    }
    destination[out] = '\0';
}

static void decode_case_pattern(char *pattern) {
    char *argv[2];
    char copy[128];
    int argc;

    cli_copy(copy, sizeof(copy), pattern);
    argc = split_words(copy, argv, 1);
    if (argc == 1) {
        cli_copy(pattern, 128, argv[0]);
    }
}

static int case_patterns_match(const char *patterns, const char *word) {
    size_t start = 0;
    char quote = '\0';

    for (size_t i = 0;; i++) {
        char c = patterns[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && patterns[i + 1] != '\0') {
                i++;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '\\' && patterns[i + 1] != '\0') {
            i++;
        } else if (c == '|' || c == '\0') {
            char pattern[128];
            copy_slice_trimmed(pattern, sizeof(pattern), patterns, start, i);
            decode_case_pattern(pattern);
            if (pattern[0] != '\0' && glob_match(pattern, word)) {
                return 1;
            }
            if (c == '\0') {
                return 0;
            }
            start = i + 1;
        }
        if (c == '\0') {
            return 0;
        }
    }
}

static uint64_t run_if_line(char *line, char *cwd) {
    struct if_parts parts;
    char condition[EXPANDED_LINE_MAX];
    char then_branch[EXPANDED_LINE_MAX];
    char else_branch[EXPANDED_LINE_MAX];
    uint64_t condition_status;

    if (!find_if_parts(line, &parts)) {
        cli_puts("if: expected 'if <command>; then <commands>; [else <commands>;] fi'\n");
        return 2;
    }

    copy_slice_trimmed(condition, sizeof(condition), line, parts.condition_start, parts.condition_end);
    copy_slice_trimmed(then_branch, sizeof(then_branch), line, parts.then_start, parts.then_end);
    if (parts.has_else) {
        copy_slice_trimmed(else_branch, sizeof(else_branch), line, parts.else_start, parts.else_end);
    } else {
        else_branch[0] = '\0';
    }

    condition_status = run_line(condition, cwd);
    if (condition_status == 0) {
        return then_branch[0] != '\0' ? run_line(then_branch, cwd) : 0;
    }
    return else_branch[0] != '\0' ? run_line(else_branch, cwd) : 0;
}

static int is_function_start(const char *line) {
    size_t cursor = 0;
    if (!shell_is_name_start(line[cursor])) {
        return 0;
    }
    while (shell_is_name_char(line[cursor])) {
        cursor++;
    }
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    if (line[cursor++] != '(') {
        return 0;
    }
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    if (line[cursor++] != ')') {
        return 0;
    }
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    return line[cursor] == '{';
}

static int find_function_definition(const char *line,
    char *name,
    size_t name_capacity,
    char *body,
    size_t body_capacity) {
    size_t cursor = 0;
    size_t name_length = 0;
    size_t body_start = 0;
    size_t body_end = 0;
    char quote = '\0';
    int found_end = 0;

    if (!shell_is_name_start(line[cursor])) {
        return 0;
    }
    while (shell_is_name_char(line[cursor])) {
        if (name_length + 1 < name_capacity) {
            name[name_length++] = line[cursor];
        }
        cursor++;
    }
    name[name_length] = '\0';
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    if (line[cursor++] != '(') {
        return 0;
    }
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    if (line[cursor++] != ')') {
        return 0;
    }
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    if (line[cursor++] != '{') {
        return 0;
    }
    body_start = cursor;
    for (size_t i = cursor; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == '}') {
            body_end = i;
            found_end = 1;
        }
    }
    if (!found_end) {
        return 0;
    }
    copy_slice_trimmed(body, body_capacity, line, body_start, body_end);
    return name[0] != '\0';
}

static int find_while_parts(const char *line, struct while_parts *parts) {
    char quote = '\0';
    int depth = 1;
    int saw_do = 0;

    parts->condition_start = 5;
    parts->condition_end = 0;
    parts->body_start = 0;
    parts->body_end = 0;
    parts->command_end = 0;

    for (size_t i = 5; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "while") || shell_keyword_at(line, i, "for")) {
            depth++;
            continue;
        }
        if (shell_keyword_at(line, i, "do") && depth == 1 && !saw_do) {
            saw_do = 1;
            parts->condition_end = i;
            parts->body_start = i + 2;
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "done")) {
            depth--;
            if (depth == 0) {
                if (!saw_do) {
                    return 0;
                }
                parts->body_end = i;
                parts->command_end = i + 4;
                return 1;
            }
            i += 3;
        }
    }
    return 0;
}

static int find_for_parts(const char *line, struct for_parts *parts) {
    char quote = '\0';
    int depth = 1;
    int saw_in = 0;
    int saw_do = 0;

    parts->name_start = 0;
    parts->name_end = 0;
    parts->words_start = 0;
    parts->words_end = 0;
    parts->body_start = 0;
    parts->body_end = 0;
    parts->command_end = 0;

    size_t cursor = 3;
    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }
    parts->name_start = cursor;
    if (!shell_is_name_start(line[cursor])) {
        return 0;
    }
    while (shell_is_name_char(line[cursor])) {
        cursor++;
    }
    parts->name_end = cursor;

    for (size_t i = cursor; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "for")) {
            depth++;
            i += 2;
            continue;
        }
        if (shell_keyword_at(line, i, "in") && depth == 1 && !saw_in) {
            saw_in = 1;
            parts->words_start = i + 2;
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "do") && depth == 1 && saw_in && !saw_do) {
            saw_do = 1;
            parts->words_end = i;
            parts->body_start = i + 2;
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "done")) {
            depth--;
            if (depth == 0) {
                if (!saw_do) {
                    return 0;
                }
                parts->body_end = i;
                parts->command_end = i + 4;
                return 1;
            }
            i += 3;
        }
    }
    return 0;
}

static int find_case_parts(const char *line, struct case_parts *parts) {
    char quote = '\0';
    int depth = 1;
    int saw_in = 0;

    parts->word_start = 4;
    parts->word_end = 0;
    parts->arms_start = 0;
    parts->arms_end = 0;
    parts->command_end = 0;

    for (size_t i = 4; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (saw_in && shell_keyword_at(line, i, "case")) {
            depth++;
            i += 3;
            continue;
        }
        if (shell_keyword_at(line, i, "in") && depth == 1 && !saw_in) {
            saw_in = 1;
            parts->word_end = i;
            parts->arms_start = i + 2;
            i++;
            continue;
        }
        if (shell_keyword_at(line, i, "esac")) {
            depth--;
            if (depth == 0) {
                if (!saw_in) {
                    return 0;
                }
                parts->arms_end = i;
                parts->command_end = i + 4;
                return 1;
            }
            i += 3;
        }
    }
    return 0;
}

static int find_group_parts(const char *line, size_t *body_start, size_t *body_end, size_t *command_end) {
    char quote = '\0';
    size_t start = 1;
    if (line[0] != '{') {
        return 0;
    }
    while (line[start] == ' ' || line[start] == '\t') {
        start++;
    }
    for (size_t i = start; line[i] != '\0'; i++) {
        char c = line[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && line[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && line[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == '}' && shell_is_separator(line[i + 1])) {
            size_t end = i;
            while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == ';')) {
                end--;
            }
            *body_start = start;
            *body_end = end;
            *command_end = i + 1;
            return 1;
        }
    }
    return 0;
}

static int find_case_arm_end(const char *arms, size_t start, size_t *pattern_end, size_t *command_end, size_t *next) {
    char quote = '\0';
    int found_pattern = 0;

    for (size_t i = start; arms[i] != '\0'; i++) {
        char c = arms[i];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && arms[i + 1] != '\0') {
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && arms[i + 1] != '\0') {
            i++;
            continue;
        }
        if (c == ')' && !found_pattern) {
            *pattern_end = i;
            found_pattern = 1;
            continue;
        }
        if (found_pattern && c == ';' && arms[i + 1] == ';') {
            *command_end = i;
            *next = i + 2;
            return 1;
        }
    }
    if (found_pattern) {
        *command_end = cli_strlen(arms);
        *next = *command_end;
        return 1;
    }
    return 0;
}

static uint64_t run_case_line(char *line, char *cwd) {
    struct case_parts parts;
    char word[ARG_EXPANDED_MAX];
    char expanded_word[ARG_EXPANDED_MAX];
    char word_copy[ARG_EXPANDED_MAX];
    char arms[EXPANDED_LINE_MAX];
    char *argv[2];
    int argc;
    uint64_t status = 0;

    if (!find_case_parts(line, &parts)) {
        cli_puts("case: expected 'case <word> in <pattern>) <commands> ;; esac'\n");
        return 2;
    }
    copy_slice_trimmed(word, sizeof(word), line, parts.word_start, parts.word_end);
    copy_slice_trimmed(arms, sizeof(arms), line, parts.arms_start, parts.arms_end);
    if (word[0] == '\0' || arms[0] == '\0') {
        return 0;
    }
    if (!expand_variables(word, expanded_word, sizeof(expanded_word), cwd)) {
        return 2;
    }
    cli_copy(word_copy, sizeof(word_copy), expanded_word);
    argc = split_words(word_copy, argv, 1);
    if (argc != 1) {
        cli_puts("case: word expands to invalid field count\n");
        return 2;
    }

    for (size_t cursor = 0; arms[cursor] != '\0';) {
        size_t pattern_start;
        size_t pattern_end = 0;
        size_t command_start;
        size_t command_end = 0;
        size_t next = 0;
        char patterns[ARG_EXPANDED_MAX];
        char expanded_patterns[ARG_EXPANDED_MAX];

        while (arms[cursor] == ' ' || arms[cursor] == '\t' || arms[cursor] == ';') {
            cursor++;
        }
        if (arms[cursor] == '\0') {
            break;
        }
        if (arms[cursor] == '(') {
            cursor++;
        }
        pattern_start = cursor;
        if (!find_case_arm_end(arms, pattern_start, &pattern_end, &command_end, &next)) {
            cli_puts("case: malformed pattern arm\n");
            return 2;
        }
        command_start = pattern_end + 1;
        copy_slice_trimmed(patterns, sizeof(patterns), arms, pattern_start, pattern_end);
        if (!expand_variables(patterns, expanded_patterns, sizeof(expanded_patterns), cwd)) {
            return 2;
        }
        if (case_patterns_match(expanded_patterns, argv[0])) {
            char commands[EXPANDED_LINE_MAX];
            copy_slice_trimmed(commands, sizeof(commands), arms, command_start, command_end);
            if (commands[0] != '\0') {
                status = run_line(commands, cwd);
            }
            return status;
        }
        cursor = next;
    }
    return 0;
}

static uint64_t run_for_line(char *line, char *cwd) {
    struct for_parts parts;
    char name[32];
    char words[EXPANDED_LINE_MAX];
    char expanded_words[EXPANDED_LINE_MAX];
    char globbed_words[EXPANDED_LINE_MAX];
    char body[EXPANDED_LINE_MAX];
    char words_copy[EXPANDED_LINE_MAX];
    char *argv[32];
    int argc;
    uint64_t status = 0;

    if (!find_for_parts(line, &parts)) {
        cli_puts("for: expected 'for name in words; do commands; done'\n");
        return 2;
    }

    copy_slice_trimmed(name, sizeof(name), line, parts.name_start, parts.name_end);
    copy_slice_trimmed(words, sizeof(words), line, parts.words_start, parts.words_end);
    copy_slice_trimmed(body, sizeof(body), line, parts.body_start, parts.body_end);
    if (name[0] == '\0' || body[0] == '\0') {
        return 0;
    }
    if (!expand_variables(words, expanded_words, sizeof(expanded_words), cwd)) {
        return 2;
    }
    expand_globs(expanded_words, cwd, globbed_words, sizeof(globbed_words));
    cli_copy(words_copy, sizeof(words_copy), globbed_words);
    argc = split_words(words_copy, argv, sizeof(argv) / sizeof(argv[0]));
    if (argc < 0) {
        cli_puts("for: too many words\n");
        return 2;
    }
    loop_depth++;
    for (int i = 0; i < argc; i++) {
        char body_copy[EXPANDED_LINE_MAX];
        setenv(name, argv[i], 1);
        cli_copy(body_copy, sizeof(body_copy), body);
        status = run_line(body_copy, cwd);
        if (return_requested) {
            break;
        }
        if (break_requested != 0) {
            if (break_requested > 1) {
                break_requested--;
            } else {
                break_requested = 0;
            }
            status = 0;
            break;
        }
        if (continue_requested != 0) {
            if (continue_requested > 1) {
                continue_requested--;
                status = 0;
                break;
            }
            continue_requested = 0;
            status = 0;
            continue;
        }
        if (exit_on_error && status != 0) {
            break;
        }
    }
    loop_depth--;
    return status;
}

static uint64_t run_while_line(char *line, char *cwd) {
    struct while_parts parts;
    char condition[EXPANDED_LINE_MAX];
    char body[EXPANDED_LINE_MAX];
    uint64_t status = 0;

    if (!find_while_parts(line, &parts)) {
        cli_puts("while: expected 'while <command>; do <commands>; done'\n");
        return 2;
    }

    copy_slice_trimmed(condition, sizeof(condition), line, parts.condition_start, parts.condition_end);
    copy_slice_trimmed(body, sizeof(body), line, parts.body_start, parts.body_end);
    if (condition[0] == '\0' || body[0] == '\0') {
        return 0;
    }
    loop_depth++;
    for (;;) {
        char condition_copy[EXPANDED_LINE_MAX];
        char body_copy[EXPANDED_LINE_MAX];
        cli_copy(condition_copy, sizeof(condition_copy), condition);
        status = run_line(condition_copy, cwd);
        if (status != 0 || return_requested) {
            status = return_requested ? status : 0;
            break;
        }
        if (break_requested != 0) {
            if (break_requested > 1) {
                break_requested--;
            } else {
                break_requested = 0;
            }
            status = 0;
            break;
        }
        if (continue_requested != 0) {
            if (continue_requested > 1) {
                continue_requested--;
                status = 0;
                break;
            }
            continue_requested = 0;
            status = 0;
            continue;
        }
        cli_copy(body_copy, sizeof(body_copy), body);
        status = run_line(body_copy, cwd);
        if (return_requested) {
            break;
        }
        if (break_requested != 0) {
            if (break_requested > 1) {
                break_requested--;
            } else {
                break_requested = 0;
            }
            status = 0;
            break;
        }
        if (continue_requested != 0) {
            if (continue_requested > 1) {
                continue_requested--;
                status = 0;
                break;
            }
            continue_requested = 0;
            status = 0;
            continue;
        }
        if (exit_on_error && status != 0) {
            break;
        }
    }
    loop_depth--;
    return status;
}

static uint64_t run_group_line(char *line, char *cwd, size_t *command_end) {
    size_t body_start = 0;
    size_t body_end = 0;
    char body[EXPANDED_LINE_MAX];
    if (!find_group_parts(line, &body_start, &body_end, command_end)) {
        cli_puts("sh: expected '{ commands; }'\n");
        return 2;
    }
    copy_slice_trimmed(body, sizeof(body), line, body_start, body_end);
    return body[0] != '\0' ? run_line(body, cwd) : 0;
}

static uint64_t run_compound_tail(uint64_t status, char *line, size_t command_end, char *cwd) {
    char *tail = line + command_end;
    while (*tail == ' ' || *tail == '\t') {
        tail++;
    }
    if (*tail == '\0' || shell_flow_requested()) {
        return status;
    }
    if (*tail == ';') {
        tail++;
        tail = cli_trim(tail);
        return *tail != '\0' ? run_line(tail, cwd) : status;
    }
    if (tail[0] == '&' && tail[1] == '&') {
        tail += 2;
        tail = cli_trim(tail);
        if (status == 0 && *tail != '\0') {
            return run_line(tail, cwd);
        }
        return status;
    }
    if (tail[0] == '|' && tail[1] == '|') {
        tail += 2;
        tail = cli_trim(tail);
        if (status != 0 && *tail != '\0') {
            return run_line(tail, cwd);
        }
        return status;
    }
    cli_puts("sh: expected separator after compound command\n");
    return 2;
}

static uint64_t run_line(char *line, char *cwd) {
    char quote = '\0';
    char *segment = line;
    enum shell_control control = SHELL_CONTROL_ALWAYS;
    uint64_t status = last_status;
    char *trimmed = cli_trim(line);
    char function_name[32];
    char function_body[EXPANDED_LINE_MAX];

    if (return_requested) {
        return return_status;
    }
    if (break_requested != 0 || continue_requested != 0) {
        return last_status;
    }
    if (trimmed != line) {
        line = trimmed;
        segment = line;
    }
    if (line[0] == '{' && shell_is_separator(line[1])) {
        size_t command_end = 0;
        status = run_group_line(line, cwd, &command_end);
        status = run_compound_tail(status, line, command_end, cwd);
        last_status = status;
        return status;
    }
    if (find_function_definition(line, function_name, sizeof(function_name), function_body, sizeof(function_body))) {
        status = define_function(function_name, function_body);
        last_status = status;
        return status;
    }
    if (shell_keyword_at(line, 0, "if")) {
        struct if_parts parts;
        status = run_if_line(line, cwd);
        if (find_if_parts(line, &parts)) {
            status = run_compound_tail(status, line, parts.command_end, cwd);
        }
        last_status = status;
        return status;
    }
    if (shell_keyword_at(line, 0, "for")) {
        struct for_parts parts;
        status = run_for_line(line, cwd);
        if (find_for_parts(line, &parts)) {
            status = run_compound_tail(status, line, parts.command_end, cwd);
        }
        last_status = status;
        return status;
    }
    if (shell_keyword_at(line, 0, "while")) {
        struct while_parts parts;
        status = run_while_line(line, cwd);
        if (find_while_parts(line, &parts)) {
            status = run_compound_tail(status, line, parts.command_end, cwd);
        }
        last_status = status;
        return status;
    }
    if (shell_keyword_at(line, 0, "case")) {
        struct case_parts parts;
        status = run_case_line(line, cwd);
        if (find_case_parts(line, &parts)) {
            status = run_compound_tail(status, line, parts.command_end, cwd);
        }
        last_status = status;
        return status;
    }

    for (char *cursor = line; ; cursor++) {
        char c = *cursor;
        if (c == '\0') {
            if (quote != '\0') {
                shell_error("sh: unmatched quote\n");
                last_status = 2;
                return 2;
            }
            int should_run = control == SHELL_CONTROL_ALWAYS ||
                (control == SHELL_CONTROL_AND && status == 0) ||
                (control == SHELL_CONTROL_OR && status != 0);
            if (should_run) {
                char expanded[EXPANDED_LINE_MAX];
                if (!expand_variables(segment, expanded, sizeof(expanded), cwd)) {
                    status = 2;
                } else {
                    status = run_command(expanded, cwd, 0);
                }
                last_status = status;
                if (shell_flow_requested()) {
                    return status;
                }
            }
            return status;
        }
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && cursor[1] != '\0') {
                cursor++;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '#' && (cursor == segment || cursor[-1] == ' ' || cursor[-1] == '\t')) {
            *cursor = '\0';
            char expanded[EXPANDED_LINE_MAX];
            if (!expand_variables(segment, expanded, sizeof(expanded), cwd)) {
                status = 2;
            } else {
                int should_run = control == SHELL_CONTROL_ALWAYS ||
                    (control == SHELL_CONTROL_AND && status == 0) ||
                    (control == SHELL_CONTROL_OR && status != 0);
                if (should_run) {
                    status = run_command(expanded, cwd, 0);
                }
            }
            last_status = status;
            if (shell_flow_requested()) {
                return status;
            }
            return status;
        } else if (c == '&' &&
            cursor > line + 1 &&
            cursor[-1] == '>' &&
            cursor[-2] == '2' &&
            cursor[1] == '1') {
            continue;
        } else if (c == ';' || c == '&' || (c == '|' && cursor[1] == '|')) {
            char *delimiter = cursor;
            int background = c == '&';
            enum shell_control next_control = SHELL_CONTROL_ALWAYS;
            int should_run;
            if (c == '&' && cursor[1] == '&') {
                background = 0;
                next_control = SHELL_CONTROL_AND;
                cursor++;
            } else if (c == '|' && cursor[1] == '|') {
                background = 0;
                next_control = SHELL_CONTROL_OR;
                cursor++;
            }
            *delimiter = '\0';
            should_run = control == SHELL_CONTROL_ALWAYS ||
                (control == SHELL_CONTROL_AND && status == 0) ||
                (control == SHELL_CONTROL_OR && status != 0);
            if (should_run) {
                char expanded[EXPANDED_LINE_MAX];
                if (!expand_variables(segment, expanded, sizeof(expanded), cwd)) {
                    status = 2;
                } else {
                    status = run_command(expanded, cwd, background);
                }
                last_status = status;
                if (shell_flow_requested()) {
                    return status;
                }
            }
            segment = cursor + 1;
            control = next_control;
        }
    }
}

int main(int argc, char **argv) {
    char cwd[CLI_PATH_MAX] = "/";
    int login = 0;
    const char *command_text = 0;
    const char *script_path = 0;
    int positional_start = 0;
    char *default_argv[] = { "sh", 0 };
    uint64_t status = 0;

    for (int i = 1; i < argc; i++) {
        if (cli_streq(argv[i], "--login")) {
            login = 1;
            continue;
        }
        if (cli_streq(argv[i], "-c")) {
            if (i + 1 >= argc) {
                cli_puts("sh: -c requires a command\n");
                return 2;
            }
            command_text = argv[++i];
            positional_start = i + 1;
            break;
        }
        script_path = argv[i];
        positional_start = i;
        break;
    }

    if (positional_start > 0 && positional_start < argc) {
        shell_argv = &argv[positional_start];
        shell_argc = argc - positional_start;
    } else {
        shell_argv = default_argv;
        shell_argc = 1;
    }

    if (getenv("PATH") == 0) {
        setenv("PATH", "/fat/bin:/:/fat", 1);
    }
    if (getenv("PS1") == 0) {
        setenv("PS1", "\\w $ ", 1);
    }
    if (getenv("TMPDIR") == 0) {
        setenv("TMPDIR", "/fat/tmp", 1);
    }
    if (getenv("HISTFILE") == 0) {
        setenv("HISTFILE", "/fat/.srvsh_history", 1);
    }
    if (getenv("HISTSIZE") == 0) {
        setenv("HISTSIZE", "64", 1);
    }
    shell_pid = (uint64_t)srv_getpid();
    setenv("PWD", cwd, 1);
    setenv("OLDPWD", cwd, 1);
    if (login) {
        status = run_optional_script("/fat/etc/profile", cwd);
        last_status = status;
        status = run_optional_script_directory("/fat/etc/profile.d", cwd);
        last_status = status;
    }
    if (command_text != 0) {
        char command[EXPANDED_LINE_MAX];
        if (text_contains_heredoc(command_text)) {
            char temp_path[CLI_PATH_MAX];
            char staged[EXPANDED_LINE_MAX];
            size_t staged_length = 0;
            make_substitution_path(temp_path, sizeof(temp_path));
            staged[0] = '\0';
            append_text(staged, sizeof(staged), &staged_length, command_text);
            if (staged_length == 0 || staged[staged_length - 1] != '\n') {
                append_char(staged, sizeof(staged), &staged_length, '\n');
            }
            if (srv_fs_write(temp_path, staged, staged_length) < 0) {
                cli_puts("sh: cannot stage -c script\n");
                return 1;
            }
            status = run_script(temp_path, cwd);
            (void)srv_unlink(temp_path);
            return (int)status;
        }
        normalize_command_text(command_text, command, sizeof(command));
        return (int)run_line(command, cwd);
    }
    if (script_path != 0) {
        return (int)run_script(script_path, cwd);
    }

    cli_puts("srvsh: interactive shell\n");
    print_help();
    configure_history();
    linenoiseHistoryLoad(history_file_from_env());
    linenoiseSetCompletionCallback(shell_completion_callback);
    for (;;) {
        char prompt[LINE_MAX];
        build_prompt(prompt, sizeof(prompt), cwd);
        cli_copy(completion_cwd, sizeof(completion_cwd), cwd);
        char *line = linenoise(prompt);
        if (line == 0) {
            cli_puts("exit\n");
            break;
        }
        if (line[0] != '\0') {
            linenoiseHistoryAdd(line);
            save_history();
        }
        run_line(line, cwd);
        linenoiseFree(line);
    }
    return 0;
}
