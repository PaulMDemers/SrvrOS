#include "battery_bounded_policy.h"
/* Bounded battery/AC diagnostic. Full runtime, linked separately from the
 * snapshot probe and the desktop. CPU0 lifecycle owner and one EC/AML worker. */
#include "battery_platform.h"
#include "battery_transition.h"
#include "battery_reset_policy.h"
#include "battery_failure_display.h"
#include "battery_recovery_resources.h"
#include "battery_query_receipt.h"
#include "battery_event_accounting.h"
#include "work_worker.h"
#include "sci_owner.h"
#include "sci_observe.h"
#include "ec_query_budget.h"
#include "ec_query_rearm.h"
#include <srvros/battery_diagnostic.h>
#include <srvros/desktop_session.h>
#include <srvros/battery_cache.h>
#include <srvros/battery_control.h>
#include <srvros/arch.h>
#include <srvros/workspace.h>
#include <srvros/workspace_image.h>
#include <srvros/workspace_log.h>
#include <srvros/usb_storage.h>
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/heap.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/semaphore.h>
#include <srvros/sbs_sample.h>
#include <srvros/timer.h>
#include <uacpi/uacpi.h>
#include <uacpi/context.h>
#include <uacpi/internal/context.h>
#include <uacpi/internal/types.h>
#include <uacpi/namespace.h>
#include <uacpi/osi.h>
#include <uacpi/notify.h>
#include <uacpi/opregion.h>
#include <uacpi/event.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
#include "../../../../kernel/src/arch/x86_64/ioapic.h"
#include "../../../../kernel/src/arch/x86_64/lapic.h"
#define HEAP_LIMIT (8u*1024u*1024u)
struct sci_counts {
    uint64_t delivered,handled,unclaimed,in_flight,first_unclaimed_us,last_unclaimed_us;
};
static struct {
    struct battery_platform platform;
    struct acpi_work_worker worker;
    struct acpi_work_item operation,query;
    struct sci_owner sci;
    uacpi_interrupt_handler irq_fn;
    uacpi_handle irq_context;
    struct ec_transport transport;
    struct sbs_sampler sampler;
    struct ec_query_budget budget;
    struct battery_event_accounting events;
    uint64_t smbus_invalid_through;
    bool smbus_refresh;
    struct battery_query_receipt query_receipt;
    uacpi_namespace_node *node,*handler_node;
    uint64_t heap,peak,errors,denied,interrupts,queries,notifications,attached,detached;
    struct sci_observer sci_observer;
    struct sci_query_state query_observer;
    struct sci_counts sci_counts[3]; /* setup, active, retirement; CPU0 only */
    bool engaged,loading,stopping,ready,done,ok,region,notify,gpe,lease,query_scheduled;
    bool ac_present,ac_coherent,transition_mode,service_mode;
    uint64_t ac_sampled_us;
    const char *setup_stage;
    uint64_t setup_started_us;
} runtime;
enum runtime_owner { OWNER_NONE,OWNER_DIAGNOSTIC,OWNER_SERVICE };
static enum runtime_owner owner;
static bool auto_return;
static bool recovery_study;
static bool query_study,query_capture,smbus_study,smbus_capture,smbus_files,smbus_controls;
static uint64_t smbus_workspace_hold;
static bool mounted_home;
/* Continuous sessions require an explicit command, capture or Home boot profile. */
static bool smbus_transitions;
static bool continuous_home, desktop_home, desktop_capture, desktop_transitions, desktop_session;
static uint64_t home_session_events;
static bool compact_smbus_records(void) { return smbus_files || (mounted_home && smbus_controls); }
static struct battery_service_status smbus_files_snapshot;
static struct block_device *query_capture_volume;
static bool query_tracking(void) { return query_study || query_capture || mounted_home; }
static bool capture_storage_parked(void) {
    return query_capture && query_capture_volume && auto_return &&
        workspace_parked_device()==query_capture_volume && !workspace_active() &&
        !workspace_image_busy() && !scheduler_io_active();
}
static struct block_device *smbus_capture_volume;
static bool smbus_capture_parked(void) {
    return smbus_capture && smbus_capture_volume && auto_return &&
        workspace_parked_device()==smbus_capture_volume && !workspace_active() &&
        (smbus_files?workspace_image_ready():!workspace_image_busy() && !scheduler_io_active());
}
/* An explicit native diagnostic owns the mount through normal runtime return.
 * This grants no fatal reset, general AML unwind, or desktop-startup capability. */
static bool bounded_home, bounded_home_session;
static bool air61_home_owned(void) {
    return runtime.platform.query_policy==BATTERY_QUERY_AIR61 && runtime.platform.bounded_samples_only &&
        runtime.platform.smbus_capture_profile && battery_bounded_home_entry(runtime.transition_mode,
        runtime.service_mode,owner==OWNER_SERVICE,((bounded_home&&!smbus_controls) ||
            (battery_bounded_home_session(bounded_home_session,smbus_controls,continuous_home,
                desktop_home,desktop_capture,desktop_session)))
            &&!smbus_capture&&!smbus_study,
        mounted_home&&workspace_mount_hold_valid(smbus_workspace_hold),auto_return,workspace_image_busy());
}
static bool mounted_home_owned(void) {
    return mounted_home && workspace_mount_hold_valid(smbus_workspace_hold) && !workspace_image_busy();
}
static bool native_query_return_scope(void) {
    return air61_home_owned() || (runtime.platform.query_policy==BATTERY_QUERY_AIR72 &&
        (capture_storage_parked() || (mounted_home_owned() && runtime.platform.smbus_capture_profile)));
}
static bool air61_smbus_owned(void) {
    if(runtime.platform.query_policy!=BATTERY_QUERY_AIR61 || !runtime.platform.bounded_samples_only ||
       !runtime.platform.smbus_capture_profile)return false;
    bool capture=smbus_capture&&!smbus_controls&&!mounted_home&&!smbus_study;
    if(smbus_files)return battery_bounded_files_entry(runtime.transition_mode,runtime.service_mode,
        owner==OWNER_SERVICE,capture&&!smbus_transitions,auto_return,smbus_capture_parked(),
        workspace_active(),workspace_image_ready());
    return battery_bounded_capture_entry(runtime.transition_mode,runtime.service_mode,owner==OWNER_SERVICE,
        capture,auto_return,smbus_capture_parked(),workspace_active(),workspace_image_busy(),scheduler_io_active());
}
static bool smbus_owned(void) {
    return air61_smbus_owned() || air61_home_owned() || (smbus_study && runtime.platform.model_smbus_queries && battery_platform_workspace_study_allowed() &&
        (smbus_workspace_hold?workspace_mount_hold_valid(smbus_workspace_hold):!workspace_active())) ||
        ((smbus_capture_parked() || mounted_home_owned()) && runtime.platform.smbus_capture_profile &&
         runtime.platform.query_policy==BATTERY_QUERY_AIR72);
}
static struct {
    bool study,pending,completed,callback_study,callback_active,callback_failed,target_study,target_pending,target_handoff;
    uint64_t interrupted,peer,peer_complete,callback_entries,callback_faults,callback_locks,callback_depth;
} recovery_irq;
static struct {
    struct battery_cache cache;
    struct scheduler_wait_queue wake,progress;
    void *rsdp;
    struct limine_memmap_response *map;
    uint64_t identity;
    bool running,stop,quarantined,finishing;
} service;
static uint64_t irq_save(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static void irq_restore(uint64_t f){if(f&(1ull<<9))__asm__ volatile("sti":::"memory");}
static void query_phase(uint8_t phase) {
    uint64_t f=irq_save();
    sci_query_phase(&runtime.query_observer,phase,timer_monotonic_us());
    irq_restore(f);
}
static void sci_report(const char *snapshot) {
    struct sci_counts counts[3];
    struct sci_observer observer;
    uint64_t f=irq_save(),epoch=owner==OWNER_SERVICE?service.cache.value.epoch:0;
    for(unsigned i=0;i<3;i++)counts[i]=runtime.sci_counts[i];
    observer=runtime.sci_observer;
    irq_restore(f);
    static const char *phases[]={"setup","active","retire"};
    for(unsigned i=0;i<3;i++)
        console_printf("battery-sci: snapshot=%s epoch=%u phase=%s delivered=%u handled=%u unclaimed=%u in_flight=%u first_unclaimed_us=%u last_unclaimed_us=%u\n",
            snapshot,epoch,phases[i],counts[i].delivered,counts[i].handled,
            counts[i].unclaimed,counts[i].in_flight,counts[i].first_unclaimed_us,counts[i].last_unclaimed_us);
    console_printf("battery-sci-observe: snapshot=%s epoch=%u total=%u retained=%u overwritten=%u ec_mask=%x\n",
        snapshot,epoch,observer.total,(uint64_t)observer.count,observer.total-observer.count,
        (uint64_t)runtime.platform.ec.gpe_mask);
    for(unsigned i=0;i<observer.count;i++) {
        const struct sci_observation *r=sci_observe_row(&observer,i);
        console_printf("battery-sci-event: snapshot=%s epoch=%u seq=%u phase=%u handled=%u at_us=%u duration_us=%u previous_valid=%u previous_handled=%u gap_us=%u query_before=%u query_after=%u valid=%x fixed_status=%x fixed_enable=%x ec_status=%x ec_enable=%x ec_raw_status=%x ec_raw_enable=%x\n",
            snapshot,epoch,r->sequence,(uint64_t)r->phase,(uint64_t)r->handled,r->at_us,r->duration_us,
            (uint64_t)r->previous_valid,(uint64_t)r->previous_handled,r->previous_gap_us,
            (uint64_t)r->query_before,(uint64_t)r->query_after,(uint64_t)r->valid,
            (uint64_t)r->fixed_status,(uint64_t)r->fixed_enable,(uint64_t)r->ec_status,
            (uint64_t)r->ec_enable,(uint64_t)r->ec_raw_status,(uint64_t)r->ec_raw_enable);
        for(unsigned j=0;j<3;j++) {
            const struct sci_controller_observation *c=j==2?&r->post_eoi:j?&r->after:&r->before;
            console_printf("battery-sci-controller: snapshot=%s epoch=%u seq=%u point=%s valid=%u vector=%u gsi=%u lapic_bits=%u lint0=%x route_low=%x route_high=%x pic_masks=%x\n",
                snapshot,epoch,r->sequence,j==2?"post_eoi":j?"after":"before",(uint64_t)c->valid,
                (uint64_t)c->vector,(uint64_t)c->gsi,(uint64_t)c->lapic_bits,
                (uint64_t)c->lint0,(uint64_t)c->route_low,(uint64_t)c->route_high,(uint64_t)c->pic_masks);
        }
        const struct sci_query_snapshot *q=&r->query_entry;
        console_printf("battery-sci-query: snapshot=%s epoch=%u seq=%u valid=%u cycle=%u phase=%u age_us=%u phase_age_us=%u transfers=%u code_valid=%u code=%u\n",
            snapshot,epoch,r->sequence,(uint64_t)q->valid,q->sequence,(uint64_t)q->phase,
            q->age_us,q->phase_age_us,q->transfers,(uint64_t)q->code_valid,(uint64_t)q->code);
        console_printf("battery-sci-return: snapshot=%s epoch=%u seq=%u observed=%u elapsed_us=%u query=%u\n",
            snapshot,epoch,r->sequence,(uint64_t)r->post_eoi_observed,r->post_eoi_elapsed_us,(uint64_t)r->query_post_eoi);
    }
}
static bool claim_diagnostic(void) {
    uint64_t f=irq_save();
    bool ok=owner==OWNER_NONE&&(f&(1ull<<9))&&scheduler_current_thread_id();
    if(ok)owner=OWNER_DIAGNOSTIC;
    irq_restore(f);return ok;
}
static void release_owner(void) {uint64_t f=irq_save();owner=OWNER_NONE;irq_restore(f);}
/* CPU0 scheduling predicate, read with IRQs masked. Platform validation and
 * page-table setup finish before engaged. The audited service callbacks use
 * kernel mappings/serialized allocators and never VFS or user memory. Keep both
 * owner watchdog and worker runnable through teardown and final owner bookkeeping;
 * an explicit finishing phase keeps the owner eligible after close. A new
 * lifetime clears that phase before admission. Other adapters and the
 * direct diagnostic retain ordinary-thread scheduling. See battery-service.md. */
static bool service_io_ready(void *arg) {
    (void)arg;return owner==OWNER_SERVICE && runtime.service_mode &&
        (runtime.engaged || service.finishing);
}
bool battery_diagnostic_auto_return(bool enabled) {
    uint64_t f=irq_save();
    bool ok=!enabled||battery_reset_allowed(true,workspace_active(),scheduler_io_active());
    if(ok)auto_return=enabled;
    irq_restore(f);return ok;
}
static void event_write(void *,uint16_t,uint8_t);
static uint8_t event_read(void *,uint16_t);
static void mask_sources(void) {
    uint64_t flags=irq_save();
    runtime.stopping=true;
    if(!runtime.engaged){irq_restore(flags);return;}
    outw(runtime.platform.events.resources.pm1_enable,0);
    for(unsigned b=0;b<2;b++)for(unsigned i=0;i<runtime.platform.events.resources.gpe[b].bytes;i++)
        event_write(NULL,runtime.platform.events.resources.gpe[b].enable+i,0);
    if(runtime.sci.phase!=SCI_OWNER_OFF)(void)sci_owner_mask(&runtime.sci);
    irq_restore(flags);
}
static __attribute__((noreturn)) void unsafe(const char *reason) {
    /* CPU0 containment is terminal: prevent either runtime thread from running
     * again while reporting and resetting. No filesystem work is permitted. */
    (void)irq_save();
    if(owner==OWNER_SERVICE) {
        service.cache.value.state=BATTERY_SERVICE_FAULTED;
        service.cache.value.error=BATTERY_SERVICE_CONTAINMENT;
    }
    /* Recheck with scheduling excluded: arming while parked is not a lease on
     * future filesystem ownership. Never try to flush from this fatal context. */
    bool mounted=workspace_active(),io=scheduler_io_active();
    bool reset=battery_reset_allowed(auto_return,mounted,io);
    mask_sources();
    /* The interrupted presenter cannot resume after terminal containment. */
    console_emergency();
    sci_report("fault");
    if(auto_return && !reset)
        console_printf("battery-reset: BLOCKED workspace=%u io=%u; live storage retained\n",(uint64_t)mounted,(uint64_t)io);
    console_printf("battery: progress setup=%s setup_elapsed_us=%u worker=%u completed=%u in_flight=%u\n",
        runtime.setup_stage?runtime.setup_stage:"not-started",
        runtime.setup_started_us?timer_monotonic_us()-runtime.setup_started_us:0,
        runtime.worker.identity,runtime.worker.completed,runtime.worker.work.in_flight);
    console_printf("battery: %s %s errors=%u denied=%u heap=%u ec_phase=%u ec_error=%u queries=%u; live state retained\n",
        reset?"FAILED":"HALTED",reason,runtime.errors,runtime.denied,runtime.heap,(uint64_t)runtime.transport.phase,
        (uint64_t)runtime.transport.error,runtime.queries);
    if(reset) {
        console_write("battery: contained fault; returning to Ubuntu without live-runtime cleanup; final report unavailable\n");
        console_write("battery-failure-display: photograph the reason and progress above; USB stays parked; reset in 30 seconds\n");
        uint64_t display_start=timer_monotonic_us();
        unsigned slices=battery_failure_display_wait(timer_delay_us);
        console_printf("battery-failure-display: slices=%u elapsed_us=%u clock_ok=%u; resetting\n",
            (uint64_t)slices,timer_monotonic_us()-display_start,(uint64_t)(slices==300));
        arch_reboot_unattended();
    }
    console_sync();__asm__ volatile("cli":::"memory");halt_forever();
}
static bool foreign_port(uint16_t port) {
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        const struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(h->mask&&(port==h->status||port==h->enable))return true;
    }
    return false;
}
static uint8_t filter_foreign_event(uint16_t port,bool write,uint8_t value) {
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++)
        value=battery_event_hold_filter(&runtime.platform.foreign_events[i],port,write,value);
    return value;
}
static uint8_t event_read(void *ctx,uint16_t port) {
    (void)ctx;
    struct battery_event_ops *io=&runtime.platform.foreign_io;
    if(foreign_port(port)&&io->read)return io->read(io->ctx,port);
    uint8_t value=0;
    if(runtime.platform.model_event && runtime.platform.model_event(port,false,&value))return value;
    return inb(port);
}
static void event_write(void *ctx,uint16_t port,uint8_t value) {
    (void)ctx;
    struct battery_event_ops *io=&runtime.platform.foreign_io;
    if(foreign_port(port)&&io->write)io->write(io->ctx,port,value);
    else if(!runtime.platform.model_event || !runtime.platform.model_event(port,true,&value))outb(port,value);
}
static const struct battery_event_ops event_ops={.read=event_read,.write=event_write};
static bool foreign_event_enabled(void) {
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        const struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(h->active&&event_read(NULL,h->enable))return true;
    }
    return false;
}
static void check_foreign_event(void) {
    if(foreign_event_enabled())unsafe("foreign event unexpectedly enabled");
}
static bool contains(uint64_t a,uint64_t n,uint64_t p,uint64_t size){return size&&p>=a&&p-a<=n&&size<=n-(p-a);}
static bool worker_context(void){uint64_t f=irq_save();bool ok=(f&(1ull<<9))&&runtime.worker.identity&&scheduler_current_thread_id()==runtime.worker.identity;irq_restore(f);return ok;}
static void pace(unsigned polls){if(!(polls%16))timer_wait_ticks(1);else timer_delay_us(20);}
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out){*out=runtime.platform.rsdp;return UACPI_STATUS_OK;}
void *uacpi_kernel_map(uacpi_phys_addr a,uacpi_size n){void *p=runtime.engaged?battery_platform_map(a,n):NULL;if(!p){runtime.denied++;console_printf("battery: denied map address=%x bytes=%u\n",a,(uint64_t)n);}return p?p:UACPI_MAP_FAILED;}
void uacpi_kernel_unmap(void *p,uacpi_size n){(void)p;(void)n;}
void uacpi_kernel_log(uacpi_log_level l,const char *s){if(l==UACPI_LOG_ERROR)runtime.errors++;if(l<=UACPI_LOG_WARN){console_write("battery-aml: ");console_write(s);}}
struct allocation_header { uint64_t charged, padding; };
void *uacpi_kernel_alloc(uacpi_size size) {
    if (!size) size = 1;
    if (size > HEAP_LIMIT - sizeof(struct allocation_header)) return NULL;
    uint64_t charged = size + sizeof(struct allocation_header), flags = irq_save();
    if (charged > HEAP_LIMIT - runtime.heap) { irq_restore(flags); return NULL; }
    struct allocation_header *header = kmalloc(charged);
    if (header) {
        header->charged = charged;
        runtime.heap += charged;
        if (runtime.heap > runtime.peak) runtime.peak = runtime.heap;
    }
    irq_restore(flags);
    return header ? header + 1 : NULL;
}
void uacpi_kernel_free(void *pointer) {
    if (!pointer) return;
    uint64_t flags = irq_save();
    struct allocation_header *header = (struct allocation_header *)pointer - 1;
    runtime.heap -= header->charged;
    kfree(header);
    irq_restore(flags);
}
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    uint64_t us = timer_monotonic_us(); return us > UINT64_MAX / 1000 ? UINT64_MAX : us * 1000;
}
void uacpi_kernel_stall(uacpi_u8 us) { timer_delay_us(us); }
void uacpi_kernel_sleep(uacpi_u64 ms) {
    if (!ms) { scheduler_yield(); return; }
    if (ms == SEMAPHORE_WAIT_FOREVER) unsafe("sleep duration overflows");
    struct kernel_semaphore delay;
    semaphore_init(&delay, 0);
    enum semaphore_result result = semaphore_wait(&delay, ms);
    semaphore_close(&delay);
    if (result != SEMAPHORE_TIMEOUT) unsafe("sleep cannot block");
}
uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 units) {
    struct kernel_semaphore *semaphore = uacpi_kernel_alloc(sizeof(*semaphore));
    if (semaphore) semaphore_init(semaphore, units);
    return semaphore;
}
void uacpi_kernel_free_semaphore(uacpi_handle handle) {
    if (!handle) return;
    semaphore_close(handle);
    if (!semaphore_drained(handle)) unsafe("semaphore has waiters");
    uacpi_kernel_free(handle);
}
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle, uacpi_u16 timeout) {
    enum semaphore_result result = semaphore_wait(handle, timeout == 0xffff ? SEMAPHORE_WAIT_FOREVER : timeout);
    return result == SEMAPHORE_OK ? UACPI_STATUS_OK : result == SEMAPHORE_TIMEOUT ?
        UACPI_STATUS_TIMEOUT : UACPI_STATUS_INTERNAL_ERROR;
}
void uacpi_kernel_signal_semaphore(uacpi_handle handle) {
    if (!semaphore_signal(handle)) unsafe("semaphore signal refused");
}
uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (void *)(uintptr_t)scheduler_current_thread_id(); }
uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) { return irq_save(); }
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state flags) { irq_restore(flags); }
uacpi_handle uacpi_kernel_create_spinlock(void) { return (void *)1; }
void uacpi_kernel_free_spinlock(uacpi_handle handle) { (void)handle; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    (void)handle;uint64_t flags=irq_save();
    if(recovery_irq.callback_active) { recovery_irq.callback_locks++;recovery_irq.callback_depth++; }
    return flags;
}
void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    (void)handle;
    if(recovery_irq.callback_active) {
        if(!recovery_irq.callback_depth || (flags&(1ull<<9)))unsafe("callback lock balance");
        recovery_irq.callback_depth--;
    }
    irq_restore(flags);
}
uacpi_handle uacpi_kernel_create_work_item(void) {
    struct acpi_work_item *item = uacpi_kernel_alloc(sizeof(*item));
    if (item) *item = (struct acpi_work_item){0};
    return item;
}
void uacpi_kernel_free_work_item(uacpi_handle handle) {
    if (handle && ((struct acpi_work_item *)handle)->pending) unsafe("free pending work");
    uacpi_kernel_free(handle);
}
void uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_handle item,
    uacpi_work_handler handler, uacpi_handle context) {
    if (!acpi_worker_enqueue(&runtime.worker, item, type, handler, context)) unsafe("work admission refused");
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    /* CPU0 IRQ callbacks cannot sleep: an ordinary-thread IRQ barrier excludes
     * all in-flight callbacks. Teardown masks sources before reaching here. */
    uint64_t flags = irq_save();
    if (!(flags & (1ull << 9))) unsafe("drain with interrupts disabled");
    irq_restore(flags);
    if (runtime.loading && runtime.worker.identity == scheduler_current_thread_id())
        unsafe("namespace load reset requires lifecycle owner");
    if (!acpi_worker_drain(&runtime.worker, 5000)) unsafe("work did not drain");
    return UACPI_STATUS_OK;
}
static struct sci_controller_observation sci_controller_snapshot(void) {
    struct sci_controller_observation c={.gsi=runtime.sci.route.gsi,
        .vector=(uint8_t)(32+runtime.sci.route.line)};
    struct lapic_vector_state local;
    if(lapic_vector_snapshot(c.vector,&local)) {
        c.valid|=1;c.lapic_bits=local.bits;c.lint0=local.lint0;
    }
    /* read_gsi serializes the register selector; it writes no route fields. */
    if(ioapic_read_gsi(c.gsi,&c.route_low,&c.route_high))c.valid|=2;
    c.pic_masks=(uint16_t)inb(0x21)|((uint16_t)inb(0xa1)<<8);c.valid|=4;
    return c;
}
/* CPU0, IRQ exclusion held throughout mask verification and thread retention. */
static bool sources_quiet(void) {
    bool quiet=!inw(runtime.platform.events.resources.pm1_enable);
    for(unsigned b=0;b<2;b++)for(unsigned i=0;i<runtime.platform.events.resources.gpe[b].bytes;i++)
        if(event_read(NULL,runtime.platform.events.resources.gpe[b].enable+i))quiet=false;
    uint32_t low=0,high=0;
    return quiet && ioapic_read_gsi(runtime.sci.route.gsi,&low,&high) && (low&(1u<<16));
}
static void mark_quarantined(void) {
    service.quarantined=true;service.stop=true;
    service.cache.value.state=BATTERY_SERVICE_FAULTED;
    service.cache.value.error=BATTERY_SERVICE_CONTAINMENT;
    service.cache.value.flags&=~BATTERY_CACHE_SAMPLING;
    scheduler_wake_all(&service.progress);
}
/* Read only the pinned interpreter's Global Lock and our serialized protocol
 * state. A free hardware lock is an observation, never a firmware ownership lease.
 * In particular global_lock_acquired does NOT cover AML Acquire(\_GL). */
