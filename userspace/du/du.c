#include <srvros/cli.h>
#include <srvros/sys.h>

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

static uint64_t sum_path(const char *root, int *seen_out) {
    uint64_t total = 0;
    int seen = 0;
    for (uint64_t index = 0;; index++) {
        char path[CLI_PATH_MAX];
        uint64_t size = 0;
        long result = srv_list(index, path, sizeof(path), &size);
        if (result <= 0) {
            break;
        }
        if (under_root(path, root)) {
            total += size;
            seen = 1;
        }
    }
    if (seen_out != 0) {
        *seen_out = seen;
    }
    return total;
}

int main(int argc, char **argv) {
    int first_path = 1;
    if (argc > 1 && cli_streq(argv[1], "-s")) {
        first_path = 2;
    }
    if (argc <= first_path) {
        int seen = 0;
        cli_putn(sum_path("/", &seen));
        cli_puts("\t/\n");
        return 0;
    }
    int status = 0;
    for (int i = first_path; i < argc; i++) {
        struct srv_stat info;
        uint64_t total = 0;
        int seen = 0;
        if (srv_stat(argv[i], &info) == 0 && info.type == 0) {
            total = info.size;
            seen = 1;
        } else {
            total = sum_path(argv[i], &seen);
        }
        if (!seen) {
            cli_puts("du: not found: ");
            cli_puts(argv[i]);
            cli_puts("\n");
            status = 1;
            continue;
        }
        cli_putn(total);
        cli_puts("\t");
        cli_puts(argv[i]);
        cli_puts("\n");
    }
    return status;
}
