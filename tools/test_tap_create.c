#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#define main tap_program_main
#include "../userspace/tap/tap.c"
#undef main
int main(void) {
    char dir[] = "/tmp/srvros-tap-XXXXXX";
    if (!mkdtemp(dir)) return 1;
    char path[128]; size_t n = strlen(dir); memcpy(path, dir, n); memcpy(path+n, "/copy", 6);
    int fds[2]; if (pipe(fds)) return 2; close(fds[1]);
    if (dup2(fds[0], STDIN_FILENO) < 0) return 3;
    close(fds[0]);
    mode_t old = umask(0);
    char *argv[] = {"tap", path, NULL}; int result = tap_program_main(2, argv);
    umask(old); struct stat st; int ok = !stat(path, &st) && (st.st_mode & 0777) == 0666;
    unlink(path); rmdir(dir);
    return result || !ok ? 4 : 0;
}
