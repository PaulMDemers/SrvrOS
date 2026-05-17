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

int main(int argc, char **argv) {
    char *child_argv[XARGS_MAX_ARGS];
    const char *command = argc > 1 ? argv[1] : "echo";
    int out = 0;
    pid_t pid;
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: xargs [command [arg ...]]\n");
        return 0;
    }
    if (read_tokens() != 0) {
        return 1;
    }

    child_argv[out++] = (char *)command;
    for (int i = 2; i < argc && out + 1 < XARGS_MAX_ARGS; i++) {
        child_argv[out++] = argv[i];
    }
    for (int i = 0; i < token_count && out + 1 < XARGS_MAX_ARGS; i++) {
        child_argv[out++] = tokens[i];
    }
    child_argv[out] = 0;
    if (out >= XARGS_MAX_ARGS - 1 && token_count > 0) {
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
