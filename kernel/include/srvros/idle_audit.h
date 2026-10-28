#ifndef SRVROS_IDLE_AUDIT_H
#define SRVROS_IDLE_AUDIT_H
#include <stdbool.h>
#include <stdint.h>
/* CPU0 scheduler HLT path only; attempts/returns are not hardware residency
 * or time asleep. A pending return may cross stop/restart; epoch guards it. */
struct scheduler_idle_audit { uint64_t epoch, enters, returns, ready; };
bool scheduler_idle_audit_start(void);
void scheduler_idle_audit_snapshot(struct scheduler_idle_audit *);
void scheduler_idle_audit_stop(struct scheduler_idle_audit *);
/* Explicit capture-only bounded wakeup experiment, never ordinary idle policy.
 * Latency is the masked wait interval, not IRQ or scheduler dispatch latency. */
#define SCHEDULER_MWAIT_ATTEMPTS 32u
#define SCHEDULER_MWAIT_LIMIT_US 20000u
struct scheduler_mwait_test {
    uint64_t epoch, attempts, returns, max_us, fault, hlt, rechecks;
    uint64_t tick_begin, tick_end;
    uint64_t hint, tsc_ticks, core_c3_ticks, package_c3_ticks;
};
bool scheduler_mwait_test_start(uint32_t hint);
void scheduler_mwait_test_stop(struct scheduler_mwait_test *);
#endif
