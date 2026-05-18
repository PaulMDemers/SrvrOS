#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define LINE_MAX_LOCAL 512
#define PATH_MAX_LOCAL 160

extern char **environ;

static const char *tool_name(const char *arg0) {
    const char *slash = strrchr(arg0 != 0 ? arg0 : "", '/');
    return slash != 0 ? slash + 1 : (arg0 != 0 ? arg0 : "");
}

static void usage(const char *tool) {
    if (strcmp(tool, "ln") == 0) {
        puts("usage: ln [-s] source target");
    } else if (strcmp(tool, "sync") == 0) {
        puts("usage: sync");
    } else if (strcmp(tool, "test") == 0) {
        puts("usage: test expression");
    } else if (strcmp(tool, "[") == 0) {
        puts("usage: [ expression ]");
    } else if (strcmp(tool, "cksum") == 0) {
        puts("usage: cksum [file ...]");
    } else if (strcmp(tool, "sum") == 0) {
        puts("usage: sum [file ...]");
    } else if (strcmp(tool, "comm") == 0) {
        puts("usage: comm [-123] file1 file2");
    } else if (strcmp(tool, "paste") == 0) {
        puts("usage: paste [-d delimiters] file ...");
    } else if (strcmp(tool, "join") == 0) {
        puts("usage: join file1 file2");
    } else if (strcmp(tool, "split") == 0) {
        puts("usage: split [-l lines|-b bytes] [file [prefix]]");
    } else if (strcmp(tool, "od") == 0) {
        puts("usage: od [-An] [-tx1] [file ...]");
    } else if (strcmp(tool, "hexdump") == 0) {
        puts("usage: hexdump [-C] [file ...]");
    } else if (strcmp(tool, "strings") == 0) {
        puts("usage: strings [-n length] [file ...]");
    } else if (strcmp(tool, "file") == 0) {
        puts("usage: file path ...");
    } else if (strcmp(tool, "tty") == 0) {
        puts("usage: tty [-s]");
    } else if (strcmp(tool, "stty") == 0) {
        puts("usage: stty [-a|raw|cooked|sane|echo|-echo]");
    } else if (strcmp(tool, "time") == 0) {
        puts("usage: time command [arg ...]");
    } else if (strcmp(tool, "timeout") == 0) {
        puts("usage: timeout seconds command [arg ...]");
    } else if (strcmp(tool, "nohup") == 0) {
        puts("usage: nohup command [arg ...]");
    } else if (strcmp(tool, "nice") == 0) {
        puts("usage: nice [-n adjustment] command [arg ...]");
    }
}

static int parse_u64(const char *text, uint64_t *out) {
    if (text == 0 || *text == '\0') {
        return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10u + (uint64_t)(text[i] - '0');
    }
    *out = value;
    return 1;
}

static int open_input(const char *path) {
    if (path == 0 || strcmp(path, "-") == 0) {
        return STDIN_FILENO;
    }
    return open(path, O_RDONLY);
}

static void close_input(int fd) {
    if (fd > STDERR_FILENO) {
        close(fd);
    }
}

static int read_line_fd(int fd, char *buffer, size_t capacity) {
    if (capacity == 0) {
        return -1;
    }
    size_t used = 0;
    while (used + 1 < capacity) {
        char c;
        ssize_t got = read(fd, &c, 1);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            break;
        }
        buffer[used++] = c;
        if (c == '\n') {
            break;
        }
    }
    buffer[used] = '\0';
    return used != 0 ? 1 : 0;
}

static int write_all(int fd, const void *buffer, size_t length) {
    const unsigned char *bytes = buffer;
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(fd, bytes + written, length - written);
        if (result <= 0) {
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

static int unsupported_link(int argc, char **argv) {
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("ln");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        first = 2;
    }
    if (argc != first + 2) {
        usage("ln");
        return 1;
    }
    fputs("ln: links are not supported by this filesystem yet\n", stderr);
    return 1;
}

static int sync_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("sync");
        return 0;
    }
    if (argc != 1) {
        usage("sync");
        return 1;
    }
    sync();
    return 0;
}

struct test_parser {
    int argc;
    char **argv;
    int pos;
};

