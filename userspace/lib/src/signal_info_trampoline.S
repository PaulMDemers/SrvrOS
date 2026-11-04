#include <srvros/signal_syscall_numbers.h>
.text
.global __posix_signal_info_trampoline
.type __posix_signal_info_trampoline,@function
__posix_signal_info_trampoline:
    /* SysV entry RSP%16=8. Save kernel token/identity across the C helper. */
    subq $24,%rsp
    movq %r8,0(%rsp)
    movq %r9,8(%rsp)
    call __posix_signal_info_invoke
    movq 0(%rsp),%rdi
    movq 8(%rsp),%rsi
    movl $SYS_SIGNAL_RETURN,%eax
    int $0x80
    ud2
.size __posix_signal_info_trampoline,.-__posix_signal_info_trampoline
.section .note.GNU-stack,"",@progbits
