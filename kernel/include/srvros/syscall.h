#ifndef SRVROS_SYSCALL_H
#define SRVROS_SYSCALL_H
#include <stdbool.h>
struct isr_frame;
/* Opt-in raw-entry scalar observation only, before any cancellation/gate. */
void syscall_gate_trace(const struct isr_frame *frame);
/* CPU0, same raw entry with IRQs excluded. The candidate is copied metadata
 * only; IDT retains its result on the kernel stack until dispatch. Typed inner
 * backends and ordinary syscall_io_safe do not inherit this authority. */
bool syscall_reserved_append_candidate(const struct isr_frame *frame);
/* Called only for that retained candidate. True is an atomic handled result,
 * including a user fault. False has completed ordinary gated fallback; IDT
 * must retain its original ordinary handoff. Unexpected decline invalidates
 * acceptance diagnostics rather than fabricating an owner-free gate. */
bool syscall_reserved_append_dispatch(struct isr_frame *frame);
#endif
