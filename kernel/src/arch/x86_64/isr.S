.section .text

.macro ISR_NOERR vector
.global isr\vector
.type isr\vector, @function
isr\vector:
    pushq $0
    pushq $\vector
    jmp isr_common
.endm

.macro ISR_ERR vector
.global isr\vector
.type isr\vector, @function
isr\vector:
    pushq $\vector
    jmp isr_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_NOERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR 29
ISR_ERR 30
ISR_NOERR 31
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47
ISR_NOERR 128

.global isr_spurious
.type isr_spurious, @function
isr_spurious:
    pushq $0
    pushq $255
    jmp isr_common

.global irq0_legacy_stub
.type irq0_legacy_stub, @function
irq0_legacy_stub:
    pushq %rax
    pushq %rdx

    incq timer_irq_ticks(%rip)
    cmpq $5, timer_irq_ticks(%rip)
    jb 1f
    movb $0xfd, %al
    outb %al, $0x21
    andq $~0x200, 32(%rsp)
1:
    movb $0x20, %al
    outb %al, $0x20

    popq %rdx
    popq %rax
    iretq

.macro IRQ_DIRECT name target
.global \name
.type \name, @function
\name:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq 128(%rsp), %rdi
    movq %rsp, %rax
    andq $-16, %rsp
    subq $16, %rsp
    movq %rax, (%rsp)
    call fpu_trap_enter
    movq (%rsp), %rsp

    movq %rsp, %rax
    andq $-16, %rsp
    subq $16, %rsp
    movq %rax, (%rsp)
    call \target
    movq (%rsp), %rsp

    movq 128(%rsp), %rdi
    movq %rsp, %rax
    andq $-16, %rsp
    subq $16, %rsp
    movq %rax, (%rsp)
    call fpu_trap_exit
    movq (%rsp), %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq
.endm

IRQ_DIRECT irq1_legacy_stub irq1_legacy_dispatch

.global isr_common
.type isr_common, @function
isr_common:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq 144(%rsp), %rdi
    movq %rsp, %rax
    andq $-16, %rsp
    subq $16, %rsp
    movq %rax, (%rsp)
    call fpu_trap_enter
    movq (%rsp), %rsp

    movq %rsp, %rdi
    movq %rsp, %rax
    andq $-16, %rsp
    subq $16, %rsp
    movq %rax, (%rsp)
    call isr_dispatch
    movq (%rsp), %rsp

    movq 144(%rsp), %rdi
    movq %rsp, %rax
    andq $-16, %rsp
    subq $16, %rsp
    movq %rax, (%rsp)
    call fpu_trap_exit
    movq (%rsp), %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp
    iretq

.section .rodata
.global isr_stub_table
isr_stub_table:
    .quad isr0
    .quad isr1
    .quad isr2
    .quad isr3
    .quad isr4
    .quad isr5
    .quad isr6
    .quad isr7
    .quad isr8
    .quad isr9
    .quad isr10
    .quad isr11
    .quad isr12
    .quad isr13
    .quad isr14
    .quad isr15
    .quad isr16
    .quad isr17
    .quad isr18
    .quad isr19
    .quad isr20
    .quad isr21
    .quad isr22
    .quad isr23
    .quad isr24
    .quad isr25
    .quad isr26
    .quad isr27
    .quad isr28
    .quad isr29
    .quad isr30
    .quad isr31
    .quad isr32
    .quad isr33
    .quad isr34
    .quad isr35
    .quad isr36
    .quad isr37
    .quad isr38
    .quad isr39
    .quad isr40
    .quad isr41
    .quad isr42
    .quad isr43
    .quad isr44
    .quad isr45
    .quad isr46
    .quad isr47
