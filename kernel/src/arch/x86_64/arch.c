#include <srvros/arch.h>
#include <srvros/console.h>

#include "irq.h"

void gdt_init(void);
void idt_init(void);
bool fpu_init(void);

void arch_init(void) {
    __asm__ volatile ("cli");

    gdt_init();
    console_write("gdt: loaded\n");

    if (!fpu_init()) {
        console_write("fpu: continuing without userspace SSE support\n");
    }

    idt_init();
    console_write("idt: loaded\n");

    irq_init();
    console_write("irq: pic masked\n");
}

void arch_enable_interrupts(void) {
    console_write("irq: enabling interrupts\n");
    __asm__ volatile ("sti");
}

void arch_test_breakpoint(void) {
    console_write("idt: testing breakpoint exception\n");
    __asm__ volatile ("int3");
}
