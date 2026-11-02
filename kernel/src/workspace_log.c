/* Ordinary scheduled worker: filesystem work uses the storage service gate. */
#include <srvros/workspace_log.h>
#include <srvros/workspace.h>
#include <srvros/bootlog.h>
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/audio.h>
#include <srvros/audio_abi.h>
#include <srvros/audio_stream_abi.h>
#include <srvros/thermal.h>
#include <srvros/power.h>
#include <srvros/usb_storage.h>
#include <srvros/xhci.h>
#include <srvros/smc.h>
#include <stddef.h>
#include <stdint.h>
static char directory[128],path[160],snapshot[BOOTLOG_EXTENDED_CAPTURE_CAPACITY];
static uint64_t snapshot_limit=BOOTLOG_DEFAULT_CAPACITY;
static bool enabled,started,busy,healthy;
static unsigned checkpoint;
static bool retain_checkpoints,wifi_capture_parked;
static uint64_t due;
static struct scheduler_wait_queue wake;
static uint64_t lock(void) {
#ifdef SRVROS_WORKSPACE_LOG_HOST_TEST
    return 0;
#else
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");return flags;
#endif
}
static void unlock(uint64_t flags) {
#ifdef SRVROS_WORKSPACE_LOG_HOST_TEST
    (void)flags;
#else
    if(flags&(1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
}
static unsigned copy(char *out,const char *text) { unsigned n=0;while(text[n]) { out[n]=text[n];n++; }out[n]=0;return n; }
static void hex(char *out,uint64_t v,unsigned digits) { for(unsigned i=0;i<digits;i++) out[i]="0123456789abcdef"[(v>>(4*(digits-1-i)))&15];out[digits]=0; }
static bool directory_exists(const char *p) { const struct vfs_node *n=vfs_lookup(p);return n && n->type==VFS_NODE_DIRECTORY; }
static bool begin(uint64_t *flags) {
    *flags=lock();
    if(!scheduler_io_begin()) { unlock(*flags);return false; }
    return true;
}
static void end(uint64_t flags) { scheduler_io_end();unlock(flags); }
/* Cached queries only: logging must not issue codec commands or sensor I/O. */
static void device_checkpoint(void) {
    scheduler_print_io_owners();
    uint64_t sampled=timer_monotonic_us();
    struct scheduler_stats cpu;scheduler_snapshot(&cpu);
    console_printf("workspace-scheduler: sampled_ticks=%u idle_ticks=%u switches=%u preemptions=%u monotonic_us=%u\n",
        cpu.sampled_ticks,cpu.idle_ticks,cpu.switches,cpu.preemptions,sampled);
    struct srv_audio_status audio;struct srv_thermal_status thermal;
    struct srv_smc_status fan;smc_status(&fan);
    console_printf("workspace-devices: smc_state=%u flags=%x fan_millirpm=%u sample_us=%u samples=%u failures=%u error=%u monotonic_us=%u\n",
        (uint64_t)fan.state,(uint64_t)fan.flags,(uint64_t)fan.fan_millirpm,fan.sampled_us,fan.samples,fan.failures,(uint64_t)fan.error,sampled);
    struct srv_pcm_status pcm;uint64_t owner;audio_stream_snapshot(&pcm,&owner);
    console_printf("workspace-devices: pcm_state=%u owner=%u handle=%u submitted=%u position=%u queued=%u underruns=%u monotonic_us=%u\n",
        (uint64_t)pcm.state,owner,pcm.handle,pcm.submitted_bytes,pcm.position_bytes,pcm.queued_bytes,pcm.underruns,sampled);
    audio_status(&audio);thermal_status(&thermal);
    console_printf("workspace-devices: audio_state=%u error=%u output=%u volume=%u pending=%u started=%u completed=%u headphones=%u caps=%x monotonic_us=%u\n",
        (uint64_t)audio.state,(uint64_t)audio.error,(uint64_t)audio.output,(uint64_t)audio.volume,
        (uint64_t)audio.pending,audio.tests_started,audio.tests_completed,(uint64_t)audio.headphone_present,(uint64_t)audio.capabilities,sampled);
    console_printf("workspace-devices: thermal_state=%u flags=%x core_mc=%d package_mc=%d sample_us=%u samples=%u monotonic_us=%u\n",
        (uint64_t)thermal.state,(uint64_t)thermal.flags,(int64_t)thermal.core_mc,(int64_t)thermal.package_mc,thermal.sampled_us,thermal.samples,sampled);
    struct power_snapshot power;power_status(&power);
    if(power.state!=POWER_DISABLED) {
        scheduler_print_activity();
        console_printf("workspace-devices: power_state=%u flags=%u energy_shift=%u package_mw=%u active_mhz=%u nominal_mhz=%u package_uj=%u sample_us=%u interval_us=%u samples=%u rejected=%u energy=%u aperf=%u mperf=%u monotonic_us=%u\n",
            (uint64_t)power.state,(uint64_t)power.flags,(uint64_t)power.energy_shift,power.package_mw,power.active_mhz,(uint64_t)power.nominal_mhz,
            power.package_uj,power.sampled_us,power.interval_us,power.samples,power.rejected,power.raw.energy,power.raw.aperf,power.raw.mperf,sampled);
    }
}
/* Report a completed operation before taking the NEXT snapshot. Its own save
 * cannot contain its duration. In particular, exit.log contains preceding
 * periodic saves, not a fabricated measurement of its own flush. */
static struct {
    bool pending,ok,valid;
    char name[24];
    uint64_t start,end,bytes,prepare,write,verify,sync;
    struct usb_storage_timing usb;
} previous_save;
/* Emitted only after the measured scope, never inside a transfer. IDs pair
 * these bounded aggregates with their save/sample without per-request logging. */
static void report_transfers(const char *scope,uint64_t id,const struct usb_storage_timing *t) {
    for(unsigned phase=0;phase<3;phase++) {
        const struct usb_storage_phase_timing *p=&t->phase[phase];
        console_printf("usb-timing: version=2 scope=%s id=%u phase=%u calls=%u callback_us=%u max_us=%u over20ms=%u over100ms=%u over1s=%u observed_calls=%u observe_us=%u resume_us=%u observe_max_us=%u resume_max_us=%u service_checks=%u service_samples=%u service_gap_max_us=%u completion_gap_max_us=%u valid=%u\n",
            scope,id,(uint64_t)phase,t->calls[phase],t->us[phase],p->max_us,
            p->over20ms,p->over100ms,p->over1s,p->observed_calls,p->observe_us,p->resume_us,
            p->observe_max_us,p->resume_max_us,p->service_checks,p->service_samples,
            p->service_gap_max_us,p->completion_gap_max_us,(uint64_t)t->valid);
    }
}
static void report_save(void) {
    if(!previous_save.pending) return;
    console_printf("workspace-cost: version=3 name=%s start_us=%u end_us=%u bytes=%u prepare_us=%u write_us=%u verify_us=%u sync_us=%u cbw_calls=%u cbw_us=%u data_calls=%u data_us=%u csw_calls=%u csw_us=%u failures=%u ok=%u valid=%u\n",
        previous_save.name,previous_save.start,previous_save.end,previous_save.bytes,
        previous_save.prepare,previous_save.write,previous_save.verify,previous_save.sync,
        previous_save.usb.calls[0],previous_save.usb.us[0],
        previous_save.usb.calls[1],previous_save.usb.us[1],
        previous_save.usb.calls[2],previous_save.usb.us[2],previous_save.usb.failures,
        (uint64_t)previous_save.ok,(uint64_t)previous_save.valid);
    report_transfers("logger",previous_save.start,&previous_save.usb);
    previous_save.pending=false;
}
static bool save(const char *name) {
    /* Save latency is a clock measurement, independent of optional MSR power
     * telemetry. Ordinary desktop stress needs these intervals too. */
    bool measured=timer_reference_ready();
    uint64_t start=measured?timer_monotonic_us():0;
    report_save();
    device_checkpoint();
    uint64_t snapshot_us=timer_monotonic_us();
    console_printf("workspace-log: snapshot=%s monotonic_us=%u total_before=%u tail_limit=%u\n",name,snapshot_us,bootlog_total(),snapshot_limit);
    uint64_t total,count=bootlog_snapshot(snapshot,snapshot_limit,&total),flags;
    if(snapshot_limit>BOOTLOG_DEFAULT_CAPACITY && total!=count) {
        console_write("workspace-log: SAVE FAILED extended capture prefix lost\n");
        return false;
    }
    unsigned at=copy(path,directory);path[at++]='/';copy(path+at,name);
    uint64_t prepared=measured?timer_monotonic_us():0;
    bool tracing=measured && usb_storage_timing_begin();
    bool entered=begin(&flags),ok=false;
    uint64_t written=prepared,verified=prepared;
    if(entered) {
        ok=workspace_active() && !process_has_open_vfs_prefix(path) &&
            exfat_write_file(path,(const uint8_t *)snapshot,count);
        written=measured?timer_monotonic_us():0;
        const struct vfs_node *node=ok?vfs_lookup(path):NULL;
        const uint8_t *data=NULL;uint64_t length=0;
        ok=ok && node && vfs_read_all(node,&data,&length) && length==count;
        for(uint64_t i=0;ok && i<count;i++) if(data[i]!=(uint8_t)snapshot[i]) ok=false;
        if(node && data) vfs_release_data(node,data);
        end(flags);
        verified=measured?timer_monotonic_us():0;
    }
    /* Our direct exFAT mutation/readback is complete. Persist that committed
     * data without submitting unrelated applications' dirty file buffers. */
    if(ok) ok=workspace_flush_committed();
    uint64_t finished=measured?timer_monotonic_us():0;
    if(measured) {
        previous_save.pending=true;copy(previous_save.name,name);previous_save.ok=ok;
        previous_save.start=start;previous_save.end=finished;previous_save.bytes=count;
        previous_save.usb=(struct usb_storage_timing){0};
        if(tracing) usb_storage_timing_end(&previous_save.usb);
        previous_save.valid=tracing && previous_save.usb.valid && start<=prepared &&
            prepared<=written && written<=verified && verified<=finished;
        previous_save.prepare=prepared-start;previous_save.write=written-prepared;
        previous_save.verify=verified-written;previous_save.sync=finished-verified;
    }
    if(ok) console_printf("workspace-log: durable snapshot=%s monotonic_us=%u\n",name,snapshot_us);
    else console_printf("workspace-log: SAVE FAILED path=%s; previous checkpoint retained\n",path);
    return ok;
}
static bool ready(void *arg) { (void)arg;return enabled && timer_ticks()>=due; }
static bool activated(void *arg) { (void)arg;return enabled; }
static bool idle(void *arg) { (void)arg;return !busy; }
static bool periodic_save(void) {
    char name[24];
    if(retain_checkpoints) {
        if(checkpoint>=32) { console_write("workspace-log: capture checkpoint limit reached\n");return false; }
        copy(name,"checkpoint-00.log");hex(name+11,checkpoint,2);copy(name+13,".log");
    } else copy(name,checkpoint&1?"session-b.log":"session-a.log");
    checkpoint++;return save(name);
}
static void worker(void *arg) {
    (void)arg;
    for(;;) {
        if(!enabled) { scheduler_wait(&wake,activated,NULL);continue; }
        if(!ready(NULL)) { scheduler_wait_timeout(&wake,ready,NULL,due);continue; }
        busy=true;
        healthy=periodic_save();
        if(!healthy) enabled=false;
        due=timer_ticks()+1000;busy=false;scheduler_wake_all(&wake);
    }
}
static bool start(uint64_t limit,bool retain) {
    if(enabled || busy || wifi_capture_parked || !workspace_active()) return false;
    if(limit>BOOTLOG_DEFAULT_CAPACITY && !bootlog_extend_capture_to(limit)) {
        console_write("workspace-log: REFUSED extended capture prefix lost\n");
        return false;
    }
    snapshot_limit=limit;
    (void)scheduler_io_observe_reset();
    uint64_t flags;if(!begin(&flags)) return false;
    static const char parent[]="/work/srvros-work/Logs";
    /* Four log files plus room for reports/screenshots. Check before creating
     * a directory so a full node table cannot masquerade as a started session. */
    if(!exfat_creation_capacity(parent,retain?64:32,8)) {
        directory[0]=0;healthy=false;
        end(flags);
        console_write("workspace-log: REFUSED insufficient file/directory slots; archive old logs in Ubuntu before testing\n");
        return false;
    }
    bool ok=directory_exists(parent) || exfat_create_directory(parent);
    uint64_t stamp=(uint64_t)timer_realtime_us();
    if((int64_t)stamp<0) stamp=timer_monotonic_us();
    unsigned at=copy(directory,parent);at+=copy(directory+at,"/session-");hex(directory+at,stamp,16);at+=16;directory[at++]='-';
    bool created=false;
    for(unsigned i=0;ok && i<256;i++) {
        hex(directory+at,i,2);
        if(!vfs_lookup(directory)) { created=exfat_create_directory(directory);break; }
    }
    end(flags);
    if(!ok || !created) { console_write("workspace-log: cannot create session directory\n");return false; }
    console_printf("workspace-log: directory=%s periodic_seconds=10\n",directory);
    retain_checkpoints=retain;checkpoint=0;healthy=save("boot.log");
    if(!healthy) return false;
    if(!started) started=scheduler_spawn("workspace-log",worker,NULL);
    enabled=started;due=timer_ticks()+1000;scheduler_wake_all(&wake);
    if(!started) console_write("workspace-log: worker unavailable; boot/exit snapshots only\n");
    return started;
}
bool workspace_log_start(void) { return start(BOOTLOG_DEFAULT_CAPACITY,false); }
bool workspace_log_start_capture(void) { return start(BOOTLOG_CAPTURE_CAPACITY,false); }
bool workspace_log_start_large_capture(void) { return start(BOOTLOG_LARGE_CAPTURE_CAPACITY,false); }
bool workspace_log_start_full_capture(void) { return start(BOOTLOG_FULL_CAPTURE_CAPACITY,false); }
bool workspace_log_start_checkpoints(void) { return start(BOOTLOG_DEFAULT_CAPACITY,true); }
bool workspace_log_active(void) { return enabled || busy; }
bool workspace_log_quiesce(uint64_t timeout_ms) {
    enabled=false;scheduler_wake_all(&wake);
    /* Saturate, and round up at the kernel's 100Hz tick rate. */
    uint64_t now=timer_ticks(),ticks=timeout_ms/10+(timeout_ms%10!=0);
    uint64_t end=ticks>UINT64_MAX-now?UINT64_MAX:now+ticks;
    while(busy) {
        if(timer_ticks()>=end)return false;
        scheduler_wait_timeout(&wake,idle,NULL,end);
    }
    return true;
}
bool workspace_log_stop(void) {
    enabled=false;scheduler_wake_all(&wake);
    while(busy) scheduler_wait(&wake,idle,NULL);
    if(!directory[0] || !workspace_active()) return false;
    bool ok=save("exit.log");
    console_printf("workspace-log: final=%s directory=%s\n",ok?"saved":"FAILED",directory);
    wifi_capture_parked=false;
    return ok && healthy;
}
bool workspace_log_pause(void) {
    enabled=false;scheduler_wake_all(&wake);
    while(busy) scheduler_wait(&wake,idle,NULL);
    if(!directory[0] || !workspace_active())return false;
    return save("session-a.log") && healthy;
}

/* Fixed payload and balanced order avoid comparing different-size log snapshots.
 * No raw block writes: retain normal exFAT metadata, readback and flush paths. */
static uint8_t wait_payload[32768];
static bool wait_write(const char *file) {
    uint64_t flags;if(!begin(&flags))return false;
    bool ok=!process_has_open_vfs_prefix(file) &&
        exfat_write_file(file,wait_payload,sizeof(wait_payload));
    const struct vfs_node *node=ok?vfs_lookup(file):NULL;
    const uint8_t *data=NULL;uint64_t length=0;
    ok=ok && node && vfs_read_all(node,&data,&length) && length==sizeof(wait_payload);
    for(uint64_t i=0;ok && i<length;i++)if(data[i]!=wait_payload[i])ok=false;
    if(node && data)vfs_release_data(node,data);
    end(flags);
    return ok && workspace_sync();
}
static bool wait_policy(uint64_t policy,uint64_t *previous) {
    uint64_t flags;if(!begin(&flags))return false;
    bool ok=xhci_storage_wait_policy(policy,previous);end(flags);return ok;
}
bool workspace_storage_wait_capture(void) {
    if(!timer_reference_ready() || !directory[0] || !workspace_active() ||
       !workspace_log_quiesce(5000) || !healthy)return false;
    report_save();
    char file[160];unsigned at=copy(file,directory);copy(file+at,"/wait-fixture.bin");
    uint64_t flags;if(!begin(&flags))return false;
    bool fresh=!vfs_lookup(file);end(flags);
    if(!fresh)return false; /* Never overwrite a preexisting document/fixture. */
    for(unsigned i=0;i<sizeof(wait_payload);i++)wait_payload[i]=(uint8_t)(i*37u+11u);
    uint64_t original=0,unused=0;
    if(!wait_policy(0,&original))return false;
    bool ok=wait_write(file); /* Allocate once; all measured operations overwrite. */
    static const uint64_t policies[]={0,4000,20000};
    static const unsigned order[6][3]={{0,1,2},{1,2,0},{2,0,1},{2,1,0},{1,0,2},{0,2,1}};
    unsigned completed=0;uint64_t start=timer_monotonic_us();
    console_printf("storage-wait: begin version=3 path=%s bytes=32768 rounds=6 policies=3 original_us=%u\n",file,original);
    for(unsigned round=0;ok && round<6;round++)for(unsigned pos=0;ok && pos<3;pos++) {
        if(timer_monotonic_us()-start>180000000) { ok=false;break; }
        uint64_t policy=policies[order[round][pos]];
        if(!wait_policy(policy,&unused)) { ok=false;break; }
        struct scheduler_stats before,after;scheduler_snapshot(&before);
        bool tracing=usb_storage_timing_begin();
        uint64_t began=timer_monotonic_us();
        ok=tracing && wait_write(file);
        uint64_t ended=timer_monotonic_us();
        struct usb_storage_timing usb={0};if(tracing)usb_storage_timing_end(&usb);
        scheduler_snapshot(&after);
        ok=ok && usb.valid && !usb.failures && ended>=began;
        console_printf("storage-wait: sample round=%u position=%u poll_us=%u start_us=%u bytes=32768 elapsed_us=%u sampled_ticks=%u idle_ticks=%u cbw_calls=%u cbw_us=%u data_calls=%u data_us=%u csw_calls=%u csw_us=%u failures=%u ok=%u\n",
            (uint64_t)round,(uint64_t)pos,policy,began,ended-began,
            after.sampled_ticks-before.sampled_ticks,after.idle_ticks-before.idle_ticks,
            usb.calls[0],usb.us[0],usb.calls[1],usb.us[1],usb.calls[2],usb.us[2],usb.failures,(uint64_t)ok);
        report_transfers("comparison",began,&usb);
        if(ok)completed++;
    }
    /* A transport failure refuses policy changes while quarantined. Retain that
     * failure; never reset/reuse DMA merely to finish a diagnostic. */
    bool restored=wait_policy(original,&unused);
    console_printf("storage-wait: complete=%s samples=%u restored=%s elapsed_us=%u\n",
        ok&&completed==18&&restored?"yes":"NO",(uint64_t)completed,restored?"yes":"NO",timer_monotonic_us()-start);
    return ok && completed==18 && restored;
}

bool workspace_log_start_extended_capture(void) { return start(BOOTLOG_EXTENDED_CAPTURE_CAPACITY,false); }

bool workspace_log_begin_wifi_capture(void) {
    if(!enabled || wifi_capture_parked || !healthy ||
       snapshot_limit!=BOOTLOG_EXTENDED_CAPTURE_CAPACITY || retain_checkpoints ||
       !directory[0] || !workspace_active())return false;
    if(!workspace_log_quiesce(5000))return false;
    healthy=save("session-a.log") && healthy;
    if(!healthy)return false;
    wifi_capture_parked=true;
    console_write("workspace-log: phase=wifi-radio periodic=paused tail_limit=1048576\n");
    return true;
}

bool workspace_log_begin_wifi_desktop(void) {
    if((!enabled && !wifi_capture_parked) || !healthy || snapshot_limit!=BOOTLOG_EXTENDED_CAPTURE_CAPACITY ||
       retain_checkpoints || !directory[0] || !workspace_active())return false;
    if(!workspace_log_quiesce(5000))return false;
    if(!save("wifi-scan.log")){healthy=false;return false;}
    snapshot_limit=BOOTLOG_DEFAULT_CAPACITY;
    console_write("workspace-log: phase=wifi-desktop startup_saved=1 tail_limit=65536\n");
    healthy=save("wifi-ui-begin.log") && healthy;
    if(!healthy)return false;
    wifi_capture_parked=false;enabled=true;due=timer_ticks()+1000;scheduler_wake_all(&wake);return true;
}
