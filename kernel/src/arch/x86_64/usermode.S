.section .text
.global usermode_enter
.type usermode_enter, @function
usermode_enter:
    subq $56, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    movq %r9, 40(%rsp)
    call scheduler_user_enter
    call fpu_enter_user
    movq 0(%rsp), %rdi
    movq 8(%rsp), %rsi
    movq 16(%rsp), %rdx
    movq 24(%rsp), %rcx
    movq 32(%rsp), %r8
    movq 40(%rsp), %r9
    addq $56, %rsp

    mov %rdi, %r14
    mov %rsi, %r10
    mov %rdx, %r11
    mov %rcx, %r12
    mov %r8, %r13

    mov %r9, %rax
    mov %rax, %rdx
    shrq $32, %rdx
    mov $0xC0000100, %ecx
    wrmsr

    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es

    pushq $0x1b
    pushq %r10
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %r14
    mov %r11, %rdi
    mov %r12, %rsi
    mov %r13, %rdx
    iretq

1:
    hlt
    jmp 1b

/* Resume the exact post-int-0x80 continuation in a newly cloned process.
 * isr_frame is 22 qwords: 15 registers, vector/error, and the five iret words. */
.global usermode_resume
.type usermode_resume, @function
usermode_resume:
    subq $184, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    call scheduler_user_enter
    call fpu_enter_user
    movq 8(%rsp), %rax
    movq %rax, %rdx
    shrq $32, %rdx
    movl $0xC0000100, %ecx
    wrmsr
    movw $0x1b, %ax
    movw %ax, %ds
    movw %ax, %es
    movq 0(%rsp), %rsi
    movq %rsp, %rdi
    movq $22, %rcx
    cld
    rep movsq
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
.size usermode_resume, .-usermode_resume
