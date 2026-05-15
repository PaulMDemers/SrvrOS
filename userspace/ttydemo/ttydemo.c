#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
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

    struct winsize saved_size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &saved_size) < 0 ||
        saved_size.ws_row == 0 ||
        saved_size.ws_col == 0) {
        printf("ttydemo: winsize get failed errno=%d row=%u col=%u\n",
            errno,
            saved_size.ws_row,
            saved_size.ws_col);
        return 1;
    }
    printf("ttydemo: winsize ok\n");

    struct winsize custom_size = saved_size;
    custom_size.ws_row = 24;
    custom_size.ws_col = 80;
    custom_size.ws_xpixel = 640;
    custom_size.ws_ypixel = 480;
    if (ioctl(STDOUT_FILENO, TIOCSWINSZ, &custom_size) < 0) {
        printf("ttydemo: winsize set failed errno=%d\n", errno);
        return 1;
    }
    struct winsize check_size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &check_size) < 0 ||
        check_size.ws_row != 24 ||
        check_size.ws_col != 80) {
        printf("ttydemo: winsize verify failed errno=%d row=%u col=%u\n",
            errno,
            check_size.ws_row,
            check_size.ws_col);
        (void)ioctl(STDOUT_FILENO, TIOCSWINSZ, &saved_size);
        return 1;
    }
    if (ioctl(STDOUT_FILENO, TIOCSWINSZ, &saved_size) < 0) {
        printf("ttydemo: winsize restore failed errno=%d\n", errno);
        return 1;
    }
    printf("ttydemo: winsize set ok\n");

    int tty_dup = dup(STDOUT_FILENO);
    if (tty_dup < 0) {
        printf("ttydemo: dup stdout failed errno=%d\n", errno);
        return 1;
    }
    if (!isatty(tty_dup) ||
        tcgetattr(tty_dup, &check) < 0 ||
        ioctl(tty_dup, TIOCGWINSZ, &check_size) < 0) {
        printf("ttydemo: dup tty failed errno=%d\n", errno);
        close(tty_dup);
        return 1;
    }
    close(tty_dup);
    printf("ttydemo: dup tty ok\n");

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
    errno = 0;
    if (ioctl(fd, TIOCGWINSZ, &check_size) == 0 || errno != ENOTTY) {
        printf("ttydemo: ioctl enotty failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    close(fd);
    unlink("/fat/ttydemo.tmp");
    printf("ttydemo: enotty ok\n");

    printf("ttydemo: ok\n");
    return 0;
}
