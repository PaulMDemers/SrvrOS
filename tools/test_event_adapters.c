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
#include <srvros/sys.h>
#define CHECK(x) do { if (!(x)) { printf("event-adapters: line %d failed: %s\n", __LINE__, #x); exit(1); } } while (0)
int __posix_eventfd_is_pseudo(int fd);
ssize_t __posix_eventfd_read(int fd, void *buffer, size_t length);
ssize_t __posix_eventfd_write(int fd, const void *buffer, size_t length);
int __posix_node_fork_ready(void);
static int error_value, live[67]={1,1,1}, last_timeout, poll_calls, sleep_calls;
static short readiness[67];
static int lock_depth, dup_error, close_error, setfd_error, fd_flags[67];
static void (*during_poll)(void);
static void (*during_sleep)(void);
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
int __posix_signal_dispatch_pending(void) { CHECK(0);return 0; }
int __posix_signal_dispatch_pending_restartable(void) { CHECK(0);return 0; }

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
int __posix_socket_is_pseudo(int fd) { (void)fd; return 0; }
int __posix_socket_close(int fd) { (void)fd; CHECK(0); return -1; }
void __posix_socket_note_close(int fd) { (void)fd; }
int poll(struct pollfd *fds, nfds_t count, int timeout) {
    CHECK(!lock_depth); poll_calls++; last_timeout=timeout; int n=0;
    for (nfds_t i=0;i<count;i++) {
        CHECK(fds[i].fd>=3 && fds[i].fd<67 && live[fds[i].fd]);
        fds[i].revents=readiness[fds[i].fd] & (fds[i].events|POLLERR|POLLHUP);
        n+=fds[i].revents!=0;
    }
    if (during_poll) { void (*callback)(void)=during_poll; during_poll=NULL; callback(); }
    return n;
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
static void close_wait_handle(void) { CHECK(close(retiring_ep)==0); }
static void modify_watch(void) {
    struct epoll_event e={.events=EPOLLIN,.data.u64=999};
    CHECK(epoll_ctl(retiring_ep,EPOLL_CTL_MOD,retiring_fd,&e)==0);
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
    CHECK(epoll_wait(ep_alias,got,3,0)==3 && got[2].data.u64==250);
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
        CHECK(epoll_wait(ep,got,1,0)==1 && !poll_calls && got[0].data.u64==456);
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
        watch(ep,retiring_fd,EPOLLIN,123); during_sleep=wake_eventfd;
        int n=epoll_wait(ep,got,2,20);
        printf("eventfd posted during wait: ready=%d sleep_calls=%d\n",n,sleep_calls);
        CHECK(n==1 && got[0].data.u64==123);
    } else if (!strcmp(mode,"zero-timeout") || !strcmp(mode,"ready-wait")) {
        int ep=epoll_create1(0), fds[2]; CHECK(pipe(fds)==0);
        watch(ep,fds[0],EPOLLIN,123);
        int immediate=!strcmp(mode,"zero-timeout");
        if (!immediate) watch(ep,eventfd(1,EFD_NONBLOCK),EPOLLIN,456);
        int n=epoll_wait(ep,got,2,immediate?0:-1);
        printf("wait: ready=%d native_poll_timeout=%d calls=%d\n",n,last_timeout,poll_calls);
        CHECK(n==(immediate?0:1) && poll_calls==1 && last_timeout==0);
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
