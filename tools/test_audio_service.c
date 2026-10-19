#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_AUDIO_HOST_TEST
#include "../kernel/src/drivers/audio.c"
static uint64_t clock_us;
static uint32_t dma_position;
static bool stop_ok=true,start_ok=true,position_ok=true,jack_ok=true;
static unsigned starts,stops,wakes;
uint64_t timer_monotonic_us(void) { return clock_us; }
uint64_t timer_ticks(void) { return clock_us/10000; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q==&wake);wakes++; }
bool hda_core_stop(struct hda_core *h) { stops++;if(stop_ok)h->playing=false;else {h->ready=false;h->error=HDA_STREAM_TIMEOUT;}return stop_ok; }
bool hda_core_start(struct hda_core *h,uint64_t bdl,uint32_t bytes,unsigned entries) {
    assert(bdl==4096 && bytes==PCM_BYTES && entries==2);starts++;
    if(start_ok)h->playing=true;else {h->ready=false;h->error=HDA_STREAM_TIMEOUT;}return start_ok;
}
bool hda_core_position(struct hda_core *h,uint32_t *value) { *value=dma_position;h->last_position=*value;if(!position_ok){h->ready=false;h->error=HDA_STREAM_ERROR;}return position_ok; }
bool hda_codec_mute(struct hda_codec *c,bool mute) { c->muted=mute;return true; }
bool hda_codec_route(struct hda_codec *c,enum hda_output out) { assert(!controller.playing);c->output=out;return true; }
bool hda_codec_headphones(struct hda_codec *c,bool *present) { assert(c->apple);*present=true;return jack_ok; }
static void setup(void) {
    controller=(struct hda_core){.ready=true};codec=(struct hda_codec){.controller=&controller,.available=true,.muted=true};
    status=(struct srv_audio_status){.version=1,.state=SRV_AUDIO_READY,.volume=25};
    pending_op=active_op=0;pending_value=0;worker_started=true;bdl_phys=4096;
    stop_ok=start_ok=position_ok=jack_ok=true;clock_us=1000000;dma_position=0;
    starts=stops=wakes=0;silence();
}
static void silent(void) { for(unsigned i=0;i<PCM_BYTES/2;i++)assert(!pcm[i]); }
int main(void) {
    pcm=malloc(PCM_BYTES);assert(pcm);setup();
    assert(!audio_request(SRV_AUDIO_TEST,0) && pending_op==SRV_AUDIO_TEST && !starts);
    assert(audio_request(SRV_AUDIO_VOLUME,50)==-1); /* Queued work is not overwritten. */
    assert(!audio_request(SRV_AUDIO_STOP,0));service();assert(!starts && !controller.playing);silent();
    assert(audio_request(99,0)==-1 && audio_request(SRV_AUDIO_VOLUME,101)==-1);
    assert(audio_request(SRV_AUDIO_TEST,1)==-1 && audio_request(SRV_AUDIO_OUTPUT,1)==-1);
    assert(!audio_request(SRV_AUDIO_TEST,0));service();assert(starts==1 && controller.playing && !codec.muted);
    int peak=0;for(unsigned i=0;i<TONE_FRAMES;i++) { assert(pcm[i*2]==pcm[i*2+1]);int v=abs(pcm[i*2]);if(v>peak)peak=v; }
    assert(peak==1200);for(unsigned i=TONE_FRAMES*2;i<PCM_BYTES/2;i++)assert(!pcm[i]);
    clock_us+=750000;dma_position=TONE_FRAMES*4+4096;service();
    assert(status.tests_started==1 && status.tests_completed==1 && status.state==SRV_AUDIO_READY);silent();
    assert(!audio_request(SRV_AUDIO_TEST,0));service();
    assert(!audio_request(SRV_AUDIO_VOLUME,0));service();assert(!controller.playing && status.volume==0);silent();
    assert(!audio_request(SRV_AUDIO_TEST,0));service();assert(controller.playing);silent();
    assert(!audio_request(SRV_AUDIO_STOP,0));service();assert(codec.muted);silent();
    setup();assert(!audio_request(SRV_AUDIO_TEST,0));service();clock_us+=750000;dma_position=0;service();
    assert(status.state==SRV_AUDIO_FAULT && status.error==HDA_STREAM_TIMEOUT && !status.tests_completed);silent();
    assert(audio_request(SRV_AUDIO_TEST,0)==-1);
    setup();start_ok=false;assert(!audio_request(SRV_AUDIO_TEST,0));service();assert(status.state==SRV_AUDIO_FAULT);silent();
    setup();assert(!audio_request(SRV_AUDIO_TEST,0));service();stop_ok=false;
    assert(!audio_request(SRV_AUDIO_STOP,0));service();assert(status.state==SRV_AUDIO_FAULT);silent();
    setup();codec.apple=true;assert(!audio_request(SRV_AUDIO_OUTPUT,1));service();
    assert(status.output==1 && status.headphone_present && (status.capabilities&SRV_AUDIO_CAP_JACK_SENSE));
    jack_ok=false;clock_us+=500000;service();assert(status.state==SRV_AUDIO_FAULT && !(status.capabilities&SRV_AUDIO_CAP_JACK_SENSE));silent();
    for(unsigned i=0;i<1000;i++) { setup();assert(!audio_request(SRV_AUDIO_TEST,0));service();assert(!audio_request(SRV_AUDIO_STOP,0));service();silent(); }
    free(pcm);puts("audio-service: PASS queued ownership, Stop priority, ramped stereo/tail, zero gain, DMA progress deadline, failed start/stop/jack and 1000 reuse cycles");
    return 0;
}
