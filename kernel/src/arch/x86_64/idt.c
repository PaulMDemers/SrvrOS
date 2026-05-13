#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/vmm.h>

#include "descriptor_tables.h"
#include "irq.h"
#include "isr_frame.h"

#include <stdint.h>

#define IDT_ENTRY_COUNT 256
#define IDT_GATE_INTERRUPT 0x8e
#define IDT_GATE_USER_INTERRUPT 0xee
#define KERNEL_CODE_SELECTOR 0x08

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

extern void (*isr_stub_table[])(void);
extern void isr_spurious(void);
extern void isr128(void);
extern void irq0_legacy_stub(void);
extern void irq1_legacy_stub(void);
void syscall_dispatch(struct isr_frame *frame);

static struct idt_entry idt[IDT_ENTRY_COUNT];

static void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t flags) {
    uint64_t address = (uint64_t)handler;

    idt[vector] = (struct idt_entry) {
        .offset_low = address & 0xffff,
        .selector = KERNEL_CODE_SELECTOR,
        .ist = 0,
        .flags = flags,
        .offset_mid = (address >> 16) & 0xffff,
        .offset_high = (address >> 32) & 0xffffffff,
        .zero = 0,
    };
}

static const char *exception_name(uint64_t vector) {
    static const char *names[] = {
        "divide error",
        "debug",
        "nmi",
        "breakpoint",
        "overflow",
        "bound range exceeded",
        "invalid opcode",
        "device not available",
        "double fault",
        "coprocessor segment overrun",
        "invalid tss",
        "segment not present",
        "stack segment fault",
        "general protection fault",
        "page fault",
        "reserved",
        "x87 floating-point exception",
        "alignment check",
        "machine check",
        "simd floating-point exception",
        "virtualization exception",
        "control protection exception",
    };

    if (vector < sizeof(names) / sizeof(names[0])) {
        return names[vector];
    }
    return "reserved";
}

static void idt_load(const struct descriptor_pointer *idtr) {
    __asm__ volatile ("lidt (%0)" : : "r"(idtr) : "memory");
}

static uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static void print_mapping(const char *label, uint64_t address) {
    uint64_t physical = 0;
    if (vmm_virt_to_phys(address, &physical)) {
        console_printf("  %s=%x phys=%x\n", label, address, physical);
    } else {
        console_printf("  %s=%x unmapped\n", label, address);
    }
}

static void print_bytes(uint64_t address, uint64_t count) {
    uint64_t physical = 0;
    if (!vmm_virt_to_phys(address, &physical)) {
        console_printf("  bytes@%x: unmapped\n", address);
        return;
    }

    const uint8_t *bytes = (const uint8_t *)pmm_phys_to_virt(physical);
    console_printf("  bytes@%x:", address);
    for (uint64_t i = 0; i < count; i++) {
        console_printf(" %x", (uint64_t)bytes[i]);
    }
    console_write("\n");
}

static void print_user_stack(uint64_t rsp) {
    if ((rsp >> 47) != 0 || !vmm_user_range_mapped(rsp, sizeof(uint64_t), false)) {
        console_printf("  stack@%x: unavailable\n", rsp);
        return;
    }

    console_printf("  stack@%x:", rsp);
    const uint64_t *words = (const uint64_t *)rsp;
    for (uint64_t i = 0; i < 6; i++) {
        if (!vmm_user_range_mapped((uint64_t)&words[i], sizeof(words[i]), false)) {
            console_write(" ?");
            break;
        }
        console_printf(" %x", words[i]);
    }
    console_write("\n");
}

void idt_init(void) {
    for (uint64_t i = 0; i < IDT_ENTRY_COUNT; i++) {
        idt_set_gate((uint8_t)i, isr_spurious, IDT_GATE_INTERRUPT);
    }

    for (uint64_t i = 0; i < 48; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], IDT_GATE_INTERRUPT);
    }
    idt_set_gate(8, irq0_legacy_stub, IDT_GATE_INTERRUPT);
    idt_set_gate(9, irq1_legacy_stub, IDT_GATE_INTERRUPT);
    idt_set_gate(0x80, isr128, IDT_GATE_USER_INTERRUPT);

    struct descriptor_pointer idtr = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)idt,
    };

    idt_load(&idtr);
}

void isr_dispatch(struct isr_frame *frame) {
    if (frame->vector == 255) {
        return;
    }

    if (frame->vector >= IRQ_VECTOR_BASE && frame->vector < IRQ_VECTOR_BASE + IRQ_COUNT) {
        irq_dispatch(frame->vector);
        return;
    }

    if (frame->vector == 0x80) {
        syscall_dispatch(frame);
        return;
    }

    if (frame->vector == 3 && (frame->cs & 3) == 0) {
        return;
    }

    struct process *process = process_current();
    console_printf("exception: vector=%u (%s) error=%x rip=%x rsp=%x rbp=%x cs=%x ss=%x rflags=%x cr2=%x cr3=%x pid=%u name=%s\n",
        frame->vector,
        exception_name(frame->vector),
        frame->error_code,
        frame->rip,
        frame->rsp,
        frame->rbp,
        frame->cs,
        frame->ss,
        frame->rflags,
        frame->vector == 14 ? read_cr2() : 0,
        read_cr3(),
        process_pid(process),
        process_name(process));
    console_printf("  regs: rax=%x rbx=%x rcx=%x rdx=%x rsi=%x rdi=%x r8=%x r9=%x r10=%x r11=%x r12=%x r13=%x r14=%x r15=%x\n",
        frame->rax,
        frame->rbx,
        frame->rcx,
        frame->rdx,
        frame->rsi,
        frame->rdi,
        frame->r8,
        frame->r9,
        frame->r10,
        frame->r11,
        frame->r12,
        frame->r13,
        frame->r14,
        frame->r15);

    if ((frame->cs & 3) == 3) {
        print_mapping("rip", frame->rip);
        print_mapping("rsp", frame->rsp);
        print_bytes(frame->rip, 12);
        print_user_stack(frame->rsp);
    }

    if (frame->vector == 3) {
        return;
    }

    halt_forever();
}
