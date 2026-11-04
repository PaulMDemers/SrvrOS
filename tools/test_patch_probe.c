/* Prototype setup-limit and post-admission diagnostic output-limit probes.
 * The diagnostic marker establishes where the latter limit is armed; target
 * behavior, signal and allocator gates remain unverified until a guest run. */
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do {if(!(x)) {printf("patch-probe: FAIL line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static long wait_bounded(long pid,uint64_t *status) {
    for(int i=0;i<500;i++) {long n=srv_wait((uint64_t)pid,status,SRV_WAIT_NOHANG);if(n)return n;srv_sleep_ticks(1);}
    return 0;
}
static int worker(int diagnostic) {
    long stale=srv_open("/fat/patch-test/writefail.out");
    if(stale>=0) {srv_close((int)stale);return 9;}
    if(!diagnostic) {
        struct srv_rlimit limit;
        if(srv_rlimit(SRV_RLIMIT_FSIZE,0,&limit)<0) return 10;
        limit.current=16;
        if(srv_rlimit(SRV_RLIMIT_FSIZE,&limit,0)<0 || srv_signal_config(SRV_SIGNAL_XFSZ,SRV_SIGNAL_IGNORE)<0) return 11;
    }
    char *argv[]={diagnostic?"/fat/patch-test/patch-diagnostic":"/fat/bin/patch","-i","/fat/patch-test/long64.patch","-o","/fat/patch-test/writefail.out","/fat/patch-test/long64.src",NULL};
    char *env[]={"PATH=/fat/bin","LC_ALL=C",diagnostic?"SRV_PATCH_TEST_OUTPUT_LIMIT=1":NULL,NULL};
    struct srv_exec_request req={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=0,.stdout_fd=1,.stderr_fd=1,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);if(pid<=0) return 12;
    uint64_t status=0;long done=wait_bounded(pid,&status);
    if(done==0) {srv_kill_signal(pid,SRV_SIGNAL_KILL);(void)wait_bounded(pid,&status);return 13;}
    if(done!=pid || status!=2) return 14;
    /* Input/output contract forbids a leftover partial artifact. */
    long fd=srv_open("/fat/patch-test/writefail.out");
    if(fd>=0) {srv_close((int)fd);return 15;}
    return 0;
}
static int run_worker(int diagnostic) {
    int pipe[2];CHECK(srv_pipe(pipe)==0);
    char *worker_argv[]={"/fat/patch-test/patch-probe",diagnostic?"--output-worker":"--limited-worker",NULL};
    struct srv_exec_request req={.path=worker_argv[0],.argv=worker_argv,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=0,.stdout_fd=pipe[1],.stderr_fd=2,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);srv_close(pipe[1]);CHECK(pid>0);
    uint64_t status=0;long done=0;
    for(int i=0;i<1200 && !done;i++) {done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG);if(!done)srv_sleep_ticks(1);}
    if(done==0) {srv_kill_signal(pid,SRV_SIGNAL_KILL);done=wait_bounded(pid,&status);}
    long flags=srv_fcntl(pipe[0],SRV_F_GETFL,0);
    CHECK(flags>=0 && srv_fcntl(pipe[0],SRV_F_SETFL,(uint64_t)flags|SRV_FD_NONBLOCK)>=0);
    char diag[8192];size_t used=0;long n;
    while(used<sizeof(diag)-1 && (n=srv_read(pipe[0],diag+used,sizeof(diag)-1-used))>0) used+=(size_t)n;
    diag[used]=0;srv_close(pipe[0]);
    printf("patch-probe: %s worker wait=%ld status=%llu diagnostic=%s\n",diagnostic?"post-admission output limit":"pre-admission size limit",done,(unsigned long long)status,diag);
    CHECK(used>0);
    CHECK(done==pid && status==0);
    if(diagnostic) CHECK(strstr(diag,"PATCH_OUTPUT_LIMIT_ARMED") && strstr(diag,"PATCH_OUTPUT_EFBIG") && !strstr(diag,"PATCH_OUTPUT_LIMIT_ARM_FAILED"));
    return 0;
}
/* The diagnostic C is emitted after an actual offset match attempt; a signal
 * sent afterwards does not imply instruction-level delivery inside a match. */
static int signal_after_match(uint64_t sig) {
    long stale=srv_open("/fat/patch-test/cpu-result.out");
    if(stale>=0) {srv_close((int)stale);puts("patch-probe: stale cpu output");return 1;}
    int marker[2];CHECK(srv_pipe(marker)==0);
    char *args[]={"/fat/patch-test/patch-diagnostic","-i","/fat/patch-test/cpu.patch", "-o", "/fat/patch-test/cpu-result.out", "/fat/patch-test/cpu.src",NULL};
    char *env[]={"PATH=/fat/bin","LC_ALL=C",NULL};
    struct srv_spawn_file_action action={.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=marker[1],.new_fd=3};
    struct srv_exec_request req={.path=args[0],.argv=args,.envp=env,
        .flags=SRV_EXEC_BACKGROUND|SRV_EXEC_INHERIT_FILES,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,
        .file_actions=&action,.file_action_count=1,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);srv_close(marker[1]);
    if(pid<=0) {printf("patch-probe: diagnostic spawn=%ld\n",pid);srv_close(marker[0]);return 1;}
    int progress=0;uint64_t status=~0ULL;long done=0;
    for(int i=0;i<500;i++) {
        struct srv_pollfd p={.fd=marker[0],.events=SRV_POLLIN};
        if(srv_poll(&p,1,0)>0 && (p.revents&SRV_POLLIN)) {
            char c=0;if(srv_read(marker[0],&c,1)==1 && c=='C')progress=1;
            break;
        }
        done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG);
        if(done)break;
        srv_sleep_ticks(1);
    }
    if(progress && !done && srv_kill_signal(pid,sig)==0) done=wait_bounded(pid,&status);
    if(!done) {srv_kill_signal(pid,SRV_SIGNAL_KILL);done=wait_bounded(pid,&status);}
    srv_close(marker[0]);
    long leftover=srv_open("/fat/patch-test/cpu-result.out");
    if(leftover>=0)srv_close((int)leftover);
    printf("patch-probe: match progress=%d signal=%llu wait=%ld status=%llu output-present=%d\n",
        progress,(unsigned long long)sig,done,(unsigned long long)status,leftover>=0);
    return progress && done==pid && status==128+sig && leftover<0 ? 0 : 1;
}
/* Pause after immutable snapshots and remove one original path. The resumed
 * process must use the admitted bytes, not re-open or re-stat that path. */
