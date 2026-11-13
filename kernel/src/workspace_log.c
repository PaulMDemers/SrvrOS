#include <srvros/sync_timing.h>
#include <srvros/commit_wait_timing.h>
/* Ordinary scheduled worker: filesystem work uses the storage service gate. */
#include <srvros/workspace_log.h>
#include <srvros/capture_write_timing.h>
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
static char directory[128],path[160],snapshot[65536];
static bool enabled,started,busy,healthy;
static unsigned checkpoint;
static bool retain_checkpoints;
static bool ecam_saved,window_saved;
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
/* Copy fixed diagnostic metadata under its own IRQ exclusion, then emit only
 * from this ordinary logger, before entering filesystem I/O. */
static void report_commit_wait(uint64_t at,const struct commit_wait_snapshot *s);
static void capture_write_checkpoint(bool final) {
    struct capture_write_snapshot s;process_capture_write_snapshot(&s);if(!s.enabled)return;
    uint64_t at=timer_monotonic_us();
    if(final) {
        struct commit_wait_snapshot wait;sync_commit_wait_snapshot(&wait);
        report_commit_wait(at,&wait);
    }
    console_printf("capture-write-snapshot: version=2 snapshot_us=%u enabled=%u invalid=%u dropped=%u final=%u\n",at,s.enabled,s.invalid,s.dropped,(uint64_t)final);
    for(unsigned i=0;i<CAPTURE_WRITE_RECORDS;i++) {
        const struct capture_write_record *r=&s.records[i];if(!r->pid)continue;
        console_printf("capture-write-record: version=2 snapshot_us=%u pid=%u first_us=%u last_us=%u retired=%u attempts=%u calls=%u requested=%u bytes=%u failures=%u growths=%u invalid=%u fast_calls=%u fast_requested=%u fast_bytes=%u fast_failures=%u fast_max_request=%u fast_max_bytes=%u final=%u\n",
            at,r->pid,r->first_us,r->last_us,r->retired,r->attempts,r->calls,r->requested,r->bytes,r->failures,r->growths,r->invalid,r->fast_calls,r->fast_requested,r->fast_bytes,r->fast_failures,r->fast_max_request,r->fast_max_bytes,(uint64_t)final);
        const struct capture_gate_summary *g=&r->gate;
        for(unsigned kind=0;kind<2;kind++) {
            const struct capture_gate_counts *c=kind?&g->worst.counts:&g->total;
            console_printf("capture-write-gate: version=1 snapshot_us=%u pid=%u kind=%u active=%u calls=%u wall_us=%u owned_us=%u scopes=%u blocked_us=%u ready_us=%u running_us=%u blocks=%u dispatches=%u exclusions=%u invalid=%u final=%u\n",
                at,r->pid,(uint64_t)kind,g->active,c->calls,c->wall_us,c->owned_us,c->scopes,c->blocked_us,c->ready_us,c->running_us,c->blocks,c->dispatches,c->exclusions,c->invalid,(uint64_t)final);
        }
        const struct capture_gate_owner *o=&g->worst.owner;
        console_printf("capture-write-gate-owner: version=%u snapshot_us=%u pid=%u start_us=%u end_us=%u thread=%u owner_pid=%u syscall=%u scope_start_us=%u scope_end_us=%u overlap_start_us=%u overlap_end_us=%u final=%u\n",
            (uint64_t)(final?2:1),at,r->pid,g->worst.start_us,g->worst.end_us,o->thread,o->pid,o->syscall,o->scope_start_us,o->scope_end_us,o->overlap_start_us,o->overlap_end_us,(uint64_t)final);
        /* Preserve baseline periodic traffic and its bounded receipt coverage.
         * The copied contexts survive worker retirement; emit the complete
         * extension only at final, without repeating it during live stress. */
        if(final) {
            console_printf("capture-append-wait: version=1 snapshot_us=%u pid=%u calls=%u wall_us=%u max_us=%u wakes=%u overlap_ready=%u final=1\n",
                at,r->pid,r->append_wait_calls,r->append_wait_us,r->append_wait_max_us,r->append_wait_wakes,r->append_wait_overlap);
            const struct sync_gate_context *q=&o->sync;
            console_printf("capture-write-gate-sync: version=1 snapshot_us=%u pid=%u state=%u invalid=%u owner_pid=%u sync_id=%u phase=%u slot=%u identity=%u inode=%u size=%u content_start_us=%u final=%u\n",
                at,r->pid,q->state,q->invalid,q->pid,q->id,q->phase,q->slot,q->identity,q->inode,q->size,q->content_start_us,(uint64_t)final);
            for(unsigned role=0;role<SYNC_CONTENT_ROLES;role++) {
                const struct sync_content_io *p=&q->io[role];
                console_printf("capture-write-gate-sync-io: version=1 snapshot_us=%u pid=%u role=%u reads=%u read_bytes=%u read_us=%u writes=%u write_bytes=%u write_us=%u flushes=%u flush_us=%u failures=%u final=%u\n",
                    at,r->pid,(uint64_t)role,p->reads,p->read_bytes,p->read_us,p->writes,p->write_bytes,p->write_us,p->flushes,p->flush_us,p->failures,(uint64_t)final);
            }
        }
        for(unsigned j=0;j<CAP_WRITE_SITE_COUNT;j++) {
            const struct capture_write_sample *p=&r->site[j];
            console_printf("capture-write-sample: version=2 snapshot_us=%u pid=%u site=%u count=%u total_us=%u max_us=%u final=%u\n",
                at,r->pid,(uint64_t)j,p->count,p->total_us,p->max_us,(uint64_t)final);
        }
    }
}
/* Cached queries only: logging must not issue codec commands or sensor I/O. */
static void device_checkpoint(void) {
    scheduler_print_io_owners();
    exfat_print_costs();
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
/* Keep the legacy emitter byte-identical for dedicated-profile tail proofs.
 * Optional sync diagnostics have a separate prefix and bounded emitter. */
static void report_sync_transfers(const char *scope,uint64_t id,const struct usb_storage_timing *t) {
    for(unsigned phase=0;phase<3;phase++) {
        const struct usb_storage_phase_timing *p=&t->phase[phase];
        console_printf("sync-usb-timing: version=2 scope=%s id=%u phase=%u calls=%u callback_us=%u max_us=%u over20ms=%u over100ms=%u over1s=%u observed_calls=%u observe_us=%u resume_us=%u observe_max_us=%u resume_max_us=%u service_checks=%u service_samples=%u service_gap_max_us=%u completion_gap_max_us=%u valid=%u\n",
            scope,id,(uint64_t)phase,t->calls[phase],t->us[phase],p->max_us,
            p->over20ms,p->over100ms,p->over1s,p->observed_calls,p->observe_us,p->resume_us,
            p->observe_max_us,p->resume_max_us,p->service_checks,p->service_samples,
            p->service_gap_max_us,p->completion_gap_max_us,(uint64_t)t->valid);
    }
    const struct usb_storage_csw_timing *c=&t->csw;
    const struct usb_bulk_timing *b=&c->sample;
    console_printf("sync-usb-command: version=1 scope=%s id=%u checkpoint_calls=%u checkpoint_us=%u start_us=%u end_us=%u opcode=%u command_bytes=%u request_bytes=%u actual=%u ok=%u submitted=%u observed=%u resumed=%u seen=%u service_valid=%u service_checks=%u service_samples=%u service_gap_max_us=%u completion_gap_us=%u\n",
        scope,id,t->checkpoint_calls,t->checkpoint_us,c->start_us,c->end_us,
        c->opcode,c->command_bytes,c->request_bytes,c->actual,c->ok,
        b->submitted,b->observed,b->resumed,(uint64_t)b->seen,(uint64_t)b->service_valid,
        b->service_checks,b->service_samples,b->service_gap_max_us,b->completion_gap_us);
}
/* One command window copied with the exact selected content, never an
 * independent command maximum from another content/sync. */
static void report_sync_bot(uint64_t at,uint64_t pid,uint64_t id,uint64_t identity,
    const struct usb_storage_content_timing *t) {
    const struct usb_storage_command_timing empty={0};
    const struct usb_storage_command_timing *q=t?&t->command:&empty;
    console_printf("sync-bot-command: version=1 snapshot_us=%u pid=%u id=%u identity=%u role=%u csw_start_us=%u csw_end_us=%u valid=%u complete=%u ok=%u start_us=%u end_us=%u checkpoint_calls=%u checkpoint_us=%u checkpoint_first_us=%u checkpoint_last_us=%u final=1\n",at,pid,id,identity,q->role,q->csw_start_us,q->csw_end_us,q->valid,q->complete,q->ok,q->start_us,q->end_us,q->checkpoint_calls,q->checkpoint_us,q->checkpoint_first_us,q->checkpoint_last_us);
    if(!q->complete || q->role>=USB_STORAGE_CONTENT_ROLES)return;
    for(unsigned phase=0;phase<3;phase++) {
        const struct usb_storage_command_phase *p=&q->phase[phase];
        console_printf("sync-bot-phase: version=1 snapshot_us=%u pid=%u id=%u identity=%u role=%u csw_start_us=%u csw_end_us=%u phase=%u calls=%u requested=%u actual=%u us=%u first_us=%u last_us=%u final=1\n",at,pid,id,identity,q->role,q->csw_start_us,q->csw_end_us,(uint64_t)phase,p->calls,p->requested,p->actual,p->us,p->first_us,p->last_us);
    }
    const struct usb_bulk_timing *b=&t->roles[q->role].worst.sample;
    console_printf("sync-bot-service: version=1 snapshot_us=%u pid=%u id=%u identity=%u role=%u csw_start_us=%u csw_end_us=%u empty_checks=%u event_checks=%u last_empty_sample_us=%u final=1\n",at,pid,id,identity,q->role,q->csw_start_us,q->csw_end_us,b->empty_checks,b->event_checks,b->last_empty_sample_us);
}
static void report_sync_commands(uint64_t at,const struct sync_timing_record *r) {
    const struct sync_content_call *c=r?&r->worst.content.worst:NULL;
    const struct usb_storage_content_timing *t=c?&c->commands:NULL;
    uint64_t pid=r?r->pid:0,id=r?r->worst.start_us:0,identity=c?c->identity:0;
    console_printf("sync-command-snapshot: version=1 snapshot_us=%u pid=%u id=%u identity=%u valid=%u active=%u invalid=%u final=1\n",at,pid,id,identity,t?t->valid:1,t?t->active:0,t?t->invalid:0);
    report_sync_bot(at,pid,id,identity,t);
    if(!t)return;
    for(unsigned role=0;role<USB_STORAGE_CONTENT_ROLES;role++) {
        const struct usb_storage_content_role *q=&t->roles[role];
        console_printf("sync-command-role: version=1 snapshot_us=%u pid=%u id=%u identity=%u role=%u reads=%u read_bytes=%u writes=%u write_bytes=%u flushes=%u other=%u failed=%u csws=%u csw_us=%u over100ms=%u write512_commands=%u write_csw_us=%u write512_csw_us=%u write_over100ms=%u write512_over100ms=%u final=1\n",at,pid,id,identity,(uint64_t)role,q->read_commands,q->read_bytes,q->write_commands,q->write_bytes,q->flush_commands,q->other_commands,q->failed_commands,q->csw_calls,q->csw_us,q->over100ms,q->write512_commands,q->write_csw_us,q->write512_csw_us,q->write_over100ms,q->write512_over100ms);
        if(!q->csw_calls)continue;
        const struct usb_storage_csw_timing *w=&q->worst;
        const struct usb_bulk_timing *b=&w->sample;
        console_printf("sync-command-worst: version=1 snapshot_us=%u pid=%u id=%u identity=%u role=%u start_us=%u end_us=%u opcode=%u command_bytes=%u request_bytes=%u actual=%u ok=%u submitted=%u observed=%u resumed=%u seen=%u service_valid=%u service_checks=%u service_samples=%u service_gap_max_us=%u completion_gap_us=%u final=1\n",at,pid,id,identity,(uint64_t)role,w->start_us,w->end_us,w->opcode,w->command_bytes,w->request_bytes,w->actual,w->ok,b->submitted,b->observed,b->resumed,(uint64_t)b->seen,(uint64_t)b->service_valid,b->service_checks,b->service_samples,b->service_gap_max_us,b->completion_gap_us);
    }
}
/* Correlate only final retained worst calls. One selected file-phase owner
 * keeps role detail bounded; all PID summaries permit independent selection. */
static void report_sync_content(uint64_t at,const struct sync_timing_snapshot *s) {
    const struct sync_timing_record *selected=NULL;
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        const struct sync_timing_record *r=&s->records[i];if(!r->pid)continue;
        const struct sync_timing_call *c=&r->worst;
        console_printf("sync-content-summary: version=1 snapshot_us=%u pid=%u id=%u calls=%u bytes=%u failures=%u final=1\n",at,r->pid,c->start_us,c->content.calls,c->content.bytes,c->content.failures);
        if(c->content.calls && (!selected || c->files_end_us-c->start_us>selected->worst.files_end_us-selected->worst.start_us))selected=r;
    }
    console_printf("sync-content-selection: version=1 snapshot_us=%u pid=%u id=%u final=1\n",at,selected?selected->pid:0,selected?selected->worst.start_us:0);
    report_sync_commands(at,selected);
    if(!selected)return;
    const struct sync_content_call *c=&selected->worst.content.worst;
    console_printf("sync-content-worst: version=%u snapshot_us=%u pid=%u id=%u slot=%u identity=%u inode=%u size=%u start_us=%u end_us=%u ok=%u dirty_after=%u final=1\n",(uint64_t)(c->prefix.valid?2:1),at,selected->pid,selected->worst.start_us,c->slot,c->identity,c->inode,c->size,c->start_us,c->end_us,c->ok,c->dirty_after);
    if(c->prefix.valid)console_printf("sync-content-prefix: version=1 snapshot_us=%u pid=%u id=%u identity=%u start_generation=%u end_generation=%u live_size=%u committed_size=%u append_calls=%u append_bytes=%u final=1\n",
        at,selected->pid,selected->worst.start_us,c->identity,c->prefix.start_generation,c->prefix.end_generation,c->prefix.live_size,c->prefix.committed_size,c->prefix.append_calls,c->prefix.append_bytes);
    for(unsigned i=0;i<SYNC_CONTENT_ROLES;i++) {
        const struct sync_content_io *p=&c->io[i];
        console_printf("sync-content-io: version=1 snapshot_us=%u pid=%u id=%u role=%u reads=%u read_bytes=%u read_us=%u writes=%u write_bytes=%u write_us=%u flushes=%u flush_us=%u failures=%u final=1\n",at,selected->pid,selected->worst.start_us,(uint64_t)i,p->reads,p->read_bytes,p->read_us,p->writes,p->write_bytes,p->write_us,p->flushes,p->flush_us,p->failures);
    }
}
/* One global worst prelude only: fixed final tail budget, no live log traffic.
 * The selected content is copied at its own completion, not from sync's
 * independent worst. Full-call USB clocks can extend before wait entry. */
