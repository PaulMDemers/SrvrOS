#include <srvros/cli.h>
#include <srvros/sys.h>

#define SED_LINE_MAX 512
#define SED_PATTERN_MAX 96

struct sed_substitution {
    char from[SED_PATTERN_MAX];
    char to[SED_PATTERN_MAX];
    int global;
};

static int parse_substitution(const char *expression, struct sed_substitution *sub) {
    char delimiter;
    size_t from_length = 0;
    size_t to_length = 0;
    size_t i;

    if (expression == 0 || expression[0] != 's' || expression[1] == '\0') {
        return 0;
    }
    delimiter = expression[1];
    i = 2;
    while (expression[i] != '\0' && expression[i] != delimiter) {
        if (expression[i] == '\\' && expression[i + 1] != '\0') {
            i++;
        }
        if (from_length + 1 < sizeof(sub->from)) {
            sub->from[from_length++] = expression[i];
        }
        i++;
    }
    if (expression[i] != delimiter || from_length == 0) {
        return 0;
    }
    i++;
    while (expression[i] != '\0' && expression[i] != delimiter) {
        if (expression[i] == '\\' && expression[i + 1] != '\0') {
            i++;
        }
        if (to_length + 1 < sizeof(sub->to)) {
            sub->to[to_length++] = expression[i];
        }
        i++;
    }
    if (expression[i] != delimiter) {
        return 0;
    }
    i++;
    sub->from[from_length] = '\0';
    sub->to[to_length] = '\0';
    sub->global = expression[i] == 'g';
    return expression[i] == '\0' || (expression[i] == 'g' && expression[i + 1] == '\0');
}

static int starts_with_at(const char *text, size_t index, const char *needle) {
    for (size_t i = 0; needle[i] != '\0'; i++) {
        if (text[index + i] != needle[i]) {
            return 0;
        }
    }
    return 1;
}

static void print_substituted_line(const char *line, const struct sed_substitution *sub) {
    size_t from_length = cli_strlen(sub->from);
    int replaced = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        if ((!replaced || sub->global) && starts_with_at(line, i, sub->from)) {
            cli_puts(sub->to);
            i += from_length - 1;
            replaced = 1;
            continue;
        }
        srv_write(SRV_STDOUT, line + i, 1);
    }
    cli_puts("\n");
}

static int sed_fd(int fd, int close_fd, const struct sed_substitution *sub) {
    char buffer[128];
    char line[SED_LINE_MAX];
    size_t length = 0;
    int status = 0;

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
                print_substituted_line(line, sub);
                length = 0;
            } else {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        print_substituted_line(line, sub);
    }
    if (close_fd) {
        srv_close(fd);
    }
    return status;
}

static int sed_file(const char *path, const struct sed_substitution *sub) {
    if (cli_streq(path, "-")) {
        return sed_fd(SRV_STDIN, 0, sub);
    }
    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("sed: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return sed_fd(fd, 1, sub);
}

int main(int argc, char **argv) {
    struct sed_substitution sub;
    int status = 0;

    if (argc < 2 || !parse_substitution(argv[1], &sub)) {
        cli_puts("usage: sed s/old/new/[g] [file ...]\n");
        return 1;
    }
    if (argc == 2) {
        return sed_fd(SRV_STDIN, 0, &sub);
    }
    for (int i = 2; i < argc; i++) {
        if (sed_file(argv[i], &sub) != 0) {
            status = 1;
        }
    }
    return status;
}
