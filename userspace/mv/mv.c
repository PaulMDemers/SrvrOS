#include <srvros/cli.h>
#include <srvros/sys.h>

#include <stdlib.h>

static char buffer[2048];

static int stat_type(const char *path, uint64_t *type_out) {
    struct srv_stat info;
    if (srv_stat(path, &info) < 0) {
        return 0;
    }
    if (type_out != 0) {
        *type_out = info.type;
    }
    return 1;
}

static int path_is_dir(const char *path) {
    uint64_t type = 0;
    return stat_type(path, &type) && type == 1;
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

static int copy_path(char *out, size_t capacity, const char *path) {
    if (cli_strlen(path) >= capacity) {
        return 0;
    }
    cli_copy(out, capacity, path);
    return 1;
}

static int copy_file(const char *source, const char *dest) {
    int in_fd = (int)srv_open(source);
    int out_fd;

    if (in_fd < 0) {
        cli_puts("mv: cannot open source ");
        cli_puts(source);
        cli_puts("\n");
        return 1;
    }
    out_fd = (int)srv_open_mode(dest, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (out_fd < 0) {
        srv_close(in_fd);
        cli_puts("mv: cannot open dest ");
        cli_puts(dest);
        cli_puts("\n");
        return 1;
    }

    for (;;) {
        long count = srv_read(in_fd, buffer, sizeof(buffer));
        if (count < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("mv: read failed\n");
            return 1;
        }
        if (count == 0) {
            break;
        }
        if (srv_write(out_fd, buffer, (size_t)count) != count) {
            srv_close(in_fd);
            srv_close(out_fd);
            cli_puts("mv: write failed\n");
            return 1;
        }
    }
    srv_close(in_fd);
    if (srv_close(out_fd) < 0) {
        cli_puts("mv: close failed\n");
        return 1;
    }
    if (srv_unlink(source) != 0) {
        cli_puts("mv: copied but cannot remove source\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    char source[CLI_PATH_MAX];
    char target[CLI_PATH_MAX];
    char dest[CLI_PATH_MAX];
    uint64_t source_type = 0;

    if (argc != 3) {
        cli_puts("usage: mv <source> <dest>\n");
        return 1;
    }
    const char *pwd = getenv("PWD");
    cli_normalize_path(source, sizeof(source), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[1]);
    cli_normalize_path(target, sizeof(target), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[2]);

    if (!stat_type(source, &source_type)) {
        cli_puts("mv: not found: ");
        cli_puts(argv[1]);
        cli_puts("\n");
        return 1;
    }
    if (path_is_dir(target)) {
        if (!cli_join_path(dest, sizeof(dest), target, base_name(source))) {
            cli_puts("mv: destination path too long\n");
            return 1;
        }
    } else {
        if (!copy_path(dest, sizeof(dest), target)) {
            cli_puts("mv: destination path too long\n");
            return 1;
        }
    }

    if (srv_rename(source, dest) == 0) {
        return 0;
    }
    if (source_type == 1) {
        cli_puts("mv: cannot move directory: ");
        cli_puts(argv[1]);
        cli_puts("\n");
        return 1;
    }
    return copy_file(source, dest);
}
