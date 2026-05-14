.section .text
.global usermode_enter
.type usermode_enter, @function
usermode_enter:
    subq $40, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    call fpu_enter_user
    movq 0(%rsp), %rdi
    movq 8(%rsp), %rsi
    movq 16(%rsp), %rdx
    movq 24(%rsp), %rcx
    addq $40, %rsp

    mov %rdi, %r8
    mov %rsi, %r9
    mov %rdx, %r10
    mov %rcx, %r11

    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es

    pushq $0x1b
    pushq %r9
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %r8
    mov %r10, %rdi
    mov %r11, %rsi
    iretq

1:
    hlt
    jmp 1b
