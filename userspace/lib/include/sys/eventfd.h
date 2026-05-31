#ifndef SRVROS_POSIX_SYS_EVENTFD_H
#define SRVROS_POSIX_SYS_EVENTFD_H

#include <stdint.h>

typedef uint64_t eventfd_t;

#define EFD_SEMAPHORE 1
#define EFD_CLOEXEC 0x80000
#define EFD_NONBLOCK 0x0800

int eventfd(unsigned int initval, int flags);

#endif
