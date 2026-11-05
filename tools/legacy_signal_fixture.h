#ifndef SRVROS_LEGACY_SIGNAL_FIXTURE_H
#define SRVROS_LEGACY_SIGNAL_FIXTURE_H
/* Explicitly model public-v1 unavailable in the older fallback contract tests.
 * Unexpected entry into a public root aborts rather than returning fake success.
 * This file is test support, never linked into the OS. */
#include "../userspace/lib/src/signal_runtime.h"
#include <stdlib.h>
#include "../userspace/lib/src/socket_signal.h"
#define LEGACY_UNUSED __attribute__((unused))
unsigned __posix_signal_info_depth(void){return 0;}
int __posix_signal_public_available(void){return 0;}
int __posix_signal_dispatch_pending(void);
#ifndef LEGACY_SIGNAL_HAS_IO_DISPATCH
int __posix_signal_dispatch_pending_io(int *restartable){*restartable=0;return __posix_signal_dispatch_pending();}
#endif
long __posix_signal_control(const struct srv_signal_control_v1 *r LEGACY_UNUSED,void *p LEGACY_UNUSED,size_t n LEGACY_UNUSED){abort();}
void __posix_signal_runtime_failure(void){abort();}
void __posix_signal_unwind(uint64_t s LEGACY_UNUSED){abort();}
int __posix_signal_io_begin(struct posix_signal_io *r LEGACY_UNUSED,uint64_t k LEGACY_UNUSED,uint64_t p LEGACY_UNUSED,uint64_t s LEGACY_UNUSED,uint64_t a LEGACY_UNUSED,uint64_t d LEGACY_UNUSED,const int64_t *fds LEGACY_UNUSED,size_t n LEGACY_UNUSED,uint64_t f LEGACY_UNUSED,uint64_t m LEGACY_UNUSED){abort();}
long __posix_signal_io_call(struct posix_signal_io *r LEGACY_UNUSED,uint64_t n LEGACY_UNUSED,const uint64_t a[6] LEGACY_UNUSED,const struct srv_signal_descriptor_v1 *e LEGACY_UNUSED,size_t count LEGACY_UNUSED){abort();}
void __posix_signal_io_end(struct posix_signal_io *r LEGACY_UNUSED){abort();}
long __posix_signal_io_local_begin(struct posix_signal_io *r LEGACY_UNUSED,uint64_t c LEGACY_UNUSED,int f LEGACY_UNUSED){abort();}
void __posix_signal_io_local_end(struct posix_signal_io *r LEGACY_UNUSED,uint64_t c LEGACY_UNUSED){abort();}
int __posix_signal_io_result(struct posix_signal_io *r LEGACY_UNUSED,struct srv_signal_io_result_v1 *o LEGACY_UNUSED){abort();}
int __posix_signal_io_capture_descriptors(struct posix_signal_io *r LEGACY_UNUSED,const int64_t *fds LEGACY_UNUSED,size_t n LEGACY_UNUSED,uint64_t f LEGACY_UNUSED,struct srv_signal_descriptor_v1 *out LEGACY_UNUSED){abort();}
void __posix_socket_poll_prepare(int fd LEGACY_UNUSED,struct posix_socket_poll_snapshot *p LEGACY_UNUSED){abort();}
short __posix_socket_poll_commit(int fd LEGACY_UNUSED,short e LEGACY_UNUSED,short a LEGACY_UNUSED,const struct posix_socket_poll_snapshot *p LEGACY_UNUSED){abort();}
#ifdef LEGACY_SIGNAL_STUB_MONOTONIC
long srv_monotonic_us(void){abort();}
long srv_ticks(void){abort();}
#endif
#undef LEGACY_UNUSED
#endif
