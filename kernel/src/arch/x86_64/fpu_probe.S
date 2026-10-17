/* Diagnostic probe: seed every SIMD register plus x87/MXCSR, cross the actual
 * IDT entry path, capture state before any C code, then restore our caller. */
.section .text
.global fpu_probe
.type fpu_probe, @function
fpu_probe:
    pushfq
    cli
    subq $512, %rsp
    fxsave64 (%rsp)
    fxrstor64 (%rdi)
    movq $0x46505554, %rax
    int3
    testq %rdx, %rdx
    jz 1f
    movq timer_irq_ticks(%rip), %rax
    movl $10000000, %ecx
    sti
2:
    pause
    cmpq timer_irq_ticks(%rip), %rax
    jne 1f
    decl %ecx
    jnz 2b
    cli
    xorl %eax, %eax
    jmp 3f
1:
    cli
    movl $1, %eax
3:
    fxsave64 (%rsi)
    fxrstor64 (%rsp)
    addq $512, %rsp
    popfq
    ret
.section .note.GNU-stack,"",@progbits
