/* Actual process image metadata and resolver; link with section GC. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
static uint64_t test_flags;
static uint64_t process_test_irq_save(void) { uint64_t old=test_flags;test_flags&=~(1ull<<9);return old; }
static void process_test_irq_restore(uint64_t old) { test_flags=old; }
static uint64_t process_test_rflags(void) {return test_flags;}
#include "../kernel/src/process.c"
static bool prepared=true;
static unsigned prepare_calls;
bool vmm_prepare_user_kernel_mappings(uint64_t root) {assert(!(test_flags&(1ull<<9)));prepare_calls++;return root && prepared;}
uint64_t vmm_kernel_address_space(void) {return 0x1000;}
bool fpu_available(void) {return false;}
void *pmm_phys_to_virt(uint64_t physical) {return (void *)(uintptr_t)(0xffff800000000000ull+physical);}
int main(void) {
    struct process *p=&processes[0];
    p->allocated=p->active=true;p->address_space=0x2000;
    p->kernel_stack_physical=0x4000000;p->kernel_stack_frames=PROCESS_KERNEL_STACK_SIZE/PAGE_SIZE;
    p->kernel_stack=pmm_phys_to_virt(p->kernel_stack_physical);
    p->kernel_stack_top=(uint64_t)p->kernel_stack+PROCESS_KERNEL_STACK_SIZE;
    struct process_user_binding out={.fs_base=0xabcdef};
    test_flags=1ull<<9;
    assert(!process_resolve_user_binding(p,NULL,1,p->address_space,&out) && out.fs_base==0xabcdef);
    assert(process_prepare_image(p)==0 && p->image_generation==1 && test_flags==(1ull<<9));
    test_flags=0;
    unsigned calls=prepare_calls;
    assert(process_prepare_image(p)==SRV_ERR_IO && next_image_generation==1 && prepare_calls==calls);
    assert(process_resolve_user_binding(p,NULL,1,p->address_space,&out));
    assert(out.user_fpu==&p->fpu && !out.fs_base && out.kernel_stack_top==p->kernel_stack_top && out.new_worker_allowed);
    struct process_user_thread *t=&p->user_threads[0];
    *t=(struct process_user_thread){.used=true,.active=true,.tid=2,.owner=p};
    assert(process_resolve_user_binding(p,t,1,p->address_space,&out));
    assert(out.user_fpu==&t->fpu && !out.kernel_stack_top);
    assert(process_admit_user_worker(p,t,1,p->address_space,&out));
    out.fs_base=0xabcdef;
    assert(!process_resolve_user_binding(p,(void *)1,1,p->address_space,&out) && out.fs_base==0xabcdef);
    assert(!process_resolve_user_binding((void *)1,NULL,1,p->address_space,&out));
    assert(!process_resolve_user_binding(p,t,2,p->address_space,&out));
    t->active=false;assert(!process_resolve_user_binding(p,t,1,p->address_space,&out));
    t->active=true;
    p->tls_memsz=32;p->tls_filesz=12;p->tls_align=16;
    p->tls_base=USER_TLS_BASE;p->tls_thread_pointer=USER_TLS_BASE+32;
    t->tls_base=USER_TLS_BASE+PAGE_SIZE;t->tls_thread_pointer=t->tls_base+32;
    assert(process_resolve_user_binding(p,t,1,p->address_space,&out) && out.fs_base==t->tls_thread_pointer);
    t->tls_thread_pointer++;out.fs_base=0xabcdef;
    assert(!process_resolve_user_binding(p,t,1,p->address_space,&out) && out.fs_base==0xabcdef);
    t->tls_thread_pointer--;p->tls_align=3;
    assert(!process_resolve_user_binding(p,NULL,1,p->address_space,&out));
    p->tls_align=16;
    p->killed=p->exiting=p->main_thread_exited=true;
    assert(process_resolve_user_binding(p,NULL,1,p->address_space,&out) && !out.new_worker_allowed);
    assert(process_resolve_user_binding(p,t,1,p->address_space,&out) && !out.new_worker_allowed);
    out.fs_base=0xabcdef;
    assert(!process_admit_user_worker(p,t,1,p->address_space,&out) && out.fs_base==0xabcdef);
    uint64_t physical=p->kernel_stack_physical;
    p->kernel_stack_physical=0x10000000000000ull;
    assert(!process_resolve_user_binding(p,NULL,1,p->address_space,&out));
    p->kernel_stack_physical=physical;
    p->fpu.active_user_trap=(void *)1;out.fs_base=0xabcdef;
    assert(!process_resolve_user_binding(p,NULL,1,p->address_space,&out) && out.fs_base==0xabcdef);
    p->fpu.active_user_trap=NULL;
    next_image_generation=0;assert(!process_resolve_user_binding(p,NULL,1,p->address_space,&out));
    struct process staged={.address_space=0x3000};
    prepared=false;assert(process_prepare_image(&staged)==SRV_ERR_IO && !staged.image_generation);
    prepared=true;next_image_generation=UINT64_MAX;
    assert(process_prepare_image(&staged)==SRV_ERR_AGAIN && !staged.image_generation);
    puts("process-image-metadata: ok");
}
