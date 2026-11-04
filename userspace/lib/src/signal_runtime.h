#ifndef SRVROS_LIB_SIGNAL_RUNTIME_H
#define SRVROS_LIB_SIGNAL_RUNTIME_H
#include <srvros/signal_abi.h>
#include <stddef.h>
#include <stdint.h>
unsigned __posix_signal_info_depth(void);
int __posix_signal_public_available(void);
long __posix_signal_control(const struct srv_signal_control_v1 *,void *,size_t);
void __posix_signal_runtime_failure(void) __attribute__((noreturn));
void __posix_signal_unwind(uint64_t target_stack);
/* A root is owned by one real noinline wrapper frame; composing wrappers pass
 * this pointer explicitly. Descriptor capture occurs under the existing libc
 * descriptor lock, never by a later lookup after dropping it. */
struct posix_signal_io {
    uint64_t ticket;
    size_t count;
    struct srv_signal_descriptor_v1 descriptors[64];
};
int __posix_signal_io_begin(struct posix_signal_io *,uint64_t kind,uint64_t primary,uint64_t selector,
    uint64_t owner_anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t temporary_mask);
long __posix_signal_io_call(struct posix_signal_io *,uint64_t number,const uint64_t args[6],
    const struct srv_signal_descriptor_v1 *expected,size_t count);
void __posix_signal_io_end(struct posix_signal_io *);
long __posix_signal_io_local_begin(struct posix_signal_io *,uint64_t outcome_cookie,int finalize);
void __posix_signal_io_local_end(struct posix_signal_io *,uint64_t guard_cookie);
int __posix_signal_io_result(struct posix_signal_io *,struct srv_signal_io_result_v1 *);
int __posix_signal_io_capture_descriptors(struct posix_signal_io *,const int64_t *,size_t,uint64_t flags,
    struct srv_signal_descriptor_v1 *);
#endif
