#include <srvros/sys.h>

#define GUI_ARG_MAX 16

int main(int argc, char **argv) {
    char *displayd_argv[GUI_ARG_MAX];
    int out = 0;
    displayd_argv[out++] = "displayd";
    for (int i = 1; i < argc && out + 1 < GUI_ARG_MAX; i++) {
        displayd_argv[out++] = argv[i];
    }
    displayd_argv[out] = 0;
    srv_puts("gui: starting displayd\n");
    if (srv_execve("/fat/bin/displayd", displayd_argv, 0) < 0) {
        srv_puts("gui: exec /fat/bin/displayd failed\n");
        return 1;
    }
    return 0;
}
