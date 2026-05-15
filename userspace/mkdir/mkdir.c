#include <srvros/cli.h>
#include <srvros/sys.h>

#include <stdlib.h>

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

static int mkdir_one(const char *path, int parents) {
    char partial[CLI_PATH_MAX];
    size_t out = 0;

    if (!parents) {
        if (srv_mkdir(path) == 0 || path_is_dir(path)) {
            return 0;
        }
        return 1;
    }

    if (path == 0 || path[0] == '\0') {
        return 1;
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

int main(int argc, char **argv) {
    int parents = 0;
    int first_path = 1;
    int status = 0;

    if (argc > 1 && cli_streq(argv[1], "-p")) {
        parents = 1;
        first_path = 2;
    }
    if (argc <= first_path) {
        cli_puts("usage: mkdir [-p] <path> [...]\n");
        return 1;
    }

    for (int i = first_path; i < argc; i++) {
        char normalized[CLI_PATH_MAX];
        const char *pwd = getenv("PWD");
        cli_normalize_path(normalized, sizeof(normalized), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[i]);
        if (mkdir_one(normalized, parents) != 0) {
            cli_puts("mkdir: failed: ");
            cli_puts(argv[i]);
            cli_puts("\n");
            status = 1;
        }
    }
    return status;
}
