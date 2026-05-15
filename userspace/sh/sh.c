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

static char path_entries[PATH_MAX_ENTRIES][CLI_PATH_MAX] = {
    "/fat/bin",
    "/",
    "/fat",
};
static size_t path_count = 3;
static uint64_t last_status = 0;
static uint64_t shell_pid = 0;
static uint64_t substitution_counter = 0;

static uint64_t run_line(char *line, char *cwd);

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
    int has_else;
};

static int find_if_parts(const char *line, struct if_parts *parts);

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
    return c == '\0' || c == ' ' || c == '\t' || c == ';' || c == '\r' || c == '\n';
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
        size_t token_length = 0;
        int quoted = 0;
        char quote = '\0';
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        while (*cursor != '\0') {
            if (quote != '\0') {
                if (*cursor == quote) {
                    quote = '\0';
                } else if (*cursor == '\\' && cursor[1] != '\0') {
                    if (token_length + 1 < sizeof(token)) {
                        token[token_length++] = *cursor++;
                    }
                }
            } else {
                if (*cursor == ' ' || *cursor == '\t') {
                    break;
                }
                if (*cursor == '\'' || *cursor == '"') {
                    quote = *cursor;
                    quoted = 1;
                } else if (*cursor == '\\' && cursor[1] != '\0') {
                    if (token_length + 1 < sizeof(token)) {
                        token[token_length++] = *cursor++;
                    }
                }
            }
            if (token_length + 1 < sizeof(token)) {
                token[token_length++] = *cursor;
            }
            cursor++;
        }
        token[token_length] = '\0';
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
        append_text(out, capacity, &out_length, token);
    }
}

