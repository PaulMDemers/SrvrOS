#include <srvros/cli.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define INSTALL_BUFFER_SIZE 2048

static char copy_buffer[INSTALL_BUFFER_SIZE];

static void usage(void) {
    cli_puts("usage: install [-d] [-D] [-m mode] source dest | install -d dir [...]\n");
}

static int parse_mode(const char *text, mode_t *mode_out) {
    mode_t mode = 0;
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '7') {
            return 0;
        }
        mode = (mode_t)((mode << 3) | (mode_t)(text[i] - '0'));
    }
    *mode_out = mode;
    return 1;
}

static int path_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char *base_name(const char *path) {
    const char *base = path;
    for (const char *cursor = path; cursor != 0 && *cursor != '\0'; cursor++) {
        if (*cursor == '/' && cursor[1] != '\0') {
            base = cursor + 1;
        }
    }
    return base;
}

static int looks_like_mount_root(const char *path) {
    if (path == 0 || path[0] != '/' || path[1] == '\0') {
        return 0;
    }
    for (size_t i = 1; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            return 0;
        }
    }
    return 1;
}

static int mkdir_p(const char *path, mode_t mode) {
    char partial[CLI_PATH_MAX];
    size_t out = 0;
    if (path == 0 || path[0] == '\0' || path_is_dir(path)) {
        return 0;
    }
    if (path[0] == '/') {
        partial[out++] = '/';
        partial[out] = '\0';
    }
    for (size_t i = path[0] == '/' ? 1 : 0;; i++) {
        char c = path[i];
        if (c == '/' || c == '\0') {
            if (out > 0 && !cli_streq(partial, "/") && !looks_like_mount_root(partial)) {
                if (mkdir(partial, mode) < 0 && !path_is_dir(partial)) {
                    return 1;
                }
                (void)chmod(partial, mode);
            }
            while (path[i] == '/') {
                i++;
            }
            if (path[i] == '\0') {
                break;
            }
            if (out > 0 && partial[out - 1] != '/' && out + 1 < sizeof(partial)) {
                partial[out++] = '/';
                partial[out] = '\0';
            }
            i--;
            continue;
        }
        if (out + 1 >= sizeof(partial)) {
            return 1;
        }
        partial[out++] = c;
        partial[out] = '\0';
    }
    return path_is_dir(path) ? 0 : 1;
}

static int parent_dir(char *out, size_t capacity, const char *path) {
    size_t slash = 0;
    int found = 0;
    for (size_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            slash = i;
            found = 1;
        }
    }
    if (!found) {
        cli_copy(out, capacity, ".");
        return 1;
    }
    if (slash == 0) {
        cli_copy(out, capacity, "/");
        return 1;
    }
    if (slash + 1 > capacity) {
        return 0;
    }
    for (size_t i = 0; i < slash && i + 1 < capacity; i++) {
        out[i] = path[i];
        out[i + 1] = '\0';
    }
    return 1;
}

static int copy_file(const char *source, const char *dest, int make_parent, mode_t mode) {
    char parent[CLI_PATH_MAX];
    int in_fd = open(source, O_RDONLY);
    if (in_fd < 0) {
        cli_puts("install: cannot open: ");
        cli_puts(source);
        cli_puts("\n");
        return 1;
    }
    if (make_parent && parent_dir(parent, sizeof(parent), dest) && !cli_streq(parent, ".") && mkdir_p(parent, 0755) != 0) {
        close(in_fd);
        cli_puts("install: cannot create parent: ");
        cli_puts(parent);
        cli_puts("\n");
        return 1;
    }
    int out_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (out_fd < 0) {
        close(in_fd);
        cli_puts("install: cannot create: ");
        cli_puts(dest);
        cli_puts("\n");
        return 1;
    }
    for (;;) {
        ssize_t count = read(in_fd, copy_buffer, sizeof(copy_buffer));
        if (count < 0) {
            close(in_fd);
            close(out_fd);
            cli_puts("install: read failed\n");
            return 1;
        }
        if (count == 0) {
            break;
        }
        if (write(out_fd, copy_buffer, (size_t)count) != count) {
            close(in_fd);
            close(out_fd);
            cli_puts("install: write failed\n");
            return 1;
        }
    }
    close(in_fd);
    if (close(out_fd) < 0) {
        cli_puts("install: close failed\n");
        return 1;
    }
    (void)chmod(dest, mode);
    return 0;
}

int main(int argc, char **argv) {
    int directories = 0;
    int make_parent = 0;
    mode_t mode = 0755;
    int first_path = 1;
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        usage();
        return 0;
    }
    while (first_path < argc && argv[first_path][0] == '-' && argv[first_path][1] != '\0') {
        if (cli_is_option_terminator(argv[first_path])) {
            first_path++;
            break;
        }
        if (cli_streq(argv[first_path], "-d") || cli_streq(argv[first_path], "--directory")) {
            directories = 1;
            first_path++;
            continue;
        }
        if (cli_streq(argv[first_path], "-D")) {
            make_parent = 1;
            first_path++;
            continue;
        }
        if (cli_streq(argv[first_path], "-c") || cli_streq(argv[first_path], "-s")) {
            first_path++;
            continue;
        }
        if ((cli_streq(argv[first_path], "-m") || cli_streq(argv[first_path], "--mode")) && first_path + 1 < argc) {
            if (!parse_mode(argv[first_path + 1], &mode)) {
                cli_puts("install: invalid mode\n");
                return 2;
            }
            first_path += 2;
            continue;
        }
        if (cli_starts_with(argv[first_path], "--mode=")) {
            if (!parse_mode(argv[first_path] + 7, &mode)) {
                cli_puts("install: invalid mode\n");
                return 2;
            }
            first_path++;
            continue;
        }
        usage();
        return 2;
    }

    if (directories) {
        if (first_path >= argc) {
            usage();
            return 2;
        }
        for (int i = first_path; i < argc; i++) {
            if (mkdir_p(argv[i], mode) != 0) {
                cli_puts("install: cannot create directory: ");
                cli_puts(argv[i]);
                cli_puts("\n");
                status = 1;
            }
        }
        return status;
    }

    int path_count = argc - first_path;
    if (path_count < 2) {
        usage();
        return 2;
    }
    const char *dest = argv[argc - 1];
    int dest_is_dir = path_is_dir(dest);
    if (path_count > 2 && !dest_is_dir) {
        cli_puts("install: destination is not a directory\n");
        return 1;
    }
    for (int i = first_path; i < argc - 1; i++) {
        char mapped[CLI_PATH_MAX];
        const char *target = dest;
        if (dest_is_dir) {
            if (!cli_join_path(mapped, sizeof(mapped), dest, base_name(argv[i]))) {
                cli_puts("install: path too long\n");
                status = 1;
                continue;
            }
            target = mapped;
        }
        if (copy_file(argv[i], target, make_parent, mode) != 0) {
            status = 1;
        }
    }
    return status;
}