static int test_file_unary(const char *op, const char *path) {
    struct stat st;
    if (strcmp(op, "-e") == 0) {
        return stat(path, &st) == 0;
    }
    if (strcmp(op, "-f") == 0) {
        return stat(path, &st) == 0 && S_ISREG(st.st_mode);
    }
    if (strcmp(op, "-d") == 0) {
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
    }
    if (strcmp(op, "-s") == 0) {
        return stat(path, &st) == 0 && st.st_size > 0;
    }
    if (strcmp(op, "-r") == 0) {
        return access(path, R_OK) == 0;
    }
    if (strcmp(op, "-w") == 0) {
        return access(path, W_OK) == 0;
    }
    if (strcmp(op, "-x") == 0) {
        return access(path, X_OK) == 0;
    }
    return -1;
}

static int test_binary_file(const char *left, const char *op, const char *right) {
    if (strcmp(op, "-nt") != 0 && strcmp(op, "-ot") != 0 && strcmp(op, "-ef") != 0) {
        return -1;
    }
    struct stat a;
    struct stat b;
    if (stat(left, &a) != 0 || stat(right, &b) != 0) {
        return 0;
    }
    if (strcmp(op, "-nt") == 0) {
        return a.st_mtime > b.st_mtime;
    }
    if (strcmp(op, "-ot") == 0) {
        return a.st_mtime < b.st_mtime;
    }
    if (strcmp(op, "-ef") == 0) {
        return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
    }
    return -1;
}

static int parse_long_exact(const char *text, long *out) {
    if (text == 0 || *text == '\0') {
        return 0;
    }
    long sign = 1;
    if (*text == '-') {
        sign = -1;
        text++;
    } else if (*text == '+') {
        text++;
    }
    if (*text == '\0') {
        return 0;
    }
    long value = 0;
    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }
        value = value * 10 + (*text - '0');
        text++;
    }
    *out = value * sign;
    return 1;
}

static int test_binary_value(const char *left, const char *op, const char *right, int *handled) {
    *handled = 1;
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
        return strcmp(left, right) == 0;
    }
    if (strcmp(op, "!=") == 0) {
        return strcmp(left, right) != 0;
    }
    int file = test_binary_file(left, op, right);
    if (file >= 0) {
        return file;
    }
    long left_value = 0;
    long right_value = 0;
    if (parse_long_exact(left, &left_value) && parse_long_exact(right, &right_value)) {
        if (strcmp(op, "-eq") == 0) return left_value == right_value;
        if (strcmp(op, "-ne") == 0) return left_value != right_value;
        if (strcmp(op, "-gt") == 0) return left_value > right_value;
        if (strcmp(op, "-ge") == 0) return left_value >= right_value;
        if (strcmp(op, "-lt") == 0) return left_value < right_value;
        if (strcmp(op, "-le") == 0) return left_value <= right_value;
    }
    *handled = 0;
    return 0;
}

static int parse_test_primary(struct test_parser *parser) {
    if (parser->pos >= parser->argc) {
        return 0;
    }
    char *arg = parser->argv[parser->pos++];
    if (strcmp(arg, "!") == 0) {
        return !parse_test_primary(parser);
    }
    if (parser->pos < parser->argc) {
        int file = test_file_unary(arg, parser->argv[parser->pos]);
        if (file >= 0) {
            parser->pos++;
            return file;
        }
    }
    if (parser->pos + 1 < parser->argc) {
        const char *op = parser->argv[parser->pos];
        const char *right = parser->argv[parser->pos + 1];
        int handled = 0;
        int result = test_binary_value(arg, op, right, &handled);
        if (handled) {
            parser->pos += 2;
            return result;
        }
    }
    return arg[0] != '\0';
}

static int parse_test_and(struct test_parser *parser) {
    int result = parse_test_primary(parser);
    while (parser->pos < parser->argc && strcmp(parser->argv[parser->pos], "-a") == 0) {
        parser->pos++;
        result = parse_test_primary(parser) && result;
    }
    return result;
}