static int grow_replacement(const char *path) {
    if(srv_unlink(path)<0) return 0;
    long fd=srv_open_mode(path,SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_TRUNC);
    if(fd<0) return 0;
    char block[4096];memset(block,'G',sizeof(block));size_t remaining=256*1024+1;
    while(remaining) {
        size_t n=remaining<sizeof(block)?remaining:sizeof(block);
        long wrote=srv_write((int)fd,block,n);
        if(wrote<=0) {srv_close((int)fd);return 0;}
        remaining-=(size_t)wrote;
    }
    if(srv_close((int)fd)<0) return 0;
    struct srv_stat st;return srv_stat(path,&st)==0 && st.size==256*1024+1;
}
static int snapshot_race(int kind) {
    const char *source=kind==1?"/fat/patch-test/race-patch.src":kind==2?"/fat/patch-test/race-grow.src":"/fat/patch-test/race-source.src";
    const char *patch=kind==1?"/fat/patch-test/race-patch.patch":kind==2?"/fat/patch-test/race-grow.patch":"/fat/patch-test/race-source.patch";
    const char *output=kind==1?"/fat/patch-test/race-patch.out":kind==2?"/fat/patch-test/race-grow.out":"/fat/patch-test/race-source.out";
    int mark[2],gate[2];CHECK(srv_pipe(mark)==0);CHECK(srv_pipe(gate)==0);
    char *args[]={"/fat/patch-test/patch-diagnostic","-i",(char *)patch,"-o",(char *)output,(char *)source,NULL};
    char *env[]={"PATH=/fat/bin","LC_ALL=C","SRV_PATCH_TEST_PAUSE_SNAPSHOT=1",NULL};
    struct srv_spawn_file_action actions[]={
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=mark[1],.new_fd=3},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=gate[0],.new_fd=4},
        {.type=SRV_SPAWN_FILE_ACTION_CLOSE,.fd=gate[1]}};
    struct srv_exec_request req={.path=args[0],.argv=args,.envp=env,
        .flags=SRV_EXEC_BACKGROUND|SRV_EXEC_INHERIT_FILES,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,
        .file_actions=actions,.file_action_count=3,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);srv_close(mark[1]);srv_close(gate[0]);
    if(pid<=0) {srv_close(mark[0]);srv_close(gate[1]);return 1;}
    int ready=0;uint64_t status=~0ULL;long done=0;
    for(int i=0;i<500;i++) {
        struct srv_pollfd p={.fd=mark[0],.events=SRV_POLLIN};
        if(srv_poll(&p,1,0)>0 && (p.revents&SRV_POLLIN)) {
            char byte=0;if(srv_read(mark[0],&byte,1)==1 && byte=='S')ready=1;
            break;
        }
        done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG);if(done)break;
        srv_sleep_ticks(1);
    }
    int removed=0,released=0;
    if(ready && !done && (kind==2?grow_replacement(source):srv_unlink(kind==1?patch:source)==0)) {
        removed=1;released=srv_write(gate[1],"R",1)==1;
        if(released)done=wait_bounded(pid,&status);
    }
    if(!done) {srv_kill_signal(pid,SRV_SIGNAL_KILL);done=wait_bounded(pid,&status);}
    srv_close(gate[1]);srv_close(mark[0]);
    printf("patch-probe: snapshot race remove=%s ready=%d removed=%d released=%d wait=%ld status=%llu\n",
        kind==1?"patch":kind==2?"grow":"source",ready,removed,released,done,(unsigned long long)status);
    return ready && removed && released && done==pid && status==0 ? 0 : 1;
}
/* Diagnostic gate W follows a positive fwrite and successful fflush. The
 * signal is sent during the test gate, not claimed to interrupt that write. */
