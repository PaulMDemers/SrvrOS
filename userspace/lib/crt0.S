.section .text
.global _start
.type _start, @function
_start:
    xor %rbp, %rbp
    sub $32, %rsp
    mov %rdi, 0(%rsp)
    mov %rsi, 8(%rsp)
    mov %rdx, 16(%rsp)
    test %rdx, %rdx
    je 1f
    mov %rdx, environ(%rip)
1:
    call __srvros_run_init_array
    mov 0(%rsp), %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    call main

    mov %rax, 24(%rsp)
    call __srvros_run_fini_array
    mov 24(%rsp), %rdi
    call exit

1:
    hlt
    jmp 1b

.type __srvros_run_init_array, @function
__srvros_run_init_array:
    push %rbx
    push %rbp
    push %r12
    lea __init_array_start(%rip), %rbx
    lea __init_array_end(%rip), %rbp
1:
    cmp %rbp, %rbx
    jae 2f
    call *(%rbx)
    add $8, %rbx
    jmp 1b
2:
    pop %r12
    pop %rbp
    pop %rbx
    ret

.type __srvros_run_fini_array, @function
__srvros_run_fini_array:
    push %rbx
    push %rbp
    push %r12
    lea __fini_array_end(%rip), %rbx
    lea __fini_array_start(%rip), %rbp
1:
    cmp %rbp, %rbx
    jbe 2f
    sub $8, %rbx
    call *(%rbx)
    jmp 1b
2:
    pop %r12
    pop %rbp
    pop %rbx
    ret
