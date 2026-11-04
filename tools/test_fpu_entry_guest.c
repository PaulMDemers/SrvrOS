/* Real user register preservation across int 0x80 and timer scheduling.
 * This is not a test of any private snapshot or signal API. */
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <srvros/sys.h>

struct pattern { uint16_t cw, pad; uint32_t mxcsr; uint64_t xmm[16][2]; uint32_t default_mxcsr; };
struct observation {
    _Alignas(16) unsigned char before[512], after[512];
    uint64_t fs_before,fs_after,rsp_before,rsp_after;
    uint64_t r12,r13,r14,r15,result;
};
_Static_assert(offsetof(struct observation,after)==512,"FX offsets");
_Static_assert(_Alignof(struct observation)>=16,"FXSAVE alignment");
_Static_assert(offsetof(struct observation,fs_before)==1024,"register offsets");
_Static_assert(offsetof(struct observation,result)==1088,"result offset");
_Static_assert(offsetof(struct pattern,mxcsr)==4 && offsetof(struct pattern,xmm)==8 &&
               offsetof(struct pattern,default_mxcsr)==264,"pattern offsets");
static unsigned ready,go,done;
static int worker_result;
static __thread volatile uint64_t tls_cookie;

static int exercise(const struct pattern *p,uint64_t cookie)
{
    struct srv_scheduler_status start={0},finish={0};
    if(srv_scheduler_status(&start) || start.version!=SRV_SCHEDULER_VERSION) return 1;
    tls_cookie=cookie;
    struct pattern local=*p;
    for(unsigned i=1;i<16;i++) {
        local.xmm[i][0]=local.xmm[0][0] ^ (UINT64_C(0x0101010101010101)*i);
        local.xmm[i][1]=local.xmm[0][1] ^ (UINT64_C(0x1010101010101010)*i);
    }
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts)) return 2;
    uint64_t deadline=(uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec+15000000000ull;
    for(unsigned attempt=0;attempt<12;attempt++) {
        struct observation o={0};
        /* RDI points to the output, RSI to read-only patterns. All tested GPRs
         * remain live in this single asm block across syscall and busy loop. */
        __asm__ volatile (
            "fninit\n\tfldcw 0(%%rsi)\n\tfld1\n\tfldpi\n\tldmxcsr 4(%%rsi)\n\t"
            "movdqu 8(%%rsi), %%xmm0\n\t"
            "movdqu 24(%%rsi), %%xmm1\n\t"
            "movdqu 40(%%rsi), %%xmm2\n\t"
            "movdqu 56(%%rsi), %%xmm3\n\t"
            "movdqu 72(%%rsi), %%xmm4\n\t"
            "movdqu 88(%%rsi), %%xmm5\n\t"
            "movdqu 104(%%rsi), %%xmm6\n\t"
            "movdqu 120(%%rsi), %%xmm7\n\t"
            "movdqu 136(%%rsi), %%xmm8\n\t"
            "movdqu 152(%%rsi), %%xmm9\n\t"
            "movdqu 168(%%rsi), %%xmm10\n\t"
            "movdqu 184(%%rsi), %%xmm11\n\t"
            "movdqu 200(%%rsi), %%xmm12\n\t"
            "movdqu 216(%%rsi), %%xmm13\n\t"
            "movdqu 232(%%rsi), %%xmm14\n\t"
            "movdqu 248(%%rsi), %%xmm15\n\t"
            "movabsq $0x123456789abcdef1, %%r12\n\t"
            "movabsq $0x23456789abcdef12, %%r13\n\t"
            "movabsq $0x3456789abcdef123, %%r14\n\t"
            "movabsq $0x456789abcdef1234, %%r15\n\t"
            "movq %%fs:0, %%r10\n\tmovq %%r10, 1024(%%rdi)\n\tmovq %%rsp, 1040(%%rdi)\n\t"
            "fxsave64 0(%%rdi)\n\tmovq $%c[sysyield], %%rax\n\tint $0x80\n\tmovq %%rax, 1088(%%rdi)\n\t"
            "movq $30000000, %%rcx\n\t1: decq %%rcx\n\tjnz 1b\n\t"
            "fxsave64 512(%%rdi)\n\tmovq %%fs:0, %%r10\n\tmovq %%r10, 1032(%%rdi)\n\tmovq %%rsp, 1048(%%rdi)\n\t"
            "movq %%r12, 1056(%%rdi)\n\tmovq %%r13, 1064(%%rdi)\n\t"
            "movq %%r14, 1072(%%rdi)\n\tmovq %%r15, 1080(%%rdi)\n\t"
            "fninit\n\tldmxcsr 264(%%rsi)"
            : : "D"(&o),"S"(&local),[sysyield] "i"(SYS_YIELD)
            : "rax","rcx","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
              "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15","cc","memory");
        uint16_t actual_cw;uint32_t actual_mxcsr;
        memcpy(&actual_cw,o.before,sizeof(actual_cw));memcpy(&actual_mxcsr,o.before+24,sizeof(actual_mxcsr));
        unsigned occupied=0;for(unsigned bit=0;bit<8;bit++) occupied+=(o.before[4]>>bit)&1u;
        static const unsigned char zero_x87[10]={0};
        if(actual_cw!=local.cw || actual_mxcsr!=local.mxcsr || occupied!=2 ||
           !memcmp(o.before+32,zero_x87,10) || !memcmp(o.before+48,zero_x87,10)) return 8;
        /* FXSAVE64 stores the 16 XMM values consecutively starting at 160. */
        for(unsigned i=0;i<16;i++)
            if(memcmp(o.before+160+i*16,local.xmm[i],16)) return 9;
        if(o.result || o.fs_before!=o.fs_after || o.rsp_before!=o.rsp_after || tls_cookie!=cookie ||
           o.r12!=UINT64_C(0x123456789abcdef1) || o.r13!=UINT64_C(0x23456789abcdef12) ||
           o.r14!=UINT64_C(0x3456789abcdef123) || o.r15!=UINT64_C(0x456789abcdef1234) ||
           memcmp(o.before,o.after,sizeof(o.before))) return 3;
        if(srv_scheduler_status(&finish) || finish.version!=SRV_SCHEDULER_VERSION ||
           finish.preemptions<start.preemptions || finish.sampled_ticks<start.sampled_ticks) return 4;
        if(finish.preemptions>start.preemptions && finish.sampled_ticks>start.sampled_ticks) return 0;
        if(clock_gettime(CLOCK_MONOTONIC,&ts)) return 5;
        if((uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec>=deadline) return 6;
    }
    return 7;
}
static void *worker(void *unused)
{
    (void)unused;
    const struct pattern pattern={.cw=0x077f,.mxcsr=0x3f80,
        .xmm={{UINT64_C(0x1122334455667788),UINT64_C(0x8877665544332211)}},.default_mxcsr=0x1f80};
    __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE)) usleep(1000);
    worker_result=exercise(&pattern,UINT64_C(0xabcddcba12344321));
    __atomic_store_n(&done,1,__ATOMIC_RELEASE);
    return NULL;
}
int main(void)
{
    pthread_t thread;
    const struct pattern pattern={.cw=0x037f,.mxcsr=0x1f80,
        .xmm={{UINT64_C(0x0f1e2d3c4b5a6978),UINT64_C(0x78695a4b3c2d1e0f)}},.default_mxcsr=0x1f80};
    if(pthread_create(&thread,NULL,worker,NULL)) return 10;
    struct timespec ts;if(clock_gettime(CLOCK_MONOTONIC,&ts)) return 11;
    uint64_t deadline=(uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec+20000000000ull;
    while(!__atomic_load_n(&ready,__ATOMIC_ACQUIRE)) {
        if(clock_gettime(CLOCK_MONOTONIC,&ts) || (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec>=deadline) return 12;
        usleep(1000);
    }
    __atomic_store_n(&go,1,__ATOMIC_RELEASE);
    int main_result=exercise(&pattern,UINT64_C(0x1020304050607080));
    while(!__atomic_load_n(&done,__ATOMIC_ACQUIRE)) {
        if(clock_gettime(CLOCK_MONOTONIC,&ts) || (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec>=deadline) return 13;
        usleep(1000);
    }
    if(pthread_join(thread,NULL)) return 14;
    if(main_result || worker_result) {
        printf("fpu-entry-guest: failed main=%d worker=%d\n",main_result,worker_result);return 15;
    }
    puts("fpu-entry-guest: main and worker register preservation ok");
    return 0;
}