static void require_recovery_resources(void) {
    struct battery_recovery_resources r={
        .lease=runtime.lease,.ec_poisoned=runtime.transport.poisoned,.ec_phase=runtime.transport.phase,
        .sampler_active=runtime.sampler.active,.sampler_waiting=runtime.sampler.waiting,
        .smbus_busy=runtime.sampler.transaction.ec && (runtime.sampler.transaction.phase!=8 ||
            runtime.sampler.transaction.pending || runtime.sampler.transaction.failed),
        .global_api=g_uacpi_rt_ctx.global_lock_acquired,.global_pending=g_uacpi_rt_ctx.global_lock_pending
    };
    uacpi_mutex *mutex=g_uacpi_rt_ctx.global_lock_mutex;
    uint64_t word=0;
    if(runtime.ready && g_uacpi_rt_ctx.facs && mutex && mutex->handle &&
       battery_platform_memory(&g_uacpi_rt_ctx.facs->global_lock,32,false,&word)) {
        const struct kernel_semaphore *sem=mutex->handle;
        r.known=true;r.facs_bits=word&3;
        r.global_aml=mutex->depth || mutex->owner!=UACPI_THREAD_ID_NONE;
        /* Covers acquisition/release windows before AML owner/API flags change. */
        r.global_mutex_busy=sem->closed || sem->units!=1 || sem->waiters || sem->queue.waiters;
    }
    unsigned blocked=battery_recovery_blockers(r);
    console_printf("battery-recovery-resources: known=%u lease=%u ec_phase=%u poison=%u sample=%u waiting=%u smbus=%u api=%u aml=%u mutex=%u pending=%u facs=%u blocked=%u\n",
        (uint64_t)r.known,(uint64_t)r.lease,(uint64_t)r.ec_phase,(uint64_t)r.ec_poisoned,
        (uint64_t)r.sampler_active,(uint64_t)r.sampler_waiting,(uint64_t)r.smbus_busy,
        (uint64_t)r.global_api,(uint64_t)r.global_aml,(uint64_t)r.global_mutex_busy,
        (uint64_t)r.global_pending,(uint64_t)r.facs_bits,(uint64_t)blocked);
    if(blocked)unsafe("recovery resources retained");
}
static void report_quarantine(void) {
    console_printf("battery-quarantine: owner=%u worker=%u io=%u heap=%u completed=%u; sources=masked retained=yes\n",
        service.identity,runtime.worker.identity,(uint64_t)scheduler_io_active(),runtime.heap,runtime.worker.completed);
}
static void report_callback_proof(void) {
    if(recovery_irq.callback_entries!=1 || recovery_irq.callback_faults!=1 || recovery_irq.callback_depth ||
       !recovery_irq.callback_locks || !recovery_irq.callback_failed || runtime.query.pending)
        unsafe("callback fixture proof");
    console_printf("battery-recovery-callback: entries=1 faults=1 locks=%u depth=0 query_pending=0; returned=yes reason=query-admission\n",recovery_irq.callback_locks);
}
static __attribute__((noreturn)) void quarantine_irq_exit(void *context) {
    (void)context;uint64_t flags=irq_save(),current=scheduler_current_thread_id();
    bool is_owner=current==service.identity;
    if((flags&(1ull<<9)) || !recovery_study || !recovery_irq.target_study || !recovery_irq.target_pending ||
       !recovery_irq.completed || recovery_irq.pending || recovery_irq.callback_active || recovery_irq.callback_depth ||
       recovery_irq.peer!=1 || recovery_irq.peer_complete!=1 || current!=recovery_irq.interrupted ||
       (!is_owner && current!=runtime.worker.identity) || runtime.sci_counts[0].in_flight ||
       runtime.sci_counts[1].in_flight || runtime.sci_counts[2].in_flight || !sources_quiet())
        unsafe("IRQ exit recovery admission");
    require_recovery_resources();
    /* EOI and every registered completion have returned; the registry cleared
     * its deferred state before entering this hook. Retain the interrupt frame
     * with its service stack. Never IRET to the failed service or restore IF. */
    if(!scheduler_quarantine_service_pair(current,is_owner?runtime.worker.identity:service.identity))
        unsafe("IRQ exit quarantine admission");
    mark_quarantined();recovery_irq.target_pending=false;recovery_irq.target_handoff=true;
    report_quarantine();report_callback_proof();
    console_printf("battery-recovery-target: interrupted=%u role=%s peer=1 completion=1 in_flight=0; exit=drained retained=yes\n",current,is_owner?"owner":"worker");
    for(;;)scheduler_yield();
}
/* Entry guard or a reported failure after the uACPI callback has returned.
 * Normally both targets must be noncurrent. The explicit target study instead
 * defers retention of the interrupted stack until the dispatcher completes EOI
 * and every completion callback. No nonlocal unwind or early scheduling. */
static void quarantine_irq_pair(bool callback) {
    uint64_t flags=irq_save();
    if(!recovery_study || !recovery_irq.study || !battery_platform_workspace_study_allowed() ||
       (flags&(1ull<<9)) || owner!=OWNER_SERVICE || !runtime.ready ||
       runtime.sci_counts[0].in_flight || runtime.sci_counts[1].in_flight!=1 || runtime.sci_counts[2].in_flight)
        unsafe("IRQ entry recovery admission");
    mask_sources();
    uint64_t current=scheduler_current_thread_id();
    if(callback && recovery_irq.target_study && (current==service.identity || current==runtime.worker.identity)) {
        if(!sources_quiet())unsafe("IRQ exit source mask");
        recovery_irq.interrupted=current;recovery_irq.pending=true;recovery_irq.target_pending=true;
        if(!irq_handler_defer_exit(runtime.sci.token,quarantine_irq_exit,NULL))unsafe("IRQ exit request refused");
        return; /* Finish the bridge, dispatcher and all post-EOI observers first. */
    }
    require_recovery_resources();
    if(!sources_quiet() || !scheduler_quarantine_noncurrent_services(service.identity,runtime.worker.identity))
        unsafe(callback?"IRQ callback quarantine admission":"IRQ entry quarantine admission");
    mark_quarantined();
    recovery_irq.interrupted=scheduler_current_thread_id();recovery_irq.pending=true;
}
static bool irq_bridge(void *context) {
    (void)context; if(service.quarantined)return false;
    runtime.interrupts++;
    struct sci_counts *counts=&runtime.sci_counts[runtime.stopping?2:runtime.ready?1:0];
    if(counts->delivered!=UINT64_MAX)counts->delivered++;
    counts->in_flight++;
    if(foreign_event_enabled()) {
        if(!recovery_irq.study)unsafe("foreign event unexpectedly enabled");
        quarantine_irq_pair(false);
        counts->in_flight--;
        if(counts->unclaimed!=UINT64_MAX)counts->unclaimed++;
        uint64_t now=timer_monotonic_us();
        if(counts->unclaimed==1)counts->first_unclaimed_us=now;
        counts->last_unclaimed_us=now;
        return false; /* Unexpected foreign event is never claimed as ours. */
    }
    bool observed=sci_observe_begin(&runtime.sci_observer,timer_monotonic_us(),
        runtime.stopping?2:runtime.ready?1:0,runtime.query_scheduled);
    if(observed) {
        runtime.sci_observer.current.query_entry=sci_query_snapshot(&runtime.query_observer,
            runtime.sci_observer.current.at_us);
        sci_observe_controller(&runtime.sci_observer,false,sci_controller_snapshot());
    }
    /* Recover only a reported query-admission failure after a normal return.
     * Never jump over interpreter frames or release their resources here. */
    recovery_irq.callback_active=recovery_irq.callback_study;
    bool handled=runtime.irq_fn && runtime.irq_fn(runtime.irq_context)==UACPI_INTERRUPT_HANDLED;
    recovery_irq.callback_active=false;
    if(recovery_irq.callback_study && recovery_irq.callback_depth)unsafe("callback lock balance");
    if(recovery_irq.callback_failed) {
        if(handled || !recovery_irq.callback_entries || !recovery_irq.callback_locks || runtime.query.pending)
            unsafe("callback return proof");
        quarantine_irq_pair(true);
    }
    if(observed)sci_observe_controller(&runtime.sci_observer,true,sci_controller_snapshot());
    uint64_t finished_us=timer_monotonic_us();
    if(observed)sci_observe_end(&runtime.sci_observer,finished_us,handled,runtime.query_scheduled);
    counts->in_flight--;
    uint64_t *total=handled?&counts->handled:&counts->unclaimed;
    if(*total!=UINT64_MAX)(*total)++;
    if(!handled) {
        if(counts->unclaimed==1)counts->first_unclaimed_us=finished_us;
        counts->last_unclaimed_us=finished_us;
    }
    /* Preserve the callback's claim result. No added event-register access or
     * logging in the IRQ path; a delivered interrupt is not necessarily ours. */
    return handled;
}
static void sci_after_eoi(void *context) {
    (void)context;if(service.quarantined || recovery_irq.target_pending) {
        if(recovery_irq.pending) { recovery_irq.pending=false;recovery_irq.completed=true; }
        return;
    }
    struct sci_controller_observation c=sci_controller_snapshot();
    sci_observe_post_eoi(&runtime.sci_observer,c,timer_monotonic_us(),runtime.query_scheduled);
}
static bool sci_read(void *ctx, uint32_t gsi, uint32_t *low, uint32_t *high) {
    (void)ctx; return ioapic_read_gsi(gsi, low, high);
}
static bool sci_prepare(void *ctx, uint32_t gsi, uint8_t vector, bool low, bool level) {
    (void)ctx; return ioapic_prepare_gsi(gsi, vector, low, level);
}
static bool sci_mask(void *ctx, uint32_t gsi, bool masked) {
    (void)ctx; return ioapic_mask_gsi(gsi, masked);
}
static bool sci_restore(void *ctx, uint32_t gsi, uint32_t low, uint32_t high) {
    (void)ctx; return ioapic_restore_masked_gsi(gsi, low, high);
}
static uint64_t sci_register(void *ctx, uint8_t line, irq_handler_fn fn, void *context) {
    (void)ctx;
    return irq_handler_register_observed(line, fn, sci_after_eoi, context);
}
static bool sci_remove(void *ctx, uint64_t token) { (void)ctx; return irq_handler_remove(token); }
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq,uacpi_interrupt_handler fn,uacpi_handle ctx,uacpi_handle *out) {
    struct sci_route route;
    if(irq!=runtime.platform.ec.sci||!fn||!sci_route_resolve(&route,irq,acpi_irq_override(irq)))return UACPI_STATUS_DENIED;
    struct sci_owner_ops ops={.read=sci_read,.prepare=sci_prepare,.mask=sci_mask,.restore=sci_restore,.register_irq=sci_register,.remove_irq=sci_remove};
    runtime.irq_fn=fn;runtime.irq_context=ctx;
    if(!sci_owner_prepare(&runtime.sci,ops,route,irq_bridge,NULL)||!sci_owner_enable(&runtime.sci)) {
        if(!sci_owner_release(&runtime.sci))unsafe("SCI install rollback");return UACPI_STATUS_DENIED;
    }
    *out=&runtime;return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler fn,uacpi_handle h) {
    if(h!=&runtime||fn!=runtime.irq_fn||!sci_owner_release(&runtime.sci))unsafe("SCI retire");
    runtime.irq_fn=NULL;return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r){(void)r;runtime.denied++;return UACPI_STATUS_DENIED;}
struct io_region{uint64_t base;size_t size;};
uacpi_status uacpi_kernel_io_map(uacpi_io_addr a,uacpi_size n,uacpi_handle *out) {
    if(!runtime.engaged||!(acpi_event_io_map(&runtime.platform.events,a,n)||battery_platform_io_map(a,n))){runtime.denied++;return UACPI_STATUS_DENIED;}
    struct io_region *r=uacpi_kernel_alloc(sizeof(*r));if(!r)return UACPI_STATUS_OUT_OF_MEMORY;*r=(struct io_region){a,n};*out=r;return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle h){uacpi_kernel_free(h);}
