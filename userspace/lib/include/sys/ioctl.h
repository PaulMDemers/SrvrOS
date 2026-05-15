#ifndef SRVROS_SYS_IOCTL_H
#define SRVROS_SYS_IOCTL_H

#include <srvros/syscall_numbers.h>

#define TIOCGWINSZ SRV_TIOCGWINSZ
#define TIOCSWINSZ SRV_TIOCSWINSZ

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd, unsigned long request, ...);

#endif
