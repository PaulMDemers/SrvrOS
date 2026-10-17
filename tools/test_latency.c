#include <assert.h>
#include <stdio.h>
#define SRVROS_LATENCY_HOST_TEST
#include "../kernel/src/latency.c"
static uint64_t now;
static bool ready;
uint64_t timer_monotonic_us(void) { return now; }
bool timer_reference_ready(void) { return ready; }
int main(void) {
    assert(!latency_reset());
    latency_enter(LATENCY_HEAP); now = 100; latency_leave(LATENCY_HEAP);
    assert(!measured.critical[LATENCY_HEAP].count);
    ready = true; assert(latency_reset());
    now = 0; latency_enter(LATENCY_CONSOLE);
    now = 400; latency_enter(LATENCY_CONSOLE);
    now = 2000; latency_leave(LATENCY_CONSOLE);
    now = 11000; latency_leave(LATENCY_CONSOLE);
    latency_leave(LATENCY_CONSOLE); /* unmatched end cannot underflow */
    struct latency_stats s; latency_snapshot(&s);
    assert(s.critical[LATENCY_CONSOLE].count == 1);
    assert(s.critical[LATENCY_CONSOLE].max_us == 11000);
    assert(s.critical[LATENCY_CONSOLE].over_1ms == 1 && s.critical[LATENCY_CONSOLE].over_10ms == 1);
    latency_tick(0); latency_tick(10000); latency_tick(40000);
    latency_input_poll(0); latency_input_poll(200); latency_input_poll(500);
    latency_snapshot(&s);
    assert(s.tick_gap.count == 2 && s.tick_gap.max_us == 30000 && s.tick_gap.total_us == 40000);
    assert(s.input_poll_gap.count == 2 && s.input_poll_gap.max_us == 300);
    latency_stop(); latency_tick(90000); latency_input_poll(100000);
    latency_snapshot(&s); assert(!s.enabled && s.tick_gap.count == 2);
    assert(latency_reset()); latency_tick(1000000); latency_input_poll(1000000);
    latency_snapshot(&s); assert(s.tick_gap.count == 0 && s.input_poll_gap.count == 0 && s.critical[0].count == 0);
    latency_enter(LATENCY_SITE_COUNT); latency_leave(LATENCY_SITE_COUNT);
    puts("latency-host: PASS (disabled clock, nested sections, thresholds, gaps, reset/stop)");
}
