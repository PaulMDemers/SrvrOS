#include <srvros/cli.h>
#include <srvros/sys.h>
#include <stdlib.h>

static int path_exists(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0;
}

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
        out[0] = '\0';
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

static void append_base36(char *out, size_t capacity, size_t *length, uint64_t value) {
    char digits[24];
    size_t count = 0;
    if (value == 0) {
        digits[count++] = '0';
    }
    while (value > 0 && count < sizeof(digits)) {
        uint64_t digit = value % 36;
        digits[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= 36;
    }
    while (count > 0 && *length + 1 < capacity) {
        out[(*length)++] = digits[--count];
        out[*length] = '\0';
    }
}

static void instantiate(char *out, size_t capacity, const char *templ, uint64_t seed) {
    size_t length = 0;
    size_t x_start = 0;
    size_t x_count = 0;
    out[0] = '\0';
    for (size_t i = 0; templ[i] != '\0' && length + 1 < capacity; i++) {
        if (templ[i] == 'X') {
            if (x_count == 0) {
                x_start = length;
            }
            x_count++;
        }
        out[length++] = templ[i];
        out[length] = '\0';
    }
    if (x_count == 0) {
        if (length + 7 < capacity) {
            out[length++] = '.';
            x_start = length;
            x_count = 6;
            for (size_t i = 0; i < 6; i++) {
                out[length++] = 'X';
            }
            out[length] = '\0';
        } else {
            return;
        }
    }

    char suffix[24];
    size_t suffix_length = 0;
    suffix[0] = '\0';
    append_base36(suffix, sizeof(suffix), &suffix_length, seed);
    for (size_t i = 0; i < x_count; i++) {
        size_t source = suffix_length > i ? suffix_length - i - 1 : i;
        out[x_start + x_count - i - 1] = suffix[source % (suffix_length == 0 ? 1 : suffix_length)];
    }
}

int main(int argc, char **argv) {
    char default_template[CLI_PATH_MAX];
    const char *tmpdir = getenv("TMPDIR");
    const char *templ = argc > 1 ? argv[1] : default_template;
    char parent[CLI_PATH_MAX];
    char path[CLI_PATH_MAX];

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: mktemp [template]\n");
        return 0;
    }
    if (argc > 2) {
        cli_puts("usage: mktemp [template]\n");
        return 1;
    }
    if (argc == 1) {
        if (tmpdir == 0 || tmpdir[0] == '\0') {
            tmpdir = "/fat/tmp";
        }
        if (!cli_join_path(default_template, sizeof(default_template), tmpdir, "tmp.XXXXXX")) {
            cli_puts("mktemp: template too long\n");
            return 1;
        }
    }
    if (!parent_dir(parent, sizeof(parent), templ)) {
        cli_puts("mktemp: bad template\n");
        return 1;
    }
    if (parent[0] != '\0' && mkdir_p(parent) != 0) {
        cli_puts("mktemp: cannot create parent\n");
        return 1;
    }

    for (uint64_t attempt = 0; attempt < 32; attempt++) {
        uint64_t seed = srv_ticks() + srv_getpid() * 131 + attempt;
        instantiate(path, sizeof(path), templ, seed);
        if (!path_exists(path)) {
            int fd = (int)srv_open_mode(path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
            if (fd < 0) {
                cli_puts("mktemp: create failed\n");
                return 1;
            }
            srv_close(fd);
            cli_puts(path);
            cli_puts("\n");
            return 0;
        }
    }
    cli_puts("mktemp: no unique path\n");
    return 1;
}
