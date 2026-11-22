/* Guest-side real output failure and repeated held-pipe interruption checks.
 * Probe does not claim cancellation inside a comparison algorithm call. */
#include <srvros/sys.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("grep-probe: FAIL line %d: %s\n",__LINE__,#x);return 1; } } while (0)
static char *env[]={"PATH=/fat/bin","LC_ALL=C",NULL};
static char *args[]={"/fat/bin/ggrep","-F","--","x","/fat/grep-test/work",NULL};
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
    int readonly=(int)srv_open("/fat/grep-test/work");CHECK(readonly>=0);
    long pid=start(readonly);CHECK(pid>0);uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("grep-probe: read-only output wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status==2);srv_close(readonly);
    int pipe[2];CHECK(srv_pipe(pipe)==0);srv_close(pipe[0]);
    pid=start(pipe[1]);srv_close(pipe[1]);CHECK(pid>0);
    done=wait_bounded(pid,&status);
    printf("grep-probe: closed consumer wait=%ld status=%llu\n",done,(unsigned long long)status);
    /* Native GNU grep also keeps the default SIGPIPE disposition. */
    CHECK(done==pid && status==128+SRV_SIGNAL_PIPE);return 0;
}
/* Capture the state before changing the pipe or killing a stuck child. */
static void output_failure_diagnostics(long pid,int read_fd,int write_fd,long initial_flags) {
    struct srv_pollfd p[2]={{.fd=read_fd,.events=SRV_POLLIN},{.fd=write_fd,.events=SRV_POLLOUT}};
    long polled=srv_poll(p,2,0);struct srv_stat pipe_stat;long stat_result=srv_fstat(read_fd,&pipe_stat);
    struct srv_process_info info;long listed=srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info);
    uint64_t status=~0ULL;long done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG);
    long read_flags=srv_fcntl(read_fd,SRV_F_GETFL,0),write_flags=srv_fcntl(write_fd,SRV_F_GETFL,0);
    printf("grep-probe: output timeout pid=%ld list=%ld state=%s poll=%ld read-revents=%d write-revents=%d read-flags=%ld write-flags=%ld initial-write-flags=%ld fstat=%ld pipe-bytes=%llu wait=%ld status=%llu\n",
        pid,listed,listed>0?info.state:"unavailable",polled,(int)p[0].revents,(int)p[1].revents,
        read_flags,write_flags,initial_flags,stat_result,(unsigned long long)(stat_result==0?pipe_stat.size:0),done,(unsigned long long)status);
    if(done==0) {
        long killed=srv_kill_signal(pid,SRV_SIGNAL_KILL);
        done=wait_bounded(pid,&status);
        printf("grep-probe: cleanup kill=%ld wait=%ld status=%llu\n",killed,done,(unsigned long long)status);
    }
    if(read_flags>=0 && srv_fcntl(read_fd,SRV_F_SETFL,(uint64_t)read_flags|SRV_FD_NONBLOCK)>=0) {
        char data[1024];size_t total=0;long n;
        while((n=srv_read(read_fd,data,sizeof(data)))>0) total+=(size_t)n;
        printf("grep-probe: output timeout buffered-bytes=%llu final-read=%ld\n",(unsigned long long)total,n);
    } else puts("grep-probe: output timeout could not enable nonblocking read");
}
static int interrupt_output(uint64_t signal) {
    int pipe[2];CHECK(srv_pipe(pipe)==0);long flags=srv_fcntl(pipe[1],SRV_F_GETFL,0);
    long pid=start(pipe[1]);
    if(pid<=0) {printf("grep-probe: output spawn failed=%ld\n",pid);srv_close(pipe[0]);srv_close(pipe[1]);return 1;}
    /* fstat on a pipe reports current bytes. Since POLLOUT only means at
     * least one free byte, use stable near-capacity occupancy with the reader
     * held open; do not alter the child's shared write descriptor flags.
     * 65536 is the current kernel pipe capacity, not a public ABI promise. */
    int stalled=0;struct srv_stat prior,after;
    for(int i=0;i<1500;i++) {
        if(srv_fstat(pipe[0],&prior)==0 && prior.size>=65536-SRV_PIPE_BUF && alive(pid)) {
            srv_sleep_ticks(20);
            if(srv_fstat(pipe[0],&after)==0 && after.size==prior.size && alive(pid)) {stalled=1;break;}
        }
        srv_sleep_ticks(1);
    }
    if(!stalled) {puts("grep-probe: near-full output did not remain stable");goto failed;}
    printf("grep-probe: held output before=%llu after=%llu poll-in=%d poll-out=%d flags=%ld\n",
        (unsigned long long)prior.size,(unsigned long long)after.size,ready(pipe[0],SRV_POLLIN),ready(pipe[1],SRV_POLLOUT),flags);
    if(srv_kill_signal(pid,signal)!=0) {puts("grep-probe: output signal failed");goto failed;}
    uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("grep-probe: held output stall signal=%llu wait=%ld status=%llu\n",(unsigned long long)signal,done,(unsigned long long)status);
    if(done!=pid || status!=128+signal) {puts("grep-probe: output signal did not produce the expected signal status");goto failed;}
    if(srv_fcntl(pipe[1],SRV_F_GETFL,0)!=flags) {puts("grep-probe: output flags changed");goto failed;}
    srv_close(pipe[0]);srv_close(pipe[1]);return 0;
failed:
    output_failure_diagnostics(pid,pipe[0],pipe[1],flags);
    srv_close(pipe[0]);srv_close(pipe[1]);return 1;
}
/* Exercise patterns too large for the interactive shell's argument limit. */
static int pattern_limit(void) {
    static char pattern[1026];
    memset(pattern,'x',1025);pattern[1025]=0;
    char *argv[]={"/fat/bin/ggrep","-F","-q","--",pattern,"/fat/grep-test/envelope",NULL};
    for(int length=1024;length<=1025;length++) {
        pattern[length]=0;
        int out=open("/fat/grep-test/pattern.out",O_WRONLY|O_CREAT|O_TRUNC,0600);
        int err=open("/fat/grep-test/pattern.err",O_WRONLY|O_CREAT|O_TRUNC,0600);
        CHECK(out>=0 && err>=0);
        struct srv_exec_request req={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
            .stdin_fd=0,.stdout_fd=out,.stderr_fd=err,.process_group=UINT64_MAX};
        long pid=srv_exec(&req);srv_close(out);srv_close(err);CHECK(pid>0);
        uint64_t status=~0ULL;long done=wait_bounded(pid,&status);
        printf("grep-probe: pattern length=%d wait=%ld status=%llu\n",length,done,(unsigned long long)status);
        CHECK(done==pid && status==(length==1024?0:2));
        err=open("/fat/grep-test/pattern.err",O_RDONLY);CHECK(err>=0);
        char c;long n=srv_read(err,&c,1);srv_close(err);
        CHECK((length==1024 && n==0) || (length==1025 && n==1));
        pattern[length]='x';
    }
    return 0;
}
/* FD 3 reports a genuine phase and FD 4 holds the child there. The
 * signal is delivered while the diagnostic handshake is blocked; this does
 * not claim the signal arrived inside an uninstrumented instruction. */
