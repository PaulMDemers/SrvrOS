#include <assert.h>
#include <stdio.h>
#define SRVROS_SYSCALL_SLEEP_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static uint64_t now;
static unsigned waits,yields,spurious;
static bool killed,signaled,no_scheduler,pointer;
static int64_t gui_ready;
int64_t gui_server_ready(void) { return gui_ready; }
int64_t gui_client_ready(uint64_t server) { assert(server==42);return gui_ready; }
bool mouse_pending(void) { return pointer; }
static int interrupt;
static bool arrive_at_check;
static struct scheduler_wait_queue queue;
struct scheduler_wait_queue *mouse_wait_queue(void) { return &queue; }
uint64_t timer_ticks(void) { return now; }
bool process_should_exit_current(void) { return killed; }
bool process_signal_pending_current(void) { return signaled; }
struct scheduler_wait_queue *process_file_poll_wait_queue(void) { return &queue; }
void scheduler_yield(void) { yields++;now++; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    if(arrive_at_check) { arrive_at_check=false;pointer=true;gui_ready=1;assert(ready(arg));return true; }
    assert(q==&queue && !ready(arg) && deadline>now);waits++;
    if(no_scheduler) return false;
    if(spurious) { spurious--;now++;assert(!ready(arg));return true; }
    if(interrupt==1) killed=true;
    else if(interrupt==2) signaled=true;
    else if(interrupt==3) pointer=true;
    else if(interrupt==4) gui_ready=1;
    else if(interrupt==5) gui_ready=-1;
    else now=deadline;
    assert(ready(arg));return true;
}
int main(void) {
    now=40;assert(syscall_sleep_ticks(0)==0 && !waits && !yields);
    assert(syscall_sleep_ticks(7)==0 && now==47 && waits==1 && !yields);
    spurious=3;assert(syscall_sleep_ticks(20)==0 && now==67 && waits==5 && !yields);
    interrupt=2;assert(syscall_sleep_ticks(500)==SRV_ERR_INTR && now==67 && waits==6);
    signaled=false;interrupt=1;assert(syscall_sleep_ticks(500)==-1 && waits==7);
    killed=false;interrupt=0;no_scheduler=true;assert(syscall_sleep_ticks(3)==0 && now==70 && yields==3);
    now=UINT64_MAX-10;assert(syscall_sleep_ticks(11)==-1 && waits==10);
    no_scheduler=false;now=100;unsigned before=waits;
    assert(syscall_pointer_wait()==0 && now==101 && waits==before+1);
    pointer=true;before=waits;assert(syscall_pointer_wait()==1 && waits==before);
    pointer=false;interrupt=3;assert(syscall_pointer_wait()==1 && now==101);
    pointer=false;arrive_at_check=true;assert(syscall_pointer_wait()==1);
    pointer=false;interrupt=2;assert(syscall_pointer_wait()==SRV_ERR_INTR);
    signaled=false;interrupt=1;assert(syscall_pointer_wait()==-1);
    killed=false;interrupt=0;no_scheduler=true;assert(syscall_pointer_wait()==0 && now==102);
    now=UINT64_MAX;assert(syscall_pointer_wait()==0);
    now=500;no_scheduler=false;pointer=false;gui_ready=0;before=waits;
    assert(syscall_gui_wait(0,10)==-1 && syscall_gui_wait(42,101)==-1 && waits==before);
    assert(syscall_gui_wait(42,0)==0 && waits==before);
    assert(syscall_gui_wait(42,10)==0 && now==510 && waits==before+1);
    spurious=3;assert(syscall_gui_wait(42,10)==0 && now==520);
    gui_ready=1;before=waits;assert(syscall_gui_wait(42,10)==1 && waits==before);
    gui_ready=0;interrupt=4;assert(syscall_gui_wait(42,10)==1 && now==520);
    gui_ready=0;arrive_at_check=true;assert(syscall_gui_wait(42,10)==1 && now==520);
    gui_ready=0;interrupt=5;assert(syscall_gui_wait(42,10)==-1);
    gui_ready=0;interrupt=2;assert(syscall_gui_wait(42,10)==SRV_ERR_INTR);
    signaled=false;interrupt=1;assert(syscall_gui_wait(42,10)==-1);
    killed=false;interrupt=0;no_scheduler=true;assert(syscall_gui_wait(42,3)==0 && now==523);
    now=UINT64_MAX-5;before=waits;assert(syscall_gui_wait(42,6)==-1 && waits==before);
    now=700;no_scheduler=false;gui_ready=0;before=waits;
    assert(syscall_gui_server_wait(11)==-1 && syscall_gui_server_wait(0)==0 && waits==before);
    assert(syscall_gui_server_wait(10)==0 && now==710);
    spurious=3;assert(syscall_gui_server_wait(10)==0 && now==720);
    gui_ready=-1;before=waits;assert(syscall_gui_server_wait(10)==-1 && waits==before);
    gui_ready=1;assert(syscall_gui_server_wait(10)==1 && waits==before);
    gui_ready=0;arrive_at_check=true;assert(syscall_gui_server_wait(10)==1 && now==720);
    gui_ready=0;interrupt=4;assert(syscall_gui_server_wait(10)==1);
    gui_ready=0;interrupt=5;assert(syscall_gui_server_wait(10)==-1);
    gui_ready=0;interrupt=2;assert(syscall_gui_server_wait(10)==SRV_ERR_INTR);
    signaled=false;interrupt=1;assert(syscall_gui_server_wait(10)==-1);
    killed=false;interrupt=0;no_scheduler=true;assert(syscall_gui_server_wait(3)==0 && now==723);
    now=UINT64_MAX-5;assert(syscall_gui_server_wait(6)==-1);
    puts("gui-server-wait: PASS readiness/race, deadline, refusal, signal/exit and fallback");
    puts("gui-wait: PASS bounded deadline, ready/arrival race, spurious wake, peer loss, signal/cancel and fallback");
    puts("pointer-wait: PASS ready input, arrival during wait, deadline, signals/exit and fallback");
    puts("syscall-sleep: PASS deadline blocking, spurious wakes, signals/cancel, zero/overflow and early-boot fallback");
}