static bool commit_sum(uint64_t *out,uint64_t value) {
    if(*out>UINT64_MAX-value)return false;
    *out+=value;return true;
}
static void report_commit_wait(uint64_t at,const struct commit_wait_snapshot *s) {
    const struct commit_wait_record *w=&s->worst;
    const struct sync_content_call *c=&w->content;
    console_printf("capture-commit-wait: version=2 snapshot_us=%u enabled=%u invalid=%u active=%u waits=%u pid=%u start_us=%u end_us=%u target=%u base=%u live=%u request=%u borrowed=%u retry=%u open_at_end=%u calls=%u overlap_us=%u parent_pid=%u parent_id=%u identity=%u slot=%u size=%u content_start=%u content_end=%u ok=%u dirty_after=%u overlap_start=%u overlap_end=%u owner_calls=%u owned_us=%u final=1\n",at,s->enabled,s->invalid,s->active,s->waits,w->pid,w->start_us,w->end_us,w->target_identity,w->target_base,w->target_size,w->request_bytes,w->borrowed,w->retry,w->open_at_end,w->calls,w->overlap_us,w->parent_pid,w->parent_id,c->identity,c->slot,c->size,c->start_us,c->end_us,c->ok,c->dirty_after,w->overlap_start_us,w->overlap_end_us,w->owner_calls,w->owned_us);
    const struct commit_owner_timing *o=&w->owner;
    console_printf("capture-commit-owner: version=2 snapshot_us=%u pid=%u seq=%u thread=%u owner_pid=%u syscall=%u name=%s start_us=%u end_us=%u released=%u overlap_start=%u overlap_end=%u csws=%u csw_us=%u status_overlap_us=%u over100ms=%u write512=%u write512_us=%u write16k=%u write16k_us=%u final=1\n",at,w->pid,o->seq,o->thread,o->pid,o->syscall,o->name[0]?o->name:"none",o->start_us,o->end_us,o->released,o->overlap_start,o->overlap_end,o->csws,o->csw_us,o->csw_overlap_us,o->longs,o->write512,o->write512_us,o->write16k,o->write16k_us);
    if(o->csws) {
        const struct usb_storage_csw_timing *v=&o->status;
        const struct usb_bulk_timing *b=&v->sample;
        console_printf("capture-commit-status: version=2 snapshot_us=%u pid=%u tag=%u start_us=%u end_us=%u opcode=%u command_bytes=%u requested=%u actual=%u bulk_ok=%u submitted=%u observed=%u resumed=%u seen=%u service_valid=%u checks=%u samples=%u service_gap=%u completion_gap=%u empty_checks=%u event_checks=%u last_empty=%u final=1\n",at,w->pid,o->tag,v->start_us,v->end_us,v->opcode,v->command_bytes,v->request_bytes,v->actual,v->ok,b->submitted,b->observed,b->resumed,(uint64_t)b->seen,(uint64_t)b->service_valid,b->service_checks,b->service_samples,b->service_gap_max_us,b->completion_gap_us,b->empty_checks,b->event_checks,b->last_empty_sample_us);
    }
    {
        const struct commit_command_timing *q=&o->command;
        const struct usb_storage_command_timing *t=&q->timing;
        console_printf("capture-owner-command: version=1 snapshot_us=%u pid=%u available=%u phases=%u tag=%u opcode=%u lba=%u blocks=%u sector=%u flags=%u valid=%u complete=%u ok=%u role=%u start_us=%u end_us=%u csw_start=%u csw_end=%u cbw_calls=%u cbw_us=%u data_calls=%u data_req=%u data_actual=%u data_us=%u csw_calls=%u csw_us=%u ck_calls=%u ck_us=%u final=1\n",at,w->pid,q->available,q->phases,q->available?q->tag:o->tag,q->opcode,q->lba,q->blocks,q->sector_size,q->flags,t->valid,t->complete,t->ok,t->role,t->start_us,t->end_us,t->csw_start_us,t->csw_end_us,t->phase[0].calls,t->phase[0].us,t->phase[1].calls,t->phase[1].requested,t->phase[1].actual,t->phase[1].us,t->phase[2].calls,t->phase[2].us,t->checkpoint_calls,t->checkpoint_us);
    }
    if(!c->start_us)return;
    uint64_t io_us=0,sidecar_us=0,failures=0,csws=0,csw_us=0,longs=0;
    bool valid=true;
    for(unsigned i=0;i<SYNC_CONTENT_ROLES;i++) {
        const struct sync_content_io *p=&c->io[i];
        valid=commit_sum(&io_us,p->read_us) && commit_sum(&io_us,p->write_us) &&
            commit_sum(&io_us,p->flush_us) && commit_sum(&failures,p->failures) && valid;
        if(i>=4)valid=commit_sum(&sidecar_us,p->read_us) && commit_sum(&sidecar_us,p->write_us) &&
            commit_sum(&sidecar_us,p->flush_us) && valid;
    }
    const struct usb_storage_content_timing *t=&c->commands;
    for(unsigned i=0;i<USB_STORAGE_CONTENT_ROLES;i++) {
        valid=commit_sum(&csws,t->roles[i].csw_calls) && commit_sum(&csw_us,t->roles[i].csw_us) &&
            commit_sum(&longs,t->roles[i].over100ms) && valid;
    }
    const struct usb_storage_content_role *payload=&t->roles[2];
    console_printf("capture-commit-io: version=2 snapshot_us=%u pid=%u valid=%u io_us=%u payload_us=%u alloc_read_us=%u alloc_write_us=%u prefix_us=%u sidecar_us=%u failures=%u usb_valid=%u usb_invalid=%u usb_active=%u csws=%u csw_us=%u over100ms=%u payload_commands=%u payload_bytes=%u payload_csw_us=%u payload_long=%u final=1\n",at,w->pid,(uint64_t)valid,io_us,c->io[2].write_us,c->io[1].read_us,c->io[1].write_us,c->io[3].read_us,sidecar_us,failures,t->valid,t->invalid,t->active,csws,csw_us,longs,payload->write_commands,payload->write_bytes,payload->csw_us,payload->over100ms);

}
static void sync_checkpoint(bool final) {
    struct sync_timing_snapshot s;sync_timing_snapshot(&s);if(!s.enabled)return;
    exfat_print_sidecar_equality();
    uint64_t at=timer_monotonic_us();
    console_printf("sync-snapshot: version=1 snapshot_us=%u enabled=%u active=%u invalid=%u dropped=%u final=%u\n",at,s.enabled,s.active,s.invalid,s.dropped,(uint64_t)final);
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        const struct sync_timing_record *r=&s.records[i];if(!r->pid)continue;
        const struct sync_timing_call *c=&r->worst;
        console_printf("sync-record: version=1 snapshot_us=%u pid=%u calls=%u total_us=%u files_us=%u devices_us=%u errors=%u conflicts=%u invalid=%u final=%u\n",at,r->pid,r->calls,r->total_us,r->files_us,r->devices_us,r->errors,r->conflicts,r->invalid,(uint64_t)final);
        /* Repeated verbose USB rows can evict app receipts from the fixed
         * log tail. Counters show progress; complete retained detail is final. */
        if(!final)continue;
        console_printf("sync-worst: version=1 snapshot_us=%u pid=%u start_us=%u files_end_us=%u end_us=%u file_failed=%u device_failed=%u files_usb_failures=%u devices_usb_failures=%u final=%u\n",at,r->pid,c->start_us,c->files_end_us,c->end_us,c->file_failed,c->device_failed,c->usb[0].failures,c->usb[1].failures,(uint64_t)final);
        report_sync_transfers("files",c->start_us,&c->usb[0]);
        report_sync_transfers("devices",c->start_us,&c->usb[1]);
    }
    if(final)report_sync_content(at,&s);
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
    sync_checkpoint(name[0]=='e' && name[1]=='x' && name[2]=='i' && name[3]=='t' && name[4]=='.');
    capture_write_checkpoint(name[0]=='e' && name[1]=='x' && name[2]=='i' && name[3]=='t' && name[4]=='.');
    device_checkpoint();
    uint64_t snapshot_us=timer_monotonic_us();
    console_printf("workspace-log: snapshot=%s monotonic_us=%u total_before=%u tail_limit=65536\n",name,snapshot_us,bootlog_total());
    uint64_t total,count=bootlog_snapshot(snapshot,sizeof(snapshot),&total),flags;
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
/* Preserve the declared ten-second capture period instead of adding the
 * preceding storage latency to every interval. Overruns skip missed periods;
 * they never queue catch-up writes or spin on a past deadline. */