static int parse_test_expr(struct test_parser *parser) {
    int result = parse_test_and(parser);
    while (parser->pos < parser->argc && strcmp(parser->argv[parser->pos], "-o") == 0) {
        parser->pos++;
        result = parse_test_and(parser) || result;
    }
    return result;
}

static int test_main_named(const char *tool, int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(tool);
        return 0;
    }
    int start = 1;
    int end = argc;
    if (strcmp(tool, "[") == 0) {
        if (argc < 2) {
            fputs("[: missing ]\n", stderr);
            return 2;
        }
        if (strcmp(argv[argc - 1], "]") == 0) {
            end--;
        }
    }
    if (end - start == 3) {
        int handled = 0;
        int result = test_binary_value(argv[start], argv[start + 1], argv[start + 2], &handled);
        if (handled) {
            return result ? 0 : 1;
        }
    }
    struct test_parser parser = {
        .argc = end - start,
        .argv = argv + start,
        .pos = 0,
    };
    int result = parse_test_expr(&parser);
    return parser.pos == parser.argc && result ? 0 : (parser.pos == parser.argc ? 1 : 2);
}

static uint32_t cksum_update(uint32_t crc, unsigned char byte) {
    crc ^= (uint32_t)byte << 24;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04c11db7u : crc << 1;
    }
    return crc;
}

static int cksum_one(const char *path, int print_name) {
    int fd = open_input(path);
    if (fd < 0) {
        fprintf(stderr, "cksum: cannot open %s\n", path);
        return 1;
    }
    uint32_t crc = 0;
    uint64_t length = 0;
    unsigned char buffer[512];
    for (;;) {
        ssize_t got = read(fd, buffer, sizeof(buffer));
        if (got < 0) {
            close_input(fd);
            return 1;
        }
        if (got == 0) {
            break;
        }
        for (ssize_t i = 0; i < got; i++) {
            crc = cksum_update(crc, buffer[i]);
        }
        length += (uint64_t)got;
    }
    for (uint64_t n = length; n != 0; n >>= 8) {
        crc = cksum_update(crc, (unsigned char)(n & 0xffu));
    }
    crc = ~crc;
    printf("%u %llu", crc, (unsigned long long)length);
    if (print_name && path != 0) {
        printf(" %s", path);
    }
    printf("\n");
    close_input(fd);
    return 0;
}

static int cksum_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("cksum");
        return 0;
    }
    if (argc == 1) {
        return cksum_one("-", 0);
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        status |= cksum_one(argv[i], 1);
    }
    return status;
}

static int sum_one(const char *path, int print_name) {
    int fd = open_input(path);
    if (fd < 0) {
        fprintf(stderr, "sum: cannot open %s\n", path);
        return 1;
    }
    uint32_t checksum = 0;
    uint64_t bytes = 0;
    unsigned char buffer[512];
    for (;;) {
        ssize_t got = read(fd, buffer, sizeof(buffer));
        if (got < 0) {
            close_input(fd);
            return 1;
        }
        if (got == 0) {
            break;
        }
        for (ssize_t i = 0; i < got; i++) {
            checksum = (checksum >> 1) + ((checksum & 1u) << 15);
            checksum = (checksum + buffer[i]) & 0xffffu;
        }
        bytes += (uint64_t)got;
    }
    printf("%u %llu", checksum & 0xffffu, (unsigned long long)((bytes + 1023u) / 1024u));
    if (print_name && path != 0) {
        printf(" %s", path);
    }
    printf("\n");
    close_input(fd);
    return 0;
}

static int sum_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("sum");
        return 0;
    }
    if (argc == 1) {
        return sum_one("-", 0);
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        status |= sum_one(argv[i], 1);
    }
    return status;
}

