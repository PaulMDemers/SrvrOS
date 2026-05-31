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
