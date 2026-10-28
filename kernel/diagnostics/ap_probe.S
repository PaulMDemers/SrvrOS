/* AP-only code. Never calls C, allocator, scheduler or BSP interrupt handlers.
 * r12 permanently owns a static slot. All descriptors/stacks remain until reset. */
.text
.macro read_msr reg,target
    mov $\reg, %ecx
    rdmsr
    shl $32, %rdx
    or %rdx, %rax
    mov %rax, \target
.endm
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
    xor %r15d, %r15d
    cmpq $0, 168(%r12)
    je .caps_done
    mov $5, %eax
    xor %ecx, %ecx
    cpuid
    shr $8, %edx
    and $15, %edx
    mov %rdx, 240(%r12)
.caps_done:
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
    cmp $32, %r13
    jae .deep_command
    lea 1(%r13), %rax
    cmp %rax, %r14
    jne .bad
    cmp $32, %r14
    ja .bad
    mov %r14, %r13
    mov %r14, 64(%r12)
    jmp .loop
.deep_command:
    cmpq $0, 168(%r12)
    je .bad
    cmp $32, %r15
    jae .bad
    lea 257(%r15), %rax
    cmp %rax, %r14
    jne .bad
    inc %r15
    mov %r14, %r13
    cmpq $3, 120(%r12)
    je .missing
    cmpq $5, 120(%r12)
    jne .deep_caps
    ud2
.deep_caps:
    cmpq $2, 168(%r12)
    je .sim_arm
    cmpq $0, 240(%r12)
    je .bad
    read_msr 0xe2,%r8
    mov %r8, 224(%r12)
    read_msr 0x10,%r9
    read_msr 0x3fc,%r10
    mov %r12, %rax
    xor %ecx, %ecx
    xor %edx, %edx
    monitor
    cmp %r13, 0(%r12)
    jne .deep_canceled
    mov %r15, 176(%r12)
    cmp %r13, 0(%r12)
    jne .deep_canceled
    incq 192(%r12)
    mov $16, %eax
    xor %ecx, %ecx
    mwait
    read_msr 0x3fc,%r11
    read_msr 0x10,%r8
    cmp %r9, %r8
    jbe .counter_fault
    cmp %r10, %r11
    jb .counter_fault
    sub %r9, %r8
    sub %r10, %r11
    cmp %r8, %r11
    ja .counter_fault
    mov %r8, 208(%r12)
    mov %r11, 216(%r12)
    read_msr 0xe2,%r8
    mov %r8, 232(%r12)
    jmp .deep_return
.sim_arm:
    /* VM protocol rehearsal only; never executes C3 or reads residency MSRs. */
    mov %r15, 176(%r12)
    incq 192(%r12)
    cmpq $4, 120(%r12)
    je .early
.sim_wait:
    cmp %r13, 0(%r12)
    jne .deep_return
    pause
    jmp .sim_wait
.deep_return:
    cmp %r13, 0(%r12)
    jne .returned
.early:
    mov %r15, 200(%r12)
.returned:
    mov %r15, 184(%r12)
.await_resume:
    mov 0(%r12), %rax
    cmp $-1, %rax
    je .retire
    cmp %r13, %rax
    jne .resume
    pause
    jmp .await_resume
.resume:
    lea 512(%r15), %rdx
    cmp %rdx, %rax
    jne .bad
    mov %rax, %r13
    mov %rax, 64(%r12)
    jmp .loop
.deep_canceled:
    cmpq $-1, 0(%r12)
    je .retire
    jmp .bad
.counter_fault:
    movq $3, 80(%r12)
    jmp .fault_park
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
