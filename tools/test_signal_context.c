/* Host-only tests for private frame primitives, not signal delivery. */
#define _GNU_SOURCE
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../kernel/src/arch/x86_64/signal_context.h"

_Static_assert(sizeof(struct signal_context_snapshot) == 688, "snapshot size");
_Static_assert(_Alignof(struct signal_context_snapshot) >= 16, "snapshot alignment");
_Static_assert(offsetof(struct signal_context_snapshot, size) == 0, "size offset");
_Static_assert(offsetof(struct signal_context_snapshot, version) == 4, "version offset");
_Static_assert(offsetof(struct signal_context_snapshot, flags) == 8, "flags offset");
_Static_assert(offsetof(struct signal_context_snapshot, token) == 16, "token offset");
_Static_assert(offsetof(struct signal_context_snapshot, interrupted_mask) == 24, "mask offset");
_Static_assert(_Alignof(struct signal_context_record) >= 16, "record alignment");
_Static_assert(offsetof(struct signal_context_record, fpu) % 16 == 0, "record FPU alignment");
_Static_assert(offsetof(struct signal_context_snapshot, r15) == 32, "GPR offset");
_Static_assert(offsetof(struct signal_context_snapshot, rip) == 152, "RIP offset");
_Static_assert(offsetof(struct signal_context_snapshot, rsp) == 160, "RSP offset");
_Static_assert(offsetof(struct signal_context_snapshot, rflags) == 168, "flags offset");
_Static_assert(offsetof(struct signal_context_snapshot, fpu) == 176, "FX offset");

static struct isr_frame valid_frame(void)
{
    return (struct isr_frame) { .r15=15, .r14=14, .r13=13, .r12=12,
        .r11=11, .r10=10, .r9=9, .r8=8, .rbp=7, .rdi=6, .rsi=5,
        .rdx=4, .rcx=3, .rbx=2, .rax=1, .vector=0x80, .error_code=0,
        .rip=0x123456, .rsp=0x10000, .cs=0x23, .ss=0x1b, .rflags=0x202 };
}

static void capture_tests(void)
{
    struct isr_frame frame=valid_frame();
    uint8_t fx[512];
    for (unsigned i=0; i<sizeof(fx); i++) fx[i]=(uint8_t)(i*37+3);
    struct signal_context_snapshot out;
    assert(signal_context_capture(&out,&frame,fx,0x91,0x82,0x73));
    assert(out.size==688 && out.version==1 && out.flags==0x91 && out.token==0x82 && out.interrupted_mask==0x73);
    assert(out.r15==15 && out.r14==14 && out.r13==13 && out.r12==12 && out.r11==11 && out.r10==10 && out.r9==9 && out.r8==8);
    assert(out.rbp==7 && out.rdi==6 && out.rsi==5 && out.rdx==4 && out.rcx==3 && out.rbx==2 && out.rax==1);
    assert(out.rip==0x123456 && out.rsp==0x10000 && out.rflags==0x202 && !memcmp(out.fpu,fx,sizeof(fx)));
    struct signal_context_snapshot before;
    memset(&out,0xa5,sizeof(out)); memcpy(&before,&out,sizeof(out));
#define REJECT_FRAME(field, value) do { frame=valid_frame(); frame.field=(value); \
    assert(!signal_context_capture(&out,&frame,fx,0,1,0)); assert(!memcmp(&out,&before,sizeof(out))); } while (0)
    REJECT_FRAME(cs,0x08); REJECT_FRAME(cs,0x1b); REJECT_FRAME(ss,0x10);
    REJECT_FRAME(rip,0); REJECT_FRAME(rip,UINT64_C(0x800000000000));
    REJECT_FRAME(rsp,UINT64_MAX); REJECT_FRAME(rflags,0x2); REJECT_FRAME(rflags,0x200);
    REJECT_FRAME(rflags,0x3202); REJECT_FRAME(rflags,0x4202); REJECT_FRAME(rflags,0x20202);
#undef REJECT_FRAME
    frame=valid_frame(); assert(!signal_context_capture(&out,&frame,NULL,0,1,0));
    assert(!memcmp(&out,&before,sizeof(out)));
}

static void cpl0_guard_test(void)
{
    long page=sysconf(_SC_PAGESIZE);
    assert(page > 0 && (size_t)page > offsetof(struct isr_frame, rsp));
    unsigned char *memory=mmap(NULL,(size_t)page*2,PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANON,-1,0);
    assert(memory != MAP_FAILED);
    assert(mprotect(memory+page,(size_t)page,PROT_NONE)==0);
    /* A CPL0 interrupt has no saved user RSP/SS. End the real prefix at
     * the page boundary so any speculative user-field read faults. */
    struct isr_frame *short_frame=(struct isr_frame *)(void *)(memory+page-offsetof(struct isr_frame,rsp));
    struct isr_frame source=valid_frame(); source.cs=0x08;
    memcpy(short_frame,&source,offsetof(struct isr_frame,rsp));
    uint8_t fx[512]={0}; struct signal_context_snapshot out, before;
    memset(&out,0x5a,sizeof(out)); memcpy(&before,&out,sizeof(out));
    assert(!signal_context_capture(&out,short_frame,fx,0,1,0));
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(munmap(memory,(size_t)page*2)==0);
}

