#ifndef SRVROS_LATENCY_H
#define SRVROS_LATENCY_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/perf_sample.h>

enum latency_site { LATENCY_CONSOLE, LATENCY_HEAP, LATENCY_AHCI, LATENCY_SERIAL, LATENCY_PMM, LATENCY_SITE_COUNT };
struct latency_sample { uint64_t count, total_us, max_us, over_1ms, over_10ms; };
struct latency_stats {
    bool enabled;
    struct perf_sample io_owner,io_gate;
    uint64_t longest_gate_syscall,usb_commands,usb_bytes,usb_flushes;
    struct latency_sample critical[LATENCY_SITE_COUNT];
    struct latency_sample tick_gap, input_poll_gap;
};
/* These hooks must be paired while interrupts are disabled. Nested calls at
 * one site measure only the outer interval; different sites may overlap. */
uint64_t latency_clock(void);
void latency_gate(uint64_t start,uint64_t syscall);
void latency_owner_begin(void);
void latency_owner_end(void);
void latency_usb(uint64_t bytes,uint8_t opcode);
void latency_enter(enum latency_site site);
void latency_leave(enum latency_site site);
void latency_tick(uint64_t now);
void latency_input_poll(uint64_t now);
bool latency_reset(void);
void latency_stop(void);
void latency_snapshot(struct latency_stats *out);
#endif
