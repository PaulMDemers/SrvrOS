#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_WORKSPACE_LOG_HOST_TEST
#include "../kernel/src/workspace_log.c"
static struct item { char path[160];struct vfs_node node;unsigned char bytes[BOOTLOG_EXTENDED_CAPTURE_CAPACITY]; } files[64];
static unsigned count,depth,writes,flushes,device_reads;
static bool capacity_ok=true,extend_ok=true;
static uint64_t snapshot_bytes=8;
static bool lost_prefix;
bool bootlog_extend_capture(void) { return extend_ok; }
bool bootlog_extend_capture_to(uint64_t capacity) { assert(capacity==BOOTLOG_CAPTURE_CAPACITY || capacity==BOOTLOG_LARGE_CAPTURE_CAPACITY || capacity==BOOTLOG_FULL_CAPTURE_CAPACITY || capacity==BOOTLOG_EXTENDED_CAPTURE_CAPACITY);return extend_ok; }
static unsigned expected_capacity=32;
static bool mounted=true,io_ok=true,sync_ok=true,corrupt,open_file;
static uint64_t ticks=10,total=100,clock_us;
static bool tracing,reference=true,power_enabled=true;
static unsigned cost_reports;
static uint64_t test_policy=20000;
bool xhci_storage_wait_policy(uint64_t policy,uint64_t *old) { assert(depth && (policy==0||policy==4000||policy==20000));*old=test_policy;test_policy=policy;return true; }
bool timer_reference_ready(void) { return reference; }
bool usb_storage_timing_begin(void) { assert(!tracing);tracing=true;return true; }
void usb_storage_timing_end(struct usb_storage_timing *out) { assert(tracing);tracing=false;*out=(struct usb_storage_timing){.valid=true,.calls={1,2,1},.us={10,20,10}}; }
static struct item *find(const char *p) { for(unsigned i=0;i<count;i++) if(!strcmp(p,files[i].path)) return &files[i];return NULL; }
static struct item *add(const char *p) { assert(count<64 && strlen(p)<160);struct item *f=&files[count++];strcpy(f->path,p);return f; }
const struct vfs_node *vfs_lookup(const char *p) { assert(depth);struct item *f=find(p);return f?&f->node:NULL; }
bool exfat_creation_capacity(const char *p,uint64_t f,uint64_t d) {
    assert(depth && !strcmp(p,"/work/srvros-work/Logs") && f==expected_capacity && d==8);return capacity_ok;
}
bool exfat_create_directory(const char *p) { assert(depth);if(find(p) || !io_ok) return false;add(p)->node.type=VFS_NODE_DIRECTORY;return true; }
bool exfat_write_file(const char *p,const uint8_t *data,uint64_t n) {
    assert(depth && n<=snapshot_limit);writes++;clock_us+=100;if(!io_ok) return false;
    struct item *f=find(p);if(!f) f=add(p);f->node.type=VFS_NODE_FILE;f->node.size=n;memcpy(f->bytes,data,n);return true;
}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *size) {
    assert(depth);clock_us+=30;for(unsigned i=0;i<count;i++) if(n==&files[i].node) { *data=files[i].bytes;*size=n->size;if(corrupt) files[i].bytes[0]^=1;return true; }return false;
}
void vfs_release_data(const struct vfs_node *n,const uint8_t *data) { (void)n;(void)data;assert(depth); }
bool workspace_active(void) { return mounted; }
bool workspace_sync(void) { assert(!depth);flushes++;clock_us+=50;return sync_ok; }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return open_file; }
bool scheduler_io_begin(void) { assert(!depth);depth=1;return true; }
void scheduler_io_end(void) { assert(depth==1);depth=0; }
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg) { (void)name;(void)fn;(void)arg;return true; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) { (void)q;return fn(arg); }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t end) { bool ready=scheduler_wait(q,fn,arg);if(!ready)ticks=end;return ready; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
void scheduler_print_activity(void) { assert(!depth); }
void scheduler_snapshot(struct scheduler_stats *out) { assert(!depth);*out=(struct scheduler_stats){100,90,200,50}; }
uint64_t timer_ticks(void) { return ticks; }
uint64_t timer_monotonic_us(void) { return clock_us; }
int64_t timer_realtime_us(void) { return 123456789; }
uint64_t bootlog_total(void) { return total; }
uint64_t bootlog_snapshot(char *out,uint64_t capacity,uint64_t *seq) { assert(capacity==snapshot_limit && capacity>=snapshot_bytes);
    if(snapshot_bytes==8)memcpy(out,"test log",8);
    else for(uint64_t i=0;i<snapshot_bytes;i++)out[i]=(char)(i%251);
    *seq=capacity>BOOTLOG_DEFAULT_CAPACITY?snapshot_bytes+(lost_prefix?1:0):total;return snapshot_bytes; }
void smc_status(struct srv_smc_status *out) { assert(!depth);*out=(struct srv_smc_status){.version=SRV_SMC_VERSION}; }
void power_status(struct power_snapshot *out) { assert(!depth);*out=(struct power_snapshot){.state=power_enabled?POWER_ACTIVE:POWER_DISABLED,.flags=3,.package_mw=10000,.active_mhz=2200}; }
void audio_stream_snapshot(struct srv_pcm_status *out,uint64_t *owner) { assert(!depth);*out=(struct srv_pcm_status){.version=SRV_PCM_VERSION};*owner=0; }
void audio_status(struct srv_audio_status *out) { assert(!depth);device_reads++;*out=(struct srv_audio_status){.version=SRV_AUDIO_VERSION,.state=SRV_AUDIO_READY,.tests_started=2,.tests_completed=2}; }
void thermal_status(struct srv_thermal_status *out) { assert(!depth);device_reads++;*out=(struct srv_thermal_status){.version=SRV_THERMAL_VERSION,.state=SRV_THERMAL_UNAVAILABLE}; }
void console_printf(const char *fmt,...) { if(!strncmp(fmt,"workspace-cost:",15)) { assert(previous_save.pending && !tracing && !depth);cost_reports++; }total++; }
void console_write(const char *text) { (void)text;total++; }
int main(void) {
    mounted=false;assert(!workspace_log_start() && !writes);mounted=true;
    capacity_ok=false;assert(!workspace_log_start() && !writes && !count && !directory[0]);capacity_ok=true;
    assert(workspace_log_start() && enabled && healthy && writes==1 && flushes==1);
    assert(device_reads==2);
    assert(previous_save.pending && previous_save.ok && previous_save.valid);
    assert(previous_save.write==100 && previous_save.verify==30 && previous_save.sync==50);
    assert(previous_save.end-previous_save.start==180 && !cost_reports && !tracing);
    char first[128];strcpy(first,directory);assert(!workspace_log_start());
    assert(!ready(NULL) && activated(NULL));ticks=due;assert(ready(NULL));
    assert(save("session-a.log") && cost_reports==1);assert(find(path) && find(path)->node.size==8);
    assert(workspace_log_pause() && !enabled && writes==3 && flushes==3);
    assert(!ready(NULL) && !activated(NULL));
    char checkpoint_path[160];strcpy(checkpoint_path,directory);strcat(checkpoint_path,"/exit.log");
    assert(!find(checkpoint_path)); /* A pause must never fabricate completion. */
    mounted=false;assert(!workspace_log_stop());mounted=true;
    assert(workspace_log_stop() && !enabled && writes==4 && flushes==4);
    assert(workspace_log_start() && strcmp(first,directory)); /* Same RTC still gets a fresh folder. */
    unsigned before=writes;open_file=true;assert(!save("session-a.log") && writes==before);open_file=false;
    corrupt=true;before=flushes;assert(!save("session-a.log") && flushes==before);assert(!previous_save.ok && previous_save.sync==0 && !tracing);corrupt=false;
    sync_ok=false;assert(!save("session-b.log"));assert(!previous_save.ok && previous_save.sync==50 && !tracing);sync_ok=true;
    power_enabled=false;assert(save("session-a.log") && !previous_save.pending && !tracing);power_enabled=true;
    reference=false;assert(save("session-b.log") && !previous_save.pending && !tracing);reference=true;
    assert(workspace_log_stop());mounted=false;assert(!workspace_log_stop());
    busy=true;enabled=true;unsigned saved=writes;
    assert(workspace_log_active() && !workspace_log_quiesce(0));
    assert(!enabled && busy && writes==saved);
    assert(!workspace_log_quiesce(11) && busy && writes==saved);
    busy=false;assert(workspace_log_quiesce(0) && !workspace_log_active());
    mounted=true;assert(workspace_log_start());
    unsigned old_writes=writes;assert(workspace_storage_wait_capture());
    assert(writes==old_writes+19 && !enabled && !tracing && test_policy==20000);
    assert(!workspace_storage_wait_capture() && writes==old_writes+19); /* collision */
    assert(workspace_log_start());corrupt=true;old_writes=writes;
    assert(!workspace_storage_wait_capture() && writes==old_writes+1 && test_policy==20000 && !tracing);corrupt=false;
    assert(workspace_log_start());sync_ok=false;old_writes=writes;
    assert(!workspace_storage_wait_capture() && writes==old_writes+1 && test_policy==20000 && !tracing);sync_ok=true;
    assert(workspace_log_start());io_ok=false;old_writes=writes;
    assert(!workspace_storage_wait_capture() && writes==old_writes+1 && test_policy==20000 && !tracing);io_ok=true;
    assert(workspace_log_stop());
    extend_ok=false;old_writes=writes;assert(!workspace_log_start_capture() && writes==old_writes);
    extend_ok=true;snapshot_bytes=BOOTLOG_CAPTURE_CAPACITY;
    assert(workspace_log_start_capture() && snapshot_limit==BOOTLOG_CAPTURE_CAPACITY && !retain_checkpoints);
    struct item *large=find(path);assert(large && large->node.size==snapshot_bytes);
    for(uint64_t i=0;i<snapshot_bytes;i++)assert(large->bytes[i]==i%251);
    assert(!workspace_log_start() && snapshot_limit==BOOTLOG_CAPTURE_CAPACITY);
    old_writes=writes;lost_prefix=true;assert(!save("session-a.log") && writes==old_writes);
    lost_prefix=false;corrupt=true;assert(!save("session-a.log"));corrupt=false;
    assert(workspace_log_stop());snapshot_bytes=8;
    assert(workspace_log_start() && snapshot_limit==BOOTLOG_DEFAULT_CAPACITY);
    assert(workspace_log_stop());
    snapshot_bytes=BOOTLOG_LARGE_CAPTURE_CAPACITY;extend_ok=false;old_writes=writes;
    assert(!workspace_log_start_large_capture() && writes==old_writes);
    extend_ok=true;assert(workspace_log_start_large_capture() && snapshot_limit==BOOTLOG_LARGE_CAPTURE_CAPACITY);
    large=find(path);assert(large && large->node.size==snapshot_bytes);
    for(uint64_t i=0;i<snapshot_bytes;i++)assert(large->bytes[i]==i%251);
    old_writes=writes;lost_prefix=true;assert(!save("session-a.log") && writes==old_writes);lost_prefix=false;
    assert(workspace_log_stop());snapshot_bytes=8;assert(workspace_log_start() && snapshot_limit==BOOTLOG_DEFAULT_CAPACITY);assert(workspace_log_stop());
    snapshot_bytes=BOOTLOG_FULL_CAPTURE_CAPACITY;extend_ok=false;old_writes=writes;
    assert(!workspace_log_start_full_capture() && writes==old_writes);extend_ok=true;
    assert(workspace_log_start_full_capture() && snapshot_limit==BOOTLOG_FULL_CAPTURE_CAPACITY);
    assert(workspace_log_stop());snapshot_bytes=BOOTLOG_EXTENDED_CAPTURE_CAPACITY;extend_ok=false;old_writes=writes;
    assert(!workspace_log_start_extended_capture() && writes==old_writes);extend_ok=true;
    assert(workspace_log_start_extended_capture() && snapshot_limit==BOOTLOG_EXTENDED_CAPTURE_CAPACITY);

    large=find(path);assert(large && large->node.size==snapshot_bytes);for(uint64_t i=0;i<snapshot_bytes;i++)assert(large->bytes[i]==i%251);
    old_writes=writes;lost_prefix=true;assert(!save("session-a.log") && writes==old_writes);lost_prefix=false;
    assert(workspace_log_stop());snapshot_bytes=8;assert(workspace_log_start() && snapshot_limit==BOOTLOG_DEFAULT_CAPACITY);assert(workspace_log_stop());
    count=0;enabled=false;busy=false;expected_capacity=64;
    capacity_ok=false;assert(!workspace_log_start_checkpoints());capacity_ok=true;
    assert(workspace_log_start_checkpoints() && snapshot_limit==BOOTLOG_DEFAULT_CAPACITY && retain_checkpoints && checkpoint==0);
    for(unsigned i=0;i<32;i++) {
        assert(periodic_save());
        char expected[160];snprintf(expected,sizeof(expected),"%s/checkpoint-%02x.log",directory,i);
        assert(find(expected));
    }
    old_writes=writes;assert(!periodic_save() && writes==old_writes && checkpoint==32);
    assert(workspace_log_stop());
    expected_capacity=32;assert(workspace_log_start() && !retain_checkpoints && checkpoint==0);
    assert(periodic_save() && strstr(path,"session-a.log"));
    assert(periodic_save() && strstr(path,"session-b.log"));
    assert(workspace_log_stop());
    puts("workspace-log: PASS gated writes/readback, collision-safe sessions, periodic readiness, open-file refusal, corruption/sync failure, stop before unmount");
}
