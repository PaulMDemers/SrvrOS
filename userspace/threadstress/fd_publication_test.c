#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: fd-publication line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static unsigned stopped,rounds;
static void *publish(void *unused) {
    (void)unused;
    while(!__atomic_load_n(&stopped,__ATOMIC_ACQUIRE)) {
        int p[2];
        if(pipe2(p,O_CLOEXEC|O_NONBLOCK)) return (void *)1;
        int copy=fcntl(p[1],F_DUPFD_CLOEXEC,32);
        if(copy<0 || dup3(p[0],60,O_CLOEXEC)!=60) return (void *)1;
        sched_yield();
        if(close(copy) || close(60) || close(p[0]) || close(p[1])) return (void *)1;
        __atomic_add_fetch(&rounds,1,__ATOMIC_RELEASE);
    }
    return NULL;
}
int posix_fd_publication_child(void) {
    for(int fd=3;fd<sysconf(_SC_OPEN_MAX);fd++) {
        if(fcntl(fd,F_GETFD)!=-1 || errno!=EBADF) {
            printf("threadstress: leaked descriptor %d\n",fd);return 1;
        }
    }
    return 0;
}
int posix_fd_publication_test(void) {
    int p[2]={-11,-12};
    CHECK(pipe2(p,O_APPEND)==-1 && errno==EINVAL && p[0]==-11 && p[1]==-12);
    CHECK(pipe2((int *)(uintptr_t)1,O_CLOEXEC)==-1 && errno==EFAULT);
    CHECK(!pipe2(p,O_CLOEXEC|O_NONBLOCK));
    CHECK(fcntl(p[0],F_GETFD)==FD_CLOEXEC && fcntl(p[1],F_GETFD)==FD_CLOEXEC);
    CHECK((fcntl(p[0],F_GETFL)&O_NONBLOCK) && (fcntl(p[1],F_GETFL)&O_NONBLOCK));
    int copy=fcntl(p[1],F_DUPFD_CLOEXEC,32);CHECK(copy==32 && fcntl(copy,F_GETFD)==FD_CLOEXEC);
    int other=fcntl(p[1],F_DUPFD,32);CHECK(other==33 && fcntl(other,F_GETFD)==0);
    /* Seeking a high number must not allocate or close lower numbers. */
    int low=dup(p[1]);CHECK(low==5 && !close(low));
    CHECK(fcntl(p[1],F_DUPFD,-1)==-1 && errno==EINVAL);
    CHECK(fcntl(p[1],F_DUPFD,67)==-1 && errno==EINVAL);
    CHECK(fcntl(-1,F_DUPFD,0)==-1 && errno==EBADF);
    CHECK(dup3(p[0],p[0],0)==-1 && errno==EINVAL);
    CHECK(dup3(p[0],other,O_APPEND)==-1 && errno==EINVAL);
    CHECK(dup3(-1,other,O_CLOEXEC)==-1 && errno==EBADF && fcntl(other,F_GETFD)==0);
    CHECK(dup3(p[0],other,O_CLOEXEC)==other && fcntl(other,F_GETFD)==FD_CLOEXEC);
    CHECK(dup2(p[0],other)==other && fcntl(other,F_GETFD)==0);
    CHECK(!close(p[0]) && !close(p[1]) && !close(copy) && !close(other));
    pthread_t task;stopped=rounds=0;
    CHECK(!pthread_create(&task,NULL,publish,NULL));
    while(!__atomic_load_n(&rounds,__ATOMIC_ACQUIRE)) sched_yield();
    for(int i=0;i<12;i++) {
        char *args[]={"threadstress","fd-publication-child",NULL};pid_t pid;
        CHECK(!posix_spawn(&pid,"/fat/bin/threadstress",NULL,NULL,args,environ));
        int status;CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    __atomic_store_n(&stopped,1,__ATOMIC_RELEASE);
    void *result;CHECK(!pthread_join(task,&result) && !result && rounds>0);
    CHECK(!posix_fd_publication_child());
    puts("threadstress: fd-publication ok");return 0;
}
