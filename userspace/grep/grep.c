#include <regex.h>
#include <srvros/cli.h>
#include <srvros/sys.h>

struct grep_options {
    int invert;
    int line_numbers;
    int count_only;
    int quiet;
    int ignore_case;
    int multi_file;
    int suppress_errors;
    int fixed_strings;
    int only_matching;
    int list_files;
    int list_without_match;
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

static int find_literal_at(const char *line, const char *needle, int ignore_case, size_t start,
    size_t *match_start, size_t *match_end) {
    if (needle[0] == '\0') {
        *match_start = start;
        *match_end = start;
        return 1;
    }
    for (size_t i = start; line[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' && line[i + j] != '\0' &&
            (!ignore_case ? line[i + j] == needle[j] :
                lower_char(line[i + j]) == lower_char(needle[j]))) {
            j++;
        }
        if (needle[j] == '\0') {
            *match_start = i;
            *match_end = i + j;
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

static int line_matches(const char *line, const char *needle, const regex_t *regex,
    const struct grep_options *options) {
    if (options->fixed_strings) {
        return contains_case(line, needle, options->ignore_case);
    }
    return regexec(regex, line, 0, 0, 0) == 0;
}

static void print_only_matches(const char *line, const char *needle, const regex_t *regex,
    const char *label, const struct grep_options *options, uint64_t line_number) {
    size_t offset = 0;
    do {
        size_t start = 0;
        size_t end = 0;
        if (options->fixed_strings) {
            if (!find_literal_at(line, needle, options->ignore_case, offset, &start, &end)) {
                break;
            }
        } else {
            regmatch_t match;
            if (regexec(regex, line + offset, 1, &match, 0) != 0) {
                break;
            }
            start = offset + (size_t)match.rm_so;
            end = offset + (size_t)match.rm_eo;
        }
        print_match_prefix(label, options, line_number);
        for (size_t i = start; i < end; i++) {
            char text[2];
            text[0] = line[i];
            text[1] = '\0';
            cli_puts(text);
        }
        cli_puts("\n");
        offset = end > start ? end : start + 1;
    } while (line[offset] != '\0');
}

static void emit_line_match(const char *line, const char *needle, const regex_t *regex,
    const char *label, const struct grep_options *options, uint64_t line_number) {
    if (options->only_matching && !options->invert) {
        print_only_matches(line, needle, regex, label, options, line_number);
        return;
    }
    print_match_prefix(label, options, line_number);
    cli_puts(line);
    cli_puts("\n");
}

static int grep_fd(const char *needle, const regex_t *regex, int fd, int close_fd,
    const char *label, const struct grep_options *options) {
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
                int is_match = line_matches(line, needle, regex, options);
                if (options->invert) {
                    is_match = !is_match;
                }
                if (options->list_without_match) {
                    if (is_match) {
                        matched = 1;
                    }
                    line_number++;
                    line_len = 0;
                    continue;
                }
                if (is_match) {
                    match_count++;
                    matched = 1;
                    if (options->list_files) {
                        if (label != 0 && label[0] != '\0') {
                            cli_puts(label);
                            cli_puts("\n");
                        }
                        if (close_fd) {
                            srv_close(fd);
                        }
                        return 0;
                    }
                    if (options->quiet) {
                        if (close_fd) {
                            srv_close(fd);
                        }
                        return 0;
                    }
                    if (!options->count_only) {
                        emit_line_match(line, needle, regex, label, options, line_number);
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
        int is_match = line_matches(line, needle, regex, options);
        if (options->invert) {
            is_match = !is_match;
        }
        if (options->list_without_match) {
            if (is_match) {
                matched = 1;
            }
            goto done;
        }
        if (is_match) {
            match_count++;
            matched = 1;
            if (options->list_files) {
                if (label != 0 && label[0] != '\0') {
                    cli_puts(label);
                    cli_puts("\n");
                }
                if (close_fd) {
                    srv_close(fd);
                }
                return 0;
            }
            if (options->quiet) {
                if (close_fd) {
                    srv_close(fd);
                }
                return 0;
            }
            if (!options->count_only) {
                emit_line_match(line, needle, regex, label, options, line_number);
            }
        }
    }
done:
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
    if (options->list_without_match) {
        if (!matched && label != 0 && label[0] != '\0') {
            cli_puts(label);
            cli_puts("\n");
        }
        return matched ? 1 : 0;
    }
    return matched ? 0 : 1;
}

static int grep_file(const char *needle, const regex_t *regex, const char *path,
    const struct grep_options *options) {
    if (cli_streq(path, "-")) {
        return grep_fd(needle, regex, SRV_STDIN, 0, "", options);
    }

    int fd = (int)srv_open(path);
    if (fd < 0) {
        if (!options->suppress_errors) {
            cli_puts("grep: cannot open ");
            cli_puts(path);
            cli_puts("\n");
        }
        return 2;
    }
    return grep_fd(needle, regex, fd, 1, path, options);
}

int main(int argc, char **argv) {
    struct grep_options options = {0};
    regex_t regex;
    int regex_ready = 0;
    int status = 1;
    int pattern_index = 1;
    const char *pattern = 0;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: grep [-EinvclLoqsF] <text> [file ...]\n");
        return 0;
    }
    for (; pattern_index < argc; pattern_index++) {
        const char *arg = argv[pattern_index];
        if (cli_is_option_terminator(arg)) {
            pattern_index++;
            break;
        }
        if (arg[0] != '-' || arg[1] == '\0' || cli_streq(arg, "-")) {
            break;
        }
        if ((cli_streq(arg, "-e") || cli_streq(arg, "--regexp")) && pattern_index + 1 < argc) {
            pattern = argv[++pattern_index];
            continue;
        }
        if (cli_starts_with(arg, "--regexp=")) {
            pattern = arg + 9;
            continue;
        }
        if (cli_streq(arg, "--invert-match")) {
            options.invert = 1;
            continue;
        }
        if (cli_streq(arg, "--line-number")) {
            options.line_numbers = 1;
            continue;
        }
        if (cli_streq(arg, "--count")) {
            options.count_only = 1;
            continue;
        }
        if (cli_streq(arg, "--files-with-matches")) {
            options.list_files = 1;
            continue;
        }
        if (cli_streq(arg, "--files-without-match")) {
            options.list_without_match = 1;
            continue;
        }
        if (cli_streq(arg, "--only-matching")) {
            options.only_matching = 1;
            continue;
        }
        if (cli_streq(arg, "--quiet") || cli_streq(arg, "--silent")) {
            options.quiet = 1;
            continue;
        }
        if (cli_streq(arg, "--ignore-case")) {
            options.ignore_case = 1;
            continue;
        }
        if (cli_streq(arg, "--no-messages")) {
            options.suppress_errors = 1;
            continue;
        }
        if (cli_streq(arg, "--fixed-strings")) {
            options.fixed_strings = 1;
            continue;
        }
        if (cli_streq(arg, "--extended-regexp")) {
            continue;
        }
        for (size_t j = 1; arg[j] != '\0'; j++) {
            if (arg[j] == 'v') {
                options.invert = 1;
            } else if (arg[j] == 'n') {
                options.line_numbers = 1;
            } else if (arg[j] == 'c') {
                options.count_only = 1;
            } else if (arg[j] == 'l') {
                options.list_files = 1;
            } else if (arg[j] == 'L') {
                options.list_without_match = 1;
            } else if (arg[j] == 'o') {
                options.only_matching = 1;
            } else if (arg[j] == 'q') {
                options.quiet = 1;
            } else if (arg[j] == 'i') {
                options.ignore_case = 1;
            } else if (arg[j] == 's') {
                options.suppress_errors = 1;
            } else if (arg[j] == 'F') {
                options.fixed_strings = 1;
            } else if (arg[j] == 'E') {
                continue;
            } else {
                cli_puts("usage: grep [-EinvclLoqsF] <text> [file ...]\n");
                return 2;
            }
        }
    }
    if (pattern == 0) {
        if (pattern_index >= argc) {
            cli_puts("usage: grep [-EinvclLoqsF] <text> [file ...]\n");
            return 2;
        }
        pattern = argv[pattern_index++];
    }
    if (pattern == 0) {
        cli_puts("usage: grep [-EinvclLoqsF] <text> [file ...]\n");
        return 2;
    }
    if (!options.fixed_strings) {
        int error = regcomp(&regex, pattern,
            REG_EXTENDED | (options.ignore_case ? REG_ICASE : 0) | REG_NOSUB);
        if (error != 0) {
            char error_text[48];
            regerror(error, &regex, error_text, sizeof(error_text));
            cli_puts("grep: ");
            cli_puts(error_text);
            cli_puts("\n");
            return 2;
        }
        regex_ready = 1;
    }
    if (pattern_index >= argc) {
        status = grep_fd(pattern, &regex, SRV_STDIN, 0, "", &options);
        if (regex_ready) {
            regfree(&regex);
        }
        return status;
    }
    options.multi_file = argc - pattern_index > 1;
    for (int i = pattern_index; i < argc; i++) {
        int result = grep_file(pattern, &regex, argv[i], &options);
        if (result == 0) {
            status = 0;
        } else if (result == 2) {
            status = 2;
        }
    }
    if (regex_ready) {
        regfree(&regex);
    }
    return status;
}
