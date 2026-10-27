/* Non-reaping supervisor wait uses the actual process ownership predicates. */
#include <assert.h>
#include <stdio.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
#include <srvros/battery_supervisor.h>
static struct process caller;
static bool external,inject,refuse,blocked,signaled;
static unsigned waits,wakes;
static uint64_t ticks=50;
void *scheduler_current_user_context(void){return &caller;}
void *scheduler_current_user_thread_context(void){return NULL;}
uint64_t timer_ticks(void){return ticks;}
void scheduler_wake_all(struct scheduler_wait_queue *q){
    assert(q==&process_wait_queue);wakes++;if(blocked)signaled=true;
}
static bool pending(void *arg){assert(arg==&external);return external;}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn condition,void *arg,uint64_t deadline){
    assert(q==&process_wait_queue);waits++;
    if(refuse)return false;
    /* Producer runs after caller's scan, before atomic compare/enqueue. */
    if(inject){inject=false;external=true;process_wait_notify();}
    if(condition(arg))return true;
    blocked=true;
    if(deadline){assert(deadline>ticks);ticks=deadline;assert(condition(arg));}
    else {external=true;process_wait_notify();assert(signaled&&condition(arg));}
    blocked=false;return true;
}
int main(void){
    caller.pid=7;
    processes[0]=(struct process){.allocated=true,.detached=true,.pid=8,.parent_pid=7,.active=true};
    unsigned before=waits;assert(!process_wait_event(0,pending,&external,0));
    assert(!process_wait_event(8,NULL,&external,0)&&waits==before);
    assert(process_wait_event(8,pending,&external,0)&&signaled);
    assert(processes[0].allocated&&!processes[0].reapable); /* no reap */
    external=false;signaled=false;inject=true;
    assert(process_wait_event(8,pending,&external,0)&&external&&!signaled);
    external=false;processes[0].reapable=true;
    assert(process_wait_event(8,pending,&external,0)&&processes[0].reapable&&!external);
    processes[0].reapable=false;processes[0].parent_pid=99;
    assert(process_wait_event(8,pending,&external,0)&&!external); /* foreign target absent */
    processes[0].parent_pid=7;processes[0].allocated=false;
    assert(process_wait_event(8,pending,&external,0)&&!external);
    processes[0].allocated=true;
    assert(process_wait_event(8,pending,&external,55)&&ticks==55&&!external);
    assert(process_wait_event(8,pending,&external,54)&&ticks==55);
    refuse=true;assert(!process_wait_event(8,pending,&external,0));refuse=false;
    assert(battery_supervisor_limit(false,false,0,false)==0);
    assert(battery_supervisor_limit(false,true,0,false)==300000000);
    assert(battery_supervisor_limit(true,true,1,true)==20000000);
    assert(battery_supervisor_limit(false,false,1,false)==1950000000ull);
    assert(battery_supervisor_limit(false,false,2,false)==210000000);
    assert(battery_supervisor_limit(false,false,3,false)==0);
    assert(battery_supervisor_limit(false,false,4,false)==0);
    assert(battery_supervisor_limit(false,false,0,true)==240000000);
    assert(battery_supervisor_tick_deadline(50000,5,0)==0);
    assert(battery_supervisor_tick_deadline(50000,5,60001)==6);
    assert(battery_supervisor_tick_deadline(50000,5,59999)==6);
    assert(battery_supervisor_tick_deadline(50000,5,50000)==6);
    assert(battery_supervisor_tick_deadline(50000,5,300050000)==30005);
    assert(battery_supervisor_tick_deadline(50000,30004,300050000)>30004);
    assert(battery_supervisor_tick_deadline(0,UINT64_MAX-1,100000)==UINT64_MAX);
    printf("process-event: PASS ownership, non-reaping, producer races, deadline and fallback; wakes=%u\n",wakes);
}
