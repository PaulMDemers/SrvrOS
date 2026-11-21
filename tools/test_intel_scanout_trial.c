#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#define SRVROS_SERVICE_WINDOW_HOST_TEST
#include "../kernel/src/drivers/intel_scanout_trial.c"
static struct {uint64_t physical,frames;void *ptr;bool freed;} allocations[16];
static unsigned allocation_count,free_count,fail_alloc,checkpoints;
static uint64_t us,dma;
static bool admission,guard_ok,reporting,ram_ok;
uint64_t service_window_test_irq_save(void){return 0;}
void service_window_test_irq_restore(uint64_t f){assert(!f);}
bool scheduler_io_begin(void){return true;}
void scheduler_io_end(void){}
void scheduler_io_checkpoint(void){checkpoints++;}
uint64_t timer_monotonic_us(void){us+=100;return us;}
bool timer_reference_ready(void){return true;}
uint64_t vmm_current_address_space(void){return 0x1000;}
uint64_t vmm_kernel_address_space(void){return 0x1000;}
bool vmm_virt_to_phys(uint64_t va,uint64_t *p){(void)va;*p=0xb0000000;return true;}
uint64_t pmm_tagged_frames(enum pmm_frame_tag tag){assert(tag==PMM_FRAME_DMA);return dma;}
uint64_t pmm_alloc_frames_tagged(uint64_t frames,enum pmm_frame_tag tag){assert(tag==PMM_FRAME_DMA);if(allocation_count+1==fail_alloc)return 0;assert(allocation_count<16);unsigned n=allocation_count++;allocations[n].frames=frames;allocations[n].physical=0x1000000+0x800000*n;allocations[n].ptr=malloc(frames*4096);assert(allocations[n].ptr);dma+=frames;return allocations[n].physical;}
void *pmm_phys_to_virt(uint64_t physical){for(unsigned i=0;i<allocation_count;i++)if(allocations[i].physical==physical){assert(!allocations[i].freed);return allocations[i].ptr;}assert(0);return NULL;}
void pmm_free_frame_range(uint64_t physical,uint64_t frames){for(unsigned i=0;i<allocation_count;i++)if(allocations[i].physical==physical){assert(!allocations[i].freed && allocations[i].frames==frames);for(unsigned n=0;n<CASES;n++)for(unsigned j=1;j<3;j++)if(owners[n].profile.physical[j]==physical)assert(owners[n].state==SCANOUT_RETIRED && (owners[n].released&(1u<<(j-1))) && !owners[n].phase && !owners[n].front);allocations[i].freed=true;free_count++;dma-=frames;free(allocations[i].ptr);return;}assert(0);}
bool intel_scanout_trial_profile(struct scanout_profile *p){*p=(struct scanout_profile){.physical={0xb0000000,0,0},.bytes=6144*900,.width=1440,.height=900,.pitch=6144,.mode=SCANOUT_SHADOW,.assured=SCANOUT_RAM_PIN};return admission;}
bool intel_scanout_trial_ram(uint64_t physical,uint64_t frames){(void)physical;(void)frames;return ram_ok;}
bool intel_scanout_trial_clock(struct intel_scanout_clock *c){c->us=timer_monotonic_us();c->frame=(uint32_t)(c->us/2000);c->line=0;return guard_ok;}
bool intel_scanout_trial_revalidate(void){return guard_ok;}
void console_printf(const char *format,...){if(!reporting)return;char out[2048];size_t n=0;for(size_t i=0;format[i];i++){if(format[i]=='%' && format[i+1]=='u'){out[n++]='%';const char *s=PRIu64;while(*s)out[n++]=*s++;i++;}else out[n++]=format[i];}out[n]=0;va_list ap;va_start(ap,format);vprintf(out,ap);va_end(ap);}
static void reset(void){for(unsigned i=0;i<allocation_count;i++)if(!allocations[i].freed)free(allocations[i].ptr);memset(allocations,0,sizeof(allocations));memset(owners,0,sizeof(owners));pool=(struct scanout_pool){0};attempted=false;us=dma=allocation_count=free_count=fail_alloc=checkpoints=0;admission=guard_ok=ram_ok=true;}
int main(int argc,char **argv){
 bool fixture=argc==2 && !strcmp(argv[1],"--fixture-report");reporting=argc==2;
 struct limine_framebuffer fb={.width=1440,.height=900,.pitch=6144,.bpp=32,.memory_model=LIMINE_FRAMEBUFFER_RGB};
 reset();assert(intel_scanout_lifetime_trial(&fb,fixture));assert(allocation_count==16 && free_count==10 && dma==8100 && checkpoints>0);assert(!intel_scanout_lifetime_trial(&fb,fixture));
 if(reporting){reset();return 0;}
 reset();admission=false;assert(!intel_scanout_lifetime_trial(&fb,false) && !allocation_count);
 reset();guard_ok=false;assert(!intel_scanout_lifetime_trial(&fb,false) && !allocation_count);
 for(unsigned fail=1;fail<3;fail++){reset();fail_alloc=fail;assert(!intel_scanout_lifetime_trial(&fb,false));assert(dma==0 && free_count==fail-1);}
 reset();ram_ok=false;assert(!intel_scanout_lifetime_trial(&fb,false) && dma==0 && free_count==1);
 reset();puts("intel-scanout-trial: PASS real producer, shadow-only events, PMM ownership/free guards, quarantine, admission and partial allocation failure");
}
