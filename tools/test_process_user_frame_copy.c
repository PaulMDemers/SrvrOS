/* Real process preflight; low-level copy/proof are explicit host mocks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
static uint64_t test_flags;
static uint64_t process_test_irq_save(void) {uint64_t old=test_flags;test_flags&=~0x200ull;return old;}
static void process_test_irq_restore(uint64_t old) {test_flags=old;}
static uint64_t process_test_rflags(void) {return test_flags;}
#include "../kernel/src/process.c"
static struct scheduler_user_return_observation observation;
static struct scheduler_user_worker_stack worker_stack;
static bool observation_ok=true,policy_ok=true,proof_ok=true,copy_ok=true,worker_ok=true;
static bool policy_success_pending;
static unsigned policies,proofs,copies,worker_queries;
static unsigned char destination_bytes[8192];
static struct process original;
static struct fpu_state kernel_fpu;
bool scheduler_observe_user_return(struct scheduler_user_return_observation *out) {
    if(!observation_ok || (test_flags&0x200)) return false;*out=observation;return true;
}
bool scheduler_observe_user_worker_stack(struct scheduler_user_worker_stack *out) {
    worker_queries++;if(!worker_ok)return false;*out=worker_stack;return true;
}
bool fpu_available(void) {return false;}
uint64_t vmm_kernel_address_space(void) {return 0x1000;}
void *pmm_phys_to_virt(uint64_t physical) {return (void *)(uintptr_t)(0xffff800000000000ull+physical);}
bool vmm_user_frame_copy_policy_current(uint64_t root) {assert(root==0x2000 && !(test_flags&0x200));policies++;policy_success_pending=policy_ok;return policy_ok;}
bool fpu_validate_user_trap(const struct isr_frame *frame) {assert(frame && policy_success_pending);policy_success_pending=false;proofs++;return proof_ok;}
bool vmm_copy_user_frame_current(uint64_t root,uint64_t dst,const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t count,
    const struct isr_frame *frame,const struct fpu_state *user,const struct fpu_state *kernel) {
    assert(root==0x2000 && frame && kernel==&kernel_fpu && user==observation.user_fpu);
    assert(stack->frames==32 && stack->base==0xffff800004000000ull && stack->physical==0x4000000 && !(test_flags&0x200));
    assert(observation.kernel_stack_top==stack->base+32*4096);
    assert(dst==0x1ff0 && length==32 && count==2);
    assert(pages[0].virtual_address==0x1000 && pages[0].physical_address==0x8000);
    assert(pages[1].virtual_address==0x2000 && pages[1].physical_address==0x9000);
    assert(pages[0].flags==(VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE));
    assert(pages[1].flags==(VMM_PAGE_PRESENT|VMM_PAGE_USER|VMM_PAGE_WRITABLE));
    copies++;if(!copy_ok)return false;
    memcpy(destination_bytes+4080,source->bytes,length);return true;
}
static void setup(struct process *p) {
    memset(p,0,sizeof(*p));p->allocated=p->active=true;p->image_generation=1;p->address_space=0x2000;
    p->kernel_stack_physical=0x4000000;p->kernel_stack_frames=32;
    p->kernel_stack=pmm_phys_to_virt(p->kernel_stack_physical);p->kernel_stack_top=(uint64_t)p->kernel_stack+PROCESS_KERNEL_STACK_SIZE;
    p->mapping_count=2;p->mappings[0]=(struct process_mapping){0x1000,0x8000,VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE};
    p->mappings[1]=(struct process_mapping){0x2000,0x9000,VMM_PAGE_PRESENT|VMM_PAGE_USER|VMM_PAGE_WRITABLE};
    observation=(struct scheduler_user_return_observation){.thread_identity=7,.process=p,.address_space=0x2000,
        .image_generation=1,.user_fpu=&p->fpu,.kernel_fpu=&kernel_fpu,.kernel_stack_top=p->kernel_stack_top};
    next_image_generation=1;memset(destination_bytes,0xa5,sizeof(destination_bytes));
    observation_ok=policy_ok=proof_ok=copy_ok=worker_ok=true;policy_success_pending=false;test_flags=0;
}
static void refused(struct process *p,const struct isr_frame *frame,const struct user_frame_source *source) {
    unsigned char before[sizeof(destination_bytes)];memcpy(before,destination_bytes,sizeof(before));memcpy(&original,p,sizeof(original));
    unsigned prior=copies;assert(!process_copy_user_frame_current(frame,source,0x1ff0,32));
    assert(!memcmp(p,&original,sizeof(original)) && !memcmp(before,destination_bytes,sizeof(before)) && copies==prior);
}
int main(void) {
    struct process *p=&processes[0];struct isr_frame frame={0};struct user_frame_source source={{0}};
    for(unsigned i=0;i<32;i++)source.bytes[i]=(uint8_t)(i+1);
    setup(p);struct process_user_binding initial;
    assert(process_resolve_user_binding(p,NULL,1,0x2000,&initial));
    assert(initial.kernel_stack_base==0xffff800004000000ull && initial.kernel_stack_physical==0x4000000 && initial.kernel_stack_frames==32);
    unsigned n=proofs;policy_ok=false;refused(p,&frame,&source);assert(proofs==n);policy_ok=true;
    proof_ok=false;refused(p,&frame,&source);proof_ok=true;
    test_flags=0x200;refused(p,&frame,&source);test_flags=0;
    p->killed=true;refused(p,&frame,&source);p->killed=false;p->exiting=true;refused(p,&frame,&source);p->exiting=false;
    p->main_thread_exited=true;refused(p,&frame,&source);p->main_thread_exited=false;
    p->mapping_count=PROCESS_MAX_MAPPINGS+1;refused(p,&frame,&source);p->mapping_count=2;
    p->mappings[1].virtual_address=0x1000;refused(p,&frame,&source);p->mappings[1].virtual_address=0x2000;
    p->mappings[1].physical_address=0x8000;refused(p,&frame,&source);p->mappings[1].physical_address=0x9000;
    p->mappings[2]=(struct process_mapping){0x5000,0x8000,VMM_PAGE_USER|VMM_PAGE_WRITABLE};p->mapping_count=3;
    refused(p,&frame,&source);p->mapping_count=2;
    p->mappings[1].flags|=VMM_PAGE_CACHE_DISABLE;refused(p,&frame,&source);p->mappings[1].flags&=~VMM_PAGE_CACHE_DISABLE;
    p->mappings[1].flags&=~VMM_PAGE_WRITABLE;refused(p,&frame,&source);p->mappings[1].flags|=VMM_PAGE_WRITABLE;
    assert(!process_copy_user_frame_current(&frame,&source,0,32));
    assert(!process_copy_user_frame_current(&frame,&source,0x1ff0,0));
    assert(!process_copy_user_frame_current(&frame,&source,0x1ff0,1025));
    assert(!process_copy_user_frame_current(&frame,&source,0x7fffffffffff,2));
    copy_ok=false;unsigned prior=copies;assert(!process_copy_user_frame_current(&frame,&source,0x1ff0,32));assert(copies==prior+1);
    for(unsigned i=0;i<sizeof(destination_bytes);i++)assert(destination_bytes[i]==0xa5);
    copy_ok=true;assert(process_copy_user_frame_current(&frame,&source,0x1ff0,32));
    assert(!memcmp(destination_bytes+4080,source.bytes,32));
    for(unsigned i=0;i<4080;i++)assert(destination_bytes[i]==0xa5);
    for(unsigned i=4112;i<sizeof(destination_bytes);i++)assert(destination_bytes[i]==0xa5);
    for(unsigned i=0;i<32;i++)assert(source.bytes[i]==(uint8_t)(i+1));
    for(unsigned i=32;i<sizeof(source.bytes);i++)assert(!source.bytes[i]);
    struct process_user_thread *worker=&p->user_threads[0];*worker=(struct process_user_thread){.used=true,.active=true,.tid=2,.owner=p};
    observation.user_thread=worker;observation.user_fpu=&worker->fpu;observation.kernel_stack_top=0xffff800004020000ull;
    worker_stack=(struct scheduler_user_worker_stack){7,0xffff800004000000ull,0x4000000,32,observation.kernel_stack_top};
    worker->active=false;refused(p,&frame,&source);worker->active=true;
    worker->owner=NULL;refused(p,&frame,&source);worker->owner=p;
    observation.user_fs_base=8;refused(p,&frame,&source);observation.user_fs_base=0;
    observation.image_generation=2;refused(p,&frame,&source);observation.image_generation=1;
    observation.address_space=0x3000;refused(p,&frame,&source);observation.address_space=0x2000;
    worker_ok=false;refused(p,&frame,&source);worker_ok=true;
    worker_stack.thread_identity=8;refused(p,&frame,&source);worker_stack.thread_identity=7;
    assert(process_copy_user_frame_current(&frame,&source,0x1ff0,32));assert(worker_queries>=3);
    for(unsigned i=0;i<4080;i++)assert(destination_bytes[i]==0xa5);
    for(unsigned i=4112;i<sizeof(destination_bytes);i++)assert(destination_bytes[i]==0xa5);
    for(unsigned i=0;i<32;i++)assert(source.bytes[i]==(uint8_t)(i+1));
    for(unsigned i=32;i<sizeof(source.bytes);i++)assert(!source.bytes[i]);
    /* At most four complete mapping scans run with IRQs excluded for a
     * two-page frame: 4 * PROCESS_MAX_MAPPINGS = 262144 record visits.
     * This correctness test does not measure maximum guest latency. */
    for(uint64_t i=0;i<PROCESS_MAX_MAPPINGS;i++)
        p->mappings[i]=(struct process_mapping){0x10000000ull+i*4096,0x20000000ull+i*4096,
            VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE};
    p->mapping_count=PROCESS_MAX_MAPPINGS;
    p->mappings[PROCESS_MAX_MAPPINGS-3]=(struct process_mapping){0x1000,0x8000,
        VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE};
    p->mappings[PROCESS_MAX_MAPPINGS-2]=(struct process_mapping){0x2000,0x9000,
        VMM_PAGE_PRESENT|VMM_PAGE_USER|VMM_PAGE_WRITABLE};
    memset(destination_bytes,0xa5,sizeof(destination_bytes));
    assert(process_copy_user_frame_current(&frame,&source,0x1ff0,32));
    assert(!memcmp(destination_bytes+4080,source.bytes,32));
    for(unsigned i=0;i<4080;i++)assert(destination_bytes[i]==0xa5);
    for(unsigned i=4112;i<sizeof(destination_bytes);i++)assert(destination_bytes[i]==0xa5);
    struct process_mapping last=p->mappings[PROCESS_MAX_MAPPINGS-1];
    p->mappings[PROCESS_MAX_MAPPINGS-1]=(struct process_mapping){0x1000,last.physical_address,
        VMM_PAGE_USER|VMM_PAGE_WRITABLE};
    refused(p,&frame,&source);
    p->mappings[PROCESS_MAX_MAPPINGS-1]=last;
    p->mappings[PROCESS_MAX_MAPPINGS-1].physical_address=0x8000;
    refused(p,&frame,&source);
    p->mappings[PROCESS_MAX_MAPPINGS-1]=last;
    puts("process-user-frame-copy: PASS (unwired preflight; VMM/proof mocked)");
    return 0;
}