static uacpi_status port_io(struct io_region *r,size_t o,unsigned bits,bool write,uint32_t *v) {
    if(!runtime.engaged||!r||!contains(0,r->size,o,bits/8))goto denied;
    uint64_t a=r->base+o;
    if(write && query_tracking() && runtime.query_receipt.in_aml)runtime.query_receipt.effects++;
    if(write&&a==runtime.platform.ec.pm1_control&&bits==16&&runtime.worker.identity==scheduler_current_thread_id()) {
        uint64_t flags=irq_save();uint16_t before=inw(a);
        bool ok=acpi_event_io_global_release(&runtime.platform.events,a,bits,before,*v);
        if(ok)outw(a,*v);irq_restore(flags);if(ok)return UACPI_STATUS_OK;goto denied;
    }
    if(write?acpi_event_io_write(&runtime.platform.events,a,bits,*v):acpi_event_io_read(&runtime.platform.events,a,bits)) {
        if(write){if(bits==8)event_write(NULL,a,filter_foreign_event(a,true,*v));else if(bits==16)outw(a,*v);else outl(a,*v);}
        else {
            *v=bits==8?event_read(NULL,a):bits==16?inw(a):inl(a);
            uint32_t raw=*v;
            if(bits==8)*v=filter_foreign_event(a,false,*v);
            const struct sci_observe_ports ports={runtime.platform.events.resources.pm1_status,
                runtime.platform.events.resources.pm1_enable,runtime.platform.ec.gpe_status,
                runtime.platform.ec.gpe_enable};
            sci_observe_read(&runtime.sci_observer,&ports,a,bits,raw,*v);
        }
        return UACPI_STATUS_OK;
    }
    if(battery_platform_io(a,bits,write,v))return UACPI_STATUS_OK;
denied:
    runtime.denied++;console_printf("battery: denied IO port=%x width=%u write=%u value=%x\n",r?r->base+o:0,(uint64_t)bits,(uint64_t)write,(uint64_t)*v);return UACPI_STATUS_DENIED;
}
#define PORT(B) \
uacpi_status uacpi_kernel_io_read##B(uacpi_handle h,uacpi_size o,uacpi_u##B *v){uint32_t n=0;uacpi_status s=port_io(h,o,B,false,&n);*v=n;return s;} \
uacpi_status uacpi_kernel_io_write##B(uacpi_handle h,uacpi_size o,uacpi_u##B v){uint32_t n=v;return port_io(h,o,B,true,&n);}
PORT(8) PORT(16) PORT(32)
#define MEMORY(B) \
uacpi_u##B uacpi_kernel_mmio_read##B(void *p){uint64_t v=0;if(!runtime.engaged||!battery_platform_memory(p,B,false,&v)){runtime.denied++;console_printf("battery: denied memory read ptr=%x width=%u\n",(uint64_t)(uintptr_t)p,(uint64_t)B);}return v;} \
void uacpi_kernel_mmio_write##B(void *p,uacpi_u##B v){if(query_tracking()&&runtime.query_receipt.in_aml)runtime.query_receipt.effects++;uint64_t n=v;if(!runtime.engaged||!battery_platform_memory(p,B,true,&n)){runtime.denied++;console_printf("battery: denied memory write ptr=%x width=%u value=%x\n",(uint64_t)(uintptr_t)p,(uint64_t)B,n);}}
MEMORY(8) MEMORY(16) MEMORY(32) MEMORY(64)
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address a,uacpi_handle *h){if(runtime.engaged&&battery_platform_pci_open(a,h))return UACPI_STATUS_OK;runtime.denied++;return UACPI_STATUS_DENIED;}
void uacpi_kernel_pci_device_close(uacpi_handle h){(void)h;}
#define PCI(B) \
uacpi_status uacpi_kernel_pci_read##B(uacpi_handle h,uacpi_size o,uacpi_u##B *v){uint32_t n=0;if(runtime.engaged&&battery_platform_pci_read(h,o,B,&n)){*v=n;return UACPI_STATUS_OK;}runtime.denied++;return UACPI_STATUS_DENIED;} \
uacpi_status uacpi_kernel_pci_write##B(uacpi_handle h,uacpi_size o,uacpi_u##B v){(void)h;(void)o;(void)v;runtime.denied++;return UACPI_STATUS_DENIED;}
PCI(8) PCI(16) PCI(32)
static bool transfer(bool write,uint8_t a,uint8_t *v,bool query) {
    if(!worker_context()||runtime.lease||runtime.transport.poisoned)return false;
    runtime.lease=true;
    bool ok=query?ec_transport_begin_query(&runtime.transport,timer_monotonic_us(),100000):
        ec_transport_begin(&runtime.transport,write,a,*v,timer_monotonic_us(),100000);
    int result=0;
    for(unsigned polls=1;ok&&!result;polls++) {
        result=ec_transport_poll(&runtime.transport,timer_monotonic_us());
        if(!result) {
            if(runtime.platform.model_transfer_wait)runtime.platform.model_transfer_wait();
            pace(polls);
        }
    }
    if(result==1&&!write)*v=runtime.transport.result;
    runtime.lease=false;return ok&&result==1;
}
static uacpi_status region_handler(uacpi_region_op op,uacpi_handle data) {
    if(op==UACPI_REGION_OP_ATTACH){uacpi_region_attach_data *a=data;
        if(a->handler_context!=&runtime||a->generic_info.base>255||!a->generic_info.length||a->generic_info.length>256-a->generic_info.base)goto denied;
        a->out_region_context=&runtime;runtime.attached++;return UACPI_STATUS_OK;}
    if(op==UACPI_REGION_OP_DETACH){runtime.detached++;return UACPI_STATUS_OK;}
    if(op!=UACPI_REGION_OP_READ&&op!=UACPI_REGION_OP_WRITE)goto denied;
    uacpi_region_rw_data *r=data;
    if(r->handler_context!=&runtime||r->region_context!=&runtime||r->byte_width!=1||r->address>255)goto denied;
    if(op==UACPI_REGION_OP_WRITE && query_tracking() && runtime.query_receipt.in_aml)runtime.query_receipt.effects++;
    if(runtime.platform.bounded_samples_only &&
       !battery_bounded_ec(op==UACPI_REGION_OP_WRITE,r->address,r->value))goto denied;
    uint8_t v=r->value;
    if(!transfer(op==UACPI_REGION_OP_WRITE,r->address,&v,false))goto denied;
    if(op==UACPI_REGION_OP_READ)r->value=v;return UACPI_STATUS_OK;
denied:runtime.denied++;return UACPI_STATUS_DENIED;
}
static uacpi_status notify_handler(uacpi_handle ctx,uacpi_namespace_node *node,uacpi_u64 v) {
    (void)ctx;(void)node;
    if(!worker_context())unsafe("Notify outside worker");
    runtime.notifications++;if(!runtime.transition_mode)console_printf("battery: Notify value=%x count=%u\n",v,runtime.notifications);
    return UACPI_STATUS_OK; /* Sampling is periodic; callbacks never nest EC bank IO. */
}
static bool contract(void) {
    if(uacpi_namespace_node_find(NULL,runtime.platform.ec.path,&runtime.node))return false;
    uint64_t hid,uid,gpe,glk=0,bank,sbs;
    if(uacpi_eval_simple_integer(runtime.node,"_HID",&hid)||hid!=0x090cd041||
       uacpi_eval_simple_integer(runtime.node,"_UID",&uid)||uacpi_eval_simple_integer(runtime.node,"_GPE",&gpe))return false;
    uacpi_status gs=uacpi_eval_simple_integer(runtime.node,"_GLK",&glk);
    if((gs&&gs!=UACPI_STATUS_NOT_FOUND)||glk)return false; /* This captured EC does not request a lock. */
    uacpi_object *object=NULL;uacpi_data_view crs;
    if(uacpi_eval_simple(runtime.node,"_CRS",&object)||!object)return false;
    bool ok=!uacpi_object_get_buffer(object,&crs)&&acpi_ec_namespace_matches(&runtime.platform.ec,uid,gpe,crs.bytes,crs.length);
    uacpi_object_unref(object);
    ok=ok&&!uacpi_eval_simple_integer(runtime.node,"SMB0._EC",&bank)&&bank==0x2010&&
        !uacpi_eval_simple_integer(runtime.node,"SMB0.SBS0._SBS",&sbs)&&sbs==1;
    if(ok && (smbus_study || smbus_capture)) {
        object=NULL;uacpi_data_view id;
        if(uacpi_eval_simple_string(runtime.node,"SMB0._HID",&object)||!object)return false;
        ok=!uacpi_object_get_string(object,&id)&&id.length==9;
        static const char expected[]="ACPI0001";
        for(unsigned i=0;ok&&i<sizeof(expected);i++)ok=id.bytes[i]==(uint8_t)expected[i];
        uacpi_object_unref(object);
        gs=uacpi_eval_simple_integer(runtime.node,"SMB0._GLK",&glk);
        ok=ok&&(gs==UACPI_STATUS_NOT_FOUND||(!gs&&!glk));
    }
    return ok;
}
static void setup_mark(const char *stage) {
    runtime.setup_stage=stage;
    console_printf("battery: setup=%s elapsed_us=%u\n",stage,
        timer_monotonic_us()-runtime.setup_started_us);
}
static void setup_work(void *ctx) {
    (void)ctx;runtime.ok=false;
    if(!worker_context())unsafe("setup outside worker");
    runtime.setup_started_us=timer_monotonic_us();
    setup_mark("handler");
    if(runtime.platform.model_setup_work)runtime.platform.model_setup_work();
    /* Root handler before namespace load provides the ECDT controller for
     * module-level AML. Exact firmware was checked before granting IO. */
    runtime.handler_node=uacpi_namespace_root();
    if(uacpi_install_address_space_handler(runtime.handler_node,UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER,region_handler,&runtime))goto done;
    runtime.region=true;
    setup_mark("namespace-load");
    if(uacpi_namespace_load())goto done;
    setup_mark("contract");
    if(!contract())goto done;
    if(uacpi_install_notify_handler(uacpi_namespace_root(),notify_handler,NULL))goto done;
    runtime.notify=true;
    setup_mark("region-connect");
    if(uacpi_reg_all_opregions(runtime.handler_node,UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER)||runtime.errors||runtime.denied)goto done;
    setup_mark("namespace-initialize");
    if(uacpi_namespace_initialize())goto done;
    runtime.ok=!runtime.errors&&!runtime.denied;
done:if(runtime.ok)setup_mark("complete");runtime.done=true;
}
static void query_work(void *ctx);
static bool queue_query(void) {
    if(recovery_irq.callback_failed)return false;
    if(runtime.stopping||runtime.query_scheduled)return true;
    if(!ec_query_budget_begin(&runtime.budget,timer_monotonic_us(),128,2000000))unsafe("query budget begin");
    if(query_tracking()) {
        uint64_t sequence=runtime.query_receipt.sequence;
        if(sequence==UINT64_MAX)unsafe("query receipt overflow");
        runtime.query_receipt=(struct battery_query_receipt){.sequence=sequence+1,
            .generation=runtime.sampler.sample.generation,
            .policy=runtime.platform.query_policy?runtime.platform.query_policy:BATTERY_QUERY_MODEL};
    }
    runtime.query_scheduled=true;
    sci_query_start(&runtime.query_observer,timer_monotonic_us());
    if(uacpi_suspend_gpe(NULL,runtime.platform.ec.gpe)||uacpi_clear_gpe(NULL,runtime.platform.ec.gpe))unsafe("query admission");
    bool refused=runtime.platform.model_query_refuse && runtime.platform.model_query_refuse();
    if(refused || !acpi_worker_enqueue(&runtime.worker,&runtime.query,UACPI_WORK_GPE_EXECUTION,query_work,NULL)) {
        if(!recovery_study || !recovery_irq.callback_study || !recovery_irq.callback_active ||
           !battery_platform_workspace_study_allowed())unsafe("query admission");
        recovery_irq.callback_failed=true;recovery_irq.callback_faults++;
        return false;
    }
    return true;
}
static bool pending_query(void) {
    uacpi_event_info info=0;
    if(uacpi_gpe_info(NULL,runtime.platform.ec.gpe,&info))unsafe("GPE read");
    return (info&UACPI_EVENT_INFO_HW_STATUS)||(runtime.platform.io.status(runtime.platform.io.context)&32);
}
static bool rearm_pending(void *context) { (void)context;return pending_query(); }
static void rearm_queue(void *context) { (void)context;queue_query(); }
static void rearm_resume(void *context) {
    (void)context;query_phase(SCI_QUERY_RESUME);
    if(uacpi_resume_gpe(NULL,runtime.platform.ec.gpe))unsafe("GPE resume");
    query_phase(SCI_QUERY_IDLE);
}
static uacpi_interrupt_ret gpe_handler(uacpi_handle ctx,uacpi_namespace_node *node,uacpi_u16 idx) {
    (void)ctx;(void)node;
    if(idx!=runtime.platform.ec.gpe)unsafe("wrong EC GPE");
    if(recovery_irq.callback_active)recovery_irq.callback_entries++;
    return queue_query()?UACPI_INTERRUPT_HANDLED:UACPI_INTERRUPT_NOT_HANDLED;
}
/* Explicit host-controller ownership in the disposable model, independent of
 * AML OSYS. It never invokes or rewrites _Q10 and never nests sampler bank IO. */
static void smbus_query_work(void) {
    if(!smbus_owned() || !runtime.service_mode ||
       runtime.sampler.active || runtime.sampler.waiting || runtime.lease ||
       (runtime.sampler.transaction.ec && runtime.sampler.transaction.phase!=8))unsafe("SMBus event ownership");
    if(!continuous_home && (smbus_capture || mounted_home) && runtime.events.totals.events>=(smbus_transitions?4096u:(smbus_files || smbus_controls)?512u:128u))unsafe("SMBus capture event limit");
    if(!continuous_home && mounted_home && smbus_controls && home_session_events>=512)unsafe("Home session event limit");
    struct ec_smbus_event event={0};runtime.lease=true;
    bool begun=ec_smbus_event_begin(&event,&runtime.transport,0x20,timer_monotonic_us());int result=0;
    for(unsigned polls=1;begun&&!result;polls++) {
        result=ec_smbus_event_poll(&event,timer_monotonic_us());if(!result)pace(polls);
    }
    runtime.lease=false;
    if(query_tracking() && event.ack_attempted)runtime.query_receipt.effects++;
    if(!begun || result!=1) {
        console_printf("battery-smbus: FAILED phase=%u status=%u ack_attempted=%u acknowledged=%u\n",
            (uint64_t)event.phase,(uint64_t)event.status,(uint64_t)event.ack_attempted,(uint64_t)event.acknowledged);
        unsafe("SMBus event");
    }
    if(event.alarm && event.recognized) {
        uint64_t f=irq_save();
        runtime.smbus_invalid_through=runtime.sampler.sample.generation;
        runtime.smbus_refresh=true;service.cache.last_attempt_ok=false;
        struct battery_service_status invalidated;
        battery_cache_read(&service.cache,timer_monotonic_us(),&invalidated);
        scheduler_wake_all(&service.wake);irq_restore(f);
        if(invalidated.flags&BATTERY_CACHE_FRESH)unsafe("SMBus cache invalidation");
        if(!continuous_home && !smbus_transitions)console_printf(compact_smbus_records()?"smbus-i:%u,%u,%u\n":"battery-smbus: invalidate generation=%u cached=%u valid=%u fresh=0\n",
            runtime.smbus_invalid_through,invalidated.sample.generation,
            (uint64_t)!!(invalidated.flags&BATTERY_CACHE_VALID));
    }
    uint64_t event_flags=irq_save();
    if(mounted_home && smbus_controls && home_session_events!=UINT64_MAX)home_session_events++;
    battery_event_record(&runtime.events,event.alarm,event.acknowledged,event.recognized);
    irq_restore(event_flags);
    if(!continuous_home && !smbus_transitions)console_printf(compact_smbus_records()?"smbus-e:%u,%u,%u,%u,%u,%u,%u,%u\n":"battery-smbus: event=%u generation=%u status=%u alarm=%u address=%u data=%u acknowledged=%u refresh=%u; aml=0 serialized=1\n",
        runtime.events.totals.events,runtime.sampler.sample.generation,(uint64_t)event.status,
        (uint64_t)event.alarm,(uint64_t)event.address,(uint64_t)event.data,
        (uint64_t)event.acknowledged,(uint64_t)(event.alarm&&event.recognized));
}
static void query_work(void *ctx) {
    (void)ctx;
    if(!worker_context()||runtime.lease)unsafe("query ownership");
    if(runtime.stopping){
        uint64_t f=irq_save();runtime.query_scheduled=false;query_phase(SCI_QUERY_IDLE);irq_restore(f);return;
    }
    for(unsigned i=0;i<8;i++) {
        if(!ec_query_budget_take(&runtime.budget,timer_monotonic_us()))unsafe("query limit");
        uint8_t code=0;
        if(query_tracking())runtime.query_receipt.transfer_done=false;
        query_phase(SCI_QUERY_TRANSFER);
        if(!transfer(false,0,&code,true))unsafe("query transport");
        uint64_t observed_flags=irq_save();
        runtime.query_observer.code=code;runtime.query_observer.code_valid=true;
        query_phase(code?SCI_QUERY_AML:SCI_QUERY_CHECK);irq_restore(observed_flags);
        unsigned policy=runtime.platform.query_policy;
        if(query_study && !policy)policy=BATTERY_QUERY_MODEL;
        enum battery_query_class query_class=battery_query_classify(policy,code);
        bool host_event=code==0x10 && smbus_owned();
        bool rejected=!host_event && battery_query_rejected(query_class);
        if(rejected) {
            console_printf("battery-query-policy: policy=%u code=%u class=%u; dispatch=refused\n",
                (uint64_t)policy,(uint64_t)code,(uint64_t)query_class);
            /* Only an explicitly held native scope admits normal return. */
            if(!query_study && !native_query_return_scope())unsafe("query policy");
        }
        if(query_tracking()) {
            struct battery_query_receipt *r=&runtime.query_receipt;
            r->code=code;r->transfer_done=true;
            if(runtime.platform.model_query_checkpoint)runtime.platform.model_query_checkpoint(code);
            if(code && rejected) {
                if(!(query_study?battery_platform_workspace_study_allowed():native_query_return_scope()) || !runtime.service_mode ||
                   owner!=OWNER_SERVICE || !runtime.ready || service.stop ||
                   !ec_query_budget_finish(&runtime.budget,timer_monotonic_us()))unsafe("query refusal admission");
                uint64_t f=irq_save();
                r->refused=true;r->completed_before=runtime.worker.completed;
                r->notifications_before=runtime.notifications;
                /* No more queries or samples; already queued notifications must
                 * still return through the normal worker completion path. */
                mask_sources();runtime.query_scheduled=false;query_phase(SCI_QUERY_IDLE);
                service.stop=true;service.cache.value.state=BATTERY_SERVICE_FAULTED;
                service.cache.value.error=BATTERY_SERVICE_CONTAINMENT;
                scheduler_wake_all(&service.wake);irq_restore(f);return;
            }
        }
        if(host_event) { smbus_query_work();runtime.queries++; }
        else if(code) {
            if(query_tracking()){runtime.query_receipt.in_aml=true;runtime.query_receipt.aml_calls++;}
            const char hex[]="0123456789ABCDEF";char method[]="_Q00";method[2]=hex[code>>4];method[3]=hex[code&15];
            if(!runtime.transition_mode)console_printf("battery: dispatch EC query=%x\n",(uint64_t)code);
            uacpi_status result=uacpi_execute_simple(runtime.node,method);
            if(query_tracking()){runtime.query_receipt.in_aml=false;runtime.query_receipt.aml_returns++;}
            if(result||runtime.errors||runtime.denied)unsafe("query AML");runtime.queries++;
        }
        query_phase(SCI_QUERY_CHECK);
        if(!ec_query_budget_check(&runtime.budget,timer_monotonic_us()))unsafe("query deadline");
        if(!(runtime.platform.io.status(runtime.platform.io.context)&32))break;
    }
    uint64_t flags=irq_save();
    if(runtime.stopping){runtime.query_scheduled=false;query_phase(SCI_QUERY_IDLE);irq_restore(flags);return;}
    if(runtime.platform.io.status(runtime.platform.io.context)&32) {
        query_phase(SCI_QUERY_QUEUED);
        if(!acpi_worker_enqueue(&runtime.worker,&runtime.query,UACPI_WORK_GPE_EXECUTION,query_work,NULL))unsafe("query reschedule");
    } else {
        if(!ec_query_budget_finish(&runtime.budget,timer_monotonic_us()))unsafe("query finish");
        runtime.query_scheduled=false;
        const struct ec_query_rearm_ops rearm={.pending=rearm_pending,
            .resume=rearm_resume,.queue=rearm_queue};
        ec_query_rearm(&rearm); /* Keep IRQs off through both pending checks. */
    }
    irq_restore(flags);
}
static void sample_work(void *ctx) {
    (void)ctx;runtime.ok=false;
    if(!worker_context()||runtime.lease)unsafe("sample ownership");
    if(runtime.service_mode&&service.stop){runtime.ok=true;runtime.done=true;return;}
    if(runtime.service_mode) {
        uint64_t f=irq_save();service.cache.value.flags|=BATTERY_CACHE_SAMPLING;irq_restore(f);
    }
    if(runtime.platform.model_sample_begin)runtime.platform.model_sample_begin(runtime.sampler.sample.generation+1);
    uint8_t state=0;
    runtime.ac_coherent=true;
    if(!transfer(false,0x60,&state,false))goto done;
    runtime.ac_present=(state&2)!=0;runtime.ac_sampled_us=timer_monotonic_us();
    runtime.lease=true;
    bool begun=sbs_sample_begin(&runtime.sampler,&runtime.transport,0x20,timer_monotonic_us());int result=0;
    for(unsigned polls=1;begun&&!result;polls++){result=sbs_sample_poll(&runtime.sampler,timer_monotonic_us());if(!result)pace(polls);}
    runtime.lease=false;
    runtime.ok=begun&&result==1&&sbs_sample_fresh(&runtime.sampler,timer_monotonic_us(),1000000)&&
        (runtime.service_mode||(runtime.sampler.sample.valid&(SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CAPACITY))==(SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CAPACITY));
    if(runtime.ok&&runtime.transition_mode) {
        uint8_t after=0;
        if(!transfer(false,0x60,&after,false))runtime.ok=false;
        else runtime.ac_coherent=runtime.ac_present==((after&2)!=0);
    }
done:runtime.done=true;
}
static void disconnect_work(void *ctx) {
    (void)ctx;runtime.ok=false;
    if(!worker_context()||runtime.transport.poisoned||runtime.lease)goto done;
    if(runtime.region&&uacpi_uninstall_address_space_handler(runtime.handler_node,UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER,region_handler))goto done;
    runtime.region=false;runtime.ok=runtime.attached==runtime.detached;
done:runtime.done=true;
}
/* Deliberately narrower than unsafe(): only the VM service owner detecting a
 * sample deadline with IF set and no SCI callback in flight. IRQ-origin faults
 * and all native paths retain terminal containment. No interpreter unwind. */
