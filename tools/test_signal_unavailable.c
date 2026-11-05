/* Legacy libc fixtures model an OS without public logical signal I/O. Active
 * continuation paths have separate signal_* fixtures. Any unexpected entry
 * into the public backend must fail loudly,never fabricate a successful root. */
#include "../userspace/lib/src/signal_runtime.h"
#include "../userspace/lib/src/socket_signal.h"
static void unexpected(void) __attribute__((noreturn));
static void unexpected(void){__builtin_trap();}
unsigned __posix_signal_info_depth(void){return 0;}
int __posix_signal_public_available(void){return 0;}
long __posix_signal_control(const struct srv_signal_control_v1 *r,void *p,size_t n){(void)r;(void)p;(void)n;unexpected();}
void __posix_signal_runtime_failure(void){unexpected();}
void __posix_signal_unwind(uint64_t p){(void)p;unexpected();}
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t k,uint64_t p,uint64_t s,uint64_t a,uint64_t d,const int64_t *f,size_t n,uint64_t g,uint64_t m){(void)r;(void)k;(void)p;(void)s;(void)a;(void)d;(void)f;(void)n;(void)g;(void)m;unexpected();}
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t n,const uint64_t a[6],const struct srv_signal_descriptor_v1 *d,size_t c){(void)r;(void)n;(void)a;(void)d;(void)c;unexpected();}
void __posix_signal_io_end(struct posix_signal_io *r){(void)r;unexpected();}
long __posix_signal_io_local_begin(struct posix_signal_io *r,uint64_t c,int f){(void)r;(void)c;(void)f;unexpected();}
void __posix_signal_io_local_end(struct posix_signal_io *r,uint64_t c){(void)r;(void)c;unexpected();}
int __posix_signal_io_result(struct posix_signal_io *r,struct srv_signal_io_result_v1 *o){(void)r;(void)o;unexpected();}
int __posix_signal_io_capture_descriptors(struct posix_signal_io *r,const int64_t *f,size_t n,uint64_t g,struct srv_signal_descriptor_v1 *d){(void)r;(void)f;(void)n;(void)g;(void)d;unexpected();}
/* Some legacy models provide their own restart decision. The default preserves
 * their existing pending-dispatch model with no automatic restart claim. */
extern int __posix_signal_dispatch_pending(void);
__attribute__((weak)) int __posix_signal_dispatch_pending_io(int *restartable){*restartable=0;return __posix_signal_dispatch_pending();}
void __posix_socket_poll_prepare(int fd,struct posix_socket_poll_snapshot *s){(void)fd;(void)s;unexpected();}
short __posix_socket_poll_commit(int fd,short e,short v,const struct posix_socket_poll_snapshot *s){(void)fd;(void)e;(void)v;(void)s;unexpected();}
