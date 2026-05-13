#include <srvros/console.h>
#include <srvros/timer.h>

#include "io.h"
#include "irq.h"

#define PIT_FREQUENCY 1193182
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43

volatile uint64_t timer_irq_ticks;
static uint64_t configured_frequency;

void timer_init(uint64_t frequency_hz) {
    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    uint64_t divisor = PIT_FREQUENCY / frequency_hz;
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 0xffff) {
        divisor = 0xffff;
    }

    configured_frequency = PIT_FREQUENCY / divisor;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xff);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xff);

    irq_enable_line(0);
    console_printf("pit: frequency=%uHz divisor=%u\n", configured_frequency, divisor);
}

void timer_handle_tick(void) {
    timer_irq_ticks++;
}

uint64_t timer_ticks(void) {
    return timer_irq_ticks;
}

void timer_wait_ticks(uint64_t wait_ticks) {
    uint64_t target = timer_irq_ticks + wait_ticks;
    while (timer_irq_ticks < target) {
        __asm__ volatile ("hlt");
    }
}

bool timer_wait_ticks_bounded(uint64_t wait_ticks, uint64_t spin_limit) {
    uint64_t target = timer_irq_ticks + wait_ticks;

    while (timer_irq_ticks < target && spin_limit > 0) {
        __asm__ volatile ("pause");
        spin_limit--;
    }

    return timer_irq_ticks >= target;
}
