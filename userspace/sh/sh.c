#include <srvros/cli.h>
#include <srvros/conio.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>

#include "linenoise.h"

#define LINE_MAX 256
#define EXPANDED_LINE_MAX 512
#define ARG_EXPANDED_MAX 512
#define PATH_MAX_ENTRIES 8
#define PIPELINE_MAX_COMMANDS 6
#define SHELL_MAX_ALIASES 16
#define SHELL_MAX_FUNCTIONS 16
#define SHELL_MAX_FUNCTION_ARGS 16
#define SHELL_MAX_JOBS 8

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

static const char *shell_builtins[] = {
    "help", "exit", "exec", "return", "shift", "set", "source", ".", "path", "cd", "pwd", "clear",
    "echo", "env", "export", "unset", "alias", "type", "which", "command", "test", "[",
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
static int resolve_command(char *out, size_t capacity, const char *command);
static void wait_pipeline_pids(long *pids, size_t count, uint64_t *last_status);

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
                cli_puts("sh: unterminated command substitution\n");
                return 0;
            }
            if (!append_command_substitution(input + i + 2, end - i - 2, cwd, out, capacity, &length)) {
                return 0;
            }
            i = end;
            continue;
        }
        if (input[i + 1] == '{') {
            char name[64];
            size_t name_length = 0;
            size_t cursor = i + 2;
            while (input[cursor] != '\0' && input[cursor] != '}' && name_length + 1 < sizeof(name)) {
                name[name_length++] = input[cursor++];
            }
            if (input[cursor] != '}') {
                cli_puts("sh: bad substitution\n");
                return 0;
            }
            name[name_length] = '\0';
            append_text(out, capacity, &length, getenv(name));
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

static void shell_completion_callback(const char *line, linenoiseCompletions *completions) {
    size_t length = cli_strlen(line);
    size_t token_start = length;
    char token[CLI_PATH_MAX];
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
    if (command_position && !cli_contains_slash(token)) {
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

static void print_help(void) {
    cli_puts("builtins: help exit exec return shift set source . path cd pwd clear echo env export unset alias type which command test [ break continue jobs wait fg bg kill service dhcp net dns rmdir read :\n");
    cli_puts("commands: ls cat write cp rm mkdir mv tap wc grep head tail tee find du df sort uniq cut xargs sed expr printf tr stat chmod ps kill which env pwd true false sleep date touch mktemp basename dirname uname hostname uptime hello webd httpget spin fpdemo desktop calcgui notesgui textedit imgedit posixdemo ttydemo jsondemo inidemo linedemo sqlitedemo zlibdemo lua\n");
    cli_puts("syntax: sh [--login] [-c command|script] [args], command [args], { commands; }, name() { commands; }, if/then/else/fi, for/in/do/done, while/do/done, case/in/esac, use ;, &&, ||, append & for background\n");
    cli_puts("expansion: $VAR ${VAR} $? $$ $! $0 $1 $# $@ $(command), command-local NAME=value, unquoted * and ? globs\n");
    cli_puts("redirection: command < file, command > file, command >> file, command 2> file, command 2>> file, command 2>&1\n");
    cli_puts("pipeline: command | command [...]\n");
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

static uint64_t test_eval(int argc, char **argv) {
    if (argc == 0) {
        return 1;
    }
    if (argc == 1) {
        return argv[0][0] != '\0' ? 0 : 1;
    }
    if (argc == 2) {
        if (cli_streq(argv[0], "!")) {
            return test_eval(1, argv + 1) == 0 ? 1 : 0;
        }
        if (cli_streq(argv[0], "-n")) {
            return argv[1][0] != '\0' ? 0 : 1;
        }
        if (cli_streq(argv[0], "-z")) {
            return argv[1][0] == '\0' ? 0 : 1;
        }
        if (cli_streq(argv[0], "-e")) {
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
        if (cli_streq(argv[1], "!=")) {
            return !cli_streq(argv[0], argv[2]) ? 0 : 1;
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

static uint64_t test_command(const char *args, int bracket_form) {
    char work[ARG_EXPANDED_MAX];
    char *argv[8];
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
        if (cli_streq(argv[argc - 1], "]")) {
            argc--;
        }
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
        job = find_job(last_background_pid);
        if (job != 0) {
            status = wait_shell_job(job, 0);
            print_wait_result("[done]", (long)last_background_pid, status);
            return status;
        }
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

static void reap_exited_service(const char *name) {
    struct srv_process_info info;
    uint64_t status = 0;
    while (find_service_process(name, &info, 1) && cli_streq(info.state, "exited")) {
        if (srv_wait(info.pid, &status, SRV_WAIT_NOHANG) <= 0) {
            return;
        }
    }
}

static void service_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    char *action;
    char *action_args;
    struct srv_process_info info;
    uint64_t status = 0;

    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    action = name;
    while (*action != '\0' && *action != ' ' && *action != '\t') {
        action++;
    }
    if (*action != '\0') {
        *action++ = '\0';
        action = cli_trim(action);
    }

    if (!cli_streq(name, "webd")) {
        cli_puts("usage: service webd [start [root]|stop|status]\n");
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

    reap_exited_service("webd");
    if (cli_streq(action, "status")) {
        if (find_service_process("webd", &info, 1)) {
            cli_puts("webd ");
            cli_puts(info.state);
            cli_puts(" pid ");
            cli_putn(info.pid);
            cli_puts("\n");
        } else {
            cli_puts("webd stopped\n");
        }
        return;
    }

    if (cli_streq(action, "start")) {
        if (find_service_process("webd", &info, 0)) {
            cli_puts("webd already running pid ");
            cli_putn(info.pid);
            cli_puts("\n");
            return;
        }
        long pid = srv_spawn_bg_args("/fat/bin/webd", action_args);
        if (pid < 0) {
            cli_puts("webd start failed\n");
            return;
        }
        last_background_pid = (uint64_t)pid;
        cli_puts("webd started pid ");
        cli_putn((uint64_t)pid);
        cli_puts("\n");
        return;
    }

    if (cli_streq(action, "stop")) {
        if (!find_service_process("webd", &info, 0)) {
            cli_puts("webd stopped\n");
            return;
        }
        if (srv_kill(info.pid) < 0) {
            cli_puts("webd stop failed\n");
            return;
        }
        long waited = srv_wait(info.pid, &status, 0);
        cli_puts("webd stopped pid ");
        cli_putn(info.pid);
        if (waited > 0) {
            cli_puts(" status ");
            cli_putn(status);
        }
        cli_puts("\n");
        return;
    }

    cli_puts("usage: service webd [start [root]|stop|status]\n");
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

static uint64_t run_script_command_line(char *line, char *cwd) {
    char *trimmed = cli_trim(line);
    if (trimmed[0] == '\0' || trimmed[0] == '#') {
        return last_status;
    }
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
    int collecting_block = 0;
    int collecting_heredoc = 0;
    char block_end_keyword[8];
    int stop = 0;
    uint64_t status = 0;
    if (fd < 0) {
        cli_puts("source: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("source: read failed\n");
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
                            cli_puts("source: heredoc write failed\n");
                            status = 1;
                        } else {
                            status = run_script_command_line(heredoc_command, cwd);
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
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "fi");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (shell_keyword_at(trimmed, 0, "for") && !find_for_parts(trimmed, &(struct for_parts){0})) {
                    collecting_block = 1;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "done");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (shell_keyword_at(trimmed, 0, "while") && !find_while_parts(trimmed, &(struct while_parts){0})) {
                    collecting_block = 1;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "done");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (shell_keyword_at(trimmed, 0, "case") && !find_case_parts(trimmed, &(struct case_parts){0})) {
                    collecting_block = 1;
                    cli_copy(block_end_keyword, sizeof(block_end_keyword), "esac");
                    block_length = 0;
                    block[0] = '\0';
                    append_text(block, sizeof(block), &block_length, trimmed);
                } else if (is_function_start(trimmed) &&
                    !find_function_definition(trimmed, (char[32]){0}, 32, (char[EXPANDED_LINE_MAX]){0}, EXPANDED_LINE_MAX)) {
                    collecting_block = 1;
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
                        heredoc_body_length = 0;
                        heredoc_body[0] = '\0';
                    } else {
                        status = run_script_command_line(line, cwd);
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
                    cli_puts("source: heredoc write failed\n");
                    status = 1;
                } else {
                    status = run_script_command_line(heredoc_command, cwd);
                }
                (void)srv_unlink(heredoc_temp);
                collecting_heredoc = 0;
                length = 0;
            }
        }
        if (collecting_heredoc) {
            cli_puts("source: unterminated heredoc\n");
            (void)srv_unlink(heredoc_temp);
            status = 2;
        }
    } else if (!stop && collecting_block) {
        cli_puts("source: unterminated block\n");
        status = 2;
    } else if (!stop && length > 0) {
        line[length] = '\0';
        status = run_script_command_line(line, cwd);
    }
    srv_close(fd);
    return status;
}

static uint64_t run_optional_script(const char *path, char *cwd) {
    struct srv_stat info;
    if (srv_stat(path, &info) < 0 || info.type != 0) {
        return 0;
    }
    return run_script(path, cwd);
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
            cli_puts("sh: pipeline spawn failed\n");
            close_pipeline_fds(pipes, segment_count - 1);
            close_redirection_fds(input_fd, output_fd, error_fd);
            wait_pipeline_pids(pids, i, 0);
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
        print_help();
        return 0;
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
                cli_puts("sh: unmatched quote\n");
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
    shell_pid = (uint64_t)srv_getpid();
    setenv("PWD", cwd, 1);
    setenv("OLDPWD", cwd, 1);
    if (login) {
        status = run_optional_script("/fat/etc/profile", cwd);
        last_status = status;
        status = run_script("/fat/etc/init.sh", cwd);
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
    linenoiseHistorySetMaxLen(64);
    linenoiseHistoryLoad("/fat/.srvsh_history");
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
            linenoiseHistorySave("/fat/.srvsh_history");
        }
        run_line(line, cwd);
        linenoiseFree(line);
    }
    return 0;
}
