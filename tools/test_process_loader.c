/* Model a timer preemption during allocation: a second executable maps its
 * segment and stack before the first resumes. Every page must stay in its
 * owning address space, including identical virtual addresses. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
static struct process first, second;
static uint8_t frames[2048][4096];
static unsigned allocated;
static bool interleave,allocating,releasing;
static struct process *nested;
void *scheduler_current_user_context(void) { return NULL; }
void *scheduler_current_user_thread_context(void) { return NULL; }
void fpu_init_state(struct fpu_state *state) { memset(state,0,sizeof(*state)); }
uint64_t gdt_default_kernel_stack_top(void) {
    if(allocating) {
        allocating=false;
        assert(process_slots_busy[0] && !processes[0].allocated);
        nested=alloc_process("nested",true);
        assert(nested==&processes[1] && nested->allocated);
    }
    return 0x1234000;
}
void pmm_free_frame_range(uint64_t address,uint64_t count) {
    assert(address==0x1000 && count==1 && releasing);releasing=false;
    assert(process_slots_busy[0] && !processes[0].allocated);
    struct process *p=alloc_process("during-release",false);
    assert(p==&processes[2]);release_process(p);
}
static uint8_t image[9000];
static struct elf64_program_header segment={.type=PT_LOAD,.flags=PF_W,
    .vaddr=0x500000,.filesz=sizeof(image),.memsz=12288};
void console_printf(const char *format,...) { (void)format;assert(0); }
void pmm_free_frame(uint64_t address) { (void)address;assert(0); }
uint64_t pmm_alloc_frames_above_tagged(uint64_t count,uint64_t minimum,enum pmm_frame_tag tag) {
    assert(count==1 && minimum==PROCESS_USER_PAGE_MIN_PHYSICAL && tag==PMM_FRAME_USER);
    if(interleave) {
        interleave=false;
        assert(load_segment(&second,image,sizeof(image),&segment));
        assert(map_user_stack(&second,0x1000000));
    }
    assert(allocated+1<2048);return (uint64_t)++allocated*4096;
}
void *pmm_phys_to_virt(uint64_t physical) {
    assert(physical/4096<=allocated);return frames[physical/4096]+physical%4096;
}
bool vmm_map_page_in_space(uint64_t space,uint64_t virt,uint64_t physical,uint64_t flags) {
    assert(space==1 || space==2);assert(virt%4096==0 && physical%4096==0);
    assert(flags&VMM_PAGE_USER);return true;
}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t virt,uint64_t *physical) {
    struct process *p=space==1?&first:&second;
    for(unsigned i=0;i<p->mapping_count;i++) if(p->mappings[i].virtual_address==(virt&~4095ull)) {
        *physical=p->mappings[i].physical_address+(virt&4095);return true;
    }
    return false;
}
int main(void) {
    allocating=true;
    struct process *p=alloc_process("first",false);
    assert(p==&processes[0] && p->allocated && p->active && p->pid==1 && nested->pid==2);
    assert(!process_slots_busy[0] && !process_slots_busy[1]);
    p->kernel_stack_physical=0x1000;p->kernel_stack_frames=1;releasing=true;
    release_process(p);assert(!releasing && !process_slots_busy[0]);
    p=alloc_process("reused",false);assert(p==&processes[0] && p->pid==4 && !p->kernel_stack_physical);
    release_process(p);release_process(nested);
    first.address_space=1;second.address_space=2;
    for(unsigned i=0;i<sizeof(image);i++)image[i]=(uint8_t)(i*31+7);
    interleave=true;
    assert(load_segment(&first,image,sizeof(image),&segment));
    assert(map_user_stack(&first,0x1000000));
    assert(first.mapping_count==3+USER_STACK_PAGES && second.mapping_count==first.mapping_count);
    for(unsigned p=0;p<2;p++) for(unsigned i=0;i<sizeof(image);i++) {
        uint64_t physical;assert(vmm_virt_to_phys_in_space(p+1,segment.vaddr+i,&physical));
        assert(*(uint8_t*)pmm_phys_to_virt(physical)==image[i]);
    }
    for(unsigned i=0;i<first.mapping_count;i++)for(unsigned j=0;j<second.mapping_count;j++)
        assert(first.mappings[i].physical_address!=second.mappings[j].physical_address);
    puts("process-loader: PASS reserved process slots, publication/release interleaving, executable segment and stack ownership");
}
