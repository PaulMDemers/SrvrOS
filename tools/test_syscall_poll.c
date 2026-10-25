#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SYSCALL_POLL_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct syscall_pollfd user[2];
static uint64_t now,buffered,phase_us;
static unsigned gates,polls,waits;
static bool safe=true,serial_ready,arrive,mutate,signal_pending;
static bool redirected,redirect_on_wait;
static struct scheduler_wait_queue queue;
uint64_t timer_ticks(void) { return now; }
uint64_t timer_monotonic_us(void) { return now*10000+phase_us; }
bool process_should_exit_current(void) { return false; }
bool process_signal_pending_current(void) { return signal_pending; }
uint64_t keyboard_buffered_count(void) { return buffered; }
bool serial_has_char(void) { return serial_ready; }
struct process *process_current(void) { return (struct process *)1; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) { assert(p==(struct process *)1 && input);return fd==0 && !redirected; }
uint16_t process_file_poll(struct process *p,int64_t fd,uint16_t events) {
    assert(p==(struct process *)1 && (fd!=0 || redirected) && !safe && gates);polls++;return events;
}
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    (void)writable;return address==(uint64_t)user && length<=sizeof(user);
}
void scheduler_io_set_user_safe(bool value) { safe=value; }
bool scheduler_io_busy(void) { return true; }
void scheduler_io_wait(void) { assert(!safe);gates++; }
void scheduler_yield(void) { assert(0); }
struct scheduler_wait_queue *process_file_poll_wait_queue(void) { return &queue; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(q==&queue && !ready(arg));waits++;
    if(redirect_on_wait) { redirected=true;assert(ready(arg));return true; }
    if(mutate) user[0]=(struct syscall_pollfd){.fd=9,.events=SRV_POLLOUT};
    if(arrive) { buffered=1;now++;assert(ready(arg)); }
    else { now=deadline;phase_us=0; }
    return true;
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
    safe=true;user[0]=(struct syscall_pollfd){.fd=0,.events=SRV_POLLIN};redirect_on_wait=true;
    assert(syscall_poll(user,1,120)==1 && !safe && gates==3 && polls==3);
    puts("syscall-poll: PASS monotonic deadline across partial ticks, console readiness/deadline/wake, immutable request across yield, non-console/mixed gate and validation");
}
