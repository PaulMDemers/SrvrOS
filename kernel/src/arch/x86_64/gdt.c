#include "descriptor_tables.h"

#include <stdint.h>

extern void gdt_load(const struct descriptor_pointer *gdtr);
extern void tss_load(uint16_t selector);

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static uint64_t gdt[7] = {
    0x0000000000000000,
    0x00af9a000000ffff,
    0x00af92000000ffff,
    0x00aff2000000ffff,
    0x00affa000000ffff,
    0,
    0,
};

static struct tss kernel_tss;
static uint8_t kernel_ring0_stack[16384] __attribute__((aligned(16)));

static void gdt_set_tss(uint16_t index, uint64_t base, uint32_t limit) {
    gdt[index] =
        ((uint64_t)limit & 0xffff) |
        ((base & 0xffffff) << 16) |
        ((uint64_t)0x89 << 40) |
        (((uint64_t)limit & 0xf0000) << 32) |
        ((base & 0xff000000ull) << 32);
    gdt[index + 1] = base >> 32;
}

void gdt_init(void) {
    kernel_tss.rsp0 = (uint64_t)kernel_ring0_stack + sizeof(kernel_ring0_stack);
    kernel_tss.iomap_base = sizeof(kernel_tss);
    gdt_set_tss(5, (uint64_t)&kernel_tss, sizeof(kernel_tss) - 1);

    struct descriptor_pointer gdtr = {
        .limit = sizeof(gdt) - 1,
        .base = (uint64_t)gdt,
    };

    gdt_load(&gdtr);
    tss_load(GDT_TSS);
}

uint64_t gdt_default_kernel_stack_top(void) {
    return (uint64_t)kernel_ring0_stack + sizeof(kernel_ring0_stack);
}

void gdt_set_kernel_stack(uint64_t stack_top) {
    kernel_tss.rsp0 = stack_top;
}
