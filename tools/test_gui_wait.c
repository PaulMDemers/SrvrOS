#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SYSCALL_POLL_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct srv_gui_wait user;
static uint64_t now,epoch,generation;
static uint32_t gui_state;
static uint16_t fd_state;
static unsigned waits,polls,mode;
static bool busy,signal_pending,exiting,copy_fault;
static struct scheduler_wait_queue queue;
uint64_t timer_ticks(void) { return now/10000; }
uint64_t timer_monotonic_us(void) { return now; }
bool process_should_exit_current(void) { return exiting; }
bool process_signal_pending_current(void) { return signal_pending; }
struct process *process_current(void) { return (struct process *)1; }
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    (void)writable;return !copy_fault && address==(uint64_t)&user && length==sizeof(user);
}
bool scheduler_io_busy(void) { return busy; }
uint32_t gui_client_wait_state(uint64_t server,uint64_t expected,uint64_t *observed) {
    assert(server==42);*observed=epoch;
    return gui_state | (expected!=epoch?SRV_GUI_CANCELED:0);
}
int process_file_watch_capture(struct process *p,int64_t fd,uint16_t events,uint64_t *out) {
    assert(p==(struct process *)1);
    if(fd!=7 || events!=SRV_POLLIN)return -1;
    *out=generation;return 0;
}
uint16_t process_file_watch_poll(struct process *p,int64_t fd,uint16_t events,uint64_t captured,bool deferred) {
    assert(p==(struct process *)1 && fd==7 && events==SRV_POLLIN && deferred==busy);polls++;
    return captured!=generation?SRV_POLLNVAL:deferred?0:fd_state;
}
void scheduler_yield(void) { assert(0); }
struct scheduler_wait_queue *process_file_poll_wait_queue(void) { return &queue; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(q==&queue && waits<20);waits++;
    /* A message arriving between the outer read and registration must prevent sleep. */
    if(mode==1) { gui_state=SRV_GUI_READY;assert(ready(arg));return true; }
    assert(!ready(arg));
    if(mode==2) { fd_state=SRV_POLLIN;assert(ready(arg));return true; }
    if(mode==3) { epoch++;assert(ready(arg));return true; }
    if(mode==4) { generation++;assert(ready(arg));return true; }
    if(mode==5) { gui_state=SRV_GUI_DISCONNECTED;assert(ready(arg));return true; }
    if(mode==6) { signal_pending=true;assert(ready(arg));return true; }
    if(mode==7) { exiting=true;assert(ready(arg));return true; }
    if(mode==8) { copy_fault=true;gui_state=SRV_GUI_READY;assert(ready(arg));return true; }
    if(mode==9 && waits<3) { now+=100;return true; } /* Spurious wake. */
    if(mode==10) { user.server=99;user.fds[0].fd=90;user.cancel_epoch=70;fd_state=SRV_POLLIN;assert(ready(arg));return true; }
    if(mode==11) { assert(deadline==timer_ticks()+1);busy=false;fd_state=SRV_POLLIN; }
    now=deadline*10000;return true;
}
static void reset(void) {
    now=epoch=generation=gui_state=fd_state=waits=polls=mode=0;
    busy=signal_pending=exiting=copy_fault=false;
    user=(struct srv_gui_wait){.abi_version=SRV_GUI_WAIT_VERSION,.struct_size=sizeof(user),
        .server=42,.deadline_us=100000,.count=1,.fds={{.fd=7,.events=SRV_POLLIN}}};
}
int main(void) {
    reset();user.deadline_us=0;assert(!syscall_gui_wait_io(&user) && user.ready==SRV_GUI_DEADLINE && !waits);
    reset();now=9000;user.deadline_us=159000;
    assert(!syscall_gui_wait_io(&user) && now>=159000 && waits==2 && user.ready==SRV_GUI_DEADLINE);
    reset();gui_state=SRV_GUI_READY;fd_state=SRV_POLLIN;epoch=4;
    assert(!syscall_gui_wait_io(&user) && user.ready==(SRV_GUI_READY|SRV_GUI_FD_READY|SRV_GUI_CANCELED) && user.cancel_epoch==4 && !waits);
    reset();busy=true;fd_state=SRV_POLLIN;gui_state=SRV_GUI_READY;
    assert(!syscall_gui_wait_io(&user) && user.ready==SRV_GUI_READY && !user.fds[0].revents && !waits);
    for(unsigned m=1;m<=11;m++) {
        reset();mode=m;if(m==11)busy=true;
        int64_t result=syscall_gui_wait_io(&user);
        if(m==6)assert(result==SRV_ERR_INTR);
        else if(m==7 || m==8)assert(result==-1);
        else {
            assert(!result && waits);
            if(m==1)assert(user.ready==SRV_GUI_READY);
            if(m==2 || m==10 || m==11)assert(user.ready==SRV_GUI_FD_READY && user.fds[0].revents==SRV_POLLIN);
            if(m==3)assert(user.ready==SRV_GUI_CANCELED && user.cancel_epoch==1);
            if(m==4)assert(user.ready==SRV_GUI_FD_READY && user.fds[0].revents==SRV_POLLNVAL);
            if(m==5)assert(user.ready==SRV_GUI_DISCONNECTED);
            if(m==9)assert(user.ready==SRV_GUI_DEADLINE && waits==3);
            if(m==10)assert(user.server==42 && user.fds[0].fd==7 && !user.cancel_epoch);
        }
    }
    reset();busy=true;fd_state=SRV_POLLIN;user.deadline_us=30000;
    assert(!syscall_gui_wait_io(&user) && user.ready==SRV_GUI_DEADLINE && waits==3);
    reset();user.count=0;assert(!syscall_gui_wait_io(&user) && !polls);
    reset();user.deadline_us=SRV_GUI_WAIT_MAX_US+1;assert(syscall_gui_wait_io(&user)<0);
    reset();now=UINT64_MAX-100;user.deadline_us=UINT64_MAX;gui_state=SRV_GUI_READY;assert(!syscall_gui_wait_io(&user));
    for(unsigned bad=0;bad<7;bad++) {
        reset();if(!bad)user.abi_version++;
        if(bad==1)user.struct_size--;
        if(bad==2)user.count=9;
        if(bad==3)user.server=0;
        if(bad==4)user.fds[0].reserved=1;
        if(bad==5)user.fds[0].events=SRV_POLLOUT;
        if(bad==6)user.fds[0].fd=-1;
        assert(syscall_gui_wait_io(&user)<0 && !waits);
    }
    reset();assert(syscall_gui_wait_io(NULL)<0);copy_fault=true;assert(syscall_gui_wait_io(&user)<0);
    puts("gui-wait: PASS immutable request, simultaneous readiness, registration/spurious races, cancellation, reuse, disconnect, signal, deadlines and owned-I/O deferral");
}
