#ifndef TEST_RESERVED_APPEND_IO_H
#define TEST_RESERVED_APPEND_IO_H
#include <stdint.h>
struct reserved_append_io {uint64_t callbacks,reads,read_bytes,writes,write_bytes,flushes;};
void reserved_append_io_snapshot(struct reserved_append_io *out);
/* Synthetic callback-clock delay for the disposable host backend only. */
void reserved_append_model_delay_us(uint64_t usec);
#endif
