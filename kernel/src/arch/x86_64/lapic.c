#include <srvros/console.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>

#include "lapic.h"

#include <stdint.h>

#define IA32_APIC_BASE_MSR 0x1b
#define IA32_APIC_BASE_ENABLE (1ull << 11)
#define LAPIC_BASE_MASK 0x000ffffffffff000ull
#define LAPIC_EOI_OFFSET 0x0b0
#define LAPIC_SPURIOUS_OFFSET 0x0f0
#define LAPIC_LVT_TIMER_OFFSET 0x320
#define LAPIC_TIMER_INITIAL_COUNT_OFFSET 0x380
#define LAPIC_TIMER_CURRENT_COUNT_OFFSET 0x390
#define LAPIC_TIMER_MASKED (1u << 16)
#define LAPIC_TIMER_DIVIDE_OFFSET 0x3e0
#define LAPIC_VIRTUAL_BASE 0xffffc000fee00000ull
#define LAPIC_TIMER_PERIODIC (1u << 17)
#define LAPIC_SPURIOUS_ENABLE (1u << 8)

static volatile uint32_t *lapic;
static uint32_t idle_timer_count;

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
    idle_timer_count=0;
    if (lapic == 0) {
        return;
    }

    lapic[LAPIC_TIMER_DIVIDE_OFFSET / sizeof(uint32_t)] = 0x3; /* Divide by 16. */
    bool calibrated = false;
    if (timer_reference_ready()) {
        lapic[LAPIC_LVT_TIMER_OFFSET / sizeof(uint32_t)] = LAPIC_TIMER_MASKED | vector;
        lapic[LAPIC_TIMER_INITIAL_COUNT_OFFSET / sizeof(uint32_t)] = 0xffffffffu;
        uint64_t start = timer_monotonic_us();
        uint32_t before = lapic[LAPIC_TIMER_CURRENT_COUNT_OFFSET / sizeof(uint32_t)];
        if (timer_delay_us(50000)) {
            uint32_t after = lapic[LAPIC_TIMER_CURRENT_COUNT_OFFSET / sizeof(uint32_t)];
            uint64_t elapsed = timer_monotonic_us() - start;
            uint64_t per_tick = elapsed ? (uint64_t)(before - after) * 10000 / elapsed : 0;
            if (before > after && after != 0 && per_tick > 0 && per_tick <= UINT32_MAX) {
                initial_count = (uint32_t)per_tick;
                calibrated = true;
            }
        }
    }
    lapic[LAPIC_LVT_TIMER_OFFSET / sizeof(uint32_t)] = LAPIC_TIMER_PERIODIC | vector;
    lapic[LAPIC_TIMER_INITIAL_COUNT_OFFSET / sizeof(uint32_t)] = initial_count;
    if(calibrated && vector==32)idle_timer_count=initial_count;
    console_printf("lapic: timer vector=%u initial_count=%u calibrated=%s target=100Hz\n",
        (uint64_t)vector, (uint64_t)initial_count, calibrated ? "yes" : "no");
}

void lapic_send_eoi(void) {
    if (lapic != 0) {
        lapic[LAPIC_EOI_OFFSET / sizeof(uint32_t)] = 0;
    }
}

/* Intel SDM vol.3, local APIC register map: 32-bit words on 16-byte
 * boundaries. ISR/IRR/TMR are read-only. LINT0 is read without modification. */
static uint32_t snapshot_read(unsigned offset) {
#ifdef SRVROS_LAPIC_HOST_TEST
    extern uint32_t lapic_test_read(unsigned);
    return lapic_test_read(offset);
#else
    return lapic[offset/sizeof(uint32_t)];
#endif
}
bool timer_idle_wakeup_ready(void) {
    return lapic && idle_timer_count && timer_reference_ready() && timer_ticks() &&
        snapshot_read(LAPIC_LVT_TIMER_OFFSET)==(LAPIC_TIMER_PERIODIC|32) &&
        snapshot_read(LAPIC_TIMER_INITIAL_COUNT_OFFSET)==idle_timer_count &&
        snapshot_read(LAPIC_TIMER_CURRENT_COUNT_OFFSET)>0 &&
        snapshot_read(LAPIC_TIMER_DIVIDE_OFFSET)==3;
}
bool lapic_vector_snapshot(uint8_t vector,struct lapic_vector_state *out) {
    if(!out)return false;
    *out=(struct lapic_vector_state){0};
    if(!lapic || vector<32 || vector==255)return false;
    unsigned bank=(vector/32)*16;
    uint32_t mask=1u<<(vector%32);
    bool isr=(snapshot_read(0x100+bank)&mask)!=0;
    bool irr=(snapshot_read(0x200+bank)&mask)!=0;
    bool tmr=(snapshot_read(0x180+bank)&mask)!=0;
    out->bits=(isr?1u:0u)|(irr?2u:0u)|(tmr?4u:0u);
    out->lint0=snapshot_read(0x350);
    return true;
}
