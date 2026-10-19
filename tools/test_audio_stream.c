#define main audio_tone_tests_main
#include "test_audio_service.c"
#undef main
static void reset_stream(void) {
    setup();stream_owner=0;stream_close=false;stream_sequence=0;stream=(struct srv_pcm_status){0};
}
static long open_stream(unsigned pid) { return audio_stream_request(pid,SRV_PCM_OPEN,0,0,0); }
static void tick_stream(unsigned bytes,unsigned usec) {
    dma_position=(dma_position+bytes)%PCM_BYTES;if(!dma_position && bytes)dma_position=PCM_BYTES;clock_us+=usec;service();
}
int main(void) {
    pcm=malloc(PCM_BYTES);assert(pcm);reset_stream();
    unsigned char samples[SRV_PCM_MAX_WRITE];
    for(unsigned i=0;i<sizeof(samples);i+=4) {samples[i]=0xd2;samples[i+1]=4;samples[i+2]=0xd2;samples[i+3]=0xe9;}
    assert(open_stream(0)<0);long h=open_stream(12);assert(h>0 && open_stream(13)<0);
    struct srv_pcm_status out;
    assert(audio_stream_request(13,SRV_PCM_QUERY,h,&out,sizeof(out))<0);
    assert(audio_stream_request(12,SRV_PCM_QUERY,h+1,&out,sizeof(out))<0);
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,3)<0);
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,sizeof(samples)+4)<0);
    assert(audio_request(SRV_AUDIO_TEST,0)<0);
    assert(audio_stream_request(12,SRV_PCM_FINISH,h,0,0)==0);service();assert(stream.state==SRV_PCM_DONE);
    assert(audio_stream_request(12,SRV_PCM_CLOSE,h,0,0)==0);service();assert(!stream_owner);silent();

    reset_stream();h=open_stream(12);
    uint64_t total=192000u*12u+789u*4u,written=0;
    bool sealed=false;
    for(unsigned tick=0;tick<1400 && stream.state!=SRV_PCM_DONE;tick++) {
        while(written<total) {
            unsigned n=total-written>sizeof(samples)?sizeof(samples):(unsigned)(total-written);
            long accepted=audio_stream_request(12,SRV_PCM_WRITE,h,samples,n);
            if(accepted==SRV_PCM_AGAIN)break;
            assert(accepted>0 && !(accepted&3));written+=(unsigned)accepted;
        }
        if(written==total && !sealed) { assert(!audio_stream_request(12,SRV_PCM_FINISH,h,0,0));sealed=true; }
        if(!controller.playing)service();
        assert(controller.playing);
        for(unsigned i=0;i<1920;i+=4) {
            unsigned at=(dma_position+i)%PCM_BYTES;
            if(stream_position+i<total) {assert(pcm[at/2]==308);assert(pcm[at/2+1]==-1419);}
            else {assert(!pcm[at/2]);assert(!pcm[at/2+1]);}
        }
        tick_stream(1920,10000);
        assert(stream.state!=SRV_PCM_FAULT);
    }
    assert(stream.state==SRV_PCM_DONE && written==total && !stream.underruns && !controller.playing);
    assert(stream.submitted_bytes==total);silent();
    audio_process_cleanup(13);assert(!stream_close);audio_process_cleanup(12);assert(!stream_owner);service();assert(!stream_owner);

    reset_stream();h=open_stream(12);
    for(unsigned i=0;i<12;i++)assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,sizeof(samples))>0);
    service();assert(controller.playing);tick_stream(76800,400000);tick_stream(76800,400000);tick_stream(57600,300000);assert(stream.underruns==1);
    tick_stream(1920,10000);assert(stream.underruns==1);silent();
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,sizeof(samples))>0);
    for(unsigned i=0;i<PCM_GUARD/2;i++)assert(!pcm[(dma_position/2+i)%(PCM_BYTES/2)]);
    assert(!audio_request(SRV_AUDIO_STOP,0));service();assert(stream.state==SRV_PCM_CANCELED && !stream_owner);silent();
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,4)<0);
    /* Stop releases ownership even before a stalled application acknowledges.
     * A replacement may open immediately; the old token cannot cancel it. */
    long replacement=open_stream(13);assert(replacement>h);
    assert(audio_stream_request(12,SRV_PCM_CLOSE,h,0,0)<0);
    audio_process_cleanup(12);assert(stream_owner==13);
    audio_process_cleanup(13);service();assert(!stream_owner);

    reset_stream();h=open_stream(12);assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,4)==4);
    audio_process_cleanup(12);service();assert(!starts && !stream_owner);silent();
    for(unsigned fault=0;fault<3;fault++) {
        reset_stream();h=open_stream(12);
        for(unsigned i=0;i<12;i++)assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,sizeof(samples))>0);
        if(fault==0)start_ok=false;
        service();
        if(fault==1)tick_stream(0,500001);
        if(fault==2)tick_stream(1920,1000000);
        assert(stream.state==SRV_PCM_FAULT && !codec.available);silent();
        audio_process_cleanup(12);service();assert(!stream_owner);
    }
    /* Stale handles cannot revive a lease, including after the owner PID is reused. */
    reset_stream();h=open_stream(12);audio_process_cleanup(12);service();
    long next=open_stream(12);assert(next>h);
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,4)<0);
    assert(audio_stream_request(12,SRV_PCM_CLOSE,h,0,0)<0);
    assert(stream_owner==12 && stream.handle==(uint64_t)next);
    audio_process_cleanup(12);service();
    stream_sequence=INT64_MAX;assert(open_stream(12)<0 && !stream_owner);

    /* All ambiguous hardware outcomes must mute, retain the allocation and
     * refuse a new owner, even if hardware still claims DMA is running. */
    for(unsigned fault=0;fault<5;fault++) {
        reset_stream();h=open_stream(12);
        for(unsigned i=0;i<12;i++)assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,sizeof(samples))>0);
        service();assert(controller.playing);
        if(fault==0) { dma_position=PCM_BYTES+4;clock_us+=10000; }
        if(fault==1) { dma_position=192000;clock_us+=10000; }
        if(fault==2)clock_us--;
        if(fault==3)position_ok=false;
        if(fault==4) { stop_ok=false;assert(!audio_stream_request(12,SRV_PCM_CLOSE,h,0,0)); }
        int16_t *retained=pcm;service();
        assert(stream.state==SRV_PCM_FAULT && !codec.available && pcm==retained);silent();
        audio_process_cleanup(12);service();assert(!stream_owner && open_stream(13)<0);
    }

    /* An aged cached cursor cannot authorize a ring write; the worker must
     * refresh it first. FINISH seals further writes, including during drain. */
    reset_stream();h=open_stream(12);
    for(unsigned i=0;i<12;i++)assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,sizeof(samples))>0);
    service();clock_us+=100000;
    uint64_t submitted=stream.submitted_bytes;
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,4)==SRV_PCM_AGAIN);
    assert(stream.submitted_bytes==submitted);
    dma_position=19200;service();assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,4)==4);
    assert(!audio_stream_request(12,SRV_PCM_FINISH,h,0,0));
    assert(audio_stream_request(12,SRV_PCM_FINISH,h,0,0)<0);
    assert(audio_stream_request(12,SRV_PCM_WRITE,h,samples,4)<0);
    assert(!audio_request(SRV_AUDIO_STOP,0));service();assert(stream.state==SRV_PCM_CANCELED && !stream_owner);silent();
    audio_process_cleanup(12);service();assert(!stream_owner);
    free(pcm);puts("audio-stream: PASS ownership, bounded writes, 12-second multi-wrap stereo continuity, drain/tail, underrun silence, Stop/exit cancellation and DMA deadlines");
}
