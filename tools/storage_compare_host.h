#ifndef STORAGE_COMPARE_HOST_H
#define STORAGE_COMPARE_HOST_H
#include <stdint.h>
uint64_t block_test_irq_save(void);
void block_test_irq_restore(uint64_t);
uint64_t usb_test_irq_save(void);
void usb_test_irq_restore(uint64_t);
#endif
