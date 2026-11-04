#include <srvros/console.h>
#include <srvros/latency.h>
#include <srvros/bootlog.h>
#include <srvros/halt.h>
#include <srvros/fpu.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/syscall_numbers.h>
#include <srvros/vmm.h>

#include "descriptor_tables.h"
#include "irq.h"
#include "isr_frame.h"
#ifdef SRVROS_USER_FRAME_COPY_GUEST_TEST
#include "user_frame_copy.h"
#include <srvros/serial.h>
#include <srvros/timer.h>
#endif
#ifdef SRVROS_SIGNAL_CONTEXT_GUEST_TEST
#include "../../process_signal.h"
#include <srvros/serial.h>
#include <srvros/timer.h>
#endif
#if defined(SRVROS_USER_FRAME_COPY_GUEST_TEST) && defined(SRVROS_SIGNAL_CONTEXT_GUEST_TEST)
#error "Build each private diagnostic kernel separately"
#endif

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
bool syscall_io_safe(const struct isr_frame *frame);

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

#ifdef SRVROS_SYSCALL_CANCEL_HOST_TEST
uint64_t syscall_cancel_test_read_cr3(void);
#endif
static uint64_t read_cr3(void) {
#ifdef SRVROS_SYSCALL_CANCEL_HOST_TEST
    return syscall_cancel_test_read_cr3();
#else
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
#endif
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
#if defined(SRVROS_USER_FRAME_COPY_GUEST_TEST) || defined(SRVROS_SIGNAL_CONTEXT_GUEST_TEST)
    /* Disposable test kernel only: admit the private CPL3 INT3 probe. */
    idt_set_gate(3, isr_stub_table[3], IDT_GATE_USER_INTERRUPT);
#endif

    struct descriptor_pointer idtr = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)idt,
    };

    idt_load(&idtr);
}

static void dispatch_syscall_with_cancellation(struct isr_frame *frame) {
    scheduler_io_syscall(frame->rax);
    scheduler_io_set_user_safe(false);
    /* I/O service windows defer owner cancellation until the surrounding
     * syscall has released its cache/filesystem state. Do not rely on a
     * later timer tick: fast userspace can immediately issue another call
     * (including exit) while every tick lands inside an owned I/O scope. */
    if ((frame->cs & 3) == 3 && process_should_exit_current()) {
        scheduler_io_wait();
        process_exit(process_current_kill_status());
    }
    bool safe=(frame->cs & 3)==3 && syscall_io_safe(frame);
    if (!safe) {
        uint64_t start=scheduler_io_active()?latency_clock():0;
        scheduler_io_wait();
        latency_gate(start,frame->rax);
    }
    if ((frame->cs & 3)==3 && process_should_exit_current()) {
        scheduler_io_wait();
        process_exit(process_current_kill_status());
    }
    scheduler_io_set_user_safe(safe);
    syscall_dispatch(frame);
#ifndef SRVROS_SYSCALL_CANCEL_HOST_TEST
    __asm__ volatile ("cli" : : : "memory");
#endif
    scheduler_io_set_user_safe(false);
    if ((frame->cs & 3) == 3 && process_should_exit_current()) {
        scheduler_io_wait();
        process_exit(process_current_kill_status());
    }
    if ((frame->cs & 3)==3) {
        /* The syscall is fully unwound. A continuously writing process must
         * not immediately reacquire I/O ahead of an app we just woke. */
        if(!safe) {
            scheduler_io_set_user_safe(true);
            scheduler_io_handoff();
            scheduler_io_set_user_safe(false);
        }
        if(process_should_exit_current()) {
            scheduler_io_wait();
            process_exit(process_current_kill_status());
        }
    }
}

/* Only synchronous faults attributable to CPL3 instructions are contained.
 * NMIs, machine checks, double faults, and supervisor exceptions retain the
 * fatal diagnostic path. Synchronous handlers are not yet supported. */
