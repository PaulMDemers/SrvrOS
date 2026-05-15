#include <srvros/cli.h>
#include <srvros/sys.h>

struct grep_options {
    int invert;
    int line_numbers;
    int count_only;
    int quiet;
    int ignore_case;
    int multi_file;
};

static char lower_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int contains(const char *line, const char *needle) {
    if (needle[0] == '\0') {
        return 1;
    }
    for (size_t i = 0; line[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' && line[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

static int contains_case(const char *line, const char *needle, int ignore_case) {
    if (!ignore_case) {
        return contains(line, needle);
    }
    if (needle[0] == '\0') {
        return 1;
    }
    for (size_t i = 0; line[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' &&
            lower_char(line[i + j]) == lower_char(needle[j])) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void print_match_prefix(const char *label, const struct grep_options *options, uint64_t line_number) {
    if (options->multi_file && label != 0 && label[0] != '\0') {
        cli_puts(label);
        cli_puts(":");
    }
    if (options->line_numbers) {
        cli_putn(line_number);
        cli_puts(":");
    }
}

static int grep_fd(const char *needle, int fd, int close_fd, const char *label, const struct grep_options *options) {
    char buffer[128];
    char line[256];
    size_t line_len = 0;
    int matched = 0;
    uint64_t line_number = 1;
    uint64_t match_count = 0;
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (close_fd) {
                srv_close(fd);
            }
            return 2;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n' || line_len + 1 >= sizeof(line)) {
                line[line_len] = '\0';
                int is_match = contains_case(line, needle, options->ignore_case);
                if (options->invert) {
                    is_match = !is_match;
                }
                if (is_match) {
                    match_count++;
                    matched = 1;
                    if (options->quiet) {
                        if (close_fd) {
                            srv_close(fd);
                        }
                        return 0;
                    }
                    if (!options->count_only) {
                        print_match_prefix(label, options, line_number);
                        cli_puts(line);
                        cli_puts("\n");
                    }
                }
                line_number++;
                line_len = 0;
            } else {
                line[line_len++] = c;
            }
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        int is_match = contains_case(line, needle, options->ignore_case);
        if (options->invert) {
            is_match = !is_match;
        }
        if (is_match) {
            match_count++;
            matched = 1;
            if (options->quiet) {
                if (close_fd) {
                    srv_close(fd);
                }
                return 0;
            }
            if (!options->count_only) {
                print_match_prefix(label, options, line_number);
                cli_puts(line);
                cli_puts("\n");
            }
        }
    }
    if (close_fd) {
        srv_close(fd);
    }
    if (options->count_only && !options->quiet) {
        if (options->multi_file && label != 0 && label[0] != '\0') {
            cli_puts(label);
            cli_puts(":");
        }
        cli_putn(match_count);
        cli_puts("\n");
    }
    return matched ? 0 : 1;
}

static int grep_file(const char *needle, const char *path, const struct grep_options *options) {
    if (cli_streq(path, "-")) {
        return grep_fd(needle, SRV_STDIN, 0, "", options);
    }

    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("grep: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return 2;
    }
    return grep_fd(needle, fd, 1, path, options);
}

int main(int argc, char **argv) {
    struct grep_options options = {0};
    int status = 1;
    int pattern_index = 1;
    for (; pattern_index < argc; pattern_index++) {
        const char *arg = argv[pattern_index];
        if (arg[0] != '-' || arg[1] == '\0' || cli_streq(arg, "-")) {
            break;
        }
        for (size_t j = 1; arg[j] != '\0'; j++) {
            if (arg[j] == 'v') {
                options.invert = 1;
            } else if (arg[j] == 'n') {
                options.line_numbers = 1;
            } else if (arg[j] == 'c') {
                options.count_only = 1;
            } else if (arg[j] == 'q') {
                options.quiet = 1;
            } else if (arg[j] == 'i') {
                options.ignore_case = 1;
            } else {
                cli_puts("usage: grep [-invcq] <text> [file ...]\n");
                return 2;
            }
        }
    }
    if (pattern_index >= argc) {
        cli_puts("usage: grep [-invcq] <text> [file ...]\n");
        return 2;
    }
    if (pattern_index + 1 >= argc) {
        return grep_fd(argv[pattern_index], SRV_STDIN, 0, "", &options);
    }
    options.multi_file = argc - pattern_index - 1 > 1;
    for (int i = pattern_index + 1; i < argc; i++) {
        int result = grep_file(argv[pattern_index], argv[i], &options);
        if (result == 0) {
            status = 0;
        } else if (result > status) {
            status = result;
        }
    }
    return status;
}
