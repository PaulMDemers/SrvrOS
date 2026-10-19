/* Included by audio.c: the same worker owns tone and stream DMA. Syscalls
 * copy only into unconsumed ring space and never issue controller commands. */
/* More than the largest 16-bit FIFOS plus 100 ms of cached-position age.
 * A conservative fixed guard also covers controllers with deep prefetch FIFOs. */
#define PCM_GUARD 96000u
#define PCM_PREFILL 192000u
static struct srv_pcm_status stream;
static uint64_t stream_owner,stream_sequence,stream_end,stream_position,stream_poll_us,stream_progress_us;
static uint32_t stream_last_position;
static bool stream_close,stream_starved;
static void stream_cancel(bool fault) {
    uint64_t f=lock();stream.state=fault?SRV_PCM_FAULT:(stream.state==SRV_PCM_DONE || stream.state==SRV_PCM_FAULT)?stream.state:SRV_PCM_CANCELED;unlock(f);
    if(!stop()) { codec.available=false;stream.state=SRV_PCM_FAULT; }
    f=lock();stream.queued_bytes=0;stream_end=stream_position;
    if(stream_close) { stream_owner=0;stream_close=false; }
    unlock(f);
}
static bool stream_ready(void) {
    return stream_owner && (stream_close || stream.state==SRV_PCM_DRAINING ||
        stream.state==SRV_PCM_PLAYING || (stream.state==SRV_PCM_BUFFERING && stream_end>=PCM_PREFILL));
}
static void stream_service(void) {
    if(stream_close) { stream_cancel(false);return; }
    if(stream.state!=SRV_PCM_BUFFERING && stream.state!=SRV_PCM_PLAYING && stream.state!=SRV_PCM_DRAINING)return;
    if(!controller.playing) {
        if(!stream_end && stream.state==SRV_PCM_DRAINING) { stream.state=SRV_PCM_DONE;return; }
        if(stream.state==SRV_PCM_BUFFERING && stream_end<PCM_PREFILL)return;
        __asm__ volatile("mfence":::"memory");
        if(!hda_codec_mute(&codec,false) || !hda_core_start(&controller,bdl_phys,PCM_BYTES,2)) {
            stream_cancel(true);codec.available=false;return;
        }
        uint64_t f=lock();stream_poll_us=stream_progress_us=timer_monotonic_us();stream_last_position=0;
        if(stream.state==SRV_PCM_BUFFERING)stream.state=SRV_PCM_PLAYING;
        unlock(f);return;
    }
    uint32_t position;uint64_t now=timer_monotonic_us();
    if(!hda_core_position(&controller,&position) || position>PCM_BYTES ||
       now<stream_poll_us || now-stream_poll_us>=1000000) {
        if(controller.error==HDA_OK)controller.error=HDA_STREAM_TIMEOUT;
        stream_cancel(true);codec.available=false;return;
    }
    /* LPIB may report CBL itself at the boundary. Round partial frames down
     * so a consumed-region clear never overwrites a partly consumed frame. */
    position=(position%PCM_BYTES)&~3u;
    uint32_t delta=(position+PCM_BYTES-stream_last_position)%PCM_BYTES;
    if(delta>(now-stream_poll_us)*192000/1000000+65536) {
        controller.error=HDA_STREAM_ERROR;stream_cancel(true);codec.available=false;return;
    }
    uint64_t f=lock();
    /* Consumed bytes become silence before they can be played on the next lap.
     * The one-second service deadline disambiguates the four-second DMA ring. */
    for(uint32_t n=0;n<delta;n+=2)pcm[((stream_last_position+n)%PCM_BYTES)/2]=0;
    __asm__ volatile("mfence":::"memory");
    stream_position+=delta;stream.position_bytes=stream_position;
    stream_last_position=position;stream_poll_us=now;
    if(delta)stream_progress_us=now;
    bool stalled=now-stream_progress_us>=500000;
    bool drained=stream.state==SRV_PCM_DRAINING && stream_position>=stream_end;
    if(!drained && stream_position>=stream_end && !stream_starved) { stream.underruns++;stream_starved=true; }
    stream.queued_bytes=stream_end>stream_position?stream_end-stream_position:0;
    unlock(f);
    if(stalled) { controller.error=HDA_STREAM_TIMEOUT;stream_cancel(true);codec.available=false; }
    else if(drained) {
        if(!stop()) { stream.state=SRV_PCM_FAULT;codec.available=false; }
        else stream.state=SRV_PCM_DONE;
    }
}
int64_t audio_stream_request(uint64_t pid,unsigned op,uint64_t handle,void *data,uint64_t length) {
    if(!pid || op<SRV_PCM_OPEN || op>SRV_PCM_CLOSE)return -1;
    if(op==SRV_PCM_WRITE) { if(!data || !length || length>SRV_PCM_MAX_WRITE || (length&3))return -1; }
    else if(op==SRV_PCM_QUERY) { if(!data || length!=sizeof(struct srv_pcm_status))return -1; }
    else if(data || length)return -1;
    uint64_t f=lock();int64_t result=-1;
    if(op==SRV_PCM_OPEN) {
        if(handle || stream_owner || !worker_started || !controller.ready || !codec.available || controller.playing || pending_op || active_op)goto done;
        if(stream_sequence>=INT64_MAX)goto done;
        stream_owner=pid;stream=(struct srv_pcm_status){.version=SRV_PCM_VERSION,.state=SRV_PCM_BUFFERING,
            .rate=48000,.channels=2,.handle=++stream_sequence};
        stream_end=stream_position=0;stream_close=stream_starved=false;result=(int64_t)stream.handle;goto done;
    }
    if(stream_owner!=pid || stream.handle!=handle || stream_close)goto done;
    if(op==SRV_PCM_QUERY) { *(struct srv_pcm_status *)data=stream;result=0;goto done; }
    if(op==SRV_PCM_CLOSE) {
        if(stream.state==SRV_PCM_DONE && !controller.playing) { stream_owner=0;result=0;goto done; }
        stream_close=true;result=0;goto wake_worker;
    }
    if(op==SRV_PCM_FINISH) {
        if(stream.state!=SRV_PCM_BUFFERING && stream.state!=SRV_PCM_PLAYING)goto done;
        stream.state=SRV_PCM_DRAINING;result=0;goto wake_worker;
    }
    if(stream.state!=SRV_PCM_BUFFERING && stream.state!=SRV_PCM_PLAYING)goto done;
    if(stream.state==SRV_PCM_PLAYING) {
        uint64_t now=timer_monotonic_us();
        if(now<stream_poll_us || now-stream_poll_us>=100000) { result=SRV_PCM_AGAIN;goto wake_worker; }
    }
    uint64_t begin=stream_end;
    if(controller.playing && begin<stream_position+PCM_GUARD)begin=stream_position+PCM_GUARD;
    uint64_t limit=stream_position+PCM_BYTES-PCM_GUARD;
    if(begin>=limit) { result=SRV_PCM_AGAIN;goto wake_worker; }
    if(length>limit-begin)length=(limit-begin)&~3ull;
    if(stream.submitted_bytes>INT64_MAX-length)goto done;
    if(begin>stream_end && !stream_starved)stream.underruns++;
    const uint8_t *bytes=data;
    for(uint64_t n=0;n<length;n+=2) {
        int16_t sample=(int16_t)((unsigned)bytes[n]|((unsigned)bytes[n+1]<<8));
        pcm[((begin+n)%PCM_BYTES)/2]=(int16_t)((int)sample*(int)status.volume/100);
    }
    __asm__ volatile("mfence":::"memory");
    stream_end=begin+length;stream.submitted_bytes+=length;stream.queued_bytes=stream_end-stream_position;
    stream_starved=false;result=(int64_t)length;
wake_worker:
    scheduler_wake_all(&wake);
done:
    unlock(f);return result;
}
void audio_process_cleanup(uint64_t pid) {
    uint64_t f=lock();if(pid && stream_owner==pid) {
        if(stream.state==SRV_PCM_DONE && !controller.playing)stream_owner=0;
        else { stream_close=true;scheduler_wake_all(&wake); }
    }unlock(f);
}

void audio_stream_snapshot(struct srv_pcm_status *out,uint64_t *owner) {
    uint64_t f=lock();if(out)*out=stream;if(owner)*owner=stream_owner;unlock(f);
}
