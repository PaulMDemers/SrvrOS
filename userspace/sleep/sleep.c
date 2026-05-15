#include <srvros/cli.h>
#include <unistd.h>

static int parse_seconds(const char *text, unsigned int *seconds_out) {
    unsigned int value = 0;
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10u + (unsigned int)(text[i] - '0');
    }
    *seconds_out = value;
    return 1;
}

int main(int argc, char **argv) {
    unsigned int seconds = 0;
    if (argc != 2 || !parse_seconds(argv[1], &seconds)) {
        cli_puts("usage: sleep <seconds>\n");
        return 1;
    }
    return sleep(seconds) == 0 ? 0 : 1;
}
