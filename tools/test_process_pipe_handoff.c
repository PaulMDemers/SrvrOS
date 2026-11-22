/* PRIVATE PROPOSAL fixture: production process constructor/ELF preparation,
 * real lifetime table and ordinary pipe byte/ref operations. PMM/VMM/scheduler
 * are explicitly HOST models. No guest execution/SYS_PROC_READY or networking
 * acceptance follows; startup/exit state transitions are injected below. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
static bool handoff_mode=true,handoff_spawn_ok=true,handoff_live,handoff_revoke_prepare;
static unsigned handoff_allocations;
static void *handoff_child;
static uint64_t handoff_token;
#define main image_legacy_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "test_process_pipe_handoff_support.inc"
#undef main
#pragma GCC diagnostic pop
int64_t pty_get_flags(uint64_t handle,bool master) {(void)handle;(void)master;abort();}
static struct process_pipe_handoff tickets[80];
static unsigned used,groups;
static void image_setup(void) {
 struct elf64_header *h=(void *)image;memset(image,0,sizeof image);
 h->ident[0]=ELFMAG0;h->ident[1]=ELFMAG1;h->ident[2]=ELFMAG2;h->ident[3]=ELFMAG3;
 h->ident[4]=ELFCLASS64;h->ident[5]=ELFDATA2LSB;h->ident[6]=EV_CURRENT;
 h->type=ET_EXEC;h->machine=EM_X86_64;h->version=EV_CURRENT;h->entry=0x500000;
 h->phoff=sizeof(*h);h->phentsize=sizeof(struct elf64_program_header);h->phnum=1;
 struct elf64_program_header *ph=(void *)(image+h->phoff);
 *ph=(struct elf64_program_header){.type=PT_LOAD,.flags=PF_X,.vaddr=0x500000,.offset=4096,.filesz=3,.memsz=4096,.align=4096};
 image[4096]=0xaa;image[4097]=0xbb;image[4098]=0xcc;
 image_prepare_ok=true;handoff_revoke_prepare=false;
 allocated=freed=0;memset(freed_page,0,sizeof freed_page);stack_frees=destroyed=0;
 current=NULL;worker=NULL;handoff_child=NULL;handoff_spawn_ok=true;
 handoff_live=false;handoff_allocations=0;simulated_flags=1ull<<9;
}
static struct process_pipe_handoff *fresh(void) {
 image_setup();assert(used<80);handoff_token=process_lifetime_create();assert(handoff_token);
 return &tickets[used++];
}
static struct process *spawn(struct process_pipe_handoff *t) {
 int64_t pid=process_lifetime_spawn_pipe(handoff_token,"/httpget","--wifi-exact 3",t);
 assert(pid>0 && handoff_child && !current && !binding_calls);
 struct process *p=handoff_child;assert(p->pid==(uint64_t)pid && p->lifetime==handoff_token && p->image_generation);
 assert(p->files[3].used && p->files[3].type==PROCESS_FILE_PIPE_READ && !p->files[4].used && process_file_nonblocking(p,3));
 assert(p->quiet && p->parent_pid==0 && allocated>USER_STACK_PAGES && elf_reads==elf_releases);
 return p;
}
static struct process_lifetime *life(struct process_pipe_handoff *t) { return lifetime_find(t->token); }
static void retire(struct process *p,struct process_pipe_handoff *t) {
 handoff_live=false;assert(!current);assert(process_pipe_handoff_finish(t));
 assert(cleanup_process_files(p));cleanup_process_address_space(p);p->active=false;p->reapable=true;
 assert(process_lifetime_revoke(t->token) && process_lifetime_drain(t->token));
 struct process_lifetime_status status;assert(process_lifetime_status(t->token,&status) && !status.members);
 assert(process_lifetime_release(t->token));assert(freed==allocated && stack_frees==1);
 for(unsigned i=0;i<PROCESS_MAX_PIPES;i++)assert(!pipes[i].used);
}
static void positive(void) {
 struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);uint8_t result[128];
 assert(!process_pipe_handoff_current(t));
 assert(process_file_pipe_read_mode(p,3,result,sizeof result,true)==SRV_ERR_AGAIN);
 p->startup_ready=true;assert(process_pipe_handoff_current(t));
 struct process_pipe_identity ident={0};
 assert(process_pipe_handoff_identity(t,&ident) && ident.pid==p->pid &&
        ident.lifetime==t->token && ident.image_generation==p->image_generation);
 assert(!process_pipe_handoff_identity(t,NULL));
 assert(process_lifetime_revoke(t->token));
 uint8_t record[128];for(unsigned i=0;i<sizeof record;i++)record[i]=(uint8_t)(i*7+3);
 unsigned before=handoff_allocations;uint64_t handle=life(t)->handoff.pipe;
 handoff_live=true;assert(process_pipe_handoff_publish(t,record,sizeof record)==sizeof record);
 assert(handoff_allocations==before && !life(t)->handoff.owns_writer && life(t)->handoff.published);
 assert(pipes[handle-1].size==128 && pipes[handle-1].read_refs==1 && !pipes[handle-1].write_refs);
 assert(process_file_pipe_read_mode(p,3,result,11,true)==11 && !memcmp(result,record,11));
 assert(process_file_pipe_read_mode(p,3,result+11,sizeof result-11,true)==117 && !memcmp(result,record,128));
 assert(process_file_pipe_read_mode(p,3,result,1,true)==0);
 assert(process_pipe_handoff_publish(t,record,1)==SRV_ERR_BADF && !process_pipe_handoff_current(t));
 assert(process_pipe_handoff_finish(t) && process_pipe_handoff_finish(t));
 assert(handoff_allocations==before && simulated_flags==(1ull<<9));retire(p,t);groups++;
}
static void startup_and_spawn_failure(void) {
 for(unsigned mode=0;mode<4;mode++) {
  struct process_pipe_handoff *t=fresh();
  if(mode==0)handoff_spawn_ok=false;
  if(mode==1)image_prepare_ok=false;
  if(mode==2)image[0]=0;
  if(mode==3)handoff_revoke_prepare=true;
  assert(process_lifetime_spawn_pipe(t->token? t->token:handoff_token,"/httpget","--wifi-exact 3",t)<0);
  assert(process_pipe_handoff_finish(t));
  struct process_lifetime_status state;assert(process_lifetime_status(handoff_token,&state) && !state.members);
  for(unsigned i=0;i<PROCESS_MAX_PIPES;i++)assert(!pipes[i].used);
  assert(process_lifetime_revoke(handoff_token) && process_lifetime_release(handoff_token));
  assert(freed==allocated && stack_frees==1);
 }
 struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);uint8_t byte=1;
 assert(process_lifetime_revoke(t->token));handoff_live=true;
 assert(process_pipe_handoff_publish(t,&byte,1)==SRV_ERR_BADF && !life(t)->handoff.owns_writer);
 assert(process_file_pipe_read_mode(p,3,&byte,1,true)==0);retire(p,t);groups++;
}
static void argument_and_startup_abort(void) {
 for(unsigned mode=0;mode<4;mode++) {
  struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);uint8_t record[129]={0};
  p->startup_ready=true;if(mode!=3)assert(process_lifetime_revoke(t->token));
  unsigned before=handoff_allocations;handoff_live=true;
  const uint8_t *source=mode==0?NULL:record;
  uint64_t count=mode==1?0:mode==2?129:1;
  assert(process_pipe_handoff_publish(t,source,count)<0);
  assert(!life(t)->handoff.owns_writer && life(t)->handoff.closed && handoff_allocations==before);
  assert(process_file_pipe_read_mode(p,3,record,1,true)==0);retire(p,t);
 }
 struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);uint8_t byte;
 /* Parent startup timeout path closes without attempting publication. */
 assert(!p->startup_ready && process_pipe_handoff_finish(t));
 assert(process_file_pipe_read_mode(p,3,&byte,1,true)==0);retire(p,t);
 groups++;
}
static void refusal(void) {
 for(unsigned mode=0;mode<12;mode++) {
  struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);p->startup_ready=true;
  assert(process_lifetime_revoke(t->token));struct process_lifetime *l=life(t);uint8_t byte=1;
  uint64_t old_image=p->image_generation,old_as=p->address_space,old_handle=p->files[3].handle;
  if(mode==0)p->killed=true;
  if(mode==1)p->image_generation++;
  if(mode==2)p->file_generations[3]++;
  if(mode==3)p->files[3].type=PROCESS_FILE_PIPE_WRITE;
  if(mode==4)p->address_space++;
  if(mode==5)l->members++;
  if(mode==6)p->user_threads[0].used=true;
  if(mode==7){pipe_ref(old_handle,false);}
  if(mode==8){pipes[old_handle-1].size=1;pipes[old_handle-1].buffer[0]=0x7f;}
  if(mode==9){pipes[old_handle-1].rights[0].used=true;}
  if(mode==10)p->files[3].handle=old_handle+1;
  if(mode==11)p->files[3].used=false;
  struct process_pipe_identity ident={.pid=0xa5};
  assert(!process_pipe_handoff_current(t) && !process_pipe_handoff_identity(t,&ident) && ident.pid==0xa5);
  unsigned before=handoff_allocations;handoff_live=true;
  assert(process_pipe_handoff_publish(t,&byte,1)==SRV_ERR_BADF);
  assert(handoff_allocations==before && !l->handoff.owns_writer && l->handoff.closed);
  assert(pipes[old_handle-1].size==(mode==8?1u:0u));
  handoff_live=false;p->killed=false;p->image_generation=old_image;p->address_space=old_as;
  p->files[3].used=true;p->files[3].handle=old_handle;p->files[3].type=PROCESS_FILE_PIPE_READ;
  if(mode==5)l->members--;
  if(mode==6)p->user_threads[0].used=false;
  if(mode==7)pipe_close(old_handle,false);
  if(mode==9)pipes[old_handle-1].rights[0].used=false;
  retire(p,t);
 }
 groups++;
}
static void replay_and_release(void) {
 struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);p->startup_ready=true;
 assert(process_lifetime_revoke(t->token));struct process_pipe_handoff copy=*t,old=*t;uint8_t byte=1;
 assert(!process_pipe_handoff_current(&copy) && !process_pipe_handoff_finish(&copy));
 assert(process_pipe_handoff_publish(&copy,&byte,1)==SRV_ERR_BADF && life(t)->handoff.owns_writer);
 assert(process_pipe_handoff_finish(t));*t=old;
 assert(!process_pipe_handoff_current(t) && process_pipe_handoff_publish(t,&byte,1)==SRV_ERR_BADF);
 retire(p,t);
 struct process_pipe_handoff *next=fresh();struct process *q=spawn(next);uint64_t handle=life(next)->handoff.pipe;
 assert(!process_pipe_handoff_finish(t) && process_pipe_handoff_publish(t,&byte,1)==SRV_ERR_BADF);
 assert(pipes[handle-1].write_refs==1);retire(q,next);
 /* Lifetime release itself drops an abandoned held writer after child cleanup. */
 next=fresh();q=spawn(next);handle=life(next)->handoff.pipe;
 assert(cleanup_process_files(q));cleanup_process_address_space(q);q->active=false;q->reapable=true;
 assert(process_lifetime_revoke(next->token) && process_lifetime_drain(next->token));
 assert(pipes[handle-1].used && pipes[handle-1].write_refs==1 && !pipes[handle-1].read_refs);
 assert(process_lifetime_release(next->token) && !pipes[handle-1].used);
 groups++;
}
static bool scheduler_retired=true;
bool scheduler_io_process_retired(void *p) {assert(p==handoff_child);return scheduler_retired;}
static void retired_observer(void) {
 struct process_pipe_handoff *t=fresh();struct process *p=spawn(t);p->startup_ready=true;
 uint64_t status=0xa5;assert(!process_pipe_handoff_retired(t,&status) && status==0xa5);
 assert(process_pipe_handoff_finish(t));
 assert(cleanup_process_files(p));cleanup_process_address_space(p);release_process_runtime(p);
 p->active=false;p->reapable=true;p->exit_status=23;
 assert(process_pipe_handoff_retired(t,&status) && status==23);
 for(unsigned mode=0;mode<7;mode++) {
  status=0xa5;
  if(mode==0)p->active=true;
  if(mode==1)p->reapable=false;
  if(mode==2)p->parent_pid=9;
  if(mode==3)p->image_generation=17;
  if(mode==4)p->files[3].used=true;
  if(mode==5)p->user_threads[0].used=true;
  if(mode==6)scheduler_retired=false;
  assert(!process_pipe_handoff_retired(t,&status) && status==0xa5);
  p->active=false;p->reapable=true;p->parent_pid=0;p->image_generation=0;
  p->files[3].used=false;p->user_threads[0].used=false;scheduler_retired=true;
 }
 assert(!process_pipe_handoff_retired(t,NULL));retire(p,t);groups++;
}
int main(void) {positive();startup_and_spawn_failure();argument_and_startup_abort();refusal();replay_and_release();retired_observer();
 printf("process-pipe-handoff: %u groups PASS real constructor/ELF/lifetime/pipe paths with HOST platform and injected startup/retirement only; no guest/SYS_PROC_READY/network acceptance\n",groups);return 0;}
