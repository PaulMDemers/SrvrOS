#ifndef SRVROS_LINUX_CAPABILITY_H
#define SRVROS_LINUX_CAPABILITY_H

#include <stdint.h>

#define _LINUX_CAPABILITY_VERSION_3 0x20080522
#define _LINUX_CAPABILITY_U32S_3 2

struct __user_cap_header_struct {
    uint32_t version;
    int pid;
};

struct __user_cap_data_struct {
    uint32_t effective;
    uint32_t permitted;
    uint32_t inheritable;
};

#define CAP_CHOWN 0
#define CAP_DAC_OVERRIDE 1
#define CAP_DAC_READ_SEARCH 2
#define CAP_FOWNER 3
#define CAP_FSETID 4
#define CAP_KILL 5
#define CAP_SETGID 6
#define CAP_SETUID 7
#define CAP_SETPCAP 8
#define CAP_LINUX_IMMUTABLE 9
#define CAP_NET_BIND_SERVICE 10
#define CAP_NET_BROADCAST 11
#define CAP_NET_ADMIN 12
#define CAP_NET_RAW 13
#define CAP_SYS_CHROOT 18

#define CAP_TO_INDEX(cap) ((cap) >> 5)
#define CAP_TO_MASK(cap) (1u << ((cap) & 31))

#endif