static void quarantine_sample_deadline(void) {
    uint64_t flags=irq_save();
    if(!recovery_study || !battery_platform_workspace_study_allowed() ||
       !(flags&(1ull<<9)) || owner!=OWNER_SERVICE || !runtime.ready ||
       scheduler_current_thread_id()!=service.identity || !runtime.worker.identity ||
       runtime.sci_counts[0].in_flight || runtime.sci_counts[1].in_flight || runtime.sci_counts[2].in_flight) {
        irq_restore(flags);return;
    }
    mask_sources();
    require_recovery_resources();
    /* Once sources have changed, a refused quarantine is terminal. */
    if(!sources_quiet() || !scheduler_quarantine_service_pair(service.identity,runtime.worker.identity))
        unsafe("quarantine admission");
    mark_quarantined();report_quarantine();
    /* Quarantined slots are neither runnable nor reusable. Never restore IF on
     * this stack; the selected storage owner/idle thread restores its own state. */
    for(;;)scheduler_yield();
}
static void operation(uacpi_work_handler fn,uint64_t ms,const char *phase) {
    runtime.done=false;runtime.ok=false;
    if(!runtime.transition_mode||fn!=sample_work)console_printf("battery: phase=%s\n",phase);
    if(!acpi_worker_enqueue(&runtime.worker,&runtime.operation,UACPI_WORK_NOTIFICATION,fn,NULL))unsafe("operation admission");
    uint64_t deadline=timer_monotonic_us()+ms*1000;
    while(!acpi_worker_drain(&runtime.worker,50)) {
        if(runtime.platform.model_owner_irq)runtime.platform.model_owner_irq();
        check_foreign_event();
        if(runtime.budget.active&&!ec_query_budget_check(&runtime.budget,timer_monotonic_us()))unsafe("independent query deadline");
        if(timer_monotonic_us()>=deadline) {
            if(fn==sample_work)quarantine_sample_deadline();
            unsafe("operation deadline");
        }
    }
    check_foreign_event();
    if(!runtime.done||!runtime.ok||runtime.errors||runtime.denied||runtime.transport.poisoned)unsafe(phase);
}
static bool runtime_open(void *rsdp,struct limine_memmap_response *map,bool transitions,bool service_mode) {
    if(runtime.engaged||!timer_reference_ready())return false;
    runtime=(typeof(runtime)){.transition_mode=transitions,.service_mode=service_mode};
    if(!battery_platform_open(&runtime.platform,rsdp,map)){console_write("battery: REFUSED platform/resource ownership preflight\n");return false;}
    if(smbus_transitions && (runtime.platform.query_policy!=BATTERY_QUERY_AIR61 || !air61_smbus_owned())) {
        console_write("battery: REFUSED Air6,1 parked AC transition owner\n");return false;
    }
    if(runtime.platform.bounded_samples_only && !air61_smbus_owned() && !air61_home_owned() && !battery_bounded_entry(transitions,service_mode,owner==OWNER_DIAGNOSTIC,auto_return,
       workspace_parked_device()!=NULL,workspace_active(),workspace_image_busy(),scheduler_io_active())) {
        console_write("battery: REFUSED bounded profile requires an admitted bounded diagnostic owner\n");return false;
    }
    if(smbus_study && !runtime.platform.model_smbus_queries) {
        console_write("battery-smbus-study: REFUSED model ownership profile\n");return false;
    }
    if((smbus_capture || mounted_home) && !smbus_owned()) {
        console_write("battery-native-smbus: REFUSED captured controller profile\n");return false;
    }
    if(query_capture && runtime.platform.query_policy!=BATTERY_QUERY_AIR72) {
        console_write("battery-native-query: REFUSED query policy profile\n");return false;
    }
    struct sci_route route;uint32_t lo,hi;
    if(!sci_route_resolve(&route,runtime.platform.ec.sci,acpi_irq_override(runtime.platform.ec.sci))||
       !ioapic_read_gsi(route.gsi,&lo,&hi)||!(lo&(1u<<16))||(lo&((1u<<12)|(1u<<14)))) {
        console_write("battery: REFUSED SCI route not free/masked\n");return false;
    }
    runtime.engaged=true;
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(!battery_event_hold_begin(h,event_ops))unsafe("foreign event mask");
        if(h->active)console_printf("battery: foreign-event held enable=%x original=%x\n",(uint64_t)h->enable,(uint64_t)h->original);
    }
    bool started=service_mode?acpi_worker_start_io(&runtime.worker,service_io_ready,NULL):
        acpi_worker_start(&runtime.worker);
    if(!started)unsafe("worker start");
    ec_transport_init(&runtime.transport,runtime.platform.io);
    if(uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE|UACPI_FLAG_BAD_CSUM_FATAL|UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL)||runtime.errors||runtime.denied)unsafe("interpreter initialize");
    uacpi_context_set_loop_timeout(1);uacpi_context_set_max_call_stack_depth(32);
    if(uacpi_bulk_configure_interfaces(UACPI_INTERFACE_ACTION_DISABLE,UACPI_INTERFACE_KIND_VENDOR)||
       uacpi_install_interface("Darwin",UACPI_INTERFACE_KIND_VENDOR)||uacpi_enter_acpi_mode())unsafe("ACPI mode/policy");
    check_foreign_event();
    if(runtime.platform.model_mode_entered)runtime.platform.model_mode_entered();
    /* Firmware may arm the fixed power button while entering ACPI mode. The
     * observed Air transition adds bit8 to an initially zero enable register.
     * Disable that source before uACPI installs Global Lock; never widen its
     * allowed-enable mask or acknowledge pending button status here. */
    uint16_t fixed=inw(runtime.platform.events.resources.pm1_enable);
    console_printf("battery: post-mode fixed-enable=%x\n",(uint64_t)fixed);
    if(fixed) {
        if((runtime.platform.original_control&1)||fixed!=(1u<<8))unsafe("unexpected post-mode fixed enable");
        if(uacpi_disable_fixed_event(UACPI_FIXED_EVENT_POWER_BUTTON)||
           inw(runtime.platform.events.resources.pm1_enable))unsafe("post-mode power button mask");
        console_write("battery: post-mode power button masked; status not acknowledged\n");
    }
    runtime.loading=true;operation(setup_work,5000,"namespace/EC connection");runtime.loading=false;
    /* Match the Linux EC raw edge policy; no firmware AML GPE finalization. */
    unsigned bank=runtime.platform.ec.gpe>=runtime.platform.events.resources.gpe[1].first&&runtime.platform.events.resources.gpe[1].bytes?1:0;
    unsigned byte=(runtime.platform.ec.gpe-runtime.platform.events.resources.gpe[bank].first)/8;
    if(byte>=runtime.platform.events.resources.gpe[bank].bytes)unsafe("GPE policy bounds");
    runtime.platform.events.gpe_enable_mask[bank][byte]=runtime.platform.ec.gpe_mask;
    if(uacpi_install_gpe_handler_raw(NULL,runtime.platform.ec.gpe,UACPI_GPE_TRIGGERING_EDGE,gpe_handler,NULL)||
       uacpi_enable_gpe(NULL,runtime.platform.ec.gpe))unsafe("EC GPE install");
    runtime.gpe=true;runtime.ready=true;
    sci_report("ready");
    return true;
}
/* Service-thread writer only. Copy on CPU0 with interrupts masked; emit outside that critical section.
 * The worker remains responsible for every event/ack/invalidation, even when
 * logging coalesces thousands of successful events into one status record. */
static void continuous_events_report(bool final) {
    if(!continuous_home)return;
    struct battery_event_totals totals;uint64_t f=irq_save(),now=timer_monotonic_us();
    bool emit=battery_event_take(&runtime.events,now,final,&totals);
    uint64_t epoch=service.cache.value.epoch;irq_restore(f);
    if(emit)console_printf("battery-monitor-events: epoch=%u at_us=%u events=%u alarms=%u acknowledgements=%u unknown=%u invalidations=%u saturated=%u final=%u\n",
        epoch,now,totals.events,totals.alarms,totals.acknowledgements,totals.unknown,
        totals.invalidations,(uint64_t)totals.saturated,(uint64_t)final);
}
static void runtime_close(void) {
    check_foreign_event();
    mask_sources();
    if(!acpi_worker_drain(&runtime.worker,3000))unsafe("pre-disconnect drain");
    /* Stop can race an owned event. Check its lease after the masked worker
     * has drained, not while a resumable EC transfer is still in flight. */
    if(smbus_study) {
        require_recovery_resources();
    }
    if(smbus_capture || mounted_home) {
        uint64_t f=irq_save();
        bool idle=acpi_work_idle(&runtime.worker.work) && !runtime.query.pending && !runtime.query_scheduled &&
            !runtime.sci_counts[0].in_flight && !runtime.sci_counts[1].in_flight && !runtime.sci_counts[2].in_flight;
        console_printf(mounted_home?"battery-home-return: idle=%u held=%u; sources_masked=%u\n":"battery-smbus-return: idle=%u parked=%u; sources_masked=%u\n",
            (uint64_t)idle,(uint64_t)(mounted_home?mounted_home_owned():smbus_capture_parked()),(uint64_t)sources_quiet());
        if(!idle || !smbus_owned() || !sources_quiet())unsafe("SMBus return boundary");
        require_recovery_resources();irq_restore(f);
    }
    continuous_events_report(true);
    if(smbus_controls)console_printf("battery-controls-events: epoch=%u events=%u alarms=%u acknowledgements=%u unknown=%u invalid_through=%u generation=%u; drained=yes\n",
        service.cache.value.epoch,runtime.events.totals.events,runtime.events.totals.alarms,runtime.events.totals.acknowledgements,
        runtime.events.totals.unknown,runtime.smbus_invalid_through,service.cache.value.sample.generation);
    if(uacpi_mask_gpe(NULL,runtime.platform.ec.gpe)||uacpi_uninstall_gpe_handler(NULL,runtime.platform.ec.gpe,gpe_handler))unsafe("GPE uninstall");runtime.gpe=false;
    operation(disconnect_work,3000,"EC disconnect");
    if(runtime.notify&&uacpi_uninstall_notify_handler(uacpi_namespace_root(),notify_handler))unsafe("Notify uninstall");
    if(!acpi_worker_drain(&runtime.worker,3000))unsafe("pre-reset drain");
    /* Reset retires events before its own wait callback: owner already drained. */
    uacpi_state_reset();
    if(!acpi_worker_stop(&runtime.worker,1000)||runtime.heap||runtime.errors||runtime.denied||runtime.sci.phase!=SCI_OWNER_OFF)unsafe("runtime retire");
    if(!(runtime.platform.original_control&1)&&(inw(runtime.platform.ec.pm1_control)&1)) {
        outb(runtime.platform.ec.smi_command,runtime.platform.ec.acpi_disable);
        uint64_t end=timer_monotonic_us()+1000000;
        while((inw(runtime.platform.ec.pm1_control)&1)&&timer_monotonic_us()<end)timer_delay_us(100);
        if(inw(runtime.platform.ec.pm1_control)&1)unsafe("ACPI mode restore");
    }
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(!battery_event_hold_restore(h,event_ops))unsafe("foreign event restore");
        if(h->mask)console_printf("battery: foreign-event restored enable=%x original=%x protected_writes=%u\n",
            (uint64_t)h->enable,(uint64_t)h->original,h->protected_writes);
    }
    if(owner==OWNER_SERVICE)service.finishing=true;
    runtime.engaged=false;
    sci_report("retired");
}
static bool stop_requested(void *arg) {(void)arg;return service.stop;}
static void service_finish(enum battery_service_state state,enum battery_service_error error) {
    uint64_t f=irq_save();
    service.cache.value.state=state;service.cache.value.error=error;
    service.cache.value.flags&=~BATTERY_CACHE_SAMPLING;
    service.running=false;service.finishing=false;owner=OWNER_NONE;
    scheduler_wake_all(&service.progress);
    irq_restore(f);
    /* Caller must return immediately: another start can now replace state. */
}
static void query_return_boundary(void) {
    if(!(query_study?battery_platform_workspace_study_allowed():native_query_return_scope()) ||
       owner!=OWNER_SERVICE || scheduler_current_thread_id()!=service.identity ||
       !runtime.stopping || !service.stop || service.quarantined)unsafe("query return context");
    if(!acpi_worker_drain(&runtime.worker,3000))unsafe("query return drain");
    uint64_t f=irq_save();
    const struct battery_query_receipt *r=&runtime.query_receipt;
    bool idle=acpi_work_idle(&runtime.worker.work) && !runtime.query.pending &&
        !runtime.query_scheduled && !runtime.budget.active && !runtime.errors && !runtime.denied &&
        !runtime.sci_counts[0].in_flight && !runtime.sci_counts[1].in_flight && !runtime.sci_counts[2].in_flight;
    console_printf("battery-query-return: sequence=%u generation=%u code=%u transfer=%u aml_calls=%u aml_returns=%u effects=%u completed_before=%u completed_after=%u notifications_before=%u notifications_after=%u idle=%u; refused=yes\n",
        r->sequence,r->generation,(uint64_t)r->code,(uint64_t)r->transfer_done,r->aml_calls,r->aml_returns,r->effects,
        r->completed_before,runtime.worker.completed,r->notifications_before,runtime.notifications,(uint64_t)idle);
    if(!battery_query_returned(r,runtime.worker.completed,idle) || !sources_quiet() ||
       (scheduler_io_active() && !scheduler_io_busy()))unsafe("query return receipt");
    require_recovery_resources();
    irq_restore(f);
}
static void service_main(void *arg) {
    (void)arg;
    service.identity=scheduler_current_thread_id();
    if(service.stop){service_finish(BATTERY_SERVICE_STOPPED,BATTERY_SERVICE_OK);return;}
    if(!runtime_open(service.rsdp,service.map,true,true)) {
        service_finish(BATTERY_SERVICE_UNAVAILABLE,BATTERY_SERVICE_PLATFORM);return;
    }
    uint64_t f=irq_save();
    if(!service.stop)service.cache.value.state=BATTERY_SERVICE_ACTIVE;
    irq_restore(f);
    uint64_t next_sample=0;
    for(;;) {
        f=irq_save();bool stopping=service.stop;irq_restore(f);
        if(stopping)break;
        check_foreign_event();
        f=irq_save();if(pending_query())queue_query();
        bool expired=runtime.budget.active&&!ec_query_budget_check(&runtime.budget,timer_monotonic_us());
        irq_restore(f);
        if(expired)unsafe("independent query deadline");
        uint64_t now=timer_monotonic_us();
        f=irq_save();bool refresh=runtime.smbus_refresh;runtime.smbus_refresh=false;irq_restore(f);
        if(now>=next_sample || refresh) {
            operation(sample_work,25000,"sample");
            if(runtime.query_receipt.refused)break;
            f=irq_save();
            service.cache.value.flags&=~BATTERY_CACHE_SAMPLING;
            /* A concurrent stop changes state to STOPPING, excluding late
             * publications while still permitting in-flight IO to retire. */
            if(!(smbus_study || smbus_capture || mounted_home) || runtime.sampler.sample.generation>runtime.smbus_invalid_through)
            (void)battery_cache_publish(&service.cache,&runtime.sampler.sample,
                runtime.ac_present,runtime.ac_coherent,runtime.ac_sampled_us,timer_monotonic_us());
            irq_restore(f);
            next_sample=timer_monotonic_us()+BATTERY_SERVICE_INTERVAL_US;
        }
        /* One service-thread writer orders periodic and final records. A
         * preempted session-owner snapshot must not appear after retirement. */
        continuous_events_report(false);
        /* Retain the independent query watchdog while sleeping between polls.
         * The engaged owner/worker may run through storage service windows. */
        if(!scheduler_wait_timeout(&service.wake,stop_requested,NULL,timer_ticks()+5))
            timer_wait_ticks(1);
    }
    bool query_failed=runtime.query_receipt.refused;
    if(query_failed)query_return_boundary();
    runtime_close();
    /* Force the study through post-close scheduling while unrelated I/O is owned. */
    if(query_failed)scheduler_yield();
    console_printf("battery-service: retired epoch=%u attempts=%u rejected=%u queries=%u notifications=%u attached=%u detached=%u heap=0 errors=0 denied=0\n",
        service.cache.value.epoch,service.cache.value.attempts,service.cache.value.rejected,
        runtime.queries,runtime.notifications,runtime.attached,runtime.detached);
    service_finish(query_failed?BATTERY_SERVICE_FAULTED:BATTERY_SERVICE_STOPPED,service.cache.value.error);
}
bool battery_service_start(void *rsdp,struct limine_memmap_response *map) {
    uint64_t f=irq_save();
    if(owner!=OWNER_NONE||!(f&(1ull<<9))||!scheduler_current_thread_id()||
       !timer_reference_ready()||service.cache.value.epoch==UINT64_MAX) {
        irq_restore(f);return false;
    }
    owner=OWNER_SERVICE;
    battery_cache_begin(&service.cache,service.cache.value.epoch+1);
    service.stop=false;service.finishing=false;service.running=true;service.identity=0;service.rsdp=rsdp;service.map=map;
    /* Wait queues retain sleepers from an earlier epoch until they wake. Do
     * not clear their masks when admitting a new lifetime. */
    bool ok=scheduler_spawn_io_service("battery-owner",service_main,NULL,service_io_ready);
    if(!ok)service_finish(BATTERY_SERVICE_UNAVAILABLE,BATTERY_SERVICE_SPAWN);
    irq_restore(f);return ok;
}
static bool service_retired(void *arg) {
    return !service.running||service.cache.value.epoch!=*(uint64_t *)arg;
}
bool battery_service_stop(uint64_t timeout_ms) {
    if(service.quarantined || timeout_ms>UINT64_MAX/1000)return false;
    uint64_t f=irq_save(),epoch=service.cache.value.epoch;
    if(!service.running){irq_restore(f);return true;}
    service.stop=true;if(!runtime.query_receipt.refused)service.cache.value.state=BATTERY_SERVICE_STOPPING;
    scheduler_wake_all(&service.wake);
    uint64_t caller=scheduler_current_thread_id();
    if(!timeout_ms||!(f&(1ull<<9))||caller==service.identity||caller==runtime.worker.identity){irq_restore(f);return false;}
    uint64_t now=timer_monotonic_us();
    if(now>UINT64_MAX-timeout_ms*1000){irq_restore(f);return false;}
    uint64_t end=now+timeout_ms*1000;
    for(;;) {
        if(service_retired(&epoch)){irq_restore(f);return true;}
        now=timer_monotonic_us();
        if(now>=end){irq_restore(f);return false;}
        uint64_t ticks=(end-now)/10000+2,due=timer_ticks();
        due=ticks>UINT64_MAX-due?UINT64_MAX:due+ticks;
        irq_restore(f);
        if(!scheduler_wait_timeout(&service.progress,service_retired,&epoch,due))return false;
        f=irq_save();
    }
}
void battery_service_status(struct battery_service_status *out) {
    if(!out)return;
    uint64_t f=irq_save();
    battery_cache_read(&service.cache,timer_monotonic_us(),out);
    irq_restore(f);
}
static struct battery_control_mailbox controls;
int battery_control_request(uint64_t group,unsigned operation,uint64_t epoch,struct srv_battery_control *out) {
    uint64_t f=irq_save();struct battery_service_status status;
    battery_service_status(&status);
    int result=battery_control_enqueue(&controls,group,operation,epoch,&status);
    *out=battery_control_view(&controls,group,status.epoch);
    irq_restore(f);return result;
}
/* Called only by the scoped session owner, outside syscalls and storage work. */
static void controls_dispatch(void *rsdp,struct limine_memmap_response *map) {
    uint64_t f=irq_save();unsigned op=battery_control_take(&controls);
    uint64_t epoch=controls.expected_epoch,serial=controls.serial;irq_restore(f);
    if(!op)return;
    struct battery_service_status status;battery_service_status(&status);
    int result=0;
    if(status.epoch!=epoch)result=SRV_BATTERY_CONTROL_STALE;
    else if(op==SRV_BATTERY_START) {
        if(!battery_service_start(rsdp,map))result=SRV_BATTERY_CONTROL_FAILED;
    } else (void)battery_service_stop(0);
    f=irq_save();battery_control_complete(&controls,result);irq_restore(f);
    console_printf("battery-control: dispatched serial=%u operation=%u epoch=%u result=%d\n",serial,(uint64_t)op,epoch,(int64_t)result);
}
static bool run_battery(void *rsdp,struct limine_memmap_response *map,bool transitions) {
    if(!claim_diagnostic())return false;
    if(!runtime_open(rsdp,map,transitions,false)){release_owner();return false;}
    struct battery_transition trial={.phase_started=timer_monotonic_us()};
    struct transition_proof {struct sbs_snapshot sample;uint64_t first_generation,first_us,ac_us;} proof[3]={0};
    bool complete=!transitions;
    static const char *instructions[]={"Keep AC CONNECTED; collecting baseline", "UNPLUG AC now; leave the USB drive connected", "RECONNECT AC now; leave the USB drive connected"};
    for(unsigned sample=0;transitions?trial.phase<3&&!trial.failed:sample<5;sample++) {
        if(transitions) {
            console_printf("\nBATTERY TEST %u/3: %s\n",(uint64_t)trial.phase+1,instructions[trial.phase]);
            console_write("Each phase needs 3 stable samples within 120 seconds. Failure stays on screen.\n");console_sync();
        }
        uint64_t flags=irq_save();if(pending_query())queue_query();irq_restore(flags);
        operation(sample_work,25000,"sample");
        const struct sbs_snapshot *s=&runtime.sampler.sample;
        if(!transitions)console_printf("battery: sample=%u ac_present=%u soc=%u voltage_uv=%u current_ua=%d capacity=%u full=%u unit=%u temp_mc=%d valid=%x age_us=%u ac_us=%u start_us=%u end_us=%u\n",
            s->generation,(uint64_t)runtime.ac_present,(uint64_t)s->relative_soc,s->voltage_uv,s->current_ua,
            s->remaining_capacity,s->full_capacity,(uint64_t)s->capacity_unit,(int64_t)s->temperature_mc,(uint64_t)s->valid,timer_monotonic_us()-s->completed_us,runtime.ac_sampled_us,s->started_us,s->completed_us);
        if(transitions) {
            unsigned before=trial.phase;
            int result=battery_transition_step(&trial,s,runtime.ac_present,runtime.ac_coherent,runtime.ac_sampled_us,timer_monotonic_us());
            console_printf("battery-transition: phase=%u ac=%u coherent=%u relative_soc=%u current_ua=%d stable=%u\n",
                (uint64_t)before,(uint64_t)runtime.ac_present,(uint64_t)runtime.ac_coherent,(uint64_t)s->relative_soc,s->current_ua,(uint64_t)trial.stable);
            if(trial.phase!=before)proof[before]=(struct transition_proof){*s,trial.first_generation,trial.first_us,runtime.ac_sampled_us};
            if(result) {complete=result>0;break;}
        }
        for(unsigned tick=0;tick<100;tick++) {
            timer_wait_ticks(1);
            check_foreign_event();
            uint64_t f=irq_save();if(pending_query())queue_query();
            bool expired=runtime.budget.active&&!ec_query_budget_check(&runtime.budget,timer_monotonic_us());irq_restore(f);
            if(expired)unsafe("independent query deadline");
        }
    }
    runtime_close();
    if(transitions) {
        /* Repeat compact accepted-phase evidence at the end so bounded log tails
         * retain the whole result even after a long wait for the user. */
        for(unsigned i=0;i<trial.phase;i++) {
            const struct transition_proof *p=&proof[i];const struct sbs_snapshot *s=&p->sample;
            console_printf("battery-transition: accepted=%u stable=3 first_gen=%u last_gen=%u first_us=%u ac_us=%u start_us=%u end_us=%u relative_soc=%u voltage_uv=%u current_ua=%d capacity=%u full=%u design=%u unit=%u valid=%x\n",
                (uint64_t)i,p->first_generation,s->generation,p->first_us,p->ac_us,s->started_us,s->completed_us,(uint64_t)s->relative_soc,s->voltage_uv,s->current_ua,s->remaining_capacity,s->full_capacity,s->design_capacity,(uint64_t)s->capacity_unit,(uint64_t)s->valid);
        }
        console_printf("battery-transition: %s phases=%u samples=%u notifications=%u queries=%u attached=%u detached=%u heap=0 errors=0 denied=0; retired=yes\n",
            complete?"PASS":"INCOMPLETE",(uint64_t)trial.phase,runtime.sampler.sample.generation,runtime.notifications,runtime.queries,runtime.attached,runtime.detached);
    } else console_printf("battery: PASS samples=5 notifications=%u queries=%u attached=%u detached=%u heap=0 errors=0 denied=0; physical accuracy NOT_TESTED\n",
        runtime.notifications,runtime.queries,runtime.attached,runtime.detached);
    release_owner();return complete;
}
bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map){return run_battery(rsdp,map,false);}
bool battery_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map){return run_battery(rsdp,map,true);}

