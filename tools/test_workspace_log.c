#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_WORKSPACE_LOG_HOST_TEST
#include "../kernel/src/workspace_log.c"
static struct item { char path[160];struct vfs_node node;unsigned char bytes[65536]; } files[64];
static unsigned count,depth,writes,flushes,device_reads;
static bool capacity_ok=true;
static unsigned expected_capacity=32;
static bool mounted=true,io_ok=true,sync_ok=true,corrupt,open_file;
static uint64_t ticks=10,total=100,clock_us,save_tick_delay;
static bool tracing,reference=true,power_enabled=true;
static unsigned cost_reports;
static struct sync_timing_snapshot sync_fixture;
void process_directory_admission_snapshot(struct process_directory_admission *out) { *out=(struct process_directory_admission){.handled=3,.entries=17}; }
static unsigned sync_headers,sync_records,sync_worst,sync_usb,sync_command,sidecar_reports;
static unsigned global_syncs,durable_reports;
static struct srv_pcm_status pcm_fixture={.version=SRV_PCM_VERSION};
static uint64_t pcm_owner;
static bool foreign_io;
bool scheduler_io_active(void) {return depth || foreign_io;}
static struct exfat_sidecar_trace sidecar_fixture;
static struct {uint64_t at;struct exfat_sidecar_trace trace;} emitted_sidecars[8];
static unsigned emitted_sidecar_count;
void exfat_sidecar_snapshot(struct exfat_sidecar_trace *out) {*out=sidecar_fixture;}
void exfat_print_sidecar_trace(uint64_t at,const struct exfat_sidecar_trace *s) {
    assert(!depth);sidecar_reports++;
    emitted_sidecars[emitted_sidecar_count%8].at=at;
    emitted_sidecars[emitted_sidecar_count++%8].trace=*s;
}
void exfat_print_append_contiguity(void) {assert(!depth);}

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
    assert(depth && n<=65536);writes++;ticks+=save_tick_delay;clock_us+=100+save_tick_delay*10000;if(!io_ok) return false;
    struct item *f=find(p);if(!f) f=add(p);f->node.type=VFS_NODE_FILE;f->node.size=n;memcpy(f->bytes,data,n);return true;
}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *size) {
    assert(depth);clock_us+=30;for(unsigned i=0;i<count;i++) if(n==&files[i].node) { *data=files[i].bytes;*size=n->size;if(corrupt) files[i].bytes[0]^=1;return true; }return false;
}
void vfs_release_data(const struct vfs_node *n,const uint8_t *data) { (void)n;(void)data;assert(depth); }
bool workspace_active(void) { return mounted; }
bool workspace_flush_committed(void) { assert(!depth);flushes++;clock_us+=50;return sync_ok; }
bool workspace_sync(void) { global_syncs++;return workspace_flush_committed(); }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return open_file; }
bool scheduler_io_begin(void) { assert(!depth);depth=1;return true; }
void scheduler_io_end(void) { assert(depth==1);depth=0; }
bool scheduler_spawn(const char *name,scheduler_thread_fn fn,void *arg) { (void)name;(void)fn;(void)arg;return true; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) { (void)q;return fn(arg); }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t end) { bool ready=scheduler_wait(q,fn,arg);if(!ready)ticks=end;return ready; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
bool scheduler_io_observe_reset(void) { assert(!depth);return true; }
void exfat_print_costs(void) { assert(!depth); }
void exfat_print_sidecar_equality(void) { assert(!depth);sidecar_reports++; }
static unsigned launch_trace_prints;
void scheduler_print_io_owners(void) { assert(!depth); }
void scheduler_print_activity(void) { assert(!depth); }
void scheduler_snapshot(struct scheduler_stats *out) { assert(!depth);*out=(struct scheduler_stats){100,90,200,50}; }
uint64_t timer_ticks(void) { return ticks; }
uint64_t timer_monotonic_us(void) { return clock_us; }
int64_t timer_realtime_us(void) { return 123456789; }
uint64_t bootlog_total(void) { return total; }
uint64_t bootlog_snapshot(char *out,uint64_t capacity,uint64_t *seq) { assert(capacity>=8);memcpy(out,"test log",8);*seq=total;return 8; }
void smc_status(struct srv_smc_status *out) { assert(!depth);*out=(struct srv_smc_status){.version=SRV_SMC_VERSION}; }
void power_status(struct power_snapshot *out) { assert(!depth);*out=(struct power_snapshot){.state=power_enabled?POWER_ACTIVE:POWER_DISABLED,.flags=3,.package_mw=10000,.active_mhz=2200}; }
void audio_stream_snapshot(struct srv_pcm_status *out,uint64_t *owner) { assert(!depth);*out=pcm_fixture;*owner=pcm_owner; }
void audio_status(struct srv_audio_status *out) { assert(!depth);device_reads++;*out=(struct srv_audio_status){.version=SRV_AUDIO_VERSION,.state=SRV_AUDIO_READY,.tests_started=2,.tests_completed=2}; }
void thermal_status(struct srv_thermal_status *out) { assert(!depth);device_reads++;*out=(struct srv_thermal_status){.version=SRV_THERMAL_VERSION,.state=SRV_THERMAL_UNAVAILABLE}; }
void console_printf(const char *fmt,...) {
    if(!strncmp(fmt,"sync-snapshot:",14))sync_headers++;
    if(!strncmp(fmt,"sync-record:",12))sync_records++;
    if(!strncmp(fmt,"sync-worst:",11))sync_worst++;
    if(!strncmp(fmt,"sync-usb-timing:",16))sync_usb++;
    if(!strncmp(fmt,"sync-usb-command:",17))sync_command++;
    if(!strncmp(fmt,"workspace-cost:",15)) { assert(previous_save.pending && !tracing && !depth);cost_reports++; }
    if(!strncmp(fmt,"workspace-log: durable",22)) {assert(!tracing && !depth);durable_reports++;}
    total++;
}
void console_write(const char *text) { (void)text;total++; }
static void test_sidecar_retention(void) {
    assert(sizeof(snapshot)==65536);
    sidecar_fixture=(struct exfat_sidecar_trace){.checks=10,.equal=4,.changed=6,.retained_bytes=4096};
    pcm_fixture=(struct srv_pcm_status){.version=SRV_PCM_VERSION,.state=SRV_PCM_PLAYING,.handle=11,.position_bytes=4};pcm_owner=9;
    clock_us=1000;
    foreign_io=true;sidecar_checkpoint();assert(!sidecar_pcm.first.at);foreign_io=false;
    sidecar_fixture.active=1;sidecar_checkpoint();assert(!sidecar_pcm.first.at);sidecar_fixture.active=0;
    pcm_fixture.state=SRV_PCM_BUFFERING;sidecar_checkpoint();assert(!sidecar_pcm.first.at);
    pcm_fixture.state=SRV_PCM_PLAYING;pcm_owner=0;sidecar_checkpoint();assert(!sidecar_pcm.first.at);pcm_owner=9;
    pcm_fixture.position_bytes=0;sidecar_checkpoint();assert(!sidecar_pcm.first.at);pcm_fixture.position_bytes=4;
    sidecar_checkpoint();assert(sidecar_pcm.first.at==1000 && sidecar_pcm.first.trace.equal==4 && !sidecar_pcm.last.at);
    clock_us=2000;sidecar_checkpoint();assert(!sidecar_pcm.last.at);
    sidecar_fixture.checks=11;sidecar_fixture.equal=5;sidecar_fixture.retained_bytes=5000;clock_us=3000;
    sidecar_checkpoint();assert(sidecar_pcm.last.at==3000 && sidecar_pcm.last.trace.equal==5);
    sidecar_fixture.active=1;clock_us=4000;sidecar_checkpoint();assert(sidecar_pcm.last.at==3000);sidecar_fixture.active=0;
    sidecar_fixture.checks=12;sidecar_fixture.equal=6;sidecar_fixture.retained_bytes=6000;
    uint64_t *bad[]={&sidecar_fixture.invalid,&sidecar_fixture.refused,&sidecar_fixture.flush_failed};
    for(unsigned i=0;i<3;i++){*bad[i]=1;sidecar_checkpoint();assert(sidecar_pcm.last.at==3000);*bad[i]=0;}
    sidecar_fixture.changed++;sidecar_checkpoint();assert(sidecar_pcm.last.at==3000);sidecar_fixture.changed--;
    pcm_fixture.state=SRV_PCM_DONE;clock_us=5000;sidecar_checkpoint();assert(sidecar_pcm.last.at==3000);
    unsigned before=emitted_sidecar_count;sidecar_history_print();assert(emitted_sidecar_count==before+2);
    assert(emitted_sidecars[before%8].at==1000 && emitted_sidecars[before%8].trace.equal==4);
    assert(emitted_sidecars[(before+1)%8].at==3000 && emitted_sidecars[(before+1)%8].trace.equal==5);
    pcm_fixture.state=SRV_PCM_DRAINING;pcm_fixture.handle=12;clock_us=6000;sidecar_checkpoint();
    assert(sidecar_pcm.first.at==6000 && !sidecar_pcm.last.at);
    pcm_owner=8;clock_us=7000;sidecar_checkpoint();assert(sidecar_pcm.first.at==7000 && !sidecar_pcm.last.at);
    sidecar_fixture=(struct exfat_sidecar_trace){0};pcm_fixture=(struct srv_pcm_status){.version=SRV_PCM_VERSION};pcm_owner=0;
    clock_us=0;sidecar_reports=emitted_sidecar_count=0;
    /* Leave a prior stream behind: the real start_log below must clear it. */
    puts("sidecar-retention: PASS original clocks and immutable copies; incomplete/post-PCM/growth refusal; handle/owner boundaries;64KiB unchanged");
}
int main(void) {
    test_sidecar_retention();
    sync_checkpoint(false);assert(!sync_headers && !sync_records && !sync_worst && !sync_usb && !sidecar_reports);
    sync_fixture.enabled=1;sync_fixture.records[0].pid=7;
    sync_checkpoint(false);assert(sync_headers==1 && sync_records==1 && !sync_worst && !sync_usb && sidecar_reports==1);
    sync_checkpoint(true);assert(sync_headers==2 && sync_records==2 && sync_worst==1 && sync_usb==6 && sync_command==2 && sidecar_reports==2);
    assert(!writes && !flushes && !tracing && !depth);
    sync_fixture=(struct sync_timing_snapshot){0};total=100;

    mounted=false;assert(!workspace_log_start() && !writes);mounted=true;
    capacity_ok=false;assert(!workspace_log_start() && !writes && !count && !directory[0]);capacity_ok=true;
    assert(workspace_log_start() && enabled && healthy && writes==1 && flushes==1);
    assert(!sidecar_pcm.first.at && !sidecar_pcm.last.at && !sidecar_pcm.owner && !sidecar_pcm.handle);
    assert(!global_syncs && durable_reports==1);
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
    assert(workspace_log_stop() && !enabled && writes==4 && flushes==4 && launch_trace_prints==1);
    assert(workspace_log_start() && strcmp(first,directory)); /* Same RTC still gets a fresh folder. */
    unsigned before=writes;open_file=true;assert(!save("session-a.log") && writes==before);open_file=false;
    unsigned durable_before=durable_reports;
    corrupt=true;before=flushes;assert(!save("session-a.log") && flushes==before);assert(!previous_save.ok && previous_save.sync==0 && !tracing && durable_reports==durable_before);corrupt=false;
    sync_ok=false;assert(!save("session-b.log"));assert(!previous_save.ok && previous_save.sync==50 && !tracing && durable_reports==durable_before);sync_ok=true;
    power_enabled=false;assert(save("session-a.log") && previous_save.pending && previous_save.valid && !tracing);
    assert(previous_save.write==100 && previous_save.verify==30 && previous_save.sync==50);
    assert(previous_save.end-previous_save.start==180);power_enabled=true;
    reference=false;assert(save("session-b.log") && !previous_save.pending && !tracing);reference=true;
    assert(workspace_log_stop());mounted=false;assert(!workspace_log_stop());
    assert(!global_syncs); /* Every log save used only the committed-data barrier. */
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
    count=0;enabled=false;busy=false;expected_capacity=64;
    capacity_ok=false;assert(!workspace_log_start_capture());capacity_ok=true;
    assert(workspace_log_start_capture() && retain_checkpoints && checkpoint==0);
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
    assert(workspace_log_start());
    assert(!workspace_log_ecam_snapshot()); /* Periodic writer must be quiesced. */
    assert(workspace_log_quiesce(5000));
    busy=true;assert(!workspace_log_ecam_snapshot());busy=false;
    assert(workspace_log_ecam_snapshot() && ecam_saved && strstr(path,"/ecam.log"));
    assert(workspace_log_window_snapshot() && window_saved && strstr(path,"/window.log"));
    old_writes=writes;assert(!workspace_log_window_snapshot() && writes==old_writes);
    old_writes=writes;assert(!workspace_log_ecam_snapshot() && writes==old_writes);
    assert(workspace_log_stop());
    assert(workspace_log_start() && !ecam_saved && workspace_log_quiesce(5000));
    corrupt=true;assert(!workspace_log_ecam_snapshot() && !ecam_saved && !healthy);corrupt=false;
    old_writes=writes;assert(!workspace_log_ecam_snapshot() && writes==old_writes);
    /* A seven-second write must not turn a ten-second capture cadence into
     * seventeen seconds and overwrite startup records in the 64KiB tail. */
    count=0;enabled=busy=false;mounted=io_ok=sync_ok=true;expected_capacity=32;save_tick_delay=0;
    assert(workspace_log_start());ticks=due;uint64_t capture_start=ticks;
    save_tick_delay=700;periodic_attempt();
    assert(healthy && enabled && !busy && ticks==capture_start+700 && due==capture_start+1000 && !ready(NULL));
    ticks=due;save_tick_delay=0;periodic_attempt();assert(due==ticks+1000 && enabled);
    capture_start=ticks=due;save_tick_delay=1500;periodic_attempt();
    assert(ticks==capture_start+1500 && due==ticks+1000 && !ready(NULL));
    save_tick_delay=0;io_ok=false;ticks=due;periodic_attempt();assert(!healthy && !enabled && !busy);io_ok=true;
    assert(periodic_deadline(UINT64_MAX-100,UINT64_MAX-1)==UINT64_MAX);
    puts("workspace-log-cadence: PASS original1000ticks/64KiB/two-slot bounds, delayed-write drift prevented, overrun skips catch-up, failure stops");
    puts("workspace-log: PASS gated writes/readback, collision-safe sessions, periodic readiness, open-file refusal, corruption/sync failure, stop before unmount");
}

void process_capture_write_snapshot(struct capture_write_snapshot *out) {*out=(struct capture_write_snapshot){0};}

void sync_timing_snapshot(struct sync_timing_snapshot *out){*out=sync_fixture;}
void sync_commit_wait_snapshot(struct commit_wait_snapshot *out){*out=(struct commit_wait_snapshot){0};}

void scheduler_print_launch_trace(void){assert(!depth);launch_trace_prints++;}
