#include <srvros/cli.h>

int main(int argc, char **argv) {
    int print_uid = 0;
    int print_gid = 0;
    int print_name = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (cli_is_help_arg(arg)) {
            cli_puts("usage: id [-u] [-g] [-n] [user]\n");
            return 0;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                if (arg[j] == 'u') {
                    print_uid = 1;
                } else if (arg[j] == 'g') {
                    print_gid = 1;
                } else if (arg[j] == 'n') {
                    print_name = 1;
                } else {
                    cli_puts("usage: id [-u] [-g] [-n] [user]\n");
                    return 2;
                }
            }
        }
    }

    if (print_uid || print_gid) {
        if (print_name) {
            cli_puts("root\n");
        } else {
            cli_puts("0\n");
        }
        return 0;
    }
    cli_puts("uid=0(root) gid=0(root) groups=0(root)\n");
    return 0;
}