bool aml_probe_run(void *rsdp,struct limine_memmap_response *map){(void)rsdp;(void)map;console_write("aml-probe: unavailable in battery diagnostic kernel\n");return false;}
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_memmap_request memmap_request;
static bool same_sample(const struct sbs_snapshot *a,const struct sbs_snapshot *b) {
#define SAME(field) if(a->field!=b->field)return false
    SAME(started_us);SAME(completed_us);SAME(generation);SAME(voltage_uv);
    SAME(remaining_capacity);SAME(full_capacity);SAME(design_capacity);SAME(current_ua);
    SAME(temperature_mc);SAME(valid);SAME(capacity_unit);SAME(specification);
    SAME(mode);SAME(status);SAME(relative_soc);SAME(minutes_empty);SAME(minutes_full);
#undef SAME
    for(unsigned i=0;i<SBS_WORD_COUNT;i++)if(a->raw[i]!=b->raw[i])return false;
    return true;
}
/* Diagnostic-only real USB traffic. The selected workspace has been flushed,
 * unmounted and write-revoked before admission. Read the raw parent directly so
 * the block cache cannot turn this into a memory-only scheduling experiment. */
static struct {
    struct block_device *volume,*device;
    uint8_t baseline[4096],buffer[4096];
    uint64_t reads,commands,busy,progress,bursts,max_us,max_read_us;
    uint64_t steps,max_step_us,slowest_step;
    struct workspace_image_profile profile;
    bool cycle_busy;
} usb_read_test;
static void filesystem_profile_print(void) {
    const struct workspace_image_profile *p=&usb_read_test.profile;
    uint64_t io_us=0;
    for(unsigned phase=0;phase<EXFAT_IO_PHASE_COUNT;phase++)
        for(unsigned op=0;op<WORKSPACE_IMAGE_IO_COUNT;op++)io_us+=p->io[phase][op].total_us;
    console_printf("battery-filesystem-profile: cycle=%u step=%u elapsed_us=%u io_us=%u\n",
        p->cycle,p->step,p->elapsed_us,io_us);
    for(unsigned phase=0;phase<EXFAT_IO_PHASE_COUNT;phase++) {
        const struct workspace_image_io_sample *r=&p->io[phase][WORKSPACE_IMAGE_READ];
        const struct workspace_image_io_sample *w=&p->io[phase][WORKSPACE_IMAGE_WRITE];
        const struct workspace_image_io_sample *f=&p->io[phase][WORKSPACE_IMAGE_FLUSH];
        console_printf("battery-filesystem-phase: phase=%u reads=%u read_bytes=%u read_us=%u read_max_us=%u writes=%u write_bytes=%u write_us=%u write_max_us=%u flushes=%u flush_us=%u flush_max_us=%u\n",
            (uint64_t)phase,r->count,r->bytes,r->total_us,r->max_us,w->count,w->bytes,w->total_us,w->max_us,f->count,f->total_us,f->max_us);
    }
}

static bool usb_read_prepare(void) {
    usb_read_test=(typeof(usb_read_test)){0};
    usb_read_test.volume=workspace_parked_device();
    usb_read_test.device=block_parent(usb_read_test.volume);
    struct block_device *d=usb_read_test.device;
    return d && d->read && block_device_size(d)>=4096 &&
        d->read(d,0,usb_read_test.baseline,4096);
}
static bool usb_read_burst(unsigned mode) {
    struct block_device *d=usb_read_test.device;
    if(workspace_parked_device()!=usb_read_test.volume)return false;
    uint64_t flags=irq_save();
    if(!scheduler_io_begin()){irq_restore(flags);return false;}
    struct usb_storage *storage=d->private_data;
    uint64_t start=timer_monotonic_us(),commands=storage->commands;
    struct battery_service_status before,after;
    battery_service_status(&before);after=before;
    bool ok=true;
    do {
        bool complete=true;
        if(mode==3) {
            uint64_t step_start=timer_monotonic_us();
            if(!workspace_image_step(usb_read_test.reads,&complete)){ok=false;break;}
            uint64_t elapsed=timer_monotonic_us()-step_start;
            if(elapsed>usb_read_test.max_step_us) {
                if(!workspace_image_last_profile(&usb_read_test.profile)){ok=false;break;}
                usb_read_test.max_step_us=elapsed;
                usb_read_test.slowest_step=usb_read_test.steps%WORKSPACE_IMAGE_STEPS;
            }
            usb_read_test.steps++;
        } else if(mode==2) {
            for(unsigned i=0;i<4096;i++)usb_read_test.buffer[i]=(uint8_t)(i+usb_read_test.reads);
            if(!workspace_scratch_cycle(usb_read_test.buffer)){ok=false;break;}
        } else {
            if(!d->read(d,0,usb_read_test.buffer,4096)){ok=false;break;}
            for(unsigned i=0;i<4096;i++)if(usb_read_test.buffer[i]!=usb_read_test.baseline[i]){ok=false;break;}
        }
        if(!ok)break;
        if(complete)usb_read_test.reads++;
        uint64_t read_start=timer_monotonic_us();
        battery_service_status(&after);
        uint64_t read_us=timer_monotonic_us()-read_start;
        if(read_us>usb_read_test.max_read_us)usb_read_test.max_read_us=read_us;
        if(mode==3) {
            if((before.flags|after.flags)&BATTERY_CACHE_SAMPLING)usb_read_test.cycle_busy=true;
            if(complete) {
                if(usb_read_test.cycle_busy)usb_read_test.busy++;
                usb_read_test.cycle_busy=false;
            }
            /* Return to the reader loop and release exclusion after ONE operation.
             * Never combine many fast operations into another long scope. */
            break;
        }
        if(after.flags&BATTERY_CACHE_SAMPLING)usb_read_test.busy++;
        (void)scheduler_io_yield();
    } while(timer_monotonic_us()-start<250000);
    /* Observe progress while exclusion is still held, before releasing it. */
    battery_service_status(&after);
    if(after.epoch==before.epoch && after.sample.generation>before.sample.generation)usb_read_test.progress++;
    usb_read_test.commands+=storage->commands-commands;
    usb_read_test.bursts++;
    uint64_t elapsed=timer_monotonic_us()-start;
    if(elapsed>usb_read_test.max_us)usb_read_test.max_us=elapsed;
    scheduler_io_end();irq_restore(flags);
    return ok;
}
static bool service_test(void *rsdp,struct limine_memmap_response *map,bool stale,bool refusal,unsigned usb_mode) {
    bool usb_reads=usb_mode!=0,usb_writes=usb_mode==2,filesystem=usb_mode==3;
    uint64_t workload_started=0,workload_elapsed=0;
    uint64_t reads=0,busy_reads=0,max_read_us=0,epoch=0;
    bool saw_stale=false,recovered=false;
    struct battery_service_status s,previous={0};
    const char *failure="start";
    console_write("Battery service test: automatic; keep AC and USB connected. No input required.\n");
    if(usb_reads&&(!usb_read_prepare()||(usb_writes&&!workspace_scratch_ready())||(filesystem&&!workspace_image_ready()))){failure="USB read preparation";goto failed;}
    if(!battery_service_start(rsdp,map))goto failed;
    if(refusal) {
        uint64_t end=timer_monotonic_us()+10000000;
        do {
            battery_service_status(&s);reads++;
            if(s.state==BATTERY_SERVICE_UNAVAILABLE) {
                if(!battery_service_stop(0))goto failed;
                console_printf("battery-service-test: PASS unavailable epoch=%u reads=%u; no live owner\n",s.epoch,reads);
                return true;
            }
            if(s.state==BATTERY_SERVICE_ACTIVE||timer_monotonic_us()>=end)goto failed;
            uacpi_kernel_sleep(10); /* Block this reader; HLT would keep it runnable. */
        } while(true);
    }
    /* Close admission immediately. A zero wait cannot release ownership or
     * permit a replacement lifecycle before the owner acknowledges retirement. */
    failure="immediate stop";
    if(battery_service_start(rsdp,map)||battery_service_stop(0)||
       !battery_service_stop(35000))goto failed;
    battery_service_status(&s);
    if(s.state!=BATTERY_SERVICE_STOPPED||(s.flags&BATTERY_CACHE_FRESH))goto failed;
    epoch=s.epoch;
    failure="restart";
    if(!battery_service_start(rsdp,map)||battery_service_start(rsdp,map)||
       battery_diagnostic_run(rsdp,map))goto failed;
    workload_started=filesystem?0:timer_monotonic_us();
    uint64_t deadline=timer_monotonic_us()+(filesystem?120000000:30000000);
    for(;;) {
        for(unsigned i=0;i<64;i++) {
            uint64_t before=timer_monotonic_us();
            battery_service_status(&s);
            uint64_t elapsed=timer_monotonic_us()-before;
            if(elapsed>max_read_us)max_read_us=elapsed;
            reads++;if(s.flags&BATTERY_CACHE_SAMPLING)busy_reads++;
            failure="snapshot epoch";
            if(s.epoch!=epoch+1)goto failed;
            if(s.flags&BATTERY_CACHE_VALID) {
                failure="snapshot consistency";
                if(previous.sample.generation==s.sample.generation&&previous.epoch==s.epoch&&
                   !same_sample(&previous.sample,&s.sample))goto failed;
                if((s.flags&BATTERY_CACHE_STALE)&&s.rejected)saw_stale=true;
                if(saw_stale&&(s.flags&BATTERY_CACHE_FRESH))recovered=true;
            }
            previous=s;
        }
        if(usb_reads&&(!filesystem||s.state==BATTERY_SERVICE_ACTIVE)) {
            if(filesystem&&!workload_started)workload_started=timer_monotonic_us();
            failure="USB read overlap";
            if(!usb_read_burst(usb_mode))goto failed;
        }
        if(s.state==BATTERY_SERVICE_UNAVAILABLE){failure="unexpected unavailable";goto failed;}
        workload_elapsed=workload_started?timer_monotonic_us()-workload_started:0;
        if((s.flags&BATTERY_CACHE_FRESH)&&s.sample.generation>=(filesystem?10:5)&&
           (!filesystem||(workload_elapsed>=60000000&&!workspace_image_pending())))break;
        failure="sample deadline";
        if(timer_monotonic_us()>=deadline)goto failed;
        uacpi_kernel_sleep(10); /* Block this reader; HLT would keep it runnable. */
    }
    if(usb_reads) {
        reads+=usb_read_test.reads;busy_reads+=usb_read_test.busy;
        if(usb_read_test.max_read_us>max_read_us)max_read_us=usb_read_test.max_read_us;
        failure="USB progress proof";
        if(!usb_read_test.reads || usb_read_test.commands<usb_read_test.reads*((usb_writes||filesystem)?3:1) ||
           !usb_read_test.busy || !usb_read_test.progress)goto failed;
    }
    failure="stale recovery";
    if(refusal||!busy_reads||reads<1000||(stale&&(!saw_stale||!recovered))||(!stale&&s.rejected))goto failed;
    console_printf("battery-service-test: sample epoch=%u generation=%u flags=%u age_us=%u ac_us=%u start_us=%u end_us=%u relative_soc=%u voltage_uv=%u current_ua=%d capacity=%u full=%u design=%u unit=%u valid=%u\n",
        s.epoch,s.sample.generation,(uint64_t)s.flags,s.age_us,s.ac_sampled_us,s.sample.started_us,s.sample.completed_us,
        (uint64_t)s.sample.relative_soc,s.sample.voltage_uv,s.sample.current_ua,s.sample.remaining_capacity,
        s.sample.full_capacity,s.sample.design_capacity,(uint64_t)s.sample.capacity_unit,(uint64_t)s.sample.valid);
    failure="active retirement";
    if(!battery_service_stop(35000))goto failed;
    battery_service_status(&s);
    if(s.state!=BATTERY_SERVICE_STOPPED||(s.flags&BATTERY_CACHE_FRESH))goto failed;
    epoch=s.epoch;
    failure="in-flight restart";
    if(!battery_service_start(rsdp,map))goto failed;
    deadline=timer_monotonic_us()+10000000;
    do {
        battery_service_status(&s);
        if(timer_monotonic_us()>=deadline)goto failed;
        uacpi_kernel_sleep(10); /* Block this reader; HLT would keep it runnable. */
    } while(!(s.flags&BATTERY_CACHE_SAMPLING));
    failure="in-flight stop";
    if(battery_service_stop(0))goto failed;
    battery_service_status(&s);
    if((s.state!=BATTERY_SERVICE_STOPPING&&s.state!=BATTERY_SERVICE_STOPPED)||(s.flags&BATTERY_CACHE_FRESH))goto failed;
    bool quick=battery_service_stop(1);
    if(!quick) {
        battery_service_status(&s);
        if(s.state!=BATTERY_SERVICE_STOPPING&&s.state!=BATTERY_SERVICE_STOPPED)goto failed;
    }
    if(!battery_service_stop(35000))goto failed;
    battery_service_status(&s);
    if(s.state!=BATTERY_SERVICE_STOPPED||s.epoch!=epoch+1||(s.flags&BATTERY_CACHE_FRESH))goto failed;
    if(usb_writes) {
        failure="USB scratch retirement";
        if(!workspace_scratch_end())goto failed;
        console_printf("battery-usb-write: PASS cycles=%u commands=%u busy_cycles=%u progress_bursts=%u bursts=%u max_burst_us=%u bytes_per_cycle=4096; retired=yes revoked=yes\n",
            usb_read_test.reads,usb_read_test.commands,usb_read_test.busy,usb_read_test.progress,
            usb_read_test.bursts,usb_read_test.max_us);
    }
    if(filesystem) {
        failure="isolated filesystem retirement";
        if(!workspace_image_close())goto failed;
        console_printf("battery-filesystem: PASS cycles=%u commands=%u busy_cycles=%u progress_bursts=%u bursts=%u max_burst_us=%u elapsed_us=%u steps=%u max_step_us=%u slowest_step=%u profile=1; retired=yes revoked=yes\n",
            usb_read_test.reads,usb_read_test.commands,usb_read_test.busy,usb_read_test.progress,
            usb_read_test.bursts,usb_read_test.max_us,workload_elapsed,usb_read_test.steps,
            usb_read_test.max_step_us,usb_read_test.slowest_step);
        filesystem_profile_print();
    }
    if(usb_reads&&!usb_writes&&!filesystem)console_printf("battery-usb-read: PASS reads=%u commands=%u busy_reads=%u progress_bursts=%u bursts=%u max_burst_us=%u bytes_per_read=4096 writes=0; retired=yes\n",
        usb_read_test.reads,usb_read_test.commands,usb_read_test.busy,usb_read_test.progress,
        usb_read_test.bursts,usb_read_test.max_us);
    console_printf("battery-service-test: PASS reads=%u busy_reads=%u max_read_us=%u stale=%u recovered=%u stop_timeout=%u epochs=%u; retired=yes\n",
        reads,busy_reads,max_read_us,(uint64_t)saw_stale,(uint64_t)recovered,(uint64_t)!quick,s.epoch);
    return true;
failed:
    if(!battery_service_stop(35000))unsafe("service test retirement");
    if(usb_writes)(void)workspace_scratch_end();
    if(filesystem)(void)workspace_image_close();
    console_printf("battery-service-test: FAIL %s reads=%u busy_reads=%u\n",failure,reads,busy_reads);
    return false;
}
bool battery_service_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return service_test(rsdp,map,false,false,false);
}
bool battery_usb_read_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return service_test(rsdp,map,false,false,true);
}
bool battery_filesystem_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return service_test(rsdp,map,false,false,3);
}
bool battery_usb_write_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return service_test(rsdp,map,false,false,2);
}
/* The compositor owns its client and optional file worker. The diagnostic owner can interleave
 * one exclusive image operation per iteration, releasing I/O exclusion before
 * waiting or process cleanup. Keep the runtime and lease until all users retire. */