static uint64_t periodic_deadline(uint64_t start,uint64_t finished) {
    uint64_t base=finished>=start && finished-start<1000?start:finished;
    return base<=UINT64_MAX-1000?base+1000:UINT64_MAX;
}
static void periodic_attempt(void) {
    uint64_t start=timer_ticks();busy=true;
    healthy=periodic_save();
    if(!healthy)enabled=false;
    due=periodic_deadline(start,timer_ticks());busy=false;scheduler_wake_all(&wake);
}
static void worker(void *arg) {
    (void)arg;
    for(;;) {
        if(!enabled) { scheduler_wait(&wake,activated,NULL);continue; }
        if(!ready(NULL)) { scheduler_wait_timeout(&wake,ready,NULL,due);continue; }
        periodic_attempt();
    }
}
static bool start_log(bool retain) {
    if(enabled || busy || !workspace_active()) return false;
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
    uint64_t capture_start=timer_ticks();
    retain_checkpoints=retain;checkpoint=0;ecam_saved=window_saved=false;healthy=save("boot.log");
    if(!healthy) return false;
    if(!started) started=scheduler_spawn("workspace-log",worker,NULL);
    enabled=started;due=periodic_deadline(capture_start,timer_ticks());scheduler_wake_all(&wake);
    if(!started) console_write("workspace-log: worker unavailable; boot/exit snapshots only\n");
    return started;
}
bool workspace_log_start(void) { return start_log(false); }
bool workspace_log_start_capture(void) { return start_log(true); }
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
const char *workspace_log_directory(void){return directory[0]?directory:NULL;}
bool workspace_log_ecam_snapshot(void) {
 if(enabled || busy || ecam_saved || !directory[0] || !workspace_active() || !healthy)return false;
 healthy=save("ecam.log");ecam_saved=healthy;return healthy;
}

bool workspace_log_window_snapshot(void){
 if(enabled || busy || window_saved || !directory[0] || !workspace_active() || !healthy)return false;
 healthy=save("window.log");window_saved=healthy;return healthy;
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
