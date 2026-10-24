#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: wait line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static void abort_handler(int signal) { (void)signal; }
int posix_wait_child(const char *mode) {
    if(!strcmp(mode,"abort-ignore")) { signal(SIGABRT,SIG_IGN);abort(); }
    if(!strcmp(mode,"abort-catch")) { signal(SIGABRT,abort_handler);abort(); }
    if(!strcmp(mode,"signal")) { signal(SIGTERM,SIG_DFL);raise(SIGTERM);return 99; }
    if(!strcmp(mode,"delay")) { usleep(150000);return 42; }
    return atoi(mode);
}
static int child(pid_t *pid,const char *code,const posix_spawnattr_t *attr) {
    char *args[]={"threadstress","wait-child",(char *)code,NULL};
    return posix_spawn(pid,"/fat/bin/threadstress",NULL,attr,args,environ);
}
int posix_wait_test(void) {
    static const char *codes[]={"0","1","127","128","200","255","256"};
    for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++) {
        pid_t pid;int status=-1;CHECK(child(&pid,codes[i],NULL)==0);
        CHECK(waitpid(-1,&status,0)==pid && WIFEXITED(status) && !WIFSIGNALED(status));
        CHECK(WEXITSTATUS(status)==(atoi(codes[i])&255));
    }
    pid_t pid;int status=-1;
    CHECK(child(&pid,"signal",NULL)==0 && wait(&status)==pid);
    CHECK(WIFSIGNALED(status) && !WIFEXITED(status) && WTERMSIG(status)==SIGTERM);
    const char *aborts[]={"abort-ignore","abort-catch"};
    for(unsigned i=0;i<2;i++) {
        CHECK(child(&pid,aborts[i],NULL)==0 && waitpid(pid,&status,0)==pid);
        CHECK(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    }
    CHECK(child(&pid,"delay",NULL)==0 && waitpid(pid,&status,WNOHANG)==0);
    CHECK(kill(pid,SIGSTOP)==-1 && errno==EINVAL);
    CHECK(kill(pid,SIGCONT)==-1 && errno==EINVAL);
    CHECK(kill(pid,63)==-1 && errno==EINVAL);
    CHECK(waitpid(0,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==42);
    posix_spawnattr_t attr;CHECK(posix_spawnattr_init(&attr)==0);
    CHECK(posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP)==0 && posix_spawnattr_setpgroup(&attr,0)==0);
    CHECK(child(&pid,"200",&attr)==0 && posix_spawnattr_destroy(&attr)==0);
    CHECK(waitpid(0,&status,WNOHANG)==-1 && errno==ECHILD);
    CHECK(waitpid(-pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==200);
    CHECK(waitpid(pid,&status,0)==-1 && errno==ECHILD);
    CHECK(waitpid(-1,&status,0)==-1 && errno==ECHILD);
    CHECK(child(&pid,"255",NULL)==0 && waitpid(pid,NULL,0)==pid);
    int command_status=system("exit 200");
    CHECK(command_status!=-1 && WIFEXITED(command_status) && WEXITSTATUS(command_status)==200);
    puts("threadstress: wait ok");return 0;
}
