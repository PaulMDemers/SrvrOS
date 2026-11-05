/* Actual process wait/lock/listener transactions with controlled enrollment races. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
static uint64_t irq_flags=512;
static uint64_t process_test_irq_save(void){uint64_t old=irq_flags;irq_flags=0;return old;}
static void process_test_irq_restore(uint64_t old){irq_flags=old;}
static uint64_t process_test_rflags(void){return irq_flags;}
#include "../kernel/src/process.c"
static struct process *caller;
static struct process saved_parent,saved_child;
static struct process_exit_record saved_history[PROCESS_EXIT_STATUS_CACHE];
static unsigned waits,unix_wakes,lock_wakes;
static int mode,operation;
void *scheduler_current_user_context(void){return caller;}
void *scheduler_current_user_thread_context(void){return NULL;}
void scheduler_wake_all(struct scheduler_wait_queue *q){if(q==&unix_wait_queue)unix_wakes++;if(q==&file_lock_wait_queue)lock_wakes++;}
void scheduler_yield(void){assert(0);}
void pmm_free_frame_range(uint64_t address,uint64_t frames){(void)address;(void)frames;assert(0);}
bool scheduler_io_process_retired(void *process){return process==&processes[1];}
int64_t pty_get_flags(uint64_t handle,bool master){(void)handle;(void)master;assert(0);return -1;}
bool scheduler_wait(struct scheduler_wait_queue *q,bool(*ready)(void *),void *context){
    assert(++waits==1);
    if(operation==1){
        assert(q==&file_lock_wait_queue);
        if(mode==0)file_locks[0].used=false;
        else if(mode==1){caller->files[3].used=false;file_generation_invalidate(caller,3);}
        else if(mode==2)caller->killed=true;
        else caller->pending_signals=UINT64_C(1)<<SRV_SIGNAL_USR1;
    } else {
        assert(operation==2 && q==&unix_wait_queue);
        if(mode==0){caller->files[3].used=false;file_generation_invalidate(caller,3);assert(unix_wakes);}
        else if(mode==1)caller->killed=true;
        else {unix_listeners[0].queue[0].used=true;unix_listeners[0].queue[0].file=(struct process_file){.used=true,.type=PROCESS_FILE_PIPE_DUPLEX};}
    }
    uint64_t flags=process_test_irq_save();assert(ready(context));process_test_irq_restore(flags);return true;
}
static void reset(void){
    memset(processes,0,sizeof(processes));memset(file_locks,0,sizeof(file_locks));memset(unix_listeners,0,sizeof(unix_listeners));
    caller=&processes[0];caller->allocated=caller->active=true;caller->pid=7;waits=unix_wakes=lock_wakes=0;irq_flags=512;
}
static bool copy_ok;static uint64_t copied_status;static unsigned copy_calls;
static bool status_copy(void *context,uint64_t status){assert(context==&copied_status && !(irq_flags&512));copy_calls++;if(!copy_ok)return false;copied_status=status;return true;}
static void wait_cases(bool retired){
    reset();struct process *child=&processes[1];*child=(struct process){.allocated=true,.detached=true,.reapable=true,.pid=11,.parent_pid=7,.exit_status=37};
    caller->main_pending_signals=UINT64_C(1)<<SRV_SIGNAL_CHLD;
    saved_parent=*caller;saved_child=*child;memcpy(saved_history,exit_records,sizeof saved_history);uint64_t sequence=next_exit_record_sequence;
    copy_ok=false;copy_calls=0;copied_status=77;int64_t result=0;
    if(retired){assert(process_try_wait_retired_checked(11,status_copy,&copied_status,&result));assert(result==SRV_ERR_FAULT);}
    else assert(process_wait_checked(11,SRV_WAIT_POSIX,status_copy,&copied_status)==SRV_ERR_FAULT);
    assert(copy_calls==1 && copied_status==77 && !memcmp(caller,&saved_parent,sizeof saved_parent) && !memcmp(child,&saved_child,sizeof saved_child));
    assert(next_exit_record_sequence==sequence && !memcmp(saved_history,exit_records,sizeof saved_history));
    copy_ok=true;
    if(retired){assert(process_try_wait_retired_checked(11,status_copy,&copied_status,&result) && result==11 && copied_status==37);}
    else assert(process_wait_checked(11,SRV_WAIT_POSIX,status_copy,&copied_status)==11 && copied_status==(37u<<8));
    assert(!child->allocated);
    reset();child=&processes[1];*child=(struct process){.allocated=true,.active=true,.detached=true,.pid=11,.parent_pid=7};copy_calls=0;copied_status=77;
    assert(process_wait_checked(11,SRV_WAIT_POSIX|SRV_WAIT_NOHANG,status_copy,&copied_status)==0 && !copy_calls && copied_status==77);
}
int main(void){
    for(mode=0;mode<4;mode++){
        reset();operation=1;caller->files[3]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS,.handle=1};
        file_contents[0].refs=1;read_files[0]=(struct process_read_file){.used=true,.refs=1,.content=&file_contents[0]};
        file_locks[0]=(struct process_file_lock){.used=true,.pid=9,.content=&file_contents[0],.start=0,.end=100,.type=SRV_F_WRLCK};
        struct srv_flock lock={.type=SRV_F_RDLCK,.len=1};
        int64_t result=process_file_set_lock(caller,3,&lock,true);
        assert(result==(mode==0?0:mode==1?SRV_ERR_BADF:SRV_ERR_INTR) && waits==1);
    }
    for(mode=0;mode<3;mode++){
        reset();operation=2;caller->files[3]=(struct process_file){.used=true,.type=PROCESS_FILE_UNIX_LISTENER,.handle=1};caller->files[4]=caller->files[3];
        unix_listeners[0].used=unix_listeners[0].listening=true;unix_listeners[0].refs=2;
        int64_t result=process_unix_accept(caller,3);
        assert(result==(mode==0?SRV_ERR_BADF:mode==1?SRV_ERR_INTR:0) && waits==1);
        if(mode==0)assert(unix_listeners[0].used && caller->files[4].used);
        if(mode==2)assert(caller->files[0].used && !unix_listeners[0].queue[0].used);
    }
    wait_cases(false);wait_cases(true);puts("signal-process-waits: lock/enrollment, listener lifetime, status-before-claim PASS");
}
