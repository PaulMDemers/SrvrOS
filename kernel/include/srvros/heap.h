#ifndef SRVROS_HEAP_H
#define SRVROS_HEAP_H

#include <stddef.h>
#include <stdint.h>

void heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void kfree(void *ptr);
uint64_t heap_total_bytes(void);
uint64_t heap_free_bytes(void);
uint64_t heap_used_bytes(void);

#endif

