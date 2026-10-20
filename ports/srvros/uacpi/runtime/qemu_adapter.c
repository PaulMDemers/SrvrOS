/* Full event-enabled uACPI adapter for a separate Q35 VM test kernel only.
 * Not included in the normal kernel, stages or installed hardware profiles. */
#include "work_worker.h"
#include "q35_fixture.h"
#include "ec_fixture.h"
#include "sci_owner.h"
#include "event_io.h"
#include "qemu_adapter.h"
#include <srvros/acpi.h>
#include <srvros/acpi_runtime_test.h>
#include <srvros/aml_probe.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/heap.h>
#include <srvros/irq_handlers.h>
#include <srvros/pmm.h>
#include <srvros/semaphore.h>
#include <srvros/timer.h>
#include <uacpi/uacpi.h>
#include <uacpi/context.h>
#include <uacpi/tables.h>
#include <uacpi/event.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
#include "../../../../kernel/src/arch/x86_64/ioapic.h"

extern volatile struct limine_rsdp_request rsdp_request;
#define HEAP_LIMIT (8u * 1024u * 1024u)
static struct {
    struct acpi_work_worker worker;
    struct acpi_work_item event_item, wedge_item, load_item, global_item;
    struct kernel_semaphore wedge_gate;
    uacpi_interrupt_handler irq_fn;
    uacpi_handle irq_context;
    uint64_t rsdp, facs, heap, peak, errors, denied, fixture_dsdt;
    struct sci_owner sci;
    struct acpi_event_io event_io;
    uint64_t interrupts, power_events, work_events, last_value, contended_waits;
    uint16_t original_control;
    uint8_t disable_command;
    bool engaged, initialized, stopping, work_scheduled, fault_install;
    bool loading, load_done, load_deny;
    bool fault_init_io;
    unsigned global_mode;
    bool global_active;
    uacpi_status load_result;
} runtime;

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}
static void irq_restore(uint64_t flags) {
    if (flags & (1ull << 9)) __asm__ volatile("sti" ::: "memory");
}
static __attribute__((noreturn)) void unsafe(const char *reason) {
    console_printf("acpi-runtime: HALTED unsafe teardown: %s heap=%u\n", reason, runtime.heap);
    console_sync();
    __asm__ volatile("cli" ::: "memory");
    halt_forever();
}
void acpi_runtime_fixture_halt(const char *reason) { unsafe(reason); }
static bool equal(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static bool contains(uint64_t base, uint64_t size, uint64_t address, uint64_t length) {
    return length && address >= base && address - base <= size && length <= size - (address - base);
}
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out) {
    *out = runtime.rsdp; return UACPI_STATUS_OK;
}
void *uacpi_kernel_map(uacpi_phys_addr address, uacpi_size length) {
    if (!runtime.engaged) return UACPI_MAP_FAILED;
    if (contains(runtime.rsdp, 36, address, length) ||
        (runtime.facs && contains(runtime.facs, 64, address, length)))
        return pmm_phys_to_virt(address);
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *table = acpi_table_at(i);
        if (contains(table->physical_address, table->length, address, length))
            return pmm_phys_to_virt(address);
    }
    runtime.denied++;
    console_printf("acpi-runtime: denied map address=%x length=%u\n", address, (uint64_t)length);
    return UACPI_MAP_FAILED;
}
void uacpi_kernel_unmap(void *pointer, uacpi_size length) { (void)pointer; (void)length; }
void uacpi_kernel_log(uacpi_log_level level, const char *text) {
    if (level == UACPI_LOG_ERROR) runtime.errors++;
    if (level <= UACPI_LOG_WARN) { console_write("acpi-runtime: "); console_write(text); }
}
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
    uint64_t flags = irq_save();
    if (timeout && !((struct kernel_semaphore *)handle)->units) runtime.contended_waits++;
    irq_restore(flags);
    if (runtime.global_active && runtime.global_mode == 2 && timeout == 0xffff &&
        scheduler_current_thread_id() == runtime.worker.identity &&
        !((struct kernel_semaphore *)handle)->units)
        console_write("acpi-global-lock: firmware-wait\n");
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
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) { (void)handle; return irq_save(); }
void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) { (void)handle; irq_restore(flags); }
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
static bool irq_bridge(void *context) {
    (void)context; runtime.interrupts++;
    return runtime.irq_fn && runtime.irq_fn(runtime.irq_context) == UACPI_INTERRUPT_HANDLED;
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
    /* Inject after route preparation, so VM failure exercises real rollback. */
    if (runtime.fault_install) return 0;
    return irq_handler_register(line, fn, context);
}
static bool sci_remove(void *ctx, uint64_t token) { (void)ctx; return irq_handler_remove(token); }
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq,
    uacpi_interrupt_handler handler, uacpi_handle context, uacpi_handle *out) {
    if (irq != 9 || runtime.sci.phase != SCI_OWNER_OFF || !handler)
        return UACPI_STATUS_DENIED;
    struct sci_route route;
    if (!sci_route_resolve(&route, irq, acpi_irq_override(9)) || route.gsi != 9)
        return UACPI_STATUS_DENIED; /* Preserve this adapter's strict Q35 gate. */
    struct sci_owner_ops ops = {.read=sci_read, .prepare=sci_prepare, .mask=sci_mask,
        .restore=sci_restore, .register_irq=sci_register, .remove_irq=sci_remove};
    runtime.irq_fn = handler; runtime.irq_context = context;
    bool ok = sci_owner_prepare(&runtime.sci, ops, route, irq_bridge, NULL);
    if (ok) ok = sci_owner_enable(&runtime.sci);
    if (!ok) {
        if (!sci_owner_release(&runtime.sci)) unsafe("SCI install rollback failed");
        runtime.irq_fn = NULL; runtime.irq_context = NULL;
        return UACPI_STATUS_DENIED;
    }
    *out = &runtime; return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle handle) {
    if (handle != &runtime || handler != runtime.irq_fn || !runtime.sci.token)
        return UACPI_STATUS_INVALID_ARGUMENT;
    if (!sci_owner_release(&runtime.sci)) unsafe("IRQ removal failed");
    runtime.irq_fn = NULL; runtime.irq_context = NULL;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *request) {
    (void)request; runtime.denied++; return UACPI_STATUS_DENIED;
}
struct io_region { uint16_t base; uint32_t size; };
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size size, uacpi_handle *out) {
    if (!runtime.engaged || !acpi_event_io_map(&runtime.event_io, base, size)) { runtime.denied++; return UACPI_STATUS_DENIED; }
    struct io_region *region = uacpi_kernel_alloc(sizeof(*region));
    if (!region) return UACPI_STATUS_OUT_OF_MEMORY;
    *region = (struct io_region){base, size}; *out = region; return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle handle) { uacpi_kernel_free(handle); }
