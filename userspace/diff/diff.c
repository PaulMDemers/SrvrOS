#include <srvros/cli.h>

#include <stdio.h>
#include <string.h>

#define LINE_SIZE 512

static void usage(void) {
    cli_puts("usage: diff [-q|-u] file1 file2\n");
}

static int read_line(FILE *file, char *buffer, size_t capacity) {
    if (fgets(buffer, (int)capacity, file) == 0) {
        return 0;
    }
    return 1;
}

static void print_line_prefixed(char prefix, const char *line) {
    char prefix_text[2];
    prefix_text[0] = prefix;
    prefix_text[1] = '\0';
    cli_puts(prefix_text);
    cli_puts(line);
    if (line[0] == '\0' || line[cli_strlen(line) - 1] != '\n') {
        cli_puts("\n");
    }
}

int main(int argc, char **argv) {
    int quiet = 0;
    int unified = 0;
    int first = 1;
    int different = 0;
    int printed_header = 0;
    unsigned long line_number = 1;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        usage();
        return 0;
    }
    while (first < argc && argv[first][0] == '-' && argv[first][1] != '\0') {
        if (cli_is_option_terminator(argv[first])) {
            first++;
            break;
        }
        if (cli_streq(argv[first], "-q")) {
            quiet = 1;
            first++;
            continue;
        }
        if (cli_streq(argv[first], "-u") || cli_streq(argv[first], "-U0")) {
            unified = 1;
            first++;
            continue;
        }
        usage();
        return 2;
    }
    if (argc != first + 2) {
        usage();
        return 2;
    }

    FILE *left = fopen(argv[first], "r");
    if (left == 0) {
        cli_puts("diff: cannot open: ");
        cli_puts(argv[first]);
        cli_puts("\n");
        return 2;
    }
    FILE *right = fopen(argv[first + 1], "r");
    if (right == 0) {
        fclose(left);
        cli_puts("diff: cannot open: ");
        cli_puts(argv[first + 1]);
        cli_puts("\n");
        return 2;
    }

    for (;;) {
        char left_line[LINE_SIZE];
        char right_line[LINE_SIZE];
        int have_left = read_line(left, left_line, sizeof(left_line));
        int have_right = read_line(right, right_line, sizeof(right_line));
        if (!have_left && !have_right) {
            break;
        }
        if (have_left != have_right || strcmp(have_left ? left_line : "", have_right ? right_line : "") != 0) {
            different = 1;
            if (!quiet) {
                if (unified && !printed_header) {
                    cli_puts("--- ");
                    cli_puts(argv[first]);
                    cli_puts("\n+++ ");
                    cli_puts(argv[first + 1]);
                    cli_puts("\n");
                    printed_header = 1;
                }
                if (unified) {
                    if (have_left) {
                        print_line_prefixed('-', left_line);
                    }
                    if (have_right) {
                        print_line_prefixed('+', right_line);
                    }
                } else {
                    cli_puts("Files ");
                    cli_puts(argv[first]);
                    cli_puts(" and ");
                    cli_puts(argv[first + 1]);
                    cli_puts(" differ at line ");
                    cli_putn(line_number);
                    cli_puts("\n");
                    break;
                }
            }
        }
        line_number++;
    }

    fclose(left);
    fclose(right);
    return different ? 1 : 0;
}
