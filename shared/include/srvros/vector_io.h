#ifndef SRVROS_VECTOR_IO_H
#define SRVROS_VECTOR_IO_H

#include <stdint.h>

#define SRV_IOV_MAX 16
#define SRV_VECTOR_READ 0
#define SRV_VECTOR_WRITE 1
#define SRV_VECTOR_PREAD 2
#define SRV_VECTOR_PWRITE 3

/* ABI layout matches the native LP64 struct iovec. */
struct srv_iovec {
    void *base;
    uint64_t length;
};

#endif
