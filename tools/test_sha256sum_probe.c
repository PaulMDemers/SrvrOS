/* Real pipe progress, cooperative signals and output error for sha256sum.
 * Pipe drain establishes input consumption, not an internal hash checkpoint. */
#include <srvros/sys.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("sha256sum-probe: FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static char *env[] = {"PATH=/fat/bin", "LC_ALL=C", NULL};
static char *stdin_args[] = {"/fat/bin/sha256sum", "--", NULL};
static char *file_args[] = {"/fat/bin/sha256sum", "--", "/fat/sha256sum-test/above", NULL};
static long start(char **args, int input, int output) {
    struct srv_exec_request req = {.path=args[0], .argv=args, .envp=env,
        .flags=SRV_EXEC_BACKGROUND, .stdin_fd=input, .stdout_fd=output,
        .stderr_fd=2, .process_group=UINT64_MAX};
    return srv_exec(&req);
}
static long wait_bounded(long pid, uint64_t *status) {
    for (int i=0; i<500; i++) { long n=srv_wait((uint64_t)pid,status,SRV_WAIT_NOHANG); if(n) return n; srv_sleep_ticks(1); }
    return 0;
}
static int alive(long pid) {
    struct srv_process_info info;
    return srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)>0 && info.pid==(uint64_t)pid && strcmp(info.state,"exited");
}
static int interrupt_after_input(uint64_t signal) {
    int p[2]; CHECK(srv_pipe(p)==0);
    long pid=start(stdin_args,p[0],1); srv_close(p[0]); CHECK(pid>0);
    static char block[32768]; memset(block,0xa5,sizeof(block));
    long flags=srv_fcntl(p[1],SRV_F_GETFL,0);
    if(flags<0 || srv_fcntl(p[1],SRV_F_SETFL,(uint64_t)flags|SRV_FD_NONBLOCK)<0) goto fail;
    uint64_t first=0,last=0;
    /* One sample after 32 KiB, another after 1 MiB more. Each write and
     * drain has its own bounded retry; leave the writer open throughout. */
    for(int chunk=0;chunk<33;chunk++) {
        size_t offset=0;
        for(int i=0;i<500 && offset<sizeof(block);i++) {
            long n=srv_write(p[1],block+offset,sizeof(block)-offset);
            if(n>0) offset+=(size_t)n;
            else { if(!alive(pid)) goto fail; srv_sleep_ticks(1); }
        }
        if(offset!=sizeof(block)) goto fail;
        int drained=0;
        for(int i=0;i<500;i++) {
            struct srv_stat st;
            if(srv_fstat(p[1],&st)==0 && st.size==0 && alive(pid)) { drained=1; break; }
            if(!alive(pid)) goto fail;
            srv_sleep_ticks(1);
        }
        if(!drained) goto fail;
        if(chunk==0 || chunk==32) {
            struct srv_process_info info;
            if(srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)<=0 ||
               info.pid!=(uint64_t)pid || !strcmp(info.state,"exited")) goto fail;
            if(chunk==0) first=info.user_mapped_bytes; else last=info.user_mapped_bytes;
        }
    }
    /* These are private mapped-user snapshots, excluding kernel/shared pages;
     * they are neither resident-set size nor a measured high-water mark. */
    printf("sha256sum-probe: private mapped user bytes after 32768=%llu after 1081344=%llu (excludes kernel/shared; not RSS/peak)\n",
           (unsigned long long)first,(unsigned long long)last);
    if(!first || !last || (last>first && last-first>128u*1024u)) goto fail;
    printf("sha256sum-probe: consumed 1081344 pipe bytes before signal %llu\n",(unsigned long long)signal);
    if(srv_kill_signal(pid,signal)!=0) goto fail;
    uint64_t status=0; long done=wait_bounded(pid,&status);
    srv_close(p[1]);
    printf("sha256sum-probe: signal wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status==128+signal);
    return 0;
fail:
    printf("sha256sum-probe: no observable progress or signal delivery failed\n");
    srv_kill_signal(pid,SRV_SIGNAL_KILL); { uint64_t status; wait_bounded(pid,&status); }
    srv_close(p[1]); return 1;
}
/* Fill before spawning so a nonblocking reader gets one complete block, then
 * encounters EAGAIN while a writer is still open. Capture stdout to ensure a
 * failed hash never emits a digest. */
static int late_input_error(void) {
    int input[2], output[2];
    CHECK(srv_pipe(input)==0);
    if(srv_pipe(output)!=0) { srv_close(input[0]); srv_close(input[1]); return 1; }
    long flags=srv_fcntl(input[0],SRV_F_GETFL,0);
    if(flags<0 || srv_fcntl(input[0],SRV_F_SETFL,(uint64_t)flags|SRV_FD_NONBLOCK)<0) goto fail_fds;
    /* Nonblocking write avoids hanging if the actual capacity is too small. */
    long wf=srv_fcntl(input[1],SRV_F_GETFL,0);
    if(wf<0 || srv_fcntl(input[1],SRV_F_SETFL,(uint64_t)wf|SRV_FD_NONBLOCK)<0) goto fail_fds;
    static char block[32768]; memset(block,0x5a,sizeof(block));
    size_t written=0;
    while(written<sizeof(block)) {
        long n=srv_write(input[1],block+written,sizeof(block)-written);
        if(n<=0) { printf("sha256sum-probe: pipe capacity below 32768: %llu bytes\n",(unsigned long long)written); goto fail_fds; }
        written+=(size_t)n;
    }
    struct srv_stat st;
    if(srv_fstat(input[0],&st)!=0 || st.size!=sizeof(block)) goto fail_fds;
    long pid=start(stdin_args,input[0],output[1]);
    srv_close(input[0]); srv_close(output[1]);
    if(pid<=0) { srv_close(input[1]); srv_close(output[0]); return 1; }
    uint64_t status=0; long done=wait_bounded(pid,&status);
    if(done==0) { srv_kill_signal(pid,SRV_SIGNAL_KILL); wait_bounded(pid,&status); }
    srv_close(input[1]);
    char data[128]; long count=srv_read(output[0],data,sizeof(data)); srv_close(output[0]);
    printf("sha256sum-probe: late input wait=%ld status=%llu stdout=%ld\n",done,(unsigned long long)status,count);
    CHECK(done==pid && status==1 && count==0);
    return 0;
fail_fds:
    srv_close(input[0]); srv_close(input[1]); srv_close(output[0]); srv_close(output[1]);
    return 1;
}
static int output_error(void) {
    int fd=(int)srv_open("/fat/sha256sum-test/above"); CHECK(fd>=0);
    long pid=start(file_args,0,fd); srv_close(fd); CHECK(pid>0);
    uint64_t status=0; long done=wait_bounded(pid,&status);
    if(done==0) { srv_kill_signal(pid,SRV_SIGNAL_KILL); wait_bounded(pid,&status); }
    printf("sha256sum-probe: read-only output wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status==1); return 0;
}
int main(void) {
    CHECK(output_error()==0);
    CHECK(late_input_error()==0);
    CHECK(interrupt_after_input(SRV_SIGNAL_INT)==0);
    CHECK(interrupt_after_input(SRV_SIGNAL_TERM)==0);
    puts("sha256sum-probe: PASS"); return 0;
}
