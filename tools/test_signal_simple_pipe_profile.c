/* Actual experimental simple-pipe consume/publication and wait predicates. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SIGNAL_PUBLIC_GUEST_TEST
#define SRVROS_SIGNAL_EXPERIMENTAL_INFO
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
static uint64_t irq_flags=512;
static uint64_t process_test_irq_save(void){uint64_t old=irq_flags;irq_flags=0;return old;}
static void process_test_irq_restore(uint64_t old){irq_flags=old;}
static uint64_t process_test_rflags(void){return irq_flags;}
#include "../kernel/src/process.c"
static struct process *caller;
static unsigned waits,copies,frees;
static int wake_mode;
void *scheduler_current_user_context(void){return caller;}
void *scheduler_current_user_thread_context(void){return NULL;}
void scheduler_wake_all(struct scheduler_wait_queue *q){(void)q;}
void scheduler_yield(void){assert(0);}
void kfree(void *p){(void)p;frees++;assert(0);}
void net_process_wake(struct process *p){(void)p;}
void keyboard_wake_waiters(void){}
void pty_wake(void){}
int64_t pty_get_flags(uint64_t h,bool master){(void)h;(void)master;assert(0);return 0;}
bool scheduler_wait(struct scheduler_wait_queue *q,bool(*condition)(void *),void *context){
    assert(q==&pipe_wait_queue && ++waits==1);
    uint64_t irq=process_test_irq_save();
    if(wake_mode==1){pipes[0].rights[0].used=true;pipes[0].size=1;}
    else if(wake_mode==2)caller->file_generations[3]++;
    else {pipes[0].size=1;pipes[0].buffer[0]='R';}
    assert(condition(context));process_test_irq_restore(irq);return true;
}
static bool copy_allowed=true;static char output;
static bool read_copy(void *context,const uint8_t *a,uint64_t an,const uint8_t *b,uint64_t bn){
    assert(context==&output && !(irq_flags&512) && an+bn==1);copies++;
    if(!copy_allowed)return false;output=an?(char)*a:(char)*b;return true;
}
static bool write_copy(void *context,uint64_t offset,uint8_t *a,uint64_t an,uint8_t *b,uint64_t bn){
    assert(context==&output && !offset && !(irq_flags&512) && an+bn==1);copies++;
    if(!copy_allowed)return false;if(an)*a=(uint8_t)output;else *b=(uint8_t)output;return true;
}
static void reset(bool writing){
    memset(processes,0,sizeof(processes));memset(pipes,0,sizeof(pipes));caller=&processes[0];caller->allocated=caller->active=true;caller->pid=1;
    caller->files[3]=(struct process_file){.used=true,.type=writing?PROCESS_FILE_PIPE_WRITE:PROCESS_FILE_PIPE_READ,.handle=1};
    caller->file_generations[3]=7;pipes[0].used=true;pipes[0].refs=2;pipes[0].read_refs=pipes[0].write_refs=1;
    waits=copies=frees=0;wake_mode=0;irq_flags=512;output='?';copy_allowed=true;
}
int main(void){
    reset(false);assert(process_signal_experimental_fd_allowed(3));pipes[0].size=1;pipes[0].buffer[0]='Q';
    assert(process_file_pipe_read_checked(caller,3,1,false,read_copy,&output)==1 && output=='Q' && !pipes[0].size && copies==1);
    reset(false);pipes[0].size=1;pipes[0].buffer[0]='Q';copy_allowed=false;
    assert(process_file_pipe_read_checked(caller,3,1,false,read_copy,&output)==SRV_ERR_FAULT && pipes[0].size==1 && output=='?');
    reset(false);pipes[0].rights[0].used=true;pipes[0].size=1;
    assert(!process_signal_experimental_fd_allowed(3) && process_file_poll(caller,3,SRV_POLLIN)==SRV_POLLNVAL);
    assert(process_file_pipe_read_checked(caller,3,1,false,read_copy,&output)==SRV_ERR_BADF && !copies && pipes[0].rights[0].used && pipes[0].size==1 && !frees);
    reset(false);wake_mode=1;
    assert(process_file_pipe_read_checked(caller,3,1,false,read_copy,&output)==SRV_ERR_BADF && waits==1 && !copies && pipes[0].rights[0].used && !frees);
    reset(false);wake_mode=2;
    assert(process_file_pipe_read_checked(caller,3,1,false,read_copy,&output)==SRV_ERR_BADF && waits==1 && !copies);
    reset(false);
    assert(process_file_pipe_read_checked(caller,3,1,false,read_copy,&output)==1 && waits==1 && output=='R');
    reset(true);output='W';assert(process_file_pipe_write_checked(caller,3,1,false,write_copy,&output)==1 && pipes[0].size==1 && pipes[0].buffer[0]=='W');
    reset(true);pipes[0].rights[0].used=true;
    assert(process_file_pipe_write_checked(caller,3,1,false,write_copy,&output)==SRV_ERR_NOTSUP && !copies && !pipes[0].size);
    reset(true);pipes[0].size=PROCESS_PIPE_CAPACITY;wake_mode=1;
    assert(process_file_pipe_write_checked(caller,3,1,false,write_copy,&output)==SRV_ERR_BADF && waits==1 && !copies && !frees);
    reset(false);caller->files[3].type=PROCESS_FILE_PIPE_DUPLEX;assert(!process_signal_experimental_fd_allowed(3));
    caller->files[3].type=PROCESS_FILE_VFS;assert(!process_signal_experimental_fd_allowed(3));
    caller->files[3].type=PROCESS_FILE_PTY_MASTER;assert(!process_signal_experimental_fd_allowed(3));
    assert(irq_flags==512);puts("signal-simple-pipe: rights-free enrollment/wake/consume/publication PASS");
}
