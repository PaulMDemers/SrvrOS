#include <srvros/cli.h>
#include <srvros/sys.h>

#include <stdlib.h>

#define RM_MAX_PATHS 256

struct rm_path {
    char path[CLI_PATH_MAX];
    uint64_t type;
};

static struct rm_path paths[RM_MAX_PATHS];
static size_t path_count;

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

static void sort_deepest_first(void) {
    for (size_t i = 0; i < path_count; i++) {
        for (size_t j = i + 1; j < path_count; j++) {
            if (cli_strlen(paths[j].path) > cli_strlen(paths[i].path)) {
                struct rm_path temp = paths[i];
                paths[i] = paths[j];
                paths[j] = temp;
            }
        }
    }
}

static int remove_one_file(const char *path, int force) {
    if (srv_unlink(path) == 0) {
        return 0;
    }
    if (force) {
        struct srv_stat info;
        if (srv_stat(path, &info) < 0) {
            return 0;
        }
    }
    cli_puts("rm: cannot remove ");
    cli_puts(path);
    cli_puts("\n");
    return 1;
}

static int remove_recursive(const char *root, int force) {
    struct srv_stat root_info;
    int status = 0;
    path_count = 0;

    if (srv_stat(root, &root_info) < 0) {
        if (force) {
            return 0;
        }
        cli_puts("rm: not found: ");
        cli_puts(root);
        cli_puts("\n");
        return 1;
    }
    if (root_info.type == 0) {
        return remove_one_file(root, force);
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
        if (cli_streq(path, root) || !under_root(path, root)) {
            continue;
        }
        if (path_count >= RM_MAX_PATHS || srv_stat(path, &info) < 0) {
            cli_puts("rm: recursion limit reached\n");
            return 1;
        }
        cli_copy(paths[path_count].path, sizeof(paths[path_count].path), path);
        paths[path_count].type = info.type;
        path_count++;
    }

    sort_deepest_first();
    for (size_t i = 0; i < path_count; i++) {
        if (paths[i].type == 0) {
            status |= remove_one_file(paths[i].path, force);
        }
    }
    for (size_t i = 0; i < path_count; i++) {
        if (paths[i].type == 1 && srv_rmdir(paths[i].path) < 0) {
            cli_puts("rm: cannot remove directory ");
            cli_puts(paths[i].path);
            cli_puts("\n");
            status = 1;
        }
    }
    if (srv_rmdir(root) < 0) {
        cli_puts("rm: cannot remove directory ");
        cli_puts(root);
        cli_puts("\n");
        status = 1;
    }
    return status;
}

int main(int argc, char **argv) {
    int recursive = 0;
    int force = 0;
    int first_path = 1;
    int status = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: rm [-fRr] <path> [...]\n");
        return 0;
    }
    while (first_path < argc && argv[first_path][0] == '-' && argv[first_path][1] != '\0') {
        if (cli_is_option_terminator(argv[first_path])) {
            first_path++;
            break;
        }
        for (size_t i = 1; argv[first_path][i] != '\0'; i++) {
            if (argv[first_path][i] == 'r' || argv[first_path][i] == 'R') {
                recursive = 1;
            } else if (argv[first_path][i] == 'f') {
                force = 1;
            } else {
                cli_puts("usage: rm [-fRr] <path> [...]\n");
                return 1;
            }
        }
        first_path++;
    }
    if (argc <= first_path) {
        if (force) {
            return 0;
        }
        cli_puts("usage: rm [-fRr] <path> [...]\n");
        return 1;
    }

    for (int i = first_path; i < argc; i++) {
        struct srv_stat info;
        char normalized[CLI_PATH_MAX];
        const char *pwd = getenv("PWD");
        cli_normalize_path(normalized, sizeof(normalized), pwd != 0 && pwd[0] != '\0' ? pwd : "/", argv[i]);
        if (srv_stat(normalized, &info) == 0 && info.type == 1) {
            if (!recursive) {
                cli_puts("rm: is a directory: ");
                cli_puts(argv[i]);
                cli_puts("\n");
                status = 1;
            } else {
                status |= remove_recursive(normalized, force);
            }
        } else {
            status |= remove_one_file(normalized, force);
        }
    }
    return status;
}
