/* Full copied-image spawn -> prepared scheduler entry -> real context return,
 * descriptor-free exit and final publication while a foreign owner stays held. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static uint64_t process_test_irq_save(void);
static void process_test_irq_restore(uint64_t);
static uint64_t process_test_rflags(void);
static void process_private_test_halt(void) __attribute__((noreturn));
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_SERVICE_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
#define SRVROS_PRIVATE_LIFECYCLE_HOST_TEST
#include "../kernel/src/process.c"
static bool irq,safe=true,refuse_view,fail_prepared,oom;
static uint64_t foreign_depth=2,foreign_owner=7,root=0x1000;
static struct process *subject;
static unsigned allocations,freed_frames,live_heap,prepared_calls,waits,ordinary_spawns,reads,notes;
static unsigned char frames[4096][4096],image[8192];
static bool frame_used[4096];
static uint64_t next_root=0x2000;
static scheduler_thread_fn selected_entry;
static void *selected_arg;
static jmp_buf exited;
static struct vfs_node dir={.used=true,.type=VFS_NODE_DIRECTORY};
static struct vfs_node executable={.used=true,.path="/fat/bin/app",.type=VFS_NODE_FILE,.metadata={.mode=0755}};
static uint64_t process_test_irq_save(void){uint64_t f=irq?0x200:0;irq=false;return f;}
static void process_test_irq_restore(uint64_t f){if(f&0x200)irq=true;}
static uint64_t process_test_rflags(void){return irq?0x200:0;}
static void process_private_test_halt(void){assert(0);abort();}
void *scheduler_current_user_context(void){return subject;}
void *scheduler_current_user_thread_context(void){return NULL;}
bool scheduler_io_process_atomic(void *p){assert(!irq && foreign_depth==2 && foreign_owner==7);return p==subject && safe;}
bool scheduler_io_begin(void){assert(!irq && foreign_depth==2 && foreign_owner==7);return false;}
bool scheduler_io_begin_process_image(void *p){(void)p;assert(0);return false;}
bool scheduler_io_active(void){return true;}
void scheduler_io_end(void){assert(0);}
void scheduler_io_wait(void){waits++;assert(0);}
void scheduler_io_checkpoint(void){assert(!irq && foreign_depth==2 && foreign_owner==7);}
void scheduler_io_set_user_safe(bool value){safe=value;}
bool scheduler_spawn(const char *n,scheduler_thread_fn e,void *p){(void)n;(void)e;(void)p;ordinary_spawns++;assert(0);return false;}
bool scheduler_spawn_prepared_process(const char *name,scheduler_thread_fn entry,void *p,uint64_t space,uint64_t generation){
 assert(!strcmp(name,"user") && !irq && p!=subject && space!=root && generation);
 struct process *child=p;assert(child->quiet && child->detached && child->mappings && child->mapping_count==USER_STACK_PAGES+1);
 prepared_calls++;if(fail_prepared)return false;selected_entry=entry;selected_arg=p;return true;
}
void scheduler_set_user_context(void *p,uint64_t space,uint64_t stack,uint64_t generation){
 assert(!irq && p==selected_arg && space==root && stack && generation);subject=p;
}
bool scheduler_enter_prepared_process(void *p,uint64_t r,uint64_t s,uint64_t g){scheduler_set_user_context(p,r,s,g);return true;}
bool scheduler_clear_user_context_private(void *p){assert(p==subject);scheduler_clear_user_context();return true;}
void scheduler_clear_user_context(void){assert(!irq);subject=NULL;root=0x9000;}
void scheduler_invalidate_current_user_trap(void){assert(!irq);}
void scheduler_kill_user_threads(void *p,void *except){assert(!irq && p==subject && !except);}
void scheduler_wake_all(struct scheduler_wait_queue *q){(void)q;assert(!irq);}
void scheduler_exit_current(void){assert(!irq && !subject && foreign_depth==2 && foreign_owner==7);longjmp(exited,1);}
void scheduler_yield(void){assert(0);}
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn f,void *p){(void)q;(void)f;(void)p;assert(0);return false;}
uint64_t vmm_current_address_space(void){return root;}
uint64_t vmm_kernel_address_space(void){return 0x9000;}
bool vmm_create_address_space(uint64_t *p){*p=next_root;next_root+=4096;return true;}
bool vmm_prepare_user_kernel_mappings(uint64_t p){assert(!irq && p);return true;}
void vmm_refresh_kernel_mappings(uint64_t p){assert(!irq && p==root);}
bool vmm_map_page_in_space(uint64_t r,uint64_t v,uint64_t p,uint64_t f){(void)r;(void)v;(void)f;assert(!irq && p && frame_used[p/4096]);return true;}
bool vmm_ensure_page_in_space(uint64_t r,uint64_t v,uint64_t p,uint64_t f){return vmm_map_page_in_space(r,v,p,f);}
bool vmm_virt_to_phys_in_space(uint64_t r,uint64_t v,uint64_t *out){
 for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++)if(processes[i].allocated && processes[i].address_space==r){
  int64_t at=process_mapping_index(&processes[i],page_down(v));if(at<0)return false;
  *out=processes[i].mappings[at].physical_address+(v&4095);return true;
 }return false;
}
bool vmm_unmap_page_in_space(uint64_t r,uint64_t v,uint64_t *p){return vmm_virt_to_phys_in_space(r,v,p);}
void vmm_destroy_address_space(uint64_t r){assert(!irq && r && r!=root);}
void *pmm_phys_to_virt(uint64_t p){assert(p<sizeof(frames));return (uint8_t *)frames+p;}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t minimum,enum pmm_frame_tag tag){
 (void)minimum;(void)tag;assert(!irq && n && allocations+n<4096);uint64_t at=allocations+1;
 for(unsigned i=0;i<n;i++)frame_used[at+i]=true;
 allocations+=(unsigned)n;return at*4096;
}
void pmm_free_frame(uint64_t p){assert(!irq && p%4096==0 && frame_used[p/4096]);frame_used[p/4096]=false;freed_frames++;}
void pmm_free_frame_range(uint64_t p,uint64_t n){for(uint64_t i=0;i<n;i++)pmm_free_frame(p+i*4096);}
void *kmalloc(uint64_t n){assert(!irq);if(oom)return NULL;void *p=malloc(n);assert(p);live_heap++;return p;}
void kfree(void *p){if(p){assert(!irq && live_heap);free(p);live_heap--;}}
uint64_t gdt_default_kernel_stack_top(void){return 0x8000;}
void fpu_init_state(struct fpu_state *p){memset(p,0,sizeof(*p));}
void console_printf(const char *f,...){(void)f;assert(0);}
void console_write(const char *s){(void)s;assert(0);}
const struct vfs_node *vfs_lookup(const char *path){if(!strcmp(path,executable.path))return &executable;return &dir;}
int64_t vfs_namespace_system_path(const char *path,char *out,size_t n){assert(strlen(path)<n);strcpy(out,path);return 0;}
bool exfat_readonly_memory_view(const struct vfs_node *n,const uint8_t **d,uint64_t *bytes){assert(!irq && n==&executable);if(refuse_view)return false;*d=image;*bytes=sizeof(image);return true;}
void vfs_note_read(const struct vfs_node *n){assert(n==&executable && !irq);notes++;}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **d,uint64_t *s){(void)n;(void)d;(void)s;reads++;assert(0);return false;}
void vfs_release_data(const struct vfs_node *n,const uint8_t *d){(void)n;(void)d;assert(0);}
bool file_stream_owned(uint64_t owner){(void)owner;return false;}
bool file_stream_cleanup(uint64_t owner){(void)owner;assert(0);return false;}
bool net_process_owned(struct process *p){assert(p==subject);return false;}
void net_process_cleanup(struct process *p){assert(p==subject);}
void net_process_wake(struct process *p){(void)p;assert(!irq);}
bool gui_server_is_owner(uint64_t p){(void)p;return false;}
bool gui_process_surfaces_owned(uint64_t p){(void)p;return false;}
void gui_process_cleanup(uint64_t p){assert(p==subject->pid && !irq);}
bool console_framebuffer_owned_by(uint64_t p){(void)p;return false;}
void console_release_framebuffer_mute_owner(uint64_t p){assert(p==subject->pid && !irq);}
void audio_process_cleanup(uint64_t p){assert(p==subject->pid && !irq);}
void audio_stream_snapshot(struct srv_pcm_status *s,uint64_t *owner){assert(!s);*owner=99;}
void usermode_enter(uint64_t entry,uint64_t stack,uint64_t argc,uint64_t argv,uint64_t envp,uint64_t tls){
 (void)argv;(void)envp;assert(entry==0x500000 && stack && argc==1 && !tls && !irq);process_exit(0);
}
int64_t net_close(uint64_t h){(void)h;assert(0);return -1;}
int64_t net_hold(uint64_t h){(void)h;assert(0);return -1;}
void pty_close(uint64_t h,bool m){(void)h;(void)m;assert(0);}
bool pty_ref(uint64_t h,bool m){(void)h;(void)m;assert(0);return false;}
bool exfat_write_file_retryable(const char *p,const uint8_t *d,uint64_t n,bool *r){(void)p;(void)d;(void)n;(void)r;assert(0);return false;}
void keyboard_wake_waiters(void){assert(!irq);}
void pty_wake(void){assert(!irq);}
struct pty_model *pty_state(uint64_t handle){(void)handle;assert(0);return NULL;}
void usermode_resume(const struct isr_frame *frame,uint64_t fs){(void)frame;(void)fs;assert(0);abort();}
int main(void){
 struct elf64_header *h=(void *)image;*h=(struct elf64_header){.type=ET_EXEC,.machine=EM_X86_64,.version=EV_CURRENT,.entry=0x500000,.phoff=sizeof(*h),.phentsize=sizeof(struct elf64_program_header),.phnum=1};
 h->ident[0]=ELFMAG0;h->ident[1]=ELFMAG1;h->ident[2]=ELFMAG2;h->ident[3]=ELFMAG3;h->ident[4]=ELFCLASS64;h->ident[5]=ELFDATA2LSB;h->ident[6]=EV_CURRENT;
 struct elf64_program_header *ph=(void *)(image+h->phoff);*ph=(struct elf64_program_header){.type=PT_LOAD,.flags=PF_X,.offset=4096,.vaddr=0x500000,.filesz=1,.memsz=4096,.align=4096};image[4096]=0xcc;
 next_pid=2;subject=&processes[0];subject->allocated=subject->active=true;subject->pid=1;subject->address_space=root;subject->cwd[0]='/';
 const char *args[]={executable.path};int64_t result=-99;
 refuse_view=true;assert(!process_try_spawn_private(executable.path,1,args,0,NULL,&result) && result==-99 && !allocations);refuse_view=false;
 safe=false;assert(!process_try_spawn_private(executable.path,1,args,0,NULL,&result) && !allocations);safe=true;
 uint64_t old_memsz=ph->memsz;ph->memsz=4096ull*4096;
 assert(!process_try_spawn_private(executable.path,1,args,0,NULL,&result) && !allocations);ph->memsz=old_memsz;
 oom=true;assert(process_try_spawn_private(executable.path,1,args,0,NULL,&result) && result==SRV_ERR_NOMEM && !live_heap && allocations==freed_frames);oom=false;
 fail_prepared=true;assert(process_try_spawn_private(executable.path,1,args,0,NULL,&result) && result==SRV_ERR_AGAIN && !live_heap && allocations==freed_frames);fail_prepared=false;
 assert(process_try_spawn_private(executable.path,1,args,0,NULL,&result) && result==4 && notes==3 && prepared_calls==2);
 struct process *child=selected_arg;assert(child->mapping_count==USER_STACK_PAGES+1 && !child->private_retiring);
 uint64_t physical;assert(vmm_virt_to_phys_in_space(child->address_space,0x500000,&physical) && *(uint8_t *)pmm_phys_to_virt(physical)==0xcc);
 subject=child;root=child->address_space;
 if(!setjmp(exited)){selected_entry(child);assert(0);}
 assert(child->reapable && !child->active && child->exit_status==0 && !child->mapping_count && !child->mappings && !child->address_space && !child->kernel_stack);
 assert(!live_heap && allocations==freed_frames && !ordinary_spawns && !reads && !waits && foreign_depth==2 && foreign_owner==7);
 puts("private-spawn: PASS full checked ELF copy, prepared entry, real exit/context return and final runtime/reap publication without waiting or changing foreign ownership");
}