static void print_help(void) {
    cli_puts("builtins: help exit exec source . path cd pwd clear echo env export which test [ jobs wait service dhcp net dns rmdir\n");
    cli_puts("commands: ls cat write cp rm mkdir mv tap wc grep head stat chmod ps kill which env pwd true false hello webd spin fpdemo desktop calcgui notesgui textedit imgedit posixdemo ttydemo jsondemo inidemo linedemo sqlitedemo zlibdemo lua\n");
    cli_puts("syntax: sh [--login] [-c command|script], command [args], if/then/else/fi, use ;, &&, ||, append & for background\n");
    cli_puts("expansion: $VAR ${VAR} $? $$ $(command) unquoted * and ? globs\n");
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

static void print_jobs(void) {
    uint64_t index = 0;
    struct srv_process_info info;
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

static void wait_for_job(const char *args) {
    uint64_t status = 0;
    uint64_t pid = 0;
    long waited;

    if (args != 0 && args[0] != '\0') {
        pid = parse_u64(args);
        if (pid == 0) {
            cli_puts("usage: wait [pid]\n");
            return;
        }
    }

    waited = srv_wait(pid, &status, 0);
    if (waited < 0) {
        cli_puts("wait: no matching background process\n");
        return;
    }
    cli_puts("[done] pid ");
    cli_putn((uint64_t)waited);
    cli_puts(" status ");
    cli_putn(status);
    cli_puts("\n");
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
    if (equals == 0 || equals == name) {
        cli_puts("usage: export NAME=value\n");
        return 2;
    }
    *equals++ = '\0';
    if (setenv(name, equals, 1) < 0) {
        cli_puts("export: failed\n");
        return 1;
    }
    if (cli_streq(name, "PATH")) {
        set_path_list(equals);
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

static uint64_t run_script(const char *path, char *cwd) {
    int fd = (int)srv_open(path);
    char buffer[128];
    char line[LINE_MAX];
    char if_block[EXPANDED_LINE_MAX];
    size_t length = 0;
    size_t if_length = 0;
    int collecting_if = 0;
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
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                line[length] = '\0';
                char *trimmed = cli_trim(line);
                if (collecting_if) {
                    append_text(if_block, sizeof(if_block), &if_length, "; ");
                    append_text(if_block, sizeof(if_block), &if_length, trimmed);
                    if (shell_keyword_at(trimmed, 0, "fi")) {
                        status = run_line(if_block, cwd);
                        collecting_if = 0;
                        if_length = 0;
                        if_block[0] = '\0';
                    }
                } else if (shell_keyword_at(trimmed, 0, "if") && !find_if_parts(trimmed, &(struct if_parts){0})) {
                    collecting_if = 1;
                    if_length = 0;
                    if_block[0] = '\0';
                    append_text(if_block, sizeof(if_block), &if_length, trimmed);
                } else {
                    status = run_line(line, cwd);
                }
                length = 0;
            } else if (length + 1 < sizeof(line)) {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        char *trimmed = cli_trim(line);
        if (collecting_if) {
            append_text(if_block, sizeof(if_block), &if_length, "; ");
            append_text(if_block, sizeof(if_block), &if_length, trimmed);
            status = run_line(if_block, cwd);
        } else {
            status = run_line(line, cwd);
        }
    } else if (collecting_if) {
        cli_puts("source: unterminated if\n");
        status = 2;
    }
    srv_close(fd);
    return status;
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
    for (size_t i = 0; i < count; i++) {
        uint64_t status = 0;
        if (pids[i] >= 0) {
            (void)srv_wait((uint64_t)pids[i], &status, 0);
            if (i + 1 == count && last_status != 0) {
                *last_status = status;
            }
        }
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
    int stderr_fd) {
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

static uint64_t run_pipeline(char **segments, size_t segment_count, const char *cwd) {
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
        pids[i] = exec_external_command(commands[i].path, commands[i].args, 1, stdin_fd, stdout_fd, stderr_fd);
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
    wait_pipeline_pids(pids, segment_count, &final_status);
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

static uint64_t run_command(char *line, char *cwd, int background) {
    char *command = cli_trim(line);
    char *args = command;
    char *pipeline_segments[PIPELINE_MAX_COMMANDS];
    int pipeline_count;
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

    pipeline_count = split_pipeline_segments(command, pipeline_segments, PIPELINE_MAX_COMMANDS);
    if (pipeline_count < 0) {
        cli_puts("sh: pipeline too long\n");
        return 2;
    }
    if (pipeline_count > 1) {
        if (background) {
            cli_puts("sh: background pipeline unsupported\n");
            return 2;
        }
        return run_pipeline(pipeline_segments, (size_t)pipeline_count, cwd);
    }

    args = find_argument_tail(command);
    if (*args != '\0') {
        *args++ = '\0';
        args = cli_trim(args);
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
    if (cli_streq(command, "which")) {
        return which_command(args);
    }
    if (cli_streq(command, "test")) {
        return test_command(args, 0);
    }
    if (cli_streq(command, "[")) {
        return test_command(args, 1);
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
        cli_normalize_path(next, sizeof(next), cwd, args[0] != '\0' ? args : "/");
        cli_copy(cwd, CLI_PATH_MAX, next);
        setenv("PWD", cwd, 1);
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
        print_jobs();
        return 0;
    }
    if (cli_streq(command, "wait")) {
        wait_for_job(args);
        return 0;
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
        status = exec_external_command(path, args, 1, input_fd, output_fd, error_fd);
        if (status < 0) {
            cli_puts("sh: background spawn failed\n");
            close_redirection_fds(input_fd, output_fd, error_fd);
            return 126;
        } else {
            cli_puts("[bg] pid ");
            cli_putn((uint64_t)status);
            cli_puts("\n");
        }
        close_redirection_fds(input_fd, output_fd, error_fd);
        return 0;
    }

    status = exec_external_command(path, args, 0, input_fd, output_fd, error_fd);
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

static uint64_t run_line(char *line, char *cwd) {
    char quote = '\0';
    char *segment = line;
    enum shell_control control = SHELL_CONTROL_ALWAYS;
    uint64_t status = last_status;
    char *trimmed = cli_trim(line);

    if (trimmed != line) {
        line = trimmed;
        segment = line;
    }
    if (shell_keyword_at(line, 0, "if")) {
        status = run_if_line(line, cwd);
        last_status = status;
        return status;
    }

    for (char *cursor = line; ; cursor++) {
        char c = *cursor;
        if (c == '\0') {
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
            continue;
        }
        if (script_path == 0) {
            script_path = argv[i];
            continue;
        }
        cli_puts("usage: sh [--login] [-c command|script]\n");
        return 2;
    }

    if (getenv("PATH") == 0) {
        setenv("PATH", "/fat/bin:/:/fat", 1);
    }
    shell_pid = (uint64_t)srv_getpid();
    setenv("PWD", cwd, 1);
    if (login) {
        status = run_script("/fat/etc/init.sh", cwd);
        last_status = status;
    }
    if (command_text != 0) {
        char command[EXPANDED_LINE_MAX];
        cli_copy(command, sizeof(command), command_text);
        return (int)run_line(command, cwd);
    }
    if (script_path != 0) {
        return (int)run_script(script_path, cwd);
    }

    cli_puts("srvsh: interactive shell\n");
    print_help();
    linenoiseHistorySetMaxLen(64);
    linenoiseHistoryLoad("/fat/.srvsh_history");
    for (;;) {
        char prompt[CLI_PATH_MAX + 4];
        cli_copy(prompt, sizeof(prompt), cwd);
        size_t prompt_length = cli_strlen(prompt);
        append_text(prompt, sizeof(prompt), &prompt_length, " $ ");
        char *line = linenoise(prompt);
        if (line == 0) {
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
