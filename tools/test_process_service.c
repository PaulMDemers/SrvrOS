/* Real process memory loops: service windows cannot expose freed mappings or
 * release an inherited serialization owner. No audio policy is involved. */
#include <assert.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_SERVICE_HOST_TEST
#include "../kernel/src/process.c"
/* Exit tests stop at the scheduler-context query immediately after the hook. */
void audio_process_cleanup(uint64_t p) {(void)p;assert(0);}
void console_release_framebuffer_mute_owner(uint64_t p) {(void)p;assert(0);}
bool exfat_write_file_retryable(const char *p,const uint8_t *d,uint64_t n,bool *r) {(void)p;(void)d;(void)n;(void)r;assert(0);return false;}
void gui_process_cleanup(uint64_t p) {(void)p;assert(0);}
void kfree(void *p) {free(p);}
int64_t net_close(uint64_t p) {(void)p;assert(0);return -1;}
void net_process_cleanup(struct process *p) {(void)p;assert(0);}
static bool scope_wakes,detach_during_clear;
static unsigned scope_net_wakes;
void net_process_wake(struct process *p) {assert(scope_wakes && p);scope_net_wakes++;}
void keyboard_wake_waiters(void) {assert(scope_wakes);}
void pty_wake(void) {assert(scope_wakes);}
void process_context_restore(struct process_context *p,uint64_t v) {(void)p;(void)v;abort();}
void pty_close(uint64_t p,bool master) {(void)p;(void)master;assert(0);}
void scheduler_clear_user_context(void) {assert(0);}
void scheduler_exit_current(void) {abort();}
void scheduler_kill_user_threads(void *p,void *e) {(void)p;(void)e;assert(0);}
static unsigned depth,begins,ends,checkpoints,yields,freed,destroyed;
static uint64_t checkpoint_scope;static bool saw_converted_checkpoint;
static bool available=true,fail_unmap,independent,bad_translation;
static unsigned unmap_calls,fail_at_unmap,alloc_calls,fail_alloc_at,map_calls,fail_map_at,spawn_calls,console_prints;
static bool fail_spawn;
static uint64_t active_space=0x1000;
uint64_t vmm_current_address_space(void) { return active_space; }
static unsigned private_begins;
static void *private_target;
static bool checking_namespace, namespace_admitted;
bool scheduler_io_begin_process_image(void *image) {
    private_begins++;if(!available || depth)return false;
    if(checking_namespace) assert(namespace_admitted);
    private_target=image;depth++;begins++;return true;
}
static struct process *observed;
static struct process *current;
void *scheduler_current_user_context(void) {return current;}
static void *current_user_thread;
static jmp_buf exit_hook;static bool check_exit_hook,check_thread_hook;
void *scheduler_current_user_thread_context(void) {
    if(check_exit_hook) {assert(current && !current->owned_user_scope);longjmp(exit_hook,1);}
    return current_user_thread;
}
void scheduler_io_set_user_safe(bool safe) {(void)safe;}
void scheduler_io_wait(void) {}
void scheduler_yield(void) {assert(0);}
bool scheduler_wait(struct scheduler_wait_queue *q,bool (*ready)(void *),void *arg) {(void)q;(void)ready;(void)arg;assert(0);return false;}
uint64_t gdt_default_kernel_stack_top(void) {return 0x10000;}
void fpu_init_state(struct fpu_state *state) {memset(state,0,sizeof(*state));}
bool scheduler_io_process_retired(void *p) {assert(p==&processes[1]);return independent;}
bool scheduler_io_process_helper_memory(void *p,struct scheduler_wait_queue *parked) { assert(p==observed && parked==&fd_poll_wait_queue);return independent; }
bool scheduler_io_process_independent(void *p) { assert(p==observed);return independent && !current_user_thread; }
static bool released[1024];
static unsigned next_frame=1,wakes;
static unsigned char backing[1024*4096];
void console_printf(const char *format,...) {(void)format;console_prints++;}
void scheduler_wake_all(struct scheduler_wait_queue *queue) {
    if(check_thread_hook) {assert(current && queue==&current->thread_wait_queue && current->owned_user_scope);longjmp(exit_hook,1);}
    if(scope_wakes) {wakes++;return;}
    if(queue==&process_wait_queue){wakes++;return;}
    assert(queue==&futexes[0].wait_queue && futexes[0].invalidated && depth);
    int64_t i=process_mapping_index(observed,futexes[0].address);
    assert(i>=0 && !released[observed->mappings[i].physical_address/4096]);
    queue->waiters=0;wakes++;
}
bool scheduler_io_begin(void) {begins++;if(!available)return false;depth++;return true;}
bool scheduler_io_active(void) {return depth != 0;}
void scheduler_io_end(void) {assert(depth);depth--;ends++;}
void scheduler_io_checkpoint(void) {
    checkpoints++;if(!depth)return;yields++;
    if(detach_during_clear) {detach_during_clear=false;user_scope_detach(current);}
    if(checkpoint_scope) {
        struct process_lifetime *life=lifetime_find(checkpoint_scope);
        assert(life && life->members==1 && !life->reservation && depth &&
            process_slots_busy[1] && !processes[1].allocated);
        saw_converted_checkpoint=true;checkpoint_scope=0;
    }
    if(observed)for(uint64_t i=0;i<observed->mapping_count;i++) {
        unsigned frame=observed->mappings[i].physical_address/4096;
        assert(frame && frame<1024 && !released[frame]);
    }
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t address,uint64_t *physical) {
    assert(observed && space==observed->address_space);
    for(uint64_t i=0;i<observed->mapping_count;i++)if(observed->mappings[i].virtual_address==address) {
        *physical=observed->mappings[i].physical_address;return !fail_unmap && ++unmap_calls!=fail_at_unmap;
    }
    assert(0);return false;
}
void pmm_free_frame_range(uint64_t physical,uint64_t count) {(void)physical;(void)count;assert(0);}
void pmm_free_frame(uint64_t physical) {
    unsigned frame=physical/4096;assert(frame && frame<1024 && !released[frame]);released[frame]=true;freed++;
}
void vmm_destroy_address_space(uint64_t space) {assert(observed && !observed->mapping_count && space==0x1000);destroyed++;}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t minimum,enum pmm_frame_tag tag) {
    assert(n==1 && minimum==PROCESS_USER_PAGE_MIN_PHYSICAL && tag==PMM_FRAME_USER);
    if(++alloc_calls==fail_alloc_at)return 0;
    return next_frame<1024?4096*next_frame++:0;
}
void *pmm_phys_to_virt(uint64_t physical) {assert(physical<sizeof(backing));return backing+physical;}
bool vmm_map_page_in_space(uint64_t space,uint64_t virt,uint64_t physical,uint64_t flags) {
    assert(space==0x1000 && virt && physical && flags);return ++map_calls!=fail_map_at;
}
void scheduler_set_user_thread_context(void *p) {(void)p;assert(0);}
void scheduler_set_user_context_fpu(void *p,uint64_t as,uint64_t stack,struct fpu_state *fpu) {(void)p;(void)as;(void)stack;(void)fpu;assert(0);}
static jmp_buf prepared_entry;static bool entering_prepared;
void usermode_enter(uint64_t e,uint64_t stack,uint64_t a,uint64_t v,uint64_t env,uint64_t fs) {
    assert(entering_prepared && current==observed && current_user_thread==&observed->user_threads[0]);
    assert(e==0x400000 && stack==0x500800 && a==123 && !v && !env && fs==observed->user_threads[0].tls_thread_pointer);
    longjmp(prepared_entry,1);
}
static void enter_prepared_thread(void *thread) {
    current_user_thread=thread;entering_prepared=true;
    if(!setjmp(prepared_entry))process_user_thread_start(current_user_thread);
    entering_prepared=false;current_user_thread=NULL;
}
bool scheduler_spawn_user_thread(const char *name,scheduler_thread_fn entry,void *raw,struct fpu_state *fpu,uint64_t fs) {
    struct process_user_thread *t=raw;assert(!strcmp(name,"uthread") && entry==process_user_thread_start);
    assert(t->used && t->active && t->owner==observed && fpu==&t->fpu && fs==t->tls_thread_pointer);
    spawn_calls++;return !fail_spawn;
}
static void reset(void) {
    assert(!depth);begins=ends=checkpoints=yields=freed=destroyed=0;next_frame=1;
    memset(released,0,sizeof(released));memset(backing,0xcc,sizeof(backing));
    memset(futexes,0,sizeof(futexes));wakes=0;unmap_calls=fail_at_unmap=alloc_calls=fail_alloc_at=map_calls=fail_map_at=spawn_calls=console_prints=0;fail_spawn=false;independent=bad_translation=false;
}
void console_write(const char *text) {(void)text;}
bool vmm_create_address_space(uint64_t *space) {*space=0x1000;return true;}
static unsigned namespace_reads;
static const struct vfs_node fixture_dirs[]={
    {.used=true,.path="/",.type=VFS_NODE_DIRECTORY},
    {.used=true,.path="/fat",.type=VFS_NODE_DIRECTORY},
    {.used=true,.path="/fat/bin",.type=VFS_NODE_DIRECTORY},
    {.used=true,.path="/fat/var",.type=VFS_NODE_DIRECTORY},
    {.used=true,.path="/fat/home",.type=VFS_NODE_DIRECTORY},
};
uint64_t vfs_count(void) { namespace_reads++;return sizeof(fixture_dirs)/sizeof(fixture_dirs[0]); }
const struct vfs_node *vfs_node_at(uint64_t i) { namespace_reads++;return i<sizeof(fixture_dirs)/sizeof(fixture_dirs[0])?&fixture_dirs[i]:NULL; }
void vfs_set_retire_callback(vfs_retire_fn callback) { (void)callback; }
const struct vfs_node *vfs_lookup(const char *path) {
    namespace_reads++;
    if(checking_namespace) {
        assert(depth);
        if(!strcmp(path,"/fat/bin")) namespace_admitted=true;
    }
    for(uint64_t i=0;i<vfs_count();i++) if(!strcmp(path,fixture_dirs[i].path)) return &fixture_dirs[i];
    assert(!strcmp(path,"/fat/bin/desktools"));return NULL;
}
bool vfs_read_all(const struct vfs_node *node,const uint8_t **data,uint64_t *size) {(void)node;(void)data;(void)size;assert(0);return false;}
void vfs_release_data(const struct vfs_node *node,const uint8_t *data) {(void)node;(void)data;assert(0);}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t address,uint64_t *physical) {
    assert(observed && space==observed->address_space);
    int64_t i=process_mapping_index(observed,address);if(i<0)return false;
    *physical=observed->mappings[i].physical_address+(bad_translation?4096:0);return true;
}
static void private_scope_contract(void) {
    struct process *parent=calloc(1,sizeof(*parent)),*child=calloc(1,sizeof(*child));assert(parent && child);
    parent->pid=42;child->parent_pid=42;child->allocated=child->active=true;
    char path[]="/fat/bin/desktools",copy[64]={0};
    struct process_service_scope scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));
    assert(scope.held && depth==1 && private_begins==1 && private_target==child && !strcmp(copy,path));
    path[1]='X';assert(!strcmp(copy,"/fat/bin/desktools"));process_service_end(scope);
    unsigned before=private_begins;
    child->address_space=1;scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));
    assert(scope.held && private_begins==before);process_service_end(scope);child->address_space=0;
    child->user_threads[0].used=true;scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));
    assert(scope.held && private_begins==before);process_service_end(scope);child->user_threads[0].used=false;
    child->startup_ready=true;scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));
    assert(scope.held && private_begins==before);process_service_end(scope);child->startup_ready=false;
    child->parent_pid=43;scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));
    assert(scope.held && private_begins==before);process_service_end(scope);child->parent_pid=42;
    scope=process_spawn_image_scope(parent,parent,path,copy,sizeof(copy));assert(private_begins==before);process_service_end(scope);
    scope=process_spawn_image_scope(parent,child,path,copy,3);assert(!copy[0] && private_begins==before);process_service_end(scope);
    struct process_service_scope outer=process_service_begin();assert(depth==1);
    scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));assert(depth==2 && scope.held);
    process_service_end(scope);assert(depth==1);process_service_end(outer);assert(!depth);
    available=false;scope=process_spawn_image_scope(parent,child,path,copy,sizeof(copy));assert(!scope.held && !depth);process_service_end(scope);available=true;
    observed=child;unsigned old_destroyed=destroyed;
    assert(load_spawn_image_checked(parent,child,"/fat/bin/desktools")==SRV_ERR_NOENT);
    assert(!depth && !child->address_space && destroyed==old_destroyed+1);
    assert(vfs_namespace_system_activate());
    checking_namespace=true;namespace_admitted=false;old_destroyed=destroyed;
    assert(load_spawn_image_checked(parent,child,"/bin/desktools")==SRV_ERR_NOENT);
    assert(namespace_admitted && !depth && !child->address_space && destroyed==old_destroyed+1);
    checking_namespace=false;vfs_namespace_system_reset();observed=NULL;
    free(child);free(parent);puts("spawn-image-scope: PASS frozen path, fresh child only, ordinary fallback, nested ownership and unavailable gate");
}
static void foreign_loader_contract(void) {
    struct process *image=calloc(1,sizeof(*image));assert(image);
    assert(!current && !depth && vfs_namespace_system_activate());
    unsigned reads=namespace_reads,old_begins=begins,old_ends=ends;
    uint64_t entry=123;
    depth=1;available=false; /* A different thread owns admission. */
    assert(load_elf_image_checked("/bin/desktools",image,USER_STACK_TOP,&entry)==SRV_ERR_IO);
    assert(depth==1 && begins==old_begins+1 && ends==old_ends &&
        namespace_reads==reads && entry==123 && !image->address_space);
    depth=0;available=true;vfs_namespace_system_reset();free(image);
    puts("process-loader: PASS foreign admission refuses before namespace access");
}
static void private_unmap_refused(struct process *p,uint64_t address,uint64_t length) {
    static struct process snapshot;memcpy(&snapshot,p,sizeof(snapshot));
    unsigned old_freed=freed,old_checks=checkpoints,old_wakes=wakes;int64_t result=731;
    assert(!process_try_munmap_private(p,address,length,&result) && result==731);
    assert(!memcmp(&snapshot,p,sizeof(snapshot)) && freed==old_freed && checkpoints==old_checks && wakes==old_wakes);
}
static void private_unmap_contract(void) {
    reset();available=true;independent=true;
    struct process *p=calloc(1,sizeof(*p));assert(p);observed=p;
    p->allocated=p->active=true;p->address_space=0x1000;p->mmap_next=USER_MMAP_BASE+100*4096;
    uint64_t base=USER_MMAP_BASE,length=65*4096;
    for(unsigned i=0;i<65;i++)assert(map_process_page(p,base+i*4096,VMM_PAGE_USER|VMM_PAGE_WRITABLE));
    assert(map_process_page(p,base+128*4096,VMM_PAGE_USER));
    assert(mmap_record_region(p,base,length,true));assert(mmap_record_region(p,base+128*4096,4096,false));
    private_unmap_refused(p,base+1,length);private_unmap_refused(p,base,0);
    private_unmap_refused(p,base,2*1024*1024+1);private_unmap_refused(p,UINT64_MAX-4095,4096);
    private_unmap_refused(p,base,4096);private_unmap_refused(p,base,length+4096);
    p->allocated=false;private_unmap_refused(p,base,length);p->allocated=true;
    p->active=false;private_unmap_refused(p,base,length);p->active=true;
    p->exiting=true;private_unmap_refused(p,base,length);p->exiting=false;
    p->user_threads[0].used=true;private_unmap_refused(p,base,length);p->user_threads[0].used=false;
    p->mmap_regions[0].anonymous=false;private_unmap_refused(p,base,length);p->mmap_regions[0].anonymous=true;
    p->mmap_regions[2]=p->mmap_regions[0];private_unmap_refused(p,base,length);p->mmap_regions[2].used=false;
    p->mapping_count=4097;private_unmap_refused(p,base,length);p->mapping_count=66;
    independent=false;private_unmap_refused(p,base,length);independent=true;
    bad_translation=true;private_unmap_refused(p,base,length);bad_translation=false;
    active_space=0x2000;private_unmap_refused(p,base,length);active_space=0x1000;
    int64_t result=731;assert(!process_try_munmap_private(NULL,base,length,&result) && result==731);
    assert(!process_try_munmap_private(p,base,length,NULL));
    /* A foreign owner remains held. No callback/checkpoint or end may run. */
    depth=1;unsigned old_begin=begins,old_end=ends,old_checks=checkpoints;
    futexes[0]=(struct process_futex){.used=true,.process=p,.address=base+4096,.wait_queue={.waiters=1}};
    fail_at_unmap=4;
    assert(process_try_munmap_private(p,base,length,&result) && result==-1);
    assert(freed==3 && p->mapping_count==63 && p->mmap_regions[0].used && wakes==1);
    for(uint64_t i=0;i<p->mapping_count;i++)assert(!released[p->mappings[i].physical_address/4096]);
    fail_at_unmap=0;
    assert(process_try_munmap_private(p,base,length,&result) && !result);
    assert(freed==65 && p->mapping_count==1 && !p->mmap_regions[0].used && p->mmap_next==base);
    assert(depth==1 && begins==old_begin && ends==old_end && checkpoints==old_checks);
    assert(!released[p->mappings[0].physical_address/4096]);depth=0;
    cleanup_process_address_space(p);assert(freed==66 && !p->mapping_count);free(p);
    /* Negative control on the original code refuses this real helper request.
     * With foreign I/O held, retire exact anonymous pages without any service
     * checkpoint, allocator callback, gate handoff or premature completion. */
    reset();p=calloc(1,sizeof(*p));assert(p);observed=p;
    p->allocated=p->active=true;p->address_space=0x1000;independent=true;
    for(unsigned i=0;i<65;i++)assert(map_process_page(p,base+i*4096,VMM_PAGE_USER|VMM_PAGE_WRITABLE));
    assert(mmap_record_region(p,base,length,true));
    current_user_thread=&p->user_threads[0];
    private_unmap_refused(p,base,length);
    p->user_threads[0].used=true;private_unmap_refused(p,base,length);
    p->user_threads[0].active=true;private_unmap_refused(p,base,length);
    p->user_threads[0].owner=p;
    independent=false;private_unmap_refused(p,base,length);independent=true;
    depth=1;unsigned worker_begins=begins,worker_ends=ends,worker_checks=checkpoints;
    assert(process_try_munmap_private(p,base,length,&result) && !result);
    assert(freed==65 && !p->mapping_count && !p->mmap_regions[0].used);
    assert(depth==1 && begins==worker_begins && ends==worker_ends && checkpoints==worker_checks);
    current_user_thread=NULL;depth=0;free(p);
    /* Region splitting preserves anonymous provenance; PROT_NONE has no frames. */
    reset();p=calloc(1,sizeof(*p));assert(p);observed=p;p->allocated=p->active=true;p->address_space=0x1000;independent=true;
    assert(mmap_record_region(p,base,3*4096,true));mmap_forget_range(p,base+4096,4096);
    assert(p->mmap_regions[0].anonymous && p->mmap_regions[1].anonymous);
    assert(process_try_munmap_private(p,base,4096,&result) && !result);
    assert(process_try_munmap_private(p,base+2*4096,4096,&result) && !result);
    assert(mmap_record_region(p,base,2*1024*1024,true));
    assert(process_try_munmap_private(p,base,2*1024*1024,&result) && !result && !freed);
    assert(mmap_record_region(p,base,3*4096,false));mmap_forget_range(p,base+4096,4096);
    assert(!p->mmap_regions[0].anonymous && !p->mmap_regions[1].anonymous);
    free(p);observed=NULL;
    puts("private-unmap: PASS bounded/provenance/owner refusal unchanged, foreign gate retention, futex-before-free, partial failure ownership/retry, split and PROT_NONE");
}
static struct process *startup_process(void) {
    reset();struct process *p=calloc(1,sizeof(*p));assert(p);current=observed=p;
    p->allocated=p->active=true;p->address_space=0x1000;p->mmap_next=USER_MMAP_BASE;
    p->tls_align=16;p->tls_memsz=4096;p->tls_filesz=4;p->next_thread_id=2;
    memcpy(p->tls_template,"TLS!",4);independent=true;return p;
}
static void map_refused(struct process *p,uint64_t address,uint64_t length,uint64_t prot,uint64_t flags,int64_t fd,int64_t off) {
    static struct process snapshot;memcpy(&snapshot,p,sizeof(snapshot));
    int64_t result=731;unsigned a=alloc_calls,c=checkpoints,f=freed;
    assert(!process_try_mmap_private(p,address,length,prot,flags,fd,off,&result) && result==731);
    assert(!memcmp(p,&snapshot,sizeof(snapshot)) && a==alloc_calls && c==checkpoints && f==freed);
}
static void startup_map_contract(void) {
    uint64_t prot=SRV_PROT_READ|SRV_PROT_WRITE,flags=SRV_MAP_PRIVATE|SRV_MAP_ANONYMOUS;
    struct process *p=startup_process();
    map_refused(p,1,4096,prot,flags,-1,0);map_refused(p,0,0,prot,flags,-1,0);
    map_refused(p,0,2*1024*1024+1,prot,flags,-1,0);map_refused(p,0,4096,0,flags,-1,0);
    map_refused(p,0,4096,prot,flags|SRV_MAP_FIXED,-1,0);map_refused(p,0,4096,prot,flags,0,0);
    map_refused(p,0,4096,prot,flags,-1,4096);
    p->user_threads[0].used=true;map_refused(p,0,4096,prot,flags,-1,0);p->user_threads[0].used=false;
    independent=false;map_refused(p,0,4096,prot,flags,-1,0);independent=true;
    active_space=2;map_refused(p,0,4096,prot,flags,-1,0);active_space=0x1000;
    p->exiting=true;map_refused(p,0,4096,prot,flags,-1,0);p->exiting=false;
    p->mapping_count=4097;map_refused(p,0,4096,prot,flags,-1,0);p->mapping_count=0;
    assert(mmap_record_region(p,USER_MMAP_BASE,4096,true));map_refused(p,0,4096,prot,flags,-1,0);p->mmap_regions[0].used=false;
    depth=1;int64_t result=731;assert(process_try_mmap_private(p,0,2*1024*1024,prot,flags,-1,0,&result));
    assert(result==USER_MMAP_BASE && p->mapping_count==512 && p->mmap_regions[0].anonymous && !checkpoints && !begins && !ends && depth==1);
    for(unsigned i=0;i<512;i++)for(unsigned j=0;j<4096;j++)assert(!backing[p->mappings[i].physical_address+j]);
    assert(process_try_munmap_private(p,USER_MMAP_BASE,2*1024*1024,&result) && !result && freed==512);
    depth=0;free(p);
    for(unsigned failure=0;failure<3;failure++) {
        p=startup_process();depth=1;
        if(failure==1)fail_map_at=4;else fail_alloc_at=4;
        if(failure==2)fail_at_unmap=1;
        assert(process_try_mmap_private(p,0,65*4096,prot,flags,-1,0,&result) && result==-1);
        assert(!checkpoints && !begins && !ends && depth==1 && console_prints==0);
        if(failure==2) {
            assert(p->mapping_count==3 && p->mmap_regions[0].used && !freed);
            fail_at_unmap=0;assert(process_try_munmap_private(p,USER_MMAP_BASE,65*4096,&result) && !result && freed==3);
        } else assert(!p->mapping_count && !p->mmap_regions[0].used && freed==(failure==1?4:3));
        depth=0;free(p);
    }
    current=observed=NULL;
    puts("startup-anonymous-map: PASS sole-main refusal, bounded zeroed allocation, foreign gate retention, PMM/PTE failure rollback and failed-unmap ownership");
}
static void thread_refused(struct process *p,uint64_t entry,uint64_t stack,uint64_t flags) {
    static struct process snapshot;memcpy(&snapshot,p,sizeof(snapshot));
    int64_t result=731;unsigned a=alloc_calls,c=checkpoints,f=freed;
    assert(!process_try_thread_create_private(entry,123,stack,flags,&result) && result==731);
    assert(!memcmp(p,&snapshot,sizeof(snapshot)) && a==alloc_calls && c==checkpoints && f==freed);
}
static void startup_thread_contract(void) {
    for(unsigned failure=0;failure<4;failure++) {
        struct process *p=startup_process();
        assert(map_process_page(p,0x400000,VMM_PAGE_USER));
        assert(map_process_page(p,0x500000,VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE));
        thread_refused(p,0,0x500800,0);thread_refused(p,0x400000,0x500800,2);
        thread_refused(p,0x400000,0x501003,0);thread_refused(p,0x400000,0x700000,0);
        p->mappings[1].flags&=~VMM_PAGE_WRITABLE;thread_refused(p,0x400000,0x500800,0);p->mappings[1].flags|=VMM_PAGE_WRITABLE;
        p->tls_align=3;thread_refused(p,0x400000,0x500800,0);p->tls_align=16;
        p->tls_memsz=4097;thread_refused(p,0x400000,0x500800,0);p->tls_memsz=4096;
        p->user_threads[0].used=true;thread_refused(p,0x400000,0x500800,0);p->user_threads[0].used=false;
        independent=false;thread_refused(p,0x400000,0x500800,0);independent=true;
        bad_translation=true;thread_refused(p,0x400000,0x500800,0);bad_translation=false;
        p->mappings[0].flags|=VMM_PAGE_NO_EXECUTE;thread_refused(p,0x400000,0x500800,0);p->mappings[0].flags&=~VMM_PAGE_NO_EXECUTE;
        depth=1;int64_t result;
        if(failure==1)fail_alloc_at=4;
        if(failure>=2)fail_spawn=true;
        if(failure==3)fail_at_unmap=1;
        assert(process_try_thread_create_private(0x400000,123,0x500800,SRV_THREAD_DETACHED,&result));
        assert(!checkpoints && !begins && !ends && depth==1 && console_prints==0);
        if(!failure) {
            assert(result==2 && spawn_calls==1 && p->mapping_count==4);
            struct process_user_thread *t=&p->user_threads[0];assert(t->used && t->active && t->detached && t->arg==123);
            uint64_t phys;assert(vmm_virt_to_phys_in_space(p->address_space,t->tls_base,&phys));assert(!memcmp(backing+phys,"TLS!",4));
            enter_prepared_thread(t);
            assert(!checkpoints && depth==1); /* No context refresh/allocation on entry. */
            process_release_thread_tls(t);assert(freed==2 && p->mapping_count==2);
        } else {
            assert(result==-1 && !p->user_threads[0].used && !p->user_threads[0].active);
            if(failure==3) {
                assert(p->mapping_count==3 && freed==1); /* Failed leaf remains tracked/owned. */
                fail_at_unmap=0;assert(process_unmap_tracked_page_checked(p,p->mappings[2].virtual_address));
            } else assert(p->mapping_count==2 && freed==(failure==1?1:2));
        }
        for(uint64_t i=0;i<p->mapping_count;i++)assert(!released[p->mappings[i].physical_address/4096]);
        depth=0;cleanup_process_address_space(p);free(p);
    }
    current=observed=NULL;
    puts("startup-thread: PASS sole-main/entry/stack/TLS refusal, complete context publication, no service window, TLS/stack failure cleanup and retained failed-unmap ownership");
}

