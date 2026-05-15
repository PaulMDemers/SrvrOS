#include <srvros/cli.h>
#include <srvros/sys.h>

static int glob_match(const char *pattern, const char *text) {
    if (*pattern == '\0') {
        return *text == '\0';
    }
    if (*pattern == '*') {
        while (pattern[1] == '*') {
            pattern++;
        }
        if (glob_match(pattern + 1, text)) {
            return 1;
        }
        return *text != '\0' && glob_match(pattern, text + 1);
    }
    if (*pattern == '?') {
        return *text != '\0' && glob_match(pattern + 1, text + 1);
    }
    return *pattern == *text && glob_match(pattern + 1, text + 1);
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

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "/";
    const char *name_pattern = 0;
    int printed_root = 0;

    if (argc == 4 && cli_streq(argv[2], "-name")) {
        name_pattern = argv[3];
    } else if (argc != 1 && argc != 2) {
        cli_puts("usage: find [path] [-name pattern]\n");
        return 1;
    }

    for (uint64_t index = 0;; index++) {
        char path[CLI_PATH_MAX];
        uint64_t size = 0;
        long result = srv_list(index, path, sizeof(path), &size);
        if (result <= 0) {
            break;
        }
        (void)size;
        if (!under_root(path, root)) {
            continue;
        }
        if (cli_streq(path, root)) {
            printed_root = 1;
        }
        if (name_pattern != 0 && !glob_match(name_pattern, base_name(path))) {
            continue;
        }
        cli_puts(path);
        cli_puts("\n");
    }

    if (!printed_root && name_pattern == 0) {
        struct srv_stat info;
        if (srv_stat(root, &info) == 0) {
            cli_puts(root);
            cli_puts("\n");
            return 0;
        }
    }
    return 0;
}
