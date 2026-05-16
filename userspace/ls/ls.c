#include <srvros/cli.h>
#include <srvros/sys.h>

#include <stdlib.h>

#define CHILD_MAX 64
#define NAME_MAX 64

static int seen(char names[CHILD_MAX][NAME_MAX], size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (cli_streq(names[i], name)) {
            return 1;
        }
    }
    return 0;
}

static void remember(char names[CHILD_MAX][NAME_MAX], size_t *count, const char *name) {
    if (*count >= CHILD_MAX || seen(names, *count, name)) {
        return;
    }
    cli_copy(names[*count], NAME_MAX, name);
    (*count)++;
}

static void print_entry(const char *name, uint64_t size, int dir, int long_format) {
    if (long_format) {
        cli_puts(dir ? "d " : "- ");
        cli_putn(size);
        cli_puts(" ");
    }
    cli_puts(name);
    if (dir) {
        cli_puts("/");
    }
    cli_puts("\n");
}

static int path_is_directory(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.type == 1;
}

static int hidden_name(const char *name) {
    return name[0] == '.';
}

static void trim_trailing_slashes(char *path) {
    size_t len = cli_strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }
}

static void build_prefix(char *prefix, size_t capacity, const char *target) {
    if (cli_streq(target, "/")) {
        cli_copy(prefix, capacity, "/");
    } else {
        cli_join_path(prefix, capacity, target, "");
    }
}

static int list_target(const char *target_arg, int long_format, int all, int directory_only, int show_header) {
    char target[CLI_PATH_MAX];
    char prefix[CLI_PATH_MAX];
    char path[CLI_PATH_MAX];
    char names[CHILD_MAX][NAME_MAX];
    size_t name_count = 0;
    uint64_t size = 0;
    int found = 0;
    int target_is_dir = 0;

    const char *pwd = getenv("PWD");
    cli_normalize_path(target, sizeof(target), pwd != 0 && pwd[0] != '\0' ? pwd : "/", target_arg);
    trim_trailing_slashes(target);
    build_prefix(prefix, sizeof(prefix), target);

    if (show_header) {
        cli_puts(target);
        cli_puts(":\n");
    }
    target_is_dir = path_is_directory(target);
    if (directory_only) {
        struct srv_stat info;
        if (srv_stat(target, &info) == 0) {
            print_entry(target, info.size, target_is_dir, long_format);
            return 0;
        }
    }
    if (!target_is_dir) {
        struct srv_stat info;
        if (srv_stat(target, &info) == 0) {
            print_entry(target, info.size, 0, long_format);
            return 0;
        }
    }

    for (uint64_t i = 0;; i++) {
        long result = srv_list(i, path, sizeof(path), &size);
        if (result <= 0) {
            break;
        }
        if (cli_streq(path, target)) {
            found = 1;
            continue;
        }
        if (!cli_starts_with(path, prefix)) {
            continue;
        }

        const char *rest = path + cli_strlen(prefix);
        char child[NAME_MAX];
        size_t child_len = 0;
        int dir = 0;
        while (rest[child_len] != '\0' && rest[child_len] != '/' && child_len + 1 < sizeof(child)) {
            child[child_len] = rest[child_len];
            child_len++;
        }
        child[child_len] = '\0';
        if (child[0] == '\0') {
            continue;
        }
        if (!all && hidden_name(child)) {
            continue;
        }
        dir = rest[child_len] == '/';
        if (!dir) {
            dir = path_is_directory(path);
        }
        if (dir) {
            if (!seen(names, name_count, child)) {
                print_entry(child, 0, 1, long_format);
                remember(names, &name_count, child);
            }
        } else {
            print_entry(child, size, 0, long_format);
        }
        found = 1;
    }
    if (!found) {
        cli_puts("ls: not found: ");
        cli_puts(target);
        cli_puts("\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *targets[16];
    int target_count = 0;
    int long_format = 0;
    int all = 0;
    int directory_only = 0;
    int status = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (size_t j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'l') {
                    long_format = 1;
                } else if (argv[i][j] == 'a') {
                    all = 1;
                } else if (argv[i][j] == 'd') {
                    directory_only = 1;
                } else if (argv[i][j] == '1') {
                    continue;
                } else {
                    cli_puts("usage: ls [-1adl] [path ...]\n");
                    return 1;
                }
            }
        } else if (target_count < (int)(sizeof(targets) / sizeof(targets[0]))) {
            targets[target_count++] = argv[i];
        } else {
            cli_puts("ls: too many paths\n");
            return 1;
        }
    }
    if (target_count == 0) {
        targets[target_count++] = "/";
    }
    for (int i = 0; i < target_count; i++) {
        if (i > 0) {
            cli_puts("\n");
        }
        if (list_target(targets[i], long_format, all, directory_only, target_count > 1) != 0) {
            status = 1;
        }
    }
    return status;
}
