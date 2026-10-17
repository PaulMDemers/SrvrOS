/* Opt-in diagnostics for this single-CPU kernel. No allocation or logging in
 * the measured paths. Reference reads add overhead to the reported intervals. */
#include <srvros/latency.h>
#include <srvros/timer.h>
#include <stddef.h>

static struct latency_stats measured;
static uint64_t starts[LATENCY_SITE_COUNT], depths[LATENCY_SITE_COUNT];
static uint64_t last_tick, last_poll;
static bool have_tick, have_poll;
static uint64_t save_irq(void) {
#ifdef SRVROS_LATENCY_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}
static void restore_irq(uint64_t flags) {
#ifdef SRVROS_LATENCY_HOST_TEST
    (void)flags;
#else
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
}
static void record(struct latency_sample *sample, uint64_t us) {
    sample->count++;
    sample->total_us += us;
    if (us > sample->max_us) sample->max_us = us;
    if (us >= 1000) sample->over_1ms++;
    if (us >= 10000) sample->over_10ms++;
}
void latency_enter(enum latency_site site) {
    if (!measured.enabled || (unsigned)site >= LATENCY_SITE_COUNT) return;
    if (depths[site]++ == 0) starts[site] = timer_monotonic_us();
}
void latency_leave(enum latency_site site) {
    if (!measured.enabled || (unsigned)site >= LATENCY_SITE_COUNT || !depths[site]) return;
    if (--depths[site] == 0) record(&measured.critical[site], timer_monotonic_us() - starts[site]);
}
void latency_tick(uint64_t now) {
    if (!measured.enabled) return;
    if (have_tick) record(&measured.tick_gap, now - last_tick);
    last_tick = now;
    have_tick = true;
}
void latency_input_poll(uint64_t now) {
    uint64_t flags = save_irq();
    if (measured.enabled) {
        if (have_poll) record(&measured.input_poll_gap, now - last_poll);
        last_poll = now;
        have_poll = true;
    }
    restore_irq(flags);
}
bool latency_reset(void) {
    uint64_t flags = save_irq();
    measured = (struct latency_stats){0};
    for (unsigned i = 0; i < LATENCY_SITE_COUNT; i++) depths[i] = starts[i] = 0;
    have_tick = have_poll = false;
    measured.enabled = timer_reference_ready();
    bool enabled = measured.enabled;
    restore_irq(flags);
    return enabled;
}
void latency_stop(void) {
    uint64_t flags = save_irq();
    measured.enabled = false;
    restore_irq(flags);
}
void latency_snapshot(struct latency_stats *out) {
    if (out == NULL) return;
    uint64_t flags = save_irq();
    *out = measured;
    restore_irq(flags);
}
