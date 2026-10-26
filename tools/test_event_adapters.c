/* Real adapter code and close(), with modeled native descriptors and waits.
 * Known-failure modes assert the intended contract, not the broken behavior. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <srvros/sys.h>
#define CHECK(x) do { if (!(x)) { printf("event-adapters: line %d failed: %s\n", __LINE__, #x); exit(1); } } while (0)
int __posix_eventfd_is_pseudo(int fd);
ssize_t __posix_eventfd_read(int fd, void *buffer, size_t length);
ssize_t __posix_eventfd_write(int fd, const void *buffer, size_t length);
int __posix_node_fork_ready(void);
int __posix_event_exec_ready(void);
int __posix_event_export_allowed(int fd);
static int error_value, live[67]={1,1,1}, last_timeout, poll_calls, sleep_calls;
static short readiness[67];
static int max_model_polls=10000;
static int lock_depth, dup_error, close_error, setfd_error, fd_flags[67];
static void (*during_poll)(void);
static void (*during_sleep)(void);
static uint64_t now_ns = 2999500000ULL;
static int poll_error, clock_error, delayed_calls;
static int64_t advance_ms = -1;
int clock_gettime(int clock, struct timespec *ts) {
    CHECK(!lock_depth && clock==CLOCK_MONOTONIC);
    if (clock_error) { errno=clock_error; return -1; }
    ts->tv_sec=(time_t)(now_ns/1000000000); ts->tv_nsec=(long)(now_ns%1000000000);
    return 0;
}
int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; CHECK(!lock_depth); lock_depth=1; return 0; }
int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; CHECK(lock_depth==1); lock_depth=0; return 0; }
long sysconf(int name) { CHECK(name==_SC_OPEN_MAX); return 67; }
static int duplicate(int fd, int minimum) {
    if (dup_error) return -dup_error;
    if (fd<0 || fd>=67 || !live[fd]) return -EBADF;
    for (int i=minimum;i<67;i++) if (!live[i]) { live[i]=live[fd]; readiness[i]=readiness[fd]; fd_flags[i]=0; return i; }
    return -EMFILE;
}
long srv_dup(int fd) { CHECK(lock_depth); return duplicate(fd,0); }
long srv_dup2(int fd,int target) {
    CHECK(lock_depth);
    if (dup_error) return -dup_error;
    if (fd<0 || fd>=67 || !live[fd] || target<0 || target>=67) return -EBADF;
    live[target]=live[fd]; readiness[target]=readiness[fd]; fd_flags[target]=0; return target;
}
long srv_syscall3(long op,long fd,long target,long flags) {
    CHECK(op==SYS_DUP3); long r=srv_dup2((int)fd,(int)target);
    if (r>=0) fd_flags[r]=(int)flags;
    return r;
}
long srv_fcntl(int fd,int op,uint64_t flags) {
    if (fd<0 || fd>=67 || !live[fd]) return -EBADF;
    if (op==SRV_F_DUPFD || op==SRV_F_DUPFD_CLOEXEC) {
        CHECK(lock_depth); int r=duplicate(fd,(int)flags);
        if (r>=0 && op==SRV_F_DUPFD_CLOEXEC) fd_flags[r]=SRV_FD_CLOEXEC;
        return r;
    }
    if (op==SRV_F_GETFD) return fd_flags[fd];
    if (op==SRV_F_SETFD) { if (setfd_error) return -setfd_error; fd_flags[fd]=(int)flags; return 0; }
    CHECK(0); return -1;
}
int __posix_socket_dup(int fd) { (void)fd; CHECK(0); return -1; }
int __posix_socket_fcntl(int fd,int op,uint64_t flags) { (void)fd;(void)op;(void)flags;CHECK(0);return -1; }
void __posix_socket_note_dup(int fd,int target) { (void)fd;(void)target; }
static uint64_t signal_mask, pending_signal;
static int mask_calls, mask_error, handled_signal;
long srv_signal_mask(uint64_t how,uint64_t mask,uint64_t *old) {
    CHECK(!lock_depth && how==SRV_SIGNAL_SETMASK); mask_calls++;
    if (mask_error) return -EINVAL;
    if (old) *old=signal_mask;
    signal_mask=mask; return 0;
}
int __posix_signal_dispatch_pending(void) {
    CHECK(!lock_depth);
    if (!(pending_signal & ~signal_mask)) return 0;
    pending_signal=0; handled_signal++; return 1;
}
static int restart_io;
int __posix_signal_dispatch_pending_restartable(void) { return restart_io; }
int __posix_signal_dispatch_pending_io(int *restartable) {
    int handled=__posix_signal_dispatch_pending(); *restartable=handled && restart_io; return handled;
}

int *__errno_location(void) { return &error_value; }
FILE *stderr;
int fprintf(FILE *stream, const char *format, ...) { (void)stream; (void)format; CHECK(0); return -1; }
char *getenv(const char *name) { (void)name; return NULL; }
static int allocate(void) {
    for (int i=3;i<67;i++) if (!live[i]) { live[i]=1; return i; }
    CHECK(0); return -1;
}
int pipe(int fds[2]) { fds[0]=allocate(); fds[1]=allocate(); return 0; }
long srv_close(int fd) {
    if (fd<0 || fd>=67 || !live[fd]) return -EBADF;
    live[fd]=0; readiness[fd]=0; fd_flags[fd]=0;
    return close_error ? -close_error : 0;
}
int __posix_socket_is_pseudo(int fd) { return fd==1000; }
int __posix_socket_close(int fd) { (void)fd; CHECK(0); return -1; }
void __posix_socket_note_close(int fd) { (void)fd; }
static int model_poll(struct pollfd *fds, nfds_t count, int timeout) {
    CHECK(!lock_depth); poll_calls++; CHECK(poll_calls<=max_model_polls); last_timeout=timeout; int n=0;
    for (nfds_t i=0;i<count;i++) {
        if (fds[i].fd<0) { fds[i].revents=0; continue; }
        CHECK(fds[i].fd>=3 && fds[i].fd<67 && live[fds[i].fd]);
        fds[i].revents=readiness[fds[i].fd] & (fds[i].events|POLLERR|POLLHUP);
        n+=fds[i].revents!=0;
    }
    if (!n && timeout>0) now_ns+=(uint64_t)(advance_ms>=0?advance_ms:timeout)*1000000;
    advance_ms=-1;
    if (during_poll) { void (*callback)(void)=during_poll; during_poll=NULL; callback(); }
    if (pending_signal & ~signal_mask) return 0; /* actual SYS_POLL signal result */
    if (poll_error) { errno=poll_error; return -1; }
    return n;
}
int __posix_socket_poll_fd(int fd) { return fd; }
short __posix_socket_poll_update(int fd,short wanted,short current) { (void)fd;(void)wanted;return current; }
long srv_poll(struct srv_pollfd *fds,size_t count,int64_t timeout) {
    CHECK(timeout>=0 && timeout<=INT_MAX);
    struct pollfd native[64]; CHECK(count<=64);
    for (size_t i=0;i<count;i++) native[i]=(struct pollfd){.fd=fds[i].fd,.events=fds[i].events};
    int result=model_poll(native,count,(int)timeout);
    for (size_t i=0;i<count;i++) fds[i].revents=native[i].revents;
    return result<0?-errno:result;
}
int usleep(unsigned int us) {
    CHECK(!lock_depth); (void)us; sleep_calls++;
    if (during_sleep) { void (*callback)(void)=during_sleep; during_sleep=NULL; callback(); }
    return 0;
}
static void watch(int ep, int fd, uint32_t mask, uint64_t tag) {
    struct epoll_event e={.events=mask,.data.u64=tag};
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&e)==0);
}
static int retiring_ep, retiring_fd, replacement_ep;
static void wake_eventfd(void) {
    uint64_t value=1; CHECK(__posix_eventfd_write(retiring_fd,&value,8)==8);
}
static void delayed_post(void) {
    if (++delayed_calls<3) during_poll=delayed_post;
    else wake_eventfd();
}
static void add_watch_during_wait(void) { watch(retiring_ep,retiring_fd,EPOLLIN,777); }
static void drain_eventfd(void) {
    uint64_t value; CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8);
}
static void retire_instance(void) {
    CHECK(close(retiring_ep)==0);
    replacement_ep=epoll_create1(0); CHECK(replacement_ep==retiring_ep);
    watch(replacement_ep,retiring_fd,EPOLLIN,999);
}
static void retire_watch(void) {
    CHECK(close(retiring_fd)==0);
    int fds[2]; CHECK(pipe(fds)==0 && fds[0]==retiring_fd);
    readiness[fds[0]]=POLLIN;
    watch(retiring_ep,fds[0],EPOLLIN,999);
}
static uint64_t nested_tag;
static void nested_wait(void) {
    struct epoll_event got;
    CHECK(epoll_wait(retiring_ep,&got,1,0)==1);
    nested_tag=got.data.u64;
}
static void close_wait_handle(void) { CHECK(close(retiring_ep)==0); }
static void modify_watch(void) {
    struct epoll_event e={.events=EPOLLIN,.data.u64=999};
    CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_MOD,retiring_fd,&e)==0);
}
static void rearm_one(void) {
    struct epoll_event event={.events=EPOLLIN|EPOLLONESHOT,.data.u64=999};
    CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_MOD,retiring_fd,&event)==0);
}
static void aliases_test(void) {
    int p[2]; CHECK(pipe(p)==0); readiness[p[0]]=POLLIN;
    /* Alias predates the watch; closing the registered number must retain it. */
    int alias=dup(p[0]), ep=epoll_create1(0); CHECK(alias>=0 && ep>=0);
    watch(ep,p[0],EPOLLIN,100);
    watch(ep,alias,EPOLLIN,200);
    int ep_alias=dup(ep); CHECK(ep_alias>=0);
    CHECK(close(ep)==0 && close(p[0])==0);
    struct epoll_event got[3]; CHECK(epoll_wait(ep_alias,got,3,0)==2);
    CHECK(got[0].data.u64==100 && got[1].data.u64==200);
    int q[2]; CHECK(pipe(q)==0 && q[0]==p[0]); readiness[q[0]]=POLLIN;
    watch(ep_alias,q[0],EPOLLIN,250);
    CHECK(epoll_wait(ep_alias,got,3,0)==3);
    CHECK(got[0].data.u64==250 && got[1].data.u64==100 && got[2].data.u64==200);
    CHECK(epoll_ctl(ep_alias,EPOLL_CTL_DEL,q[0],NULL)==0);
    CHECK(epoll_wait(ep_alias,got,3,0)==2 && got[0].data.u64==100);
    CHECK(close(q[0])==0 && close(q[1])==0);
    CHECK(close(alias)==0 && epoll_wait(ep_alias,got,3,0)==0);
    /* All duplication APIs preserve the instance and FD flags independently. */
    int a=fcntl(ep_alias,F_DUPFD,20), b=fcntl(ep_alias,F_DUPFD_CLOEXEC,30);
    CHECK(a>=20 && b>=30 && fcntl(a,F_GETFD)==0 && fcntl(b,F_GETFD)==FD_CLOEXEC);
    CHECK(dup2(ep_alias,40)==40 && dup3(ep_alias,41,O_CLOEXEC)==41);
    CHECK(fcntl(40,F_GETFD)==0 && fcntl(41,F_GETFD)==FD_CLOEXEC);
    CHECK(close(ep_alias)==0 && close(a)==0 && close(b)==0 && close(40)==0);
    int ev=eventfd(1,EFD_NONBLOCK); watch(41,ev,EPOLLIN,300);
    CHECK(dup(ev)==-1 && errno==EOPNOTSUPP);
    CHECK(dup2(ev,41)==-1 && errno==EOPNOTSUPP);
    struct epoll_event nested={.events=EPOLLIN};
    CHECK(epoll_ctl(41,EPOLL_CTL_ADD,41,&nested)==-1 && errno==EINVAL);
    CHECK(epoll_wait(41,got,3,0)==1 && got[0].data.u64==300);
    CHECK(close(41)==0 && close(ev)==0 && close(p[1])==0);
    CHECK(__posix_node_fork_ready());
}
static void replacement_test(void) {
    int a=epoll_create1(0), b=epoll_create1(0), ev=eventfd(1,EFD_NONBLOCK);
    watch(a,ev,EPOLLIN,1); watch(b,ev,EPOLLIN,2);
    CHECK(dup2(a,b)==b); struct epoll_event got[2];
    CHECK(epoll_wait(b,got,2,0)==1 && got[0].data.u64==1);
    CHECK(dup2(a,a)==a);
    CHECK(dup3(a,a,0)==-1 && errno==EINVAL);
    dup_error=EMFILE; CHECK(dup2(a,b)==-1 && errno==EMFILE); dup_error=0;
    CHECK(epoll_wait(b,got,2,0)==1 && got[0].data.u64==1);
    CHECK(dup2(-1,b)==-1 && errno==EBADF);
    CHECK(close(a)==0); int p[2]; CHECK(pipe(p)==0);
    CHECK(dup3(p[0],b,0)==b); /* Last instance alias replaced by a normal pipe. */
    CHECK(epoll_wait(b,got,2,0)==-1 && errno==EBADF);
    CHECK(close(b)==0 && close(p[0])==0 && close(p[1])==0 && close(ev)==0);
    CHECK(__posix_node_fork_ready());
}
static void check_wait_mask(void) { CHECK(signal_mask==4); }
static void recycle_counter(void) {
    CHECK(close(retiring_fd)==0);
    CHECK(eventfd(9,0)==retiring_fd);
}
static void signal_counter(void) { pending_signal=8; if (restart_io) during_poll=wake_eventfd; }
static void make_nonblocking(void) { CHECK(fcntl(retiring_fd,F_SETFL,O_NONBLOCK)==0); }
int main(int argc, char **argv) {
    const char *mode=argc>1?argv[1]:"controls";
    uint64_t value=0, add=4;
    struct epoll_event got[2];
    if (!strcmp(mode,"controls")) {
        int fd=eventfd(3,EFD_NONBLOCK);
        CHECK(fd>=0 && __posix_eventfd_is_pseudo(fd));
        CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==7);
        CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EAGAIN);
        CHECK(__posix_eventfd_write(fd,&add,7)==-1 && errno==EINVAL);
        int ep=epoll_create1(EPOLL_CLOEXEC);
        watch(ep,fd,EPOLLIN,123);
        CHECK(epoll_wait(ep,got,2,0)==0 && !poll_calls && !sleep_calls);
        CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].data.u64==123 && got[0].events==EPOLLIN);
        struct epoll_event e={.events=EPOLLOUT,.data.u64=456};
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&e)==0);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLOUT && got[0].data.u64==456);
        int p[2]; CHECK(pipe(p)==0); watch(ep,p[0],POLLIN,789);
        CHECK(epoll_wait(ep,got,1,0)==1 && poll_calls==1 && last_timeout==0 && got[0].data.u64==456);
        CHECK(close(p[0])==0 && close(p[1])==0);
        CHECK(epoll_ctl(ep,EPOLL_CTL_DEL,fd,NULL)==0);
        CHECK(epoll_wait(ep,got,2,0)==0);
        CHECK(close(fd)==0 && !__posix_eventfd_is_pseudo(fd));
        CHECK(close(ep)==0);
    } else if (!strcmp(mode,"close-capacity")) {
        for (int i=0;i<9;i++) {
            int ep=epoll_create1(0);
            printf("create/close cycle %d: fd=%d errno=%d\n",i+1,ep,errno);
            CHECK(ep>=0); CHECK(close(ep)==0);
        }
        CHECK(__posix_node_fork_ready());
    } else if (!strcmp(mode,"instance-reuse")) {
        int fd=eventfd(1,EFD_NONBLOCK), ep=epoll_create1(0);
        watch(ep,fd,EPOLLIN,123); CHECK(close(ep)==0);
        int replacement=epoll_create1(0); CHECK(replacement==ep);
        int n=epoll_wait(replacement,got,2,0);
        printf("fresh epoll instance: ready=%d stale_tag=%llu\n",n,n?(unsigned long long)got[0].data.u64:0);
        CHECK(n==0);
    } else if (!strcmp(mode,"watched-reuse")) {
        int fd=eventfd(1,EFD_NONBLOCK), ep=epoll_create1(0);
        watch(ep,fd,EPOLLIN,123); CHECK(close(fd)==0);
        int replacement=eventfd(2,EFD_NONBLOCK); CHECK(replacement==fd);
        int n=epoll_wait(ep,got,2,0);
        printf("reused watched number: ready=%d stale_tag=%llu\n",n,n?(unsigned long long)got[0].data.u64:0);
        CHECK(n==0);
    } else if (!strcmp(mode,"aliases")) {
        aliases_test();
    } else if (!strcmp(mode,"replacement")) {
        replacement_test();
    } else if (!strcmp(mode,"failure-retirement")) {
        int ep=epoll_create1(0); close_error=EIO;
        CHECK(close(ep)==-1 && errno==EIO); close_error=0;
        CHECK(__posix_node_fork_ready());
        ep=epoll_create1(0); CHECK(ep>=0);
        CHECK(close(-1)==-1 && errno==EBADF);
        CHECK(epoll_wait(ep,got,2,0)==0 && close(ep)==0);
        setfd_error=EIO; CHECK(epoll_create1(EPOLL_CLOEXEC)==-1); setfd_error=0;
        CHECK(__posix_node_fork_ready());
        int full[8]; for (int i=0;i<8;i++) { full[i]=epoll_create1(0); CHECK(full[i]>=0); }
        CHECK(epoll_create1(0)==-1 && errno==EMFILE);
        for (int i=0;i<8;i++) CHECK(close(full[i])==0);
        CHECK(__posix_node_fork_ready());
    } else if (!strcmp(mode,"wait-retirement")) {
        int p[2]; CHECK(pipe(p)==0); readiness[p[0]]=POLLIN;
        retiring_fd=p[0]; retiring_ep=epoll_create1(0);
        watch(retiring_ep,p[0],EPOLLIN,123);
        during_poll=retire_instance;
        CHECK(epoll_wait(retiring_ep,got,2,0)==-1 && errno==EBADF);
        retiring_ep=replacement_ep; during_poll=retire_watch;
        CHECK(epoll_wait(retiring_ep,got,2,0)==0);
        CHECK(epoll_wait(retiring_ep,got,2,0)==1 && got[0].data.u64==999);
        during_poll=modify_watch; CHECK(epoll_wait(retiring_ep,got,2,0)==0);
        CHECK(epoll_wait(retiring_ep,got,2,0)==1 && got[0].data.u64==999);
        int alias=dup(retiring_ep); CHECK(alias>=0);
        during_poll=close_wait_handle;
        CHECK(epoll_wait(retiring_ep,got,2,0)==1 && got[0].data.u64==999);
        CHECK(close(alias)==0);
    } else if (!strcmp(mode,"eventfd-wakeup")) {
        retiring_fd=eventfd(0,EFD_NONBLOCK); int ep=epoll_create1(0);
        watch(ep,retiring_fd,EPOLLIN,123); during_poll=wake_eventfd; during_sleep=wake_eventfd;
        int n=epoll_wait(ep,got,2,20);
        printf("eventfd posted during wait: ready=%d polls=%d sleeps=%d\n",n,poll_calls,sleep_calls);
        CHECK(n==1 && got[0].data.u64==123);
    } else if (!strcmp(mode,"zero-timeout") || !strcmp(mode,"ready-wait")) {
        int ep=epoll_create1(0), fds[2]; CHECK(pipe(fds)==0);
        watch(ep,fds[0],EPOLLIN,123);
        int immediate=!strcmp(mode,"zero-timeout");
        if (!immediate) watch(ep,eventfd(1,EFD_NONBLOCK),EPOLLIN,456);
        int n=epoll_wait(ep,got,2,immediate?0:-1);
        printf("wait: ready=%d native_poll_timeout=%d calls=%d\n",n,last_timeout,poll_calls);
        CHECK(n==(immediate?0:1) && poll_calls==1 && last_timeout==0);
    } else if (!strcmp(mode,"wait-deadlines")) {
        int ep=epoll_create1(0); uint64_t start=now_ns;
        CHECK(epoll_wait(ep,got,2,23)==0 && now_ns-start==23000000 && poll_calls==3);
        int p[2]; CHECK(pipe(p)==0); watch(ep,p[0],EPOLLIN,123);
        start=now_ns; int before=poll_calls;
        CHECK(epoll_wait(ep,got,2,23)==0 && now_ns-start==23000000 && poll_calls-before==4);
        /* Scheduler delay near INT_MAX's deadline: no microsecond wrap and no
         * deadline restart. The last blocking poll must use only the remainder. */
        CHECK(epoll_ctl(ep,EPOLL_CTL_DEL,p[0],NULL)==0);
        start=now_ns; before=poll_calls; advance_ms=(int64_t)INT_MAX-5;
        CHECK(epoll_wait(ep,got,2,INT_MAX)==0);
        CHECK(now_ns-start==(uint64_t)INT_MAX*1000000 && poll_calls-before==2 && last_timeout==5);
        clock_error=EIO; CHECK(epoll_wait(ep,got,2,1)==-1 && errno==EIO);
    } else if (!strcmp(mode,"wait-infinite")) {
        int ep=epoll_create1(0); retiring_fd=eventfd(0,EFD_NONBLOCK);
        watch(ep,retiring_fd,EPOLLIN,123); during_poll=delayed_post;
        CHECK(epoll_wait(ep,got,2,-1)==1 && got[0].data.u64==123 && delayed_calls==3);
        CHECK(poll_calls==3 && now_ns==3029500000ULL);
        uint64_t value; CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8);
        int p[2]; CHECK(pipe(p)==0); watch(ep,p[0],EPOLLIN,456);
        delayed_calls=0; during_poll=delayed_post;
        CHECK(epoll_wait(ep,got,2,-7)==1 && got[0].data.u64==123 && delayed_calls==3);
    } else if (!strcmp(mode,"wait-control")) {
        retiring_ep=epoll_create1(0); retiring_fd=eventfd(1,EFD_NONBLOCK);
        during_poll=add_watch_during_wait;
        CHECK(epoll_wait(retiring_ep,got,2,-1)==1 && got[0].data.u64==777);
        /* Closing/reusing an empty waiting instance cannot return its new state. */
        CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_DEL,retiring_fd,NULL)==0);
        during_poll=retire_instance;
        CHECK(epoll_wait(retiring_ep,got,2,-1)==-1 && errno==EBADF);
        retiring_ep=replacement_ep;
        int p[2]; CHECK(pipe(p)==0); retiring_fd=p[0]; readiness[p[0]]=POLLIN;
        CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_DEL,3000,NULL)==0);
        watch(retiring_ep,p[0],EPOLLIN,123); during_poll=modify_watch;
        CHECK(epoll_wait(retiring_ep,got,2,100)==1 && got[0].data.u64==999);
    } else if (!strcmp(mode,"wait-interruption")) {
        int ep=epoll_create1(0); poll_error=EINTR;
        CHECK(epoll_wait(ep,got,2,-1)==-1 && errno==EINTR);
        int fd=eventfd(0,EFD_NONBLOCK); watch(ep,fd,EPOLLIN,123);
        CHECK(epoll_wait(ep,got,2,100)==-1 && errno==EINTR);
        int p[2]; CHECK(pipe(p)==0); watch(ep,p[0],EPOLLIN,456);
        CHECK(epoll_wait(ep,got,2,-1)==-1 && errno==EINTR);
        poll_error=EIO; CHECK(epoll_wait(ep,got,2,100)==-1 && errno==EIO);
    } else if (!strcmp(mode,"wait-drain")) {
        int ep=epoll_create1(0), p[2]; CHECK(pipe(p)==0);
        watch(ep,p[0],EPOLLIN,1); retiring_fd=eventfd(1,EFD_NONBLOCK);
        watch(ep,retiring_fd,EPOLLIN,2); during_poll=drain_eventfd;
        CHECK(epoll_wait(ep,got,2,0)==0 && last_timeout==0);
    } else if (!strcmp(mode,"counter-boundaries")) {
        int fd=eventfd(UINT_MAX,EFD_NONBLOCK), ep=epoll_create1(0);
        CHECK(fd>=0 && ep>=0); watch(ep,fd,EPOLLIN|EPOLLOUT,123);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==(EPOLLIN|EPOLLOUT));
        add=UINT64_MAX-2-UINT_MAX; CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==(EPOLLIN|EPOLLOUT));
        add=2; CHECK(__posix_eventfd_write(fd,&add,8)==-1 && errno==EAGAIN);
        add=1; CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLIN);
        add=0; CHECK(__posix_eventfd_write(fd,&add,8)==8); /* legal even when full */
        add=1; CHECK(__posix_eventfd_write(fd,&add,8)==-1 && errno==EAGAIN);
        add=UINT64_MAX; CHECK(__posix_eventfd_write(fd,&add,8)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_write(fd,NULL,8)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_write(fd,&add,7)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_read(fd,NULL,8)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_read(fd,&value,7)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==UINT64_MAX-1);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLOUT);
        CHECK(__posix_eventfd_write(fd,&add,8)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EAGAIN);
        /* Eight bytes, including unaligned buffers; trailing bytes untouched. */
        unsigned char bytes[17]; memset(bytes,0xa5,sizeof(bytes)); add=17;
        memcpy(bytes+1,&add,8); CHECK(__posix_eventfd_write(fd,bytes+1,16)==8);
        memset(bytes,0xa5,sizeof(bytes)); CHECK(__posix_eventfd_read(fd,bytes+1,16)==8);
        memcpy(&value,bytes+1,8); CHECK(value==17 && bytes[0]==0xa5);
        for (int i=9;i<17;i++) CHECK(bytes[i]==0xa5);
        CHECK(close(fd)==0 && close(ep)==0);
    } else if (!strcmp(mode,"counter-ready-after-drain")) {
        retiring_fd=eventfd(0,EFD_NONBLOCK); int ep=epoll_create1(0);
        add=UINT64_MAX-1; CHECK(__posix_eventfd_write(retiring_fd,&add,8)==8);
        watch(ep,retiring_fd,EPOLLOUT,123); during_poll=drain_eventfd;
        CHECK(epoll_wait(ep,got,2,50)==1 && got[0].events==EPOLLOUT && got[0].data.u64==123);
        CHECK(poll_calls==1 && last_timeout==10 && during_poll==NULL);
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==-1 && errno==EAGAIN);
    } else if (!strcmp(mode,"eventfd-flags")) {
        int known=EFD_SEMAPHORE|EFD_NONBLOCK|EFD_CLOEXEC;
        for (int bit=0;bit<32;bit++) {
            int bad=(int)((uint32_t)1<<bit);
            if (bad & known) continue;
            CHECK(eventfd(3,bad|known)==-1 && errno==EINVAL);
            CHECK(__posix_node_fork_ready());
        }
        CHECK(eventfd(0,-1)==-1 && errno==EINVAL);
        /* Each allocation reuses the same slot, alternating mode bits. Changes
         * to status/descriptor flags must not toggle the semaphore mode. */
        for (int bits=0;bits<8;bits++) {
            int flags=(bits&1?EFD_SEMAPHORE:0)|(bits&2?EFD_NONBLOCK:0)|(bits&4?EFD_CLOEXEC:0);
            int fd=eventfd(3,flags); CHECK(fd==3000);
            CHECK(fcntl(fd,F_GETFD)==(bits&4?FD_CLOEXEC:0));
            CHECK((fcntl(fd,F_GETFL)&O_NONBLOCK)==(bits&2?O_NONBLOCK:0));
            CHECK(fcntl(fd,F_SETFL,0)==0 && (fcntl(fd,F_GETFL)&O_NONBLOCK)==0);
            CHECK(fcntl(fd,F_SETFD,FD_CLOEXEC)==0 && fcntl(fd,F_GETFD)==FD_CLOEXEC);
            CHECK(fcntl(fd,F_SETFL,O_NONBLOCK)==0);
            CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==(bits&1?1:3));
            if (bits&1) {
                CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==1);
                CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==1);
            }
            CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EAGAIN);
            CHECK(close(fd)==0 && __posix_node_fork_ready());
        }
        int full[16]; for (int i=0;i<16;i++) { full[i]=eventfd(1,EFD_NONBLOCK); CHECK(full[i]>=0); }
        CHECK(eventfd(0,0)==-1 && errno==EMFILE);
        CHECK(eventfd(0,0x40000000)==-1 && errno==EINVAL);
        for (int i=0;i<16;i++) CHECK(close(full[i])==0);
        CHECK(__posix_node_fork_ready());
    } else if (!strcmp(mode,"semaphore-readiness")) {
        int fd=eventfd(2,EFD_NONBLOCK|EFD_SEMAPHORE), ep=epoll_create1(0);
        watch(ep,fd,EPOLLIN|EPOLLOUT,123);
        CHECK(__posix_eventfd_read(fd,NULL,8)==-1 && errno==EINVAL);
        CHECK(__posix_eventfd_read(fd,&value,7)==-1 && errno==EINVAL);
        unsigned char bytes[17]; memset(bytes,0xa5,sizeof(bytes));
        CHECK(__posix_eventfd_read(fd,bytes+1,16)==8); memcpy(&value,bytes+1,8);
        CHECK(value==1 && bytes[0]==0xa5); for (int i=9;i<17;i++) CHECK(bytes[i]==0xa5);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==(EPOLLIN|EPOLLOUT));
        CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==1);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLOUT);
        CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EAGAIN);
        add=UINT64_MAX-1; CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLIN);
        CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==1);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==(EPOLLIN|EPOLLOUT));
        add=2; CHECK(__posix_eventfd_write(fd,&add,8)==-1 && errno==EAGAIN);
        add=1; CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLIN);
        CHECK(close(fd)==0 && close(ep)==0);
    } else if (!strcmp(mode,"epoll-rotation")) {
        int ep=epoll_create1(0), alias=dup(ep), p[2], fds[64];
        CHECK(alias>=0 && pipe(p)==0); readiness[p[0]]=POLLIN;
        for (int i=0;i<64;i++) {
            fds[i]=i<3?eventfd(1,EFD_NONBLOCK):(i==3?p[0]:dup(p[0]));
            CHECK(fds[i]>=0); watch(ep,fds[i],EPOLLIN,(unsigned)i+1);
        }
        struct epoll_event batch[7];
        for (int call=0;call<20;call++) {
            CHECK(epoll_wait(call&1?alias:ep,batch,7,0)==7);
            for (int i=0;i<7;i++) CHECK(batch[i].data.u64==(unsigned)(call*7+i)%64+1);
            CHECK(last_timeout==0);
        }
        for (int i=0;i<64;i++) CHECK(close(fds[i])==0);
        CHECK(close(p[1])==0 && close(ep)==0 && close(alias)==0);
        CHECK(__posix_node_fork_ready());
    } else if (!strcmp(mode,"epoll-rotation-control")) {
        int p[2],q[2],r[2]; CHECK(pipe(p)==0 && pipe(q)==0 && pipe(r)==0);
        int a=q[0], b=r[0]; readiness[p[0]]=readiness[a]=readiness[b]=POLLIN;
        retiring_ep=epoll_create1(0); retiring_fd=p[0];
        watch(retiring_ep,p[0],EPOLLIN,1); watch(retiring_ep,a,EPOLLIN,2); watch(retiring_ep,b,EPOLLIN,3);
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && got[0].data.u64==1);
        during_poll=nested_wait;
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && nested_tag==2 && got[0].data.u64==3);
        readiness[a]=readiness[b]=0; during_poll=modify_watch;
        CHECK(epoll_wait(retiring_ep,got,1,0)==0); /* stale sole result does not advance */
        readiness[a]=readiness[b]=POLLIN;
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && got[0].data.u64==999);
        CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_DEL,a,NULL)==0);
        int fd=eventfd(1,EFD_NONBLOCK); watch(retiring_ep,fd,EPOLLIN,777);
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && got[0].data.u64==777);
    } else if (!strcmp(mode,"epoll-rearm")) {
        int ep=epoll_create1(0), alias=dup(ep), fd=eventfd(2,EFD_NONBLOCK);
        struct epoll_event event={.events=EPOLLIN|EPOLLONESHOT,.data.u64=123};
        CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==EPOLLIN);
        CHECK(epoll_wait(alias,got,1,0)==0);
        CHECK(epoll_ctl(alias,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EEXIST);
        CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,NULL)==-1 && errno==EINVAL);
        CHECK(epoll_wait(ep,got,1,0)==0); /* failed controls cannot rearm */
        CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==0); /* unchanged data */
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].data.u64==123);
        CHECK(epoll_wait(ep,got,1,0)==0);
        event.events=EPOLLOUT|EPOLLONESHOT; event.data.u64=456;
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==0);
        CHECK(epoll_wait(alias,got,1,0)==1 && got[0].events==EPOLLOUT && got[0].data.u64==456);
        CHECK(epoll_wait(ep,got,1,0)==0);
        event.events=EPOLLIN; CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==0);
        CHECK(epoll_wait(ep,got,1,0)==1 && epoll_wait(alias,got,1,0)==1);
        CHECK(epoll_ctl(ep,EPOLL_CTL_DEL,fd,NULL)==0);
        watch(ep,fd,EPOLLIN|EPOLLONESHOT,777);
        int other=eventfd(1,EFD_NONBLOCK); watch(ep,other,EPOLLIN|EPOLLONESHOT,888);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].data.u64==888);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].data.u64==777);
        CHECK(epoll_wait(ep,got,2,23)==0 && last_timeout==3); /* parked, no ready spin */
        CHECK(close(fd)==0); int fresh=eventfd(1,EFD_NONBLOCK); CHECK(fresh==fd);
        watch(ep,fresh,EPOLLIN|EPOLLONESHOT,999);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].data.u64==999);
        CHECK(close(fresh)==0 && close(other)==0 && close(ep)==0 && close(alias)==0);
    } else if (!strcmp(mode,"epoll-oneshot-races")) {
        int p[2]; CHECK(pipe(p)==0); readiness[p[0]]=POLLIN;
        retiring_ep=epoll_create1(0); retiring_fd=p[0];
        watch(retiring_ep,p[0],EPOLLIN|EPOLLONESHOT,123);
        during_poll=nested_wait;
        CHECK(epoll_wait(retiring_ep,got,1,0)==0 && nested_tag==123);
        int before=poll_calls; CHECK(epoll_wait(retiring_ep,got,1,0)==0 && poll_calls==before);
        /* MOD while a disabled wait is parked takes effect on the next snapshot. */
        during_poll=rearm_one;
        CHECK(epoll_wait(retiring_ep,got,1,50)==1 && got[0].data.u64==999);
        rearm_one(); during_poll=rearm_one; /* active snapshot becomes stale */
        CHECK(epoll_wait(retiring_ep,got,1,0)==0);
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && got[0].data.u64==999);
        CHECK(epoll_wait(retiring_ep,got,1,0)==0);
        int alias=dup(p[0]); CHECK(alias>=0);
        watch(retiring_ep,alias,EPOLLIN|EPOLLONESHOT,456);
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && got[0].data.u64==456);
        CHECK(epoll_wait(retiring_ep,got,1,0)==0);
        struct epoll_event event={.events=EPOLLONESHOT,.data.u64=789};
        CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_MOD,p[0],&event)==0);
        readiness[p[0]]=POLLHUP|POLLERR;
        CHECK(epoll_wait(retiring_ep,got,1,0)==1 && got[0].events==(EPOLLHUP|EPOLLERR));
        CHECK(epoll_wait(retiring_ep,got,1,0)==0);
    } else if (!strcmp(mode,"adapter-ownership")) {
        CHECK(__posix_event_exec_ready());
        int ep=epoll_create1(EPOLL_CLOEXEC),alias=dup(ep),fd=eventfd(0,EFD_CLOEXEC);
        CHECK(!__posix_event_exec_ready() && !__posix_node_fork_ready());
        CHECK(fcntl(alias,F_SETFD,FD_CLOEXEC)==0 && __posix_event_exec_ready());
        CHECK(fcntl(fd,F_SETFD,0)==0 && !__posix_event_exec_ready());
        CHECK(!__posix_event_export_allowed(ep) && errno==EOPNOTSUPP);
        CHECK(!__posix_event_export_allowed(alias) && !__posix_event_export_allowed(fd));
        int p[2]; CHECK(pipe(p)==0 && __posix_event_export_allowed(p[0]));
        struct epoll_event event={.events=EPOLLIN};
        CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,1000,&event)==-1 && errno==EOPNOTSUPP);
        CHECK(close(alias)==0 && close(ep)==0 && close(fd)==0);
        CHECK(__posix_event_exec_ready() && __posix_node_fork_ready());
    } else if (!strcmp(mode,"poll-counter")) {
        int fd=eventfd(1,EFD_NONBLOCK),p[2]; CHECK(pipe(p)==0);
        struct pollfd fds[3]={{.fd=fd,.events=POLLIN|POLLOUT},{.fd=p[0],.events=POLLIN},{.fd=-1,.events=POLLIN}};
        CHECK(poll(fds,3,100)==1 && fds[0].revents==(POLLIN|POLLOUT) && last_timeout==0);
        CHECK(__posix_eventfd_read(fd,&value,8)==8); add=UINT64_MAX-1;
        CHECK(__posix_eventfd_write(fd,&add,8)==8);
        CHECK(poll(fds,3,0)==1 && fds[0].revents==POLLIN);
        readiness[p[0]]=POLLHUP;
        CHECK(poll(fds,3,0)==2 && fds[1].revents==POLLHUP && !fds[2].revents);
        CHECK(close(fd)==0);
        CHECK(poll(fds,3,0)==2 && fds[0].revents==POLLNVAL);
    } else if (!strcmp(mode,"poll-counter-wait")) {
        retiring_fd=eventfd(0,EFD_NONBLOCK);
        struct pollfd fd={.fd=retiring_fd,.events=POLLIN};
        uint64_t start=now_ns;
        CHECK(poll(&fd,1,23)==0 && now_ns-start==23000000 && poll_calls==3);
        during_poll=wake_eventfd;
        CHECK(poll(&fd,1,-1)==1 && fd.revents==POLLIN && last_timeout==10);
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8);
        poll_error=EINTR; CHECK(poll(&fd,1,100)==-1 && errno==EINTR);
        poll_error=0; advance_ms=(int64_t)INT_MAX-5; start=now_ns;
        CHECK(poll(&fd,1,INT_MAX)==0 && now_ns-start==(uint64_t)INT_MAX*1000000 && last_timeout==5);
    } else if (!strcmp(mode,"poll-counter-lifetime")) {
        retiring_fd=eventfd(0,0); struct pollfd fd={.fd=retiring_fd,.events=POLLIN};
        during_poll=recycle_counter;
        CHECK(poll(&fd,1,-1)==1 && fd.revents==POLLNVAL);
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8 && value==9);
    } else if (!strcmp(mode,"event-masks")) {
        int ep=epoll_create1(0),fd=eventfd(1,EFD_NONBLOCK);
        uint32_t refused[]={EPOLLET,EPOLLMSG,EPOLLRDHUP,EPOLLRDBAND,EPOLLWRBAND};
        struct epoll_event event={.data.u64=123};
        for (unsigned i=0;i<sizeof(refused)/sizeof(refused[0]);i++) {
            event.events=EPOLLIN|refused[i];
            CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EOPNOTSUPP);
        }
        event.events=0x02000000; CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EINVAL);
        watch(ep,fd,EPOLLRDNORM|EPOLLWRNORM,123);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==(EPOLLRDNORM|EPOLLWRNORM));
        event.events=EPOLLIN|EPOLLRDNORM|EPOLLOUT|EPOLLWRNORM;
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==0);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==event.events);
        event.events=EPOLLRDHUP;
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==-1 && errno==EOPNOTSUPP);
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==(EPOLLIN|EPOLLRDNORM|EPOLLOUT|EPOLLWRNORM));
    } else if (!strcmp(mode,"control-errors")) {
        int ep=epoll_create1(0),fd=eventfd(0,EFD_NONBLOCK),p[2]; CHECK(pipe(p)==0);
        struct epoll_event event={.events=EPOLLIN};
        CHECK(epoll_ctl(-1,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EBADF);
        CHECK(epoll_ctl(p[0],EPOLL_CTL_ADD,fd,&event)==-1 && errno==EINVAL);
        CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,-1,&event)==-1 && errno==EBADF);
        CHECK(epoll_ctl(ep,99,fd,NULL)==-1 && errno==EINVAL);
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==-1 && errno==ENOENT);
        CHECK(epoll_ctl(ep,EPOLL_CTL_DEL,fd,NULL)==-1 && errno==ENOENT);
        CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
        CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EEXIST);
    } else if (!strcmp(mode,"pwait-mask")) {
        int ep=epoll_create1(0); sigset_t temporary=0;
        signal_mask=8; pending_signal=8;
        CHECK(epoll_pwait(ep,got,1,30,&temporary)==-1 && errno==EINTR);
        CHECK(signal_mask==8 && mask_calls==2 && handled_signal==1);
        temporary=8; pending_signal=8; signal_mask=0;
        CHECK(epoll_pwait(ep,got,1,23,&temporary)==0);
        CHECK(signal_mask==0 && pending_signal==8 && handled_signal==1);
        CHECK(__posix_signal_dispatch_pending()==1);
        temporary=4; during_poll=check_wait_mask;
        CHECK(epoll_pwait(ep,got,1,10,&temporary)==0 && signal_mask==0);
    } else if (!strcmp(mode,"pwait-restore")) {
        int ep=epoll_create1(0),fd=eventfd(1,EFD_NONBLOCK); watch(ep,fd,EPOLLIN,123);
        sigset_t temporary=4; signal_mask=8;
        CHECK(epoll_pwait(ep,got,1,-1,&temporary)==1 && signal_mask==8 && mask_calls==2);
        CHECK(epoll_pwait(ep,NULL,1,0,&temporary)==-1 && errno==EINVAL && signal_mask==8);
        CHECK(epoll_pwait(-1,got,1,0,&temporary)==-1 && errno==EBADF && signal_mask==8);
        clock_error=EIO;
        CHECK(epoll_pwait(ep,got,1,10,&temporary)==-1 && errno==EIO && signal_mask==8);
        clock_error=0; mask_error=1;
        CHECK(epoll_pwait(ep,got,1,0,&temporary)==-1 && errno==EINVAL && signal_mask==8);
        mask_error=0; int before=mask_calls;
        CHECK(epoll_pwait(ep,got,1,0,NULL)==1 && mask_calls==before);
        pending_signal=8; temporary=0;
        CHECK(epoll_pwait(ep,got,1,0,&temporary)==-1 && errno==EINTR && signal_mask==8);
    } else if (!strcmp(mode,"blocking-signal-dispatch")) {
        max_model_polls=8;
        retiring_fd=eventfd(0,0); during_poll=signal_counter;
        value=123; CHECK(__posix_eventfd_read(retiring_fd,&value,8)==-1 && errno==EINTR && value==123);
        CHECK(handled_signal==1 && poll_calls==1);
        restart_io=1; during_poll=signal_counter;
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8 && value==1);
        CHECK(handled_signal==2 && poll_calls==3);
    } else if (!strcmp(mode,"blocking-counter")) {
        retiring_fd=eventfd(0,0); during_poll=wake_eventfd;
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8 && value==1 && poll_calls==1);
        add=UINT64_MAX-1; CHECK(__posix_eventfd_write(retiring_fd,&add,8)==8);
        during_poll=drain_eventfd; add=3;
        CHECK(__posix_eventfd_write(retiring_fd,&add,8)==8 && poll_calls==2);
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8 && value==3);
        CHECK((fcntl(retiring_fd,F_GETFL)&O_ACCMODE)==O_RDWR);
    } else if (!strcmp(mode,"blocking-retirement")) {
        retiring_fd=eventfd(0,0); during_poll=recycle_counter;
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==-1 && errno==EBADF);
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==8 && value==9);
        during_poll=make_nonblocking;
        CHECK(__posix_eventfd_read(retiring_fd,&value,8)==-1 && errno==EAGAIN);
        CHECK(poll_calls==2);
    } else if (!strcmp(mode,"blocking-interruption")) {
        int fd=eventfd(0,0); poll_error=EINTR;
        value=99; CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EINTR && value==99);
        add=UINT64_MAX-1; CHECK(__posix_eventfd_write(fd,&add,8)==8);
        add=1; CHECK(__posix_eventfd_write(fd,&add,8)==-1 && errno==EINTR);
        poll_error=0; CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==UINT64_MAX-1);
    } else if (!strcmp(mode,"epoll-edge-refusal")) {
        int ep=epoll_create1(0), alias=dup(ep), p[2];
        CHECK(ep>=0 && alias>=0 && pipe(p)==0);
        int counter=eventfd(1,EFD_NONBLOCK); CHECK(counter>=0);
        readiness[p[0]]=POLLIN;
        for (int kind=0;kind<2;kind++) {
            int fd=kind?p[0]:counter;
            struct epoll_event event={.events=EPOLLIN|EPOLLET,.data.u64=999};
            for (int i=0;i<70;i++) {
                CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EOPNOTSUPP);
            }
            CHECK(epoll_wait(ep,got,1,0)==0);
            event.events=EPOLLIN; event.data.u64=123;
            CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
            event.events=EPOLLOUT|EPOLLET|EPOLLONESHOT; event.data.u64=999;
            CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==-1 && errno==EOPNOTSUPP);
            for (int i=0;i<2;i++) {
                CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==EPOLLIN && got[0].data.u64==123);
            }
            /* DEL ignores its event argument, including unsupported bits. */
            CHECK(epoll_ctl(alias,EPOLL_CTL_DEL,fd,&event)==0);
            CHECK(epoll_wait(ep,got,1,0)==0);
            event.events=EPOLLIN|EPOLLONESHOT; event.data.u64=456;
            CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
            CHECK(epoll_wait(ep,got,1,0)==1 && got[0].data.u64==456);
            event.events=EPOLLIN|EPOLLONESHOT|EPOLLET; event.data.u64=999;
            CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==-1 && errno==EOPNOTSUPP);
            CHECK(epoll_wait(ep,got,1,0)==0); /* failed MOD must not rearm */
            event.events=EPOLLIN|EPOLLONESHOT; event.data.u64=789;
            CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==0);
            CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==EPOLLIN && got[0].data.u64==789);
            CHECK(epoll_wait(ep,got,1,0)==0);
            CHECK(epoll_ctl(ep,EPOLL_CTL_DEL,fd,NULL)==0);
        }
        CHECK(close(counter)==0 && close(p[0])==0 && close(p[1])==0);
        CHECK(close(alias)==0 && close(ep)==0);
    } else if (!strcmp(mode,"epoll-oneshot")) {
        int ep=epoll_create1(0), fd=eventfd(1,EFD_NONBLOCK);
        watch(ep,fd,EPOLLIN|EPOLLONESHOT,123);
        CHECK(epoll_wait(ep,got,1,0)==1);
        int n=epoll_wait(ep,got,1,0); printf("unrearmed one-shot: ready=%d\n",n);
        CHECK(n==0);
    } else if (!strcmp(mode,"epoll-priority")) {
        int ep=epoll_create1(0), p[2]; CHECK(pipe(p)==0);
        watch(ep,p[0],EPOLLPRI,123); readiness[p[0]]=POLLPRI;
        int n=epoll_wait(ep,got,1,0); printf("native priority readiness: ready=%d\n",n);
        CHECK(n==1 && got[0].events==EPOLLPRI && got[0].data.u64==123);
        struct epoll_event event={.events=EPOLLIN,.data.u64=456};
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)==0);
        CHECK(epoll_wait(ep,got,1,0)==0); /* unrequested priority is suppressed */
        event.events=EPOLLIN|EPOLLPRI|EPOLLONESHOT;
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)==0);
        readiness[p[0]]=POLLIN|POLLPRI|POLLHUP;
        CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==(EPOLLIN|EPOLLPRI|EPOLLHUP));
        CHECK(epoll_wait(ep,got,1,0)==0);
    } else if (!strcmp(mode,"epoll-fairness")) {
        int ep=epoll_create1(0), a=eventfd(1,EFD_NONBLOCK), b=eventfd(1,EFD_NONBLOCK);
        watch(ep,a,EPOLLIN,1); watch(ep,b,EPOLLIN,2);
        unsigned seen=0;
        for (int i=0;i<4;i++) { CHECK(epoll_wait(ep,got,1,0)==1); seen|=(unsigned)got[0].data.u64; }
        printf("ready set across four maxevents=1 waits: seen=%u\n",seen);
        CHECK(seen==3);
    } else if (!strcmp(mode,"epoll-hup-only")) {
        int ep=epoll_create1(0), p[2]; CHECK(pipe(p)==0);
        watch(ep,p[0],0,123); readiness[p[0]]=POLLHUP;
        int n=epoll_wait(ep,got,2,0);
        printf("zero-interest native hangup: ready=%d\n",n);
        CHECK(n==1 && got[0].events==EPOLLHUP && got[0].data.u64==123);
        readiness[p[0]]=POLLERR;
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==EPOLLERR);
        readiness[p[0]]=POLLIN; CHECK(epoll_wait(ep,got,2,0)==0);
        struct epoll_event event={.events=EPOLLHUP,.data.u64=456};
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)==0);
        readiness[p[0]]=POLLHUP|POLLERR;
        CHECK(epoll_wait(ep,got,2,0)==1 && got[0].events==(EPOLLHUP|EPOLLERR) && got[0].data.u64==456);
    } else if (!strcmp(mode,"semaphore-read")) {
        int fd=eventfd(2,EFD_NONBLOCK|EFD_SEMAPHORE); CHECK(fd>=0);
        ssize_t n=__posix_eventfd_read(fd,&value,8);
        printf("semaphore read: result=%ld value=%llu\n",(long)n,(unsigned long long)value);
        CHECK(n==8 && value==1);
        CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==1);
        CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EAGAIN);
    } else if (!strcmp(mode,"invalid-flags")) {
        int fd=eventfd(0,0x40000000);
        printf("eventfd unknown flag: fd=%d errno=%d\n",fd,errno);
        CHECK(fd==-1 && errno==EINVAL && __posix_node_fork_ready());
    } else if (!strcmp(mode,"counter-writable")) {
        int fd=eventfd(0,EFD_NONBLOCK), ep=epoll_create1(0);
        add=UINT64_MAX-1; CHECK(__posix_eventfd_write(fd,&add,8)==8);
        watch(ep,fd,EPOLLOUT,123);
        int n=epoll_wait(ep,got,2,0);
        printf("full eventfd writable: ready=%d\n",n);
        CHECK(n==0);
    } else if (!strcmp(mode,"counter-overflow") || !strcmp(mode,"reserved-value")) {
        int fd=eventfd(1,EFD_NONBLOCK);
        add=!strcmp(mode,"reserved-value")?UINT64_MAX:UINT64_MAX-1;
        int expected=add==UINT64_MAX?EINVAL:EAGAIN;
        ssize_t n=__posix_eventfd_write(fd,&add,8); int err=errno;
        ssize_t read_count=__posix_eventfd_read(fd,&value,8);
        printf("write: result=%ld errno=%d; read: result=%ld value=%llu\n",(long)n,err,(long)read_count,(unsigned long long)value);
        CHECK(n==-1 && err==expected && read_count==8 && value==1);
    } else { printf("unknown mode: %s\n",mode); return 2; }
    printf("event-adapters: %s PASS\n",mode);
    return 0;
}
