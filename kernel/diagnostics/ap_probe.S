/* AP-only code. Never calls C, allocator, scheduler or BSP interrupt handlers.
 * r12 permanently owns a static slot. All descriptors/stacks remain until reset. */
.text
.global ap_probe_entry
.type ap_probe_entry,@function
ap_probe_entry:
    cli
    cld
    mov 24(%rdi), %r12
    mov 160(%r12), %rsp
    xor %rbp, %rbp
    lgdt 128(%r12)
    pushq $8
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    mov $16, %eax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    mov $24, %eax
    ltr %ax
    lidt 144(%r12)
    mov $1, %eax
    xor %ecx, %ecx
    cpuid
    mov %eax, %eax
    mov %rax, 96(%r12)
    shr $24, %ebx
    mov %rbx, 88(%r12)
    xor %r13d, %r13d
    /* VM-only injections are set by BSP after verifying CPUID hypervisor. */
    cmpq $1, 120(%r12)
    je .missing
    cmpq $2, 120(%r12)
    jne .ready
    ud2
.ready:
    movq $1, 72(%r12)
.loop:
    mov 0(%r12), %r14
    cmp $-1, %r14
    je .retire
    cmp %r13, %r14
    jne .command
    cmpq $0, 104(%r12)
    je .pause
    mov %r12, %rax
    xor %ecx, %ecx
    xor %edx, %edx
    monitor
    mov 0(%r12), %r14
    cmp %r13, %r14
    jne .loop
    incq 112(%r12)
    xor %eax, %eax
    xor %ecx, %ecx
    mwait /* C1 only; monitored BSP store is the wake source. IF remains clear. */
    jmp .loop
.pause:
    pause
    jmp .loop
.command:
    lea 1(%r13), %rax
    cmp %rax, %r14
    jne .bad
    cmp $32, %r14
    ja .bad
    mov %r14, %r13
    mov %r14, 64(%r12)
    jmp .loop
.missing:
    /* Remain cancellable even when simulating missing startup acknowledgement. */
    cmpq $-1, 0(%r12)
    je .retire
    pause
    jmp .missing
.bad:
    movq $2, 80(%r12)
    jmp .fault_park
.retire:
    movq $-1, 64(%r12)
    movq $2, 72(%r12)
.halt:
    cli
    hlt
    jmp .halt
.global ap_probe_fault
.type ap_probe_fault,@function
ap_probe_fault:
    /* Every vector terminates on this AP's dedicated IST stack; no IRET or EOI.
     * A faulted AP is never reused, and all storage remains pinned. */
    cli
    movq $1, 80(%r12)
.fault_park:
    movq $3, 72(%r12)
    jmp .halt
.section .note.GNU-stack,"",@progbits
