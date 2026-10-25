#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: umask line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static void *mask_worker(void *unused) { (void)unused;return (void *)(unsigned long)umask(0077); }
int posix_umask_child(void) {
    mode_t prior=umask(0);if(prior!=0027) return 11;
    umask(prior);
    int fd=open("/fat/umask-child",O_CREAT|O_EXCL|O_WRONLY,0666);struct stat info;
    if(fd<0 || fstat(fd,&info) || (info.st_mode&0777)!=0640 || close(fd) || unlink("/fat/umask-child")) return 12;
    return 0;
}
int posix_umask_test(void) {
    mode_t saved=umask(0027);struct stat info;
    CHECK(!mkdir("/fat/mask-dir///",0777));
    CHECK(!stat("/fat/mask-dir",&info) && (info.st_mode&0777)==0750);
    int dir=open("/fat/mask-dir",O_RDONLY|O_DIRECTORY);CHECK(dir>=0);
    CHECK(!mkdirat(dir,"sub/",0777));
    CHECK(!stat("/fat/mask-dir/sub",&info) && (info.st_mode&0777)==0750);
    CHECK(mkdirat(dir,"missing/.",0777)==-1 && errno==ENOENT);
    int fd=openat(dir,"file",O_CREAT|O_EXCL|O_RDWR,0666);CHECK(fd>=0);
    CHECK(!fstat(fd,&info) && (info.st_mode&0777)==0640 && !close(fd));
    fd=openat(dir,"file",O_CREAT|O_RDWR,0777);CHECK(fd>=0);
    CHECK(!fstat(fd,&info) && (info.st_mode&0777)==0640 && !close(fd));
    pthread_t task;void *old;CHECK(!pthread_create(&task,NULL,mask_worker,NULL) && !pthread_join(task,&old));
    CHECK((unsigned long)old==0027 && umask(0027)==0077);
    char *args[]={"threadstress","umask-child",NULL};
    for(int kind=0;kind<2;kind++) {
        pid_t child;
        if(!kind) CHECK(!posix_spawn(&child,"/fat/bin/threadstress",NULL,NULL,args,NULL));
        else { child=fork();CHECK(child>=0);if(!child) { execv("/fat/bin/threadstress",args);_exit(90); } }
        int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
        CHECK(umask(0027)==0027);
    }
    CHECK(!unlinkat(dir,"file",0) && !unlinkat(dir,"sub",AT_REMOVEDIR) && !close(dir));
    CHECK(!rmdir("/fat/mask-dir"));umask(saved);
    puts("threadstress: umask ok");return 0;
}
