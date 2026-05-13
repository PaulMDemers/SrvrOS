#include <srvros/cli.h>
#include <srvros/sys.h>

int main(int argc, char **argv) {
    char buffer[512];
    size_t out = 0;
    int append = 0;
    int path_index = 1;
    if (argc > 1 && cli_streq(argv[1], "-a")) {
        append = 1;
        path_index = 2;
    }
    if (argc < path_index + 2) {
        cli_puts("usage: write [-a] <path> <text>\n");
        return 1;
    }
    for (int i = path_index + 1; i < argc; i++) {
        if (i > path_index + 1 && out + 1 < sizeof(buffer)) {
            buffer[out++] = ' ';
        }
        for (size_t j = 0; argv[i][j] != '\0' && out + 1 < sizeof(buffer); j++) {
            buffer[out++] = argv[i][j];
        }
    }
    buffer[out++] = '\n';
    long result = append ?
        srv_fs_append(argv[path_index], buffer, out) :
        srv_fs_write(argv[path_index], buffer, out);
    if (result < 0) {
        cli_puts("write: failed\n");
        return 2;
    }
    return 0;
}
