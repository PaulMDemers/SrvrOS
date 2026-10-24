#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
static struct process *caller;
static unsigned wakes;
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q==&process_wait_queue);wakes++; }
void *scheduler_current_user_context(void) { return caller; }
void *scheduler_current_user_thread_context(void) { return NULL; }
bool scheduler_wait(struct scheduler_wait_queue *q,bool (*ready)(void *),void *arg) { (void)q;(void)ready;(void)arg;assert(0);return false; }
void scheduler_yield(void) { assert(0); }
void pmm_free_frame_range(uint64_t physical,uint64_t frames) { (void)physical;(void)frames;assert(0); }
static void setup(void) {
    wakes=0;memset(processes,0,sizeof(processes));
    processes[0]=(struct process){.allocated=true,.active=true,.pid=1};
    processes[1]=(struct process){.allocated=true,.active=true,.pid=10,.parent_pid=1};
    processes[2]=(struct process){.allocated=true,.detached=true,.reapable=true,.pid=11,.parent_pid=10,.exit_status=37};
    processes[3]=(struct process){.allocated=true,.active=true,.pid=20,.parent_pid=1};
}
int main(void) {
    setup();caller=&processes[3];uint64_t status=0;
    assert(process_wait(11,&status,true)==-1); /* Another live parent's child. */
    assert(processes[2].allocated);
    caller=&processes[1];assert(process_wait(11,&status,true)==11 && status==37);
    assert(!processes[2].allocated);
    setup();assert(process_set_reaper(1));caller=&processes[0];
    assert(process_wait(11,&status,true)==-1 && processes[2].allocated); /* Init cannot steal it either. */
    processes[1].active=false;
    assert(process_wait(11,&status,true)==11 && status==37); /* Dead parent: init adopts the result. */
    setup();processes[1].allocated=false;caller=&processes[0];
    assert(process_wait(11,&status,true)==11); /* Parent has already been reaped. */
    setup();processes[2].parent_pid=0;caller=&processes[0];
    assert(process_wait(11,&status,true)==-1 && processes[2].allocated); /* Kernel autotest owns it. */
    caller=NULL;assert(process_wait(11,&status,true)==11 && status==37);
    setup();caller=&processes[1];processes[2].reapable=false;processes[2].active=true;
    assert(process_wait(11,&status,true)==0); /* Live owned child. */
    caller=&processes[3];assert(process_wait(0,&status,true)==-1);
    setup();caller=&processes[1];processes[2].reapable=false; /* Already claimed by a competing reaper. */
    assert(process_wait(11,&status,true)==0 && wakes==0);
    release_process(&processes[2]);
    assert(wakes==1 && process_wait(11,&status,true)==-1);
    setup();strcpy(processes[1].name,"sample");processes[1].mapping_count=17;
    uint64_t pid=0,parent=0,mapped=0;const char *name=NULL,*state=NULL;
    assert(process_list_details(1,&pid,&name,&state,&parent,&mapped)==2);
    assert(pid==10 && parent==1 && mapped==17*4096 && !strcmp(name,"sample") && !strcmp(state,"foreground"));
    assert(process_list(2,&pid,&name,&state)==3 && pid==11 && !strcmp(state,"exited"));
    assert(process_list_details(PROCESS_MAX_PROCESSES,&pid,&name,&state,&parent,&mapped)==0);
    puts("wait-ownership: PASS parent-only waits, init orphan adoption, live parent protection and kernel-owned task isolation");
}
