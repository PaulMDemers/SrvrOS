/* Synthetic EC/SMBus controller behind real transports and uACPI opregions.
 * No native port binding. All EC/AML/battery work belongs to one worker;
 * per-byte locking alone would not exclude an AML SMBus bank transaction. */
#include "ec_fixture.h"
#include "qemu_adapter.h"
#include <srvros/console.h>
#include <srvros/ec_transport.h>
#include <srvros/sbs_sample.h>
#include <srvros/timer.h>
#include <uacpi/uacpi.h>
#include <uacpi/namespace.h>
#include <uacpi/opregion.h>
#include <uacpi/notify.h>

static struct {
    struct acpi_work_worker *worker;
    struct acpi_work_item setup_item, query_item, sample_item, stop_item;
    struct ec_transport transport;
    struct sbs_sampler sampler;
    uacpi_namespace_node *node, *handler_node;
    uint8_t memory[256], query[64], mode, address, output;
    uint16_t words[32];
    unsigned step, busy, head, count, lease;
    uint64_t commands, transactions, queries, notifications, errors, attached, detached;
    uint64_t blocked_callers, queued_during_sample, samples_failed;
    bool obf, ready, query_scheduled, sample_scheduled, inject, stalled;
    bool region_installed, notify_installed, setup_scheduled, setup_delay;
    bool stopping, stop_done, stop_ok, disconnect_delay, namespace_init;
    bool early, deny_early_io, reg_connected;
} ec;

