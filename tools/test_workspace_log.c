#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_WORKSPACE_LOG_HOST_TEST
#include "../kernel/src/workspace_log.c"
static struct item { char path[160];struct vfs_node node;unsigned char bytes[65536]; } files[24];
static unsigned count,depth,writes,flushes,device_reads;
static bool mounted=true,io_ok=true,sync_ok=true,corrupt,open_file;
static uint64_t ticks=10,total=100;
static struct item *find(const char *p) { for(unsigned i=0;i<count;i++) if(!strcmp(p,files[i].path)) return &files[i];return NULL; }
static struct item *add(const char *p) { assert(count<24 && strlen(p)<160);struct item *f=&files[count++];strcpy(f->path,p);return f; }
const struct vfs_node *vfs_lookup(const char *p) { assert(depth);struct item *f=find(p);return f?&f->node:NULL; }
bool exfat_create_directory(const char *p) { assert(depth);if(find(p) || !io_ok) return false;add(p)->node.type=VFS_NODE_DIRECTORY;return true; }
bool exfat_write_file(const char *p,const uint8_t *data,uint64_t n) {
    assert(depth && n<=65536);writes++;if(!io_ok) return false;
    struct item *f=find(p);if(!f) f=add(p);f->node.type=VFS_NODE_FILE;f->node.size=n;memcpy(f->bytes,data,n);return true;
}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *size) {
    assert(depth);for(unsigned i=0;i<count;i++) if(n==&files[i].node) { *data=files[i].bytes;*size=n->size;if(corrupt) files[i].bytes[0]^=1;return true; }return false;
}
void vfs_release_data(const struct vfs_node *n,const uint8_t *data) { (void)n;(void)data;assert(depth); }
bool workspace_active(void) { return mounted; }
bool workspace_sync(void) { assert(!depth);flushes++;return sync_ok; }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return open_file; }
bool scheduler_io_begin(void) { assert(!depth);depth=1;return true; }
void scheduler_io_end(void) { assert(depth==1);depth=0; }
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg) { (void)name;(void)fn;(void)arg;return true; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) { (void)q;return fn(arg); }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t end) { bool ready=scheduler_wait(q,fn,arg);if(!ready)ticks=end;return ready; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
void scheduler_snapshot(struct scheduler_stats *out) { assert(!depth);*out=(struct scheduler_stats){100,90,200,50}; }
uint64_t timer_ticks(void) { return ticks; }
uint64_t timer_monotonic_us(void) { return ticks*10000; }
int64_t timer_realtime_us(void) { return 123456789; }
uint64_t bootlog_total(void) { return total; }
uint64_t bootlog_snapshot(char *out,uint64_t capacity,uint64_t *seq) { assert(capacity>=8);memcpy(out,"test log",8);*seq=total;return 8; }
void smc_status(struct srv_smc_status *out) { assert(!depth);*out=(struct srv_smc_status){.version=SRV_SMC_VERSION}; }
void power_status(struct power_snapshot *out) { assert(!depth);*out=(struct power_snapshot){.state=POWER_ACTIVE,.flags=3,.package_mw=10000,.active_mhz=2200}; }
void audio_stream_snapshot(struct srv_pcm_status *out,uint64_t *owner) { assert(!depth);*out=(struct srv_pcm_status){.version=SRV_PCM_VERSION};*owner=0; }
void audio_status(struct srv_audio_status *out) { assert(!depth);device_reads++;*out=(struct srv_audio_status){.version=SRV_AUDIO_VERSION,.state=SRV_AUDIO_READY,.tests_started=2,.tests_completed=2}; }
void thermal_status(struct srv_thermal_status *out) { assert(!depth);device_reads++;*out=(struct srv_thermal_status){.version=SRV_THERMAL_VERSION,.state=SRV_THERMAL_UNAVAILABLE}; }
void console_printf(const char *fmt,...) { (void)fmt;total++; }
void console_write(const char *text) { (void)text;total++; }
int main(void) {
    mounted=false;assert(!workspace_log_start() && !writes);mounted=true;
    assert(workspace_log_start() && enabled && healthy && writes==1 && flushes==1);
    assert(device_reads==2);
    char first[128];strcpy(first,directory);assert(!workspace_log_start());
    assert(!ready(NULL) && activated(NULL));ticks=due;assert(ready(NULL));
    assert(save("session-a.log"));assert(find(path) && find(path)->node.size==8);
    assert(workspace_log_pause() && !enabled && writes==3 && flushes==3);
    assert(!ready(NULL) && !activated(NULL));
    char checkpoint_path[160];strcpy(checkpoint_path,directory);strcat(checkpoint_path,"/exit.log");
    assert(!find(checkpoint_path)); /* A pause must never fabricate completion. */
    mounted=false;assert(!workspace_log_stop());mounted=true;
    assert(workspace_log_stop() && !enabled && writes==4 && flushes==4);
    assert(workspace_log_start() && strcmp(first,directory)); /* Same RTC still gets a fresh folder. */
    unsigned before=writes;open_file=true;assert(!save("session-a.log") && writes==before);open_file=false;
    corrupt=true;before=flushes;assert(!save("session-a.log") && flushes==before);corrupt=false;
    sync_ok=false;assert(!save("session-b.log"));sync_ok=true;
    assert(workspace_log_stop());mounted=false;assert(!workspace_log_stop());
    busy=true;enabled=true;unsigned saved=writes;
    assert(workspace_log_active() && !workspace_log_quiesce(0));
    assert(!enabled && busy && writes==saved);
    assert(!workspace_log_quiesce(11) && busy && writes==saved);
    busy=false;assert(workspace_log_quiesce(0) && !workspace_log_active());
    puts("workspace-log: PASS gated writes/readback, collision-safe sessions, periodic readiness, open-file refusal, corruption/sync failure, stop before unmount");
}
