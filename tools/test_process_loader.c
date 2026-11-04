/* Model a timer preemption during allocation: a second executable maps its
 * segment and stack before the first resumes. Every page must stay in its
 * owning address space, including identical virtual addresses. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
void scheduler_wake_all(struct scheduler_wait_queue *queue) { (void)queue; }
bool exfat_write_file_retryable(const char *p,const uint8_t *d,uint64_t n,bool *r) {(void)p;(void)d;(void)n;(void)r;assert(0);return false;}
int64_t net_close(uint64_t h) {(void)h;assert(0);return -1;}
int64_t net_hold(uint64_t h) {(void)h;assert(0);return -1;}
void pty_close(uint64_t h,bool m) {(void)h;(void)m;assert(0);}
bool pty_ref(uint64_t h,bool m) {(void)h;(void)m;assert(0);return false;}
static struct process first, second,clone;
static struct process *current;
static bool exec_commit_test;
static jmp_buf exec_commit;
/* Exec now allocates privately and performs its own batched initialization. */
void *kmalloc(size_t size) {assert(size==sizeof(clone));memset(&clone,0xa5,sizeof(clone));return &clone;}
bool scheduler_io_active(void) {return false;}
void *kcalloc(size_t count,size_t size) {assert(count==1 && size==sizeof(clone));memset(&clone,0,sizeof(clone));return &clone;}
void kfree(void *p) {assert(p==&clone);}
void scheduler_clear_user_context(void) {assert(exec_commit_test && current && !current->owned_user_scope);longjmp(exec_commit,1);}
void scheduler_set_user_context(void *p,uint64_t as,uint64_t stack) {(void)p;(void)as;(void)stack;assert(0);}
void usermode_enter(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f) {(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;abort();}
static unsigned destroyed_spaces;
bool vmm_create_address_space(uint64_t *out) { *out=3;return true; }
void vmm_destroy_address_space(uint64_t space) { assert(space==3);destroyed_spaces++; }
static uint8_t frames[4096][4096];
static unsigned allocated, freed, fail_after;
static bool freed_frames[4096];
static bool interleave,allocating,releasing;
static struct process *nested;
void *scheduler_current_user_context(void) { return current; }
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
void console_printf(const char *format,...) { (void)format; }
void console_write(const char *s) { (void)s; }
void pmm_free_frame(uint64_t address) {
    assert(address && address%4096==0 && address/4096<=allocated);
    assert(!freed_frames[address/4096]);freed_frames[address/4096]=true;freed++;
}
uint64_t pmm_alloc_frames_above_tagged(uint64_t count,uint64_t minimum,enum pmm_frame_tag tag) {
    assert(count==1 && minimum==PROCESS_USER_PAGE_MIN_PHYSICAL && tag==PMM_FRAME_USER);
    if(interleave) {
        interleave=false;
        assert(load_segment(&second,image,sizeof(image),&segment));
        assert(map_user_stack(&second,0x1000000));
    }
    if(fail_after && allocated==fail_after) return 0;
    assert(allocated+1<4096);return (uint64_t)++allocated*4096;
}
void *pmm_phys_to_virt(uint64_t physical) {
    assert(physical/4096<=allocated);return frames[physical/4096]+physical%4096;
}
bool vmm_map_page_in_space(uint64_t space,uint64_t virt,uint64_t physical,uint64_t flags) {
    assert(space==1 || space==2 || space==3);assert(virt%4096==0 && physical%4096==0);
    assert(flags&VMM_PAGE_USER);return true;
}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t virt,uint64_t *physical) {
    struct process *p=space==1?&first:space==2?&second:&clone;
    for(unsigned i=0;i<p->mapping_count;i++) if(p->mappings[i].virtual_address==(virt&~4095ull)) {
        *physical=p->mappings[i].physical_address+(virt&4095);return true;
    }
    return false;
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t virt,uint64_t *physical) {
    return vmm_virt_to_phys_in_space(space,virt,physical);
}
static uint8_t tls_byte(uint64_t address) {
    uint64_t physical;assert(vmm_virt_to_phys_in_space(1,address,&physical));
    return *(uint8_t*)pmm_phys_to_virt(physical);
}
static void tls_lifecycle(void) {
    unsigned before=first.mapping_count;
    first.tls_memsz=4096;first.tls_filesz=3;first.tls_align=16;
    first.tls_template[0]=21;first.tls_template[1]=42;first.tls_template[2]=63;
    assert(process_setup_tls_block(&first,0,&first.tls_base,&first.tls_thread_pointer));
    assert(first.mapping_count==before+2);
    uint64_t base=0,tp=0;
    /* Failure on the second page must retire the first, without damaging main TLS. */
    fail_after=allocated+1;unsigned free_before=freed;
    assert(!process_setup_tls_block(&first,1,&base,&tp));fail_after=0;
    assert(first.mapping_count==before+2 && freed==free_before+1);
    struct process_user_thread worker={.owner=&first};
    for(unsigned round=0;round<16;round++) {
        assert(process_setup_tls_block(&first,1,&worker.tls_base,&worker.tls_thread_pointer));
        assert(worker.tls_base==USER_TLS_BASE+8192 && worker.tls_thread_pointer==worker.tls_base+4096);
        assert(tls_byte(worker.tls_base)==21 && tls_byte(worker.tls_base+1)==42 && tls_byte(worker.tls_base+2)==63);
        assert(tls_byte(worker.tls_base+3)==0 && tls_byte(worker.tls_base+4095)==0);
        uint64_t physical;assert(vmm_virt_to_phys_in_space(1,worker.tls_thread_pointer,&physical));
        assert(*(uint64_t*)pmm_phys_to_virt(physical)==worker.tls_thread_pointer);
        assert(!process_setup_tls_block(&first,1,&base,&tp));
        assert(first.mapping_count==before+4);
        process_release_thread_tls(&worker);
        assert(!worker.tls_base && !worker.tls_thread_pointer && first.mapping_count==before+2);
        assert(tls_byte(first.tls_base)==21);
    }
    assert(!process_setup_tls_block(&first,PROCESS_MAX_USER_THREADS+1,&base,&tp));
    puts("process-loader: TLS initialized template, zero fill, isolation, rollback and slot reclamation PASS");
}
static void clone_lifecycle(void) {
    unsigned parent_pages=first.mapping_count,free_before=freed;
    fail_after=allocated+2;
    assert(!process_clone_address_space(&clone,&first));
    assert(clone.mapping_count==2 && first.mapping_count==parent_pages);
    cleanup_process_address_space(&clone);fail_after=0;
    assert(!clone.mapping_count && !clone.address_space && freed==free_before+2 && destroyed_spaces==1);
    assert(process_clone_address_space(&clone,&first) && clone.mapping_count==parent_pages);
    for(unsigned i=0;i<parent_pages;i++) {
        assert(clone.mappings[i].virtual_address==first.mappings[i].virtual_address);
        assert(clone.mappings[i].flags==first.mappings[i].flags);
        assert(clone.mappings[i].physical_address!=first.mappings[i].physical_address);
        assert(!memcmp(pmm_phys_to_virt(clone.mappings[i].physical_address),pmm_phys_to_virt(first.mappings[i].physical_address),4096));
    }
    uint8_t *child_byte=pmm_phys_to_virt(clone.mappings[0].physical_address);
    uint8_t *parent_byte=pmm_phys_to_virt(first.mappings[0].physical_address);
    uint8_t original=*parent_byte;*child_byte^=0xff;assert(*parent_byte==original);
    cleanup_process_address_space(&clone);
    assert(freed==free_before+2+parent_pages && first.mapping_count==parent_pages && destroyed_spaces==2);
    puts("process-loader: fork private pages, flags, parent preservation and partial-copy rollback PASS");
}

