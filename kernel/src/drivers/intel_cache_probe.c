#include <srvros/x86_cache.h>
#include <srvros/intel_scanout_trial.h>
#include <srvros/timer.h>
#include <srvros/console.h>
static struct x86_cache_snapshot sample[2];
static bool attempted,report_ok;
#ifdef SRVROS_CACHE_PROBE_HOST_TEST
bool cache_test_cpu(void *,struct x86_cache_cpu *);
bool cache_test_msr(void *,uint32_t,uint64_t *);
#define read_cpu cache_test_cpu
#define read_msr cache_test_msr
#else
static void cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
static bool read_cpu(void *ctx,struct x86_cache_cpu *c) {
    (void)ctx;uint32_t a,b,x,d;
    cpuid(0,&a,&b,&x,&d);if(a<1)return false;
    c->vendor[0]=b;c->vendor[1]=d;c->vendor[2]=x;
    cpuid(1,&a,&b,&x,&d);c->signature=a;c->features=d;
    cpuid(0x80000000,&a,&b,&x,&d);if(a<0x80000008)return false;
    cpuid(0x80000008,&a,&b,&x,&d);c->physical_bits=a&255;
    __asm__ volatile("mov %%cr0,%0":"=r"(c->cr0));return true;
}
static bool read_msr(void *ctx,uint32_t n,uint64_t *v) {
    (void)ctx;uint32_t a,d;
    /* The core admits exactly architectural PAT/MTRR reads after feature
     * checks. No external register index or write callback is accepted. */
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));*v=a|((uint64_t)d<<32);return true;
}
#endif
static uint64_t now(void *ctx){(void)ctx;return timer_monotonic_us();}
static const struct x86_cache_io io={0,read_cpu,read_msr,now};
void intel_cache_probe_boot(void) {
    if(attempted)return;
    attempted=true;
    console_write("intel-cache: beginning bounded read-only CPU0 PAT/MTRR audit; no GPU authority\n");
    if(timer_reference_ready())(void)x86_cache_snapshot(&sample[0],&io);
}
bool intel_cache_probe_report(void) {
    if(!attempted)return true;
    if(timer_reference_ready())(void)x86_cache_snapshot(&sample[1],&io);
    uint64_t pages[3]={0};bool native=intel_scanout_trial_cache_pages(pages);
    native=native && sample[0].cpu.signature==0x306d4 && sample[0].cpu.physical_bits==39;
    bool equal=x86_cache_equal(&sample[0],&sample[1]);
    bool complete=equal && sample[1].begin_us>=sample[0].end_us;
    bool queries=true;
    const unsigned indices[]={0,2,3};
    for(unsigned n=0;n<2;n++) {
        const struct x86_cache_snapshot *s=&sample[n];
        console_printf("intel-cache-sample: v=1 sample=%u status=%u begin_us=%u end_us=%u reads=%u count=%u fixed=%u signature=%x features=%x bits=%u cr0=%x pat=%x cap=%x def=%x vendor0=%x vendor1=%x vendor2=%x\n",
            (uint64_t)n,(uint64_t)s->status,s->begin_us,s->end_us,(uint64_t)s->reads,(uint64_t)s->count,(uint64_t)s->fixed_count,
            (uint64_t)s->cpu.signature,(uint64_t)s->cpu.features,(uint64_t)s->cpu.physical_bits,s->cpu.cr0,s->pat,s->cap,s->def_type,
            (uint64_t)s->cpu.vendor[0],(uint64_t)s->cpu.vendor[1],(uint64_t)s->cpu.vendor[2]);
        for(unsigned i=0;i<s->count && i<X86_CACHE_VARIABLES;i++)
            console_printf("intel-cache-variable: v=1 sample=%u index=%u base=%x mask=%x\n",(uint64_t)n,(uint64_t)i,s->base[i],s->mask[i]);
        for(unsigned i=0;i<s->fixed_count && i<X86_CACHE_FIXED;i++)
            console_printf("intel-cache-fixed: v=1 sample=%u index=%u raw=%x\n",(uint64_t)n,(uint64_t)i,s->fixed[i]);
        if(native)for(unsigned page=0;page<3;page++)for(unsigned j=0;j<3;j++) {
            unsigned m=255,p=255,e=255;bool ok=x86_cache_page(s,pages[page],indices[j],&m,&p,&e);queries=queries && ok;
            console_printf("intel-cache-page: v=1 sample=%u page=%u physical=%x pat_index=%u ok=%u mtrr=%u pat=%u effective=%u\n",
                (uint64_t)n,(uint64_t)page,pages[page],(uint64_t)indices[j],(uint64_t)ok,(uint64_t)m,(uint64_t)p,(uint64_t)e);
        }
    }
    bool preserved=native && intel_scanout_trial_revalidate();
    console_printf("intel-cache-summary: v=1 cpu_complete=%u native_pages=%u preserved=%u equal=%u queries=%u writes=0 candidate_mappings=0 authority=none\n",
        (uint64_t)complete,(uint64_t)(native?3:0),(uint64_t)preserved,(uint64_t)equal,(uint64_t)(native && queries));
    report_ok=complete && native && queries && preserved;
    return report_ok;
}

bool intel_cache_probe_uc_page(uint64_t physical) {
    struct x86_cache_snapshot current;unsigned m,p,e;uint64_t pages[3];
    if(!report_ok || !intel_scanout_trial_cache_pages(pages) || physical!=pages[1] ||
       !x86_cache_snapshot(&current,&io) || !x86_cache_equal(&sample[1],&current))return false;
    return x86_cache_page(&current,physical,2,&m,&p,&e) && e==0;
}

/* Keep the accepted candidate-page checker unchanged. These two explicitly
 * audited pages are only cache eligibility, never MMIO or hardware authority. */
bool intel_cache_probe_gt_uc_page(uint64_t physical,uint32_t offset) {
    struct x86_cache_snapshot current;unsigned m,p,e;uint64_t pages[3];
    if((offset!=0xa000 && offset!=0x130000) || !report_ok || !intel_scanout_trial_cache_pages(pages) ||
       pages[0]<0x101000 || physical!=pages[0]-0x101000+offset ||
       !x86_cache_snapshot(&current,&io) || !x86_cache_equal(&sample[1],&current))return false;
    return x86_cache_page(&current,physical,2,&m,&p,&e) && m==0 && e==0;
}