static int comm_main(int argc, char **argv) {
    int show1 = 1;
    int show2 = 1;
    int show3 = 1;
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("comm");
        return 0;
    }
    if (argc > 1 && argv[1][0] == '-') {
        for (size_t i = 1; argv[1][i] != '\0'; i++) {
            if (argv[1][i] == '1') show1 = 0;
            else if (argv[1][i] == '2') show2 = 0;
            else if (argv[1][i] == '3') show3 = 0;
            else {
                usage("comm");
                return 1;
            }
        }
        first = 2;
    }
    if (argc != first + 2) {
        usage("comm");
        return 1;
    }
    int a = open_input(argv[first]);
    int b = open_input(argv[first + 1]);
    if (a < 0 || b < 0) {
        close_input(a);
        close_input(b);
        return 1;
    }
    char la[LINE_MAX_LOCAL];
    char lb[LINE_MAX_LOCAL];
    int ha = read_line_fd(a, la, sizeof(la));
    int hb = read_line_fd(b, lb, sizeof(lb));
    while (ha > 0 || hb > 0) {
        int cmp = ha > 0 && hb > 0 ? strcmp(la, lb) : (ha > 0 ? -1 : 1);
        if (cmp < 0) {
            if (show1) fputs(la, stdout);
            ha = read_line_fd(a, la, sizeof(la));
        } else if (cmp > 0) {
            if (show2) {
                if (show1) putchar('\t');
                fputs(lb, stdout);
            }
            hb = read_line_fd(b, lb, sizeof(lb));
        } else {
            if (show3) {
                if (show1) putchar('\t');
                if (show2) putchar('\t');
                fputs(la, stdout);
            }
            ha = read_line_fd(a, la, sizeof(la));
            hb = read_line_fd(b, lb, sizeof(lb));
        }
    }
    close_input(a);
    close_input(b);
    return ha < 0 || hb < 0 ? 1 : 0;
}

static int paste_main(int argc, char **argv) {
    const char *delims = "\t";
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("paste");
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "-d") == 0) {
        delims = argv[2];
        first = 3;
    }
    if (argc <= first) {
        usage("paste");
        return 1;
    }
    int fds[16];
    int active[16];
    int count = argc - first;
    if (count > 16) count = 16;
    for (int i = 0; i < count; i++) {
        fds[i] = open_input(argv[first + i]);
        active[i] = fds[i] >= 0;
    }
    char lines[16][LINE_MAX_LOCAL];
    for (;;) {
        int any = 0;
        for (int i = 0; i < count; i++) {
            lines[i][0] = '\0';
            if (active[i]) {
                int got = read_line_fd(fds[i], lines[i], sizeof(lines[i]));
                if (got > 0) {
                    any = 1;
                    size_t len = strlen(lines[i]);
                    if (len != 0 && lines[i][len - 1] == '\n') {
                        lines[i][len - 1] = '\0';
                    }
                } else {
                    active[i] = 0;
                }
            }
        }
        if (!any) {
            break;
        }
        for (int i = 0; i < count; i++) {
            if (i != 0) {
                putchar(delims[(i - 1) % (int)strlen(delims)]);
            }
            fputs(lines[i], stdout);
        }
        putchar('\n');
    }
    for (int i = 0; i < count; i++) close_input(fds[i]);
    return 0;
}

static void split_field(char *line, char **key, char **rest) {
    while (*line != '\0' && isspace((unsigned char)*line)) line++;
    *key = line;
    while (*line != '\0' && !isspace((unsigned char)*line)) line++;
    if (*line != '\0') {
        *line++ = '\0';
    }
    while (*line != '\0' && isspace((unsigned char)*line)) line++;
    *rest = line;
    size_t len = strlen(*rest);
    if (len != 0 && (*rest)[len - 1] == '\n') {
        (*rest)[len - 1] = '\0';
    }
}

static int join_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("join");
        return 0;
    }
    if (argc != 3) {
        usage("join");
        return 1;
    }
    int a = open_input(argv[1]);
    if (a < 0) return 1;
    char la[LINE_MAX_LOCAL];
    while (read_line_fd(a, la, sizeof(la)) > 0) {
        char left_copy[LINE_MAX_LOCAL];
        strncpy(left_copy, la, sizeof(left_copy));
        left_copy[sizeof(left_copy) - 1] = '\0';
        char *left_key;
        char *left_rest;
        split_field(left_copy, &left_key, &left_rest);
        int b = open_input(argv[2]);
        if (b < 0) {
            close_input(a);
            return 1;
        }
        char lb[LINE_MAX_LOCAL];
        while (read_line_fd(b, lb, sizeof(lb)) > 0) {
            char *right_key;
            char *right_rest;
            split_field(lb, &right_key, &right_rest);
            if (strcmp(left_key, right_key) == 0) {
                printf("%s", left_key);
                if (*left_rest != '\0') printf(" %s", left_rest);
                if (*right_rest != '\0') printf(" %s", right_rest);
                printf("\n");
            }
        }
        close_input(b);
    }
    close_input(a);
    return 0;
}

