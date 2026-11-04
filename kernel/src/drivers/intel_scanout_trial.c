#include <srvros/intel_scanout_trial.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include <srvros/service_window.h>
#include <limine.h>
#define CASES 8u
static struct scanout_lifetime owners[CASES];
static struct scanout_pool pool;
static bool attempted;
struct stats {uint64_t allocated,released,retained,copies,requests,completed,restores,polls,max_wait_us;};
struct trial {unsigned case_id;bool fixture;struct service_window window;struct stats stats;};
static uint64_t clock_us(void){return timer_monotonic_us();}
static bool observe(struct trial *t,struct intel_scanout_clock *c) {
    t->stats.polls++;
    if(!t->fixture)return intel_scanout_trial_clock(c);
    c->us=clock_us();c->frame=(uint32_t)(c->us/2000);c->line=0;return true;
}
static bool copy_frame(struct trial *t,struct scanout_lifetime *o,unsigned marker,bool cancel) {
    struct scanout_token token;if(!scanout_copy_begin(o,o->owner,o->epoch,&token))return false;
    uint32_t *p=pmm_phys_to_virt(o->profile.physical[token.target]);
    uint64_t words=o->profile.bytes/4;
    for(uint64_t i=0;i<words;i++) {
        p[i]=(uint32_t)i^marker;
        if((i&16383)==16383){service_window_checkpoint(&t->window);if(p[i]!=((uint32_t)i^marker))return false;}
    }
    if(!words || p[0]!=marker || p[words-1]!=((uint32_t)(words-1)^marker))return false;
    t->stats.copies++;
    if(cancel && !scanout_close(o,o->owner,o->epoch))return false;
    return scanout_copy_end(o,&token,true);
}
static bool submit(struct trial *t,struct scanout_lifetime *o,struct scanout_token *token) {
    if(!scanout_request(o,o->owner,o->epoch,clock_us(),token))return false;
    t->stats.requests++;
    /* This adapter has no MMIO writer. These are logical publications only. */
    return scanout_issued(o,token,SCANOUT_ISSUED,clock_us());
}
static bool wait_shadow(struct trial *t,struct scanout_lifetime *o,const struct scanout_token *token) {
    struct intel_scanout_clock first,current;
    if(!observe(t,&first)){(void)scanout_poison(o,token->epoch,SCANOUT_CHANGED);return false;}
    for(unsigned spin=0;spin<(t->fixture?100000u:2048u);spin++) {
        if(!observe(t,&current)){(void)scanout_poison(o,token->epoch,SCANOUT_CHANGED);return false;}
        if(!scanout_tick(o,token->epoch,current.us))return false;
        uint32_t delta=current.frame-first.frame;
        if(delta && delta<=8 && current.us>first.us) {
            uint64_t elapsed=current.us-first.us;if(elapsed>t->stats.max_wait_us)t->stats.max_wait_us=elapsed;
            unsigned previous=o->front;
            if(!scanout_complete(o,token,token->target,SCANOUT_SHADOW,current.us))return false;
            console_printf("intel-lifetime-event: v=1 case=%u epoch=%u serial=%u previous=%u target=%u frame_before=%u frame_after=%u begin_us=%u end_us=%u mode=shadow\n",
                (uint64_t)t->case_id,token->epoch,token->serial,(uint64_t)previous,(uint64_t)token->target,(uint64_t)first.frame,(uint64_t)current.frame,first.us,current.us);
            t->stats.completed++;if(!token->target)t->stats.restores++;
            return true;
        }
        service_window_checkpoint(&t->window);
    }
    (void)scanout_poison(o,token->epoch,SCANOUT_TIMEOUT);return false;
}
static bool release_ram(struct trial *t,struct scanout_lifetime *o,uint64_t frames) {
    for(unsigned target=1;target<3;target++) {
        uint64_t physical;
        if(!scanout_take_release(o,o->epoch,target,&physical))return false;
        pmm_free_frame_range(physical,frames);t->stats.released+=frames;
        if(scanout_take_release(o,o->epoch,target,&physical))return false;
    }
    return true;
}
static bool finish(struct trial *t,struct scanout_lifetime *o,uint64_t frames) {
    if(!scanout_close(o,o->owner,o->epoch))return false;
    if(o->state!=SCANOUT_RETIRED) {
        struct scanout_token token;
        if(!submit(t,o,&token) || !wait_shadow(t,o,&token))return false;
    }
    return o->state==SCANOUT_RETIRED && release_ram(t,o,frames);
}
static bool run_case(struct trial *t,unsigned n,const struct scanout_profile *base,uint64_t frames) {
    t->case_id=n;struct scanout_profile p=*base;
    for(unsigned i=1;i<3;i++) {
        p.physical[i]=pmm_alloc_frames_tagged(frames,PMM_FRAME_DMA);
        if(!p.physical[i] || p.physical[i]>0x100000000ull-frames*4096 || (!t->fixture && !intel_scanout_trial_ram(p.physical[i],frames))) {
            for(unsigned j=1;j<=i;j++)if(p.physical[j])pmm_free_frame_range(p.physical[j],frames);
            return false;
        }
    }
    p.assured=SCANOUT_RAM_PIN; /* Private PMM allocations now held by this trial. */
    if(!scanout_open(&owners[n],&pool,&p,100+n,clock_us())) {
        for(unsigned i=1;i<3;i++)pmm_free_frame_range(p.physical[i],frames);
        return false;
    }
    t->stats.allocated+=2*frames;
    struct scanout_lifetime *o=&owners[n];struct scanout_token token;
    bool ok=false;
    if(n==0) {
        ok=true;
        for(unsigned i=0;i<4 && ok;i++)ok=copy_frame(t,o,0x10203040+i,false) && submit(t,o,&token) && wait_shadow(t,o,&token);
        if(ok)ok=finish(t,o,frames);
    } else if(n==1)ok=copy_frame(t,o,0x20406080,true) && o->state==SCANOUT_RETIRED && release_ram(t,o,frames);
    else if(n==2)ok=copy_frame(t,o,0x306090c0,false) && finish(t,o,frames);
    else if(n==3 || n==4) {
        if(n==4 && !(copy_frame(t,o,0x11223344,false) && submit(t,o,&token) && wait_shadow(t,o,&token)))goto done;
        if(!copy_frame(t,o,0x55667788,false) || !submit(t,o,&token) || !scanout_close(o,o->owner,o->epoch))goto done;
        uint64_t physical;if(scanout_take_release(o,o->epoch,token.target,&physical))goto done;
        ok=wait_shadow(t,o,&token) && finish(t,o,frames);
    } else {
        if(!copy_frame(t,o,0x66778899,false) || !scanout_request(o,o->owner,o->epoch,clock_us(),&token))goto done;
        t->stats.requests++;
        if(n==5)ok=!scanout_issued(o,&token,SCANOUT_UNCERTAIN,clock_us());
        else if(n==6)ok=scanout_issued(o,&token,SCANOUT_ISSUED,clock_us()) && !scanout_tick(o,o->epoch,o->deadline_us);
        else ok=scanout_poison(o,o->epoch,SCANOUT_CHANGED);
        uint64_t physical;
        ok=ok && o->state==SCANOUT_FAULT && !scanout_take_release(o,o->epoch,1,&physical) && !scanout_take_release(o,o->epoch,2,&physical);
    }
done:
    if(!ok && o->state!=SCANOUT_FAULT && o->state!=SCANOUT_RETIRED)(void)scanout_poison(o,o->epoch,SCANOUT_CHANGED);
    if(o->state==SCANOUT_FAULT)t->stats.retained+=2*frames;
    console_printf("intel-lifetime-case: v=1 case=%u ok=%u epoch=%u state=%u front=%u phase=%u released=%u fault=%u injected=%u\n",
        (uint64_t)n,(uint64_t)ok,o->epoch,(uint64_t)o->state,(uint64_t)o->front,(uint64_t)o->phase,(uint64_t)o->released,(uint64_t)o->fault,(uint64_t)(n>=5));
    return ok && scanout_invariant(o);
}
bool intel_scanout_lifetime_trial(struct limine_framebuffer *fb,bool fixture) {
    if(attempted)return false;
    attempted=true;
    struct trial t={.fixture=fixture};struct scanout_profile p={0};
    bool admitted=false;
    if(fixture && fb && fb->bpp==32 && fb->memory_model==LIMINE_FRAMEBUFFER_RGB && fb->width && fb->width<=4096 && fb->height && fb->height<=4096 &&
       fb->pitch>=fb->width*4 && fb->pitch<=65536 && !(fb->pitch&63) && fb->pitch*fb->height<=16*1024*1024 &&
       vmm_virt_to_phys((uint64_t)fb->address,&p.physical[0])) {
        p.width=(uint32_t)fb->width;p.height=(uint32_t)fb->height;p.pitch=(uint32_t)fb->pitch;p.bytes=fb->pitch*fb->height;p.mode=SCANOUT_SHADOW;p.assured=0;admitted=true;
    } else if(!fixture)admitted=intel_scanout_trial_profile(&p);
    uint64_t frames=(p.bytes+4095)/4096,began=clock_us(),dma_before=pmm_tagged_frames(PMM_FRAME_DMA);
    unsigned cases=0;bool ok=false,preserved=false;
    if(admitted && frames && frames<=4096 && timer_reference_ready() && vmm_current_address_space()==vmm_kernel_address_space()) {
        t.window=service_window_begin();struct intel_scanout_clock initial;
        ok=observe(&t,&initial);
        for(unsigned n=0;n<CASES && ok;n++) {
            console_printf("intel-lifetime: shadow case %u/%u; display writes disabled\n",(uint64_t)n+1,(uint64_t)CASES);
            ok=run_case(&t,n,&p,frames);if(ok)cases++;
            if(clock_us()<began || clock_us()-began>30000000)ok=false;
        }
        preserved=fixture?true:intel_scanout_trial_revalidate();
        ok=ok && preserved;
        service_window_end(t.window);
    }
    console_printf("intel-lifetime-summary: v=1 kind=%s admitted=%u ok=%u cases=%u frames_per_buffer=%u allocated=%u released=%u retained=%u copies=%u requests=%u completed=%u restores=%u polls=%u max_wait_us=%u elapsed_us=%u preserved=%u dma_before=%u dma_after=%u hardware_writes=0 hardware_completion=0 ownership=shadow\n",
        fixture?"fixture":"native",(uint64_t)admitted,(uint64_t)ok,(uint64_t)cases,frames,t.stats.allocated,t.stats.released,t.stats.retained,t.stats.copies,t.stats.requests,t.stats.completed,t.stats.restores,t.stats.polls,t.stats.max_wait_us,clock_us()-began,(uint64_t)preserved,dma_before,pmm_tagged_frames(PMM_FRAME_DMA));
    return ok;
}
