/* Opt-in desktop test worker. All writes stay inside a freshly created run directory. */
#include <srvros/desktop_trial.h>
#include <srvros/trial_file.h>
#include <srvros/sys.h>
#include <sys/stat.h>
#include <errno.h>

static int io_round(const char *dir,const char *run,unsigned round) {
    char temp[160],payload[160]; unsigned char block[32768];
    snprintf(temp,sizeof(temp),"%s/payload.tmp",dir);
    snprintf(payload,sizeof(payload),"%s/payload.bin",dir);
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) return -1;
    int ok=1;
    for (unsigned start=0;ok && start<DESK_IO_BYTES;start+=sizeof(block)) {
        for (unsigned i=0;i<sizeof(block);i++) block[i]=desk_trial_byte(run,round,start+i);
        ok=trial_write_all(fd,block,sizeof(block))==0;
    }
    if (ok) ok=fsync(fd)==0;
    if (close(fd)!=0) ok=0;
    if (!ok || (round>1 && unlink(payload)!=0) || rename(temp,payload)!=0) return -1;
    fd=open(payload,O_RDONLY); if (fd<0) return -1;
    for (unsigned start=0;ok && start<DESK_IO_BYTES;) {
        ssize_t got=read(fd,block,sizeof(block));
        if (got<=0 || (unsigned)got>DESK_IO_BYTES-start) { ok=0; break; }
        for (unsigned i=0;i<(unsigned)got;i++) if (block[i]!=desk_trial_byte(run,round,start+i)) ok=0;
        start+=(unsigned)got;
    }
    if (ok && read(fd,block,1)!=0) ok=0;
    if (close(fd)!=0) ok=0;
    return ok?0:-1;
}
static int progress(unsigned rounds,unsigned state) {
    struct desk_io_progress p={DESK_IO_MAGIC,rounds,state,0};
    return trial_write_all(1,&p,sizeof(p));
}
static int failure(const char *stage) {
    fprintf(stderr,"deskio: failed stage=%s errno=%d\n",stage,errno); return 1;
}
static int stop_requested(void) {
    /* Readiness polling handles redirected stdin; the current libc fcntl
     * shortcut for fd 0 does not apply O_NONBLOCK to its backing pipe. */
    struct srv_pollfd p={.fd=0,.events=SRV_POLLIN};
    long ready=srv_poll(&p,1,0);
    if (ready<0 || (p.revents&(SRV_POLLERR|SRV_POLLNVAL))) return -1;
    return (p.revents&(SRV_POLLIN|SRV_POLLHUP))?1:0;
}
int main(int argc,char **argv) {
    if (argc!=2 || !desk_trial_run_valid(argv[1])) return failure("arguments");
    const char *run=argv[1];
    char dir[128],request[160],expected[64],actual[64],result[160],body[256];
    snprintf(request,sizeof(request),"/work/srvros-work/trackpad-%s.req",run);
    snprintf(expected,sizeof(expected),"STARTED %s\n",run);
    int fd=open(request,O_RDONLY); if (fd<0) return failure("request-open");
    ssize_t got=read(fd,actual,sizeof(actual)); close(fd);
    if (got!=(ssize_t)strlen(expected) || memcmp(actual,expected,(size_t)got)) return failure("request-identity");
    snprintf(dir,sizeof(dir),"/work/srvros-work/desktop-%s",run);
    if (mkdir(dir,0700)!=0) return failure("mkdir");
    long started=srv_monotonic_us();
    if(started<=0) return failure("reference-clock");
    unsigned rounds=0,state=DESK_IO_RUNNING;
    if (progress(0,state)!=0) return failure("progress-pipe");
    while (rounds<DESK_IO_ROUNDS) {
        int n=stop_requested();
        if (n>0) { state=DESK_IO_STOPPED; break; }
        if (n<0) { state=DESK_IO_FAILED; break; }
        if (io_round(dir,run,rounds+1)!=0 || srv_sync()!=0) { state=DESK_IO_FAILED; break; }
        rounds++;
        if (progress(rounds,DESK_IO_RUNNING)!=0) { state=DESK_IO_FAILED; break; }
        /* Pace the workload, leaving time for physical interaction. */
        for (unsigned i=0;i<10;i++) {
            srv_sleep_ticks(10);
            n=stop_requested();
            if (n>0) { state=DESK_IO_STOPPED; break; }
            if (n<0) { state=DESK_IO_FAILED; break; }
        }
        if (state!=DESK_IO_RUNNING) break;
    }
    if (state==DESK_IO_RUNNING) state=DESK_IO_COMPLETE;
    long ended=srv_monotonic_us();
    if(ended<started) state=DESK_IO_FAILED;
    snprintf(result,sizeof(result),"%s/work-clock.txt",dir);
    snprintf(body,sizeof(body),"format=srvros-desktop-workclock-v1\nrun=%s\nstart_us=%llu\nend_us=%llu\n",
        run,(unsigned long long)started,(unsigned long long)(ended>0?ended:0));
    if(trial_record(result,body)!=0 || srv_sync()!=0) state=DESK_IO_FAILED;
    snprintf(result,sizeof(result),"%s/result.txt",dir);
    snprintf(body,sizeof(body),"format=srvros-desktop-io-v1\nrun=%s\nstate=%u\nrounds=%u\nbytes=%u\n",
        run,state,rounds,rounds*DESK_IO_BYTES);
    if (trial_record(result,body)!=0 || srv_sync()!=0) state=DESK_IO_FAILED;
    if (progress(rounds,state)!=0) return 1;
    return state==DESK_IO_FAILED?1:0;
}