static void split_suffix(char *out, size_t capacity, const char *prefix, unsigned index) {
    unsigned a = (index / 26u) % 26u;
    unsigned b = index % 26u;
    snprintf(out, capacity, "%s%c%c", prefix, 'a' + a, 'a' + b);
}

static int split_main(int argc, char **argv) {
    uint64_t limit = 1000;
    int by_bytes = 0;
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("split");
        return 0;
    }
    if (argc > 2 && (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "-b") == 0)) {
        by_bytes = strcmp(argv[1], "-b") == 0;
        if (!parse_u64(argv[2], &limit) || limit == 0) {
            usage("split");
            return 1;
        }
        first = 3;
    }
    const char *path = first < argc ? argv[first] : "-";
    const char *prefix = first + 1 < argc ? argv[first + 1] : "x";
    if (first + 2 < argc) {
        usage("split");
        return 1;
    }
    int in = open_input(path);
    if (in < 0) {
        return 1;
    }
    unsigned index = 0;
    uint64_t used = 0;
    int out = -1;
    char out_path[PATH_MAX_LOCAL];
    char buffer[256];
    for (;;) {
        if (out < 0 || used >= limit) {
            if (out >= 0) close(out);
            split_suffix(out_path, sizeof(out_path), prefix, index++);
            out = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out < 0) {
                close_input(in);
                return 1;
            }
            used = 0;
        }
        if (by_bytes) {
            size_t want = (size_t)(limit - used);
            if (want > sizeof(buffer)) want = sizeof(buffer);
            ssize_t got = read(in, buffer, want);
            if (got < 0) {
                close(out);
                close_input(in);
                return 1;
            }
            if (got == 0) {
                break;
            }
            if (write_all(out, buffer, (size_t)got) < 0) {
                close(out);
                close_input(in);
                return 1;
            }
            used += (uint64_t)got;
        } else {
            char line[LINE_MAX_LOCAL];
            int got = read_line_fd(in, line, sizeof(line));
            if (got < 0) {
                close(out);
                close_input(in);
                return 1;
            }
            if (got == 0) {
                break;
            }
            if (write_all(out, line, strlen(line)) < 0) {
                close(out);
                close_input(in);
                return 1;
            }
            used++;
        }
    }
    if (out >= 0) close(out);
    close_input(in);
    return 0;
}

static int dump_file(const char *path, int canonical) {
    int fd = open_input(path);
    if (fd < 0) return 1;
    unsigned char buffer[16];
    uint64_t offset = 0;
    for (;;) {
        ssize_t got = read(fd, buffer, sizeof(buffer));
        if (got < 0) {
            close_input(fd);
            return 1;
        }
        if (got == 0) break;
        printf("%08llx", (unsigned long long)offset);
        for (int i = 0; i < 16; i++) {
            if (i < got) printf(" %02x", buffer[i]);
            else printf("   ");
        }
        if (canonical) {
            printf("  |");
            for (int i = 0; i < got; i++) putchar(isprint(buffer[i]) ? buffer[i] : '.');
            printf("|");
        }
        printf("\n");
        offset += (uint64_t)got;
    }
    close_input(fd);
    return 0;
}

static int od_main(int argc, char **argv) {
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("od");
        return 0;
    }
    while (first < argc && argv[first][0] == '-') {
        if (strcmp(argv[first], "-An") == 0 || strcmp(argv[first], "-tx1") == 0) {
            first++;
        } else {
            break;
        }
    }
    if (first == argc) return dump_file("-", 0);
    int status = 0;
    for (int i = first; i < argc; i++) status |= dump_file(argv[i], 0);
    return status;
}

