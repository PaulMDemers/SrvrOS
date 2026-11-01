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
static bool available=true,fail_unmap,independent,bad_translation;
static unsigned unmap_calls,fail_at_unmap;
static uint64_t active_space=0x1000;
uint64_t vmm_current_address_space(void) { return active_space; }
static unsigned private_begins;
static void *private_target;
bool scheduler_io_begin_process_image(void *image) {
    private_begins++;if(!available || depth)return false;
    private_target=image;depth++;begins++;return true;
}
static struct process *observed;
static struct process *current;
void *scheduler_current_user_context(void) {return current;}
static void *current_user_thread;
void *scheduler_current_user_thread_context(void) {return current_user_thread;}
uint64_t gdt_default_kernel_stack_top(void) {return 0x10000;}
void fpu_init_state(struct fpu_state *state) {memset(state,0,sizeof(*state));}
bool scheduler_io_process_retired(void *p) {assert(p==&processes[1]);return independent;}
bool scheduler_io_process_independent(void *p) { assert(p==observed);return independent; }
static bool released[96];
static unsigned next_frame=1,wakes;
static unsigned char backing[96*4096];
void console_printf(const char *format,...) {(void)format;}
void scheduler_wake_all(struct scheduler_wait_queue *queue) {
    if(queue==&process_wait_queue){wakes++;return;}
    assert(queue==&futexes[0].wait_queue && futexes[0].invalidated && depth);
    int64_t i=process_mapping_index(observed,futexes[0].address);
    assert(i>=0 && !released[observed->mappings[i].physical_address/4096]);
    queue->waiters=0;wakes++;
}
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
        *physical=observed->mappings[i].physical_address;return !fail_unmap && ++unmap_calls!=fail_at_unmap;
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
    memset(futexes,0,sizeof(futexes));wakes=0;unmap_calls=fail_at_unmap=0;independent=bad_translation=false;
}
void console_write(const char *text) {(void)text;}
bool vmm_create_address_space(uint64_t *space) {*space=0x1000;return true;}
const struct vfs_node *vfs_lookup(const char *path) {assert(!strcmp(path,"/fat/bin/desktools"));return NULL;}
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
    assert(!depth && !child->address_space && destroyed==old_destroyed+1);observed=NULL;
    free(child);free(parent);puts("spawn-image-scope: PASS frozen path, fresh child only, ordinary fallback, nested ownership and unavailable gate");
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
    private_scope_contract();
    private_unmap_contract();
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