static void session_scope_contract(void) {
    memset(processes,0,sizeof(processes));memset(process_lifetimes,0,sizeof(process_lifetimes));
    memset(process_slots_busy,0,sizeof(process_slots_busy));next_lifetime=1;next_pid=100;
    current=&processes[0];*current=(struct process){.allocated=true,.active=true,.pid=42};
    uint64_t token=0,other=0;struct srv_scope_status st={0};
    current->lifetime=77;assert(process_scope_create(&token)==SRV_ERR_NOTSUP);current->lifetime=0;
    assert(!process_scope_create(&token) && token==1);
    current->owned_user_scope=0; /* Reused slot cannot rely on stale owner pointer. */
    assert(process_scope_control(SRV_SCOPE_STATUS,token,&st)==SRV_ERR_INVAL);
    current->owned_user_scope=token;current->killed=true;
    assert(process_scope_control(SRV_SCOPE_REVOKE,token,NULL)==SRV_ERR_INVAL);
    current->killed=false;
    assert(process_scope_create(&other)==SRV_ERR_BUSY);
    assert(!process_scope_reserve(token));
    assert(process_scope_reserve(token)==SRV_ERR_BUSY);
    assert(!process_scope_control(SRV_SCOPE_REVOKE,token,NULL));
    assert(!process_scope_control(SRV_SCOPE_REVOKE,token,NULL));
    assert(process_scope_control(SRV_SCOPE_RELEASE,token,NULL)==SRV_ERR_BUSY);
    assert(!process_scope_control(SRV_SCOPE_STATUS,token,&st) && st.revoked && st.members==1);
    checkpoint_scope=token;saw_converted_checkpoint=false;
    struct process *child=alloc_process_lifetime("root",true,token);
    assert(saw_converted_checkpoint && child && child->lifetime==token && !child->owned_user_scope);
    process_scope_cancel_reservation(token);
    assert(!process_scope_control(SRV_SCOPE_STATUS,token,&st) && st.members==1);
    child->active=false;child->reapable=true;child->exit_status=7;
    memset(child->files,0,sizeof(child->files));
    independent=true;uint64_t status=0;int64_t result=0;
    assert(process_try_wait_retired(child->pid,&status,&result));
    assert(!process_scope_control(SRV_SCOPE_STATUS,token,&st) && !st.members && st.nonzero_exits==1 && st.first_exit_status==7);
    assert(!process_scope_control(SRV_SCOPE_RELEASE,token,NULL));
    assert(process_scope_control(SRV_SCOPE_STATUS,token,&st)==SRV_ERR_INVAL);
    assert(!process_scope_create(&token) && token==2);
    assert(!process_scope_reserve(token));process_scope_cancel_reservation(token);
    assert(process_scope_reserve(token)==SRV_ERR_BUSY); /* failed root burns the attempt */
    assert(!process_scope_control(SRV_SCOPE_REVOKE,token,NULL));
    assert(!process_scope_control(SRV_SCOPE_RELEASE,token,NULL));
    assert(!process_scope_create(&token));assert(!process_scope_reserve(token));
    child=alloc_process_lifetime("root",true,token);assert(child);
    scope_wakes=true;
    user_scope_detach(current);assert(!current->owned_user_scope);
    assert(child->killed && child->kill_signal==SRV_SIGNAL_KILL && scope_net_wakes==1);
    assert(process_scope_create(&other)==SRV_ERR_BUSY);
    child->active=false;child->reapable=true;child->exit_status=0;
    memset(child->files,0,sizeof(child->files));
    assert(process_try_wait_retired(child->pid,&status,&result));
    assert(!lifetime_find(token));assert(!process_scope_create(&other) && other!=token);
    user_scope_detach(current);
    assert(!process_scope_create(&token));assert(!process_scope_reserve(token));
    release_process(current);assert(!lifetime_find(token));
    *current=(struct process){.allocated=true,.active=true,.pid=42};
    process_scope_cancel_reservation(token);assert(!process_scope_create(&other));
    assert(!process_scope_control(SRV_SCOPE_REVOKE,other,NULL));
    assert(!process_scope_control(SRV_SCOPE_RELEASE,other,NULL));
    assert(!process_scope_create(&token));assert(!process_scope_reserve(token));
    child=alloc_process_lifetime("root",true,token);assert(child);
    child->parent_pid=77;child->active=false;child->reapable=true;child->exit_status=9;
    memset(child->files,0,sizeof(child->files));
    processes[2]=(struct process){.allocated=true,.active=true,.pid=77};
    assert(!process_scope_control(SRV_SCOPE_REVOKE,token,NULL));
    assert(!process_scope_reap(token)); /* live parent retains wait right */
    processes[2].active=false;
    independent=false;assert(!process_scope_reap(token));independent=true;
    child->mapping_count=1;assert(!process_scope_reap(token));child->mapping_count=0;
    child->files[2].used=true;assert(!process_scope_reap(token));child->files[2].used=false;
    child->user_threads[0].used=true;assert(!process_scope_reap(token));child->user_threads[0].used=false;
    assert(process_scope_reap(token)==1);
    assert(!process_scope_reap(token));
    assert(!process_scope_control(SRV_SCOPE_STATUS,token,&st) && !st.members && st.nonzero_exits==1 && st.first_exit_status==9);
    assert(!process_scope_control(SRV_SCOPE_RELEASE,token,NULL));
    current->killed=true;assert(process_scope_create(&other)==SRV_ERR_INVAL);current->killed=false;
    assert(!process_scope_create(&token));check_thread_hook=true;
    if(!setjmp(exit_hook)) process_thread_exit(17);
    check_thread_hook=false;assert(current->owned_user_scope==token && current->main_thread_exited);
    user_scope_detach(current);current->main_thread_exited=false;
    /* This loop resumes through longjmp; retain its counter explicitly. */
    for(volatile unsigned worker=0;worker<2;worker++) {
        assert(!process_scope_create(&token));
        assert(!process_scope_reserve(token));
        struct process_lifetime *pending=lifetime_find(token);
        assert(pending && pending->reservation && pending->members==1);
        current_user_thread=worker?&current->user_threads[0]:NULL;
        check_exit_hook=true;
        if(!setjmp(exit_hook)) process_exit(0);
        check_exit_hook=false;current_user_thread=NULL;
        assert(!lifetime_find(token) && !current->owned_user_scope);
    }
    assert(!process_scope_create(&other)); /* No stranded ownerless reservation. */
    user_scope_detach(current);
    /* Losing an owner cancels the whole capability-scoped tree, including
     * children outside its process group; an unrelated process is untouched.
     * Explicit revoke alone remains graceful. */
    assert(!process_scope_create(&token));
    assert(!process_scope_reserve(token));
    child=alloc_process_lifetime("root",true,token);assert(child);
    struct process *owner=current;current=child;
    struct process *grandchild=alloc_process("descendant",true);assert(grandchild);
    current=owner;grandchild->process_group=999;
    child->signal_ignore_mask=UINT64_MAX;grandchild->main_signal_mask=UINT64_MAX;
    processes[4]=(struct process){.allocated=true,.active=true,.pid=999};
    assert(!process_scope_control(SRV_SCOPE_REVOKE,token,NULL));
    assert(!child->killed && !grandchild->killed);
    unsigned before=scope_net_wakes;
    user_scope_detach(owner);
    assert(child->killed && child->kill_signal==SRV_SIGNAL_KILL);
    assert(grandchild->killed && grandchild->kill_signal==SRV_SIGNAL_KILL);
    assert(scope_net_wakes==before+2 && !processes[4].killed);
    assert(lifetime_find(token)->members==2 && !lifetime_find(token)->owner);
    assert(process_scope_create(&other)==SRV_ERR_BUSY);
    user_scope_detach(owner);assert(scope_net_wakes==before+2); /* idempotent */
    child->active=false;child->exit_status=137;release_process(child);
    assert(lifetime_find(token)->members==1);
    grandchild->active=false;grandchild->exit_status=137;release_process(grandchild);
    assert(!lifetime_find(token));
    /* The owner can disappear while clear_process yields before the reserved
     * child is published. Its reference must survive and it must start killed. */
    assert(!process_scope_create(&token));assert(!process_scope_reserve(token));
    detach_during_clear=true;
    child=alloc_process_lifetime("late-root",true,token);assert(child && !detach_during_clear);
    assert(child->killed && child->kill_signal==SRV_SIGNAL_KILL);
    assert(lifetime_find(token)->members==1 && !lifetime_find(token)->owner);
    child->active=false;child->exit_status=137;release_process(child);
    assert(!lifetime_find(token));
    scope_wakes=false;current=NULL;
    memset(processes,0,sizeof(processes));memset(process_lifetimes,0,sizeof(process_lifetimes));
    puts("session-scope: PASS reservation/revoke, single root, fast reap errors, stale capability, orphan tree cancellation and late publication");
}

