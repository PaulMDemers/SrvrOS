#include <errno.h>
#include <spawn.h>
#include <srvros/cli.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define XARGS_MAX_TOKENS 64
#define XARGS_MAX_ARGS 96
#define XARGS_TOKEN_LENGTH 96

static char token_storage[XARGS_MAX_TOKENS][XARGS_TOKEN_LENGTH];
static char *tokens[XARGS_MAX_TOKENS];
static int token_count;

static int parse_positive(const char *text, int *value_out) {
    int value = 0;
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
    }
    if (value <= 0) {
        return 0;
    }
    *value_out = value;
    return 1;
}

static int push_token(const char *text, size_t length) {
    if (length == 0) {
        return 0;
    }
    if (token_count >= XARGS_MAX_TOKENS) {
        cli_puts("xargs: too many arguments\n");
        return 1;
    }
    if (length >= XARGS_TOKEN_LENGTH) {
        length = XARGS_TOKEN_LENGTH - 1;
    }
    for (size_t i = 0; i < length; i++) {
        token_storage[token_count][i] = text[i];
    }
    token_storage[token_count][length] = '\0';
    tokens[token_count] = token_storage[token_count];
    token_count++;
    return 0;
}

static int read_tokens(void) {
    char buffer[128];
    char token[XARGS_TOKEN_LENGTH];
    size_t length = 0;
    int in_quote = 0;
    char quote = '\0';

    for (;;) {
        long count = srv_read(SRV_STDIN, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("xargs: read failed\n");
            return 1;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (in_quote) {
                if (c == quote) {
                    in_quote = 0;
                } else if (length + 1 < sizeof(token)) {
                    token[length++] = c;
                }
                continue;
            }
            if (c == '\'' || c == '"') {
                in_quote = 1;
                quote = c;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if (push_token(token, length) != 0) {
                    return 1;
                }
                length = 0;
                continue;
            }
            if (length + 1 < sizeof(token)) {
                token[length++] = c;
            }
        }
    }
    if (in_quote) {
        cli_puts("xargs: unmatched quote\n");
        return 2;
    }
    if (push_token(token, length) != 0) {
        return 1;
    }
    return 0;
}

static int run_child(const char *command, char **prefix, int prefix_count, int first_token, int count) {
    char *child_argv[XARGS_MAX_ARGS];
    int out = 0;
    pid_t pid;
    int status = 0;

    child_argv[out++] = (char *)command;
    for (int i = 0; i < prefix_count && out + 1 < XARGS_MAX_ARGS; i++) {
        child_argv[out++] = prefix[i];
    }
    for (int i = 0; i < count && out + 1 < XARGS_MAX_ARGS; i++) {
        child_argv[out++] = tokens[first_token + i];
    }
    child_argv[out] = 0;
    if (out >= XARGS_MAX_ARGS - 1 && count > 0) {
        cli_puts("xargs: argument list truncated\n");
    }

    int error = posix_spawnp(&pid, command, 0, 0, child_argv, environ);
    if (error != 0) {
        cli_puts("xargs: cannot run ");
        cli_puts(command);
        cli_puts("\n");
        errno = error;
        return 127;
    }
    if (waitpid(pid, &status, 0) < 0) {
        cli_puts("xargs: wait failed\n");
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int main(int argc, char **argv) {
    const char *command = "echo";
    char *prefix[XARGS_MAX_ARGS];
    int prefix_count = 0;
    int max_per_run = XARGS_MAX_TOKENS;
    int no_run_if_empty = 0;
    int command_index;
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: xargs [-r] [-n count] [command [arg ...]]\n");
        return 0;
    }
    command_index = 1;
    while (command_index < argc) {
        const char *arg = argv[command_index];
        if (cli_is_option_terminator(arg)) {
            command_index++;
            break;
        }
        if (cli_streq(arg, "-r") || cli_streq(arg, "--no-run-if-empty")) {
            no_run_if_empty = 1;
            command_index++;
        } else if (cli_streq(arg, "-n") && command_index + 1 < argc) {
            if (!parse_positive(argv[command_index + 1], &max_per_run)) {
                cli_puts("usage: xargs [-r] [-n count] [command [arg ...]]\n");
                return 2;
            }
            command_index += 2;
        } else if (cli_starts_with(arg, "-n") && arg[2] != '\0') {
            if (!parse_positive(arg + 2, &max_per_run)) {
                cli_puts("usage: xargs [-r] [-n count] [command [arg ...]]\n");
                return 2;
            }
            command_index++;
        } else {
            break;
        }
    }
    if (command_index < argc) {
        command = argv[command_index++];
    }
    for (int i = command_index; i < argc && prefix_count < XARGS_MAX_ARGS - 1; i++) {
        prefix[prefix_count++] = argv[i];
    }
    if (read_tokens() != 0) {
        return 1;
    }
    if (token_count == 0) {
        return no_run_if_empty ? 0 : run_child(command, prefix, prefix_count, 0, 0);
    }
    for (int first = 0; first < token_count;) {
        int count = token_count - first;
        if (count > max_per_run) {
            count = max_per_run;
        }
        int result = run_child(command, prefix, prefix_count, first, count);
        if (result != 0 && status == 0) {
            status = result;
        }
        first += count;
    }
    return status;
}
