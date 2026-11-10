#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SERVICE_WINDOW_HOST_TEST
#define SRVROS_INTEL_MEMORY_TRIAL
#include "../kernel/src/drivers/intel_memory_trial.c"
static unsigned char ram[12*1024*1024];
static struct {uint64_t pa,pages;bool active;} spans[8];
static unsigned count;
static uint64_t used,us=100;
static bool tag_loss,alloc_fail,profile_refused,preserve_fail;
uint64_t service_window_test_irq_save(void){return 512;}
void service_window_test_irq_restore(uint64_t flags){assert(flags==512);}
bool scheduler_io_begin(void){return true;}
void scheduler_io_checkpoint(void){}
void scheduler_io_end(void){}
uint64_t timer_monotonic_us(void){return ++us;}
bool timer_reference_ready(void){return true;}
uint64_t pmm_alloc_frames_tagged(uint64_t pages,enum pmm_frame_tag tag) {
    assert(tag==PMM_FRAME_DMA);if(alloc_fail)return 0;
    assert(count<8 && used+(pages+1)*4096<=sizeof ram);uint64_t pa=0x1000000+used;
    spans[count].pa=pa;spans[count].pages=pages;spans[count++].active=true;used+=(pages+1)*4096;return pa;
}
void *pmm_phys_to_virt(uint64_t pa){assert(pa>=0x1000000 && pa<0x1000000+sizeof ram);return ram+pa-0x1000000;}
bool vmm_virt_to_phys(uint64_t va,uint64_t *pa){assert(va>=(uintptr_t)ram && va<(uintptr_t)ram+sizeof ram);*pa=0x1000000+va-(uintptr_t)ram;return true;}
bool pmm_frame_has_tag(uint64_t pa,enum pmm_frame_tag tag) {
    assert(tag==PMM_FRAME_DMA);if(tag_loss)return false;
    for(unsigned i=0;i<count;i++)if(spans[i].active && spans[i].pa<=pa && pa<spans[i].pa+spans[i].pages*4096)return true;
    return false;
}
void pmm_free_frame_range(uint64_t pa,uint64_t pages) {
    for(unsigned i=0;i<count;i++)if(spans[i].pa==pa){assert(spans[i].active && spans[i].pages==pages);spans[i].active=false;return;}
    assert(false);
}
uint64_t pmm_tagged_frames(enum pmm_frame_tag tag){assert(tag==PMM_FRAME_DMA);uint64_t n=13;for(unsigned i=0;i<count;i++)if(spans[i].active)n+=spans[i].pages;return n;}
bool intel_scanout_trial_profile(struct scanout_profile *p){*p=(struct scanout_profile){.width=1440,.height=900,.pitch=6144};return !profile_refused;}
bool intel_scanout_trial_ram(uint64_t pa,uint64_t pages){(void)pa;(void)pages;return true;}
bool intel_scanout_trial_revalidate(void){return !preserve_fail;}
void console_write(const char *s){fputs(s,stdout);}
void console_printf(const char *fmt,...) {
    va_list args;va_start(args,fmt);
    while(*fmt) {
        if(*fmt!='%'){putchar(*fmt++);continue;}
        fmt++;if(*fmt=='u')printf("%"PRIu64,va_arg(args,uint64_t));
        else if(*fmt=='s')fputs(va_arg(args,const char *),stdout);
        else assert(false);
        fmt++;
    }
    va_end(args);
}
int main(int argc,char **argv) {
    bool native=false;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--tag-loss"))tag_loss=true;
        else if(!strcmp(argv[i],"--allocation-fail"))alloc_fail=true;
        else if(!strcmp(argv[i],"--profile-refusal")){profile_refused=true;native=true;}
        else if(!strcmp(argv[i],"--preserve-fail")){preserve_fail=true;native=true;}
        else if(!strcmp(argv[i],"--native"))native=true;
        else assert(false);
    }
    struct limine_framebuffer fb={.bpp=32};
    bool ok=intel_memory_trial(&fb,native?"srvros.intel-memory=backing":"srvros.intel-memory=fixture");
    bool expected=!(tag_loss || alloc_fail || profile_refused || preserve_fail);
    assert(ok==expected);assert(!intel_memory_trial(&fb,"srvros.intel-memory=fixture"));
    assert(intel_memory_retained()==(tag_loss || preserve_fail));
}
