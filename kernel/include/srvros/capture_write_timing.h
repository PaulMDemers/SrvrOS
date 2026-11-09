#ifndef SRVROS_CAPTURE_WRITE_TIMING_H
#define SRVROS_CAPTURE_WRITE_TIMING_H
#include <stdint.h>
#include <stdbool.h>
#include <srvros/capture_gate_timing.h>
#define CAPTURE_WRITE_RECORDS 8u
enum capture_write_site { CAP_WRITE_CALL, CAP_WRITE_GATE, CAP_WRITE_HANDLER,
    CAP_WRITE_HANDOFF, CAP_WRITE_RESERVE, CAP_WRITE_COPY, CAP_WRITE_SITE_COUNT };
struct capture_write_sample { uint64_t count,total_us,max_us; };
struct capture_write_record {
    uint64_t pid,first_us,last_us,retired,attempts,calls,requested,bytes,failures,growths,invalid;
    struct capture_write_sample site[CAP_WRITE_SITE_COUNT];
    struct capture_gate_summary gate;
};
struct capture_write_snapshot {
    uint64_t enabled,invalid,dropped;
    struct capture_write_record records[CAPTURE_WRITE_RECORDS];
};
/* Observation only: CPU0, no allocation/logging/ownership or userspace ABI.
 * Calls measure wall time, not CPU time. Fixed records survive worker exit. */
bool process_capture_write_timing_enable(void);
uint64_t process_capture_write_clock(void);
uint64_t process_capture_write_begin(void);
void process_capture_write_record(enum capture_write_site site,uint64_t start);
void process_capture_write_result(uint64_t start,uint64_t requested,int64_t result);
void process_capture_write_growth(void);
void process_capture_write_gate_wait(void);
void process_capture_write_snapshot(struct capture_write_snapshot *out);
#endif
