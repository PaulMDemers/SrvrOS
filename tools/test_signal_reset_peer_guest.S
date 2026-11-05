.section .text
.globl reset_peer_trampoline
reset_peer_trampoline:
    subq $40,%rsp
    movq %r8,0(%rsp)
    movq %r9,8(%rsp)
    call *%rcx
    movq 0(%rsp),%rsi
    movq 8(%rsp),%rdx
    movq $2,%rdi
    movabsq $0x5349475452415031,%rax
    int3
    ud2