static bool dispatch_user_fault(struct isr_frame *frame) {
    if ((frame->cs & 3) != 3) return false;
    uint64_t signal;
    switch (frame->vector) {
    case 0: case 4: case 16: case 19: signal = SRV_SIGNAL_FPE; break;
    case 6: signal = SRV_SIGNAL_ILL; break;
    case 5: case 11: case 12: case 13: case 14: case 17: case 21:
        signal = SRV_SIGNAL_SEGV; break;
    default: return false;
    }
    scheduler_io_set_user_safe(false);
    scheduler_io_wait();
    if (!process_record_user_fault(signal)) return false;
    process_exit(process_current_kill_status());
    halt_forever(); /* A faulting continuation must never be resumed. */
    return true;
}

static void isr_dispatch_inner(struct isr_frame *frame) {
    if (frame->vector == 255) {
        return;
    }

    if (frame->vector >= IRQ_VECTOR_BASE && frame->vector < IRQ_VECTOR_BASE + IRQ_COUNT) {
        irq_dispatch(frame->vector);
        return;
    }

    if (frame->vector == 0x80) {
        dispatch_syscall_with_cancellation(frame);
        return;
    }

    if (frame->vector == 3 && (frame->cs & 3) == 0) {
        fpu_selftest_trap(frame->rax);
        return;
    }

    if (dispatch_user_fault(frame)) return;

    struct process *process = process_current();
    if (frame->vector != 3) {
        console_emergency();
        console_clear();
        console_write("srvros panic\n");
    }
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

    console_write("last log lines:\n");
    bootlog_dump(2048);
    halt_forever();
}

/* Account privilege transitions independently of I/O admission: syscall-safe
 * is not a CPU mode, and blocked time belongs to no userspace process. */
#ifdef SRVROS_SYSCALL_CANCEL_HOST_TEST
void syscall_cancel_test_exclude_irqs(void);
#endif

#ifdef SRVROS_USER_FRAME_COPY_GUEST_TEST
/* Disposable test-kernel diagnostics only; no syscall/normal-build capability. */
#define USER_FRAME_COPY_TEST_MAGIC UINT64_C(0x534947434f505931)
static volatile struct {
    uint64_t complete,sequence,success,reference_ready,helper_us,mappings,record_visit_bound;
    uint64_t cr0,cr3,cr4,efer,rsp,frame,source,destination,length,io_active;
} user_frame_copy_probe;
static void user_frame_copy_probe_hex(uint64_t value) {
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<16;i++) serial_putc(digits[(value>>(60-4*i))&15]);
}
static void user_frame_copy_guest_test(struct isr_frame *frame) {
    struct user_frame_source source;
    for(size_t i=0;i<sizeof(source.bytes);i++) source.bytes[i]=(uint8_t)(0x5au^(i*13u));
    user_frame_copy_probe.complete=0;
    user_frame_copy_probe.sequence++;
    user_frame_copy_probe.reference_ready=timer_reference_ready();
    user_frame_copy_probe.mappings=UINT64_MAX;
    struct scheduler_user_return_observation obs;
    struct process_user_binding binding;
    /* Entry observation is diagnostic only; helper still uses strict return admission. */
    if(scheduler_observe_user_entry(&obs) && process_resolve_user_binding(obs.process,obs.user_thread,
        obs.image_generation,obs.address_space,&binding)) {
        uint64_t bytes=0,pid=process_pid(obs.process);
        if(process_list_details(SRV_PROC_LIST_BY_PID|pid,NULL,NULL,NULL,NULL,&bytes))
            user_frame_copy_probe.mappings=bytes/PMM_FRAME_SIZE;
    }
    user_frame_copy_probe.record_visit_bound=user_frame_copy_probe.mappings<=65536 ?
        4*user_frame_copy_probe.mappings : UINT64_MAX;
    uint64_t start=user_frame_copy_probe.reference_ready?timer_monotonic_us():0;
    bool copied=process_copy_user_frame_current(frame,&source,frame->rdi,(size_t)frame->rsi);
    user_frame_copy_probe.helper_us=user_frame_copy_probe.reference_ready?timer_monotonic_us()-start:UINT64_MAX;
    user_frame_copy_probe.success=copied;
    uint64_t cr0,cr3,cr4,rsp;uint32_t lo,hi;
    __asm__ volatile("mov %%cr0,%0; mov %%cr3,%1; mov %%cr4,%2; mov %%rsp,%3"
        : "=r"(cr0),"=r"(cr3),"=r"(cr4),"=r"(rsp));
    __asm__ volatile("rdmsr" : "=a"(lo),"=d"(hi) : "c"(0xc0000080u));
    user_frame_copy_probe.cr0=cr0;user_frame_copy_probe.cr3=cr3;user_frame_copy_probe.cr4=cr4;
    user_frame_copy_probe.efer=((uint64_t)hi<<32)|lo;user_frame_copy_probe.rsp=rsp;
    user_frame_copy_probe.frame=(uint64_t)frame;user_frame_copy_probe.source=(uint64_t)&source;
    user_frame_copy_probe.destination=frame->rdi;user_frame_copy_probe.length=frame->rsi;
    user_frame_copy_probe.io_active=scheduler_io_active();
    user_frame_copy_probe.complete=1;
    if((!copied && frame->rdx) || (frame->rdx==2 && !user_frame_copy_probe.reference_ready)) {
        serial_write("user-frame-copy-test: unexpected admission/timing failure; stable probe record; halted\n");
        for(;;) __asm__ volatile("cli; hlt" : : : "memory");
    }
    serial_write("user-frame-copy-test: ok=");user_frame_copy_probe_hex(copied);
    serial_write(" maps=");user_frame_copy_probe_hex(user_frame_copy_probe.mappings);
    serial_write(" visit_bound=");user_frame_copy_probe_hex(user_frame_copy_probe.record_visit_bound);
    serial_write(" helper_us=");user_frame_copy_probe_hex(user_frame_copy_probe.helper_us);serial_putc('\n');
    frame->rax=copied?0:(uint64_t)SRV_ERR_FAULT;
}
#endif

