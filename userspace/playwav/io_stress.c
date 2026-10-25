/* Boot-only diagnostic. PCM producer never performs filesystem work while
 * streaming. A separate process continuously uses ordinary writable Home I/O. */
#include "io_stress.h"
#include "io_stress_receipt.h"
#include <srvros/audio.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BLOCK 16384
#define CHUNKS 16
#define BURST_BYTES (BLOCK*CHUNKS)
static unsigned char block[BLOCK],check[BLOCK];
static int16_t pcm[SRV_PCM_MAX_WRITE/2];
static uint64_t now(void) { long n=srv_monotonic_us();return n>0?(uint64_t)n:0; }
static int save(const char *path,const void *p,size_t n) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return -1;
    int ok=write(fd,p,n)==(ssize_t)n && fsync(fd)==0;
    if(close(fd))ok=0;return ok?0:-1;
}
static int disk_worker(const char *directory,unsigned seconds) {
    if(chdir(directory))return 1;
    char go;if(srv_read(0,&go,1)!=1 || go!='G')return 1;
    uint64_t begin=now(),end=begin+(uint64_t)(seconds-3)*1000000;
    struct audio_io_receipt r={0};
    while(now()<end) {
        uint64_t start=now();
        for(unsigned i=0;i<BLOCK;i++)block[i]=(unsigned char)(i+r.bursts*17);
        int fd=open("payload.tmp",O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return 1;
        int ok=1;
        for(unsigned i=0;i<CHUNKS;i++)if(write(fd,block,BLOCK)!=BLOCK){ok=0;break;}
        if(fsync(fd))ok=0;if(close(fd))ok=0;
        if(!ok || rename("payload.tmp","payload.bin"))return 1;
        fd=open("payload.bin",O_RDONLY);if(fd<0)return 1;
        for(unsigned i=0;i<CHUNKS;i++)if(read(fd,check,BLOCK)!=BLOCK || memcmp(check,block,BLOCK)){ok=0;break;}
        char extra;if(read(fd,&extra,1)!=0)ok=0;if(close(fd))ok=0;
        if(!ok || unlink("payload.bin") || srv_sync()<0)return 1;
        uint64_t finish=now();if(!r.first)r.first=start;
        r.last=finish;r.bursts++;r.bytes+=BURST_BYTES;
        if(finish-start>r.max_us)r.max_us=finish-start;
    }
    return save("disk.receipt",&r,sizeof(r))?1:0;
}
static int producer(unsigned seconds) {
    char directory[160],duration[24];
    snprintf(directory,sizeof(directory),"/work/srvros-work/audio-io-%llu",(unsigned long long)now());
    if(mkdir(directory,0700))return 1;
    int gate[2];if(srv_pipe_pair(gate)<0)return 1;
    snprintf(duration,sizeof(duration),"%u",seconds);
    char *args[]={"/fat/bin/playwav","--io-worker",directory,duration,NULL};
    struct srv_exec_request request={.path=args[0],.argv=args,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=gate[0],.stdout_fd=1,.stderr_fd=2};
    long child=srv_exec(&request);srv_close(gate[0]);
    if(child<=0){srv_close(gate[1]);return 1;}
    printf("audio-io: begin seconds=%u directory=%s automatic=yes\n",seconds,directory);fflush(stdout);
    long handle=srv_pcm_open();int failed=handle<=0,started=0;
    uint64_t start=0,end=0,submitted=0,frames=(uint64_t)seconds*48000,phase=0;
    struct srv_pcm_status status={0};
    /* A quiet triangle wave with alternating pitch each second makes glitches
     * audible without depending on disk reads in the producer itself. */
    while(!failed && submitted<frames*4) {
        unsigned count=(unsigned)((frames*4-submitted)<sizeof(pcm)?(frames*4-submitted):sizeof(pcm));
        for(unsigned i=0;i<count/4;i++) {
            unsigned hz=((submitted/4+i)/48000)&1?440:330;
            phase=(phase+hz)%48000;
            int v=phase<24000?(int)phase:48000-(int)phase;
            pcm[2*i]=pcm[2*i+1]=(int16_t)((v-12000)/6);
        }
        unsigned sent=0,waits=0;
        while(sent<count && !failed) {
            long n=srv_pcm_write(handle,(unsigned char *)pcm+sent,count-sent);
            if(n==SRV_PCM_AGAIN) {
                if(++waits>500 || srv_sleep_ticks(1)<0)failed=1;
            } else if(n<=0 || (unsigned long)n>count-sent)failed=1;
            else {sent+=(unsigned)n;submitted+=(unsigned)n;waits=0;}
            if(!started && !failed) {
                if(srv_pcm_query(handle,&status)<0)failed=1;
                else if(status.state==SRV_PCM_PLAYING && status.position_bytes) {
                    start=now();started=1;
                    if(srv_write(gate[1],"G",1)!=1)failed=1;
                    srv_close(gate[1]);gate[1]=-1;
                }
            }
        }
    }
    if(!failed && srv_pcm_finish(handle)<0)failed=1;
    for(unsigned waits=0;!failed;waits++) {
        if(srv_pcm_query(handle,&status)<0){failed=1;break;}
        if(status.state==SRV_PCM_DONE){end=now();break;}
        if(status.state!=SRV_PCM_DRAINING || waits>600 || srv_sleep_ticks(1)<0){failed=1;break;}
    }
    if(handle>0)srv_pcm_close(handle);
    if(gate[1]>=0)srv_close(gate[1]);
    uint64_t exit_status=1,deadline=now()+60000000;long waited=0;
    do {waited=srv_wait(child,&exit_status,SRV_WAIT_NOHANG);if(waited)break;srv_sleep_ticks(1);}while(now()<deadline);
    if(waited!=child){srv_kill(child);srv_wait(child,&exit_status,0);failed=1;}
    if(exit_status)failed=1;
    char path[192];snprintf(path,sizeof(path),"%s/disk.receipt",directory);
    struct audio_io_receipt r={0};int fd=open(path,O_RDONLY);
    if(fd<0)failed=1;
    else {if(read(fd,&r,sizeof(r))!=sizeof(r))failed=1;close(fd);}
    /* Reject completed playback without sustained verified file work, delayed
     * work, and bursts that run outside the measured live PCM interval. */
    if(!started || !audio_io_receipt_valid(&r,seconds,start,end,status.submitted_bytes,status.underruns))failed=1;
    char result[640];
    int size=snprintf(result,sizeof(result),"audio-io: result=%s seconds=%u start_us=%llu end_us=%llu first_io_us=%llu last_io_us=%llu bursts=%llu bytes=%llu max_burst_us=%llu pcm_bytes=%llu underruns=%llu worker_status=%llu\n",
        failed?"FAIL":"PASS",seconds,(unsigned long long)start,(unsigned long long)end,
        (unsigned long long)r.first,(unsigned long long)r.last,(unsigned long long)r.bursts,
        (unsigned long long)r.bytes,(unsigned long long)r.max_us,(unsigned long long)status.submitted_bytes,
        (unsigned long long)status.underruns,(unsigned long long)exit_status);
    snprintf(path,sizeof(path),"%s/result.txt",directory);
    if(size<=0 || (size_t)size>=sizeof(result) || save(path,result,(size_t)size) || srv_sync()<0) {
        puts("audio-io: result=FAIL reason=receipt-write");return 1;
    }
    fputs(result,stdout);fflush(stdout);return failed?1:0;
}
int audio_io_stress_main(int argc,char **argv) {
    if(argc==4 && !strcmp(argv[1],"--io-worker")) {
        unsigned seconds=(unsigned)atoi(argv[3]);
        if(seconds<8 || seconds>1800)return 1;
        return disk_worker(argv[2],seconds);
    }
    if(argc==3 && !strcmp(argv[1],"--io-stress")) {
        unsigned seconds=(unsigned)atoi(argv[2]);if(seconds<8 || seconds>1800)return 1;
        return producer(seconds);
    }
    return -1;
}
