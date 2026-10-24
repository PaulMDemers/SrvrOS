#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: masked poll line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static volatile sig_atomic_t delivered;
static void handler(int signal) { if(signal==SIGUSR1) delivered++; }
static void *sender(void *target) {
    usleep(10000);
    return (void *)(intptr_t)pthread_kill((pthread_t)target,SIGUSR1);
}
int posix_masked_poll_test(void) {
    sigset_t blocked,old,empty,current;
    sigemptyset(&blocked);sigaddset(&blocked,SIGUSR1);sigemptyset(&empty);
    struct sigaction action={.sa_handler=handler},previous;
    CHECK(sigaction(SIGUSR1,&action,&previous)==0);
    CHECK(pthread_sigmask(SIG_BLOCK,&blocked,&old)==0);delivered=0;
    struct timespec timeout={.tv_sec=1};
    for(unsigned i=0;i<16;i++) {
        pthread_t thread;void *result=NULL;
        CHECK(pthread_create(&thread,NULL,sender,(void *)pthread_self())==0);
        int value=i&1?ppoll(NULL,0,&timeout,&empty):pselect(0,NULL,NULL,NULL,&timeout,&empty);
        CHECK(value==-1 && errno==EINTR && delivered==(int)i+1);
        CHECK(pthread_sigmask(SIG_BLOCK,NULL,&current)==0 && sigismember(&current,SIGUSR1));
        CHECK(pthread_join(thread,&result)==0 && !result);
        CHECK(timeout.tv_sec==1 && timeout.tv_nsec==0);
    }
    CHECK(raise(SIGUSR1)==0 && delivered==16);
    timeout=(struct timespec){0};
    CHECK(ppoll(NULL,0,&timeout,NULL)==0 && delivered==16);
    CHECK(pselect(0,NULL,NULL,NULL,NULL,&empty)==-1 && errno==EINTR && delivered==17);
    CHECK(pselect(-1,NULL,NULL,NULL,&timeout,&empty)==-1 && errno==EINVAL);
    CHECK(pthread_sigmask(SIG_BLOCK,NULL,&current)==0 && sigismember(&current,SIGUSR1));
    timeout.tv_nsec=1000000000;
    CHECK(ppoll(NULL,0,&timeout,&empty)==-1 && errno==EINVAL);
    CHECK(pthread_sigmask(SIG_BLOCK,NULL,&current)==0 && sigismember(&current,SIGUSR1));
    int pipefd[2];CHECK(pipe(pipefd)==0 && write(pipefd[1],"x",1)==1);
    fd_set read;FD_ZERO(&read);FD_SET(pipefd[0],&read);timeout=(struct timespec){0};
    CHECK(pselect(pipefd[0]+1,&read,NULL,NULL,&timeout,&empty)==1 && FD_ISSET(pipefd[0],&read));
    CHECK(close(pipefd[0])==0 && close(pipefd[1])==0);
    CHECK(pthread_sigmask(SIG_SETMASK,&old,NULL)==0 && sigaction(SIGUSR1,&previous,NULL)==0);
    puts("threadstress: masked poll ok");return 0;
}
