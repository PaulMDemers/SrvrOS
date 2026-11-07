/* Real detached spawn: load ELF, reject vectors, clean child without rebinding worker. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
static struct process *current;
static struct process parent_snapshot;
static struct process_user_thread *worker;
static uint64_t bound_cr3=0xabc000,bound_fs=0x123000,bound_tss=0x456000;
static struct fpu_state *bound_fpu;
static unsigned binding_calls, admissions, allocated,freed,stack_frees,destroyed,elf_reads,elf_releases;
static _Alignas(4096) unsigned char pages[1024][4096],kernel_stack[PROCESS_KERNEL_STACK_SIZE];
static bool freed_page[1024];
void *scheduler_current_user_context(void) { return current; }
void *scheduler_current_user_thread_context(void) { return worker; }
void fpu_init_state(struct fpu_state *state) { memset(state,0,sizeof(*state)); }
uint64_t gdt_default_kernel_stack_top(void) { return 0x456000; }
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t minimum,enum pmm_frame_tag tag) {
    if(tag==PMM_FRAME_KERNEL_STACK) { assert(n==PROCESS_KERNEL_STACK_SIZE/4096 && minimum==PROCESS_KERNEL_STACK_MIN_PHYSICAL);return 0x1000000; }
    assert(n==1 && tag==PMM_FRAME_USER && minimum==PROCESS_USER_PAGE_MIN_PHYSICAL);
    assert(allocated+1<1024);return (uint64_t)++allocated*4096;
}
void *pmm_phys_to_virt(uint64_t p) {
    if(p==0x1000000)return kernel_stack;
    assert(p/4096<=allocated);return pages[p/4096]+p%4096;
}
void pmm_free_frame(uint64_t p) { assert(p && p%4096==0 && p/4096<=allocated && !freed_page[p/4096]);freed_page[p/4096]=true;freed++; }
void pmm_free_frame_range(uint64_t p,uint64_t n) { assert(p==0x1000000 && n==PROCESS_KERNEL_STACK_SIZE/4096);stack_frees++; }
bool vmm_create_address_space(uint64_t *out) { *out=3;return true; }
bool vmm_prepare_user_kernel_mappings(uint64_t root) { (void)root;assert(!"vector rejection must precede preparation");return false; }
void vmm_destroy_address_space(uint64_t space) { assert(space==3);destroyed++; }
bool vmm_map_page_in_space(uint64_t space,uint64_t virt,uint64_t physical,uint64_t flags) { assert(space==3 && virt && physical && (flags&VMM_PAGE_USER));return true; }
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t virt,uint64_t *physical) {
    assert(space==3);struct process *child=&processes[1];
    for(unsigned i=0;i<child->mapping_count;i++)if(child->mappings[i].virtual_address==(virt&~4095ull)) { *physical=child->mappings[i].physical_address+(virt&4095);return true; }
    return false;
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t virt,uint64_t *physical) { return vmm_virt_to_phys_in_space(space,virt,physical); }
void console_printf(const char *s,...) { (void)s; }
void console_write(const char *s) { (void)s; }
bool exfat_write_file_mode(const char *p,const uint8_t *d,uint64_t n,uint64_t m) { (void)p;(void)d;(void)n;(void)m;abort(); }
bool exfat_write_file_retryable(const char *p,const uint8_t *d,uint64_t n,bool *r) { (void)p;(void)d;(void)n;(void)r;abort(); }
void *kmalloc(size_t n) { assert(n==sizeof(struct process_mapping)*PROCESS_MAX_MAPPINGS);return malloc(n); }
void kfree(void *p) { free(p); }
int64_t net_close(uint64_t h) { (void)h;abort(); }
int64_t net_hold(uint64_t h) { (void)h;abort(); }
bool pty_ref(uint64_t h,bool m) { (void)h;(void)m;abort(); }
void pty_close(uint64_t h,bool m) { (void)h;(void)m;abort(); }
int64_t pty_set_flags(uint64_t h,bool m,uint64_t f) { (void)h;(void)m;(void)f;abort(); }
struct pty_model *pty_state(uint64_t h) { (void)h;abort(); }
uint64_t process_context_save(struct process_context *context) { (void)context;abort(); }
void usermode_enter(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;abort(); }
static uint8_t image[9000];
static const struct vfs_node executable={.used=true,.type=VFS_NODE_FILE,.metadata={.mode=0755}};
static const struct vfs_node directory={.used=true,.type=VFS_NODE_DIRECTORY};
uint64_t vfs_count(void) { return 0; }
const struct vfs_node *vfs_node_at(uint64_t i) { (void)i;return NULL; }
void vfs_set_retire_callback(vfs_retire_fn callback) { (void)callback; }
const struct vfs_node *vfs_lookup(const char *path) { return !strcmp(path,"/") || !strcmp(path,"/fat") || !strcmp(path,"/fat/bin") ? &directory : &executable; }
bool vfs_read_all(const struct vfs_node *node,const uint8_t **data,uint64_t *size) { assert(node==&executable);elf_reads++;*data=image;*size=sizeof(image);return true; }
void vfs_release_data(const struct vfs_node *node,const uint8_t *data) { assert(node==&executable && data==image);elf_releases++; }
void scheduler_set_user_context(void *p,uint64_t cr3,uint64_t tss,uint64_t generation) { (void)generation;binding_calls++;current=p;bound_cr3=cr3;bound_tss=tss; }
bool scheduler_spawn(const char *name,scheduler_thread_fn entry,void *p) { (void)name;(void)entry;(void)p;admissions++;return false; }
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
    worker=&parent->user_threads[0];*worker=(struct process_user_thread){.used=true,.active=true,.owner=parent,.tid=2,.tls_thread_pointer=bound_fs,.stack_top=0x900000};
    memset(&worker->fpu,0x5a,sizeof(worker->fpu));
    bound_fpu=&worker->fpu;current=parent;
    memcpy(&parent_snapshot,parent,sizeof(*parent));
    struct process_user_thread before=*worker;char *argv[]={"echo",NULL};
    int64_t result=process_spawn_exec("/fat/bin/echo",0,(const char *const *)argv,0,NULL,true,0,1,2,0,false,NULL,0,0,0,0);
    assert(result==SRV_ERR_NOMEM && elf_reads==1 && elf_releases==1);
    assert(allocated>=USER_STACK_PAGES+1 && freed==allocated && destroyed==1 && stack_frees==1);
    assert(!binding_calls && !admissions && current==parent && worker==&parent->user_threads[0]);
    assert(bound_cr3==0xabc000 && bound_fs==0x123000 && bound_tss==0x456000 && bound_fpu==&worker->fpu);
    assert(!memcmp(worker,&before,sizeof(before)) && !memcmp(parent,&parent_snapshot,sizeof(*parent)));
    assert(!processes[1].allocated && !process_slots_busy[1]);
    puts("process-spawn-vector-failure: PASS (real ELF load, child cleanup, modeled worker binding)");
}

void net_process_wake(struct process *p) { (void)p;assert(!"No live scope in failed-spawn fixture"); }
void keyboard_wake_waiters(void) { assert(!"No live scope in failed-spawn fixture"); }
void pty_wake(void) { assert(!"No live scope in failed-spawn fixture"); }
