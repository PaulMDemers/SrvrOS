#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: resource line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static _Thread_local volatile sig_atomic_t fsize_signals;
static void fsize_handler(int signal) { if(signal==SIGXFSZ) fsize_signals++; }
static int shared_fd;
static void *limit_worker(void *unused) {
    (void)unused;
    struct rlimit limit;
    int bad=getrlimit(RLIMIT_FSIZE,&limit) || limit.rlim_cur!=8;
    errno=0;
    if(pwrite(shared_fd,"x",1,8)!=-1 || errno!=EFBIG || fsize_signals!=1) bad=1;
    return (void *)(long)bad;
}
int posix_resource_child(int again) {
    struct rlimit limit;
    CHECK(getrlimit(RLIMIT_FSIZE,&limit)==0 && limit.rlim_cur==8 && limit.rlim_max==16);
    if(!again) {
        char *args[]={"threadstress","resource-exec-child",NULL};
        execve("/fat/bin/threadstress",args,environ);
        return -1;
    }
    CHECK(signal(SIGXFSZ,SIG_IGN)!=SIG_ERR);
    int fd=open("/fat/resource-child.tmp",O_CREAT|O_TRUNC|O_RDWR,0600);
    CHECK(fd>=0 && write(fd,"1234567890",10)==8);
    CHECK(write(fd,"!",1)==-1 && errno==EFBIG);
    CHECK(close(fd)==0 && unlink("/fat/resource-child.tmp")==0);
    return 0;
}
int posix_resource_test(void) {
    struct rlimit saved,limit;
    CHECK(getrlimit(RLIMIT_FSIZE,&saved)==0);
    limit=(struct rlimit){8,16};CHECK(setrlimit(RLIMIT_FSIZE,&limit)==0);
    struct sigaction action={.sa_handler=fsize_handler},old;
    CHECK(sigaction(SIGXFSZ,&action,&old)==0);fsize_signals=0;
    int fd=open("/fat/resource.tmp",O_CREAT|O_TRUNC|O_RDWR,0600);
    CHECK(fd>=0 && write(fd,"1234567890",10)==8 && !fsize_signals);
    CHECK(write(fd,"!",1)==-1 && errno==EFBIG && fsize_signals==1);
    CHECK(lseek(fd,0,SEEK_CUR)==8 && write(fd,"",0)==0 && fsize_signals==1);
    CHECK(pwrite(fd,"zzzz",4,6)==2 && fsize_signals==1 && lseek(fd,0,SEEK_CUR)==8);
    CHECK(pwrite(fd,"!",1,8)==-1 && errno==EFBIG && fsize_signals==2);
    CHECK(ftruncate(fd,9)==-1 && errno==EFBIG && fsize_signals==3);
    CHECK(ftruncate(fd,7)==0 && ftruncate(fd,8)==0);
    shared_fd=fd;pthread_t worker;void *result;
    CHECK(pthread_create(&worker,NULL,limit_worker,NULL)==0);
    CHECK(pthread_join(worker,&result)==0 && !result && fsize_signals==3);
    char *args[]={"threadstress","resource-child",NULL};pid_t pid;int status;
    CHECK(posix_spawn(&pid,"/fat/bin/threadstress",NULL,NULL,args,environ)==0);
    CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(getrlimit(RLIMIT_FSIZE,&limit)==0 && limit.rlim_cur==8 && limit.rlim_max==16);
    limit=(struct rlimit){9,8};CHECK(setrlimit(RLIMIT_FSIZE,&limit)==-1 && errno==EINVAL);
    CHECK(getrlimit(RLIMIT_FSIZE,&limit)==0 && limit.rlim_cur==8);
    limit=(struct rlimit){0,0};CHECK(setrlimit(RLIMIT_FSIZE,&limit)==0);
    CHECK(ftruncate(fd,0)==0 && write(fd,"",0)==0);
    CHECK(pwrite(fd,"!",1,0)==-1 && errno==EFBIG && fsize_signals==4);
    CHECK(setrlimit(RLIMIT_FSIZE,&saved)==0 && sigaction(SIGXFSZ,&old,NULL)==0);
    CHECK(close(fd)==0 && unlink("/fat/resource.tmp")==0);
    CHECK(getrlimit(RLIMIT_CPU,&limit)==0 && limit.rlim_cur==RLIM_INFINITY);
    limit.rlim_cur=1;CHECK(setrlimit(RLIMIT_CPU,&limit)==-1 && errno==EOPNOTSUPP);
    CHECK(getrlimit(RLIMIT_CPU,&limit)==0 && limit.rlim_cur==RLIM_INFINITY);
    CHECK(getrlimit(99,&limit)==-1 && errno==EINVAL);
    errno=0;CHECK(getpriority(PRIO_PROCESS,0)==0 && !errno);
    CHECK(setpriority(PRIO_PROCESS,0,0)==0);
    CHECK(setpriority(PRIO_PROCESS,0,5)==-1 && errno==EOPNOTSUPP);
    CHECK(getpriority(PRIO_PROCESS,2147483647)==-1 && errno==ESRCH);
    CHECK(getpriority(99,0)==-1 && errno==EINVAL);
    puts("threadstress: resource ok");return 0;
}
