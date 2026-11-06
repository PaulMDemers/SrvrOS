/* Actual-source staged spawn image preflight and cleanup. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
static uint64_t simulated_flags=1ull<<9;
static uint64_t process_test_irq_save(void) {uint64_t f=simulated_flags;simulated_flags&=~(1ull<<9);return f;}
static void process_test_irq_restore(uint64_t f) {simulated_flags=f;}
static uint64_t process_test_rflags(void) {return simulated_flags;}
#include "../kernel/src/process.c"
static struct process *current;
static struct process parent_snapshot;
static struct process_user_thread *worker;
static uint64_t bound_cr3=0xabc000,bound_fs=0x123000,bound_tss=0x456000;
static struct fpu_state *bound_fpu;
static unsigned binding_calls, admissions, allocated,freed,stack_frees,destroyed,elf_reads,elf_releases;
static _Alignas(4096) unsigned char pages[1024][4096],kernel_stack[PROCESS_KERNEL_STACK_SIZE];
static bool freed_page[1024];
static bool exec_success,exec_cleared,exec_old_retired,exec_rebound,fatal_exit_visited,user_entry_visited,earlier_fatal_injected;
static unsigned exec_fpu_resets,exec_cleanup_observations;
static struct process *exec_parent;
static void *kernel_association;
static jmp_buf exec_entry;
static bool main_exit_test,worker_exit_test;
static struct process_user_thread *exiting_worker;
static jmp_buf worker_exit_return;
static unsigned thread_invalidations;
static bool fork_publish_expected;
static struct process *published_fork;
static void observe_old_retirement(void) {
    assert(exec_success && exec_cleared && !current && !kernel_association);
    assert(!exec_parent->image_generation && bound_cr3==0x2000 && !bound_fs && !bound_fpu);
    exec_cleanup_observations++;
}
bool scheduler_io_active(void) {return false;}
void keyboard_wake_waiters(void) {}
void net_process_wake(struct process *p) { (void)p; }
void pty_wake(void) {}
void scheduler_clear_user_context(void) {
    if(worker_exit_test) {
        assert(!(simulated_flags&(1ull<<9)) && current==exec_parent && worker==exiting_worker);
        assert(!worker->active && kernel_association && exec_parent->image_generation==77);
        assert(!worker->signal_state.stack.depth && !worker->signal_state.stack.last_token);
        kernel_association=NULL;current=NULL;worker=NULL;
        bound_cr3=0x2000;bound_fs=0;bound_fpu=NULL;return;
    }
    assert(exec_success && !exec_cleared && !(simulated_flags&(1ull<<9)));
    assert(current==exec_parent && exec_parent->image_generation==41 && kernel_association);
    assert(!process_signal_records_active(exec_parent) && exec_parent->usr1_action.mode==SIGNAL_ACTION_DEFAULT);
    exec_cleared=true;kernel_association=NULL;current=NULL;
    bound_cr3=0x2000;bound_fs=0;bound_fpu=NULL;bound_tss=0x456000;
}
void scheduler_invalidate_current_user_trap(void) {
    assert(!(simulated_flags&(1ull<<9)));
    uint64_t flags=process_test_irq_save();
    assert(main_exit_test && current==exec_parent && exec_parent->image_generation==77 && kernel_association);
    kernel_association=NULL;thread_invalidations++;process_test_irq_restore(flags);
}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *p) {(void)q;(void)fn;(void)p;abort();}
void scheduler_yield(void) {abort();}
void scheduler_io_wait(void) {
    const char *mode=getenv("SRVROS_EXEC_PENDING_TEST");
    assert(exec_success && mode && (!strcmp(mode,"fatal") || !strcmp(mode,"earlier")));
    uint64_t usr1=1ull<<SRV_SIGNAL_USR1,usr2=1ull<<SRV_SIGNAL_USR2;
    uint64_t ignored=(1ull<<SRV_SIGNAL_CHLD)|(1ull<<SRV_SIGNAL_WINCH);
    uint64_t retained=(1ull<<SRV_SIGNAL_ALRM)|(1ull<<SRV_SIGNAL_HUP);
    bool earlier=!strcmp(mode,"earlier");
    assert(!(simulated_flags&(1ull<<9)) && current==exec_parent && exec_rebound &&
        current->image_generation==42 && bound_cr3==3 && current->killed &&
        current->kill_signal==(earlier?SRV_SIGNAL_TERM:SRV_SIGNAL_USR1) &&
        process_current_kill_status()==(earlier?143u:138u));
    assert(((current->main_pending_signals&usr1)!=0)==earlier && (current->pending_signals&usr1) &&
        (current->pending_signals&usr2) &&
        (((current->main_pending_signals&ignored)!=0)==earlier) &&
        (((current->pending_signals&ignored)!=0)==earlier) &&
        (current->main_pending_signals&retained)==retained &&
        (current->pending_signals&retained)==retained && !user_entry_visited && earlier_fatal_injected==earlier);
    fatal_exit_visited=true;simulated_flags|=1ull<<9;longjmp(exec_entry,1);
}
void scheduler_io_set_user_safe(bool safe) {assert(exec_success && !safe && !(simulated_flags&(1ull<<9)));}
void scheduler_kill_user_threads(void *p,void *except) {(void)p;(void)except;abort();}
void audio_process_cleanup(uint64_t pid) {(void)pid;abort();}
void gui_process_cleanup(uint64_t pid) {(void)pid;abort();}
void console_release_framebuffer_mute_owner(uint64_t pid) {(void)pid;abort();}
void net_process_cleanup(struct process *p) {(void)p;abort();}
void process_context_restore(struct process_context *p,uint64_t value) {(void)p;(void)value;abort();}
void scheduler_exit_current(void) {
    assert(worker_exit_test && simulated_flags==(1ull<<9) && !kernel_association && !current);
    assert(!exiting_worker->used && !exiting_worker->tls_base && !exiting_worker->tls_thread_pointer);
    longjmp(worker_exit_return,1);
}
void usermode_resume(const struct isr_frame *f,uint64_t fs) { (void)f;(void)fs;abort(); }
void *scheduler_current_user_context(void) { return current; }
void *scheduler_current_user_thread_context(void) { return worker; }
void fpu_init_state(struct fpu_state *state) {
    if(exec_success && state==&exec_parent->fpu) {
        assert(exec_old_retired && !current && !kernel_association);exec_fpu_resets++;
    }
    memset(state,0,sizeof(*state));
}
void fpu_invalidate_user_trap(struct fpu_state *state) { if(state)state->active_user_trap=NULL; }
uint64_t gdt_default_kernel_stack_top(void) { return 0x456000; }
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue;if(main_exit_test)assert(!kernel_association); }
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t minimum,enum pmm_frame_tag tag) {
    if(tag==PMM_FRAME_KERNEL_STACK) { assert(n==PROCESS_KERNEL_STACK_SIZE/4096 && minimum==PROCESS_KERNEL_STACK_MIN_PHYSICAL);return 0x1000000; }
    assert(n==1 && tag==PMM_FRAME_USER && minimum==PROCESS_USER_PAGE_MIN_PHYSICAL);
    assert(allocated+1<1024);return (uint64_t)++allocated*4096;
}
void *pmm_phys_to_virt(uint64_t p) {
    if(p==0x1000000)return kernel_stack;
    assert(p/4096<=allocated);return pages[p/4096]+p%4096;
}
void pmm_free_frame(uint64_t p) {
    if(exec_success) {assert(p==4096);observe_old_retirement();}
    if(worker_exit_test)assert(p==4096 && !current && !kernel_association && exec_parent->image_generation==77 && !(simulated_flags&(1ull<<9)));
    assert(p && p%4096==0 && p/4096<=allocated && !freed_page[p/4096]);freed_page[p/4096]=true;freed++;
}
void pmm_free_frame_range(uint64_t p,uint64_t n) { assert(p==0x1000000 && n==PROCESS_KERNEL_STACK_SIZE/4096);stack_frees++; }
static bool image_prepare_ok;
static unsigned image_prepare_calls;
bool vmm_prepare_user_kernel_mappings(uint64_t root) { assert(root==3);image_prepare_calls++;return image_prepare_ok; }
static struct process *mapped_image;
bool vmm_create_address_space(uint64_t *out) { mapped_image=(struct process *)((char *)out - offsetof(struct process,address_space));*out=3;return true; }
void vmm_destroy_address_space(uint64_t space) {
    if(exec_success && space==0xabc000) {
        observe_old_retirement();assert(freed_page[1] && !exec_parent->mapping_count);exec_old_retired=true;
    } else assert(space==3);
    destroyed++;
}
bool vmm_map_page_in_space(uint64_t space,uint64_t virt,uint64_t physical,uint64_t flags) { assert(space==3 && virt && physical && (flags&VMM_PAGE_USER));return true; }
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t virt,uint64_t *physical) {
    assert(space==3 || (exec_success && space==0xabc000));
    struct process *child=space==3 ? mapped_image : exec_parent;
    for(unsigned i=0;i<child->mapping_count;i++)if(child->mappings[i].virtual_address==(virt&~4095ull)) { *physical=child->mappings[i].physical_address+(virt&4095);return true; }
    return false;
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t virt,uint64_t *physical) { return vmm_virt_to_phys_in_space(space,virt,physical); }
void console_printf(const char *s,...) {
    const char *mode=getenv("SRVROS_EXEC_PENDING_TEST");
    if(exec_success && mode && !strcmp(mode,"earlier") &&
        !strcmp(s,"exec: entering pid=%u %s entry=%x stack=%x\n")) {
        assert(current==exec_parent && exec_rebound && !current->killed);
        current->killed=true;current->kill_signal=SRV_SIGNAL_TERM;earlier_fatal_injected=true;
    }
}
void console_write(const char *s) { (void)s; }
bool exfat_write_file_mode(const char *p,const uint8_t *d,uint64_t n,uint64_t m) { (void)p;(void)d;(void)n;(void)m;abort(); }
bool exfat_write_file_retryable(const char *p,const uint8_t *d,uint64_t n,bool *r) { (void)p;(void)d;(void)n;(void)r;abort(); }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) {
    if(exec_success && p==mapped_image) {
        assert(exec_old_retired && exec_fpu_resets==1 && !current && !kernel_association);
        mapped_image=exec_parent; /* move_process_image transferred the backing. */
    }
    free(p);
}
int64_t net_close(uint64_t h) { (void)h;abort(); }
int64_t net_hold(uint64_t h) { (void)h;abort(); }
bool pty_ref(uint64_t h,bool m) { (void)h;(void)m;abort(); }
void pty_close(uint64_t h,bool m) { (void)h;(void)m;abort(); }
int64_t pty_set_flags(uint64_t h,bool m,uint64_t f) { (void)h;(void)m;(void)f;abort(); }
struct pty_model *pty_state(uint64_t h) { (void)h;abort(); }
uint64_t process_context_save(struct process_context *context) { (void)context;abort(); }
void usermode_enter(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f) {
    assert(exec_success && exec_rebound && exec_old_retired && exec_fpu_resets==1);
    assert(current==exec_parent && current->image_generation==42 && !kernel_association);
    assert(!(simulated_flags&(1ull<<9)));user_entry_visited=true;
    if (getenv("SRVROS_EXEC_PENDING_TEST")) {
        fprintf(stderr,"exec-pending negative control: reached user entry killed=%d signal=%llu local=%llx shared=%llx\n",
            current->killed,(unsigned long long)current->kill_signal,
            (unsigned long long)current->main_pending_signals,(unsigned long long)current->pending_signals);
        const char *mode=getenv("SRVROS_EXEC_PENDING_TEST");
        if (!strcmp(mode,"ignored")) {
            uint64_t ignored=(1ull<<SRV_SIGNAL_CHLD)|(1ull<<SRV_SIGNAL_WINCH);
            uint64_t retained=(1ull<<SRV_SIGNAL_ALRM)|(1ull<<SRV_SIGNAL_HUP);
            assert(!current->killed && !(current->main_pending_signals&ignored) &&
                !(current->pending_signals&ignored) &&
                (current->main_pending_signals&retained)==retained &&
                (current->pending_signals&retained)==retained &&
                "default-ignored bits must clear; blocked and ignored dispositions stay pending");
        } else {
            assert(!"unblocked pending default signal must terminate before user entry");
        }
    }
    assert(a==current->entry && b==current->stack_top && c==current->argc && d==current->argv && e==current->envp && f==current->tls_thread_pointer);
    simulated_flags|=1ull<<9;longjmp(exec_entry,1);
}
static uint8_t image[9000];
static const struct vfs_node executable={.used=true,.type=VFS_NODE_FILE,.metadata={.mode=0755}};
static const struct vfs_node directory={.used=true,.type=VFS_NODE_DIRECTORY};
uint64_t vfs_count(void) { return 0; }
const struct vfs_node *vfs_node_at(uint64_t i) { (void)i;return NULL; }
void vfs_set_retire_callback(vfs_retire_fn callback) { (void)callback; }
const struct vfs_node *vfs_lookup(const char *path) { return !strcmp(path,"/") || !strcmp(path,"/fat") || !strcmp(path,"/fat/bin") ? &directory : &executable; }
bool vfs_read_all(const struct vfs_node *node,const uint8_t **data,uint64_t *size) { assert(node==&executable);elf_reads++;*data=image;*size=sizeof(image);return true; }
void vfs_release_data(const struct vfs_node *node,const uint8_t *data) { assert(node==&executable && data==image);elf_releases++; }
void scheduler_set_user_context(void *p,uint64_t cr3,uint64_t tss,uint64_t generation) {
    assert(exec_success && exec_cleared && exec_old_retired && !current && !kernel_association);
    assert(p==exec_parent && cr3==3 && generation==42 && generation==exec_parent->image_generation);
    assert(exec_fpu_resets==1 && exec_parent->mapping_count>=USER_STACK_PAGES+1 && !exec_parent->files[3].used);
    assert(!exec_parent->fpu.active_user_trap);
    binding_calls++;current=p;bound_cr3=cr3;bound_tss=tss;
    bound_fs=exec_parent->tls_thread_pointer;bound_fpu=&exec_parent->fpu;exec_rebound=true;
}
bool scheduler_spawn(const char *name,scheduler_thread_fn entry,void *p) {
    (void)entry;admissions++;
    if(fork_publish_expected) {
        struct process *child=p;assert(!strcmp(name,"fork") && child!=current && child->allocated && child->active);
        assert(!process_signal_records_active(child) && !child->main_signal_state.stack.last_token);
        assert(!memcmp(&child->usr1_action,&current->usr1_action,sizeof(child->usr1_action)));
        assert(child->usr1_action_generation==current->usr1_action_generation && child->main_signal_mask==current->main_signal_mask);
        assert(child->signal_catch_mask==current->signal_catch_mask && child->signal_ignore_mask==current->signal_ignore_mask);
        assert(!child->pending_signals && !child->main_pending_signals && child->image_generation!=current->image_generation);
        for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++) assert(!child->user_threads[i].used && !child->user_threads[i].signal_state.stack.last_token);
        published_fork=child;return true;
    }
    return false;
}
int main(void) {
    struct elf64_header *h=(struct elf64_header *)image;
    h->ident[0]=ELFMAG0;h->ident[1]=ELFMAG1;h->ident[2]=ELFMAG2;h->ident[3]=ELFMAG3;
    h->ident[4]=ELFCLASS64;h->ident[5]=ELFDATA2LSB;h->ident[6]=EV_CURRENT;
    h->type=ET_EXEC;h->machine=EM_X86_64;h->version=EV_CURRENT;h->entry=0x500000;
    h->phoff=sizeof(*h);h->phentsize=sizeof(struct elf64_program_header);h->phnum=1;
    struct elf64_program_header *ph=(void *)(image+h->phoff);
    *ph=(struct elf64_program_header){.type=PT_LOAD,.flags=PF_X,.vaddr=0x500000,.offset=4096,.filesz=3,.memsz=4096,.align=4096};
    image[4096]=0xaa;image[4097]=0xbb;image[4098]=0xcc;
    struct process *parent=&processes[0];parent->active=parent->allocated=true;parent->pid=10;
    parent->address_space=bound_cr3;parent->kernel_stack_top=bound_tss;
    memset(&parent->fpu,0x39,sizeof(parent->fpu));
    parent->fpu.initialized=true;parent->fpu.active_user_trap=NULL;
    worker=&parent->user_threads[0];*worker=(struct process_user_thread){.used=true,.active=true,.owner=parent,.tid=2,.tls_thread_pointer=bound_fs,.stack_top=0x900000};
    memset(&worker->fpu,0x5a,sizeof(worker->fpu));
    worker->fpu.initialized=true;worker->fpu.active_user_trap=NULL;
    bound_fpu=&worker->fpu;current=parent;kernel_association=(void *)0xcafe;
    memcpy(&parent_snapshot,parent,sizeof(*parent));
    struct process_user_thread before=*worker;char *argv[]={"echo",NULL};
    int64_t result=process_spawn_exec("/fat/bin/echo",0,(const char *const *)argv,0,NULL,true,0,1,2,0,false,NULL,0,0,0,0);
    assert(result==SRV_ERR_NOMEM && elf_reads==1 && elf_releases==1);
    assert(allocated>=USER_STACK_PAGES+1 && freed==allocated && destroyed==1 && stack_frees==1);
    assert(!binding_calls && !admissions && current==parent && worker==&parent->user_threads[0]);
    assert(bound_cr3==0xabc000 && bound_fs==0x123000 && bound_tss==0x456000 && bound_fpu==&worker->fpu);
    assert(!memcmp(worker,&before,sizeof(before)) && !memcmp(parent,&parent_snapshot,sizeof(*parent)));
    assert(!processes[1].allocated && !process_slots_busy[1]);
    assert(!image_prepare_calls && next_image_generation==0);
    allocated=freed=0;memset(freed_page,0,sizeof(freed_page));
    result=process_spawn_exec("/fat/bin/echo",1,(const char *const *)argv,0,NULL,true,0,1,2,0,false,NULL,0,0,0,0);
    assert(result==SRV_ERR_IO && image_prepare_calls==1 && next_image_generation==0);
    assert(elf_reads==2 && elf_releases==2 && destroyed==2 && stack_frees==2);
    assert(!binding_calls && !admissions && current==parent && worker==&parent->user_threads[0]);
    assert(!memcmp(parent,&parent_snapshot,sizeof(*parent)) && !processes[1].allocated);
    allocated=freed=0;memset(freed_page,0,sizeof(freed_page));
    image_prepare_ok=true;next_image_generation=UINT64_MAX;
    result=process_spawn_exec("/fat/bin/echo",1,(const char *const *)argv,0,NULL,true,0,1,2,0,false,NULL,0,0,0,0);
    assert(result==SRV_ERR_AGAIN && image_prepare_calls==2 && next_image_generation==UINT64_MAX);
    assert(elf_reads==3 && elf_releases==3 && destroyed==3 && stack_frees==3);
    assert(!binding_calls && !admissions && !memcmp(parent,&parent_snapshot,sizeof(*parent)) && !processes[1].allocated);
    allocated=freed=0;memset(freed_page,0,sizeof(freed_page));
    next_image_generation=41;worker=NULL;memset(parent->user_threads,0,sizeof(parent->user_threads));
    parent->files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=0};
    parent->files[1]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=1};
    parent->image_generation=41;memcpy(&parent_snapshot,parent,sizeof(*parent));
    parent->usr1_action=(struct signal_action_descriptor){.mode=SIGNAL_ACTION_INFO,.handler=0x500000,.trampoline=0x500100};
    parent->usr1_action_generation=9;parent->signal_catch_mask=1ull<<SRV_SIGNAL_USR1;
    if (getenv("SRVROS_EXEC_PENDING_TEST")) {
        uint64_t usr1=1ull<<SRV_SIGNAL_USR1,usr2=1ull<<SRV_SIGNAL_USR2;
        uint64_t ignored=(1ull<<SRV_SIGNAL_CHLD)|(1ull<<SRV_SIGNAL_WINCH);
        uint64_t blocked=1ull<<SRV_SIGNAL_ALRM,retained_ignore=1ull<<SRV_SIGNAL_HUP;
        parent->signal_catch_mask|=usr2;
        parent->signal_ignore_mask|=retained_ignore;
        parent->main_signal_mask|=blocked;
        bool fatal=strcmp(getenv("SRVROS_EXEC_PENDING_TEST"),"ignored")!=0;
        parent->main_pending_signals=(fatal?usr1:0)|ignored|blocked|retained_ignore;
        parent->pending_signals=(fatal?(usr1|usr2):0)|ignored|blocked|retained_ignore;
    }
    memcpy(&parent_snapshot,parent,sizeof(*parent));
    result=process_exec_replace("/fat/bin/echo",1,(const char *const *)argv,0,NULL,1,500,-1,0,0,0);
    assert(result==SRV_ERR_BADF && next_image_generation==42 && image_prepare_calls==3);
    assert(!binding_calls && current==parent && !memcmp(parent,&parent_snapshot,sizeof(*parent)));
    assert(elf_reads==4 && destroyed==4 && stack_frees==3);
    /* Failed image and vector setup must also leave the old image/association
     * intact, independently of the later preflight and stdio failures. */
    h->type=0;
    result=process_exec_replace("/fat/bin/echo",1,(const char *const *)argv,0,NULL,-1,-1,-1,0,0,0);
    assert(result==SRV_ERR_NOEXEC && !binding_calls && next_image_generation==42);
    assert(!memcmp(parent,&parent_snapshot,sizeof(*parent)) && kernel_association==(void *)0xcafe);
    h->type=ET_EXEC;allocated=freed=0;memset(freed_page,0,sizeof(freed_page));
    result=process_exec_replace("/fat/bin/echo",0,(const char *const *)argv,0,NULL,-1,-1,-1,0,0,0);
    assert(result==SRV_ERR_NOMEM && !binding_calls && next_image_generation==42 && freed==allocated);
    assert(!memcmp(parent,&parent_snapshot,sizeof(*parent)) && kernel_association==(void *)0xcafe);
    for (unsigned fail=0;fail<2;fail++) for (unsigned kind=0;kind<9;kind++) {
        allocated=freed=0;memset(freed_page,0,sizeof(freed_page));
        image_prepare_ok=fail!=0;next_image_generation=fail ? UINT64_MAX : 41;
        unsigned before=image_prepare_calls;int64_t code;
        switch(kind) {
        case 0: code=process_run_elf_args("/fat/bin/echo","") ? 1 : -1;break;
        case 1: code=process_spawn_elf_args("/fat/bin/echo","");break;
        case 2: code=process_spawn_elf_args_redirect("/fat/bin/echo","","/fat/out",false);break;
        case 3: code=process_start_background("/fat/bin/echo") ? 1 : -1;break;
        case 4: code=process_spawn_background_elf_args_quiet_flag("/fat/bin/echo","",true,0);break;
        case 5: code=process_spawn_background_elf_args_fds("/fat/bin/echo","",-1,-1);break;
        case 6: code=process_spawn_exec("/fat/bin/echo",1,(const char *const *)argv,0,NULL,true,-1,-1,-1,0,false,NULL,0,0,0,0);break;
        case 7: {struct isr_frame frame={.cs=0x23,.ss=0x1b};code=process_fork(&frame);break;}
        default: code=process_exec_replace("/fat/bin/echo",1,(const char *const *)argv,0,NULL,-1,-1,-1,0,0,0);break;
        }
        assert(code<0 && image_prepare_calls==before+1 && !binding_calls && !admissions);
        if (kind>=6) assert(code==(fail ? SRV_ERR_AGAIN : SRV_ERR_IO));
        assert(next_image_generation==(fail ? UINT64_MAX : 41));
        assert(current==parent && !memcmp(parent,&parent_snapshot,sizeof(*parent)) && !processes[1].allocated);
        assert(kernel_association==(void *)0xcafe);
    }
    /* Successful exec must remain unbound at every old-backing retirement
     * observation, then select the complete new image exactly once. */
    allocated=1;freed=0;memset(freed_page,0,sizeof(freed_page));
    image_prepare_ok=true;next_image_generation=41;parent->image_generation=41;
    if(!parent->mappings){parent->mappings=calloc(PROCESS_MAX_MAPPINGS,sizeof(*parent->mappings));assert(parent->mappings);}
    parent->mapping_count=1;parent->mappings[0]=(struct process_mapping){.virtual_address=0x500000,.physical_address=4096,.flags=VMM_PAGE_USER|VMM_PAGE_WRITABLE};
    parent->files[3]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=1,.flags=SRV_FD_CLOEXEC};
    parent->fpu.active_user_trap=NULL;bound_fpu=&parent->fpu;bound_fs=parent->tls_thread_pointer;
    exec_parent=parent;kernel_association=(void *)0xcafe;exec_success=true;
    simulated_flags=1ull<<9;
    if(!setjmp(exec_entry)) {
        (void)process_exec_replace("/fat/bin/echo",1,(const char *const *)argv,0,NULL,-1,-1,-1,0,0,0);
        assert(!"successful exec returned");
    }
    if (getenv("SRVROS_EXEC_PENDING_TEST") && strcmp(getenv("SRVROS_EXEC_PENDING_TEST"),"ignored"))
        assert(fatal_exit_visited && !user_entry_visited);
    else assert(user_entry_visited && !fatal_exit_visited);
    assert(exec_cleared && exec_rebound && exec_cleanup_observations==2 && freed==1);
    assert(simulated_flags==(1ull<<9) && binding_calls==1 && next_image_generation==42);
    assert(parent->image_generation==42 && bound_cr3==3 && bound_fpu==&parent->fpu);
    assert(!parent->usr1_action.mode && !parent->usr1_action_generation && !process_signal_records_active(parent));
    exec_success=false;current=NULL;cleanup_process_address_space(parent);
    parent->killed=false;parent->kill_signal=0;parent->main_pending_signals=0;parent->pending_signals=0;
    parent->main_signal_mask=0;parent->signal_ignore_mask=0;
    assert(freed==allocated && !parent->image_generation && !parent->mapping_count);

    /* Main pthread_exit returns for atexit without retiring the shared image. */
    parent->image_generation=77;parent->address_space=3;next_image_generation=77;
    current=parent;worker=NULL;bound_cr3=3;bound_fs=parent->tls_thread_pointer;bound_fpu=&parent->fpu;
    kernel_association=(void *)0xcafe;main_exit_test=true;
    parent->main_signal_state.stack.depth=1;parent->main_signal_state.stack.last_token=5;
    parent->user_threads[0].signal_state.stack.depth=1;
    process_thread_exit(1234);
    assert(thread_invalidations==1 && !kernel_association && current==parent);
    assert(parent->main_thread_exited && parent->main_thread_return_value==1234 && parent->main_signal_mask==UINT64_MAX);
    assert(parent->image_generation==77 && parent->address_space==3 && bound_cr3==3 && bound_fpu==&parent->fpu);
    assert(!parent->main_signal_state.stack.depth && !parent->main_signal_state.stack.last_token && parent->user_threads[0].signal_state.stack.depth==1);
    main_exit_test=false;

    /* A detached worker clears its binding before TLS backing and slot reuse. */
    parent->main_thread_exited=false;parent->tls_memsz=32;parent->tls_align=16;
    allocated=1;freed=0;memset(freed_page,0,sizeof(freed_page));
    if(!parent->mappings){parent->mappings=calloc(PROCESS_MAX_MAPPINGS,sizeof(*parent->mappings));assert(parent->mappings);}
    parent->mapping_count=1;parent->mappings[0]=(struct process_mapping){.virtual_address=USER_TLS_BASE+PAGE_SIZE,.physical_address=4096,.flags=VMM_PAGE_USER|VMM_PAGE_WRITABLE};
    exiting_worker=&parent->user_threads[0];
    *exiting_worker=(struct process_user_thread){.used=true,.active=true,.detached=true,.owner=parent,.tid=2,.tls_base=USER_TLS_BASE+PAGE_SIZE,.tls_thread_pointer=USER_TLS_BASE+PAGE_SIZE+32};
    worker=exiting_worker;bound_fpu=&worker->fpu;kernel_association=(void *)0xcafe;worker_exit_test=true;
    worker->signal_state.stack.depth=1;worker->signal_state.stack.last_token=7;
    if(!setjmp(worker_exit_return))process_thread_exit(55);
    assert(!current && !worker && freed==1 && !parent->mapping_count);
    assert(exiting_worker->return_value==55 && !exiting_worker->active && !exiting_worker->used);
    assert(parent->image_generation==77 && parent->address_space==3 && !kernel_association && bound_cr3==0x2000);
    worker_exit_test=false;cleanup_process_address_space(parent);

    /* A record in any embedded slot rejects fork/exec before allocation,
     * file mutation, image generation or scheduler publication. */
    current=parent;worker=NULL;parent->main_thread_exited=false;parent->image_generation=77;next_image_generation=77;
    for(unsigned i=0;i<=PROCESS_MAX_USER_THREADS;i++) {
        struct signal_thread_state *s=i?&parent->user_threads[i-1].signal_state:&parent->main_signal_state;
        s->stack.depth=1;memcpy(&parent_snapshot,parent,sizeof(*parent));
        unsigned a=allocated,f=freed,e=elf_reads,b=binding_calls,ad=admissions,prep=image_prepare_calls;
        struct isr_frame frame={.cs=0x23,.ss=0x1b};
        assert(process_fork(&frame)==SRV_ERR_NOTSUP);
        assert(process_exec_replace("/fat/bin/echo",1,(const char *const *)argv,0,NULL,-1,-1,-1,0,0,0)==SRV_ERR_NOTSUP);
        assert(!memcmp(parent,&parent_snapshot,sizeof(*parent)) && next_image_generation==77);
        assert(allocated==a && freed==f && elf_reads==e && binding_calls==b && admissions==ad && image_prepare_calls==prep);
        *s=(struct signal_thread_state){0};
    }
    /* Ordinary fork inherits the descriptor and mask, with fresh private state.
     * The scheduler mock checks the exact publication, without running a child. */
    parent->usr1_action=(struct signal_action_descriptor){.mode=SIGNAL_ACTION_INFO,.handler=0x500000,.trampoline=0x500100,.mask=1ull<<12};
    parent->usr1_action_generation=17;parent->signal_catch_mask=1ull<<10;parent->main_signal_mask=1ull<<12;
    parent->pending_signals=parent->main_pending_signals=1ull<<10;
    parent->tls_filesz=parent->tls_memsz=parent->tls_base=parent->tls_thread_pointer=0;
    parent->address_space=3;if(!parent->mappings){parent->mappings=calloc(PROCESS_MAX_MAPPINGS,sizeof(*parent->mappings));assert(parent->mappings);}
    parent->mapping_count=1;
    allocated=1;freed=0;memset(freed_page,0,sizeof(freed_page));
    parent->mappings[0]=(struct process_mapping){.virtual_address=0x500000,.physical_address=4096,.flags=VMM_PAGE_USER};
    memcpy(&parent_snapshot,parent,sizeof(*parent));fork_publish_expected=true;
    struct isr_frame frame={.cs=0x23,.ss=0x1b,.rip=0x500000,.rsp=0x900000,.rflags=0x202};
    assert(process_fork(&frame)>0 && published_fork && !memcmp(parent,&parent_snapshot,sizeof(*parent)));
    fork_publish_expected=false;cleanup_process_address_space(published_fork);release_process(published_fork);
    puts("process-image-lifecycle: constructor failure/exhaustion, exec rollback/commit, main atexit and worker TLS retirement PASS");
}
