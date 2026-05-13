#include <srvros/cli.h>

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            cli_puts(" ");
        }
        cli_puts(argv[i]);
    }
    cli_puts("\n");
    return 0;
}