static int phase_test(char phase, uint64_t signal) {
    int mark[2], release[2];
    CHECK(srv_pipe(mark)==0); CHECK(srv_pipe(release)==0);
    int m=(int)srv_fcntl(mark[1],SRV_F_DUPFD,16);
    int r=(int)srv_fcntl(release[0],SRV_F_DUPFD,16);
    int out=open("/fat/grep-test/phase.out",O_WRONLY|O_CREAT|O_TRUNC,0600);
    int o=out>=0?(int)srv_fcntl(out,SRV_F_DUPFD,16):-1;
    CHECK(m>=16 && r>=16 && o>=16);
    char pause[]="SRV_GREP_TEST_PAUSE=I"; pause[20]=phase;
    char *environment[]={"PATH=/fat/bin","LC_ALL=C",pause,NULL};
    char *argv[]={"/fat/grep-test/ggrep-diagnostic","-F","--","zzzzzz","/fat/grep-test/work",NULL};
    struct srv_spawn_file_action actions[]={
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=m,.new_fd=3},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=r,.new_fd=4},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=o,.new_fd=1}};
    struct srv_exec_request req={.path=argv[0],.argv=argv,.envp=environment,
        .flags=SRV_EXEC_BACKGROUND|SRV_EXEC_INHERIT_FILES,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,
        .file_actions=actions,.file_action_count=3,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);
    srv_close(m);srv_close(r);srv_close(o);srv_close(out);srv_close(mark[1]);srv_close(release[0]);
    if(pid<=0) {printf("grep-probe: phase %c exec=%ld\n",phase,pid);srv_close(mark[0]);srv_close(release[1]);return 1;}
    int progress=0; char byte=0;
    for(int i=0;i<1500;i++) {
        if(ready(mark[0],SRV_POLLIN)) { if(srv_read(mark[0],&byte,1)==1 && byte==phase) progress=1; break; }
        if(!alive(pid)) break;
        srv_sleep_ticks(1);
    }
    uint64_t status=~0ULL; long done=0; int ok=progress && alive(pid);
    if(ok && signal) {
        ok=srv_kill_signal(pid,signal)==0;
        if(ok) {done=wait_bounded(pid,&status);ok=done==pid && status==128+signal;}
    } else if(ok) {
        byte='R';ok=srv_write(release[1],&byte,1)==1;
        if(ok) {done=wait_bounded(pid,&status);ok=done==pid && status==1;}
    }
    printf("grep-probe: gated phase=%c marker=%d signal=%llu wait=%ld status=%llu\n",
        phase,progress,(unsigned long long)signal,done,(unsigned long long)status);
    if(!ok && done!=pid) {srv_kill_signal(pid,SRV_SIGNAL_KILL);wait_bounded(pid,&status);}
    srv_close(mark[0]);srv_close(release[1]);return ok?0:1;
}
int main(void) {
    CHECK(!write_errors());
    CHECK(!pattern_limit());
    for(int i=0;i<2;i++) {CHECK(!interrupt_output(SRV_SIGNAL_INT));CHECK(!interrupt_output(SRV_SIGNAL_TERM));}
    const char phases[]="ICM";
    for(unsigned i=0;i<sizeof phases-1;i++) {
        CHECK(!phase_test(phases[i],0));
        CHECK(!phase_test(phases[i],SRV_SIGNAL_INT));
        CHECK(!phase_test(phases[i],SRV_SIGNAL_TERM));
    }
    puts("grep-probe: PASS output errors, held output signals, and gated phase resume and signals");
    return 0;
}
