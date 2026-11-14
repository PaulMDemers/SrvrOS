#include <srvros/intel_memory_trial.h>
#include <srvros/scanout_memory.h>
#include <srvros/intel_scanout_trial.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/service_window.h>
#include <limine.h>
#ifdef SRVROS_INTEL_MEMORY_TRIAL
static struct scanout_memory owners[3];
static struct scanout_memory_pool pool;
static struct scanout_space arena;
static struct scanout_space_pool arena_pool;
static struct scanout_publish_pool publication_pool;
static bool attempted,retained;
struct trial {
    struct service_window window;
    uint64_t begin,allocated,released,expected_retained,cpu_borrows,cpu_ends,close_blocked,range_refused,words;
    unsigned case_id,callbacks,cases,reason;
    bool fixture,inject_release;
};
static bool clock_ok(struct trial *t) {
    uint64_t now=timer_monotonic_us();
    return now>=t->begin && now-t->begin<=30000000;
}
static uint64_t allocate(void *ctx,uint32_t pages) {
    struct trial *t=ctx;uint64_t pa=pmm_alloc_frames_tagged(pages,PMM_FRAME_DMA);
    if(pa)t->allocated+=pages;
    return pa;
}
static bool validate(void *ctx,uint64_t pa,uint32_t pages) {
    struct trial *t=ctx;
    if(!clock_ok(t) || (!t->fixture && !intel_scanout_trial_ram(pa,pages)))return false;
    for(uint32_t i=0;i<pages;i++) {
        uint64_t physical=pa+(uint64_t)i*4096,actual=0;
        if(!pmm_frame_has_tag(physical,PMM_FRAME_DMA) ||
           !vmm_virt_to_phys((uint64_t)pmm_phys_to_virt(physical),&actual) || actual!=physical)return false;
    }
    return true;
}
static bool release(void *ctx,uint64_t pa,uint32_t pages) {
    struct trial *t=ctx;
    /* Deliberate known-private, zero-GPU-exposure failure; keep exactly1 page
     * until reboot. No actual PMM free is attempted in that injected case. */
    if(t->inject_release){t->expected_retained+=pages;return false;}
    if(!validate(ctx,pa,pages))return false;
    pmm_free_frame_range(pa,pages);t->released+=pages;return true;
}
static bool no_encode(void *ctx,uint64_t pa,uint64_t *v) {
    (void)pa;(void)v;((struct trial *)ctx)->callbacks++;return false;
}
static bool no_guard(void *ctx){((struct trial *)ctx)->callbacks++;return false;}
static bool no_read(void *ctx,uint32_t index,uint64_t *v){(void)index;(void)v;return no_guard(ctx);}
static bool no_write(void *ctx,uint32_t index,uint64_t v){(void)index;(void)v;return no_guard(ctx);}
static uint64_t no_clock(void *ctx){(void)no_guard(ctx);return 0;}
static bool create(struct trial *t,struct scanout_memory *m,const struct scanout_memory_layout *l,struct scanout_memory_token *token) {
    return scanout_memory_create(m,&pool,0x4d454d4f5259ull,l,0x100000000ull,
        (struct scanout_memory_io){t,allocate,validate,release},token);
}
static bool pattern(struct trial *t,struct scanout_memory *m,const struct scanout_memory_token *token,uint32_t seed,bool close_during) {
    struct scanout_memory_cpu cpu;
    if(!scanout_memory_cpu_begin(m,token,&cpu))return false;
    t->cpu_borrows++;
    if(close_during) {
        if(scanout_memory_close(m,token) || m->releases || !m->cpu_serial)return false;
        t->close_blocked++;
    }
    volatile uint32_t *p=pmm_phys_to_virt(cpu.physical);uint64_t words=cpu.bytes/4;
    for(uint64_t i=0;i<words;i++) {
        p[i]=(uint32_t)i^seed;
        if((i&16383)==16383){service_window_checkpoint(&t->window);if(!clock_ok(t))return false;}
    }
    for(uint64_t i=0;i<words;i++) {
        if(p[i]!=((uint32_t)i^seed))return false;
        if((i&16383)==16383){service_window_checkpoint(&t->window);if(!clock_ok(t))return false;}
    }
    t->words+=words;
    if(!scanout_memory_cpu_end(m,&cpu) || scanout_memory_cpu_end(m,&cpu))return false;
    t->cpu_ends++;return true;
}
static bool unchanged_pattern(struct scanout_memory *m,uint32_t seed) {
    const volatile uint32_t *p=pmm_phys_to_virt(m->physical);
    for(uint64_t i=0;i<m->layout.bytes/4;i++)if(p[i]!=((uint32_t)i^seed))return false;
    return true;
}
static bool range_refusal(struct trial *t,struct scanout_memory *m,const struct scanout_memory_token *token) {
    struct scanout_memory_binding io={t,no_encode,{t,no_guard,no_read,no_write,no_guard,no_guard,no_clock}};
    if(scanout_memory_prepare(m,token,&arena,&publication_pool,16,io) || m->range.object ||
       m->state!=MEM_RAM || m->error || t->callbacks)return false;
    t->range_refused++;return true;
}
static bool finish(struct trial *t,unsigned id,struct scanout_memory *m,const struct scanout_memory_token *token,bool inject) {
    t->inject_release=inject;
    bool closed=scanout_memory_close(m,token);
    bool ok=inject?(!closed && m->state==MEM_FAULT && m->error==MEM_RELEASE && m->release_attempts==1 && !m->releases):
        (closed && m->state==MEM_RELEASED && m->release_attempts==1 && m->releases==1);
    unsigned attempts=m->release_attempts;
    if(scanout_memory_close(m,token) || m->release_attempts!=attempts)ok=false;
    console_printf("intel-memory-case: v=1 id=%u epoch=%u pages=%u state=%u error=%u release_attempts=%u releases=%u injected=%u ok=%u\n",
        (uint64_t)id,m->epoch,(uint64_t)m->layout.pages,(uint64_t)m->state,(uint64_t)m->error,
        (uint64_t)m->release_attempts,(uint64_t)m->releases,(uint64_t)inject,(uint64_t)ok);
    t->cases++; /* Attempted case rows stay decodable on a failed outcome. */
    return ok;
}
bool intel_memory_retained(void){return retained;}
bool intel_memory_trial(struct limine_framebuffer *fb,const char *cmdline) {
    if(attempted)return false;
    attempted=true;
    bool fixture=false;
    /* The immutable staging profile is checked independently by the collector.
     * No native grant/encoding/writer exists, even on a matching profile. */
    if(cmdline)for(const char *p=cmdline;*p;p++) {
        static const char key[]="srvros.intel-memory=fixture";unsigned i=0;
        if(p!=cmdline && p[-1]!=' ')continue;
        while(key[i] && p[i]==key[i])i++;
        if(!key[i] && (!p[i] || p[i]==' '))fixture=true;
    }
    struct trial t={.fixture=fixture};struct scanout_profile profile={0};struct scanout_memory_layout full={0},small={0};
    bool admitted=fixture?(fb && fb->bpp==32):intel_scanout_trial_profile(&profile);
    bool ok=admitted && timer_reference_ready() &&
        scanout_memory_layout(fixture?1440:profile.width,fixture?900:profile.height,fixture?6144:profile.pitch,true,&full) &&
        scanout_memory_layout(1,1,64,false,&small) && full.pages==1356;
    uint64_t before=pmm_tagged_frames(PMM_FRAME_DMA);t.begin=timer_monotonic_us();t.window=service_window_begin();
    console_write("intel-memory: four backing ownership cases; full padded buffers, no GPU mappings or plane writes; no input needed\n");
    struct scanout_memory_token first={0},a={0},b={0},faulted={0};
    if(ok)ok=create(&t,&owners[0],&small,&first) && pattern(&t,&owners[0],&first,0x11223344,true) && finish(&t,0,&owners[0],&first,false);
    if(ok)ok=create(&t,&owners[0],&full,&a) && create(&t,&owners[1],&full,&b) &&
        !scanout_memory_close(&owners[0],&first) &&
        (owners[0].physical+full.pages*4096ull<=owners[1].physical || owners[1].physical+full.pages*4096ull<=owners[0].physical) &&
        pattern(&t,&owners[0],&a,0x55667788,false) && pattern(&t,&owners[1],&b,0xabcdef01,false) &&
        unchanged_pattern(&owners[0],0x55667788) && unchanged_pattern(&owners[1],0xabcdef01);
    struct scanout_space_range protect={0,8704};
    if(ok)ok=scanout_space_open(&arena,&arena_pool,0x4d454d4f5259ull,SCANOUT_SPACE_PAGES,0,0,&protect,1) &&
        range_refusal(&t,&owners[0],&a) && range_refusal(&t,&owners[1],&b) &&
        scanout_space_close(&arena,arena.owner,arena.epoch) &&
        finish(&t,1,&owners[0],&a,false) && unchanged_pattern(&owners[1],0xabcdef01) && finish(&t,2,&owners[1],&b,false);
    if(ok)ok=create(&t,&owners[2],&small,&faulted) && finish(&t,3,&owners[2],&faulted,true);
    bool preserved=fixture || (admitted && intel_scanout_trial_revalidate());
    uint64_t after=pmm_tagged_frames(PMM_FRAME_DMA),elapsed=timer_monotonic_us()-t.begin;
    ok=ok && preserved && clock_ok(&t) && !t.callbacks && t.cases==4 &&
        t.allocated==2714 && t.released==2713 && t.expected_retained==1 && after==before+1;
    if(!ok)for(unsigned i=0;i<3;i++)if(owners[i].physical && owners[i].state!=MEM_RELEASED)retained=true;
    service_window_end(t.window);
    console_printf("intel-memory-summary: v=1 kind=%s admitted=%u complete=%u cases=%u pages=%u visible_pages=%u bytes=%u allocated=%u released=%u retained=%u cpu_borrows=%u cpu_ends=%u close_blocked=%u range_refused=%u words=%u callbacks=%u preserved=%u dma_before=%u dma_after=%u elapsed_us=%u terminal_retained=%u gpu_writes=0 managed_pages=0 hardware_ownership=0 obligations=255\n",
        fixture?"fixture":"native",(uint64_t)admitted,(uint64_t)ok,(uint64_t)t.cases,(uint64_t)full.pages,
        (full.visible_bytes+4095)/4096,full.bytes,t.allocated,t.released,t.expected_retained,t.cpu_borrows,t.cpu_ends,t.close_blocked,
        t.range_refused,t.words,(uint64_t)t.callbacks,(uint64_t)preserved,before,after,elapsed,(uint64_t)retained);
    return ok;
}
#else
bool intel_memory_retained(void){return false;}
bool intel_memory_trial(struct limine_framebuffer *fb,const char *cmdline){(void)fb;(void)cmdline;return false;}
#endif