struct desktop_tree { uint64_t pids[PROCESS_MAX_PROCESSES];unsigned count; };
static bool tree_has(const struct desktop_tree *t,uint64_t pid) {
    for(unsigned i=0;i<t->count;i++)if(t->pids[i]==pid)return true;
    return false;
}
static void desktop_tree_scan(struct desktop_tree *t) {
    for(unsigned pass=0;pass<PROCESS_MAX_PROCESSES;pass++) {
        unsigned before=t->count;uint64_t index=0,pid,parent;
        while((index=process_list_details(index,&pid,NULL,NULL,&parent,NULL))!=0) {
            /* These fixed diagnostic programs inherit their kernel-created
             * root group. It also identifies a worker whose parent was reaped
             * between polls, before its PID could enter the retained tree. */
            if((!tree_has(t,parent) && process_group(pid)!=t->pids[0]) || tree_has(t,pid))continue;
            if(t->count==PROCESS_MAX_PROCESSES)unsafe("desktop descendant bound");
            t->pids[t->count++]=pid;
        }
        if(t->count==before)return;
    }
    unsafe("desktop descendant scan");
}
static bool desktop_children_stop(struct desktop_tree *t,bool kill) {
    desktop_tree_scan(t);
    uint64_t index=0,pid;const char *state;bool live=false;
    while((index=process_list_details(index,&pid,NULL,&state,NULL,NULL))!=0) {
        if(!tree_has(t,pid)||state[0]=='e')continue;
        live=true;if(kill)(void)process_signal_pid(pid,SRV_SIGNAL_KILL);
    }
    return !live;
}
/* Explicit VM diagnostics exercise the storage boundary while the runtime is
 * active. These refusals must precede even scratch-file allocation or sync. */
static void mounted_study_guard(void) {
    if(!workspace_mount_hold_valid(smbus_workspace_hold) || workspace_unmount() ||
       workspace_scratch_prepare() || workspace_image_open() ||
       !workspace_mount_hold_valid(smbus_workspace_hold))unsafe("mounted workspace hold");
    console_printf("battery-workspace-hold: guarded token=%u unmount=blocked scratch=blocked image=blocked\n",smbus_workspace_hold);
}
static bool mounted_study_acquire(void) {
    if(smbus_workspace_hold)return false;
    smbus_workspace_hold=workspace_mount_hold_acquire();
    if(!smbus_workspace_hold)return false;
    console_printf("battery-workspace-hold: acquired token=%u\n",smbus_workspace_hold);return true;
}
static void mounted_study_release(void) {
    if(!workspace_mount_hold_release(smbus_workspace_hold))unsafe("mounted workspace release");
    console_printf("battery-workspace-hold: released token=%u\n",smbus_workspace_hold);
    smbus_workspace_hold=0;
}
static bool desktop_test(void *rsdp,struct limine_memmap_response *map,unsigned fault,bool text,bool filesystem,bool appfiles,bool workspace) {
    const char *failure="workspace mounted";
    struct battery_service_status before={0},after={0};
    uint64_t pid=0,status=UINT64_MAX,reads=0,busy=0,start=0,end=0;
    bool started=false,reaped=false,ok=false;
    uint64_t first_step=0,last_step=0,workload_end=0;
    struct desktop_tree tree={0};
    if(workspace) {
        failure="workspace VM admission";
        if(!battery_platform_workspace_study_allowed() || !workspace_active() || workspace_image_busy() ||
           (smbus_study && !workspace_mount_hold_valid(smbus_workspace_hold)))goto done;
    } else if(workspace_active())goto done;
    if(filesystem || (appfiles && !workspace)) {
        failure="filesystem preparation";
        if(!workspace_image_ready() || !usb_read_prepare())goto done;
    }
    failure="start";
    if(!battery_service_start(rsdp,map))goto done;
    started=true;
    uint64_t deadline=timer_monotonic_us()+15000000;
    for(;;) {
        battery_service_status(&before);
        if(before.state==BATTERY_SERVICE_ACTIVE && (before.flags&BATTERY_CACHE_FRESH))break;
        if(before.state==BATTERY_SERVICE_UNAVAILABLE || timer_monotonic_us()>=deadline) {
            failure="unavailable or startup deadline";goto done;
        }
        uacpi_kernel_sleep(10);
    }
    if(workspace && smbus_study)mounted_study_guard();
    failure="desktop spawn";
    int64_t child=process_spawn_background_elf_args(fault==1?"/fat/bin/missing-coexist":"/fat/bin/deskd",workspace?"--coexist-workspace":appfiles?"--coexist-files":text?"--coexist-text":"--coexist-capture");
    if(child<=0)goto done;
    pid=(uint64_t)child;tree.pids[tree.count++]=pid;start=timer_monotonic_us();
    if(process_group(pid)!=pid){failure="desktop process group";goto done;}
    deadline=start+(fault==2?500000:90000000);
    for(;;) {
        desktop_tree_scan(&tree);
        battery_service_status(&after);reads++;
        if(after.flags&BATTERY_CACHE_SAMPLING)busy++;
        failure="service progress";
        if(after.state!=BATTERY_SERVICE_ACTIVE || after.epoch!=before.epoch || after.rejected || after.sample.generation<before.sample.generation)goto done;
        if(!reaped) {
            int64_t waited=process_wait(pid,&status,true);
            if(waited==(int64_t)pid){reaped=true;end=timer_monotonic_us();}
            else if(waited<0){failure="desktop ownership";goto done;}
        }
        if(reaped && (!filesystem || (timer_monotonic_us()-first_step>=60000000 && !workspace_image_pending())))break;
        if(timer_monotonic_us()>=deadline){failure="desktop deadline";goto done;}
        if(filesystem) {
            if(reaped && status!=0){failure="desktop completion";goto done;}
            failure="filesystem step";
            if(!first_step)first_step=timer_monotonic_us();
            if(!usb_read_burst(3))goto done;
            last_step=timer_monotonic_us();
            if(fault==3 && usb_read_test.steps>=3){failure="filesystem abort fixture";goto done;}
        }
        uacpi_kernel_sleep(10);
    }
    workload_end=timer_monotonic_us();failure="desktop completion";
    ok=status==0 && end>=start+59000000 && after.sample.generation>=before.sample.generation+10 && busy>0 && (after.flags&BATTERY_CACHE_FRESH);
    if(ok && filesystem) {
        failure="filesystem progress";
        ok=usb_read_test.reads>=2 && usb_read_test.busy>0 && usb_read_test.progress>0 &&
            usb_read_test.commands>=usb_read_test.reads*3 && usb_read_test.steps==usb_read_test.reads*WORKSPACE_IMAGE_STEPS &&
            last_step>=first_step+59000000 && !workspace_image_pending();
    }
done:
    /* No owned client or file worker may outlive the capture or its image lease.
     * Retain descendant identities even after their parents have exited. */
    if(pid) {
        if(!reaped)(void)process_signal_pid(pid,SRV_SIGNAL_KILL);
        uint64_t stop_deadline=timer_monotonic_us()+5000000;
        for(;;) {
            bool children=desktop_children_stop(&tree,!ok);
            if(!reaped)reaped=process_wait(pid,&status,true)==(int64_t)pid;
            if(children&&reaped)break;
            if(timer_monotonic_us()>=stop_deadline)unsafe("desktop process retirement");
            uacpi_kernel_sleep(10);
        }
    }
    if(started && smbus_files) {
        battery_service_status(&smbus_files_snapshot);
        console_printf("battery-native-smbus: snapshot generation=%u events=%u valid=%u fresh=%u\n",
            smbus_files_snapshot.sample.generation,runtime.events.totals.events,
            (uint64_t)!!(smbus_files_snapshot.flags&BATTERY_CACHE_VALID),
            (uint64_t)!!(smbus_files_snapshot.flags&BATTERY_CACHE_FRESH));
    }
    if(started && !service.quarantined && !battery_service_stop(35000))unsafe("desktop service retirement");
    if(service.quarantined) {
        console_printf("battery-workspace: QUARANTINED descendants=%u; clients_retired=yes runtime_retired=NO\n",tree.count);return false;
    }
    if((filesystem || (appfiles && !workspace)) && workspace_image_busy() && !workspace_image_close()) {ok=false;failure="filesystem retirement";}
    /* Defer the saved start record until competing process/runtime writers stop. */
    if(pid)console_printf("battery-desktop: started pid=%u epoch=%u generation=%u start_us=%u storage=%s\n",pid,before.epoch,before.sample.generation,start,workspace?"workspace":appfiles?"image":"ram");
    console_printf("battery-desktop: %s outcome=%s epoch=%u first_gen=%u last_gen=%u start_us=%u end_us=%u reads=%u busy_reads=%u process_status=%u; retired=yes\n",
        ok?"PASS":"FAIL",ok?"complete":failure,before.epoch,before.sample.generation,after.sample.generation,start,end,reads,busy,status);
    if(workspace)console_printf("battery-workspace: %s descendants=%u; retired=yes mounted=%s\n",ok?"PASS":"FAIL",tree.count,workspace_active()?"yes":"NO");
    else if(appfiles)console_printf("battery-app-files: %s descendants=%u; retired=yes revoked=%s\n",ok?"PASS":"FAIL",tree.count,workspace_image_busy()?"NO":"yes");
    if(filesystem) {
        if(ok) {
            console_printf("battery-filesystem: PASS cycles=%u commands=%u busy_cycles=%u progress_bursts=%u bursts=%u max_burst_us=%u elapsed_us=%u steps=%u max_step_us=%u slowest_step=%u profile=1; retired=yes revoked=yes\n",
                usb_read_test.reads,usb_read_test.commands,usb_read_test.busy,usb_read_test.progress,
                usb_read_test.bursts,usb_read_test.max_us,workload_end-first_step,usb_read_test.steps,
                usb_read_test.max_step_us,usb_read_test.slowest_step);
            filesystem_profile_print();
        }
        if(ok)console_printf("battery-text-load: PASS epoch=%u start_us=%u end_us=%u first_step_us=%u last_step_us=%u; retired=yes revoked=yes\n",
            before.epoch,start,workload_end,first_step,last_step);
        else console_printf("battery-text-load: FAIL outcome=%s\n",failure);
    }
    return ok;
}
/* Scope lasts for this desktop process group only. Explicit Home controls
 * retain the accepted native mount boundary across every service lifetime.
 * Cached reads never confer this capability, and no userspace owns firmware. */
static bool controls_test(void *rsdp,struct limine_memmap_response *map,bool automatic,bool workspace,unsigned files) {
    if(workspace?((!mounted_home && !battery_platform_workspace_study_allowed()) || !workspace_mount_hold_valid(smbus_workspace_hold) || workspace_image_busy()):workspace_active())return false;
    uint64_t f=irq_save();bool free=owner==OWNER_NONE && !controls.group;irq_restore(f);
    if(!free)return false;
    uint64_t lifetime=continuous_home?process_lifetime_create():0;
    if(continuous_home && !lifetime)return false;
    const char *arguments=desktop_transitions?"--battery-transition-capture":desktop_capture?"--battery-idle-capture":desktop_home?"--home-apps":continuous_home?"--battery-monitor":files==2?"--battery-control-files-cancel":files?"--battery-control-files":automatic?"--battery-controls":"--battery-controls-manual";
    int64_t child=lifetime?process_lifetime_spawn(lifetime,"/fat/bin/deskd",arguments):
        process_spawn_background_elf_args("/fat/bin/deskd",arguments);
    if(child<=0) {
        if(lifetime && (!process_lifetime_revoke(lifetime) || !process_lifetime_release(lifetime)))unsafe("desktop failed spawn ownership");
        return false;
    }
    if(lifetime)console_printf("battery-desktop-lifetime: acquired token=%u root=%u\n",lifetime,(uint64_t)child);
    uint64_t pid=(uint64_t)child,status=UINT64_MAX,first_serial=controls.serial;
    struct battery_service_status before;battery_service_status(&before);
    struct desktop_tree tree={.pids={pid},.count=1};
    f=irq_save();bool bound=process_group(pid)==pid && battery_control_bind(&controls,pid);irq_restore(f);
    uint64_t deadline=timer_monotonic_us()+110000000;bool reaped=false,ok=false,guarded=false;
    uint64_t capture_start=timer_monotonic_us(),first_generation=0,last_generation=0;
    if(bound && continuous_home) {
        bool started=battery_service_start(rsdp,map);
        console_printf("battery-monitor: auto_start=%u native=%s\n",(uint64_t)started,desktop_capture?"explicit-capture":desktop_session?"explicit-session":"disabled");
    }
    if(bound)for(;;) {
        if(workspace && !workspace_mount_hold_valid(smbus_workspace_hold))unsafe("control workspace identity");
        if(!lifetime)desktop_tree_scan(&tree);
        controls_dispatch(rsdp,map);
        if(workspace && !desktop_session && !guarded) {
            struct battery_service_status state;battery_service_status(&state);
            if(state.state==BATTERY_SERVICE_ACTIVE && (state.flags&BATTERY_CACHE_FRESH)) {
                mounted_study_guard();guarded=true;
            }
        }
        if(desktop_capture) {
            struct battery_service_status state;battery_service_status(&state);
            if(state.state==BATTERY_SERVICE_UNAVAILABLE || state.state==BATTERY_SERVICE_FAULTED)break;
            if(state.state==BATTERY_SERVICE_ACTIVE && (state.flags&BATTERY_CACHE_FRESH)) {
                if(!first_generation)first_generation=state.sample.generation;
                last_generation=state.sample.generation;
            }
            if(timer_monotonic_us()-capture_start>=(desktop_transitions?480000000ull:240000000ull))break;
        }
        if(process_wait(pid,&status,true)==(int64_t)pid){reaped=true;ok=status==0;break;}
        if(!continuous_home && timer_monotonic_us()>=deadline)break;
        uacpi_kernel_sleep(10);
    }
    if(desktop_capture) {
        uint64_t elapsed=timer_monotonic_us()-capture_start;
        ok=ok && (desktop_transitions || elapsed>=175000000) && first_generation && last_generation>=first_generation+(desktop_transitions?8:30);
        console_printf("%s: first_gen=%u last_gen=%u elapsed_us=%u complete=%s\n",
            desktop_transitions?"battery-home-transitions-sample":"battery-home-desktop-sample",first_generation,last_generation,elapsed,ok?"yes":"NO");
    }
    f=irq_save();uint64_t requests=controls.serial-first_serial;unsigned pending=controls.operation;
    battery_control_revoke(&controls);irq_restore(f);
    deadline=timer_monotonic_us()+5000000;
    uint64_t descendants=tree.count;
    if(lifetime) {
        if(!process_lifetime_revoke(lifetime))unsafe("desktop lifetime revocation");
        struct process_lifetime_status state;
        for(;;) {
            if(!process_lifetime_drain(lifetime) || !process_lifetime_status(lifetime,&state))unsafe("desktop lifetime lookup");
            if(!state.members)break;
            if(timer_monotonic_us()>=deadline)unsafe("desktop lifetime retirement");
            uacpi_kernel_sleep(10);
        }
        descendants=state.peak;
        console_printf("battery-desktop-lifetime: drained token=%u members=0 peak=%u revoked=yes\n",lifetime,descendants);
    } else {
        if(!reaped)(void)process_signal_pid(pid,SRV_SIGNAL_KILL);
        while(!desktop_children_stop(&tree,!ok) || !reaped) {
            if(!reaped)reaped=process_wait(pid,&status,true)==(int64_t)pid;
            if(timer_monotonic_us()>=deadline)unsafe("control clients retirement");
            uacpi_kernel_sleep(10);
        }
        descendants=tree.count;
    }
    if(!battery_service_stop(35000))unsafe("control service retirement");
    if(lifetime) {
        if(!process_lifetime_release(lifetime))unsafe("desktop lifetime release");
        console_printf("battery-desktop-lifetime: released token=%u runtime_retired=yes\n",lifetime);
    }
    struct battery_service_status retired;battery_service_status(&retired);
    ok=ok && !pending && (desktop_home?descendants>=2:descendants==(files?3u:2u)) && (automatic?requests==(files?5u:4u) && retired.epoch==before.epoch+(files?3u:2u) && retired.state==BATTERY_SERVICE_STOPPED:
        retired.state==BATTERY_SERVICE_STOPPED || retired.state==BATTERY_SERVICE_DISABLED || retired.state==BATTERY_SERVICE_UNAVAILABLE || (continuous_home && retired.state==BATTERY_SERVICE_FAULTED));
    console_printf("battery-controls: %s requests=%u descendants=%u epoch=%u; revoked=yes retired=yes\n",ok?"PASS":"FAIL",requests,descendants,retired.epoch);
    return ok;
}
bool battery_controls_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return controls_test(rsdp,map,true,false,0);
}
bool battery_desktop_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return desktop_test(rsdp,map,0,false,false,false,false);
}
bool battery_text_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return desktop_test(rsdp,map,0,true,false,false,false);
}
bool battery_app_files_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return desktop_test(rsdp,map,0,true,false,true,false);
}
bool battery_load_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return desktop_test(rsdp,map,0,true,true,false,false);
}
static struct { bool active,done,ok,before,after;uint64_t start,end; } recovery_io;
static bool recovery_peer_irq(void *arg) {
    (void)arg;recovery_irq.peer++;
    return false;
}
static void recovery_peer_complete(void *arg) {
    (void)arg;
    if(recovery_irq.completed && !recovery_irq.pending)recovery_irq.peer_complete++;
}
static void recovery_io_owner(void *arg) {
    (void)arg;
    uint64_t deadline=timer_monotonic_us()+15000000;
    while(service.cache.value.sample.generation<2 || !(service.cache.value.flags&BATTERY_CACHE_SAMPLING)) {
        if(timer_monotonic_us()>=deadline) { recovery_io.done=true;return; }
        timer_wait_ticks(1);
    }
    uint64_t flags=irq_save();
    if(!scheduler_io_begin()) { recovery_io.done=true;irq_restore(flags);return; }
    recovery_io.active=true;recovery_io.start=timer_monotonic_us();
    recovery_io.before=workspace_sync();
    if(recovery_irq.study) {
        /* The unrelated storage scope remains owned across IRQ recovery. The
         * target study arms delivery on a battery stack; other studies interrupt
         * this storage stack. The second registration proves every handler and
         * completion runs; removal checks registry depth/pending bookkeeping. */
        uint64_t identity=scheduler_current_thread_id(),token=0;
        volatile uint64_t sentinel=0x735256524f534952ull;
        if(!runtime.platform.model_irq_fault)unsafe("IRQ recovery fixture setup");
        if(recovery_irq.target_study) {
            token=irq_handler_register_observed(9,recovery_peer_irq,recovery_peer_complete,NULL);
            if(!token)unsafe("IRQ recovery fixture registration");
        }
        uint64_t until=timer_monotonic_us()+5000000;
        while(!runtime.platform.model_irq_fault()) {
            if(timer_monotonic_us()>=until)unsafe("IRQ recovery fixture sample wait");
            (void)scheduler_io_yield();__asm__ volatile("sti; hlt; cli":::"memory");
        }
        if(recovery_irq.target_study) {
            while(!service.quarantined) {
                if(timer_monotonic_us()>=until)unsafe("IRQ target handoff deadline");
                (void)scheduler_io_yield();__asm__ volatile("sti; hlt; cli":::"memory");
            }
        } else {
            token=irq_handler_register_observed(9,recovery_peer_irq,recovery_peer_complete,NULL);
            if(!token)unsafe("IRQ recovery fixture registration");
            __asm__ volatile("int $41":::"memory");
        }
        bool returned=sentinel==0x735256524f534952ull && scheduler_current_thread_id()==identity &&
            scheduler_io_active() && !scheduler_io_busy() &&
            recovery_irq.peer==1 && recovery_irq.peer_complete==1 && recovery_irq.completed &&
            !recovery_irq.pending && !runtime.sci_counts[1].in_flight && irq_handler_remove(token);
        if(!returned)unsafe("IRQ recovery return proof");
        if(recovery_irq.target_study) {
            if(!recovery_irq.target_handoff || recovery_irq.target_pending || recovery_irq.interrupted==identity)
                unsafe("IRQ target handoff proof");
            console_printf("battery-recovery-resume: storage=%u; scope=retained registry_released=yes\n",identity);
        } else {
            if(recovery_irq.interrupted!=identity)unsafe("IRQ interrupted identity proof");
            report_quarantine();
            if(recovery_irq.callback_study)report_callback_proof();
            console_printf("battery-recovery-irq: interrupted=%u peer=1 completion=1 in_flight=0; returned=yes registry_released=yes claim=unclaimed\n",identity);
        }
    }
    /* Hold one unrelated storage scope across the 25-second sample deadline.
     * This models an admitted operation, not a claim of pending hardware DMA. */
    while(timer_monotonic_us()-recovery_io.start<28000000) {
        (void)scheduler_io_yield();
        __asm__ volatile("sti; hlt; cli":::"memory");
    }
    recovery_io.after=workspace_sync();
    recovery_io.end=timer_monotonic_us();
    recovery_io.ok=recovery_io.before && recovery_io.after && service.quarantined;
    recovery_io.active=false;recovery_io.done=true;
    scheduler_io_end();irq_restore(flags);
}
static void workspace_recovery_test(void *rsdp,struct limine_memmap_response *map,bool io,bool irq,bool callback,bool target) {
    if(!battery_platform_workspace_study_allowed()) {
        console_write("battery-recovery: REFUSED VM-only study\n");return;
    }
    if(!workspace_active() || owner!=OWNER_NONE) {
        console_write("battery-recovery: REFUSED ownership\n");return;
    }
    recovery_irq=(typeof(recovery_irq)){.study=irq,.callback_study=callback,.target_study=target};
    if(io) {
        recovery_io=(typeof(recovery_io)){0};
        if(!scheduler_spawn("battery-recovery-io",recovery_io_owner,NULL)) {
            console_write("battery-recovery: REFUSED I/O fixture spawn\n");return;
        }
    }
    recovery_study=true;
    (void)desktop_test(rsdp,map,0,true,false,true,true);
    recovery_study=false;
    if(!service.quarantined)unsafe("recovery fixture did not quarantine");
    if(io) {
        if(!recovery_io.done || recovery_io.active || !recovery_io.ok)unsafe("recovery I/O owner proof");
        console_printf("battery-recovery-io: before=%u after=%u elapsed_us=%u; owner_completed=yes\n",
            (uint64_t)recovery_io.before,(uint64_t)recovery_io.after,recovery_io.end-recovery_io.start);
    }
    uint64_t completed=runtime.worker.completed,generation=service.cache.value.sample.generation;
    uint64_t heap=runtime.heap,interrupts=runtime.interrupts,queries=runtime.queries;
    scheduler_wake_all(&runtime.worker.ready);scheduler_wake_all(&runtime.worker.progress);
    /* Cross the modeled worker's original 26-second sleep expiry and issue a
     * late dispatcher delivery: neither may reenter the retained interpreter. */
    timer_wait_ticks(300);
    __asm__ volatile("int $41":::"memory");
    bool held=scheduler_service_pair_quarantined(service.identity,runtime.worker.identity) &&
        runtime.worker.completed==completed && service.cache.value.sample.generation==generation &&
        runtime.heap==heap && runtime.interrupts==interrupts && runtime.queries==queries &&
        service.cache.value.state==BATTERY_SERVICE_FAULTED && !battery_service_start(rsdp,map) && !battery_service_stop(0);
    console_printf("battery-recovery: held=%s generation=%u completed=%u io=%u; restart=refused runtime_retired=NO\n",
        held?"yes":"NO",generation,completed,(uint64_t)scheduler_io_active());
    if(!held || scheduler_io_active())unsafe("recovery retention proof");
    bool saved=workspace_log_start() && workspace_log_stop();
    bool unmounted=saved && workspace_unmount();
    console_printf("battery-recovery: saved=%s unmounted=%s; runtime_retired=NO\n",saved?"yes":"NO",unmounted?"yes":"NO");
    if(!unmounted || !battery_reset_allowed(true,workspace_active(),scheduler_io_active()))unsafe("recovery storage drain");
    console_write("battery-recovery: returning after storage drain; test outcome=contained-failure\n");
    arch_reboot_unattended();
}
/* No artificial query or method substitution: only naturally scheduled QR_EC
 * exchanges can exercise this capture. No storage/client work while engaged. */
