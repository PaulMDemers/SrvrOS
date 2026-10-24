#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <srvros/sys.h>
#include <unistd.h>

#define ATFORK_MAX 32
struct fork_hooks { void (*prepare)(void),(*parent)(void),(*child)(void); };
static struct fork_hooks hooks[ATFORK_MAX];
static unsigned hook_count,hook_lock;
int __posix_node_fork_ready(void);
int __posix_socket_fork_ready(void);
static void lock_hooks(void) {
    while(__atomic_exchange_n(&hook_lock,1,__ATOMIC_ACQUIRE)) srv_yield();
}
static void unlock_hooks(void) { __atomic_store_n(&hook_lock,0,__ATOMIC_RELEASE); }
int pthread_atfork(void (*prepare)(void),void (*parent)(void),void (*child)(void)) {
    lock_hooks();
    if(hook_count==ATFORK_MAX) { unlock_hooks();return ENOMEM; }
    hooks[hook_count++]=(struct fork_hooks){prepare,parent,child};
    unlock_hooks();return 0;
}
pid_t fork(void) {
    struct fork_hooks snapshot[ATFORK_MAX];
    lock_hooks();unsigned count=hook_count;
    for(unsigned i=0;i<count;i++) snapshot[i]=hooks[i];
    unlock_hooks();
    for(unsigned i=count;i>0;i--) if(snapshot[i-1].prepare) snapshot[i-1].prepare();
    /* These adapters currently own state in libc memory, so private cloning
     * would silently split an open description. Refuse until they are shared. */
    long result=(!__posix_node_fork_ready() || !__posix_socket_fork_ready())?
        SRV_ERR_NOTSUP:srv_syscall0(SYS_FORK);
    for(unsigned i=0;i<count;i++) {
        void (*callback)(void)=result==0?snapshot[i].child:snapshot[i].parent;
        if(callback) callback();
    }
    if(result<0) { errno=(int)-result;return -1; }
    return (pid_t)result;
}
