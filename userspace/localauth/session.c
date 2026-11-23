#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char cwd[82];
    const char *home = getenv("HOME");
    if (getuid() == 0 || getgid() == 0 || !home ||
        !getcwd(cwd, sizeof(cwd)) || strcmp(cwd, home) != 0)
        return 1;
    int fd = open(home, O_RDONLY | O_DIRECTORY);
    if (fd < 0 || close(fd) < 0) return 1;
    printf("Session uid=%" PRIu32 " gid=%" PRIu32 " home=%s\n", getuid(), getgid(), home);
#ifdef LOCAL_AUTH_VM_TEST
    const char *other = getuid() == 1001 ? "/fat/home/local-beta" : "/fat/home/local-alpha";
    errno = 0;
    if (chdir(other) == 0 || errno != EACCES) return 1;
    errno = 0;
    fd = open("/fat/etc/local-auth/accounts", O_RDONLY);
    if (fd >= 0 || errno != EACCES) return 1;
    puts("session-check: PASS");
#endif
    return 0;
}
