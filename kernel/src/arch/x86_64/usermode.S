.section .text
.global usermode_enter
.type usermode_enter, @function
usermode_enter:
    subq $48, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    call fpu_enter_user
    movq 0(%rsp), %rdi
    movq 8(%rsp), %rsi
    movq 16(%rsp), %rdx
    movq 24(%rsp), %rcx
    movq 32(%rsp), %r8
    addq $48, %rsp

    mov %rdi, %r9
    mov %rsi, %r10
    mov %rdx, %r11
    mov %rcx, %r12
    mov %r8, %r13

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
    pushq %r9
    mov %r11, %rdi
    mov %r12, %rsi
    mov %r13, %rdx
    iretq

1:
    hlt
    jmp 1b
