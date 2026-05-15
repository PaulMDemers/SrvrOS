#ifndef SRVROS_TERMIOS_H
#define SRVROS_TERMIOS_H

#include <srvros/syscall_numbers.h>

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS SRV_NCCS

#define ICRNL SRV_TTY_IFLAG_ICRNL

#define ECHO SRV_TTY_LFLAG_ECHO
#define ICANON SRV_TTY_LFLAG_ICANON
#define ISIG SRV_TTY_LFLAG_ISIG

#define VEOF SRV_TTY_VEOF
#define VEOL SRV_TTY_VEOL
#define VERASE SRV_TTY_VERASE
#define VINTR SRV_TTY_VINTR
#define VKILL SRV_TTY_VKILL
#define VTIME SRV_TTY_VTIME
#define VMIN SRV_TTY_VMIN

#define TCSANOW SRV_TCSANOW
#define TCSADRAIN SRV_TCSADRAIN
#define TCSAFLUSH SRV_TCSAFLUSH

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
};

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

#endif
