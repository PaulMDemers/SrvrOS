#include <srvros/cli.h>
#include <srvros/sys.h>

#include <stdlib.h>

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
    const char *root = "/";
    char normalized_root[CLI_PATH_MAX];
    const char *name_pattern = 0;
    int type_filter = 0;
    int printed_root = 0;

    int index = 1;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: find [path] [-name pattern] [-type f|d]\n");
        return 0;
    }
    if (index < argc && cli_is_option_terminator(argv[index])) {
        index++;
    }
    if (index < argc && argv[index][0] != '-') {
        root = argv[index++];
    }
    while (index < argc) {
        if (cli_is_option_terminator(argv[index])) {
            index++;
        } else if (cli_streq(argv[index], "-name") && index + 1 < argc) {
            name_pattern = argv[index + 1];
            index += 2;
        } else if (cli_streq(argv[index], "-type") && index + 1 < argc &&
            (cli_streq(argv[index + 1], "f") || cli_streq(argv[index + 1], "d"))) {
            type_filter = cli_streq(argv[index + 1], "d") ? 1 : 2;
            index += 2;
        } else {
            cli_puts("usage: find [path] [-name pattern] [-type f|d]\n");
            return 1;
        }
    }
    const char *pwd = getenv("PWD");
    cli_normalize_path(normalized_root, sizeof(normalized_root), pwd != 0 && pwd[0] != '\0' ? pwd : "/", root);
    root = normalized_root;

    for (uint64_t list_index = 0;; list_index++) {
        char path[CLI_PATH_MAX];
        struct srv_stat info;
        uint64_t size = 0;
        long result = srv_list(list_index, path, sizeof(path), &size);
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
        if (type_filter != 0) {
            if (srv_stat(path, &info) != 0) {
                continue;
            }
            if (type_filter == 1 && info.type != 1) {
                continue;
            }
            if (type_filter == 2 && info.type == 1) {
                continue;
            }
        }
        cli_puts(path);
        cli_puts("\n");
    }

    if (!printed_root && name_pattern == 0) {
        struct srv_stat info;
        if (srv_stat(root, &info) == 0) {
            if ((type_filter == 0) ||
                (type_filter == 1 && info.type == 1) ||
                (type_filter == 2 && info.type != 1)) {
                cli_puts(root);
                cli_puts("\n");
            }
            return 0;
        }
    }
    return 0;
}