static const uint8_t *executable_data = image;
static uint64_t executable_size = sizeof(image);
static struct vfs_node executable = {.type=VFS_NODE_FILE, .metadata={.mode=0755}};
static const struct vfs_node fixture_directory={.used=true,.type=VFS_NODE_DIRECTORY};
uint64_t vfs_count(void) { return 0; }
const struct vfs_node *vfs_node_at(uint64_t i) { (void)i;return NULL; }
void vfs_set_retire_callback(vfs_retire_fn callback) { (void)callback; }
const struct vfs_node *vfs_lookup(const char *path) {
    if(!strcmp(path,"/") || !strcmp(path,"/fat") || !strcmp(path,"/fat/bin")) return &fixture_directory;
    return &executable;
}
bool vfs_read_all(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    (void)node; *data=executable_data; *size=executable_size; return true;
}
void vfs_release_data(const struct vfs_node *node, const uint8_t *data) { (void)node; (void)data; }
static void test_elf_loading(void) {
    memset(image,0,sizeof(image));
    struct elf64_header *h=(struct elf64_header *)image;
    h->ident[0]=ELFMAG0; h->ident[1]=ELFMAG1; h->ident[2]=ELFMAG2; h->ident[3]=ELFMAG3;
    h->ident[4]=ELFCLASS64; h->ident[5]=ELFDATA2LSB; h->ident[6]=EV_CURRENT;
    h->type=ET_EXEC; h->machine=EM_X86_64; h->version=EV_CURRENT;
    h->phoff=sizeof(*h); h->phentsize=sizeof(struct elf64_program_header); h->phnum=2;
    struct elf64_program_header *ph=(void *)(image+h->phoff);
    ph[0]=(struct elf64_program_header){.type=PT_LOAD,.flags=PF_X,.vaddr=0x500000,.offset=4096,.filesz=3,.memsz=4096,.align=4096};
    ph[1]=(struct elf64_program_header){.type=PT_LOAD,.flags=PF_W,.vaddr=0x502000,.offset=8192,.filesz=3,.memsz=4096,.align=4096};
    image[4096]=0xaa; image[8192]=0xbb;
    uint64_t entry;
    h->entry=0x1000000500000ull;
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==SRV_ERR_NOEXEC);
    h->entry=0x502000; /* Mapped data is not an executable entry. */
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==SRV_ERR_NOEXEC);
    h->entry=0x500000;
    ph[1].vaddr=0x1000000502000ull;
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==SRV_ERR_NOEXEC);
    ph[1].vaddr=0xfffffffffffff000ull; ph[1].memsz=8192;
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==SRV_ERR_NOEXEC);
    ph[1].vaddr=0x502000; ph[1].memsz=4096; ph[1].align=3;
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==SRV_ERR_NOEXEC);
    ph[1].align=4096; ph[1].offset=8193;
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==SRV_ERR_NOEXEC);
    ph[1].offset=8192;
    assert(load_elf_image_checked_impl("test",&clone,USER_STACK_TOP,&entry)==0 && entry==0x500000);
    uint64_t physical;
    assert(vmm_virt_to_phys_in_space(3,0x500000,&physical) && *(uint8_t *)pmm_phys_to_virt(physical)==0xaa);
    assert(vmm_virt_to_phys_in_space(3,0x502000,&physical) && *(uint8_t *)pmm_phys_to_virt(physical)==0xbb);
    cleanup_process_address_space(&clone);
    memset(freed_frames,0,sizeof(freed_frames)); allocated=freed=0; destroyed_spaces=0;
}
static void test_scope_exec_hooks(void) {
    current=&processes[0];clear_process(current);current->allocated=current->active=true;current->pid=44;
    uint64_t token=0;assert(!process_scope_create(&token));
    const char *args[]={"test"};
    int64_t result=process_exec_replace("/fat/bin/test",1,args,0,NULL,123,-1,-1,0,0,0);
    if(result!=SRV_ERR_BADF) fprintf(stderr,"exec result %lld\n",(long long)result);
    assert(result==SRV_ERR_BADF);
    assert(current->owned_user_scope==token && lifetime_find(token));
    exec_commit_test=true;
    if(!setjmp(exec_commit)) process_exec_replace("/fat/bin/test",1,args,0,NULL,-1,-1,-1,0,0,0);
    exec_commit_test=false;assert(!current->owned_user_scope && !lifetime_find(token));
    cleanup_process_address_space(&clone);current=NULL;clear_process(&processes[0]);
    memset(freed_frames,0,sizeof(freed_frames));allocated=freed=destroyed_spaces=0;
    puts("session-scope-exec: PASS failed stdio retains owner; successful commit detaches before old image retirement");
}

static void test_real_elf(const char *path) {
    FILE *f=fopen(path,"rb"); assert(f); assert(!fseek(f,0,SEEK_END));
    long n=ftell(f); assert(n>0 && !fseek(f,0,SEEK_SET));
    uint8_t *data=malloc((size_t)n); assert(data && fread(data,1,(size_t)n,f)==(size_t)n); fclose(f);
    executable_data=data; executable_size=(uint64_t)n;
    uint64_t entry; assert(load_elf_image_checked_impl(path,&clone,USER_STACK_TOP,&entry)==0);
    cleanup_process_address_space(&clone); free(data);
    executable_data=image; executable_size=sizeof(image);
    memset(freed_frames,0,sizeof(freed_frames)); allocated=freed=0; destroyed_spaces=0;
}

int main(int argc, char **argv) {
    test_elf_loading();
    test_scope_exec_hooks();
    for(int i=1;i<argc;i++) test_real_elf(argv[i]);
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
    tls_lifecycle();
    clone_lifecycle();
    puts("process-loader: PASS reserved process slots, publication/release interleaving, executable segment and stack ownership");
}
