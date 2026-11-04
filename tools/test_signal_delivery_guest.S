/* Private test trampoline and uninterrupted register probe. */
.text
.globl test_probe
test_probe:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    /* Use an existing mapped stack page at +0x300 so the complete760-byte
     * signal payload crosses a page boundary, including the max-table run. */
    movq %rsp,test_caller_rsp(%rip)
    andq $-4096,%rsp
    subq $4096,%rsp
    addq $0x300,%rsp
    fxsave64 test_fx_saved(%rip)
    movq %rsp,test_probe_rsp(%rip)
    movq %fs:0,%rax
    movq %rax,test_fs_before(%rip)
    movabsq $0x6162636465666768,%rax
    movq %rax,-8(%rsp)
    movq %rax,-16(%rsp)
    movq %rax,-24(%rsp)
    movq %rax,-32(%rsp)
    movq %rax,-40(%rsp)
    movq %rax,-48(%rsp)
    movq %rax,-56(%rsp)
    movq %rax,-64(%rsp)
    movq %rax,-72(%rsp)
    movq %rax,-80(%rsp)
    movq %rax,-88(%rsp)
    movq %rax,-96(%rsp)
    movq %rax,-104(%rsp)
    movq %rax,-112(%rsp)
    movq %rax,-120(%rsp)
    movq %rax,-128(%rsp)
    fninit
    fld1
    fldpi
    fldcw test_cw(%rip)
    ldmxcsr test_mxcsr(%rip)
    movdqu test_xmm_pattern+0(%rip),%xmm0
    movdqu test_xmm_pattern+16(%rip),%xmm1
    movdqu test_xmm_pattern+32(%rip),%xmm2
    movdqu test_xmm_pattern+48(%rip),%xmm3
    movdqu test_xmm_pattern+64(%rip),%xmm4
    movdqu test_xmm_pattern+80(%rip),%xmm5
    movdqu test_xmm_pattern+96(%rip),%xmm6
    movdqu test_xmm_pattern+112(%rip),%xmm7
    movdqu test_xmm_pattern+128(%rip),%xmm8
    movdqu test_xmm_pattern+144(%rip),%xmm9
    movdqu test_xmm_pattern+160(%rip),%xmm10
    movdqu test_xmm_pattern+176(%rip),%xmm11
    movdqu test_xmm_pattern+192(%rip),%xmm12
    movdqu test_xmm_pattern+208(%rip),%xmm13
    movdqu test_xmm_pattern+224(%rip),%xmm14
    movdqu test_xmm_pattern+240(%rip),%xmm15
    fxsave64 test_fx_before(%rip)
    movq $1,%rax
    movq $2,%rbx
    movq $3,%rcx
    movq $4,%rdx
    movq $5,%rsi
    movq $6,%rdi
    movq $7,%rbp
    movq $8,%r8
    movq $9,%r9
    movq $10,%r10
    movq $11,%r11
    movq $12,%r12
    movq $13,%r13
    movq $14,%r14
    movq test_expected_pointer(%rip),%r15
    std
.globl test_loop_begin
test_loop_begin:
    movl $1,test_ready(%rip)
1:
    cmpl $0,test_done(%rip)
    je 1b
.globl test_loop_end
test_loop_end:
    movq %rsp,test_probe_rsp_after(%rip)
    movq %rax,test_registers+0(%rip)
    movq %rbx,test_registers+8(%rip)
    movq %rcx,test_registers+16(%rip)
    movq %rdx,test_registers+24(%rip)
    movq %rsi,test_registers+32(%rip)
    movq %rdi,test_registers+40(%rip)
    movq %rbp,test_registers+48(%rip)
    movq %r8,test_registers+56(%rip)
    movq %r9,test_registers+64(%rip)
    movq %r10,test_registers+72(%rip)
    movq %r11,test_registers+80(%rip)
    movq %r12,test_registers+88(%rip)
    movq %r13,test_registers+96(%rip)
    movq %r14,test_registers+104(%rip)
    movq %r15,test_registers+112(%rip)
    /* Save the word a flags push will overwrite before inspecting DF. */
    movq -8(%rsp),%rax
    movq %rax,test_redzone+120(%rip)
    pushfq
    popq %rax
    movq %rax,test_probe_flags(%rip)
    cld
    fxsave64 test_fx_after(%rip)
    movq %fs:0,%rax
    movq %rax,test_fs_after(%rip)
    leaq -128(%rsp),%rsi
    leaq test_redzone(%rip),%rdi
    movq $15,%rcx
    cld
    rep movsq
    fxrstor64 test_fx_saved(%rip)
    movq test_caller_rsp(%rip),%rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.globl test_trampoline
test_trampoline:
    pushfq
    popq %rax
    andq $0x400,%rax
    orq %rax,test_handler_df(%rip)
    subq $40,%rsp
    movq %r8,0(%rsp)
    movq %r9,8(%rsp)
    call *%rcx
    fninit
    fldz
    ldmxcsr test_clobber_mxcsr(%rip)
    pxor %xmm0,%xmm0
    pxor %xmm1,%xmm1
    pxor %xmm2,%xmm2
    pxor %xmm3,%xmm3
    pxor %xmm4,%xmm4
    pxor %xmm5,%xmm5
    pxor %xmm6,%xmm6
    pxor %xmm7,%xmm7
    pxor %xmm8,%xmm8
    pxor %xmm9,%xmm9
    pxor %xmm10,%xmm10
    pxor %xmm11,%xmm11
    pxor %xmm12,%xmm12
    pxor %xmm13,%xmm13
    pxor %xmm14,%xmm14
    pxor %xmm15,%xmm15
    movq $101,%rbx
    movq $102,%rbp
    movq $103,%r12
    movq $104,%r13
    movq $105,%r14
    movq $106,%r15
    movq 0(%rsp),%rsi
    movq 8(%rsp),%rdx
    movq $2,%rdi
    movabsq $0x5349475452415031,%rax
    int3
    ud2
.section .rodata
.p2align 4
test_cw: .short 0x077f
.p2align 2
test_mxcsr: .long 0x3f80
test_clobber_mxcsr: .long 0x1f80
.p2align 4
.globl test_xmm_pattern
test_xmm_pattern:
    .quad 0x1020304050607000,0x9080706050403000
    .quad 0x1020304050607001,0x9080706050403001
    .quad 0x1020304050607002,0x9080706050403002
    .quad 0x1020304050607003,0x9080706050403003
    .quad 0x1020304050607004,0x9080706050403004
    .quad 0x1020304050607005,0x9080706050403005
    .quad 0x1020304050607006,0x9080706050403006
    .quad 0x1020304050607007,0x9080706050403007
    .quad 0x1020304050607008,0x9080706050403008
    .quad 0x1020304050607009,0x9080706050403009
    .quad 0x102030405060700a,0x908070605040300a
    .quad 0x102030405060700b,0x908070605040300b
    .quad 0x102030405060700c,0x908070605040300c
    .quad 0x102030405060700d,0x908070605040300d
    .quad 0x102030405060700e,0x908070605040300e
    .quad 0x102030405060700f,0x908070605040300f
