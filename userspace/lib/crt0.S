.section .text
.global _start
.type _start, @function
_start:
    xor %rbp, %rbp
    call main

    mov %rax, %rdi
    mov $2, %rax
    int $0x80

1:
    hlt
    jmp 1b
