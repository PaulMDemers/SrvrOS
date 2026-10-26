#include <errno.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/exec_limits.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: exec-vectors line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static size_t boundary_length(void) {
    return ARG_MAX-5*sizeof(char *)-strlen("threadstress")-1-strlen("exec-budget-child")-1-1;
}
int posix_exec_vectors_child(int argc,char **argv) {
    if(!strcmp(argv[1],"exec-budget-child")) {
        if(argc!=3 || strlen(argv[2])!=boundary_length()) return 11;
        for(size_t i=0;i<boundary_length();i++) if(argv[2][i]!='b') return 12;
        return environ[0]?13:0;
    }
    if(argc!=SRV_EXEC_ARGS_MAX || strlen(argv[2])!=8192) return 21;
    for(int i=3;i<argc;i++) if(strcmp(argv[i],"arg")) return 22;
    for(unsigned i=0;i<128;i++) {
        char name[16];snprintf(name,sizeof(name),"V%u",i);
        char *value=getenv(name);if(!value || strcmp(value,"kept")) return 23;
    }
    /* Mutation of a full inherited vector must not drop its final entry. */
    if(setenv("EXTRA","new",1)!=-1 || errno!=ENOMEM || !getenv("V127")) return 24;
    if(unsetenv("V0") || setenv("EXTRA","new",1) || strcmp(getenv("EXTRA"),"new")) return 25;
    return 0;
}
int posix_exec_vectors_test(void) {
    CHECK(sysconf(_SC_ARG_MAX)==ARG_MAX && ARG_MAX>=_POSIX_ARG_MAX);
    char *args[SRV_EXEC_ARGS_MAX+1],*env[SRV_EXEC_ENV_MAX+2],entries[SRV_EXEC_ENV_MAX][20];
    static char longarg[ARG_MAX+1];memset(longarg,'a',8192);longarg[8192]=0;
    args[0]="threadstress";args[1]="exec-vectors-child";args[2]=longarg;
    for(unsigned i=3;i<SRV_EXEC_ARGS_MAX;i++) args[i]="arg";
    args[SRV_EXEC_ARGS_MAX]=NULL;
    for(unsigned i=0;i<SRV_EXEC_ENV_MAX;i++) { snprintf(entries[i],sizeof(entries[i]),"V%u=kept",i);env[i]=entries[i]; }
    env[SRV_EXEC_ENV_MAX]=NULL;
    for(int kind=0;kind<2;kind++) {
        pid_t child;
        if(kind==0) CHECK(!posix_spawn(&child,"/fat/bin/threadstress",NULL,NULL,args,env));
        else {
            child=fork();CHECK(child>=0);
            if(!child) { execve("/fat/bin/threadstress",args,env);_exit(77); }
        }
        int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    env[SRV_EXEC_ENV_MAX]="OVER=limit";env[SRV_EXEC_ENV_MAX+1]=NULL;
    CHECK(execve("/fat/bin/threadstress",args,env)==-1 && errno==E2BIG);
    char *empty[]={NULL};char *boundary[]={"threadstress","exec-budget-child",longarg,NULL};
    size_t length=boundary_length();memset(longarg,'b',length);longarg[length]=0;
    pid_t child;CHECK(!posix_spawn(&child,"/fat/bin/threadstress",NULL,NULL,boundary,empty));
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    longarg[length]='b';longarg[length+1]=0;
    CHECK(execve("/fat/bin/threadstress",boundary,empty)==-1 && errno==E2BIG);
    /* Directly supplied oversized environ is never silently truncated by setenv. */
    char **saved=environ;environ=env;
    CHECK(setenv("NEW","value",1)==-1 && errno==ENOMEM && environ==env && getenv("OVER"));
    environ=saved;
    puts("threadstress: exec-vectors ok");return 0;
}
