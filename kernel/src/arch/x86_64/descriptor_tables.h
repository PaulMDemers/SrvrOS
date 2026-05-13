#ifndef SRVROS_X86_64_DESCRIPTOR_TABLES_H
#define SRVROS_X86_64_DESCRIPTOR_TABLES_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA 0x1b
#define GDT_USER_CODE 0x23
#define GDT_TSS 0x28

struct descriptor_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#endif
