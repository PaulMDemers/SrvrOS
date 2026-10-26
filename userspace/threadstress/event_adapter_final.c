#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("event-adapter-final line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static volatile sig_atomic_t caught;
static int begun;
static void handler(int sig) { (void)sig; caught++; }
struct action { int fd,kind; pthread_t target; };
static void *change_counter(void *arg) {
    struct action *a=arg;
    while (!__atomic_load_n(&begun,__ATOMIC_ACQUIRE)) sched_yield();
    if (usleep(20000)) return (void *)1;
    uint64_t value=5;
    if (a->kind==2 || a->kind==3) {
        if (pthread_kill(a->target,SIGUSR2)) return (void *)2;
        if (a->kind==2) return NULL;
        if (usleep(20000)) return (void *)3;
    }
    if (a->kind==1) return read(a->fd,&value,8)==8 && value==UINT64_MAX-1?NULL:(void *)4;
    if (a->kind==4) {
        if (close(a->fd)) return (void *)5;
        return eventfd(9,0)==a->fd?NULL:(void *)6;
    }
    return write(a->fd,&value,8)==8?NULL:(void *)7;
}
static int start_action(struct action *action,pthread_t *worker,int fd,int kind) {
    *action=(struct action){fd,kind,pthread_self()};
    __atomic_store_n(&begun,0,__ATOMIC_RELAXED);
    CHECK(pthread_create(worker,NULL,change_counter,action)==0);
    __atomic_store_n(&begun,1,__ATOMIC_RELEASE);
    return 0;
}
static int join_action(pthread_t worker) {
    void *result; CHECK(pthread_join(worker,&result)==0 && !result); return 0;
}
int posix_event_adapter_final_test(void) {
    struct sigaction act={.sa_handler=handler},old;
    sigset_t empty,blocked,previous,current;
    sigemptyset(&empty); sigemptyset(&blocked); sigaddset(&blocked,SIGUSR2);
    CHECK(sigaction(SIGUSR2,&act,&old)==0);
    CHECK(pthread_sigmask(SIG_SETMASK,&blocked,&previous)==0);
    puts("event-adapter-final: entered");
    int ep=epoll_create1(0),fd=eventfd(0,0); CHECK(ep>=0 && fd>=0);
    struct epoll_event got,event={.events=EPOLLIN,.data.u64=123};
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
    CHECK(raise(SIGUSR2)==0 && !caught);
    CHECK(epoll_pwait(ep,&got,1,100,&empty)==-1 && errno==EINTR && caught==1);
    CHECK(pthread_sigmask(SIG_SETMASK,NULL,&current)==0 && current==blocked);
    CHECK(epoll_pwait(ep,&got,1,23,&blocked)==0);
    CHECK(epoll_pwait(-1,&got,1,0,&empty)==-1 && errno==EBADF);
    CHECK(pthread_sigmask(SIG_SETMASK,NULL,&current)==0 && current==blocked);
    uint64_t value=1; CHECK(write(fd,&value,8)==8);
    CHECK(epoll_pwait(ep,&got,1,0,&empty)==1 && got.data.u64==123);
    CHECK(pthread_sigmask(SIG_SETMASK,NULL,&current)==0 && current==blocked);
    CHECK(raise(SIGUSR2)==0 && caught==1);
    CHECK(epoll_pwait(ep,&got,1,0,&empty)==-1 && errno==EINTR && caught==2);
    CHECK(read(fd,&value,8)==8 && value==1);
    struct action action; pthread_t worker;
    CHECK(start_action(&action,&worker,fd,2)==0);
    CHECK(epoll_pwait(ep,&got,1,-1,&empty)==-1 && errno==EINTR);
    CHECK(join_action(worker)==0 && caught==3);
    CHECK(pthread_sigmask(SIG_SETMASK,NULL,&current)==0 && current==blocked);
    CHECK(pthread_sigmask(SIG_SETMASK,&empty,NULL)==0);

    puts("event-adapter-final: masks ok");
    for (int round=0;round<4;round++) {
        CHECK(start_action(&action,&worker,fd,0)==0);
        CHECK(read(fd,&value,8)==8 && value==5);
        CHECK(join_action(worker)==0);
        value=UINT64_MAX-1; CHECK(write(fd,&value,8)==8);
        CHECK(start_action(&action,&worker,fd,1)==0);
        value=7; CHECK(write(fd,&value,8)==8);
        CHECK(join_action(worker)==0);
        CHECK(read(fd,&value,8)==8 && value==7);
    }
    puts("event-adapter-final: blocking counts ok");
    CHECK((fcntl(fd,F_GETFL)&O_ACCMODE)==O_RDWR);
    CHECK(start_action(&action,&worker,fd,2)==0);
    value=77; CHECK(read(fd,&value,8)==-1 && errno==EINTR && value==77);
    CHECK(join_action(worker)==0);
    act.sa_flags=SA_RESTART; CHECK(sigaction(SIGUSR2,&act,NULL)==0);
    CHECK(start_action(&action,&worker,fd,3)==0);
    CHECK(read(fd,&value,8)==8 && value==5);
    CHECK(join_action(worker)==0);
    act.sa_flags=0; CHECK(sigaction(SIGUSR2,&act,NULL)==0);

    puts("event-adapter-final: blocking signals ok");
    struct pollfd fds[2]={{.fd=fd,.events=POLLIN},{.fd=-1,.events=POLLIN}};
    CHECK(start_action(&action,&worker,fd,0)==0);
    CHECK(poll(fds,2,1000)==1 && fds[0].revents==POLLIN && !fds[1].revents);
    CHECK(join_action(worker)==0 && read(fd,&value,8)==8 && value==5);
    CHECK(poll(fds,2,23)==0);
    CHECK(start_action(&action,&worker,fd,2)==0);
    CHECK(poll(fds,2,-1)==-1 && errno==EINTR);
    CHECK(join_action(worker)==0);
    puts("event-adapter-final: poll ok");
    /* No signal in close/reuse scenario. */
    CHECK(pthread_sigmask(SIG_SETMASK,&blocked,NULL)==0);
    CHECK(start_action(&action,&worker,fd,4)==0);
    CHECK(read(fd,&value,8)==-1 && errno==EBADF);
    CHECK(join_action(worker)==0 && read(fd,&value,8)==8 && value==9);
    CHECK(close(fd)==0);
    CHECK(poll(fds,2,0)==1 && fds[0].revents==POLLNVAL);
    CHECK(pthread_sigmask(SIG_SETMASK,&empty,NULL)==0);

    puts("event-adapter-final: retirement ok");
    /* A real socketpair exercises data and peer-close readiness via epoll. */
    int pair[2]; CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    event.events=EPOLLIN|EPOLLRDNORM; event.data.u64=456;
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,pair[0],&event)==0);
    CHECK(write(pair[1],"x",1)==1);
    CHECK(epoll_wait(ep,&got,1,100)==1 && got.data.u64==456 && got.events==(EPOLLIN|EPOLLRDNORM));
    char byte; CHECK(read(pair[0],&byte,1)==1 && byte=='x');
    CHECK(close(pair[1])==0);
    CHECK(epoll_wait(ep,&got,1,100)==1 && (got.events&EPOLLHUP));
    CHECK(close(pair[0])==0);
    fd=eventfd(1,EFD_NONBLOCK); CHECK(fd>=0);
    uint32_t unsupported[]={EPOLLET,EPOLLRDHUP,EPOLLMSG,EPOLLRDBAND,EPOLLWRBAND};
    for (unsigned i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++) {
        event.events=unsupported[i];
        CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EOPNOTSUPP);
    }
    event.events=EPOLLRDNORM|EPOLLWRNORM;
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==event.events);
    puts("event-adapter-final: events ok");
    int pseudo=socket(AF_INET,SOCK_STREAM,0); CHECK(pseudo>=0);
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,pseudo,&event)==-1 && errno==EOPNOTSUPP);
    CHECK(close(pseudo)==0);
    CHECK(fork()==-1 && errno==EOPNOTSUPP);
    char *argv[]={"/fat/bin/echo",NULL}; pid_t child;
    CHECK(execve(argv[0],argv,NULL)==-1 && errno==EOPNOTSUPP);
    CHECK(posix_spawn(&child,argv[0],NULL,NULL,argv,NULL)==EOPNOTSUPP);
    CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    char payload='x'; struct iovec iov={.iov_base=&payload,.iov_len=1};
    union { struct cmsghdr align; unsigned char bytes[CMSG_SPACE(sizeof(int))]; } control;
    struct msghdr message={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
    struct cmsghdr *cmsg=CMSG_FIRSTHDR(&message);
    cmsg->cmsg_len=CMSG_LEN(sizeof(int)); cmsg->cmsg_level=SOL_SOCKET; cmsg->cmsg_type=SCM_RIGHTS;
    int *right=(int *)CMSG_DATA(cmsg); *right=ep;
    CHECK(sendmsg(pair[0],&message,0)==-1 && errno==EOPNOTSUPP);
    *right=fd; CHECK(sendmsg(pair[0],&message,0)==-1 && errno==EOPNOTSUPP);
    CHECK(close(pair[0])==0 && close(pair[1])==0);
    CHECK(fcntl(ep,F_SETFD,FD_CLOEXEC)==0 && fcntl(fd,F_SETFD,FD_CLOEXEC)==0);
    puts("event-adapter-final: refusals ok");
    posix_spawn_file_actions_t actions;
    CHECK(posix_spawn_file_actions_init(&actions)==0);
    CHECK(posix_spawn_file_actions_adddup2(&actions,ep,40)==0);
    CHECK(posix_spawn(&child,argv[0],&actions,NULL,argv,NULL)==EOPNOTSUPP);
    CHECK(posix_spawn_file_actions_destroy(&actions)==0);
    CHECK(posix_spawn(&child,argv[0],NULL,NULL,argv,NULL)==0);
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(close(fd)==0 && close(ep)==0);
    CHECK(sigaction(SIGUSR2,&old,NULL)==0);
    CHECK(pthread_sigmask(SIG_SETMASK,&previous,NULL)==0);
    puts("threadstress: event-adapter-final ok");
    return 0;
}