#define IO_ACCESS(bits, input, output) \
uacpi_status uacpi_kernel_io_read##bits(uacpi_handle handle, uacpi_size offset, uacpi_u##bits *value) { \
    struct io_region *r = handle; if (!runtime.engaged || !r || !contains(0,r->size,offset,sizeof(*value)) || \
        !acpi_event_io_read(&runtime.event_io,r->base+offset,bits)) { runtime.denied++; return UACPI_STATUS_DENIED; } \
    *value = input(r->base+offset); return UACPI_STATUS_OK; } \
uacpi_status uacpi_kernel_io_write##bits(uacpi_handle handle, uacpi_size offset, uacpi_u##bits value) { \
    struct io_region *r = handle; if (!runtime.engaged || !r || !contains(0,r->size,offset,sizeof(value)) || \
        !acpi_event_io_write(&runtime.event_io,r->base+offset,bits,value)) { runtime.denied++; return UACPI_STATUS_DENIED; } \
    if (runtime.fault_init_io && r->base+offset==runtime.event_io.resources.pm1_enable) { \
        runtime.fault_init_io=false; runtime.denied++; return UACPI_STATUS_DENIED; } \
    output(r->base+offset,value); return UACPI_STATUS_OK; }
IO_ACCESS(8,inb,outb) IO_ACCESS(16,inw,outw) IO_ACCESS(32,inl,outl)
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out) {
    (void)address; (void)out; runtime.denied++; return UACPI_STATUS_DENIED;
}
void uacpi_kernel_pci_device_close(uacpi_handle handle) { (void)handle; }
#define DENY_PCI(bits) \
uacpi_status uacpi_kernel_pci_read##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits *v) { (void)h;(void)o;(void)v;runtime.denied++;return UACPI_STATUS_DENIED; } \
uacpi_status uacpi_kernel_pci_write##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits v) { (void)h;(void)o;(void)v;runtime.denied++;return UACPI_STATUS_DENIED; }
DENY_PCI(8) DENY_PCI(16) DENY_PCI(32)
#define DENY_MMIO(bits) \
uacpi_u##bits uacpi_kernel_mmio_read##bits(void *p) { (void)p;runtime.denied++;return 0; } \
void uacpi_kernel_mmio_write##bits(void *p,uacpi_u##bits v) { (void)p;(void)v;runtime.denied++; }
DENY_MMIO(8) DENY_MMIO(16) DENY_MMIO(64)
uacpi_u32 uacpi_kernel_mmio_read32(void *p) {
    /* Only the explicit Q35 FACS fixture may read its validated lock word.
     * Pinned uACPI updates that word through atomic cmpxchg, not MMIO writes. */
    if(runtime.engaged && runtime.global_active && runtime.facs &&
       p==(void *)((uintptr_t)pmm_phys_to_virt(runtime.facs)+__builtin_offsetof(struct acpi_facs,global_lock)))
        return *(volatile uint32_t *)p;
    runtime.denied++;return 0;
}
void uacpi_kernel_mmio_write32(void *p,uacpi_u32 value) {
    (void)p;(void)value;runtime.denied++;
}

