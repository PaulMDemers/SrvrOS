#ifndef SRVROS_BOOTLOG_H
#define SRVROS_BOOTLOG_H

#include <stdbool.h>
#include <stdint.h>

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
