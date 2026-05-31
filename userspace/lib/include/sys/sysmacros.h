#ifndef SRVROS_POSIX_SYS_SYSMACROS_H
#define SRVROS_POSIX_SYS_SYSMACROS_H

#include <sys/types.h>

#define major(dev) ((unsigned int)(((dev) >> 8) & 0xfffU))
#define minor(dev) ((unsigned int)(((dev) & 0xffU) | (((dev) >> 12) & 0xfffff00U)))
#define makedev(maj, min) \
    ((dev_t)((((dev_t)(maj) & 0xfffU) << 8) | (((dev_t)(min) & 0xffU)) | (((dev_t)(min) & 0xfffff00U) << 12)))

#endif
