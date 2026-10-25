#include <errno.h>
#include <srvros/exec_limits.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: fork line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static int hook_trace,private_global=17;
static void prepare_one(void) { hook_trace=hook_trace*10+1; }
static void prepare_two(void) { hook_trace=hook_trace*10+2; }
static void parent_one(void) { hook_trace=hook_trace*10+3; }
static void parent_two(void) { hook_trace=hook_trace*10+4; }
static void child_one(void) { hook_trace=hook_trace*10+5; }
static void child_two(void) { hook_trace=hook_trace*10+6; }
static unsigned worker_ready,worker_release;
static void *held_worker(void *unused) {
    (void)unused;__atomic_store_n(&worker_ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&worker_release,__ATOMIC_ACQUIRE)) sched_yield();
    return NULL;
}
int posix_fork_exec_child(void) {
    const char *value=getenv("FORK_EXEC_TEST");
    if(!value || strcmp(value,"kept")) return 11;
    sigset_t mask,pending;
    struct sigaction action;
    if(pthread_sigmask(SIG_BLOCK,NULL,&mask) || !sigismember(&mask,SIGUSR1)) return 12;
    if(sigpending(&pending) || !sigismember(&pending,SIGUSR1)) return 13;
    if(sigaction(SIGUSR2,NULL,&action) || action.sa_handler!=SIG_IGN) return 14;
    int received=0;
    if(sigwait(&mask,&received) || received!=SIGUSR1) return 15;
    return 207;
}
int posix_fork_test(void) {
    CHECK(pthread_atfork(prepare_one,parent_one,child_one)==0);
    CHECK(pthread_atfork(prepare_two,parent_two,child_two)==0);
    unsigned char *memory=mmap(NULL,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    CHECK(memory!=MAP_FAILED);for(unsigned i=0;i<65536;i++) memory[i]=(unsigned char)(i*7);
    int file=open("/fat/fork.tmp",O_CREAT|O_TRUNC|O_RDWR|O_CLOEXEC,0600),channel[2];
    CHECK(file>=0 && write(file,"abc",3)==3 && pipe(channel)==0);
    sigset_t blocked,old,pending;sigemptyset(&blocked);sigaddset(&blocked,SIGUSR1);
    CHECK(pthread_sigmask(SIG_BLOCK,&blocked,&old)==0 && raise(SIGUSR1)==0);
    struct rlimit saved,limit={16,32};
    CHECK(getrlimit(RLIMIT_FSIZE,&saved)==0 && setrlimit(RLIMIT_FSIZE,&limit)==0);
    pid_t parent=getpid();hook_trace=0;errno=E2BIG;
    pid_t child=fork();CHECK(child>=0);
    if(!child) {
        int bad=hook_trace!=2156 || getpid()==parent || getppid()!=parent || errno!=E2BIG;
        if(sigpending(&pending) || sigismember(&pending,SIGUSR1)) bad=1;
        if(pthread_sigmask(SIG_BLOCK,NULL,&pending) || !sigismember(&pending,SIGUSR1)) bad=1;
        if(getrlimit(RLIMIT_FSIZE,&limit) || limit.rlim_cur!=16 || limit.rlim_max!=32) bad=1;
        for(unsigned i=0;i<65536;i++) if(memory[i]!=(unsigned char)(i*7)) bad=1;
        memory[0]=99;private_global=91;errno=ERANGE;
        char byte=0;
        if(fcntl(file,F_GETFD)!=FD_CLOEXEC || lseek(file,0,SEEK_SET)!=0 || read(file,&byte,1)!=1 || byte!='a') bad=1;
        if(pwrite(file,"Z",1,5)!=1 || close(file)) bad=1;
        if(close(channel[0]) || write(channel[1],bad?"bad":"yes",3)!=3 || close(channel[1])) bad=1;
        _exit(bad?1:200);
    }
    CHECK(hook_trace==2134 && close(channel[1])==0);
    char reply[3];CHECK(read(channel[0],reply,3)==3 && !memcmp(reply,"yes",3));
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==200);
    CHECK(memory[0]==0 && private_global==17 && close(channel[0])==0);
    CHECK(lseek(file,0,SEEK_CUR)==1);
    char data[6];CHECK(pread(file,data,6,0)==6 && !memcmp(data,"abc\0\0Z",6));
    CHECK(close(file)==0 && unlink("/fat/fork.tmp")==0 && munmap(memory,65536)==0);
    int received=0;CHECK(sigwait(&blocked,&received)==0 && received==SIGUSR1);
    CHECK(pthread_sigmask(SIG_SETMASK,&old,NULL)==0 && setrlimit(RLIMIT_FSIZE,&saved)==0);
    for(unsigned i=0;i<8;i++) {
        hook_trace=0;child=fork();CHECK(child>=0);
        if(!child) _exit(128+i);
        CHECK(hook_trace==2134 && waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==128+(int)i);
    }
    for(unsigned mode=0;mode<6;mode++) {
        hook_trace=0;child=fork();CHECK(child>=0);
        if(!child) {
            char *args[]={"threadstress","wait-child","255",NULL};
            if(mode==0) execl("/fat/bin/threadstress",args[0],args[1],args[2],(char *)NULL);
            if(mode==1) { setenv("PATH","/missing:/fat/bin",1);execlp("threadstress",args[0],args[1],args[2],(char *)NULL); }
            if(mode==2) {
                char *env[]={"FORK_EXEC_TEST=kept",NULL};
                sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGUSR1);
                if(pthread_sigmask(SIG_BLOCK,&blocked,NULL) || raise(SIGUSR1) || signal(SIGUSR2,SIG_IGN)==SIG_ERR) _exit(3);
                execle("/fat/bin/threadstress",args[0],"fork-exec-child",(char *)NULL,env);
            }
            if(mode==3) execv("/fat/bin/threadstress",args);
            if(mode==4) { chdir("/fat/bin");setenv("PATH","/missing:",1);execvp("threadstress",args); }
            if(mode==5) {
                chdir("/fat/bin");setenv("PATH","",1);
                pid_t nested;
                if(posix_spawnp(&nested,"threadstress",NULL,NULL,args,NULL) || waitpid(nested,&status,0)!=nested ||
                    !WIFEXITED(status)) _exit(4);
                _exit(WEXITSTATUS(status));
            }
            _exit(2);
        }
        CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(mode==2?207:255));
    }
    char *overfull[SRV_EXEC_ARGS_MAX+2];for(unsigned i=0;i<SRV_EXEC_ARGS_MAX+1;i++) overfull[i]="x";overfull[SRV_EXEC_ARGS_MAX+1]=NULL;
    CHECK(execv("/fat/bin/threadstress",overfull)==-1 && errno==E2BIG);
    static char longarg[SRV_EXEC_BYTES_MAX+1];memset(longarg,'x',SRV_EXEC_BYTES_MAX);longarg[SRV_EXEC_BYTES_MAX]=0;
    char *longargs[]={"threadstress",longarg,NULL};
    CHECK(execv("/fat/bin/threadstress",longargs)==-1 && errno==E2BIG);
    char *emptyargs[]={"non-executable",NULL};
    CHECK(execv("/missing",emptyargs)==-1 && errno==ENOENT);
    file=open("/fat/fork-invalid.tmp",O_CREAT|O_TRUNC|O_WRONLY,0700);
    CHECK(file>=0 && write(file,"bad image",9)==9 && close(file)==0);
    CHECK(execv("/fat/fork-invalid.tmp",emptyargs)==-1 && errno==ENOEXEC);
    CHECK(chmod("/fat/fork-invalid.tmp",0600)==0);
    CHECK(execv("/fat/fork-invalid.tmp",emptyargs)==-1 && errno==EACCES);
    CHECK(unlink("/fat/fork-invalid.tmp")==0);
    const char *script="/fat/fork-script.tmp";
    file=open(script,O_CREAT|O_TRUNC|O_WRONLY,0700);
    const char program[]="exit \"$1\"\n";
    CHECK(file>=0 && write(file,program,sizeof(program)-1)==sizeof(program)-1 && close(file)==0);
    for(unsigned mode=0;mode<2;mode++) {
        child=fork();CHECK(child>=0);
        if(!child) {
            char *arguments[]={"custom-name","37",NULL};
            if(mode) { setenv("PATH","/fat",1);execvp("fork-script.tmp",arguments); }
            else execvp(script,arguments);
            _exit(113);
        }
        CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==37);
    }
    CHECK(unlink(script)==0);
    pthread_t worker;worker_ready=worker_release=0;
    CHECK(pthread_create(&worker,NULL,held_worker,NULL)==0);
    while(!__atomic_load_n(&worker_ready,__ATOMIC_ACQUIRE)) sched_yield();
    hook_trace=0;CHECK(fork()==-1 && errno==EOPNOTSUPP && hook_trace==2134);
    __atomic_store_n(&worker_release,1,__ATOMIC_RELEASE);CHECK(pthread_join(worker,NULL)==0);
    int event=eventfd(0,0);CHECK(event>=0);hook_trace=0;
    CHECK(fork()==-1 && errno==EOPNOTSUPP && hook_trace==2134 && close(event)==0);
    puts("threadstress: fork ok");return 0;
}
