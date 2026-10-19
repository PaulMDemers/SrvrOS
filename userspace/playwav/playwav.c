#include "wav.h"
#include <srvros/audio.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static unsigned char buffer[SRV_PCM_MAX_WRITE];
static int pause_tick(void) { const struct timespec delay={0,10000000};return nanosleep(&delay,0); }
int main(int argc,char **argv) {
    if(argc==2 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h"))) {
        puts("usage: playwav FILE.wav\nPlays 48000 Hz, 16-bit stereo PCM WAV. One player at a time.\nSet volume/output in Settings before playback; Stop cancels it.");return 0;
    }
    if(argc!=2) { fputs("usage: playwav FILE.wav\n",stderr);return 1; }
    FILE *f=fopen(argv[1],"rb");struct wav_info info;
    if(!f) { perror("playwav");return 1; }
    if(wav_open_data(f,&info)) { fputs("playwav: expected a complete 48000 Hz, 16-bit stereo PCM WAV\n",stderr);fclose(f);return 1; }
    long handle=srv_pcm_open();if(handle<0) { fputs("playwav: sound output is unavailable or already in use\n",stderr);fclose(f);return 1; }
    printf("playwav: playing %u bytes at 48000 Hz, stereo\n",info.bytes);fflush(stdout);
    uint32_t remaining=info.bytes;int failed=0;unsigned blocked=0;
    while(remaining && !failed) {
        size_t bytes=remaining<sizeof(buffer)?remaining:sizeof(buffer);
        if(fread(buffer,1,bytes,f)!=bytes) { failed=1;break; }
        size_t sent=0;
        while(sent<bytes) {
            long n=srv_pcm_write((uint64_t)handle,buffer+sent,bytes-sent);
            if(n==SRV_PCM_AGAIN) { if(++blocked>500 || pause_tick()) { failed=1;break; }continue; }
            if(n<=0 || (size_t)n>bytes-sent || (n&3)) { failed=1;break; }
            blocked=0;sent+=(size_t)n;
        }
        remaining-=(uint32_t)sent;
    }
    fclose(f);
    struct srv_pcm_status status={0};
    if(!failed && srv_pcm_finish((uint64_t)handle)<0)failed=1;
    for(unsigned waits=0;!failed;waits++) {
        if(srv_pcm_query((uint64_t)handle,&status)<0) { failed=1;break; }
        if(status.state==SRV_PCM_DONE)break;
        if(status.state!=SRV_PCM_DRAINING || waits>600 || pause_tick()) { failed=1;break; }
    }
    srv_pcm_close((uint64_t)handle);
    if(failed) { fputs("playwav: playback stopped or failed\n",stderr);return 1; }
    printf("playwav: complete bytes=%llu underruns=%llu\n",(unsigned long long)status.submitted_bytes,(unsigned long long)status.underruns);
    return 0;
}
