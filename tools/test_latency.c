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
    now=100; latency_owner_begin(); now=50100; latency_owner_end();
    latency_gate(100,42); latency_usb(4096,0x2a); latency_usb(0,0x35);
    latency_snapshot(&s);
    assert(s.io_owner.count==1 && s.io_owner.max==50000);
    assert(s.io_gate.count==1 && s.io_gate.max==50000 && s.longest_gate_syscall==42);
    assert(s.usb_commands==2 && s.usb_bytes==4096 && s.usb_flushes==1);
    struct perf_sample histogram={0};
    for(unsigned i=0;i<95;i++) perf_record(&histogram,50000);
    for(unsigned i=0;i<4;i++) perf_record(&histogram,100000);
    perf_record(&histogram,250001);
    assert(perf_percentile(&histogram,95)==50000 && perf_percentile(&histogram,99)==100000 && histogram.max==250001);
    now=100;uint64_t work_start=latency_clock();now=20100;
    latency_work_record(LATENCY_PROCESS_IMAGE,work_start,true);latency_snapshot(&s);
    assert(s.work[LATENCY_PROCESS_IMAGE].count==1 && s.work[LATENCY_PROCESS_IMAGE].max_us==20000);
    assert(s.work_entry_masked[LATENCY_PROCESS_IMAGE]==1);
    latency_work_record(LATENCY_WORK_COUNT,work_start,false);latency_work_record(LATENCY_PROCESS_IMAGE,0,false);
    latency_stop();now=30000;latency_work_record(LATENCY_PROCESS_IMAGE,work_start,true);latency_snapshot(&s);
    assert(s.work[LATENCY_PROCESS_IMAGE].count==1);
    assert(latency_reset());latency_snapshot(&s);assert(!s.work[LATENCY_PROCESS_IMAGE].count && !s.work_entry_masked[LATENCY_PROCESS_IMAGE]);
    puts("latency-host: PASS (disabled clock, nested sections, thresholds, gaps, reset/stop)");
}