enum { LEASE_NONE, LEASE_BYTE, LEASE_QUERY, LEASE_SAMPLE };
static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}
static void irq_restore(uint64_t flags) {
    if (flags & (1ull << 9)) __asm__ volatile("sti" ::: "memory");
}
static bool equal(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static bool worker_context(void) {
    uint64_t flags = irq_save();
    bool ok = (flags & (1ull << 9)) && ec.worker && ec.worker->identity &&
        scheduler_current_thread_id() == ec.worker->identity;
    irq_restore(flags);
    return ok;
}
static void pace(unsigned polls) {
    /* Bound uninterrupted polling; ordinary worker remains storage-gated. */
    if (!(polls % 16)) timer_wait_ticks(1);
    else timer_delay_us(20);
}
static uint8_t model_status(void *context) {
    (void)context;
    return (ec.stalled ? 2 : 0) | (ec.obf ? 1 : 0) | (ec.count ? 32 : 0);
}
static uint8_t model_read(void *context) {
    (void)context;
    if (!ec.obf) ec.errors++;
    ec.obf = false;
    return ec.output;
}
static void model_command(void *context, uint8_t command) {
    (void)context;
    ec.commands++;
    ec.mode = command;
    ec.step = 0;
    if (command == 0x84) {
        uint64_t flags = irq_save();
        ec.output = ec.count ? ec.query[ec.head] : 0;
        if (ec.count) { ec.head = (ec.head + 1) % 64; ec.count--; }
        ec.obf = true;
        irq_restore(flags);
    } else if (command != 0x80 && command != 0x81) ec.errors++;
}
static void model_write(void *context, uint8_t value) {
    (void)context;
    if (!ec.step++) {
        ec.address = value;
        if (ec.mode == 0x80) {
            if (value == 0x20 && ec.busy && !--ec.busy) ec.memory[0x20] = 0;
            ec.output = ec.memory[value]; ec.obf = true;
        }
        return;
    }
    if (ec.mode != 0x81) { ec.errors++; return; }
    ec.memory[ec.address] = value;
    if (ec.address != 0x20) return;
    if (value != 9 || ec.memory[0x22] != 0x16 || ec.memory[0x23] >= 32) {
        ec.errors++; return;
    }
    uint16_t word = ec.words[ec.memory[0x23]];
    ec.memory[0x24] = word; ec.memory[0x25] = word >> 8;
    ec.busy = 2; ec.transactions++;
    if (ec.inject && ec.lease == LEASE_SAMPLE) {
        ec.inject = false;
        if (!acpi_ec_fixture_signal(0x20) || !acpi_ec_fixture_signal(0x21)) ec.errors++;
    }
}
static bool transfer(bool write, uint8_t address, uint8_t *value) {
    if (!worker_context() || ec.lease != LEASE_NONE) { ec.blocked_callers++; return false; }
    ec.lease = LEASE_BYTE;
    bool ok = ec_transport_begin(&ec.transport, write, address, *value, timer_monotonic_us(), 100000);
    int result = 0;
    for (unsigned polls = 1; ok && !result; polls++) {
        result = ec_transport_poll(&ec.transport, timer_monotonic_us());
        if (!result) pace(polls);
    }
    if (result == 1 && !write) *value = ec.transport.result;
    ec.lease = LEASE_NONE;
    return ok && result == 1;
}
static uacpi_status region_handler(uacpi_region_op op, uacpi_handle data) {
    if (op == UACPI_REGION_OP_ATTACH) {
        uacpi_region_attach_data *attach = data;
        if (attach->handler_context != &ec || attach->generic_info.base > 255 ||
            !attach->generic_info.length || attach->generic_info.length > 256 - attach->generic_info.base)
            return UACPI_STATUS_DENIED;
        attach->out_region_context = &ec; ec.attached++; return UACPI_STATUS_OK;
    }
    if (op == UACPI_REGION_OP_DETACH) { ec.detached++; return UACPI_STATUS_OK; }
    if (op != UACPI_REGION_OP_READ && op != UACPI_REGION_OP_WRITE) return UACPI_STATUS_UNIMPLEMENTED;
    uacpi_region_rw_data *rw = data;
    if (rw->handler_context != &ec || rw->region_context != &ec || rw->byte_width != 1 || rw->address > 255)
        return UACPI_STATUS_INVALID_ARGUMENT;
    if (ec.deny_early_io && uacpi_get_current_init_level() < UACPI_INIT_LEVEL_NAMESPACE_LOADED)
        return UACPI_STATUS_DENIED;
    uint8_t value = rw->value;
    if (!transfer(op == UACPI_REGION_OP_WRITE, rw->address, &value)) return UACPI_STATUS_DENIED;
    if (op == UACPI_REGION_OP_READ) rw->value = value;
    return UACPI_STATUS_OK;
}
bool acpi_ec_fixture_bind_early(bool deny_io) {
    if (!worker_context() || ec.region_installed ||
        uacpi_get_current_init_level() != UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED) return false;
    ec.early = true;
    ec.deny_early_io = deny_io;
    ec.handler_node = uacpi_namespace_root();
    if (uacpi_install_address_space_handler(ec.handler_node,
        UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, region_handler, &ec)) return false;
    ec.region_installed = true;
    return true;
}
static uacpi_status notify_handler(uacpi_handle context, uacpi_namespace_node *node, uacpi_u64 value) {
    if (context != &ec || node != ec.node || value != 0x80 || !worker_context() || ec.lease) {
        ec.errors++; return UACPI_STATUS_INTERNAL_ERROR;
    }
    uint64_t read = 0;
    if (uacpi_eval_simple_integer(ec.node, "RD00", &read) || read != 0x42) ec.errors++;
    ec.notifications++;
    return UACPI_STATUS_OK;
}
static void setup_impl(void *context) {
    (void)context;
    if (!worker_context() || uacpi_namespace_node_find(NULL, "\\_SB.ECT0", &ec.node)) {
        ec.errors++; return;
    }
    if (ec.early) {
        uint64_t value = 0, reg = 1;
        if (!ec.region_installed ||
            uacpi_eval_simple_integer(ec.node, "LVAL", &value) || value != 0x42 ||
            uacpi_eval_simple_integer(ec.node, "LREG", &reg) || reg || ec.memory[0x11] != 0x3c) {
            ec.errors++; return;
        }
        console_write("acpi-ec: early-load verified read=66 write=60 reg=0 worker=yes\n");
    } else {
        ec.handler_node = ec.node;
        /* Installation after load may itself invoke _REG. */
        ec.reg_connected = true;
        if (uacpi_install_address_space_handler(ec.handler_node, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, region_handler, &ec)) {
            ec.errors++; return;
        }
        ec.region_installed = true;
    }
    if (uacpi_install_notify_handler(ec.node, notify_handler, &ec)) { ec.errors++; return; }
    ec.notify_installed = true;
    ec.reg_connected = true;
    if (uacpi_reg_all_opregions(ec.handler_node, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER)) { ec.errors++; return; }
    uint64_t reg = 0, value = 0;
    if (uacpi_eval_simple_integer(ec.node, "REGC", &reg) || reg != 1 ||
        uacpi_eval_simple_integer(ec.node, "RVAL", &value) || value != 0x42 || ec.memory[0x11] != 0xa5)
        ec.errors++;
    else {
        if (ec.namespace_init) {
            uint64_t count = 0, ini = 0, sta = 0;
            if (!acpi_runtime_fixture_initialize_namespace() ||
                uacpi_eval_simple_integer(ec.node, "INIC", &count) || count != 1 ||
                uacpi_eval_simple_integer(ec.node, "IVAL", &ini) || ini != 0x42 ||
                uacpi_eval_simple_integer(ec.node, "SVAL", &sta) || sta != 0x42) {
                ec.errors++; return;
            }
        }
        ec.ready = true;
    }
}
static void setup_work(void *context) {
    if (ec.setup_delay) timer_wait_ticks(10);
    setup_impl(context);
    ec.setup_scheduled = false;
}
static void query_work(void *context) {
    (void)context;
    if (!worker_context() || ec.lease || ec.sampler.active) { ec.errors++; return; }
    for (unsigned batch = 0; batch < 8; batch++) {
        ec.lease = LEASE_QUERY;
        bool ok = ec_transport_begin_query(&ec.transport, timer_monotonic_us(), 100000);
        int result = 0;
        for (unsigned polls = 1; ok && !result; polls++) {
            result = ec_transport_poll(&ec.transport, timer_monotonic_us());
            if (!result) pace(polls);
        }
        uint8_t code = ec.transport.result;
        ec.lease = LEASE_NONE; /* _Qxx may access EC/SMBus: release before AML. */
        if (!ok || result != 1) { ec.errors++; break; }
        if (!code) break;
        char method[] = "_Q00";
        static const char hex[] = "0123456789ABCDEF";
        method[2] = hex[code >> 4]; method[3] = hex[code & 15];
        if (uacpi_execute_simple(ec.node, method)) ec.errors++;
        uint64_t count = 0, value = 0;
        if (uacpi_eval_simple_integer(ec.node, "QNUM", &count) || count != ec.queries + 1 ||
            uacpi_eval_simple_integer(ec.node, "QVAL", &value) || value != (code == 0x20 ? 0x42 : 12000)) ec.errors++;
        ec.queries++;
    }
    uint64_t flags = irq_save();
    if (ec.count && !ec.transport.poisoned) {
        if (!acpi_worker_enqueue(ec.worker, &ec.query_item, UACPI_WORK_GPE_EXECUTION, query_work, NULL)) ec.errors++;
    } else ec.query_scheduled = false;
    irq_restore(flags);
}
bool acpi_ec_fixture_signal(uint8_t query) {
    uint64_t flags = irq_save();
    if (ec.stopping || !ec.ready || ec.transport.poisoned || !query || ec.count == 64) {
        irq_restore(flags); return false;
    }
    ec.query[(ec.head + ec.count) % 64] = query; ec.count++;
    if (ec.sampler.active) ec.queued_during_sample++;
    bool ok = true;
    if (!ec.query_scheduled) {
        ec.query_scheduled = true;
        ok = acpi_worker_enqueue(ec.worker, &ec.query_item, UACPI_WORK_GPE_EXECUTION, query_work, NULL);
        if (!ok) { ec.query_scheduled = false; ec.errors++; }
    }
    irq_restore(flags); return ok;
}
static void sample_work(void *context) {
    (void)context;
    if (!worker_context() || ec.lease) { ec.errors++; return; }
    ec.lease = LEASE_SAMPLE;
    bool ok = sbs_sample_begin(&ec.sampler, &ec.transport, 0x20, timer_monotonic_us());
    int result = 0;
    for (unsigned polls = 1; ok && !result; polls++) {
        result = sbs_sample_poll(&ec.sampler, timer_monotonic_us());
        if (!result) pace(polls);
    }
    ec.lease = LEASE_NONE;
    if (!ok || result != 1) ec.samples_failed++;
    ec.sample_scheduled = false;
}
void acpi_ec_fixture_reset(struct acpi_work_worker *worker) {
    /* Only after the old runtime has drained/reset and all callbacks stopped. */
    ec = (typeof(ec)){.worker = worker};
    ec.memory[0x10] = 0x42;
    ec.words[0x1a] = 0x31; ec.words[9] = 12000; ec.words[10] = (uint16_t)-250;
    ec.words[13] = 86; ec.words[15] = 4300; ec.words[16] = 5000; ec.words[24] = 6000;
    ec.words[8] = 2981; ec.words[18] = 120; ec.words[19] = 65535; ec.words[22] = 0xc0;
    ec_transport_init(&ec.transport, (struct ec_io){NULL, model_status, model_read, model_command, model_write});
}
static void disconnect_work(void *context) {
    (void)context;
    bool ok = worker_context() && !ec.lease && !ec.sampler.active;
    if (ok && ec.disconnect_delay) ok = !uacpi_execute_simple(ec.node, "DLYS");
    if (ok && ec.region_installed) {
        /* Upstream may log _REG errors without returning them from uninstall.
         * Verify the fixture's completion handshake before releasing state. */
        ok = !uacpi_uninstall_address_space_handler(ec.handler_node,
            UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, region_handler);
        if (ok) {
            ec.region_installed = false;
            uint64_t reg = 1, value = 0;
            if (ec.reg_connected) ok = !uacpi_eval_simple_integer(ec.node, "REGC", &reg) && !reg &&
                !uacpi_eval_simple_integer(ec.node, "DVAL", &value) && value == 0x42 &&
                ec.memory[0x11] == 0x5a && !ec.transport.poisoned;
        }
    }
    ec.stop_ok = ok;
    ec.stop_done = true;
}
bool acpi_ec_fixture_prepare_stop(void) {
    /* Sources are masked by the owner. Close external admission before draining
     * existing setup/sample/query work; _REG disconnect can sleep and use EC IO. */
    uint64_t flags = irq_save();
    ec.stopping = true;
    irq_restore(flags);
    if (!ec.handler_node && !ec.setup_scheduled) return true;
    if (!acpi_worker_drain(ec.worker, 5000)) return false;
    if (ec.notify_installed) {
        /* Upstream removal calls wait_for_work_completion: keep this on the
         * lifecycle caller, otherwise the worker would wait for itself. */
        if (uacpi_uninstall_notify_handler(ec.node, notify_handler)) return false;
        ec.notify_installed = false;
    }
    if (!acpi_worker_enqueue(ec.worker, &ec.stop_item, UACPI_WORK_NOTIFICATION,
        disconnect_work, NULL) || !acpi_worker_drain(ec.worker, 5000)) return false;
    bool ok = ec.stop_done && ec.stop_ok;
    console_printf("acpi-ec: disconnect=%s worker=yes io=%s\n",
        ok ? "complete" : "FAILED", ok ? (ec.reg_connected ? "verified" : "not-connected") : "unverified");
    if (!ok) return false; /* Caller halts with namespace/worker still owned. */
    ec.ready = false;
    return true;
}
bool acpi_ec_fixture_retired(void) {
    bool ok = ec.attached == ec.detached && !ec.lease && !ec.setup_scheduled && !ec.query_scheduled && !ec.sample_scheduled;
    if (ec.handler_node) console_printf("acpi-ec: retired=%s attached=%u detached=%u\n", ok ? "yes" : "NO", ec.attached, ec.detached);
    return ok;
}
void acpi_ec_fixture_command(const char *args) {
    if (equal(args, "start") || equal(args, "start-slow") || equal(args, "initialize")) {
        uint64_t flags = irq_save();
        bool ok = !ec.stopping && !ec.node && !ec.setup_scheduled && ec.worker;
        if (ok) {
            ec.setup_scheduled = true;
            ec.setup_delay = equal(args, "start-slow");
            ec.namespace_init = equal(args, "initialize");
            ok = acpi_worker_enqueue(ec.worker, &ec.setup_item, UACPI_WORK_NOTIFICATION, setup_work, NULL);
            if (!ok) ec.setup_scheduled = false;
        }
        irq_restore(flags);
        console_printf("acpi-ec: setup=%s\n", ok ? "queued" : "REFUSED");
    } else if (equal(args, "sample") || equal(args, "poison")) {
        uint64_t flags = irq_save();
        bool ok = !ec.stopping && ec.ready && !ec.sample_scheduled && !ec.transport.poisoned;
        if (ok) {
            ec.stalled = equal(args, "poison"); ec.inject = !ec.stalled;
            ec.sample_scheduled = true;
            ok = acpi_worker_enqueue(ec.worker, &ec.sample_item, UACPI_WORK_NOTIFICATION, sample_work, NULL);
            if (!ok) ec.sample_scheduled = false;
        }
        irq_restore(flags);
        console_printf("acpi-ec: sample=%s\n", ok ? "queued" : "REFUSED");
    } else if (equal(args, "disconnect-slow")) {
        bool ok = ec.ready && !ec.stopping;
        if (ok) ec.disconnect_delay = true;
        console_printf("acpi-ec: disconnect-delay=%s\n", ok ? "armed" : "REFUSED");
    } else if (equal(args, "foreign")) {
        uint8_t value = 0;
        bool ok = transfer(false, 0x10, &value);
        console_printf("acpi-ec: foreign=%s\n", ok ? "UNSAFE" : "REFUSED");
    } else if (equal(args, "queries")) {
        bool ok = true;
        for (unsigned i = 0; i < 32; i++) ok = acpi_ec_fixture_signal(i & 1 ? 0x21 : 0x20) && ok;
        console_printf("acpi-ec: queries=%s\n", ok ? "queued" : "REFUSED");
    } else {
        uint64_t flags = irq_save();
        uint64_t ready = ec.ready, queries = ec.queries, notes = ec.notifications;
        uint64_t queued = ec.queued_during_sample, errors = ec.errors, foreign = ec.blocked_callers;
        uint64_t attached = ec.attached, detached = ec.detached, failed = ec.samples_failed;
        uint64_t poison = ec.transport.poisoned, current = ec.sampler.current;
        uint64_t generation = ec.sampler.sample.generation, soc = ec.sampler.sample.relative_soc;
        uint64_t voltage = ec.sampler.sample.voltage_uv, transactions = ec.transactions;
        uint64_t pending = ec.setup_scheduled || ec.sample_scheduled || ec.query_scheduled || ec.count;
        irq_restore(flags);
        console_printf("acpi-ec: ready=%u queries=%u notify=%u queued_in_sample=%u errors=%u foreign=%u attached=%u detached=%u pending=%u\n",
            ready, queries, notes, queued, errors, foreign, attached, detached, pending);
        console_printf("acpi-ec: generation=%u current=%u soc=%u voltage_uv=%u transactions=%u failed=%u poisoned=%u\n",
            generation, current, soc, voltage, transactions, failed, poison);
    }
}
