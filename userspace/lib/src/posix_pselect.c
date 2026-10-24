#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <sys/select.h>
#include <srvros/sys.h>

int __posix_poll_ms(struct pollfd *,nfds_t,int64_t);
int __posix_select_ms(int,fd_set *,fd_set *,fd_set *,int64_t);

static int timeout_ms(const struct timespec *timeout,int64_t *out) {
    *out=-1;
    if(!timeout) return 0;
    if(timeout->tv_sec<0 || timeout->tv_nsec<0 || timeout->tv_nsec>=1000000000) {
        errno=EINVAL;return -1;
    }
    int64_t part=(timeout->tv_nsec+999999)/1000000;
    *out=timeout->tv_sec>(INT64_MAX-part)/1000?INT64_MAX:timeout->tv_sec*1000+part;
    return 0;
}
static int mask_enter(const sigset_t *mask,uint64_t *old) {
    if(!mask) return 0;
    /* Deliberately bypass sigprocmask's handler checkpoint. In this cooperative
     * delivery ABI poll checks/dispatches before waiting and forces a zero wait
     * if it consumed a signal; the kernel then checks pending state atomically
     * with blocking. A signal in either gap cannot be consumed and lost.
     * An asynchronous signal-frame ABI must replace this with a kernel masked
     * wait before it enables asynchronous handler entry. */
    long result=srv_signal_mask(SRV_SIGNAL_SETMASK,(uint64_t)*mask,old);
    if(result<0) { errno=EINVAL;return -1; }
    return 0;
}
static int mask_leave(const sigset_t *mask,uint64_t old,int result) {
    int saved=errno;
    if(mask && srv_signal_mask(SRV_SIGNAL_SETMASK,old,NULL)<0) { errno=EINVAL;return -1; }
    errno=saved;return result;
}
int ppoll(struct pollfd *fds,nfds_t count,const struct timespec *timeout,const sigset_t *mask) {
    int64_t ms;uint64_t old=0;
    if(timeout_ms(timeout,&ms)<0 || mask_enter(mask,&old)<0) return -1;
    return mask_leave(mask,old,__posix_poll_ms(fds,count,ms));
}
int pselect(int count,fd_set *read,fd_set *write,fd_set *except,const struct timespec *timeout,const sigset_t *mask) {
    int64_t ms;uint64_t old=0;
    if(timeout_ms(timeout,&ms)<0 || mask_enter(mask,&old)<0) return -1;
    return mask_leave(mask,old,__posix_select_ms(count,read,write,except,ms));
}
