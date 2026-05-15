#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int main(void) {
    printf("ttydemo: start\n");

    struct termios saved;
    if (tcgetattr(STDIN_FILENO, &saved) < 0) {
        printf("ttydemo: tcgetattr failed errno=%d\n", errno);
        return 1;
    }

    struct termios raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        printf("ttydemo: tcsetattr raw failed errno=%d\n", errno);
        return 1;
    }

    struct termios check;
    if (tcgetattr(STDIN_FILENO, &check) < 0 || (check.c_lflag & (ICANON | ECHO)) != 0) {
        printf("ttydemo: raw verify failed\n");
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        return 1;
    }
    printf("ttydemo: raw mode ok\n");

    if (tcsetattr(STDIN_FILENO, TCSANOW, &saved) < 0) {
        printf("ttydemo: restore failed errno=%d\n", errno);
        return 1;
    }
    if (tcgetattr(STDIN_FILENO, &check) < 0 || check.c_lflag != saved.c_lflag) {
        printf("ttydemo: restore verify failed\n");
        return 1;
    }
    printf("ttydemo: restore ok\n");

    int fd = open("/fat/ttydemo.tmp", O_CREAT | O_RDWR | O_TRUNC);
    if (fd < 0) {
        printf("ttydemo: open failed errno=%d\n", errno);
        return 1;
    }
    errno = 0;
    if (tcgetattr(fd, &check) == 0 || errno != ENOTTY) {
        printf("ttydemo: enotty failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    close(fd);
    unlink("/fat/ttydemo.tmp");
    printf("ttydemo: enotty ok\n");

    printf("ttydemo: ok\n");
    return 0;
}
