#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define CHECK(x) do { if(!(x)) { printf("lifetimeprobe: FAIL line=%d\n",__LINE__);return 1; } } while(0)
static long spawn(const char *mode,uint64_t group) {
    char *args[]={"/fat/bin/lifetimeprobe",(char *)mode,NULL};
    struct srv_exec_request r={.path=args[0],.argv=args,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,.process_group=group};
    return srv_exec(&r);
}
struct waiter { uint64_t pid,status;long result; };
static void *wait_child(void *arg) {
    struct waiter *w=arg;
    do { w->result=srv_wait(w->pid,&w->status,0); }while(w->result==SRV_ERR_INTR);
    return NULL;
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    if(!strcmp(argv[1],"--status17"))return 17;
    if(!strcmp(argv[1],"--delayed17")) { srv_sleep_ticks(3);return 17; }
    if(!strcmp(argv[1],"--churn")) {
        for(unsigned i=0;i<128;i++) {
            long pid=spawn("--status17",UINT64_MAX);CHECK(pid>0);
            uint64_t status=0;CHECK(srv_wait(pid,&status,0)==pid && status==17);
        }
        CHECK(srv_spawn_bg("/fat/bin/no-such-lifetime-program")<0);
        CHECK(srv_spawn_args("/fat/bin/lifetimeprobe","--status17")==17);
        char *args[]={"/fat/bin/lifetimeprobe","--status17",NULL};
        struct srv_exec_request r={.path=args[0],.argv=args,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
        CHECK(srv_exec(&r)==17);
        for(unsigned i=0;i<16;i++) {
            long pid=spawn("--delayed17",0);CHECK(pid>0);
            struct waiter w[2]={{.pid=pid},{.pid=pid}};pthread_t threads[2];
            CHECK(!pthread_create(&threads[0],NULL,wait_child,&w[0]));
            CHECK(!pthread_create(&threads[1],NULL,wait_child,&w[1]));
            CHECK(!pthread_join(threads[0],NULL) && !pthread_join(threads[1],NULL));
            unsigned winner=w[0].result==pid?0:1,loser=1-winner;
            CHECK(w[winner].result==pid && w[winner].status==17 && w[loser].result==-1);
        }
        puts("lifetimeprobe: competing-waiters=16 one-winner=yes");
        puts("lifetimeprobe: churn=128 failed-spawn=refused synchronous=2");return 0;
    }
    if(!strcmp(argv[1],"--orphan")) {
        CHECK(spawn("--session",0)>0);return 0;
    }
    if(!strcmp(argv[1],"--session")) {
        CHECK(setsid()>0);
        char *args[]={"/fat/bin/lifetimeprobe","--leaf",NULL};
        srv_execve(args[0],args,NULL);CHECK(0);
    }
    if(!strcmp(argv[1],"--leaf")) {
        CHECK(spawn("--hold",UINT64_MAX)>0);
        puts("lifetimeprobe: orphan setsid exec grandchild started");
    } else CHECK(!strcmp(argv[1],"--hold"));
    CHECK(srv_syscall0(SYS_PROC_READY)==0);
    for(;;)srv_sleep_ticks(1);
}
