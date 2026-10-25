#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: pipe-atomic line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
struct pipe_writer { int fd,tag,rounds;unsigned started,done; };
static void *writer(void *data) {
    struct pipe_writer *w=data;char record[PIPE_BUF];memset(record,'A'+w->tag,sizeof(record));
    __atomic_store_n(&w->started,1,__ATOMIC_RELEASE);
    for(int i=0;i<w->rounds;i++) if(write(w->fd,record,sizeof(record))!=sizeof(record)) return (void *)1;
    __atomic_store_n(&w->done,1,__ATOMIC_RELEASE);return NULL;
}
static int read_all(int fd,char *data,size_t size) {
    size_t done=0;while(done<size) { ssize_t n=read(fd,data+done,size-done);if(n<=0) return -1;done+=(size_t)n; }return 0;
}
int posix_pipe_atomic_test(void) {
    int fds[2];static char data[65536],out[65536];memset(data,'A',sizeof(data));
    CHECK(PIPE_BUF>=_POSIX_PIPE_BUF && !pipe2(fds,O_NONBLOCK));
    CHECK(fpathconf(fds[1],_PC_PIPE_BUF)==PIPE_BUF);
    CHECK(write(fds[1],data,sizeof(data)-PIPE_BUF/2)==sizeof(data)-PIPE_BUF/2);
    CHECK(write(fds[1],data,PIPE_BUF)==-1 && errno==EAGAIN);
    CHECK(!read_all(fds[0],out,sizeof(data)-PIPE_BUF/2) && !memcmp(out,data,sizeof(data)-PIPE_BUF/2));
    CHECK(read(fds[0],out,1)==-1 && errno==EAGAIN);
    CHECK(write(fds[1],data,sizeof(data)-2)==sizeof(data)-2);
    CHECK(write(fds[1],data,PIPE_BUF+1)==2);
    CHECK(!read_all(fds[0],out,sizeof(out)) && !memcmp(out,data,sizeof(out)));
    CHECK(!fcntl(fds[0],F_SETFL,0) && !fcntl(fds[1],F_SETFL,0));
    CHECK(write(fds[1],data,sizeof(data)-PIPE_BUF/2)==sizeof(data)-PIPE_BUF/2);
    struct pipe_writer blocked={.fd=fds[1],.tag=1,.rounds=1};pthread_t task;
    CHECK(!pthread_create(&task,NULL,writer,&blocked));
    while(!__atomic_load_n(&blocked.started,__ATOMIC_ACQUIRE)) sched_yield();
    usleep(30000);CHECK(!__atomic_load_n(&blocked.done,__ATOMIC_ACQUIRE));
    CHECK(!read_all(fds[0],out,PIPE_BUF/2));
    void *result;CHECK(!pthread_join(task,&result) && !result);
    CHECK(!read_all(fds[0],out,sizeof(out)));
    for(size_t i=0;i<sizeof(out);i++) CHECK(out[i]==(i<sizeof(out)-PIPE_BUF?'A':'B'));
    struct pipe_writer writers[4];pthread_t tasks[4];unsigned counts[4]={0};
    for(int i=0;i<4;i++) { writers[i]=(struct pipe_writer){.fd=fds[1],.tag=i,.rounds=40};CHECK(!pthread_create(&tasks[i],NULL,writer,&writers[i])); }
    for(int i=0;i<160;i++) {
        CHECK(!read_all(fds[0],out,PIPE_BUF));int tag=out[0]-'A';CHECK(tag>=0 && tag<4);counts[tag]++;
        for(int j=1;j<PIPE_BUF;j++) CHECK(out[j]==out[0]);
    }
    for(int i=0;i<4;i++) CHECK(!pthread_join(tasks[i],&result) && !result && counts[i]==40);
    CHECK(!close(fds[0]));
    sigset_t blocked_signals,old_mask,pending;sigemptyset(&blocked_signals);sigaddset(&blocked_signals,SIGPIPE);
    CHECK(!pthread_sigmask(SIG_BLOCK,&blocked_signals,&old_mask));
    CHECK(write(fds[1],"x",1)==-1 && errno==EPIPE);
    CHECK(!sigpending(&pending) && sigismember(&pending,SIGPIPE));int received=0;
    CHECK(!sigwait(&blocked_signals,&received) && received==SIGPIPE);
    CHECK(!pthread_sigmask(SIG_SETMASK,&old_mask,NULL));
    struct sigaction ignore={.sa_handler=SIG_IGN},old_action;CHECK(!sigaction(SIGPIPE,&ignore,&old_action));
    CHECK(write(fds[1],"x",1)==-1 && errno==EPIPE);
    CHECK(!sigpending(&pending) && !sigismember(&pending,SIGPIPE));
    pid_t child=fork();CHECK(child>=0);
    if(!child) { signal(SIGPIPE,SIG_DFL);write(fds[1],"x",1);_exit(91); }
    int status;CHECK(waitpid(child,&status,0)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGPIPE);
    CHECK(!sigaction(SIGPIPE,&old_action,NULL) && !close(fds[1]));
    puts("threadstress: pipe-atomic ok");return 0;
}
