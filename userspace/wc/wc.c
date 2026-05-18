#include <srvros/cli.h>
#include <srvros/sys.h>

struct wc_options {
    int lines;
    int words;
    int bytes;
};

static void print_selected_count(uint64_t value, int *printed) {
    if (*printed) {
        cli_puts(" ");
    }
    cli_putn(value);
    *printed = 1;
}

static int wc_fd(int fd, const char *label, int close_fd, const struct wc_options *options) {
    char buffer[128];
    uint64_t bytes = 0;
    uint64_t lines = 0;
    uint64_t words = 0;
    int in_word = 0;

    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (close_fd) {
                srv_close(fd);
            }
            return 1;
        }
        if (count == 0) {
            break;
        }
        bytes += (uint64_t)count;
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n') {
                lines++;
            }
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                in_word = 0;
            } else if (!in_word) {
                words++;
                in_word = 1;
            }
        }
    }
    if (close_fd) {
        srv_close(fd);
    }
    int printed = 0;
    if (options->lines) {
        print_selected_count(lines, &printed);
    }
    if (options->words) {
        print_selected_count(words, &printed);
    }
    if (options->bytes) {
        print_selected_count(bytes, &printed);
    }
    if (label != 0 && label[0] != '\0') {
        cli_puts(" ");
        cli_puts(label);
    }
    cli_puts("\n");
    return 0;
}

static int wc_file(const char *path, const struct wc_options *options) {
    if (cli_streq(path, "-")) {
        return wc_fd(SRV_STDIN, "-", 0, options);
    }

    int fd = (int)srv_open(path);
    if (fd < 0) {
        cli_puts("wc: open failed: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    return wc_fd(fd, path, 1, options);
}

int main(int argc, char **argv) {
    struct wc_options options = {0};
    int status = 0;
    int first_file = 1;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: wc [-lwc] [file ...]\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (cli_is_option_terminator(arg)) {
            first_file = i + 1;
            break;
        }
        if (arg[0] != '-' || arg[1] == '\0' || cli_streq(arg, "-")) {
            first_file = i;
            break;
        }
        if (cli_streq(arg, "--lines")) {
            options.lines = 1;
            first_file = i + 1;
            continue;
        }
        if (cli_streq(arg, "--words")) {
            options.words = 1;
            first_file = i + 1;
            continue;
        }
        if (cli_streq(arg, "--bytes") || cli_streq(arg, "--chars")) {
            options.bytes = 1;
            first_file = i + 1;
            continue;
        }
        for (size_t j = 1; arg[j] != '\0'; j++) {
            if (arg[j] == 'l') {
                options.lines = 1;
            } else if (arg[j] == 'w') {
                options.words = 1;
            } else if (arg[j] == 'c') {
                options.bytes = 1;
            } else {
                cli_puts("usage: wc [-lwc] [file ...]\n");
                return 1;
            }
        }
        first_file = i + 1;
    }
    if (!options.lines && !options.words && !options.bytes) {
        options.lines = 1;
        options.words = 1;
        options.bytes = 1;
    }
    if (argc <= first_file) {
        return wc_fd(SRV_STDIN, "", 0, &options);
    }
    for (int i = first_file; i < argc; i++) {
        int result = wc_file(argv[i], &options);
        if (result != 0) {
            status = result;
        }
    }
    return status;
}
