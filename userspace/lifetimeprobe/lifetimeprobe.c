#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("lifetimeprobe: FAIL line=%d\n",__LINE__);return 1; } } while(0)
static long spawn(const char *mode,uint64_t group) {
    char *args[]={"/fat/bin/lifetimeprobe",(char *)mode,NULL};
    struct srv_exec_request r={.path=args[0],.argv=args,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,.process_group=group};
    return srv_exec(&r);
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    if(!strcmp(argv[1],"--status17"))return 17;
    if(!strcmp(argv[1],"--churn")) {
        for(unsigned i=0;i<128;i++) {
            long pid=spawn("--status17",UINT64_MAX);CHECK(pid>0);
            uint64_t status=0;CHECK(srv_wait(pid,&status,0)==pid && status==17);
        }
        CHECK(srv_spawn_bg("/fat/bin/no-such-lifetime-program")<0);
        CHECK(srv_spawn_args("/fat/bin/lifetimeprobe","--status17")==17);
        CHECK(srv_spawn_args_redirect("/fat/bin/lifetimeprobe","--status17","/fat/lifetime-test.txt",0)==17);
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