static int hexdump_main(int argc, char **argv) {
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("hexdump");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-C") == 0) first = 2;
    if (first == argc) return dump_file("-", 1);
    int status = 0;
    for (int i = first; i < argc; i++) status |= dump_file(argv[i], 1);
    return status;
}

static int strings_one(const char *path, int min_len) {
    int fd = open_input(path);
    if (fd < 0) return 1;
    char run[256];
    int used = 0;
    unsigned char c;
    while (read(fd, &c, 1) == 1) {
        if (isprint(c) || c == '\t') {
            if (used + 1 < (int)sizeof(run)) run[used++] = (char)c;
        } else {
            if (used >= min_len) {
                run[used] = '\0';
                puts(run);
            }
            used = 0;
        }
    }
    if (used >= min_len) {
        run[used] = '\0';
        puts(run);
    }
    close_input(fd);
    return 0;
}

static int strings_main(int argc, char **argv) {
    int min_len = 4;
    int first = 1;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("strings");
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        uint64_t parsed;
        if (!parse_u64(argv[2], &parsed) || parsed == 0 || parsed > 255) return 1;
        min_len = (int)parsed;
        first = 3;
    }
    if (first == argc) return strings_one("-", min_len);
    int status = 0;
    for (int i = first; i < argc; i++) status |= strings_one(argv[i], min_len);
    return status;
}

static int file_one(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("%s: cannot stat\n", path);
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        printf("%s: directory\n", path);
        return 0;
    }
    if (st.st_size == 0) {
        printf("%s: empty\n", path);
        return 0;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("%s: cannot open\n", path);
        return 1;
    }
    unsigned char buffer[512];
    ssize_t got = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (got >= 4 && buffer[0] == 0x7f && buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'F') {
        printf("%s: ELF 64-bit executable\n", path);
    } else if (got >= 2 && buffer[0] == 0x1f && buffer[1] == 0x8b) {
        printf("%s: gzip compressed data\n", path);
    } else if (got >= 4 && buffer[0] == 'P' && buffer[1] == 'K' && buffer[2] == 3 && buffer[3] == 4) {
        printf("%s: Zip archive data\n", path);
    } else if (got >= 262 && memcmp(buffer + 257, "ustar", 5) == 0) {
        printf("%s: POSIX tar archive\n", path);
    } else {
        int text = 1;
        for (ssize_t i = 0; i < got; i++) {
            if (buffer[i] == 0 || (!isprint(buffer[i]) && !isspace(buffer[i]))) {
                text = 0;
                break;
            }
        }
        printf("%s: %s\n", path, text ? "ASCII text" : "data");
    }
    return 0;
}

static int file_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("file");
        return 0;
    }
    if (argc < 2) {
        usage("file");
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) status |= file_one(argv[i]);
    return status;
}

static int tty_main(int argc, char **argv) {
    int silent = argc > 1 && strcmp(argv[1], "-s") == 0;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("tty");
        return 0;
    }
    if (isatty(STDIN_FILENO)) {
        if (!silent) puts("/dev/console");
        return 0;
    }
    if (!silent) puts("not a tty");
    return 1;
}

static int stty_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("stty");
        return 0;
    }
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) != 0) {
        perror("stty");
        return 1;
    }
    if (argc == 1 || strcmp(argv[1], "-a") == 0) {
        printf("speed 0 baud; rows cols unknown; %s %s %s\n",
            (t.c_lflag & ICANON) ? "icanon" : "-icanon",
            (t.c_lflag & ECHO) ? "echo" : "-echo",
            (t.c_iflag & ICRNL) ? "icrnl" : "-icrnl");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "raw") == 0) {
            t.c_lflag &= ~(ICANON | ECHO);
            t.c_iflag &= ~ICRNL;
            t.c_cc[VMIN] = 1;
            t.c_cc[VTIME] = 0;
        } else if (strcmp(argv[i], "cooked") == 0 || strcmp(argv[i], "sane") == 0) {
            t.c_lflag |= ICANON | ECHO | ISIG;
            t.c_iflag |= ICRNL;
        } else if (strcmp(argv[i], "echo") == 0) {
            t.c_lflag |= ECHO;
        } else if (strcmp(argv[i], "-echo") == 0) {
            t.c_lflag &= ~ECHO;
        } else {
            usage("stty");
            return 1;
        }
    }
    return tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0 ? 0 : 1;
}

