/* Guest-side real output failure and repeated held-pipe interruption checks.
 * Probe does not claim cancellation inside a comparison algorithm call. */
#include <srvros/sys.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("diff-probe: FAIL line %d: %s\n",__LINE__,#x);return 1; } } while (0)
static char *env[]={"PATH=/fat/bin","LC_ALL=C",NULL};
static char *args[]={"/fat/bin/diff","--","/fat/diff-test/work.a","/fat/diff-test/work.b",NULL};
static long start(int output) {
    struct srv_exec_request req={.path=args[0],.argv=args,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=0,.stdout_fd=output,.stderr_fd=2,.process_group=UINT64_MAX};
    return srv_exec(&req);
}
static long wait_bounded(long pid,uint64_t *status) {
    for(int i=0;i<500;i++) {long n=srv_wait((uint64_t)pid,status,SRV_WAIT_NOHANG);if(n)return n;srv_sleep_ticks(1);}
    return 0;
}
static int alive(long pid) {
    struct srv_process_info info;
    return srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)>0 && info.pid==(uint64_t)pid && strcmp(info.state,"exited");
}
static int ready(int fd,int events) {
    struct srv_pollfd p={.fd=fd,.events=(int16_t)events};
    return srv_poll(&p,1,0)>0 && (p.revents&events);
}
static int write_errors(void) {
    int readonly=(int)srv_open("/fat/diff-test/work.a");CHECK(readonly>=0);
    long pid=start(readonly);CHECK(pid>0);uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("diff-probe: read-only output wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status==2);srv_close(readonly);
    int pipe[2];CHECK(srv_pipe(pipe)==0);srv_close(pipe[0]);
    pid=start(pipe[1]);srv_close(pipe[1]);CHECK(pid>0);
    done=wait_bounded(pid,&status);
    printf("diff-probe: closed consumer wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status==2);return 0;
}
static int interrupt_output(uint64_t signal) {
    int pipe[2];CHECK(srv_pipe(pipe)==0);long flags=srv_fcntl(pipe[1],SRV_F_GETFL,0);
    long pid=start(pipe[1]);CHECK(pid>0);
    /* A nonempty, nonwritable pipe with its reader held open establishes that
     * output made progress and the consumer cannot unblock it via EOF. */
    int full=0;
    for(int i=0;i<1500;i++) {
        if(ready(pipe[0],SRV_POLLIN) && !ready(pipe[1],SRV_POLLOUT) && alive(pid)) {full=1;break;}
        srv_sleep_ticks(1);
    }
    CHECK(full);srv_sleep_ticks(20);CHECK(alive(pid));
    CHECK(srv_kill_signal(pid,signal)==0);
    uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("diff-probe: blocked output signal=%llu wait=%ld status=%llu\n",(unsigned long long)signal,done,(unsigned long long)status);
    CHECK(done==pid && status!=0 && status!=1);
    CHECK(srv_fcntl(pipe[1],SRV_F_GETFL,0)==flags);
    srv_close(pipe[0]);srv_close(pipe[1]);return 0;
}
/* Diagnostic fd 3 marks the second Myers iteration. It proves comparison
 * progress but not signal delivery inside a specific algorithm instruction. */
static int interrupt_compare(uint64_t signal) {
    int out=open("/fat/diff-test/cpu-partial.out",O_WRONLY|O_CREAT|O_TRUNC,0600);CHECK(out>=0);
    int p[2];CHECK(srv_pipe(p)==0);
    char *argv[]={"/fat/diff-test/diff-diagnostic","--","/fat/diff-test/cpu.a","/fat/diff-test/cpu.b",NULL};
    struct srv_spawn_file_action actions[]={
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=out,.new_fd=1},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=p[1],.new_fd=3}};
    struct srv_exec_request req={.path=argv[0],.argv=argv,.envp=env,
        .flags=SRV_EXEC_BACKGROUND|SRV_EXEC_INHERIT_FILES,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,
        .file_actions=actions,.file_action_count=2,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);close(out);srv_close(p[1]);
    if(pid<=0) printf("diff-probe: diagnostic exec failed=%ld\n",pid);
    CHECK(pid>0);int progress=0;
    for(int i=0;i<500;i++) {
        if(ready(p[0],SRV_POLLIN)) {char c=0;CHECK(srv_read(p[0],&c,1)==1 && c=='C');progress=1;break;}
        CHECK(alive(pid));srv_sleep_ticks(1);
    }
    CHECK(progress);srv_sleep_ticks(20);
    struct srv_process_info info;
    CHECK(srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)>0 && info.pid==(uint64_t)pid && strcmp(info.state,"exited"));
    printf("diff-probe: comparison progress signal=%llu state=%s mapped=%llu\n",(unsigned long long)signal,info.state,(unsigned long long)info.user_mapped_bytes);
    CHECK(srv_kill_signal(pid,signal)==0);uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("diff-probe: comparison signal wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status!=0 && status!=1);srv_close(p[0]);return 0;
}
static int report_stat(void) {
    const char *paths[]={"/fat/diff-test/middle.a","/fat/diff-test/middle.b"};
    for(unsigned i=0;i<2;i++) {
        struct stat st;
        if(stat(paths[i],&st)<0) {perror("diff-probe stat");return 1;}
        printf("%s %lld %ld\n",paths[i],(long long)st.st_mtim.tv_sec,(long)st.st_mtim.tv_nsec);
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--stat")) return report_stat();
    CHECK(!interrupt_compare(SRV_SIGNAL_INT));CHECK(!interrupt_compare(SRV_SIGNAL_INT));
    CHECK(!interrupt_compare(SRV_SIGNAL_TERM));CHECK(!interrupt_compare(SRV_SIGNAL_TERM));
    CHECK(!write_errors());
    for(int i=0;i<2;i++) {CHECK(!interrupt_output(SRV_SIGNAL_INT));CHECK(!interrupt_output(SRV_SIGNAL_TERM));}
    puts("diff-probe: PASS output errors and repeated signals after comparison progress and blocked output");
    return 0;
}