#ifdef SRVROS_SIGNAL_CONTEXT_GUEST_TEST
#define SIGNAL_PRIVATE_TEST_MAGIC UINT64_C(0x5349475452415031)
static volatile struct {
    uint64_t complete,sequence,operation,result,vector,elapsed_us,mappings,visit_bound;
    uint64_t cr3,frame,from_rip,from_rsp,to_rip,to_rsp,io_active,measured;
} signal_private_probe;
static void signal_private_hex(uint64_t value) {
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<16;i++) serial_putc(digits[(value>>(60-4*i))&15]);
}
static void signal_private_fault(void) __attribute__((noreturn));
static void signal_private_fault(void) {
    /* Existing contained user-fault/process teardown, outside the transaction. */
    bool recorded=process_record_user_fault(SRV_SIGNAL_SEGV);
    process_exit(recorded?process_current_kill_status():128+SRV_SIGNAL_SEGV);
}
static uint64_t signal_private_mappings(void) {
    uint64_t bytes=0,pid=process_pid(process_current());
    if(!pid || !process_list_details(SRV_PROC_LIST_BY_PID|pid,NULL,NULL,NULL,NULL,&bytes)) return UINT64_MAX;
    return bytes/PMM_FRAME_SIZE;
}
static void signal_private_common_return(struct isr_frame *frame,bool test) {
    if(process_should_exit_current()) process_exit(process_current_kill_status());
    uint64_t operation=test?frame->rdi:0;
    if(test && (operation==1 || operation==4)) {
        struct signal_action_descriptor action={.mode=SIGNAL_ACTION_INFO,
            .handler=frame->rsi,.trampoline=frame->rdx,.flags=frame->rcx,.mask=frame->r8};
        frame->rax=process_signal_action_exchange(&action,NULL,NULL)?0:(uint64_t)SRV_ERR_INVAL;
        /* Operation4 pins a RESET regression's selected current thread: no
         * user/scheduler gap between installing and testing private delivery. */
        if(operation==1 || frame->rax!=0) return;
    }
    if(test && operation==3) {
        if(frame->rsi!=1 || signal_private_mappings()!=65536 || !timer_reference_ready()) {
            frame->rax=(uint64_t)SRV_ERR_INVAL;return;
        }
        /* Readiness check only: no queued request survives this trap/lifetime. */
        frame->rax=0;return;
    }
    if(test && operation!=2 && operation!=4) {frame->rax=(uint64_t)SRV_ERR_INVAL;return;}
    /* Read-only diagnostics gathered before the complete prepare/copy/commit. */
    uint64_t mappings=signal_private_mappings();
    uint64_t cr3,vector=frame->vector;
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    uint64_t from_rip=frame->rip,from_rsp=frame->rsp;
    bool measured=!test && mappings==65536;
    bool reference=timer_reference_ready();
    uint64_t start=reference?timer_monotonic_us():0;
    uint64_t returned=PROCESS_SIGNAL_RETURN_REFUSED;
    enum process_signal_delivery_result result;
    if(test && operation==2) {
        returned=process_signal_return_private(frame,frame->rsi,frame->rdx);
        if(returned>=PROCESS_SIGNAL_RETURN_EXIT_MIN) process_exit(returned);
        result=returned==PROCESS_SIGNAL_RETURN_RESTORED?PROCESS_SIGNAL_DELIVERED:PROCESS_SIGNAL_FATAL;
    } else {
        result=process_signal_deliver_private(frame);
        if(result==PROCESS_SIGNAL_DELIVERED_EXIT_USR1) process_exit((uint64_t)result);
    }
    uint64_t elapsed=reference?timer_monotonic_us()-start:UINT64_MAX;
    if(result==PROCESS_SIGNAL_NONE || result==PROCESS_SIGNAL_DEFERRED) return;
    signal_private_probe.complete=0;signal_private_probe.sequence++;
    signal_private_probe.operation=operation;signal_private_probe.result=result;
    signal_private_probe.vector=vector;signal_private_probe.elapsed_us=elapsed;
    signal_private_probe.mappings=mappings;signal_private_probe.visit_bound=mappings<=65536?4*mappings:UINT64_MAX;
    signal_private_probe.cr3=cr3;signal_private_probe.frame=(uint64_t)frame;
    signal_private_probe.from_rip=from_rip;signal_private_probe.from_rsp=from_rsp;
    signal_private_probe.to_rip=frame->rip;signal_private_probe.to_rsp=frame->rsp;
    signal_private_probe.io_active=scheduler_io_active();signal_private_probe.measured=measured;
    signal_private_probe.complete=1;
    if(result==PROCESS_SIGNAL_FATAL) {serial_write("signal-private-test: contained transaction failure\n");signal_private_fault();}
    if(!test && measured) {
        if(!reference || mappings!=65536) signal_private_fault();
        serial_write("signal-private-test: max maps=");signal_private_hex(mappings);
        serial_write(" visits=");signal_private_hex(4*mappings);
        serial_write(" transaction_us=");signal_private_hex(elapsed);
        serial_write(" vector=");signal_private_hex(vector);serial_putc('\n');
    }
    /* Private return must skip another delivery with its consumed proof. */
}
#endif

