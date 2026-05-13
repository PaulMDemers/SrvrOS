#include <srvros/console.h>
#include <srvros/vmm.h>

#include "lapic.h"

#include <stdint.h>

#define IA32_APIC_BASE_MSR 0x1b
#define IA32_APIC_BASE_ENABLE (1ull << 11)
#define LAPIC_BASE_MASK 0x000ffffffffff000ull
#define LAPIC_EOI_OFFSET 0x0b0
#define LAPIC_SPURIOUS_OFFSET 0x0f0
#define LAPIC_LVT_TIMER_OFFSET 0x320
#define LAPIC_TIMER_INITIAL_COUNT_OFFSET 0x380
#define LAPIC_TIMER_DIVIDE_OFFSET 0x3e0
#define LAPIC_VIRTUAL_BASE 0xffffc000fee00000ull
#define LAPIC_TIMER_PERIODIC (1u << 17)
#define LAPIC_SPURIOUS_ENABLE (1u << 8)

static volatile uint32_t *lapic;

static uint64_t rdmsr(uint32_t msr) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

bool lapic_init(void) {
    uint64_t apic_base = rdmsr(IA32_APIC_BASE_MSR);
    uint64_t physical = apic_base & LAPIC_BASE_MASK;

    if ((apic_base & IA32_APIC_BASE_ENABLE) == 0 || physical == 0) {
        console_write("lapic: disabled\n");
        return false;
    }

    if (!vmm_map_page(LAPIC_VIRTUAL_BASE,
            physical,
            VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
        console_write("lapic: mapping failed\n");
        return false;
    }

    lapic = (volatile uint32_t *)LAPIC_VIRTUAL_BASE;
    lapic[LAPIC_SPURIOUS_OFFSET / sizeof(uint32_t)] = LAPIC_SPURIOUS_ENABLE | 0xff;
    console_printf("lapic: mapped phys=%x virt=%x\n", physical, LAPIC_VIRTUAL_BASE);
    return true;
}

void lapic_timer_init(uint8_t vector, uint32_t initial_count) {
    if (lapic == 0) {
        return;
    }

    lapic[LAPIC_TIMER_DIVIDE_OFFSET / sizeof(uint32_t)] = 0x3;
    lapic[LAPIC_LVT_TIMER_OFFSET / sizeof(uint32_t)] = LAPIC_TIMER_PERIODIC | vector;
    lapic[LAPIC_TIMER_INITIAL_COUNT_OFFSET / sizeof(uint32_t)] = initial_count;

    console_printf("lapic: timer vector=%u initial_count=%u\n",
        (uint64_t)vector,
        (uint64_t)initial_count);
}

void lapic_send_eoi(void) {
    if (lapic != 0) {
        lapic[LAPIC_EOI_OFFSET / sizeof(uint32_t)] = 0;
    }
}
