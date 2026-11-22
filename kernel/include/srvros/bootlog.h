#ifndef SRVROS_BOOTLOG_H
#define SRVROS_BOOTLOG_H

#include <stdbool.h>
#include <stdint.h>

/* Only dedicated joined-provider and full-GT images select this provenance tail.
 * Ordinary images and older diagnostic payloads retain their64KiB budget. */
#if defined(SRVROS_INTEL_COMMAND_OWNERSHIP) || defined(SRVROS_INTEL_GT_BOOTSTRAP_NATIVE)
#define SRVROS_BOOTLOG_CAPACITY 98304u
#else
#define SRVROS_BOOTLOG_CAPACITY 65536u
#endif

void bootlog_putc(char c);
uint64_t bootlog_size(void);
void bootlog_dump(uint64_t max_bytes);
uint64_t bootlog_snapshot(char *out,uint64_t capacity,uint64_t *total);
/* Same bounded chronological tail, dropping only an incomplete leading line.
 * Exact line boundaries preserve their first full record; total stays raw. */
uint64_t bootlog_snapshot_lines(char *out,uint64_t capacity,uint64_t *total);
uint64_t bootlog_total(void);
bool bootlog_persist(const char *path);

#endif
