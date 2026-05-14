#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int fd = open("/fat/posixdemo/lock.txt", O_RDWR);
    if (fd < 0) {
        puts("lockprobe: open failed");
        return 1;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 8;
    lock.l_pid = 0;
    if (fcntl(fd, F_SETLK, &lock) == 0) {
        puts("lockprobe: unexpected lock");
        close(fd);
        return 2;
    }
    if (errno != EAGAIN && errno != EACCES) {
        puts("lockprobe: wrong errno");
        close(fd);
        return 3;
    }

    if (fcntl(fd, F_GETLK, &lock) < 0 || lock.l_type != F_WRLCK || lock.l_pid <= 0) {
        puts("lockprobe: query failed");
        close(fd);
        return 4;
    }

    puts("lockprobe: conflict ok");
    close(fd);
    return 0;
}
