#ifndef SRVROS_POSIX_SYS_UN_H
#define SRVROS_POSIX_SYS_UN_H

#include <srvros/syscall_numbers.h>
#include <sys/socket.h>

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[SRV_UNIX_PATH_MAX];
};

#endif
