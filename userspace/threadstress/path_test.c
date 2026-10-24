#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: path line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_path_child(void) {
    char cwd[PATH_MAX];
    if(!getcwd(cwd,sizeof(cwd)) || strcmp(cwd,"/fat/posix-cwd")) return 17;
    int fd=open("value",O_RDONLY);char data[4]={0};
    if(fd<0 || read(fd,data,3)!=3 || strcmp(data,"cwd") || close(fd)) return 18;
    return 0;
}
static void *change_worker(void *unused) { (void)unused;return (void *)(long)chdir("/"); }
int posix_path_test(void) {
    char initial[PATH_MAX],cwd[PATH_MAX];CHECK(getcwd(initial,sizeof(initial)));
    CHECK(setenv("PWD","/not-the-cwd",1)==0);
    CHECK(getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,initial));
    CHECK(mkdir("/fat/posix-cwd",0700)==0 || errno==EEXIST);
    CHECK(chdir("/fat/posix-cwd")==0);
    CHECK(!strcmp(getenv("PWD"),"/not-the-cwd"));
    CHECK(getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/fat/posix-cwd"));
    char *allocated=getcwd(NULL,0);CHECK(allocated && !strcmp(allocated,cwd));free(allocated);
    char small[4]="old";CHECK(!getcwd(small,sizeof(small)) && errno==ERANGE && !strcmp(small,"old"));
    int fd=open("value",O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(fd>=0 && write(fd,"cwd",3)==3 && close(fd)==0);
    CHECK(chdir("value")==-1 && errno==ENOTDIR);
    CHECK(open("value/../wrong",O_CREAT|O_WRONLY,0600)==-1 && errno==ENOTDIR);
    CHECK(open("missing/../wrong",O_CREAT|O_WRONLY,0600)==-1 && errno==ENOENT);
    CHECK(chdir("")==-1 && errno==ENOENT);
    char huge[PATH_MAX+1];memset(huge,'x',sizeof(huge)-1);huge[sizeof(huge)-1]=0;
    CHECK(open(huge,O_CREAT|O_WRONLY,0600)==-1 && errno==ENAMETOOLONG);
    CHECK(getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/fat/posix-cwd"));
    pid_t child=fork();CHECK(child>=0);
    if(!child) {
        char *args[]={"threadstress","path-child",NULL};char *empty[]={NULL};
        execve("/fat/bin/threadstress",args,empty);_exit(19);
    }
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    char *args[]={"threadstress","path-child",NULL};char *misleading[]={"PWD=/",NULL};
    CHECK(posix_spawn(&child,"/fat/bin/threadstress",NULL,NULL,args,misleading)==0);
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    child=fork();CHECK(child>=0);
    if(!child) _exit(chdir("/")?20:0);
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/fat/posix-cwd"));
    CHECK(unlink("value")==0);
    pthread_t worker;void *value;CHECK(!pthread_create(&worker,NULL,change_worker,NULL));
    CHECK(!pthread_join(worker,&value) && !value);
    CHECK(getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/"));
    CHECK(rmdir("/fat/posix-cwd")==0 && chdir(initial)==0);
    CHECK(setenv("PWD",initial,1)==0);
    puts("threadstress: path ok");return 0;
}