void isr_dispatch(struct isr_frame *frame) {
    bool user=(frame->cs & 3)==3;
    if(user) {
        scheduler_account_kernel_enter();
        scheduler_io_set_user_safe(false);
    }
#ifdef SRVROS_USER_FRAME_COPY_GUEST_TEST
    bool copy_test=user && frame->vector==3 && frame->rax==USER_FRAME_COPY_TEST_MAGIC;
    if(!copy_test)
#endif
#ifdef SRVROS_SIGNAL_CONTEXT_GUEST_TEST
    bool signal_test=user && frame->vector==3 && frame->rax==SIGNAL_PRIVATE_TEST_MAGIC;
    if(!signal_test)
#endif
        isr_dispatch_inner(frame);
    if(user) {
#ifdef SRVROS_SYSCALL_CANCEL_HOST_TEST
        syscall_cancel_test_exclude_irqs();
#else
        __asm__ volatile ("cli" : : : "memory");
#endif
#ifdef SRVROS_USER_FRAME_COPY_GUEST_TEST
        if(copy_test) user_frame_copy_guest_test(frame);
#endif
#ifdef SRVROS_SIGNAL_CONTEXT_GUEST_TEST
        signal_private_common_return(frame,signal_test);
#endif
        scheduler_user_enter();
    }
}