int main(void) {
    /* Identity snapshots leave foreign serialization ownership and accounting
     * untouched, for both initial and worker contexts. */
    depth=1;unsigned self_begins=begins,self_ends=ends;
    assert(process_thread_self()==1 && depth==1);
    struct process_user_thread self_thread={.used=true,.tid=42};
    current_user_thread=&self_thread;assert(process_thread_self()==42 && depth==1);
    self_thread.used=false;assert(process_thread_self()==1 && depth==1);
    assert(begins==self_begins && ends==self_ends);current_user_thread=NULL;depth=0;

    current=&processes[0];*current=(struct process){.allocated=true,.active=true,.pid=42,.pending_signals=1ull<<SRV_SIGNAL_CHLD};
    struct process *child=&processes[1];
    *child=(struct process){.allocated=true,.detached=true,.reapable=true,.pid=43,.parent_pid=42,.exit_status=7,.user_us=12,.lifetime=9};
    process_lifetimes[0]=(struct process_lifetime){.token=9,.members=1};
    uint64_t status=999;int64_t result=999;independent=false;
    assert(!process_try_wait_retired(43,&status,&result) && child->allocated && status==999 && result==999);
    independent=true;
    child->active=true;assert(!process_try_wait_retired(43,&status,&result));child->active=false;
    child->reapable=false;assert(!process_try_wait_retired(43,&status,&result));child->reapable=true;
    child->files[3].used=true;assert(!process_try_wait_retired(43,&status,&result));child->files[3].used=false;
    child->mapping_count=1;assert(!process_try_wait_retired(43,&status,&result));child->mapping_count=0;
    child->address_space=0x1000;assert(!process_try_wait_retired(43,&status,&result));child->address_space=0;
    child->kernel_stack_frames=1;assert(!process_try_wait_retired(43,&status,&result));child->kernel_stack_frames=0;
    child->user_threads[0].used=true;assert(!process_try_wait_retired(43,&status,&result));child->user_threads[0].used=false;
    child->parent_pid=44;assert(!process_try_wait_retired(43,&status,&result));child->parent_pid=42;
    process_slots_busy[1]=true;assert(!process_try_wait_retired(43,&status,&result));process_slots_busy[1]=false;
    process_lifetimes[0].members=0;assert(!process_try_wait_retired(43,&status,&result));process_lifetimes[0].members=1;
    assert(status==999 && result==999 && child->allocated && child->reapable);
    assert(!process_try_wait_retired(0,&status,&result) && !process_try_wait_retired(42,&status,&result));
    depth=1;unsigned old_begins=begins,old_ends=ends,old_checks=checkpoints;
    assert(process_try_wait_retired(43,&status,&result) && status==7 && result==43);
    assert(!child->allocated && !child->reapable && !process_lifetimes[0].members && current->child_user_us==12 && !current->pending_signals);
    assert(depth==1 && begins==old_begins && ends==old_ends && checkpoints==old_checks);
    assert(!process_try_wait_retired(43,&status,&result) && current->child_user_us==12);
    depth=0;next_pid=99;child->signal_ignore_mask=123;observed=NULL;
    struct process *replacement=alloc_process_lifetime("/fat/bin/replacement",true,0);
    assert(replacement==child && replacement->allocated && replacement->pid==99 && replacement->active && !replacement->reapable);
    assert(!replacement->signal_ignore_mask && !replacement->exit_status && !replacement->user_us && !replacement->lifetime);
    current=NULL;memset(processes,0,sizeof(processes));
    puts("retired-child: PASS empty reapable direct child only, single claim/accounting, unchanged foreign I/O owner");
    session_scope_contract();
    private_scope_contract();
    foreign_loader_contract();
    private_unmap_contract();
    startup_map_contract();
    startup_thread_contract();
    struct process *p=malloc(sizeof(*p));assert(p);observed=NULL;reset();
    clear_process(p);assert(begins==1 && ends==1 && yields && !depth && !p->mapping_count);
    observed=p;p->address_space=0x1000;
    struct process_service_scope outer=process_service_begin();assert(depth==1);
    for(unsigned i=0;i<65;i++)assert(map_process_page(p,0x500000+i*4096,VMM_PAGE_USER|VMM_PAGE_WRITABLE));
    assert(checkpoints>=2 && p->mapping_count==65);
    for(unsigned i=4096;i<66*4096;i++)assert(backing[i]==0);
    /* Mapping retirement invalidates/wakes a futex before its frame is freed. */
    futexes[0]=(struct process_futex){.used=true,.process=p,.address=0x500000,.wait_queue={.waiters=1}};
    unsigned before=yields;cleanup_process_address_space(p);
    assert(wakes==1 && futexes[0].invalidated && !futexes[0].wait_queue.waiters);
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