static void power_work(void *context) {
    (void)context;
    for (unsigned i = 0; i < 8; i++) {
        uint64_t value = 0;
        uacpi_status result = uacpi_eval_simple_integer(NULL, "\\TST0", &value);
        uint64_t flags = irq_save();
        if (result) runtime.errors++;
        runtime.last_value = value;
        runtime.work_events++;
        if (runtime.work_events >= runtime.power_events) {
            runtime.work_scheduled = false;
            irq_restore(flags); return;
        }
        irq_restore(flags);
    }
    uacpi_kernel_schedule_work(UACPI_WORK_NOTIFICATION, &runtime.event_item, power_work, NULL);
}
static uacpi_interrupt_ret power_handler(uacpi_handle context) {
    (void)context;
    runtime.power_events++;
    (void)acpi_ec_fixture_signal(0x20);
    if (!runtime.work_scheduled) {
        runtime.work_scheduled = true;
        uacpi_kernel_schedule_work(UACPI_WORK_NOTIFICATION, &runtime.event_item, power_work, NULL);
    }
    return UACPI_INTERRUPT_HANDLED;
}
static bool stop_runtime(void) {
    if (!runtime.engaged) return true;
    runtime.stopping = true;
    outw(0x602, 0);
    for (uint16_t port = 0x628; port < 0x630; port++) outb(port, 0);
    if (runtime.sci.phase != SCI_OWNER_OFF && !sci_owner_mask(&runtime.sci)) unsafe("SCI mask failed");
    if (!acpi_ec_fixture_prepare_stop()) unsafe("EC disconnect failed");
    /* uacpi_state_reset retires event machinery before its work callback.
     * Drain here while the namespace, Global Lock event and SCI owner still
     * exist. A worker awaiting firmware cannot outlive those objects. */
    if (!acpi_worker_drain(&runtime.worker, 5000)) {
        if(runtime.global_active) {
            const volatile struct acpi_facs *facs=pmm_phys_to_virt(runtime.facs);
            console_printf("acpi-global-lock: retained lock=%u sci_phase=%u token=%u heap=%u\n",
                (uint64_t)facs->global_lock,(uint64_t)runtime.sci.phase,runtime.sci.token,runtime.heap);
        }
        unsafe("pre-reset work did not drain");
    }
    uacpi_state_reset();
    runtime.initialized = false;
    if (!acpi_ec_fixture_retired()) unsafe("EC state retained after reset");
    if (runtime.sci.phase != SCI_OWNER_OFF || runtime.sci.token) unsafe("SCI retained after reset");
    if (!acpi_worker_drain(&runtime.worker, 1000) || !acpi_worker_stop(&runtime.worker, 1000)) unsafe("worker stop failed");
    bool restored = true; /* Route restoration belongs to the SCI owner. */
    if (!(runtime.original_control & 1) && (inw(0x604) & 1)) {
        outb(0xb2, runtime.disable_command);
        uint64_t end = timer_monotonic_us() + 1000000;
        while ((inw(0x604) & 1) && timer_monotonic_us() < end) timer_delay_us(100);
    }
    restored &= (inw(0x604) & 1) == (runtime.original_control & 1);
    if (restored && !runtime.heap) runtime.engaged = false;
    return restored && !runtime.heap;
}
static bool bytes_equal(const char *a, const char *b, unsigned length) {
    for (unsigned i = 0; i < length; i++) if (a[i] != b[i]) return false;
    return true;
}
static uint64_t find_fixture_dsdt(void) {
    uint64_t found = 0;
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *table = acpi_table_at(i);
        if (!equal(table->signature, "DSDT") || table->length < 36 || table->length > 4096) continue;
        const uint8_t *data = pmm_phys_to_virt(table->physical_address);
        if (!bytes_equal((const char *)data + 10, "SRVROS", 6) ||
            !bytes_equal((const char *)data + 16, "INITTEST", 8)) continue;
        uint8_t sum = 0;
        for (unsigned j = 0; j < table->length; j++) sum += data[j];
        if (sum || found) return 0;
        found = table->physical_address;
    }
    return found;
}
static uacpi_table_installation_disposition fixture_table(struct acpi_sdt_hdr *header,
    uacpi_u64 *override) {
    if (!bytes_equal(header->signature, "DSDT", 4)) return UACPI_TABLE_INSTALLATION_DISPOSITION_ALLOW;
    /* QEMU appends user tables, while FADT still points at its generated DSDT.
     * Override only in explicit start-fixture mode. Never modify either table. */
    if (bytes_equal(header->oem_table_id, "INITTEST", 8))
        return UACPI_TABLE_INSTALLATION_DISPOSITION_DENY; /* Already installed via FADT override. */
    *override = runtime.fixture_dsdt;
    return UACPI_TABLE_INSTALLATION_DISPOSITION_PHYSICAL_OVERRIDE;
}
static void load_namespace_work(void *context) {
    (void)context;
    if (!acpi_ec_fixture_bind_early(runtime.load_deny)) {
        runtime.load_result = UACPI_STATUS_DENIED;
    } else {
        uint64_t errors = runtime.errors, denied = runtime.denied;
        runtime.load_result = uacpi_namespace_load();
        if (!runtime.load_result && (runtime.errors != errors || runtime.denied != denied))
            runtime.load_result = UACPI_STATUS_DENIED;
    }
    console_printf("acpi-runtime: namespace worker returned status=%u\n", (uint64_t)runtime.load_result);
    runtime.load_done = true;
}
static bool start_runtime(bool fail_install, bool fixture, bool early, bool deny, bool fail_io) {
    if (runtime.engaged || !timer_reference_ready() || !rsdp_request.response) return false;
    const struct acpi_fadt *fadt = acpi_q35_fixture_fadt();
    if (!fadt) return false;
    struct acpi_event_io event_io = {.fixed_enable_mask = (1u<<5) | (1u<<8)};
    bool parsed = false;
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *table = acpi_table_at(i);
        if (pmm_phys_to_virt(table->physical_address) != (const void *)fadt) continue;
        parsed = acpi_event_resources_parse(&event_io.resources,
            (const uint8_t *)fadt, table->length) == ACPI_EC_RESOURCES_OK;
        break;
    }
    /* Fixed global-lock/power-button users only; no GPE enablement. The full
     * disabled GPE status domain is owned by this disposable Q35 fixture. */
    if (!parsed || inw(0x602)) return false;
    uint64_t fixture_dsdt = fixture ? find_fixture_dsdt() : 0;
    if (fixture && !fixture_dsdt) return false;
    for (uint16_t port = 0x628; port < 0x630; port++) if (inb(port)) return false;
    uint32_t low, high;
    if (!ioapic_read_gsi(9, &low, &high) || !(low & (1u<<16)) || (low & ((1u<<12)|(1u<<14)))) return false;
    runtime = (typeof(runtime)){
        .engaged = true,
        .original_control = inw(0x604), .disable_command = fadt->acpi_disable,
        .rsdp = (uintptr_t)rsdp_request.response->address - pmm_hhdm_offset(),
        .facs = fadt->firmware_ctrl, .fault_install = fail_install, .fixture_dsdt = fixture_dsdt,
        .event_io = event_io,
        .fault_init_io = fail_io,
    };
    if (!acpi_worker_start(&runtime.worker)) { runtime.engaged = false; return false; }
    acpi_ec_fixture_reset(&runtime.worker);
    uacpi_status result = fixture ? uacpi_set_table_installation_handler(fixture_table) : UACPI_STATUS_OK;
    if (result) goto failed;
    result = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE | UACPI_FLAG_BAD_CSUM_FATAL | UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL);
    if (!result && (runtime.errors || runtime.denied)) {
        console_printf("acpi-runtime: initialize API=success refused errors=%u denied=%u\n",runtime.errors,runtime.denied);
        result = UACPI_STATUS_DENIED;
    }
    if (result) goto failed;
    uacpi_context_set_loop_timeout(1);
    uacpi_context_set_max_call_stack_depth(32);
    if (early) {
        runtime.loading = true; runtime.load_deny = deny;
        if (!acpi_worker_enqueue(&runtime.worker, &runtime.load_item, UACPI_WORK_NOTIFICATION,
                                load_namespace_work, NULL)) {
            runtime.loading = false; result = UACPI_STATUS_DENIED; goto failed;
        }
        if (!acpi_worker_drain(&runtime.worker, 5000)) unsafe("namespace load deadline; state retained");
        runtime.loading = false;
        result = runtime.load_done ? runtime.load_result : UACPI_STATUS_INTERNAL_ERROR;
        console_printf("acpi-runtime: early-load=%s worker=yes status=%u\n",
                       result ? "FAILED" : "complete", (uint64_t)result);
    } else result = uacpi_namespace_load();
    if (result) goto failed;
    result = uacpi_install_fixed_event_handler(UACPI_FIXED_EVENT_POWER_BUTTON, power_handler, NULL);
    if (result) goto failed;
    result = uacpi_enter_acpi_mode();
    if (result) goto failed;
    if (runtime.errors || runtime.denied) { result = UACPI_STATUS_DENIED; goto failed; }
    runtime.initialized = true;
    return true;