bool battery_query_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    if(query_study || query_capture || smbus_study || smbus_capture || owner!=OWNER_NONE || !auto_return ||
       workspace_active() || workspace_image_busy() || scheduler_io_active() || !workspace_parked_device()) {
        console_write("battery-native-query: REFUSED parked capture admission\n");return false;
    }
    query_capture=true;query_capture_volume=workspace_parked_device();
    console_write("battery-native-query: waiting up to 30 seconds for a natural policy refusal; no input required\n");
    if(!battery_service_start(rsdp,map)) {
        query_capture=false;query_capture_volume=NULL;return false;
    }
    uint64_t until=timer_monotonic_us()+30000000;
    while(service.running && timer_monotonic_us()<until)uacpi_kernel_sleep(10);
    if(service.running && !battery_service_stop(10000))unsafe("capture stop deadline");
    struct battery_service_status status;battery_service_status(&status);
    bool clean=owner==OWNER_NONE && !runtime.engaged && !runtime.heap && !runtime.errors &&
        !runtime.denied && !service.quarantined && capture_storage_parked();
    bool exercised=clean && runtime.query_receipt.refused && runtime.worker.stopped &&
        runtime.attached==runtime.detached && runtime.attached &&
        status.state==BATTERY_SERVICE_FAULTED && status.error==BATTERY_SERVICE_CONTAINMENT &&
        !(status.flags&BATTERY_CACHE_FRESH);
    console_printf("battery-native-query: outcome=%s code=%u generation=%u state=%u error=%u fresh=%u valid=%u; retired=%s parked=%u\n",
        exercised?"returned-failure":"not-exercised",(uint64_t)runtime.query_receipt.code,
        runtime.query_receipt.generation,(uint64_t)status.state,(uint64_t)status.error,
        (uint64_t)!!(status.flags&BATTERY_CACHE_FRESH),(uint64_t)!!(status.flags&BATTERY_CACHE_VALID),
        clean?"yes":"NO",(uint64_t)capture_storage_parked());
    if(!clean)unsafe("capture retirement proof");
    query_capture=false;query_capture_volume=NULL;
    return exercised;
}
/* Only the boot capture owns this capability. It cannot be enabled from a
 * normal mounted desktop or through the VM study command. */
bool battery_smbus_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    if(query_study || query_capture || smbus_study || smbus_capture || owner!=OWNER_NONE ||
       !auto_return || workspace_active() || workspace_image_busy() || scheduler_io_active() || !workspace_parked_device()) {
        console_write("battery-native-smbus: REFUSED parked capture admission\n");return false;
    }
    smbus_capture=true;smbus_capture_volume=workspace_parked_device();
    console_write("battery-native-smbus: waiting up to 30 seconds for natural host events and fresh samples; no input required\n");
    if(!battery_service_start(rsdp,map)) {smbus_capture=false;smbus_capture_volume=NULL;return false;}
    uint64_t until=timer_monotonic_us()+30000000;struct battery_service_status snapshot={0};
    bool observed=false;
    while(service.running && timer_monotonic_us()<until) {
        battery_service_status(&snapshot);
        if(runtime.events.totals.events && snapshot.sample.generation>=3 && (snapshot.flags&BATTERY_CACHE_FRESH)) {observed=true;break;}
        uacpi_kernel_sleep(10);
    }
    console_printf("battery-native-smbus: snapshot generation=%u events=%u valid=%u fresh=%u\n",
        snapshot.sample.generation,runtime.events.totals.events,(uint64_t)!!(snapshot.flags&BATTERY_CACHE_VALID),
        (uint64_t)!!(snapshot.flags&BATTERY_CACHE_FRESH));
    if(service.running && !battery_service_stop(10000))unsafe("SMBus capture stop deadline");
    struct battery_service_status final;battery_service_status(&final);
    bool clean=owner==OWNER_NONE && !runtime.engaged && !runtime.heap && !runtime.errors && !runtime.denied &&
        !service.quarantined && smbus_capture_parked();
    bool exercised=clean && observed && runtime.worker.stopped && runtime.attached &&
        runtime.attached==runtime.detached && final.state==BATTERY_SERVICE_STOPPED && final.error==BATTERY_SERVICE_OK &&
        runtime.events.totals.events && snapshot.sample.generation>runtime.smbus_invalid_through && !(final.flags&BATTERY_CACHE_FRESH);
    console_printf("battery-native-smbus: outcome=%s events=%u alarms=%u acknowledgements=%u unknown=%u invalid_through=%u generation=%u state=%u error=%u fresh=%u valid=%u; retired=%s parked=%u\n",
        exercised?"completed":"not-exercised",runtime.events.totals.events,runtime.events.totals.alarms,runtime.events.totals.acknowledgements,
        runtime.events.totals.unknown,runtime.smbus_invalid_through,final.sample.generation,(uint64_t)final.state,(uint64_t)final.error,
        (uint64_t)!!(final.flags&BATTERY_CACHE_FRESH),(uint64_t)!!(final.flags&BATTERY_CACHE_VALID),
        clean?"yes":"NO",(uint64_t)smbus_capture_parked());
    if(!clean)unsafe("SMBus capture retirement proof");
    smbus_capture=false;smbus_capture_volume=NULL;return exercised;
}
/* Attended Air6,1 consumer of the same parked service. All EC/AML work stays
 * serialized in that service; this loop only reads the cache and prints prompts. */
bool battery_smbus_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    if(query_study || query_capture || smbus_study || smbus_capture || owner!=OWNER_NONE ||
       !auto_return || workspace_active() || workspace_image_busy() || scheduler_io_active() || !workspace_parked_device()) {
        console_write("battery-smbus-transitions: REFUSED parked capture admission\n");return false;
    }
    smbus_capture=smbus_transitions=true;smbus_capture_volume=workspace_parked_device();
    console_write("battery-smbus-transitions: begin phases=3 stable=3 phase_seconds=120 event_limit=4096; USB=parked\n");
    if(!battery_service_start(rsdp,map)) {
        smbus_capture=smbus_transitions=false;smbus_capture_volume=NULL;return false;
    }
    struct battery_transition trial={.phase_started=timer_monotonic_us()};
    struct {struct battery_service_status status;uint64_t first_generation,first_us;} proof[3]={0};
    static const char *instructions[]={"Keep AC CONNECTED; collecting baseline",
        "UNPLUG AC now; leave USB connected", "RECONNECT AC now; leave USB connected"};
    unsigned shown=3;uint64_t printed=0,last_prompt=0;int result=0;
    while(service.running && !result) {
        uint64_t now=timer_monotonic_us();
        if(shown!=trial.phase || now-last_prompt>=5000000) {
            shown=trial.phase;last_prompt=now;
            console_printf("\nBATTERY TEST %u/3: %s\n",(uint64_t)shown+1,instructions[shown]);
            console_write("Wait for the next prompt. Each phase needs 3 coherent samples within 120 seconds.\n");console_sync();
        }
        struct battery_service_status snapshot;battery_service_status(&snapshot);
        unsigned before=trial.phase;
        result=battery_transition_cached_step(&trial,&snapshot,timer_monotonic_us());
        if(snapshot.sample.generation!=printed) {
            printed=snapshot.sample.generation;
            console_printf("battery-smbus-transitions: phase=%u generation=%u ac=%u current_ua=%d fresh=%u stable=%u\n",
                (uint64_t)before,printed,(uint64_t)!!(snapshot.flags&BATTERY_CACHE_AC_PRESENT),
                snapshot.sample.current_ua,(uint64_t)!!(snapshot.flags&BATTERY_CACHE_FRESH),(uint64_t)trial.stable);
        }
        if(trial.phase!=before) {
            proof[before].status=snapshot;proof[before].first_generation=trial.first_generation;proof[before].first_us=trial.first_us;
        }
        if(!result)uacpi_kernel_sleep(10);
    }
    if(service.running && !battery_service_stop(10000))unsafe("AC capture stop deadline");
    struct battery_service_status final;battery_service_status(&final);
    if(final.state==BATTERY_SERVICE_UNAVAILABLE && owner==OWNER_NONE && !runtime.engaged && !runtime.heap && !service.quarantined) {
        console_write("battery-smbus-transitions: REFUSED unavailable platform; no runtime entered\n");
        smbus_capture=smbus_transitions=false;smbus_capture_volume=NULL;return false;
    }
    bool clean=owner==OWNER_NONE && !runtime.engaged && !runtime.heap && !runtime.errors && !runtime.denied &&
        !service.quarantined && smbus_capture_parked() && runtime.worker.stopped && runtime.attached &&
        runtime.attached==runtime.detached && final.state==BATTERY_SERVICE_STOPPED && final.error==BATTERY_SERVICE_OK && !(final.flags&BATTERY_CACHE_FRESH);
    for(unsigned i=0;i<trial.phase;i++) {
        const struct battery_service_status *p=&proof[i].status;const struct sbs_snapshot *s=&p->sample;
        console_printf("battery-smbus-transitions: accepted=%u stable=3 first_gen=%u last_gen=%u first_us=%u ac_us=%u start_us=%u end_us=%u ac=%u current_ua=%d relative_soc=%u voltage_uv=%u capacity=%u full=%u design=%u unit=%u valid=%x\n",
            (uint64_t)i,proof[i].first_generation,s->generation,proof[i].first_us,p->ac_sampled_us,s->started_us,s->completed_us,
            (uint64_t)!!(p->flags&BATTERY_CACHE_AC_PRESENT),s->current_ua,(uint64_t)s->relative_soc,s->voltage_uv,
            s->remaining_capacity,s->full_capacity,s->design_capacity,(uint64_t)s->capacity_unit,(uint64_t)s->valid);
    }
    bool passed=result==1 && clean && runtime.events.totals.events && final.sample.generation>runtime.smbus_invalid_through;
    console_printf("battery-smbus-transitions: outcome=%s phases=%u events=%u alarms=%u acknowledgements=%u unknown=%u generation=%u invalid_through=%u retired=%s parked=%u\n",
        passed?"completed":"incomplete",(uint64_t)trial.phase,runtime.events.totals.events,runtime.events.totals.alarms,
        runtime.events.totals.acknowledgements,runtime.events.totals.unknown,final.sample.generation,runtime.smbus_invalid_through,
        clean?"yes":"NO",(uint64_t)smbus_capture_parked());
    if(!clean)unsafe("AC capture retirement proof");
    smbus_capture=smbus_transitions=false;smbus_capture_volume=NULL;return passed;
}
/* Explicit parked image capture only. The image view must stay bound to the
 * exact parked volume while runtime IO is admitted; desktop_test retires every
 * client, stops the service, then revokes the image before ordinary remount. */
bool battery_smbus_files_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    if(query_study || query_capture || smbus_study || smbus_capture || owner!=OWNER_NONE ||
       !auto_return || workspace_active() || !workspace_parked_device() ||
       !workspace_image_ready() || scheduler_io_active()) {
        console_write("battery-native-smbus: REFUSED parked image admission\n");return false;
    }
    smbus_capture=true;smbus_files=true;smbus_capture_volume=workspace_parked_device();
    smbus_files_snapshot=(struct battery_service_status){0};
    console_write("battery-smbus-files: begin limit=512 storage=image; aml=0 serialized=1\n");
    bool complete=desktop_test(rsdp,map,0,true,false,true,false);
    /* The image is revoked by desktop_test after runtime retirement. */
    smbus_files=false;
    struct battery_service_status final;battery_service_status(&final);
    bool clean=owner==OWNER_NONE && !service.running && !runtime.engaged && !runtime.heap &&
        !runtime.errors && !runtime.denied && !service.quarantined && smbus_capture_parked();
    bool exercised=complete && clean && final.state==BATTERY_SERVICE_STOPPED && final.error==BATTERY_SERVICE_OK &&
        (smbus_files_snapshot.flags&BATTERY_CACHE_FRESH) && runtime.events.totals.events &&
        smbus_files_snapshot.sample.generation>runtime.smbus_invalid_through &&
        !(final.flags&BATTERY_CACHE_FRESH);
    console_printf("battery-native-smbus: outcome=%s events=%u alarms=%u acknowledgements=%u unknown=%u invalid_through=%u generation=%u state=%u error=%u fresh=%u valid=%u; retired=%s parked=%u\n",
        exercised?"completed":"not-exercised",runtime.events.totals.events,runtime.events.totals.alarms,runtime.events.totals.acknowledgements,
        runtime.events.totals.unknown,runtime.smbus_invalid_through,final.sample.generation,(uint64_t)final.state,(uint64_t)final.error,
        (uint64_t)!!(final.flags&BATTERY_CACHE_FRESH),(uint64_t)!!(final.flags&BATTERY_CACHE_VALID),
        clean?"yes":"NO",(uint64_t)smbus_capture_parked());
    if(!clean)unsafe("SMBus image retirement");
    smbus_capture=false;smbus_capture_volume=NULL;return exercised;
}
/* Explicit full-runtime command, never ordinary startup. The same path is
 * rehearsed with a disposable VM backend; native admission remains the exact
 * captured Air profile. No clients or filesystem calls while runtime retires. */
