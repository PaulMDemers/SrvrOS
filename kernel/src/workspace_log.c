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
#include <srvros/smc.h>
#include <stddef.h>
#include <stdint.h>
static char directory[128],path[160],snapshot[65536];
static bool enabled,started,busy,healthy;
static unsigned checkpoint;
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
    if(power.state!=POWER_DISABLED)
        console_printf("workspace-devices: power_state=%u flags=%u energy_shift=%u package_mw=%u active_mhz=%u nominal_mhz=%u package_uj=%u sample_us=%u interval_us=%u samples=%u rejected=%u energy=%u aperf=%u mperf=%u monotonic_us=%u\n",
            (uint64_t)power.state,(uint64_t)power.flags,(uint64_t)power.energy_shift,power.package_mw,power.active_mhz,(uint64_t)power.nominal_mhz,
            power.package_uj,power.sampled_us,power.interval_us,power.samples,power.rejected,power.raw.energy,power.raw.aperf,power.raw.mperf,sampled);
}
static bool save(const char *name) {
    device_checkpoint();
    console_printf("workspace-log: snapshot=%s monotonic_us=%u total_before=%u tail_limit=65536\n",name,timer_monotonic_us(),bootlog_total());
    uint64_t total,count=bootlog_snapshot(snapshot,sizeof(snapshot),&total),flags;
    unsigned at=copy(path,directory);path[at++]='/';copy(path+at,name);
    if(!begin(&flags)) return false;
    bool ok=workspace_active() && !process_has_open_vfs_prefix(path) &&
        exfat_write_file(path,(const uint8_t *)snapshot,count);
    const struct vfs_node *node=ok?vfs_lookup(path):NULL;
    const uint8_t *data=NULL;uint64_t length=0;
    ok=ok && node && vfs_read_all(node,&data,&length) && length==count;
    for(uint64_t i=0;ok && i<count;i++) if(data[i]!=(uint8_t)snapshot[i]) ok=false;
    if(node && data) vfs_release_data(node,data);
    end(flags);
    if(ok) ok=workspace_sync();
    if(!ok) console_printf("workspace-log: SAVE FAILED path=%s; previous checkpoint retained\n",path);
    return ok;
}
static bool ready(void *arg) { (void)arg;return enabled && timer_ticks()>=due; }
static bool activated(void *arg) { (void)arg;return enabled; }
static bool idle(void *arg) { (void)arg;return !busy; }
static void worker(void *arg) {
    (void)arg;
    for(;;) {
        if(!enabled) { scheduler_wait(&wake,activated,NULL);continue; }
        if(!ready(NULL)) { scheduler_wait_timeout(&wake,ready,NULL,due);continue; }
        busy=true;
        healthy=save(checkpoint++&1?"session-b.log":"session-a.log");
        if(!healthy) enabled=false;
        due=timer_ticks()+1000;busy=false;scheduler_wake_all(&wake);
    }
}
bool workspace_log_start(void) {
    if(enabled || busy || !workspace_active()) return false;
    uint64_t flags;if(!begin(&flags)) return false;
    static const char parent[]="/work/srvros-work/Logs";
    /* Four log files plus room for reports/screenshots. Check before creating
     * a directory so a full node table cannot masquerade as a started session. */
    if(!exfat_creation_capacity(parent,32,8)) {
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
    checkpoint=0;healthy=save("boot.log");
    if(!healthy) return false;
    if(!started) started=scheduler_spawn("workspace-log",worker,NULL);
    enabled=started;due=timer_ticks()+1000;scheduler_wake_all(&wake);
    if(!started) console_write("workspace-log: worker unavailable; boot/exit snapshots only\n");
    return started;
}
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
    return ok && healthy;
}
bool workspace_log_pause(void) {
    enabled=false;scheduler_wake_all(&wake);
    while(busy) scheduler_wait(&wake,idle,NULL);
    if(!directory[0] || !workspace_active())return false;
    return save("session-a.log") && healthy;
}
