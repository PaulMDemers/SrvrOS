#ifndef SRVROS_BOOTLOG_H
#define SRVROS_BOOTLOG_H

#include <stdbool.h>
#include <stdint.h>

#define BOOTLOG_DEFAULT_CAPACITY 65536u
#define BOOTLOG_CAPTURE_CAPACITY 131072u
#define BOOTLOG_LARGE_CAPTURE_CAPACITY 262144u
#define BOOTLOG_FULL_CAPTURE_CAPACITY 524288u
#define BOOTLOG_EXTENDED_CAPTURE_CAPACITY 1048576u
/* Opt-in startup capture only. Refuse if the ordinary ring already wrapped;
 * a larger buffer cannot recover discarded proof. */
bool bootlog_extend_capture(void);
/* Fixed opt-in sizes only; never recover an already discarded prefix. */
bool bootlog_extend_capture_to(uint64_t capacity);
void bootlog_putc(char c);
uint64_t bootlog_size(void);
void bootlog_dump(uint64_t max_bytes);
uint64_t bootlog_snapshot(char *out,uint64_t capacity,uint64_t *total);
uint64_t bootlog_total(void);
bool bootlog_persist(const char *path);

#endif
