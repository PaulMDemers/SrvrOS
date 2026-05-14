.section .text
.global _start
.type _start, @function
_start:
    xor %rbp, %rbp
    test %rdx, %rdx
    je 1f
    mov %rdx, environ(%rip)
1:
    call main

    mov %rax, %rdi
    mov $2, %rax
    int $0x80

1:
    hlt
    jmp 1b
