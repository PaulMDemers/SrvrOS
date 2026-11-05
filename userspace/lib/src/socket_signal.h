#ifndef SRVROS_LIB_SOCKET_SIGNAL_H
#define SRVROS_LIB_SOCKET_SIGNAL_H
#include <stdint.h>
struct posix_socket_poll_snapshot {
    uint64_t descriptor_generation,object_generation;
    void *object;
    int fd,native_fd,error,connecting;
};
/* Caller holds the existing event lifecycle mutex. Prepare may query native
 * NET_ERROR and must run before LOCAL_BEGIN. Commit is bounded pure metadata:
 * no native query, lock acquisition, tracing, allocation or callback. */
void __posix_socket_poll_prepare(int,struct posix_socket_poll_snapshot *);
short __posix_socket_poll_commit(int,short,short,const struct posix_socket_poll_snapshot *);
#endif
