/* Copied to a single owned page in real-mode RAM. The caller fills the
 * segment and physical pointers before publishing the legacy FACS vector. */
.section .rodata.s3_wake,"a",@progbits
.code16
.global s3_wake_start, s3_wake_end, s3_wake_segment, s3_wake_gdt, s3_wake_gdt_base
.global s3_wake_cr3, s3_wake_efer, s3_wake_cr0, s3_wake_flat, s3_wake_long, s3_wake_return
s3_wake_start:
    .byte 0xea
    .word .Lreal - s3_wake_start
s3_wake_segment:
    .word 0
.Lreal:
    cli
    cld
    lgdtl %cs:(.Lgdtr - s3_wake_start)
    movl $0x20, %eax /* PAE with a four-level root. */
    movl %eax, %cr4
    movl %cs:(s3_wake_cr3 - s3_wake_start), %eax
    movl %eax, %cr3
    movl $0xc0000080, %ecx
    movl %cs:(s3_wake_efer - s3_wake_start), %eax
    xorl %edx, %edx
    wrmsr
    movl %cs:(s3_wake_cr0 - s3_wake_start), %eax
    movl %eax, %cr0
    .byte 0x66, 0xea
s3_wake_flat:
    .long 0
    .word 0x08
.code64
s3_wake_long:
    movw $0x10, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    jmp *s3_wake_return(%rip)
.balign 8
s3_wake_return:
    .quad 0
s3_wake_cr3:
    .long 0
s3_wake_efer:
    .long 0
s3_wake_cr0:
    .long 0
.balign 8
s3_wake_gdt:
    /* Preset Accessed because the identity mapping is read-only. */
    .quad 0, 0x00209b0000000000, 0x00cf93000000ffff
.Lgdtr:
    .word .Lgdtr - s3_wake_gdt - 1
s3_wake_gdt_base:
    .long 0
s3_wake_end:

.bss
.balign 8
.Lreturn_stack: .quad 0
.Lreturn_cr4: .quad 0
.Lreturn_gdtr: .skip 10
.Lreturn_idtr: .skip 10

.text
.global s3_fixture_enter, s3_fixture_resume
.type s3_fixture_enter, @function
s3_fixture_enter:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, .Lreturn_stack(%rip)
    movq %cr4, %rax
    movq %rax, .Lreturn_cr4(%rip)
    sgdt .Lreturn_gdtr(%rip)
    sidt .Lreturn_idtr(%rip)
    wbinvd
    movl %edi, %edx
    movl %esi, %eax
    outw %ax, %dx
    xorl %eax, %eax
    jmp .Lunwind
.size s3_fixture_enter, . - s3_fixture_enter

.type s3_fixture_resume, @function
s3_fixture_resume:
    movq .Lreturn_stack(%rip), %rsp
    movq .Lreturn_cr4(%rip), %rax
    movq %rax, %cr4
    lgdt .Lreturn_gdtr(%rip)
    lidt .Lreturn_idtr(%rip)
    movl $1, %eax
.Lunwind:
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
.size s3_fixture_resume, . - s3_fixture_resume