bool battery_home_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    if(mounted_home || query_study || query_capture || smbus_study || smbus_capture ||
       owner!=OWNER_NONE || controls.group || service.running || service.quarantined ||
       !workspace_active() || workspace_image_busy() || scheduler_io_active()) {
        console_write("battery-home: REFUSED mounted ownership\n");return false;
    }
    if(!mounted_study_acquire()){console_write("battery-home: REFUSED mount hold\n");return false;}
    mounted_home=bounded_home=true;
    console_write("battery-home: begin limit=128 mounted=yes clients=0; startup=disabled\n");
    bool started=battery_service_start(rsdp,map),observed=false,guarded=false;
    uint64_t until=timer_monotonic_us()+30000000;
    struct battery_service_status snapshot={0};
    while(started && service.running && timer_monotonic_us()<until) {
        if(!mounted_home_owned())unsafe("Home mount identity");
        battery_service_status(&snapshot);
        if(snapshot.state==BATTERY_SERVICE_ACTIVE && (snapshot.flags&BATTERY_CACHE_FRESH)) {
            if(!guarded){mounted_study_guard();guarded=true;}
            if(snapshot.sample.generation>=3 && runtime.events.totals.events){observed=true;break;}
        }
        uacpi_kernel_sleep(10);
    }
    console_printf("battery-home: snapshot generation=%u events=%u valid=%u fresh=%u\n",
        snapshot.sample.generation,runtime.events.totals.events,(uint64_t)!!(snapshot.flags&BATTERY_CACHE_VALID),
        (uint64_t)!!(snapshot.flags&BATTERY_CACHE_FRESH));
    if(!battery_service_stop(10000))unsafe("Home stop deadline");
    struct battery_service_status final;battery_service_status(&final);
    bool retired=owner==OWNER_NONE && !service.running && !service.quarantined &&
        !runtime.engaged && !runtime.heap && !runtime.errors && !runtime.denied &&
        runtime.attached==runtime.detached && (!runtime.attached || runtime.worker.stopped) &&
        !(final.flags&(BATTERY_CACHE_FRESH|BATTERY_CACHE_SAMPLING)) && mounted_home_owned();
    bool complete=started && observed && retired && runtime.attached &&
        final.state==BATTERY_SERVICE_STOPPED && final.error==BATTERY_SERVICE_OK &&
        snapshot.sample.generation>runtime.smbus_invalid_through;
    const char *outcome=complete?"complete":runtime.query_receipt.refused?"query-refused":
        final.state==BATTERY_SERVICE_UNAVAILABLE?"unavailable":"incomplete";
    console_printf("battery-home: outcome=%s epoch=%u generation=%u events=%u alarms=%u acknowledgements=%u state=%u error=%u fresh=%u; retired=%s mounted=yes\n",
        outcome,final.epoch,final.sample.generation,runtime.events.totals.events,runtime.events.totals.alarms,runtime.events.totals.acknowledgements,
        (uint64_t)final.state,(uint64_t)final.error,(uint64_t)!!(final.flags&BATTERY_CACHE_FRESH),retired?"yes":"NO");
    if(!retired)unsafe("Home retirement proof");
    mounted_study_release();mounted_home=bounded_home=false;
    return complete;
}
/* Disposable model study: the command thread owns an unrelated storage scope
 * while both battery threads return and retire. No desktop/client teardown is
 * involved, and this does not claim physical keyboard or pending-DMA acceptance. */
static void workspace_query_test(void *rsdp,struct limine_memmap_response *map) {
    if(!battery_platform_workspace_study_allowed()) {
        console_write("battery-query: REFUSED VM-only study\n");return;
    }
    if(!workspace_active() || workspace_image_busy() || owner!=OWNER_NONE) {
        console_write("battery-query: REFUSED ownership\n");return;
    }
    query_study=true;
    if(!battery_service_start(rsdp,map))unsafe("query study start");
    uint64_t until=timer_monotonic_us()+15000000;
    struct battery_service_status status;
    do {
        battery_service_status(&status);
        if(status.state==BATTERY_SERVICE_ACTIVE && (status.flags&BATTERY_CACHE_FRESH))break;
        if(!service.running || timer_monotonic_us()>=until)unsafe("query study startup");
        uacpi_kernel_sleep(10);
    } while(true);
    uint64_t f=irq_save();bool entered=scheduler_io_begin();irq_restore(f);
    if(!entered)unsafe("query study IO admission");
    bool before=workspace_sync();
    while(service.running) {
        if(timer_monotonic_us()>=until)unsafe("query study retirement deadline");
        uacpi_kernel_sleep(10);
    }
    bool after=workspace_sync();
    f=irq_save();bool owns=scheduler_io_active()&&!scheduler_io_busy();scheduler_io_end();irq_restore(f);
    battery_service_status(&status);
    bool retired=owner==OWNER_NONE && runtime.worker.stopped && !runtime.engaged && !runtime.heap &&
        !runtime.errors && !runtime.denied && !service.quarantined && runtime.query_receipt.refused &&
        status.state==BATTERY_SERVICE_FAULTED && status.error==BATTERY_SERVICE_CONTAINMENT &&
        !(status.flags&BATTERY_CACHE_FRESH) && (status.flags&BATTERY_CACHE_VALID) &&
        runtime.attached==runtime.detached;
    console_printf("battery-query: state=%u error=%u generation=%u fresh=%u before=%u after=%u io_owned=%u; retired=%s outcome=query-refused\n",
        (uint64_t)status.state,(uint64_t)status.error,status.sample.generation,(uint64_t)!!(status.flags&BATTERY_CACHE_FRESH),
        (uint64_t)before,(uint64_t)after,(uint64_t)owns,retired?"yes":"NO");
    if(!retired || !before || !after || !owns)unsafe("query study proof");
    query_study=false;
    bool saved=workspace_log_start() && workspace_log_stop();
    bool unmounted=saved && workspace_unmount();
    console_printf("battery-query: saved=%s unmounted=%s; runtime_retired=yes\n",saved?"yes":"NO",unmounted?"yes":"NO");
    if(!unmounted || !battery_reset_allowed(true,workspace_active(),scheduler_io_active()))unsafe("query storage drain");
    console_write("battery-query: returning after storage drain; test outcome=returned-failure\n");
    arch_reboot_unattended();
}
static bool command_is(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void smbus_test(void *rsdp,struct limine_memmap_response *map) {
    if(!battery_platform_workspace_study_allowed() || smbus_study || owner!=OWNER_NONE ||
       workspace_active() || workspace_image_busy() || scheduler_io_active()) {
        console_write("battery-smbus-study: REFUSED VM-only idle study\n");return;
    }
    smbus_study=true;
    if(!battery_service_start(rsdp,map))unsafe("SMBus study start");
    uint64_t until=timer_monotonic_us()+20000000;struct battery_service_status status;
    do {
        battery_service_status(&status);
        if(status.sample.generation>=3 && runtime.events.totals.events>=3 && (status.flags&BATTERY_CACHE_FRESH))break;
        if(!service.running || timer_monotonic_us()>=until)unsafe("SMBus study deadline");
        uacpi_kernel_sleep(10);
    } while(true);
    if(!battery_service_stop(10000))unsafe("SMBus study stop");
    if(owner!=OWNER_NONE || runtime.engaged || runtime.heap || runtime.errors || runtime.denied ||
       runtime.attached!=runtime.detached || !runtime.attached)unsafe("SMBus study retirement");
    console_printf("battery-smbus-study: PASS events=%u alarms=%u acknowledgements=%u unknown=%u generation=%u; fresh_before_stop=1 retired=yes native=disabled\n",
        runtime.events.totals.events,runtime.events.totals.alarms,runtime.events.totals.acknowledgements,runtime.events.totals.unknown,status.sample.generation);
    smbus_study=false;
}
/* VM-only mounted workspace study. Native refuses before platform activity;
 * the same serialized event owner spans real Text/file clients and their stop. */
static void smbus_workspace_test(void *rsdp,struct limine_memmap_response *map) {
    if(!battery_platform_workspace_study_allowed() || smbus_study || smbus_capture ||
       query_study || query_capture || owner!=OWNER_NONE || !workspace_active() || workspace_image_busy()) {
        console_write("battery-smbus-workspace: REFUSED VM-only mounted study\n");return;
    }
    if(!mounted_study_acquire()){console_write("battery-smbus-workspace: REFUSED mount hold\n");return;}
    smbus_study=true;
    bool complete=desktop_test(rsdp,map,0,true,false,true,true);
    struct battery_service_status final;battery_service_status(&final);
    bool retired=owner==OWNER_NONE && !service.running && !service.quarantined &&
        !runtime.engaged && !runtime.heap && !runtime.errors && !runtime.denied &&
        runtime.attached && runtime.attached==runtime.detached &&
        final.state==BATTERY_SERVICE_STOPPED && final.error==BATTERY_SERVICE_OK &&
        !(final.flags&(BATTERY_CACHE_FRESH|BATTERY_CACHE_SAMPLING)) && workspace_mount_hold_valid(smbus_workspace_hold);
    console_printf("battery-smbus-workspace: outcome=%s events=%u alarms=%u acknowledgements=%u unknown=%u invalid_through=%u generation=%u state=%u error=%u fresh=%u; retired=%s native=disabled\n",
        complete?"complete":"interrupted",runtime.events.totals.events,runtime.events.totals.alarms,runtime.events.totals.acknowledgements,
        runtime.events.totals.unknown,runtime.smbus_invalid_through,final.sample.generation,
        (uint64_t)final.state,(uint64_t)final.error,(uint64_t)!!(final.flags&BATTERY_CACHE_FRESH),retired?"yes":"NO");
    if(!retired)unsafe("SMBus workspace retirement");
    mounted_study_release();
    smbus_study=false;
}
/* The explicit Home command uses the same session owner as the VM study.
 * Its capability is held through client retirement and every Start/Stop epoch;
 * runtime_open still checks the exact native controller/firmware profile.
 * Only the explicit Home entry opts into ordinary startup; no fatal reset
 * privilege is granted while storage remains mounted. */
static bool workspace_controls_test(void *rsdp,struct limine_memmap_response *map,bool automatic,unsigned files,bool home,bool continuous) {
    if((!home && !battery_platform_workspace_study_allowed()) || !workspace_active() || workspace_image_busy() ||
       scheduler_io_active() || mounted_home || continuous_home || smbus_study || smbus_capture || query_study || query_capture ||
       (continuous && (!home || (!desktop_capture && !desktop_session && !battery_platform_workspace_study_allowed()))) ||
       owner!=OWNER_NONE || controls.group || service.running || service.quarantined) {
        console_write(home?"battery-workspace-controls: REFUSED explicit Home ownership\n":"battery-workspace-controls: REFUSED VM-only mounted study\n");return false;
    }
    if(!mounted_study_acquire()){console_write("battery-workspace-controls: REFUSED mount hold\n");return false;}
    smbus_study=!home;mounted_home=home;smbus_controls=true;home_session_events=0;continuous_home=continuous;
    console_printf("battery-workspace-controls: begin mounted=yes native=%s\n",home?"explicit":"disabled");
    if(desktop_capture)console_write("battery-home-monitor: begin continuous=yes log_interval_us=5000000 native=explicit-capture\n");
    else if(desktop_session)console_write("battery-home-monitor: begin continuous=yes log_interval_us=5000000 native=explicit-session\n");
    else if(home)console_write(continuous_home?"battery-home-monitor: begin continuous=yes log_interval_us=5000000 native=disabled\n":"battery-home-session: limit=512 per_session=yes startup=disabled\n");
    if(files)console_printf("battery-control-files: begin pending-application-jobs=yes native=%s\n",home?"explicit":"disabled");
    bool logging=!desktop_home || (desktop_capture?workspace_log_active():workspace_log_start());
    if(desktop_home)console_printf("battery-home-desktop: logging=%s apps=ordinary native=%s\n",logging?"started":"FAILED",desktop_capture?"explicit-capture":desktop_session?"explicit-session":"disabled");
    bool complete=logging && controls_test(rsdp,map,automatic,true,files);
    if(desktop_home) {
        if(!workspace_log_quiesce(5000))unsafe("desktop logger retirement");
        bool saved=logging && (desktop_capture || workspace_log_stop());
        if(desktop_capture)console_write("battery-home-desktop: logger_retired=yes final_deferred=yes\n");
        else console_printf("battery-home-desktop: logger_retired=yes final_saved=%s\n",saved?"yes":"NO");
        complete=complete && saved;
    }
    struct battery_service_status final;battery_service_status(&final);
    bool retired=owner==OWNER_NONE && !controls.group && !service.running && !service.quarantined &&
        !runtime.engaged && !runtime.heap && !runtime.errors && !runtime.denied &&
        runtime.attached==runtime.detached && (!runtime.attached || runtime.worker.stopped) &&
        !(final.flags&(BATTERY_CACHE_FRESH|BATTERY_CACHE_SAMPLING)) && workspace_mount_hold_valid(smbus_workspace_hold) && !workspace_image_busy();
    console_printf("battery-workspace-controls: complete=%s epoch=%u state=%u; revoked=%s retired=%s mounted=%s native=%s\n",
        complete?"yes":"NO",final.epoch,(uint64_t)final.state,controls.group?"NO":"yes",retired?"yes":"NO",workspace_active()?"yes":"NO",home?"explicit":"disabled");
    if(!retired)unsafe("workspace controls retirement");
    mounted_study_release();
    smbus_controls=false;smbus_study=false;mounted_home=false;continuous_home=false;
    return complete;
}
static bool home_desktop_capture_run(void *rsdp,struct limine_memmap_response *map,bool transitions) {
    if(desktop_home || desktop_capture || continuous_home || !workspace_log_active())return false;
    /* Only the explicit boot capture calls this. runtime_open still enforces
     * the captured native EC/SMBus/firmware profile. Fatal mounted ownership
     * cannot use automatic reset; the outer capture owns the final log. */
    if(!battery_diagnostic_auto_return(false))return false;
    desktop_home=desktop_capture=true;desktop_transitions=transitions;
    if(transitions)console_write("battery-home-transitions-capture: begin phases=3 watchdog_seconds=480 startup=disabled input=AC-unplug-reconnect\n");
    else console_write("battery-home-desktop-capture: begin seconds=180 watchdog_seconds=240 startup=disabled input=none\n");
    bool ok=workspace_controls_test(rsdp,map,false,0,true,true);
    desktop_home=desktop_capture=desktop_transitions=false;
    return ok;
}
/* Explicit ordinary session, never selected by default boot or cache reads.
 * The same inherited lifetime and exact mount hold span every service epoch. */
bool battery_home_desktop_session_run(void *rsdp,struct limine_memmap_response *map) {
    if(desktop_home || desktop_capture || desktop_session || continuous_home || workspace_log_active())return false;
    if(!battery_diagnostic_auto_return(false))return false;
    desktop_home=desktop_session=true;
    bool ok=workspace_controls_test(rsdp,map,false,0,true,true);
    desktop_home=desktop_session=false;
    return ok;
}
bool battery_home_desktop_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return home_desktop_capture_run(rsdp,map,false);
}
bool battery_home_transitions_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return home_desktop_capture_run(rsdp,map,true);
}
bool battery_home_session_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    /* Only this bounded diagnostic admits Air6,1 across repeated lifetimes.
     * Ordinary/continuous entry cannot acquire this capability. */
    if(bounded_home_session || bounded_home || auto_return)return false;
    bounded_home_session=true;
    bool ok=workspace_controls_test(rsdp,map,true,1,true,false);
    bounded_home_session=false;
    return ok;
}
void acpi_runtime_test_command(const char *args) {
    if(command_is(args,"battery-home-desktop")) {
        if(!battery_platform_workspace_study_allowed() || desktop_home || continuous_home || workspace_log_active()) {
            console_write("battery-home-desktop: REFUSED VM-only idle logger session\n");return;
        }
        desktop_home=true;
        (void)workspace_controls_test(rsdp_request.response?rsdp_request.response->address:NULL,
            memmap_request.response,false,0,true,true);
        desktop_home=false;
        return;
    }
    if(command_is(args,"battery-home-monitor")) {
        if(!battery_platform_workspace_study_allowed() || continuous_home) {
            console_write("battery-home-monitor: REFUSED VM-only session\n");return;
        }
        (void)workspace_controls_test(rsdp_request.response?rsdp_request.response->address:NULL,
            memmap_request.response,false,0,true,true);
        return;
    }

    if(command_is(args,"battery-home-controls") || command_is(args,"battery-home-controls-manual") ||
       command_is(args,"battery-home-controls-files") || command_is(args,"battery-home-controls-files-cancel")) {
        unsigned files=command_is(args,"battery-home-controls-files-cancel")?2:command_is(args,"battery-home-controls-files")?1:0;
        workspace_controls_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            files || command_is(args,"battery-home-controls"),files,true,false);return;
    }
    if(command_is(args,"battery-home")) {
        (void)battery_home_diagnostic_run(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);return;
    }
    if(command_is(args,"battery-workspace-controls") || command_is(args,"battery-workspace-controls-manual") ||
       command_is(args,"battery-workspace-controls-files") || command_is(args,"battery-workspace-controls-files-cancel")) {
        unsigned files=command_is(args,"battery-workspace-controls-files-cancel")?2:command_is(args,"battery-workspace-controls-files")?1:0;
        workspace_controls_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            files || command_is(args,"battery-workspace-controls"),files,false,false);return;
    }
    if(command_is(args,"battery-smbus-workspace")) {
        smbus_workspace_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);return;
    }
    if(command_is(args,"battery-smbus-study")) {
        smbus_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);return;
    }
    if(command_is(args,"battery-workspace-query")) {
        workspace_query_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);return;
    }
    if(command_is(args,"battery-workspace-recovery") || command_is(args,"battery-workspace-recovery-io") || command_is(args,"battery-workspace-recovery-irq") || command_is(args,"battery-workspace-recovery-callback") || command_is(args,"battery-workspace-recovery-target")) {
        bool target=command_is(args,"battery-workspace-recovery-target");
        bool callback=target || command_is(args,"battery-workspace-recovery-callback");
        bool irq=callback || command_is(args,"battery-workspace-recovery-irq");
        workspace_recovery_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            irq || command_is(args,"battery-workspace-recovery-io"),irq,callback,target);return;
    }
    /* No native boot option or exported diagnostic entry point. The backend
     * gate precedes all service/platform activity, including fault arming. */
    if(command_is(args,"battery-workspace") || command_is(args,"battery-workspace-fault") || command_is(args,"battery-workspace-timeout")) {
        if(!battery_platform_workspace_study_allowed()) {
            console_write("battery-workspace: REFUSED VM-only study\n");return;
        }
        if(command_is(args,"battery-workspace-fault")) {
            /* Model only: reproduce stale parked-time arming after mount. */
            auto_return=true;
        }
        (void)desktop_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            command_is(args,"battery-workspace-timeout")?2:0,true,false,true,true);
        auto_return=false;return;
    }
    if(command_is(args,"battery-controls") || command_is(args,"battery-controls-manual")) {
        (void)controls_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,command_is(args,"battery-controls"),false,0);return;
    }
    if(command_is(args,"battery-app-files") || command_is(args,"battery-app-files-timeout") || command_is(args,"battery-app-files-missing")) {
        (void)desktop_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            command_is(args,"battery-app-files-timeout")?2:command_is(args,"battery-app-files-missing")?1:0,true,false,true,false);return;
    }
    bool desktop_load=command_is(args,"battery-text-load") || command_is(args,"battery-text-load-missing") ||
        command_is(args,"battery-text-load-timeout") || command_is(args,"battery-text-load-abort");
    if(desktop_load) {
        (void)desktop_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            command_is(args,"battery-text-load-missing")?1:command_is(args,"battery-text-load-timeout")?2:
            command_is(args,"battery-text-load-abort")?3:0,true,true,false,false);return;
    }
    bool desktop_text=command_is(args,"battery-desktop-text") || command_is(args,"battery-desktop-text-missing") || command_is(args,"battery-desktop-text-timeout");
    if(desktop_text || command_is(args,"battery-desktop") || command_is(args,"battery-desktop-missing") || command_is(args,"battery-desktop-timeout")) {
        (void)desktop_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,
            (command_is(args,"battery-desktop-missing") || command_is(args,"battery-desktop-text-missing"))?1:
            (command_is(args,"battery-desktop-timeout") || command_is(args,"battery-desktop-text-timeout"))?2:0,desktop_text,false,false,false);return;
    }
    if(command_is(args,"battery-filesystem-prepare")) {
        if(!workspace_image_open())console_write("workspace-image: preparation FAILED\n");
        return;
    }
    if(command_is(args,"battery-usb-write-prepare")) {
        if(!workspace_scratch_prepare())console_write("workspace-scratch: preparation FAILED\n");
        return;
    }
    bool filesystem=command_is(args,"battery-filesystem");
    bool usb_writes=command_is(args,"battery-usb-write");
    bool usb_reads=command_is(args,"battery-usb-read");
    bool service_command=filesystem||usb_writes||usb_reads||command_is(args,"battery-service"),stale=command_is(args,"battery-service-stale"),
        refusal=command_is(args,"battery-service-refusal");
    if(service_command||stale||refusal) {
        (void)service_test(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,stale,refusal,filesystem?3:usb_writes?2:usb_reads?1:0);return;
    }
    bool transitions=command_is(args,"battery-transitions");
    if(!transitions&&!command_is(args,"battery")){console_write("battery diagnostic: acpiruntime battery | battery-transitions | battery-service\n");return;}
    (void)run_battery(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response,transitions);
}
