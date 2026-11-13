/* Real lifecycle admission/unmap code with a foreign owner held throughout.
 * Any namespace/device callback or ownership change is an unexpected symbol. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void process_private_test_halt(void) __attribute__((noreturn));
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PRIVATE_LIFECYCLE_HOST_TEST
#include "../kernel/src/process.c"
static struct process p;
static struct process_mapping mappings[4097];
static void *worker;
static bool atomic=true,stream_owned,network_owned,server_owned,surface_owned,mute_owned,bad_pte,unmap_fail;
static uint64_t audio_owner;
static unsigned freed,unmapped,begin_calls,end_calls,wait_calls;
static jmp_buf stopped;
void *scheduler_current_user_context(void){return &p;}
void *scheduler_current_user_thread_context(void){return worker;}
bool scheduler_io_process_atomic(void *owner){assert(owner==&p);return atomic;}
bool file_stream_owned(uint64_t owner){assert(owner==p.stream_instance);return stream_owned;}
bool net_process_owned(struct process *owner){assert(owner==&p);return network_owned;}
bool gui_server_is_owner(uint64_t pid){assert(pid==p.pid);return server_owned;}
bool gui_process_surfaces_owned(uint64_t pid){assert(pid==p.pid);return surface_owned;}
bool console_framebuffer_owned_by(uint64_t pid){assert(pid==p.pid);return mute_owned;}
void audio_stream_snapshot(struct srv_pcm_status *out,uint64_t *owner){assert(!out);*owner=audio_owner;}
uint64_t vmm_current_address_space(void){return 0x2000;}
uint64_t vmm_kernel_address_space(void){return 0x1000;}
bool vmm_virt_to_phys_in_space(uint64_t root,uint64_t page,uint64_t *physical){
 assert(root==0x2000);int64_t i=process_mapping_index(&p,page);
 if(i<0)return false;
 *physical=mappings[i].physical_address+(bad_pte?4096:0);return true;
}
bool vmm_unmap_page_in_space(uint64_t root,uint64_t page,uint64_t *physical){
 assert(root==0x2000);int64_t i=process_mapping_index(&p,page);assert(i>=0);
 unmapped++;if(unmap_fail)return false;*physical=mappings[i].physical_address;return true;
}
void pmm_free_frame(uint64_t frame){assert(frame && frame%4096==0);freed++;}
void kfree(void *ptr){assert(ptr==mappings);}
void scheduler_wake_all(struct scheduler_wait_queue *queue){(void)queue;}
static void process_private_test_halt(void){longjmp(stopped,1);}
bool scheduler_io_begin(void){begin_calls++;return false;}
void scheduler_io_end(void){end_calls++;assert(0);}
void scheduler_io_wait(void){wait_calls++;assert(0);}
static void reset(void){
 memset(&p,0,sizeof(p));memset(mappings,0,sizeof(mappings));
 p.allocated=p.active=true;p.pid=42;p.address_space=0x2000;p.mappings=mappings;p.mapping_count=3;p.stream_instance=7;
 for(unsigned i=0;i<3;i++)mappings[i]=(struct process_mapping){.virtual_address=0x400000+i*4096,.physical_address=0x800000+i*4096};
 for(unsigned i=0;i<3;i++)p.files[i]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=i};
 worker=NULL;atomic=true;stream_owned=network_owned=server_owned=surface_owned=mute_owned=bad_pte=unmap_fail=false;
 audio_owner=99;freed=unmapped=0;
}
int main(void){
 reset();assert(process_private_exit_admit() && p.private_retiring);
 cleanup_process_mappings(&p);assert(freed==3 && unmapped==3 && !p.mapping_count && !p.mappings);
 /* A post-proof unmap failure retains the frame and mapping and fail-stops. */
 reset();assert(process_private_exit_admit());unmap_fail=true;
 if(!setjmp(stopped)){cleanup_process_mappings(&p);assert(0);}
 assert(!freed && p.mapping_count==3 && p.mappings==mappings && unmapped==1);
 for(unsigned reason=0;reason<13;reason++){
  reset();switch(reason){
   case 0:atomic=false;break;case 1:stream_owned=true;break;case 2:network_owned=true;break;
   case 3:server_owned=true;break;case 4:surface_owned=true;break;case 5:mute_owned=true;break;
   case 6:audio_owner=p.pid;break;case 7:p.owned_user_scope=1;break;case 8:bad_pte=true;break;
   case 9:worker=&p.user_threads[0];break;case 10:p.mapping_count=4097;break;
   case 11:p.address_space=0x1000;break;case 12:p.active=false;break;
  }
  assert(!process_private_exit_admit() && !p.private_retiring && !freed && !unmapped);
 }
 for(unsigned type=PROCESS_FILE_VFS;type<=PROCESS_FILE_PTY_SLAVE;type++){
  reset();p.files[4]=(struct process_file){.used=true,.type=type};
  assert(!process_private_exit_admit() && !freed && !unmapped);
 }
 reset();worker=&p.user_threads[0];struct process_user_thread *t=worker;
 *t=(struct process_user_thread){.used=true,.active=true,.owner=&p};
 p.tls_memsz=16;p.tls_align=16;t->tls_base=USER_TLS_BASE+4096;t->tls_thread_pointer=t->tls_base+16;
 mappings[0].virtual_address=t->tls_base;
 bad_pte=true;assert(!process_private_thread_retire() && !freed && t->tls_base);bad_pte=false;
 atomic=false;assert(!process_private_thread_retire() && !freed);atomic=true;
 unmap_fail=true;assert(!process_private_thread_retire() && !freed && t->tls_base);unmap_fail=false;
 assert(process_private_thread_retire() && freed==1 && p.mapping_count==2 && !t->tls_base && !t->tls_thread_pointer);
 assert(!begin_calls && !end_calls && !wait_calls);
 puts("private-lifecycle: PASS foreign owner retained, callback resources refuse, checked PTE/TLS retirement, unexpected unmap retains frame and fail-stops");
}