static void plan_tests(void)
{
    struct signal_context_plan p;
    assert(signal_context_plan_frame(&p,0x10000,696,NULL,0));
    assert(p.entry_rsp==0xfcc8 && p.snapshot==0xfcd0 && p.end==0xff80);
    assert((p.entry_rsp & 15)==8 && (p.snapshot & 15)==0 && p.end<=0x10000-128);
    struct signal_context_range outside={0xf000,0xfcc8};
    assert(signal_context_plan_frame(&p,0x10000,696,&outside,1));
    struct signal_context_plan before={0x111,0x222,0x333}; p=before;
    struct signal_context_range overlap={0xff7f,0xff81};
    assert(!signal_context_plan_frame(&p,0x10000,696,&overlap,1)); assert(!memcmp(&p,&before,sizeof(p)));
    assert(!signal_context_plan_frame(&p,0x10000,695,NULL,0));
    assert(!signal_context_plan_frame(&p,0x10000,1025,NULL,0));
    assert(!signal_context_plan_frame(&p,800,696,NULL,0));
    assert(!signal_context_plan_frame(&p,UINT64_MAX,696,NULL,0));
    assert(!signal_context_plan_frame(&p,UINT64_C(0x800000000000),696,NULL,0));
    assert(!signal_context_plan_frame(&p,0x10000,696,NULL,1));
    assert(!memcmp(&p,&before,sizeof(p)));
}

static void stack_tests(void)
{
    struct signal_context_stack stack={0}, before;
    struct signal_context_record item={0}, out, sentinel;
    item.frame=valid_frame(); item.image_generation=10; item.thread_generation=20;
    memset(&out,0x5a,sizeof(out)); memcpy(&sentinel,&out,sizeof(out));
    assert(!signal_context_pop(&stack,&out,10,20,1,100)); assert(!memcmp(&out,&sentinel,sizeof(out)));
    assert(!signal_context_push(NULL,&item,10,20));
    assert(!signal_context_push(&stack,NULL,10,20));
    assert(!signal_context_pop(NULL,&out,10,20,1,100));
    assert(!signal_context_pop(&stack,NULL,10,20,1,100));
    stack.depth=5; before=stack;
    assert(!signal_context_push(&stack,&item,10,20));
    assert(!signal_context_pop(&stack,&out,10,20,1,100));
    assert(!memcmp(&stack,&before,sizeof(stack)) && !memcmp(&out,&sentinel,sizeof(out)));
    stack.depth=0;
    for (uint64_t i=1; i<=4; ++i) {
        item.token=i; item.frame_identity=100+i; item.mask=500+i; item.fpu[0]=(uint8_t)(i+30);
        assert(signal_context_push(&stack,&item,10,20));
        if (i==1) {
            before=stack;
            struct signal_context_record foreign=item;
            foreign.token=2; foreign.frame_identity=102; foreign.image_generation=11;
            assert(!signal_context_push(&stack,&foreign,11,20));
            foreign.image_generation=10; foreign.thread_generation=21;
            assert(!signal_context_push(&stack,&foreign,10,21));
            assert(!memcmp(&stack,&before,sizeof(stack)));
        }
    }
    before=stack; item.token=5; item.frame_identity=105;
    assert(!signal_context_push(&stack,&item,10,20)); assert(!memcmp(&stack,&before,sizeof(stack)));
#define BAD_POP(image,thread,token,identity) do { \
    assert(!signal_context_pop(&stack,&out,(image),(thread),(token),(identity))); \
    assert(!memcmp(&stack,&before,sizeof(stack))); assert(!memcmp(&out,&sentinel,sizeof(out))); } while (0)
    BAD_POP(11,20,4,104); BAD_POP(10,21,4,104); BAD_POP(10,20,3,103); BAD_POP(10,20,4,103);
#undef BAD_POP
    for (uint64_t i=4; i>0; --i) {
        assert(signal_context_pop(&stack,&out,10,20,i,100+i));
        assert(out.mask==500+i && out.fpu[0]==i+30 && out.frame.r15==15);
    }
    before=stack; item.token=4; item.frame_identity=104;
    assert(!signal_context_push(&stack,&item,10,20)); assert(!memcmp(&stack,&before,sizeof(stack)));
    /* Model invalidation by dropping the old stack: changed owners cannot pop it. */
    item.token=5; item.image_generation=10; item.thread_generation=20;
    assert(signal_context_push(&stack,&item,10,20)); before=stack;
    assert(!signal_context_pop(&stack,&out,11,20,5,104)); assert(!memcmp(&stack,&before,sizeof(stack)));
    assert(!signal_context_pop(&stack,&out,10,21,5,104)); assert(!memcmp(&stack,&before,sizeof(stack)));
    stack=(struct signal_context_stack){0}; item.image_generation=11; item.thread_generation=21;
    item.token=1; item.frame_identity=200;
    assert(signal_context_push(&stack,&item,11,21));
    assert(!signal_context_pop(&stack,&out,10,20,1,200));
    assert(signal_context_pop(&stack,&out,11,21,1,200));
    stack.last_token=UINT64_MAX; before=stack; item.token=UINT64_MAX;
    assert(!signal_context_push(&stack,&item,11,21)); assert(!memcmp(&stack,&before,sizeof(stack)));
}

static void generation_tests(void)
{
    uint64_t state=0, id=99;
    assert(signal_context_next_generation(&state,&id) && state==1 && id==1);
    state=UINT64_MAX-1; assert(signal_context_next_generation(&state,&id));
    assert(state==UINT64_MAX && id==UINT64_MAX);
    id=77; assert(!signal_context_next_generation(&state,&id)); assert(state==UINT64_MAX && id==77);
    state=5; assert(!signal_context_next_generation(&state,&state)); assert(state==5);
}

int main(void)
{
    capture_tests(); cpl0_guard_test(); plan_tests(); stack_tests(); generation_tests();
    puts("signal-context-host: PASS (private helpers only)");
    return 0;
}
