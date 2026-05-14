#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "cloexec") == 0) {
        int fd = open("/fat/status.txt", O_RDONLY);
        if (fd < 3 || fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
            return 121;
        }
        char *probe_argv[] = {"/fat/bin/fdprobe", "closed", 0};
        execve("/fat/bin/fdprobe", probe_argv, environ);
        return 122;
    }
    if (argc > 1 && strcmp(argv[1], "inherit") == 0) {
        int fd = open("/fat/status.txt", O_RDONLY);
        if (fd < 3) {
            return 123;
        }
        char *probe_argv[] = {"/fat/bin/fdprobe", "open", 0};
        execve("/fat/bin/fdprobe", probe_argv, environ);
        return 124;
    }
    char *false_argv[] = {"/fat/bin/false", 0};
    execve("/fat/bin/false", false_argv, environ);
    return 120;
}
