/* Real process memory loops: service windows cannot expose freed mappings or
 * release an inherited serialization owner. No audio policy is involved. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_SERVICE_HOST_TEST
#include "../kernel/src/process.c"
static unsigned depth,begins,ends,checkpoints,yields,freed,destroyed;
static bool available=true,fail_unmap;
static struct process *observed;
static bool released[96];
static unsigned next_frame=1;
static unsigned char backing[96*4096];
void console_printf(const char *format,...) {(void)format;}
bool scheduler_io_begin(void) {begins++;if(!available)return false;depth++;return true;}
void scheduler_io_end(void) {assert(depth);depth--;ends++;}
void scheduler_io_checkpoint(void) {
    checkpoints++;if(!depth)return;yields++;
    if(observed)for(uint64_t i=0;i<observed->mapping_count;i++) {
        unsigned frame=observed->mappings[i].physical_address/4096;
        assert(frame && frame<96 && !released[frame]);
    }
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t address,uint64_t *physical) {
    assert(observed && space==observed->address_space);
    for(uint64_t i=0;i<observed->mapping_count;i++)if(observed->mappings[i].virtual_address==address) {
        *physical=observed->mappings[i].physical_address;return !fail_unmap;
    }
    assert(0);return false;
}
void pmm_free_frame(uint64_t physical) {
    unsigned frame=physical/4096;assert(frame && frame<96 && !released[frame]);released[frame]=true;freed++;
}
void vmm_destroy_address_space(uint64_t space) {assert(observed && !observed->mapping_count && space==0x1000);destroyed++;}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t minimum,enum pmm_frame_tag tag) {
    assert(n==1 && minimum==PROCESS_USER_PAGE_MIN_PHYSICAL && tag==PMM_FRAME_USER);
    return next_frame<96?4096*next_frame++:0;
}
void *pmm_phys_to_virt(uint64_t physical) {assert(physical<sizeof(backing));return backing+physical;}
bool vmm_map_page_in_space(uint64_t space,uint64_t virt,uint64_t physical,uint64_t flags) {
    assert(space==0x1000 && virt && physical && flags);return true;
}
static void reset(void) {
    assert(!depth);begins=ends=checkpoints=yields=freed=destroyed=0;next_frame=1;
    memset(released,0,sizeof(released));memset(backing,0xcc,sizeof(backing));
}
int main(void) {
    struct process *p=malloc(sizeof(*p));assert(p);observed=NULL;reset();
    clear_process(p);assert(begins==1 && ends==1 && yields && !depth && !p->mapping_count);
    observed=p;p->address_space=0x1000;
    struct process_service_scope outer=process_service_begin();assert(depth==1);
    for(unsigned i=0;i<65;i++)assert(map_process_page(p,0x500000+i*4096,VMM_PAGE_USER|VMM_PAGE_WRITABLE));
    assert(checkpoints>=2 && p->mapping_count==65);
    for(unsigned i=4096;i<66*4096;i++)assert(backing[i]==0);
    unsigned before=yields;cleanup_process_address_space(p);
    assert(depth==1 && p->address_space==0 && !p->mapping_count && freed==65 && destroyed==1 && yields>=before+2);
    process_service_end(outer);assert(!depth && begins==ends);
    /* Unmap refusal preserves the original tracked-frame fallback and yields
     * only after removing that frame from the live prefix. */
    reset();p->address_space=0x1000;fail_unmap=true;
    outer=process_service_begin();
    for(unsigned i=0;i<33;i++)assert(map_process_page(p,0x600000+i*4096,VMM_PAGE_USER));
    process_service_end(outer);cleanup_process_address_space(p);
    assert(freed==33 && destroyed==1 && !depth && begins==ends);fail_unmap=false;
    /* Early boot/unavailable gate retains bounded synchronous fallback, and
     * must not end an ownership scope it never acquired. */
    reset();available=false;observed=NULL;clear_process(p);assert(!depth && !ends && !yields && begins==1);
    free(p);puts("process-service: PASS private publication, batched mapping retirement, inherited gate, fallback and refusal ownership");
}