static int spawn_and_wait(char **argv, uint64_t timeout_seconds, int use_timeout, int report_time) {
    pid_t pid;
    uint64_t start = (uint64_t)clock();
    int error = posix_spawnp(&pid, argv[0], 0, 0, argv, environ);
    if (error != 0) {
        fprintf(stderr, "%s: command not found\n", argv[0]);
        return 127;
    }
    int status = 0;
    for (;;) {
        pid_t got = waitpid(pid, &status, use_timeout ? WNOHANG : 0);
        if (got == pid) {
            break;
        }
        if (!use_timeout) {
            return 1;
        }
        uint64_t now = (uint64_t)clock();
        if (now - start >= timeout_seconds * 100u) {
            srv_kill((uint64_t)pid);
            (void)waitpid(pid, &status, 0);
            return 124;
        }
        usleep(10000);
    }
    if (report_time) {
        uint64_t end = (uint64_t)clock();
        fprintf(stderr, "real %llu ticks\n", (unsigned long long)(end - start));
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
}

static int time_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("time");
        return 0;
    }
    if (argc < 2) {
        usage("time");
        return 1;
    }
    return spawn_and_wait(&argv[1], 0, 0, 1);
}

static int timeout_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("timeout");
        return 0;
    }
    uint64_t seconds;
    if (argc < 3 || !parse_u64(argv[1], &seconds)) {
        usage("timeout");
        return 1;
    }
    return spawn_and_wait(&argv[2], seconds, 1, 0);
}

static int nohup_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("nohup");
        return 0;
    }
    if (argc < 2) {
        usage("nohup");
        return 1;
    }
    return spawn_and_wait(&argv[1], 0, 0, 0);
}

static int nice_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage("nice");
        return 0;
    }
    int first = 1;
    if (argc > 3 && strcmp(argv[1], "-n") == 0) {
        first = 3;
    } else if (argc > 1 && argv[1][0] == '-' && isdigit((unsigned char)argv[1][1])) {
        first = 2;
    }
    if (argc <= first) {
        usage("nice");
        return 1;
    }
    return spawn_and_wait(&argv[first], 0, 0, 0);
}

int main(int argc, char **argv) {
    const char *tool = tool_name(argc > 0 ? argv[0] : "");
    if (strcmp(tool, "ln") == 0) return unsupported_link(argc, argv);
    if (strcmp(tool, "sync") == 0) return sync_main(argc, argv);
    if (strcmp(tool, "test") == 0 || strcmp(tool, "[") == 0) return test_main_named(tool, argc, argv);
    if (strcmp(tool, "cksum") == 0) return cksum_main(argc, argv);
    if (strcmp(tool, "sum") == 0) return sum_main(argc, argv);
    if (strcmp(tool, "comm") == 0) return comm_main(argc, argv);
    if (strcmp(tool, "paste") == 0) return paste_main(argc, argv);
    if (strcmp(tool, "join") == 0) return join_main(argc, argv);
    if (strcmp(tool, "split") == 0) return split_main(argc, argv);
    if (strcmp(tool, "od") == 0) return od_main(argc, argv);
    if (strcmp(tool, "hexdump") == 0) return hexdump_main(argc, argv);
    if (strcmp(tool, "strings") == 0) return strings_main(argc, argv);
    if (strcmp(tool, "file") == 0) return file_main(argc, argv);
    if (strcmp(tool, "tty") == 0) return tty_main(argc, argv);
    if (strcmp(tool, "stty") == 0) return stty_main(argc, argv);
    if (strcmp(tool, "time") == 0) return time_main(argc, argv);
    if (strcmp(tool, "timeout") == 0) return timeout_main(argc, argv);
    if (strcmp(tool, "nohup") == 0) return nohup_main(argc, argv);
    if (strcmp(tool, "nice") == 0) return nice_main(argc, argv);
    fputs("posixutils: unknown applet\n", stderr);
    return 127;
}
