#ifndef SRVROS_WORKSPACE_LOG_H
#define SRVROS_WORKSPACE_LOG_H
#include <stdbool.h>
#include <stdint.h>
bool workspace_log_start(void);
/* Firmware bring-up only: retain a larger startup proof. Refuse lost prefix. */
bool workspace_log_start_capture(void);
/* Explicit full RF-table receipt capture, 256KiB; same lost-prefix refusal. */
bool workspace_log_start_large_capture(void);
/* Explicit larger RF prerequisite profile only; retain the full startup prefix. */
bool workspace_log_start_full_capture(void);
bool workspace_log_start_extended_capture(void);
/* Explicit diagnostic only; pauses logging and uses a fresh session file. */
bool workspace_storage_wait_capture(void);
bool workspace_log_stop(void);
/* Stop periodic admission and wait at most timeout_ms for its current write.
 * False retains the write and its resources; caller must keep the mount held.
 * Does not write exit.log. Call stop only after successful quiescence. */
bool workspace_log_quiesce(uint64_t timeout_ms);
bool workspace_log_active(void);
/* Drain the logger and persist a nonfinal checkpoint; stop can save exit.log
 * in the same directory after a later remount. */
bool workspace_log_pause(void);
#endif
