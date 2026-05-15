#include <srvros/cli.h>
#include <srvros/sys.h>

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

int main(int argc, char **argv) {
    char target[CLI_PATH_MAX] = "/";
    char prefix[CLI_PATH_MAX];
    char path[CLI_PATH_MAX];
    char names[CHILD_MAX][NAME_MAX];
    size_t name_count = 0;
    uint64_t size = 0;
    int long_format = 0;
    int found = 0;
    int target_is_dir = 0;

    for (int i = 1; i < argc; i++) {
        if (cli_streq(argv[i], "-l")) {
            long_format = 1;
        } else {
            cli_copy(target, sizeof(target), argv[i]);
        }
    }

    size_t len = cli_strlen(target);
    while (len > 1 && target[len - 1] == '/') {
        target[--len] = '\0';
    }
    if (cli_streq(target, "/")) {
        cli_copy(prefix, sizeof(prefix), "/");
    } else {
        cli_join_path(prefix, sizeof(prefix), target, "");
    }

    target_is_dir = path_is_directory(target);
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
        cli_puts("ls: not found\n");
        return 1;
    }
    return 0;
}
