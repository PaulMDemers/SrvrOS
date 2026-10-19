#include <srvros/audio.h>
#include <srvros/audio_abi.h>
#include <srvros/audio_stream_abi.h>
#include <srvros/hda_codec.h>
#include <srvros/console.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <stddef.h>

#define AUDIO_MMIO 0xffffc001f5000000ull
#define PCM_BYTES (192000u*4u)
#define PCM_PAGES ((PCM_BYTES+4095)/4096)
#define TONE_FRAMES 24000u
struct bdl_entry { uint64_t address; uint32_t length, flags; };
static struct hda_core controller;
static struct hda_codec codec;
static struct srv_audio_status status={.version=SRV_AUDIO_VERSION,.volume=25};
static struct scheduler_wait_queue wake;
static bool initialized, worker_started;
static uint64_t pcm_phys, bdl_phys, started_us, next_jack_tick;
static bool jack_known, jack_present;
static int16_t *pcm;
static unsigned pending_op, pending_value,active_op;
static uint64_t lock(void) {
#ifdef SRVROS_AUDIO_HOST_TEST
    return 0;
#else
    uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f) {
#ifdef SRVROS_AUDIO_HOST_TEST
    (void)f;
#else
    if(f&(1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
}
static uint32_t read_reg(void *ctx,unsigned offset,unsigned bytes) {
    (void)ctx;volatile void *p=(volatile void *)(AUDIO_MMIO+offset);
    if(bytes==1) return *(volatile uint8_t *)p;
    if(bytes==2) return *(volatile uint16_t *)p;
    return *(volatile uint32_t *)p;
}
static void write_reg(void *ctx,unsigned offset,unsigned bytes,uint32_t v) {
    (void)ctx;volatile void *p=(volatile void *)(AUDIO_MMIO+offset);
    if(bytes==1) *(volatile uint8_t *)p=v;
    else if(bytes==2) *(volatile uint16_t *)p=v;
    else *(volatile uint32_t *)p=v;
}
static uint64_t now(void *ctx) { (void)ctx;return timer_monotonic_us(); }
static void relax(void *ctx) {
    (void)ctx;static unsigned polls;
    if((++polls&1023)==0) scheduler_yield();
    else __asm__ volatile("pause":::"memory");
}
static void publish(void) {
    uint64_t f=lock();
    status.codec_vendor=codec.vendor;status.codec_subsystem=codec.subsystem;
    status.error=controller.error;status.commands=controller.commands;
    status.last_position=controller.last_position;status.output=codec.output;
    status.capabilities=(codec.apple?SRV_AUDIO_CAP_HEADPHONES:0)|(jack_known?SRV_AUDIO_CAP_JACK_SENSE:0);
    status.headphone_present=jack_present;
    status.pending=pending_op?pending_op:active_op;
    if(controller.error==HDA_UNSUPPORTED_CODEC) status.state=SRV_AUDIO_UNAVAILABLE;
    else if(!controller.ready || !codec.available) status.state=SRV_AUDIO_FAULT;
    else status.state=controller.playing?SRV_AUDIO_PLAYING:SRV_AUDIO_READY;
    unlock(f);
}
static void silence(void) { for(unsigned i=0;i<PCM_BYTES/2;i++) pcm[i]=0; __asm__ volatile("mfence":::"memory"); }
static bool stop(void) {
    /* Never release DMA memory, including after an uncertain stop. Silence the
     * retained buffer so a failed controller cannot loop the audible test. */
    silence();bool stopped=hda_core_stop(&controller);
    bool muted=!controller.ready || hda_codec_mute(&codec,true);
    return stopped && muted;
}
#include "audio_stream.inc"
static bool test_tone(void) {
    if(!stop() || !controller.ready) return false;
    silence();
    unsigned volume=status.volume;
    /* 500 Hz triangle, half a second, 10 ms attack/release, stereo. Integer
     * synthesis avoids touching task FPU state in the service worker. */
    for(unsigned frame=0;frame<TONE_FRAMES;frame++) {
        unsigned phase=frame%96;
        int sample=phase<48?(int)phase-24:72-(int)phase;
        int envelope=frame<480?(int)frame:(frame>TONE_FRAMES-480?(int)(TONE_FRAMES-frame):480);
        int16_t value=(int16_t)(sample*200*(int)volume/100*envelope/480);
        pcm[frame*2]=value;pcm[frame*2+1]=value;
    }
    __asm__ volatile("mfence":::"memory");
    if(!hda_codec_mute(&codec,false) || !hda_core_start(&controller,bdl_phys,PCM_BYTES,2)) { silence();return false; }
    started_us=timer_monotonic_us();status.tests_started++;return true;
}
static bool jack_due(void) { return codec.apple && codec.available && timer_ticks()>=next_jack_tick; }
static bool eligible(void *arg) { (void)arg;return pending_op || stream_ready() || (controller.playing && codec.available) || jack_due(); }
static bool requested(void *arg) { (void)arg;return pending_op!=0; }
static void service(void) {
    uint64_t f=lock();unsigned op=pending_op,value=pending_value;pending_op=0;active_op=op;unlock(f);
    bool ok=true;
    if(op==SRV_AUDIO_STOP) {
        if(stream_owner) {
            /* Revoke the lease immediately after silence/stop. The old owner
             * may be blocked in unrelated file I/O and cannot acknowledge yet.
             * Its generation handle never authorizes a replacement stream. */
            f=lock();stream_close=true;unlock(f);stream_cancel(false);
            ok=codec.available;
        } else ok=stop();
    }
    else if(op==SRV_AUDIO_TEST) ok=test_tone();
    else if(op==SRV_AUDIO_VOLUME) {
        /* Volume changes take effect immediately by stopping a current
         * tone. PCM gain is fixed while its exclusive lease is held. */
        if(controller.playing) ok=stop();
        status.volume=value;
    } else if(op==SRV_AUDIO_OUTPUT) {
        ok=stop() && hda_codec_route(&codec,(enum hda_output)value);
    }
    if(ok && stream_owner) { stream_service();ok=codec.available; }
    else if(ok && controller.playing) {
        uint32_t position;ok=hda_core_position(&controller,&position);
        uint64_t elapsed=timer_monotonic_us()-started_us;
        if(ok && elapsed>=750000) {
            bool progressed=position>=TONE_FRAMES*4 && position<PCM_BYTES;
            ok=stop() && progressed;
            if(ok) status.tests_completed++;
            else if(controller.error==HDA_OK) controller.error=HDA_STREAM_TIMEOUT;
        }
    }
    if(ok && jack_due()) {
        ok=hda_codec_headphones(&codec,&jack_present);jack_known=ok;next_jack_tick=timer_ticks()+50;
    }
    if(!ok) { (void)stop();codec.available=false;jack_known=false; }
    f=lock();active_op=0;publish();unlock(f);
}
static void worker(void *arg) {
    (void)arg;
    for(;;) {
        if(!eligible(NULL)) {
            if(codec.apple && codec.available) scheduler_wait_timeout(&wake,eligible,NULL,next_jack_tick);
            else scheduler_wait(&wake,eligible,NULL);
            continue;
        }
        service();
        if(controller.playing && codec.available) scheduler_wait_timeout(&wake,requested,NULL,timer_ticks()+1);
        else scheduler_yield();
    }
}
void audio_init(void) {
    if(initialized) return;
    initialized=true;status.state=SRV_AUDIO_UNAVAILABLE;
    const struct pci_device *device=NULL;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *d=pci_device_at(i);
        if(d->class_code==4 && d->subclass==3 && d->vendor_id==0x8086 &&
           (d->device_id==0x9ca0||d->device_id==0x2668||d->device_id==0x293e)) { device=d;break; }
    }
    if(!device || !timer_reference_ready()) { audio_print_status();return; }
    if(!pci_power_d0(device)) { status.error=HDA_POWER;audio_print_status();return; }
    uint32_t bar=device->bar[0];
    if((bar&1) || (bar&6)==2 || (bar&6)==6) { status.error=HDA_BAD_BAR;audio_print_status();return; }
    uint64_t physical=bar&~15u;
    if((bar&6)==4) physical|=(uint64_t)device->bar[1]<<32;
    if(!physical || (physical&4095) || physical>0x000fffffffffc000ull) { status.error=HDA_BAD_BAR;audio_print_status();return; }
    for(unsigned i=0;i<4;i++) if(!vmm_map_page(AUDIO_MMIO+i*4096,physical+i*4096,
        VMM_PAGE_WRITABLE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_NO_EXECUTE)) {
            status.error=HDA_MAPPING;audio_print_status();return;
        }
    uint16_t command=pci_read_config16(device,4);
    pci_write_config16(device,4,command|6|0x400); /* DMA, MMIO; polling, no INTx. */
    if(device->device_id==0x9ca0) {
        /* Intel PCH: class zero DMA and coherent (snooped) sample accesses.
         * Touch only the documented fields, with native-width PCI writes. */
        uint8_t traffic=pci_read_config8(device,0x44);
        uint16_t control=pci_read_config16(device,0x78);
        if(traffic&7) pci_write_config8(device,0x44,traffic&~7u);
        if(control&0x800) pci_write_config16(device,0x78,control&~0x800u);
        if((pci_read_config8(device,0x44)&7)||(pci_read_config16(device,0x78)&0x800)) {
            status.error=HDA_BAD_ARGUMENT;audio_print_status();return;
        }
    }
    console_printf("audio: controller=%x bar=%x\n",(uint64_t)device->device_id,physical);
    bool ok=hda_core_reset(&controller,(struct hda_io){NULL,read_reg,write_reg,now,relax}) &&
        hda_codec_discover(&codec,&controller,device->device_id==0x9ca0);
    if(!ok) { if(controller.error==HDA_OK) controller.error=HDA_UNSUPPORTED_CODEC;publish();audio_print_status();return; }
    bdl_phys=pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    pcm_phys=pmm_alloc_frames_tagged(PCM_PAGES,PMM_FRAME_DMA);
    if(!bdl_phys||!pcm_phys||(!controller.dma64 && (bdl_phys>0xfffff000ull||pcm_phys+PCM_PAGES*4096ull>0x100000000ull))) {
        if(bdl_phys) pmm_free_frame(bdl_phys);
        if(pcm_phys) pmm_free_frame_range(pcm_phys,PCM_PAGES);
        controller.error=HDA_DMA_MEMORY;codec.available=false;publish();audio_print_status();return;
    }
    pcm=pmm_phys_to_virt(pcm_phys);silence();
    struct bdl_entry *bdl=pmm_phys_to_virt(bdl_phys);
    bdl[0]=(struct bdl_entry){pcm_phys,PCM_BYTES/2,0};
    bdl[1]=(struct bdl_entry){pcm_phys+PCM_BYTES/2,PCM_BYTES/2,0};
    worker_started=scheduler_spawn_io_service("audio",worker,NULL,eligible);
    if(!worker_started) { controller.error=HDA_WORKER;codec.available=false; }
    publish();audio_print_status();
}
void audio_status(struct srv_audio_status *out) { if(out) { uint64_t f=lock();*out=status;if(stream_owner)out->capabilities|=SRV_AUDIO_CAP_PCM_BUSY;out->pending=pending_op?pending_op:active_op;unlock(f); } }
int audio_request(unsigned op,unsigned value) {
    if(op<SRV_AUDIO_TEST||op>SRV_AUDIO_OUTPUT||
       (op==SRV_AUDIO_VOLUME&&value>100)||(op==SRV_AUDIO_OUTPUT&&value>1)||
       ((op==SRV_AUDIO_TEST||op==SRV_AUDIO_STOP)&&value)) return -1;
    uint64_t f=lock();
    if((stream_owner && op!=SRV_AUDIO_STOP) || !worker_started || !codec.available || !controller.ready ||
       (op==SRV_AUDIO_OUTPUT && value==1&&!codec.apple) || (pending_op&&op!=SRV_AUDIO_STOP)) { unlock(f);return -1; }
    pending_op=op;pending_value=value;scheduler_wake_all(&wake);unlock(f);return 0;
}
void audio_print_status(void) {
    struct srv_audio_status s;audio_status(&s);
    console_printf("audio: state=%u codec=%x subsystem=%x output=%s volume=%u pending=%u error=%s\n",
        (uint64_t)s.state,(uint64_t)s.codec_vendor,(uint64_t)s.codec_subsystem,
        s.output?"headphones":"speakers",(uint64_t)s.volume,(uint64_t)s.pending,hda_error_name(s.error));
    console_printf("audio: codec_mask=%x inputs=%u outputs=%u stream=%u dma64=%u command=%x response=%x\n",
        (uint64_t)controller.codecs,(uint64_t)controller.inputs,(uint64_t)controller.outputs,(uint64_t)controller.stream,
        (uint64_t)controller.dma64,(uint64_t)controller.last_command,(uint64_t)controller.last_response);
    if(s.capabilities&SRV_AUDIO_CAP_JACK_SENSE) console_printf("audio: headphones=%s (manual output selection)\n",s.headphone_present?"connected":"disconnected");
    if(stream.handle) console_printf("audio-pcm: state=%u owner=%u handle=%u submitted=%u position=%u queued=%u underruns=%u\n",
        (uint64_t)stream.state,stream_owner,stream.handle,stream.submitted_bytes,stream.position_bytes,stream.queued_bytes,stream.underruns);
    console_printf("audio: commands=%u tests_started=%u tests_completed=%u position=%u; physical sound NOT_TESTED\n",
        s.commands,s.tests_started,s.tests_completed,(uint64_t)s.last_position);
}
