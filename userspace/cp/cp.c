#include <srvros/cli.h>
#include <srvros/sys.h>

#include <stdlib.h>

#define CP_BUFFER_SIZE 2048
#define CP_MAX_PATHS 256

static char copy_buffer[CP_BUFFER_SIZE];

static int path_is_dir(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.type == 1;
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

static const char *base_name(const char *path) {
    const char *base = path;
    for (const char *cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/' && cursor[1] != '\0') {
            base = cursor + 1;
        }
    }
    return base;
}

static int under_root(const char *path, const char *root) {
    size_t root_length = cli_strlen(root);
    if (cli_streq(root, "/")) {
        return path[0] == '/';
    }
    if (!cli_starts_with(path, root)) {
        return 0;
    }
    return path[root_length] == '\0' || path[root_length] == '/';
}

static int mkdir_p(const char *path) {
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
                if (srv_mkdir(partial) < 0 && !path_is_dir(partial)) {
                    return 1;
                }
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

static int copy_file(const char *source, const char *dest) {
    int in_fd = (int)srv_open(source);
    int out_fd;
    char parent[CLI_PATH_MAX];

    if (in_fd < 0) {
        cli_puts("cp: cannot open source ");
        cli_puts(source);
        cli_puts("\n");
        return 1;
    }
    if (parent_dir(parent, sizeof(parent), dest) && !cli_streq(parent, ".") && mkdir_p(parent) != 0) {
        srv_close(in_fd);
        cli_puts("cp: cannot create parent ");
        cli_puts(parent);
        cli_puts("\n");
        return 1;
    }
    out_fd = (int)srv_open_mode(dest, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (out_fd < 0) {
        srv_close(in_fd);
        cli_puts("cp: cannot open dest ");
        cli_puts(dest);
        cli_puts("\n");
        return 1;
    }

    for (;;) {
        long count = srv_read(in_fd, copy_buffer, sizeof(copy_buffer));
        if (count < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("cp: read failed\n");
            return 1;
        }
        if (count == 0) {
            break;
        }
        if (srv_write(out_fd, copy_buffer, (size_t)count) != count) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("cp: write failed\n");
            return 1;
        }
    }
    srv_close(in_fd);
    if (srv_close(out_fd) < 0) {
        cli_puts("cp: close failed\n");
        return 1;
    }
    return 0;
}

static int map_dest(char *out, size_t capacity, const char *source_root, const char *dest_root, const char *source_path) {
    size_t root_length = cli_strlen(source_root);
    if (cli_streq(source_path, source_root)) {
        cli_copy(out, capacity, dest_root);
        return 1;
    }
    return cli_join_path(out, capacity, dest_root, source_path + root_length + 1);
}

static int copy_recursive(const char *source, const char *dest) {
    char final_dest[CLI_PATH_MAX];
    char listed[CP_MAX_PATHS][CLI_PATH_MAX];
    uint64_t listed_type[CP_MAX_PATHS];
    size_t listed_count = 0;
    int status = 0;

    if (path_is_dir(dest)) {
        if (!cli_join_path(final_dest, sizeof(final_dest), dest, base_name(source))) {
            return 1;
        }
    } else {
        cli_copy(final_dest, sizeof(final_dest), dest);
    }
    if (mkdir_p(final_dest) != 0) {
        cli_puts("cp: cannot create directory ");
        cli_puts(final_dest);
        cli_puts("\n");
        return 1;
    }

    for (uint64_t index = 0;; index++) {
        char path[CLI_PATH_MAX];
        uint64_t size = 0;
        struct srv_stat info;
        long result = srv_list(index, path, sizeof(path), &size);
        if (result <= 0) {
            break;
        }
        (void)size;
        if (cli_streq(path, source) || !under_root(path, source)) {
            continue;
        }
        if (listed_count >= CP_MAX_PATHS || srv_stat(path, &info) < 0) {
            cli_puts("cp: recursion limit reached\n");
            return 1;
        }
        cli_copy(listed[listed_count], sizeof(listed[listed_count]), path);
        listed_type[listed_count] = info.type;
        listed_count++;
    }

    for (size_t i = 0; i < listed_count; i++) {
        if (listed_type[i] == 1) {
            char mapped[CLI_PATH_MAX];
            if (!map_dest(mapped, sizeof(mapped), source, final_dest, listed[i]) || mkdir_p(mapped) != 0) {
                cli_puts("cp: cannot create directory ");
                cli_puts(mapped);
                cli_puts("\n");
                status = 1;
            }
        }
    }
    for (size_t i = 0; i < listed_count; i++) {
        if (listed_type[i] == 0) {
            char mapped[CLI_PATH_MAX];
            if (!map_dest(mapped, sizeof(mapped), source, final_dest, listed[i]) ||
                copy_file(listed[i], mapped) != 0) {
                status = 1;
            }
        }
    }
    return status;
}

int main(int argc, char **argv) {
    int recursive = 0;
    int first_path = 1;
    struct srv_stat info;
    char source_path[CLI_PATH_MAX];
    char dest_path[CLI_PATH_MAX];
    char dest[CLI_PATH_MAX];

    if (argc > 1 && (cli_streq(argv[1], "-r") || cli_streq(argv[1], "-R"))) {
        recursive = 1;
        first_path = 2;
    }
    if (argc != first_path + 2) {
        cli_puts("usage: cp [-r] <source> <dest>\n");
        return 1;
    }
    const char *pwd = getenv("PWD");
    cli_normalize_path(source_path, sizeof(source_path), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[first_path]);
    cli_normalize_path(dest_path, sizeof(dest_path), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[first_path + 1]);

    if (srv_stat(source_path, &info) < 0) {
        cli_puts("cp: not found: ");
        cli_puts(argv[first_path]);
        cli_puts("\n");
        return 1;
    }
    if (info.type == 1) {
        if (!recursive) {
            cli_puts("cp: is a directory: ");
            cli_puts(argv[first_path]);
            cli_puts("\n");
            return 1;
        }
        return copy_recursive(source_path, dest_path);
    }

    if (path_is_dir(dest_path)) {
        if (!cli_join_path(dest, sizeof(dest), dest_path, base_name(source_path))) {
            return 1;
        }
    } else {
        cli_copy(dest, sizeof(dest), dest_path);
    }
    return copy_file(source_path, dest);
}
