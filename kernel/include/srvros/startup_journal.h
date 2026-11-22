#ifndef SRVROS_STARTUP_JOURNAL_H
#define SRVROS_STARTUP_JOURNAL_H
#include <stdbool.h>
#include <stdint.h>

/* Explicit diagnostic only. The immutable module binds one preallocated EFI
 * file by disk geometry, GPT/BPB hashes, physical extent and fresh nonce. */
bool startup_journal_open(void);
bool startup_journal_checkpoint(uint32_t phase, uint64_t flags);
bool startup_journal_close(void);
#endif
