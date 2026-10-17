/* ACPI 6.6 FADT reset GAS at 116, reset value at 128. Only the byte-wide
 * System I/O form is supported here. Do not guess at memory/PCI addresses. */
#include <srvros/acpi.h>
#include <srvros/arch.h>
#include <srvros/console.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include "io.h"
#include <stdbool.h>
#include <stdint.h>

static uint16_t fadt_reset_port(const uint8_t *table, uint64_t length, uint8_t *value) {
    if (!table || length < 129) return 0;
    uint32_t flags = (uint32_t)table[112] | (uint32_t)table[113] << 8 |
        (uint32_t)table[114] << 16 | (uint32_t)table[115] << 24;
    if (!(flags & (1u << 10)) || table[116] != 1 || table[117] != 8 ||
        table[118] != 0 || table[119] > 1) return 0;
    uint64_t address = 0;
    for (unsigned i = 0; i < 8; i++) address |= (uint64_t)table[120 + i] << (8 * i);
    if (!address || address > 0xffff) return 0;
    *value = table[128];
    return (uint16_t)address;
}

bool arch_reboot(void) {
    uint8_t value = 0;
    uint16_t port = 0;
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *t = acpi_table_at(i);
        if (t->signature[0] == 'F' && t->signature[1] == 'A' &&
            t->signature[2] == 'C' && t->signature[3] == 'P') {
            port = fadt_reset_port(pmm_phys_to_virt(t->physical_address), t->length, &value);
            break;
        }
    }
    if (!port) {
        console_write("reboot: no supported ACPI reset register; manual reboot required\n");
        return false;
    }
    console_printf("reboot: ACPI reset port=%x value=%x\n", (uint64_t)port, (uint64_t)value);
    console_sync();
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    outb(port, value);
    timer_delay_us(1000000);
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
    console_write("reboot: reset did not complete; manual reboot required\n");
    return false;
}
