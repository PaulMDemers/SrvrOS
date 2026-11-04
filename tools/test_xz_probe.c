/* Guest subprocess failure and blocked-I/O cancellation checks for xz.
 * This runs against the production CLI, with real pipes and descriptors.
 * CPU-active and allocator-internal diagnostics require a separate build. */
#ifdef XZ_ALLOC_DIAGNOSTIC
#include <lzma.h>
#include <srvros/sys.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../ports/srvros/xz/allocator.h"
#define CHECK(x) do { if (!(x)) { printf("xz-alloc: FAIL line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static uint8_t plain[65536],compressed[131072],restored[65536];
static size_t compressed_size;
static int encode(size_t fail,size_t *attempts,size_t *peak) {
    lzma_stream stream=LZMA_STREAM_INIT;
    CHECK(srv_xz_allocator_reset(fail)==0);srv_xz_allocator_limit(8*1024*1024);
    stream.allocator=&srv_xz_allocator;
    lzma_ret ret=lzma_easy_encoder(&stream,0,LZMA_CHECK_CRC64);
    if(ret==LZMA_OK) {
        stream.next_in=plain;stream.avail_in=sizeof(plain);
        stream.next_out=compressed;stream.avail_out=sizeof(compressed);
        do {ret=lzma_code(&stream,LZMA_FINISH);} while(ret==LZMA_OK);
        if(ret==LZMA_STREAM_END) compressed_size=sizeof(compressed)-stream.avail_out;
    }
    *attempts=srv_xz_allocator_attempts();*peak=srv_xz_allocator_peak();
    lzma_end(&stream);
    CHECK(!srv_xz_allocator_live() && !srv_xz_allocator_bytes());
    if(fail==SIZE_MAX) CHECK(ret==LZMA_STREAM_END && *peak<=8*1024*1024);
    else CHECK(ret==LZMA_MEM_ERROR);
    return 0;
}
static int decode(size_t fail,size_t *attempts,size_t *peak) {
    lzma_stream stream=LZMA_STREAM_INIT;
    CHECK(srv_xz_allocator_reset(fail)==0);srv_xz_allocator_limit(32*1024*1024);
    stream.allocator=&srv_xz_allocator;
    lzma_ret ret=lzma_stream_decoder(&stream,32*1024*1024,0);
    if(ret==LZMA_OK) {
        stream.next_in=compressed;stream.avail_in=compressed_size;
        stream.next_out=restored;stream.avail_out=sizeof(restored);
        do {ret=lzma_code(&stream,LZMA_FINISH);} while(ret==LZMA_OK);
        if(ret==LZMA_STREAM_END) CHECK(stream.avail_out==0 && !memcmp(plain,restored,sizeof(plain)));
    }
    *attempts=srv_xz_allocator_attempts();*peak=srv_xz_allocator_peak();
    lzma_end(&stream);
    CHECK(!srv_xz_allocator_live() && !srv_xz_allocator_bytes());
    if(fail==SIZE_MAX) CHECK(ret==LZMA_STREAM_END && *peak<=32*1024*1024);
    else CHECK(ret==LZMA_MEM_ERROR);
    return 0;
}
static uint64_t own_mapped(void) {
    struct srv_process_info info;
    long got=srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)getpid(),&info);
    return got>0 ? info.user_mapped_bytes : 0;
}
int main(void) {
    CHECK(srv_xz_allocator_reset(SIZE_MAX)==0);srv_xz_allocator_limit(4096);
    void *p=srv_xz_allocator.alloc(NULL,1,128);CHECK(p && srv_xz_allocator_live()==1);
    CHECK(!srv_xz_allocator.alloc(NULL,2,4096));
    CHECK(!srv_xz_allocator.alloc(NULL,SIZE_MAX,2));
    srv_xz_allocator.free(NULL,p);CHECK(!srv_xz_allocator_live() && !srv_xz_allocator_bytes());
    CHECK(srv_xz_allocator_reset(0)==0 && !srv_xz_allocator.alloc(NULL,1,1));
    CHECK(srv_xz_allocator_reset(SIZE_MAX)==0);
    p=srv_xz_allocator.alloc(NULL,1,128);CHECK(p);srv_xz_allocator.free(NULL,p);
    CHECK(!srv_xz_allocator_live() && !srv_xz_allocator_bytes());
    uint32_t r=0x12345678;for(size_t i=0;i<sizeof(plain);i++) {r=r*1664525u+1013904223u;plain[i]=(uint8_t)(r>>24);}
    size_t enc_count,dec_count,enc_peak,dec_peak,count,peak;
    CHECK(!encode(SIZE_MAX,&enc_count,&enc_peak));
    /* Failure attempts overwrite compressed, so regenerate the valid stream. */
    for(size_t i=0;i<enc_count;i++) CHECK(!encode(i,&count,&peak));
    CHECK(!encode(SIZE_MAX,&count,&peak));
    CHECK(!decode(SIZE_MAX,&dec_count,&dec_peak));
    for(size_t i=0;i<dec_count;i++) CHECK(!decode(i,&count,&peak));
    uint64_t baseline=own_mapped();CHECK(baseline>0);
    for(int i=0;i<64;i++) {
        CHECK(!encode(SIZE_MAX,&count,&peak));CHECK(!decode(SIZE_MAX,&count,&peak));
        CHECK(own_mapped()==baseline);
    }
    printf("xz-alloc: private mapped bytes before/after cycles=%llu\n",(unsigned long long)baseline);
    printf("xz-alloc: PASS encoder attempts=%llu peak=%llu decoder attempts=%llu peak=%llu; 64 repeat cycles and each allocation failure cleaned\n",
        (unsigned long long)enc_count,(unsigned long long)enc_peak,
        (unsigned long long)dec_count,(unsigned long long)dec_peak);
    return 0;
}
#else
#include <fcntl.h>
#include <unistd.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("xz-probe: FAIL line %d: %s\n",__LINE__,#x);return 1; } } while (0)
static char *env[]={"PATH=/fat/bin","LC_ALL=C",NULL};
static char *from_pipe[]={"/fat/bin/xz","-0","-c","--memlimit-compress=8MiB","--no-adjust",NULL};
static char *from_file[]={"/fat/bin/xz","-0","-c","--memlimit-compress=8MiB","--no-adjust","/fat/xz-test/binary.raw",NULL};
static long start(char **argv,int input,int output) {
    struct srv_exec_request req={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=input,.stdout_fd=output,.stderr_fd=2,.process_group=UINT64_MAX};
    return srv_exec(&req);
}
static long wait_bounded(long pid,uint64_t *status) {
    for (int i=0;i<500;i++) {
        long done=srv_wait((uint64_t)pid,status,SRV_WAIT_NOHANG);
        if (done) return done;
        srv_sleep_ticks(1);
    }
    return 0;
}
static uint64_t mapped(long pid) {
    struct srv_process_info info;
    return srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)>0 && info.pid==(uint64_t)pid
        ? info.user_mapped_bytes : 0;
}
static int alive(long pid) {
    struct srv_process_info info;
    return srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)>0 &&
        info.pid==(uint64_t)pid && strcmp(info.state,"exited");
}
static int poll_ready(int fd,int events) {
    struct srv_pollfd p={.fd=fd,.events=(int16_t)events};
    long n=srv_poll(&p,1,0);
    return n>0 && (p.revents&events);
}
static int interrupt_input(uint64_t signal) {
    int p[2];CHECK(srv_pipe(p)==0);
    long flags=srv_fcntl(p[0],SRV_F_GETFL,0);
    CHECK(srv_write(p[1],"partial input\n",14)==14);
    long pid=start(from_pipe,p[0],1);CHECK(pid>0);
    /* Observe that the prefix was consumed while the writer remains open. */
    int consumed=0;
    for(int i=0;i<500;i++) {
        if (!poll_ready(p[0],SRV_POLLIN) && alive(pid)) { consumed=1;break; }
        srv_sleep_ticks(1);
    }
    CHECK(consumed);srv_sleep_ticks(20);CHECK(alive(pid));
    CHECK(srv_kill_signal(pid,signal)==0);
    uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("xz-probe: input signal=%llu wait=%ld status=%llu\n",(unsigned long long)signal,done,(unsigned long long)status);
    CHECK(done==pid && status!=0);
    CHECK(srv_fcntl(p[0],SRV_F_GETFL,0)==flags);
    srv_close(p[0]);srv_close(p[1]);return 0;
}
static int interrupt_output(uint64_t signal) {
    int p[2];CHECK(srv_pipe(p)==0);
    long flags=srv_fcntl(p[1],SRV_F_GETFL,0);
    int input=(int)srv_open("/fat/xz-test/binary.raw");CHECK(input>=0);
    long pid=start(from_file,input,p[1]);CHECK(pid>0);srv_close(input);
    /* Keep reader and writer open and never drain. POLLOUT clearing proves
     * output filled the pipe after a positive transfer, before SIGINT. */
    int full=0;uint64_t peak=0;
    for(int i=0;i<1000;i++) {
        uint64_t sample=mapped(pid);if(sample>peak)peak=sample;
        if (poll_ready(p[0],SRV_POLLIN) && !poll_ready(p[1],SRV_POLLOUT) && alive(pid)) { full=1;break; }
        srv_sleep_ticks(1);
    }
    CHECK(full);srv_sleep_ticks(20);CHECK(alive(pid));
    printf("xz-probe: compression sampled max private mapped bytes=%llu (not RSS or true peak)\n",(unsigned long long)peak);
    CHECK(peak>0);
    CHECK(srv_kill_signal(pid,signal)==0);
    uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("xz-probe: output signal=%llu wait=%ld status=%llu\n",(unsigned long long)signal,done,(unsigned long long)status);
    CHECK(done==pid && status!=0);
    CHECK(srv_fcntl(p[1],SRV_F_GETFL,0)==flags);
    srv_close(p[0]);srv_close(p[1]);return 0;
}
static int write_failures(void) {
    int input=(int)srv_open("/fat/xz-test/binary.raw");CHECK(input>=0);
    int readonly=(int)srv_open("/fat/xz-test/binary.raw");CHECK(readonly>=0);
    long pid=start(from_file,input,readonly);CHECK(pid>0);
    uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("xz-probe: readonly output wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status!=0);srv_close(readonly);srv_close(input);
    input=(int)srv_open("/fat/xz-test/binary.raw");CHECK(input>=0);
    int p[2];CHECK(srv_pipe(p)==0);srv_close(p[0]);
    pid=start(from_file,input,p[1]);CHECK(pid>0);srv_close(p[1]);
    done=wait_bounded(pid,&status);
    printf("xz-probe: closed consumer wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status!=0);srv_close(input);return 0;
}
static int decode_mapping_sample(void) {
    int p[2];CHECK(srv_pipe(p)==0);
    int input=(int)srv_open("/fat/xz-test/binary.xz");CHECK(input>=0);
    char *args[]={"/fat/bin/xz","-dc","--memlimit-decompress=32MiB","/fat/xz-test/binary.xz",NULL};
    long pid=start(args,input,p[1]);srv_close(input);CHECK(pid>0);
    int full=0;uint64_t peak=0;
    for(int i=0;i<1000;i++) {
        uint64_t sample=mapped(pid);if(sample>peak)peak=sample;
        if(poll_ready(p[0],SRV_POLLIN) && !poll_ready(p[1],SRV_POLLOUT) && alive(pid)) {full=1;break;}
        srv_sleep_ticks(1);
    }
    CHECK(full && peak>0);
    printf("xz-probe: preset6 decode sampled max private mapped bytes=%llu (not RSS or true peak)\n",(unsigned long long)peak);
    CHECK(srv_kill_signal(pid,SRV_SIGNAL_INT)==0);
    uint64_t status=0;long done=wait_bounded(pid,&status);
    CHECK(done==pid && status!=0);srv_close(p[0]);srv_close(p[1]);return 0;
}
/* Diagnostic CLI emits C after its first lzma_code return. Use regular file
 * output so a full pipe cannot explain cancellation. The marker demonstrates
 * progress, not delivery inside a codec call. */
static int signal_after_codec_progress(uint64_t signal) {
    int out=open("/fat/xz-test/cpu-partial.xz",O_WRONLY|O_CREAT|O_TRUNC,0600);CHECK(out>=0);
    int p[2];CHECK(srv_pipe(p)==0);
    char *args[]={"/fat/xz-test/xz-diagnostic","-0","-c","--memlimit-compress=8MiB",
        "--no-adjust","/fat/xz-test/cpu.raw",NULL};
    struct srv_spawn_file_action actions[]={
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=out,.new_fd=1},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=p[1],.new_fd=3}};
    struct srv_exec_request req={.path=args[0],.argv=args,.envp=env,
        .flags=SRV_EXEC_BACKGROUND|SRV_EXEC_INHERIT_FILES,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,
        .file_actions=actions,.file_action_count=2,.process_group=UINT64_MAX};
    long pid=srv_exec(&req);close(out);srv_close(p[1]);
    if(pid<=0) printf("xz-probe: diagnostic exec failed=%ld\n",pid);
    CHECK(pid>0);
    int ready=0;
    for(int i=0;i<500;i++) {
        if(poll_ready(p[0],SRV_POLLIN)) {char c=0;CHECK(srv_read(p[0],&c,1)==1 && c=='C');ready=1;break;}
        CHECK(alive(pid));srv_sleep_ticks(1);
    }
    CHECK(ready);srv_sleep_ticks(20);
    struct srv_process_info info;
    CHECK(srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)pid,&info)>0 && info.pid==(uint64_t)pid && strcmp(info.state,"exited"));
    printf("xz-probe: codec progress signal=%llu state=%s mapped=%llu\n",
        (unsigned long long)signal,info.state,(unsigned long long)info.user_mapped_bytes);
    CHECK(srv_kill_signal(pid,signal)==0);
    uint64_t status=0;long done=wait_bounded(pid,&status);
    printf("xz-probe: codec signal wait=%ld status=%llu\n",done,(unsigned long long)status);
    CHECK(done==pid && status!=0);srv_close(p[0]);return 0;
}
int main(void) {
    CHECK(!signal_after_codec_progress(SRV_SIGNAL_INT));
    CHECK(!signal_after_codec_progress(SRV_SIGNAL_INT));
    CHECK(!signal_after_codec_progress(SRV_SIGNAL_TERM));
    CHECK(!decode_mapping_sample());
    CHECK(!write_failures());
    for(int i=0;i<2;i++) {
        CHECK(!interrupt_input(SRV_SIGNAL_INT));CHECK(!interrupt_output(SRV_SIGNAL_INT));
        CHECK(!interrupt_input(SRV_SIGNAL_TERM));CHECK(!interrupt_output(SRV_SIGNAL_TERM));
    }
    puts("xz-probe: PASS write errors blocked I/O signals and cancellation after codec progress");
    return 0;
}

#endif /* XZ_ALLOC_DIAGNOSTIC */
