#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <srvros/sys.h>

static void termios_from_srv(struct termios *out, const struct srv_termios *in) {
    out->c_iflag = in->iflag;
    out->c_oflag = in->oflag;
    out->c_cflag = in->cflag;
    out->c_lflag = in->lflag;
    for (size_t i = 0; i < NCCS; i++) {
        out->c_cc[i] = in->cc[i];
    }
}

static void termios_to_srv(struct srv_termios *out, const struct termios *in) {
    out->iflag = in->c_iflag;
    out->oflag = in->c_oflag;
    out->cflag = in->c_cflag;
    out->lflag = in->c_lflag;
    for (size_t i = 0; i < SRV_NCCS; i++) {
        out->cc[i] = i < NCCS ? in->c_cc[i] : 0;
    }
}

int tcgetattr(int fd, struct termios *termios_p) {
    if (termios_p == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct srv_termios srv;
    if (srv_tty_getattr(fd, &srv) < 0) {
        errno = ENOTTY;
        return -1;
    }
    termios_from_srv(termios_p, &srv);
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    if (termios_p == NULL ||
        (optional_actions != TCSANOW && optional_actions != TCSADRAIN && optional_actions != TCSAFLUSH)) {
        errno = EINVAL;
        return -1;
    }

    struct srv_termios srv;
    termios_to_srv(&srv, termios_p);
    if (srv_tty_setattr(fd, optional_actions, &srv) < 0) {
        errno = ENOTTY;
        return -1;
    }
    return 0;
}

pid_t tcgetpgrp(int fd) {
    if (!isatty(fd)) {
        errno = ENOTTY;
        return -1;
    }
    long result = srv_proc_control(SRV_PROC_GET_FOREGROUND_PGROUP, 0, 0);
    if (result <= 0) {
        errno = ENOTTY;
        return -1;
    }
    return (pid_t)result;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    if (pgrp <= 0) {
        errno = EINVAL;
        return -1;
    }
    if (!isatty(fd)) {
        errno = ENOTTY;
        return -1;
    }
    if (srv_proc_control(SRV_PROC_SET_FOREGROUND_PGROUP, 0, (uint64_t)pgrp) < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}
