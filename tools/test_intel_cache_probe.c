#include <srvros/x86_cache.h>
#include <srvros/intel_scanout_trial.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static unsigned scenario,reads;
static bool gt_mtrr_uc;
static uint64_t us=100;
bool timer_reference_ready(void){return true;}
uint64_t timer_monotonic_us(void){return ++us;}
bool intel_scanout_trial_cache_pages(uint64_t p[3]) {
    if(scenario==1)return false;
    p[0]=0xc0101000;p[1]=0xc0811000;p[2]=0xb2200000;return true;
}
bool intel_scanout_trial_revalidate(void){return scenario!=4;}
bool cache_test_cpu(void *ctx,struct x86_cache_cpu *c) {
    assert(!ctx);*c=(struct x86_cache_cpu){{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x11000,39,0x80050033};
    if(scenario==2)c->features=0;
    return true;
}
bool cache_test_msr(void *ctx,uint32_t n,uint64_t *v) {
    assert(!ctx);reads++;
    if(n==0xfe)*v=0x402;
    else if(n==0x277)*v=0x0007040600070406ull;
    else if(n==0x2ff)*v=0x806;
    else if(n==0x200)*v=0xb0000001;
    else if(n==0x201)*v=0x7ff0000800ull;
    else if(n==0x202)*v=gt_mtrr_uc?0xc0000000:0;
    else if(n==0x203)*v=gt_mtrr_uc?0x7ff0000800ull:0;
    else {assert(0);return false;}
    if(scenario==3 && reads>10 && n==0x277)*v^=7;
    return true;
}
void console_write(const char *s){fputs(s,stdout);}
void console_printf(const char *f,...) {
    va_list a;va_start(a,f);
    for(;*f;f++) {
        if(*f!='%'){putchar(*f);continue;}
        f++;uint64_t n=va_arg(a,uint64_t);
        if(*f=='u')printf("%" PRIu64,n);
        else {assert(*f=='x');printf("0x%016" PRIx64,n);}
    }
    va_end(a);
}
int main(int argc,char **argv) {
    if(argc>1) {
        const char *names[]={"native-model","cpu-only","refused","changed","preserve-failed"};
        for(unsigned i=0;i<5;i++)if(!strcmp(argv[1],names[i]))scenario=i;
    }
    gt_mtrr_uc=argc>1 && (!strcmp(argv[1],"fresh-uc") || !strcmp(argv[1],"wake-uc") || !strcmp(argv[1],"engine-uc") || !strcmp(argv[1],"reader-uc") || !strcmp(argv[1],"reset-uc") || !strcmp(argv[1],"completion-uc") || !strcmp(argv[1],"hda-uc"));
    intel_cache_probe_boot();intel_cache_probe_boot();
    assert(!intel_cache_probe_uc_page(0xc0811000));
    bool ok=intel_cache_probe_report();assert(ok==(scenario==0));
    assert(reads==(scenario==2?0:20));
    if(argc>1 && !strcmp(argv[1],"fresh-uc")) {
        assert(!intel_cache_probe_uc_page(0xc0812000) && reads==20);
        assert(intel_cache_probe_uc_page(0xc0811000) && reads==30);
        assert(!intel_cache_probe_gt_uc_page(0xc000b000,0xb000) && reads==30);
        assert(!intel_cache_probe_gt_uc_page(0xc000a000,0x130000) && reads==30);
        assert(intel_cache_probe_gt_uc_page(0xc000a000,0xa000) && reads==40);
        assert(intel_cache_probe_gt_uc_page(0xc0130000,0x130000) && reads==50);
        scenario=3;assert(!intel_cache_probe_gt_uc_page(0xc000a000,0xa000) && reads==60);
        assert(!intel_cache_probe_uc_page(0xc0811000) && reads==70);
    }
    if(argc>1 && !strcmp(argv[1],"gt-pat-only"))assert(!intel_cache_probe_gt_uc_page(0xc000a000,0xa000) && reads==30);
    if(argc>1 && !strcmp(argv[1],"firmware-wb")) {
        assert(!intel_cache_probe_firmware_wb_page(0x12345001) && reads==20);
        assert(!intel_cache_probe_firmware_wb_page(0x100000000ull) && reads==20);
        assert(intel_cache_probe_firmware_wb_page(0x12345000) && reads==30);
        assert(!intel_cache_probe_firmware_wb_page(0xb0000000) && reads==40);
        scenario=3;assert(!intel_cache_probe_firmware_wb_page(0x12345000) && reads==50);
    }
    if(argc>1 && !strcmp(argv[1],"wake-uc")) {
        assert(!intel_cache_probe_wake_uc_page(0xc0138000,0x139000) && reads==20);
        assert(!intel_cache_probe_wake_uc_page(0xc0139000,0x138000) && reads==20);
        assert(intel_cache_probe_wake_uc_page(0xc000a000,0xa000) && reads==30);
        assert(intel_cache_probe_wake_uc_page(0xc0130000,0x130000) && reads==40);
        assert(intel_cache_probe_wake_uc_page(0xc0138000,0x138000) && reads==50);
        scenario=3;assert(!intel_cache_probe_wake_uc_page(0xc0138000,0x138000) && reads==60);
    }
    if(argc>1 && !strcmp(argv[1],"engine-uc")) {
        assert(!intel_cache_probe_engine_uc_page(0xc000a000,0xa000) && reads==20);
        assert(!intel_cache_probe_engine_uc_page(0xc0003000,0x2000) && reads==20);
        const uint32_t pages[]={0x2000,0x12000,0x1a000,0x1c000,0x22000};
        for(unsigned i=0;i<5;i++)assert(intel_cache_probe_engine_uc_page(0xc0000000ull+pages[i],pages[i]));
        assert(reads==70);scenario=3;
        assert(!intel_cache_probe_engine_uc_page(0xc0022000,0x22000) && reads==80);
    }
    if(argc>1 && !strcmp(argv[1],"completion-uc")) {
        assert(!intel_cache_probe_completion_uc_page(0xc0005000,0x4000) && reads==20);
        assert(!intel_cache_probe_completion_uc_page(0xc0004000,0x5000) && reads==20);
        assert(intel_cache_probe_completion_uc_page(0xc0004000,0x4000) && reads==30);scenario=3;
        assert(!intel_cache_probe_completion_uc_page(0xc0004000,0x4000) && reads==40);
    }
    if(argc>1 && !strcmp(argv[1],"reset-uc")) {
        assert(!intel_cache_probe_reset_uc_page(0xc000a000,0x9000) && reads==20);
        assert(!intel_cache_probe_reset_uc_page(0xc0009000,0xa000) && reads==20);
        assert(intel_cache_probe_reset_uc_page(0xc0009000,0x9000) && reads==30);scenario=3;
        assert(!intel_cache_probe_reset_uc_page(0xc0009000,0x9000) && reads==40);
    }
    if(argc>1 && !strcmp(argv[1],"reader-uc")) {
        assert(!intel_cache_probe_reader_uc_page(0xc000a000,0xa000) && reads==20);
        assert(!intel_cache_probe_reader_uc_page(0xc0071000,0x70000) && reads==20);
        for(unsigned i=0;i<3;i++)assert(intel_cache_probe_reader_uc_page(0xc0070000ull+i*4096,0x70000+i*4096));
        assert(reads==50);scenario=3;
        assert(!intel_cache_probe_reader_uc_page(0xc0070000,0x70000) && reads==60);
    }
    if(argc>1 && !strcmp(argv[1],"hda-uc")) {
        assert(!intel_cache_probe_hda_uc_page(0xc1611000) && reads==20);
        assert(!intel_cache_probe_hda_uc_page(0xc000a000) && reads==20);
        assert(intel_cache_probe_hda_uc_page(0xc1610000) && reads==30);
        scenario=3;assert(!intel_cache_probe_hda_uc_page(0xc1610000) && reads==40);
    }
    if(argc>1 && !strcmp(argv[1],"reset-pat-only"))assert(!intel_cache_probe_reset_uc_page(0xc0009000,0x9000) && reads==30);
    if(argc>1 && !strcmp(argv[1],"hda-pat-only"))assert(!intel_cache_probe_hda_uc_page(0xc1610000) && reads==30);
    return 0;
}
