/* Exercise real worker foreground guards using the process service fixture. */
#define main process_service_unused_main
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#include "test_process_service.c"
#pragma clang diagnostic pop
#undef main

static struct process snapshot;
static unsigned char slot_snapshot[sizeof(process_slots_busy)];
static void unchanged(struct process *parent,uint64_t pid_before,unsigned allocations,unsigned spawns)
{
    assert(current==parent && current_user_thread==&parent->user_threads[0]);
    assert(!memcmp(parent,&snapshot,sizeof(snapshot)));
    assert(!memcmp(slot_snapshot,process_slots_busy,sizeof(slot_snapshot)));
    assert(next_pid==pid_before && alloc_calls==allocations && spawn_calls==spawns);
}
int main(void)
{
    reset();memset(processes,0,sizeof(processes));memset(process_slots_busy,0,sizeof(process_slots_busy));
    struct process *parent=&processes[0];
    parent->allocated=parent->active=true;parent->pid=9;parent->address_space=0x1000;
    parent->user_threads[0]=(struct process_user_thread){.used=true,.active=true,.tid=8,.owner=parent,
        .stack_top=0x500800,.tls_base=0x700000,.tls_thread_pointer=0x700900};
    memset(&parent->user_threads[0].fpu,0x5a,sizeof(parent->user_threads[0].fpu));
    current=observed=parent;current_user_thread=&parent->user_threads[0];next_pid=40;
    snapshot=*parent;memcpy(slot_snapshot,process_slots_busy,sizeof(slot_snapshot));
    uint64_t before=next_pid;unsigned allocations=alloc_calls,spawns=spawn_calls;
#define UNCHANGED() unchanged(parent,before,allocations,spawns)
    assert(process_spawn_elf("/valid")==SRV_ERR_NOTSUP);UNCHANGED();
    assert(process_spawn_elf_args("/valid","arg")==SRV_ERR_NOTSUP);UNCHANGED();
    assert(process_spawn_elf_args_redirect("/valid","arg","/output",true)==SRV_ERR_NOTSUP);UNCHANGED();
    struct process_spawn_file_action action={.type=1,.fd=1,.new_fd=2,.path="/output"};
    assert(process_spawn_exec("/valid",0,NULL,0,NULL,false,0,1,2,0,true,&action,1,SRV_EXEC_INHERIT_FILES,0,0)==SRV_ERR_NOTSUP);UNCHANGED();
    /* A detached worker must reach ordinary process-table admission, rather
     * than the foreground guard. Saturate the table without touching parent. */
    for(unsigned i=1;i<PROCESS_MAX_PROCESSES;i++)process_slots_busy[i]=true;
    assert(process_spawn_exec("/valid",0,NULL,0,NULL,true,0,1,2,0,false,NULL,0,0,0,0)==SRV_ERR_AGAIN);
    assert(current==parent && current_user_thread==&parent->user_threads[0] && !memcmp(parent,&snapshot,sizeof(snapshot)));
    puts("process-foreground-worker: PASS (refusal before effects; detached reaches admission)");
}
