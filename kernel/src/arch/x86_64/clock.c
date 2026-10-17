/* ACPI PM timer reference, independent of interrupt delivery and CPU speed.
 * ACPI 6.6 FADT: PM_TMR_BLK at 76, X_PM_TMR_BLK at 208. */
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include "io.h"
#include <stddef.h>
#define PM_FREQUENCY 3579545ull
#define PM_MASK 0x00ffffffu
static uint16_t pm_port;
static uint32_t last_counter;
static uint64_t accumulated;

static uint64_t read_le(const uint8_t *p, unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; i++) value |= (uint64_t)p[i] << (8 * i);
    return value;
}

static uint16_t fadt_timer_port(const uint8_t *data, uint64_t length) {
    if (data == NULL || length < 116 || data[91] != 4) return 0;
    /* Hardware-reduced ACPI omits fixed power-management registers. */
    if (read_le(data + 112, 4) & (1u << 20)) return 0;
    uint64_t port = read_le(data + 76, 4);
    if (length >= 220 && read_le(data + 212, 8) != 0) {
        /* This backend supports System I/O; do not misread a memory GAS as I/O. */
        if (data[208] != 1) return 0;
        port = read_le(data + 212, 8);
    }
    return port != 0 && port <= 0xfffc ? (uint16_t)port : 0;
}

static uint64_t clock_irq_save(void) {
#ifdef SRVROS_CLOCK_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}
static void clock_irq_restore(uint64_t flags) {
#ifdef SRVROS_CLOCK_HOST_TEST
    (void)flags;
#else
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
}
static uint32_t read_counter(void) {
#ifdef SRVROS_CLOCK_HOST_TEST
    return test_pm_read() & PM_MASK;
#else
    return inl(pm_port) & PM_MASK;
#endif
}

bool timer_reference_init(void) {
    pm_port = 0; accumulated = 0;
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *table = acpi_table_at(i);
        if (table->signature[0] == 'F' && table->signature[1] == 'A' &&
            table->signature[2] == 'C' && table->signature[3] == 'P') {
            pm_port = fadt_timer_port(pmm_phys_to_virt(table->physical_address), table->length);
            break;
        }
    }
    if (pm_port != 0) {
        last_counter = read_counter();
        bool advancing = false;
        for (unsigned i = 0; i < 100000; i++) {
            if (read_counter() != last_counter) { advancing = true; break; }
            __asm__ volatile ("pause");
        }
        if (!advancing) pm_port = 0;
    }
    console_printf("clock: reference=%s port=%x\n", pm_port ? "acpi-pm" : "unavailable", (uint64_t)pm_port);
    return pm_port != 0;
}

bool timer_reference_ready(void) { return pm_port != 0; }

uint64_t timer_monotonic_us(void) {
    if (pm_port == 0) return timer_ticks() * 10000;
    uint64_t flags = clock_irq_save();
    uint32_t now = read_counter();
    accumulated += (now - last_counter) & PM_MASK;
    last_counter = now;
    uint64_t value = (accumulated / PM_FREQUENCY) * 1000000 +
        (accumulated % PM_FREQUENCY) * 1000000 / PM_FREQUENCY;
    clock_irq_restore(flags);
    return value;
}

bool timer_delay_us(uint64_t delay) {
    if (!timer_reference_ready()) return false;
    uint64_t start = timer_monotonic_us();
    /* Separate iteration bound handles a broken/stopped reference. */
    for (uint64_t spin = 0; spin < 10000000; spin++) {
        if (timer_monotonic_us() - start >= delay) return true;
        __asm__ volatile ("pause");
    }
    return false;
}
