#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SYSCALL_POLL_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct syscall_pollfd user[2];
static uint64_t now,buffered,phase_us;
static unsigned gates,polls,waits,pipe_polls,wait_calls;
static bool safe=true,io_busy=true,serial_ready,arrive,mutate,signal_pending,exiting;
static bool redirected,redirect_on_wait,arrive_before_enrollment;
static struct process_file files[PROCESS_MAX_OPEN_FILES];
static uint64_t generations[PROCESS_MAX_OPEN_FILES];
static bool simple[PROCESS_MAX_OPEN_FILES];
static uint16_t pipe_events[PROCESS_MAX_OPEN_FILES];
static enum { WAKE_NONE,WAKE_PIPE,WAKE_REPLACE,WAKE_CLOSE,WAKE_HANDLE,WAKE_ABA,WAKE_RIGHTS,
    WAKE_NO_READERS,WAKE_BLOCKING,WAKE_EXHAUST,WAKE_SIGNAL,WAKE_EXIT } wake;
static struct scheduler_wait_queue queue;
uint64_t timer_ticks(void) { return now; }
uint64_t timer_monotonic_us(void) { return now*10000+phase_us; }
bool process_should_exit_current(void) { return exiting; }
bool process_signal_pending_current(void) { return signal_pending; }
uint64_t keyboard_buffered_count(void) { return buffered; }
bool serial_has_char(void) { return serial_ready; }
struct process *process_current(void) { return (struct process *)1; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) { assert(p==(struct process *)1 && input);return fd==0 && !redirected; }
struct process_file *process_file_at(struct process *p,uint64_t fd) {
    assert(p==(struct process *)1);return fd<PROCESS_MAX_OPEN_FILES && files[fd].used?&files[fd]:NULL;
}
uint64_t process_fd_generation(struct process *p,uint64_t fd) {
    return process_file_at(p,fd)?generations[fd]:UINT64_MAX;
}
bool process_file_pipe_io_safe(struct process *p,uint64_t fd,uint64_t length,bool writing) {
    struct process_file *file=process_file_at(p,fd);assert(length==0);
    return file && file->type==(writing?PROCESS_FILE_PIPE_WRITE:PROCESS_FILE_PIPE_READ) &&
        (file->flags&SRV_FD_NONBLOCK) && simple[fd];
}
uint16_t process_file_poll(struct process *p,int64_t fd,uint16_t events) {
    assert(p==(struct process *)1 && (fd!=0 || redirected));polls++;
    struct process_file *file=fd>=0?process_file_at(p,(uint64_t)fd):NULL;
    if(file && (file->type==PROCESS_FILE_PIPE_READ || file->type==PROCESS_FILE_PIPE_WRITE)) {
        if(safe) {
            assert(process_file_pipe_io_safe(p,(uint64_t)fd,0,file->type==PROCESS_FILE_PIPE_WRITE));
            assert(events==(file->type==PROCESS_FILE_PIPE_WRITE?SRV_POLLOUT:SRV_POLLIN));
            pipe_polls++;
        } else assert(gates);
        return pipe_events[fd];
    }
    assert(!safe && gates);
    return fd==3 && !file?SRV_POLLNVAL:events;
}
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    (void)writable;return address==(uint64_t)user && length<=sizeof(user);
}
void scheduler_io_set_user_safe(bool value) { assert(!value);safe=value; }
bool scheduler_io_busy(void) { return io_busy; }
void scheduler_io_wait(void) { assert(!safe);gates++;io_busy=false; }
void scheduler_yield(void) { assert(0); }
struct scheduler_wait_queue *process_file_poll_wait_queue(void) { return &queue; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(q==&queue);wait_calls++;
    /* Arrival between the outer observation and atomic check/enrollment must
     * complete without enrollment and without an intervening tick. */
    if(arrive_before_enrollment) {pipe_events[3]=SRV_POLLIN;assert(ready(arg));return true;}
    assert(!ready(arg));waits++;
    if(redirect_on_wait) { redirected=true;assert(ready(arg));return true; }
    if(mutate) user[0]=(struct syscall_pollfd){.fd=9,.events=SRV_POLLOUT};
    if(wake) {
        unsigned before=polls;
        switch(wake) {
        case WAKE_PIPE:pipe_events[3]=SRV_POLLIN;break;
        case WAKE_REPLACE:generations[3]++;files[3].type=PROCESS_FILE_VFS;break;
        case WAKE_CLOSE:generations[3]++;files[3].used=false;break;
        case WAKE_HANDLE:files[3].handle++;pipe_events[3]=SRV_POLLIN;break;
        case WAKE_ABA:generations[3]++;pipe_events[3]=SRV_POLLIN;break;
        case WAKE_RIGHTS:simple[3]=false;pipe_events[3]=SRV_POLLIN;break;
        case WAKE_NO_READERS:simple[3]=false;pipe_events[3]=SRV_POLLERR;break;
        case WAKE_BLOCKING:files[3].flags=0;pipe_events[3]=SRV_POLLIN;break;
        case WAKE_EXHAUST:generations[3]=UINT64_MAX;pipe_events[3]=SRV_POLLIN;break;
        case WAKE_SIGNAL:signal_pending=true;break;
        case WAKE_EXIT:exiting=true;break;
        default:assert(0);
        }
        assert(ready(arg));
        if(wake!=WAKE_PIPE)assert(polls==before); /* No revoked/replacement poll. */
        return true;
    }
    if(arrive) { buffered=1;now++;assert(ready(arg)); }
    else { now=deadline;phase_us=0; }
    return true;
}
static void reset_pipe(bool writing) {
    memset(files,0,sizeof(files));memset(generations,0,sizeof(generations));
    memset(simple,0,sizeof(simple));memset(pipe_events,0,sizeof(pipe_events));
    user[0]=(struct syscall_pollfd){.fd=3,.events=writing?SRV_POLLOUT:SRV_POLLIN};
    files[3]=(struct process_file){.used=true,.type=writing?PROCESS_FILE_PIPE_WRITE:PROCESS_FILE_PIPE_READ,
        .handle=71,.flags=SRV_FD_NONBLOCK};generations[3]=7;simple[3]=true;
    now=buffered=phase_us=0;gates=polls=waits=pipe_polls=wait_calls=0;
    safe=io_busy=true;serial_ready=arrive=mutate=signal_pending=exiting=false;
    redirected=redirect_on_wait=arrive_before_enrollment=false;wake=WAKE_NONE;
}
static void pipe_regressions(void) {
    reset_pipe(false);pipe_events[3]=SRV_POLLIN;
    assert(syscall_poll(user,1,0)==1 && user[0].revents==SRV_POLLIN && !gates && pipe_polls && safe && io_busy);
    reset_pipe(true);pipe_events[3]=SRV_POLLOUT;
    assert(syscall_poll(user,1,0)==1 && user[0].revents==SRV_POLLOUT && !gates && safe);
    reset_pipe(false);pipe_events[3]=SRV_POLLHUP;
    assert(syscall_poll(user,1,0)==1 && user[0].revents==SRV_POLLHUP && !gates);
    reset_pipe(false);pipe_events[3]=SRV_POLLIN|SRV_POLLHUP;
    assert(syscall_poll(user,1,0)==1 && user[0].revents==(SRV_POLLIN|SRV_POLLHUP) && !gates);
    reset_pipe(false);arrive_before_enrollment=true;
    assert(syscall_poll(user,1,120)==1 && wait_calls==1 && !waits && !now && !gates);
    reset_pipe(false);wake=WAKE_PIPE;mutate=true;
    assert(syscall_poll(user,1,120)==1 && user[0].fd==3 && user[0].events==SRV_POLLIN &&
        user[0].revents==SRV_POLLIN && waits==1 && !gates && safe);
    reset_pipe(true);
    assert(syscall_poll(user,1,20)==0 && now==2 && waits==1 && !gates && safe); /* Full pipe. */
    for(unsigned action=WAKE_REPLACE;action<=WAKE_EXHAUST;action++) {
        reset_pipe(action==WAKE_NO_READERS);wake=action;
        assert(syscall_poll(user,1,120)==1 && waits==1 && gates==1 && !safe);
        assert(user[0].revents==(action==WAKE_CLOSE?SRV_POLLNVAL:action==WAKE_NO_READERS?SRV_POLLERR:SRV_POLLIN));
    }
    reset_pipe(false);wake=WAKE_SIGNAL;
    assert(syscall_poll(user,1,120)==0 && waits==1 && !gates && signal_pending);
    reset_pipe(false);wake=WAKE_EXIT;
    assert(syscall_poll(user,1,120)==-1 && waits==1 && !gates && exiting);
    reset_pipe(false);simple[3]=false;pipe_events[3]=SRV_POLLIN;
    assert(syscall_poll(user,1,0)==1 && gates==1 && !safe); /* Rights already queued. */
    reset_pipe(true);simple[3]=false;pipe_events[3]=SRV_POLLERR;
    assert(syscall_poll(user,1,0)==1 && gates==1 && user[0].revents==SRV_POLLERR); /* No readers. */
    reset_pipe(false);files[3].flags=0;pipe_events[3]=SRV_POLLIN;
    assert(syscall_poll(user,1,0)==1 && gates==1); /* Blocking pipe. */
    reset_pipe(false);generations[3]=UINT64_MAX;pipe_events[3]=SRV_POLLIN;
    assert(syscall_poll(user,1,0)==1 && gates==1); /* Generation exhaustion. */
    reset_pipe(false);user[0].events|=SRV_POLLOUT;pipe_events[3]=SRV_POLLIN;
    assert(syscall_poll(user,1,0)==1 && gates==1); /* Unsupported direction/mask. */
    reset_pipe(false);files[3].type=PROCESS_FILE_PIPE_DUPLEX;
    assert(syscall_poll(user,1,0)==1 && gates==1); /* Duplex retains old path. */
    reset_pipe(false);files[3].type=PROCESS_FILE_PTY_MASTER;
    assert(syscall_poll(user,1,0)==1 && gates==1);
    reset_pipe(false);files[3].type=PROCESS_FILE_NET_CONNECTION;
    assert(syscall_poll(user,1,0)==1 && gates==1);
    reset_pipe(false);pipe_events[3]=SRV_POLLIN;user[1]=(struct syscall_pollfd){.fd=0,.events=SRV_POLLIN};
    assert(syscall_poll(user,2,0)==1 && gates==1 && !safe); /* Mixed console/pipe remains gated. */
    reset_pipe(false);pipe_events[3]=SRV_POLLIN;user[1]=(struct syscall_pollfd){.fd=-1,.events=0};
    assert(syscall_poll(user,2,0)==1 && !gates && safe);
    reset_pipe(false);io_busy=false;pipe_events[3]=SRV_POLLIN;
    assert(syscall_poll(user,1,0)==1 && !gates && safe); /* Same API when storage idle. */
}
int main(void) {
    user[0]=(struct syscall_pollfd){.fd=0,.events=SRV_POLLIN};
    assert(syscall_poll(user,1,0)==0 && !user[0].revents && !gates && !polls);
    assert(syscall_poll(user,1,120)==0 && now==12 && waits==1 && !gates);
    arrive=true;mutate=true;
    assert(syscall_poll(user,1,120)==1 && user[0].fd==0 && user[0].revents==SRV_POLLIN && !gates && !polls);
    buffered=0;serial_ready=true;assert(syscall_poll(user,1,0)==1 && !gates);
    serial_ready=false;arrive=mutate=false;
    user[0].fd=9;assert(syscall_poll(user,1,0)==1 && gates==1 && polls==1 && !safe);
    safe=true;user[0].fd=0;user[1]=(struct syscall_pollfd){.fd=9,.events=SRV_POLLOUT};
    assert(syscall_poll(user,2,0)==1 && gates==2 && polls==2);
    safe=true;user[0]=(struct syscall_pollfd){.fd=-1,.events=SRV_POLLOUT};
    assert(syscall_poll(user,1,0)==0 && gates==2);
    assert(syscall_poll(NULL,0,20)==0 && now==15 && gates==2);
    assert(syscall_poll(NULL,1,0)==-1 && syscall_poll(user,MAX_POLL_FDS+1,0)==-1 && gates==2);
    unsigned before_waits=waits;phase_us=9000;uint64_t start_us=timer_monotonic_us();
    assert(syscall_poll(NULL,0,150)==0 && timer_monotonic_us()-start_us>=150000 && waits==before_waits+2);
    assert(poll_deadline_us(INT64_MAX,0)==UINT64_MAX && poll_deadline_us(1,UINT64_MAX-1)==UINT64_MAX);
    safe=io_busy=true;user[0]=(struct syscall_pollfd){.fd=0,.events=SRV_POLLIN};redirect_on_wait=true;
    assert(syscall_poll(user,1,120)==1 && !safe && gates==3 && polls==3);
    pipe_regressions();
    puts("syscall-poll: PASS console/default behavior, strict pipe metadata readiness, immutable requests, atomic arrival/enrollment, descriptor/rights/readers revocation, HUP/EOF/signal/deadline and gated mixed fallback; no eligibility grants");
    return 0;
}
