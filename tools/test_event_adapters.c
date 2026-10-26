/* Real adapter code and close(), with modeled native descriptors and waits.
 * Known-failure modes assert the intended contract, not the broken behavior. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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
static int error_value, live[67], last_timeout, poll_calls, sleep_calls;
static short readiness[67];
int *__errno_location(void) { return &error_value; }
FILE *stderr;
int fprintf(FILE *stream, const char *format, ...) { (void)stream; (void)format; CHECK(0); return -1; }
char *getenv(const char *name) { (void)name; return NULL; }
static int allocate(void) {
    for (int i=3;i<67;i++) if (!live[i]) { live[i]=1; return i; }
    CHECK(0); return -1;
}
int pipe(int fds[2]) { fds[0]=allocate(); fds[1]=allocate(); return 0; }
long srv_close(int fd) { CHECK(fd>=3 && fd<67 && live[fd]); live[fd]=0; return 0; }
int __posix_socket_is_pseudo(int fd) { (void)fd; return 0; }
int __posix_socket_close(int fd) { (void)fd; CHECK(0); return -1; }
void __posix_socket_note_close(int fd) { (void)fd; }
int fcntl(int fd, int op, ...) { CHECK(fd>=3 && fd<67 && live[fd]); CHECK(op==F_SETFD); return 0; }
int poll(struct pollfd *fds, nfds_t count, int timeout) {
    poll_calls++; last_timeout=timeout; int n=0;
    for (nfds_t i=0;i<count;i++) {
        CHECK(fds[i].fd>=3 && fds[i].fd<67 && live[fds[i].fd]);
        fds[i].revents=readiness[fds[i].fd] & (fds[i].events|POLLERR|POLLHUP);
        n+=fds[i].revents!=0;
    }
    return n;
}
int usleep(unsigned int us) { (void)us; sleep_calls++; return 0; }
static void watch(int ep, int fd, uint32_t mask, uint64_t tag) {
    struct epoll_event e={.events=mask,.data.u64=tag};
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&e)==0);
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