failed:
    console_printf("acpi-runtime: initialization failed status=%s cleanup=%s\n", uacpi_status_to_string(result), stop_runtime() ? "yes" : "NO");
    return false;
}
bool acpi_runtime_fixture_initialize_namespace(void) {
    uint64_t flags = irq_save();
    bool allowed = (flags & (1ull << 9)) && runtime.initialized && !runtime.stopping &&
        runtime.worker.identity && scheduler_current_thread_id() == runtime.worker.identity;
    irq_restore(flags);
    if (!allowed) return false;
    uint64_t errors = runtime.errors, denied = runtime.denied;
    uacpi_status status = uacpi_namespace_initialize();
    bool ok = !status && runtime.errors == errors && runtime.denied == denied;
    console_printf("acpi-runtime: namespace=%s status=%u errors=%u denied=%u\n",
        ok ? "initialized" : "FAILED", (uint64_t)status,
        runtime.errors - errors, runtime.denied - denied);
    return ok;
}
/* Disposable Q35 firmware-state injection only. No native adapter calls this.
 * Tests the pinned full uACPI FACS handshake, not a mock acquisition routine. */
static volatile uint32_t *global_word(void) {
    return (volatile uint32_t *)((uintptr_t)pmm_phys_to_virt(runtime.facs) +
        __builtin_offsetof(struct acpi_facs, global_lock));
}
static void global_work(void *context) {
    (void)context;
    uint32_t seq=0;
    uint64_t denied=runtime.denied, errors=runtime.errors;
    uacpi_status acquired=uacpi_acquire_global_lock(100,&seq);
    uint32_t held=*global_word();
    if (acquired || (held & 3) != 2 || !seq || runtime.global_mode==2)
        unsafe("Global Lock acquisition failed");
    if (runtime.global_mode==3) *global_word()=held|1; /* Firmware requests handoff. */
    uacpi_status released=uacpi_release_global_lock(seq);
    uint32_t after=*global_word();
    console_printf("acpi-global-lock: acquire=%u release=%u held=%u after=%u denied=%u errors=%u\n",
        (uint64_t)acquired,(uint64_t)released,(uint64_t)held,(uint64_t)after,
        runtime.denied-denied,runtime.errors-errors);
    if (runtime.denied!=denied || runtime.errors!=errors)
        unsafe("Global Lock release denied");
    if (released || (after & 3) || after!=(held & ~3u))
        unsafe("Global Lock release failed");
    runtime.global_active=false;
    console_write("acpi-global-lock: PASS\n");
}
static void queue_global(unsigned mode) {
    uint64_t flags=irq_save();
    bool ok=runtime.initialized && !runtime.stopping && runtime.facs &&
        !runtime.global_active && acpi_work_idle(&runtime.worker.work);
    if (ok) {
        const struct acpi_facs *facs=pmm_phys_to_virt(runtime.facs);
        ok=bytes_equal(facs->signature,"FACS",4) && facs->length>=64 && !(*global_word()&3);
    }
    if (ok) {
        runtime.global_mode=mode;runtime.global_active=true;
        if(mode==2) *global_word()|=2; /* Firmware owns lock and never releases. */
        ok=acpi_worker_enqueue(&runtime.worker,&runtime.global_item,UACPI_WORK_NOTIFICATION,global_work,NULL);
        if(!ok) unsafe("Global Lock fixture queue failed");
    }
    console_printf("acpi-global-lock: %s mode=%u\n",ok?"queued":"REFUSED",(uint64_t)mode);
    irq_restore(flags);
}
static void wedged_work(void *context) {
    (void)context;
    console_write("acpi-runtime: wedge-blocked\n");
    semaphore_wait(&runtime.wedge_gate, SEMAPHORE_WAIT_FOREVER);
    unsafe("unexpected wedge wake");
}
void acpi_runtime_test_command(const char *args) {
    bool early = equal(args, "start-early") || equal(args, "start-early-denied") ||
        equal(args, "start-early-fail");
    if (early || equal(args, "start") || equal(args, "start-fail") || equal(args, "start-fixture") || equal(args,"start-io-fail")) {
        bool ok = start_runtime(equal(args, "start-fail") || equal(args, "start-early-fail"),
            early || equal(args, "start-fixture"), early,
            equal(args, "start-early-denied"), equal(args,"start-io-fail"));
        console_printf("acpi-runtime: %s\n", ok ? "READY" : "REFUSED");
    } else if (equal(args, "stop")) {
        uint64_t flags = irq_save();
        bool pending = !acpi_work_idle(&runtime.worker.work);
        irq_restore(flags);
        bool ok = stop_runtime();
        console_printf("acpi-runtime: stopped=%s heap=%u errors=%u denied=%u pending=%u\n",
            ok ? "yes" : "NO", runtime.heap, runtime.errors, runtime.denied, (uint64_t)pending);
    } else if (args[0] == 'e' && args[1] == 'c' && args[2] == ' ' && runtime.initialized) {
        acpi_ec_fixture_command(args + 3);
    } else if (equal(args,"global-lock") || equal(args,"global-lock-block") || equal(args,"global-lock-release-denied")) {
        queue_global(equal(args,"global-lock-block") ? 2 : equal(args,"global-lock-release-denied") ? 3 : 1);
    } else if (equal(args, "wedge") && runtime.initialized) {
        semaphore_init(&runtime.wedge_gate, 0);
        uacpi_kernel_schedule_work(UACPI_WORK_NOTIFICATION, &runtime.wedge_item, wedged_work, NULL);
    } else if (equal(args, "race")) {
        uint64_t held = 0, value = 0, end = timer_monotonic_us() + 1000000;
        while (runtime.initialized && timer_monotonic_us() < end) {
            if (uacpi_eval_simple_integer(NULL, "\\HELD", &held) || held) break;
            scheduler_yield();
        }
        uint64_t before = runtime.contended_waits, start = timer_monotonic_us();
        uacpi_status result = held ? uacpi_eval_simple_integer(NULL, "\\TST0", &value) : UACPI_STATUS_TIMEOUT;
        uint64_t waits = runtime.contended_waits - before;
        console_printf("acpi-runtime: race=%s value=%u waits=%u elapsed_us=%u\n",
            !result && waits ? "PASS" : "FAIL", value, waits, timer_monotonic_us() - start);
    } else if (equal(args, "eval")) {
        uint64_t value = 0;
        uacpi_status result = runtime.initialized ? uacpi_eval_simple_integer(NULL,"\\TST0",&value) : UACPI_STATUS_DENIED;
        console_printf("acpi-runtime: eval=%s value=%u\n", uacpi_status_to_string(result), value);
    } else {
        uint64_t flags = irq_save();
        uint64_t active = runtime.initialized, interrupts = runtime.interrupts;
        uint64_t events = runtime.power_events, work = runtime.work_events, value = runtime.last_value;
        uint64_t errors = runtime.errors, denied = runtime.denied, heap = runtime.heap, peak = runtime.peak;
        irq_restore(flags);
        console_printf("acpi-runtime: active=%u irq=%u events=%u work=%u value=%u errors=%u denied=%u heap=%u peak=%u\n",
            active, interrupts, events, work, value, errors, denied, heap, peak);
    }
}
bool aml_probe_run(void *rsdp, struct limine_memmap_response *map) {
    (void)rsdp; (void)map;
    console_write("aml-probe: unavailable in separate full-runtime VM test kernel\n");
    return false;
}
