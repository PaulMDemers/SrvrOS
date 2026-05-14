#include <srvros/cli.h>
#include <srvros/sys.h>

static const char *path_entries[] = {
    "/fat/bin",
    "/",
    "/fat",
};

static int path_exists(const char *path) {
    for (uint64_t index = 0;; index++) {
        char listed[CLI_PATH_MAX];
        uint64_t size = 0;
        long result = srv_list(index, listed, sizeof(listed), &size);
        if (result <= 0) {
            break;
        }
        if (cli_streq(listed, path)) {
            return 1;
        }
    }
    long fd = srv_open(path);
    if (fd >= 0) {
        srv_close((int)fd);
        return 1;
    }
    return 0;
}

static int resolve_command(char *out, size_t capacity, const char *command) {
    if (cli_contains_slash(command)) {
        cli_copy(out, capacity, command);
        return path_exists(out);
    }
    for (size_t i = 0; i < sizeof(path_entries) / sizeof(path_entries[0]); i++) {
        if (cli_join_path(out, capacity, path_entries[i], command) && path_exists(out)) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int missing = 0;
    if (argc < 2) {
        cli_puts("usage: which <command> [...]\n");
        return 2;
    }
    for (int i = 1; i < argc; i++) {
        char path[CLI_PATH_MAX];
        if (resolve_command(path, sizeof(path), argv[i])) {
            cli_puts(path);
            cli_puts("\n");
        } else {
            missing = 1;
        }
    }
    return missing;
}
