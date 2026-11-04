/* Actual fpu_init flow with privileged initialization modeled only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t features=(1u<<24)|(1u<<25)|(1u<<26), captured_mask;
static uint64_t fake_cr0=~0ull, fake_cr4;
static unsigned captures,cr0_reads,cr0_writes,cr4_reads,cr4_writes;
static void fpu_init_test_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    assert(leaf==1);*a=*b=*c=0;*d=features;
}
static uint64_t fpu_init_test_read_cr0(void) {cr0_reads++;return fake_cr0;}
static void fpu_init_test_write_cr0(uint64_t value) {cr0_writes++;fake_cr0=value;}
static uint64_t fpu_init_test_read_cr4(void) {cr4_reads++;return fake_cr4;}
static void fpu_init_test_write_cr4(uint64_t value) {cr4_writes++;fake_cr4=value;}
static void fpu_init_test_capture(uint8_t *area) {
    for(unsigned i=0;i<512;i++) assert(area[i]==0);
    captures++;
    for(unsigned i=0;i<4;i++) area[28+i]=(uint8_t)(captured_mask>>(i*8));
}
#define SRVROS_FPU_INIT_HOST_TEST
#include "../kernel/src/arch/x86_64/fpu.c"
void console_write(const char *message) {(void)message;}
int main(void) {
    /* Init-only hooks replace these privileged operations; keep the actual
     * definitions compiled and warning-checked without executing them here. */
    (void)cpuid;(void)read_cr0;(void)write_cr0;(void)read_cr4;(void)write_cr4;
    assert(!available && !sampled_mxcsr_mask_valid && !sampled_mxcsr_mask);
    features=0;assert(!fpu_init());
    assert(!captures && !cr0_reads && !cr0_writes && !cr4_reads && !cr4_writes &&
        !available && !sampled_mxcsr_mask_valid && !sampled_mxcsr_mask);
    features=(1u<<24)|(1u<<25)|(1u<<26);
    memset(initial_fxsave_area,0xa5,sizeof(initial_fxsave_area));
    captured_mask=0xffff;assert(fpu_init());
    assert(captures==1 && sampled_mxcsr_mask_valid && sampled_mxcsr_mask==0xffff);
    assert(!(fake_cr0&(CR0_EM|CR0_TS)) && (fake_cr0&(CR0_MP|CR0_NE))==(CR0_MP|CR0_NE));
    assert((fake_cr4&(CR4_OSFXSR|CR4_OSXMMEXCPT))==(CR4_OSFXSR|CR4_OSXMMEXCPT));
    memset(initial_fxsave_area,0xa5,sizeof(initial_fxsave_area));captured_mask=0;
    assert(fpu_init() && captures==2 && sampled_mxcsr_mask_valid && sampled_mxcsr_mask==0xffbf);
    features=0;assert(!fpu_init() && captures==2 && !sampled_mxcsr_mask_valid && !sampled_mxcsr_mask && !available);
    assert(cr0_reads==2 && cr0_writes==2 && cr4_reads==2 && cr4_writes==2);
    features=(1u<<24)|(1u<<25)|(1u<<26);captured_mask=0x7f80;
    assert(fpu_init() && captures==3 && sampled_mxcsr_mask_valid && sampled_mxcsr_mask==0x7f80);
    assert(cr0_reads==3 && cr0_writes==3 && cr4_reads==3 && cr4_writes==3);
    puts("fpu-init-mask: zero-before-capture, fallback and failed retry pass");
}
