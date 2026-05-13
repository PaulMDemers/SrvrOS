#include <srvros/cli.h>
#include <srvros/sys.h>

static int wc_file(const char *path) {
    int fd = (int)srv_open(path);
    char buffer[128];
    uint64_t bytes = 0;
    uint64_t lines = 0;
    uint64_t words = 0;
    int in_word = 0;
    if (fd < 0) {
        cli_puts("wc: open failed: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        bytes += (uint64_t)count;
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n') {
                lines++;
            }
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                in_word = 0;
            } else if (!in_word) {
                words++;
                in_word = 1;
            }
        }
    }
    srv_close(fd);
    cli_putn(lines);
    cli_puts(" ");
    cli_putn(words);
    cli_puts(" ");
    cli_putn(bytes);
    cli_puts(" ");
    cli_puts(path);
    cli_puts("\n");
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc < 2) {
        cli_puts("usage: wc <path> [...]\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        int result = wc_file(argv[i]);
        if (result != 0) {
            status = result;
        }
    }
    return status;
}
