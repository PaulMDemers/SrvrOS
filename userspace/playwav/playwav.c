#include "wav.h"
#include "convert.h"
#include <srvros/audio.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static int16_t buffer[SRV_PCM_MAX_WRITE/2];
static struct source { FILE *file;unsigned channels;uint32_t remaining;size_t at,size;unsigned char bytes[16384]; } source;
static int read_frame(void *context,int16_t samples[2]) {
    struct source *s=context;unsigned frame=s->channels*2;
    if(s->at==s->size) {
        size_t n=s->remaining<sizeof(s->bytes)?s->remaining:sizeof(s->bytes);
        if(!n || fread(s->bytes,1,n,s->file)!=n)return -1;
        s->size=n;s->at=0;s->remaining-=(uint32_t)n;
    }
    if(s->size-s->at<frame)return -1;
    const unsigned char *p=s->bytes+s->at;s->at+=frame;
    samples[0]=(int16_t)((unsigned)p[0]|((unsigned)p[1]<<8));
    samples[1]=s->channels==1?samples[0]:(int16_t)((unsigned)p[2]|((unsigned)p[3]<<8));return 0;
}
static int pause_tick(void) { const struct timespec delay={0,10000000};return nanosleep(&delay,0); }
int main(int argc,char **argv) {
    if(argc==2 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h"))) {
        puts("usage: playwav FILE.wav\nPlays 44100/48000 Hz, 16-bit mono/stereo PCM WAV. One player at a time.\nSet volume/output in Settings before playback; Stop cancels it.");return 0;
    }
    if(argc!=2) { fputs("usage: playwav FILE.wav\n",stderr);return 1; }
    FILE *f=fopen(argv[1],"rb");struct wav_info info;
    if(!f) { perror("playwav");return 1; }
    if(wav_open_data(f,&info)) { fputs("playwav: expected a complete 44100/48000 Hz, 16-bit mono/stereo PCM WAV\n",stderr);fclose(f);return 1; }
    source=(struct source){.file=f,.channels=info.channels,.remaining=info.bytes};
    struct pcm_convert converter;
    if(pcm_convert_init(&converter,info.rate,info.bytes/(info.channels*2),read_frame,&source)) {
        fputs("playwav: incomplete PCM data\n",stderr);fclose(f);return 1;
    }
    long handle=srv_pcm_open();if(handle<0) { fputs("playwav: sound output is unavailable or already in use\n",stderr);fclose(f);return 1; }
    printf("playwav: playing %u bytes at %u Hz, %u channels\n",info.bytes,info.rate,info.channels);fflush(stdout);
    int failed=0;unsigned blocked=0;
    while(!failed) {
        long frames=pcm_convert_read(&converter,buffer,sizeof(buffer)/4);
        if(frames<0) { failed=1;break; }
        if(!frames)break;
        size_t bytes=(size_t)frames*4;
        size_t sent=0;
        while(sent<bytes) {
            long n=srv_pcm_write((uint64_t)handle,(unsigned char *)buffer+sent,bytes-sent);
            if(n==SRV_PCM_AGAIN) { if(++blocked>500 || pause_tick()) { failed=1;break; }continue; }
            if(n<=0 || (size_t)n>bytes-sent || (n&3)) { failed=1;break; }
            blocked=0;sent+=(size_t)n;
        }
    }
    struct srv_pcm_status status={0};
    if(!failed && srv_pcm_finish((uint64_t)handle)<0)failed=1;
    for(unsigned waits=0;!failed;waits++) {
        if(srv_pcm_query((uint64_t)handle,&status)<0) { failed=1;break; }
        if(status.state==SRV_PCM_DONE)break;
        if(status.state!=SRV_PCM_DRAINING || waits>600 || pause_tick()) { failed=1;break; }
    }
    srv_pcm_close((uint64_t)handle);
    fclose(f);
    if(failed) { fputs("playwav: playback stopped or failed\n",stderr);return 1; }
    printf("playwav: complete bytes=%llu underruns=%llu\n",(unsigned long long)status.submitted_bytes,(unsigned long long)status.underruns);
    return 0;
}
