#define main diff_program_main
#include "../userspace/diff/diff.c"
#undef main
#include <stdlib.h>
#include <unistd.h>
long srv_write(int fd, const void *buffer, size_t length) { return write(fd, buffer, length); }
int main(void) {
    char name[] = "/tmp/srvros-diff-XXXXXX";
    int fd = mkstemp(name); if (fd < 0) return 1; close(fd);
    char *argv[] = {"diff", ".", name, NULL};
    int result = diff_program_main(3, argv);
    unlink(name);
    return result == 2 ? 0 : 2;
}
