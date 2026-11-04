#include <srvros/x86_cache.h>
#include <srvros/intel_scanout_trial.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static unsigned scenario,reads;
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
    else if(n==0x202 || n==0x203)*v=0;
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
    intel_cache_probe_boot();intel_cache_probe_boot();
    assert(!intel_cache_probe_uc_page(0xc0811000));
    bool ok=intel_cache_probe_report();assert(ok==(scenario==0));
    assert(reads==(scenario==2?0:20));
    if(argc>1 && !strcmp(argv[1],"fresh-uc")) {
        assert(!intel_cache_probe_uc_page(0xc0812000) && reads==20);
        assert(intel_cache_probe_uc_page(0xc0811000) && reads==30);
        scenario=3;assert(!intel_cache_probe_uc_page(0xc0811000) && reads==40);
    }
    return 0;
}
