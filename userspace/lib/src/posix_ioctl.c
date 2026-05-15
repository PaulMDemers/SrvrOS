#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>

#include <srvros/sys.h>

static void winsize_from_srv(struct winsize *out, const struct srv_winsize *in) {
    out->ws_row = in->row;
    out->ws_col = in->col;
    out->ws_xpixel = in->xpixel;
    out->ws_ypixel = in->ypixel;
}

static void winsize_to_srv(struct srv_winsize *out, const struct winsize *in) {
    out->row = in->ws_row;
    out->col = in->ws_col;
    out->xpixel = in->ws_xpixel;
    out->ypixel = in->ws_ypixel;
}

int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *argument = va_arg(args, void *);
    va_end(args);

    if (request == TIOCGWINSZ) {
        if (argument == 0) {
            errno = EINVAL;
            return -1;
        }
        struct srv_winsize srv;
        if (srv_ioctl(fd, request, &srv) < 0) {
            errno = ENOTTY;
            return -1;
        }
        winsize_from_srv((struct winsize *)argument, &srv);
        return 0;
    }

    if (request == TIOCSWINSZ) {
        if (argument == 0) {
            errno = EINVAL;
            return -1;
        }
        struct srv_winsize srv;
        winsize_to_srv(&srv, (const struct winsize *)argument);
        if (srv_ioctl(fd, request, &srv) < 0) {
            errno = ENOTTY;
            return -1;
        }
        return 0;
    }

    errno = EINVAL;
    return -1;
}
