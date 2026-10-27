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
#endif
