#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
static int test_open(const char *,int,...);
static int test_close(int);
static ssize_t test_read(int,void *,size_t);
static ssize_t test_write(int,const void *,size_t);
static int test_fsync(int);
static int test_mkdir(const char *,mode_t);
#define open test_open
#define close test_close
#define read test_read
#define write test_write
#define fsync test_fsync
#define mkdir test_mkdir
#include "../userspace/playwav/io_stress.c"
#undef open
#undef close
#undef read
#undef write
#undef fsync
#undef mkdir
static unsigned pipes_created,duplex[32],closed[32],nonblock[32],active,finished,killed,waited;
static unsigned fail_fcntl,completion_mode;
static uint64_t bytes;
static char saved[640];
static int test_open(const char *path,int flags,...) {
    assert(!active);
    if(strstr(path,"disk.receipt")){assert(flags==O_RDONLY);return 100;}
    assert(strstr(path,"result.txt") && flags==(O_WRONLY|O_CREAT|O_EXCL));return 101;
}
static int test_close(int fd) {assert(!active && (fd==100 || fd==101));return 0;}
static ssize_t test_read(int fd,void *out,size_t n) {
    assert(!active && fd==100 && n==sizeof(struct audio_io_receipt));
    struct audio_io_receipt r={.first=1100000,.last=6100000,.bursts=1,.bytes=262144,.max_us=5000000};
    memcpy(out,&r,n);return n;
}
static ssize_t test_write(int fd,const void *in,size_t n) {
    assert(!active && fd==101 && n<sizeof(saved));memcpy(saved,in,n);saved[n]=0;return n;
}
static int test_fsync(int fd) {assert(!active && fd==101);return 0;}
static int test_mkdir(const char *path,mode_t mode) {assert(!active && path[0]=='/' && mode==0700);return 0;}
int gui_audio(unsigned op,unsigned value,struct srv_audio_status *status) {
    assert(op==SRV_AUDIO_QUERY && !value);*status=(struct srv_audio_status){.volume=1};return 0;
}
static long make_pipe(int fds[2],int pair) {
    assert(!active && pipes_created<2);fds[0]=10+2*pipes_created++;fds[1]=fds[0]+1;
    duplex[fds[0]]=duplex[fds[1]]=pair;return 0;
}
long srv_pipe(int fds[2]) {return make_pipe(fds,0);}
long srv_pipe_pair(int fds[2]) {return make_pipe(fds,1);}
long srv_fcntl(int fd,int cmd,uint64_t flags) {
    assert(!active && (fd==11 || fd==12) && cmd==SRV_F_SETFL && flags==SRV_FD_NONBLOCK);
    if(fail_fcntl==(unsigned)fd)return -1;
    nonblock[fd]=1;return 0;
}
long srv_close(int fd) {
    assert(fd>=10 && fd<14 && !closed[fd]);
    /* Kernel simple-pipe close has an atomic prelude. Duplex close does not:
     * inject refusal here rather than let a storage wait hide in live PCM. */
    assert(!active || !duplex[fd]);closed[fd]=1;return 0;
}
long srv_exec(const struct srv_exec_request *r) {
    assert(!active && r->stdin_fd==10 && r->stdout_fd==13 && r->stderr_fd==2);return 42;
}
long srv_read(int fd,void *out,size_t n) {
    assert(fd==12 && nonblock[fd] && !closed[fd] && n==1);
    uint64_t minimum=(completion_mode==4?10:completion_mode==5?4:8)*192000;
    if(bytes<minimum || completion_mode==3)return SRV_ERR_AGAIN;
    if(completion_mode==2)return 0;
    *(char *)out=completion_mode==1?'X':'D';return 1;
}
long srv_write(int fd,const void *in,size_t n) {
    assert(active && fd==11 && nonblock[fd] && !closed[fd] && n==1 && *(const char *)in=='G');return 1;
}
long srv_monotonic_us(void) {return 1000000+bytes*1000000/192000;}
long srv_sleep_ticks(uint64_t ticks) {assert(ticks==1);return 0;}
long srv_sync(void) {assert(!active);return 0;}
long srv_kill(int64_t pid) {assert(!active && pid==42);killed++;return 0;}
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {assert(!active && pid==42 && (flags==SRV_WAIT_NOHANG || (flags==0 && killed)));*status=0;waited++;return pid;}
long srv_pcm_open(void) {active=1;return 1;}
long srv_pcm_write(uint64_t handle,const void *data,size_t n) {assert(active && handle==1 && data && n<=SRV_PCM_MAX_WRITE && n%4==0);bytes+=n;return n;}
long srv_pcm_finish(uint64_t handle) {assert(active && handle==1);finished=1;return 0;}
long srv_pcm_query(uint64_t handle,struct srv_pcm_status *out) {
    assert(active && handle==1);*out=(struct srv_pcm_status){.state=finished?SRV_PCM_DONE:SRV_PCM_PLAYING,.position_bytes=bytes,.submitted_bytes=bytes};return 0;
}
long srv_pcm_close(uint64_t handle) {assert(active && handle==1);active=0;return 0;}
static void reset(void) {
    pipes_created=active=finished=killed=waited=fail_fcntl=completion_mode=0;bytes=0;saved[0]=0;
    memset(duplex,0,sizeof(duplex));memset(closed,0,sizeof(closed));memset(nonblock,0,sizeof(nonblock));
}
static void retired(void) {for(unsigned i=10;i<14;i++)assert(closed[i]);assert(!active && waited==1);}
int main(void) {
    reset();assert(!producer(8));retired();assert(finished && !killed && bytes==8*192000 && strstr(saved,"result=PASS") && strstr(saved,"volume=1"));
    for(unsigned mode=4;mode<=5;mode++) {
        reset();completion_mode=mode;assert(!producer(8));retired();assert(finished && !killed && strstr(saved,"result=PASS"));
        if(mode==4)assert(bytes>=10*192000 && bytes<10*192000+SRV_PCM_MAX_WRITE);
        else assert(bytes==8*192000); /* early disk completion cannot shorten PCM */
    }
    for(unsigned mode=1;mode<=3;mode++) {
        reset();completion_mode=mode;assert(producer(8)==1);retired();assert(!finished && !killed && strstr(saved,"result=FAIL"));
        if(mode==3)assert(bytes==(8+AUDIO_IO_TAIL_SECONDS)*192000);
    }
    for(unsigned fd=11;fd<=12;fd++) {
        reset();fail_fcntl=fd;assert(producer(8)==1);retired();assert(killed==1 && !bytes && !finished);
    }
    puts("audio-io-producer: PASS actual producer, one-way/no gated live close, nonblocking start/completion, full minimum, bounded tail, malformed/EOF refusal and descriptor/child retirement");
}