static int signal_after_output(uint64_t sig) {
    const char *output="/fat/patch-test/output-progress.out";
    long stale=srv_open(output);if(stale>=0) {srv_close((int)stale);return 1;}
    int mark[2],gate[2];CHECK(srv_pipe(mark)==0);CHECK(srv_pipe(gate)==0);
    char *args[]={"/fat/patch-test/patch-diagnostic","-i","/fat/patch-test/long64.patch","-o",(char *)output,"/fat/patch-test/long64.src",NULL};
    char *env[]={"PATH=/fat/bin","LC_ALL=C","SRV_PATCH_TEST_PAUSE_OUTPUT=1",NULL};
    struct srv_spawn_file_action actions[]={
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=mark[1],.new_fd=3},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=gate[0],.new_fd=4},
        {.type=SRV_SPAWN_FILE_ACTION_CLOSE,.fd=gate[1]}};
    struct srv_exec_request req={.path=args[0],.argv=args,.envp=env,
        .flags=SRV_EXEC_BACKGROUND|SRV_EXEC_INHERIT_FILES,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,
        .file_actions=actions,.file_action_count=3,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);srv_close(mark[1]);srv_close(gate[0]);
    if(pid<=0) {srv_close(mark[0]);srv_close(gate[1]);return 1;}
    int progress=0;uint64_t status=~0ULL;long done=0;
    for(int i=0;i<500;i++) {
        struct srv_pollfd p={.fd=mark[0],.events=SRV_POLLIN};
        if(srv_poll(&p,1,0)>0 && (p.revents&SRV_POLLIN)) {
            char byte=0;if(srv_read(mark[0],&byte,1)==1 && byte=='W') {progress=1;break;}
        }
        done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG);if(done)break;
        srv_sleep_ticks(1);
    }
    struct srv_stat st={0};long stat_result=srv_stat(output,&st);
    if(progress && !done && stat_result==0 && st.size>0 && srv_kill_signal(pid,sig)==0) done=wait_bounded(pid,&status);
    if(!done) {srv_kill_signal(pid,SRV_SIGNAL_KILL);done=wait_bounded(pid,&status);}
    srv_close(gate[1]);srv_close(mark[0]);
    long leftover=srv_open(output);if(leftover>=0)srv_close((int)leftover);
    printf("patch-probe: output progress=%d bytes=%llu signal=%llu wait=%ld status=%llu output-present=%d\n",
        progress,(unsigned long long)(stat_result==0?st.size:0),(unsigned long long)sig,done,(unsigned long long)status,leftover>=0);
    return progress && stat_result==0 && st.size>0 && done==pid && status==128+sig && leftover<0 ? 0 : 1;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--limited-worker")) return worker(0);
    if(argc==2 && !strcmp(argv[1],"--output-worker")) return worker(1);
    CHECK(!run_worker(0));CHECK(!run_worker(1));
    CHECK(!snapshot_race(0));CHECK(!snapshot_race(1));CHECK(!snapshot_race(2));
    for(int i=0;i<2;i++) {
        CHECK(!signal_after_match(SRV_SIGNAL_INT));CHECK(!signal_after_match(SRV_SIGNAL_TERM));
        CHECK(!signal_after_output(SRV_SIGNAL_INT));CHECK(!signal_after_output(SRV_SIGNAL_TERM));
    }
    puts("patch-probe: PASS snapshot races, marked output-limit failures and signals after match and output progress");
    return 0;
}
