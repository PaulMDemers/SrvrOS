#include <srvros/pty.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void (*pump)(void);
static bool interrupted,exiting;
static unsigned wakes,waits,hups;
static uint64_t active,signal_number;
static int64_t signal_group;
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q;wakes++; }
void process_file_poll_wake(void) { wakes++; }
bool process_should_exit_current(void) { return exiting; }
bool process_signal_pending_current(void) { return interrupted; }
void process_tty_hangup(uint64_t handle) { assert(handle);hups++; }
void process_tty_signal(uint64_t handle,uint64_t number) { signal_group=-(int64_t)pty_state(handle)->foreground_group;signal_number=number; }
bool scheduler_wait(struct scheduler_wait_queue *q,bool (*ready)(void *),void *argument) {
    (void)q;waits++;assert(pump);pump();assert(ready(argument));return true;
}
static void send_line(void) { assert(pty_write(active,true,(const uint8_t *)"wake\n",5,true)==5); }
static void interrupt(void) { interrupted=true; }
static void close_master(void) { pty_close(active,true); }
static void drain_output(void) { static uint8_t bytes[PTY_BYTES];assert(pty_read(active,true,bytes,sizeof(bytes),true)>0); }
int main(void) {
    uint64_t handles[16];
    for(unsigned i=0;i<16;i++) { handles[i]=pty_create();assert(handles[i]); }
    assert(!pty_create());
    for(unsigned i=0;i<16;i++) { pty_close(handles[i],true);pty_close(handles[i],false);assert(!pty_state(handles[i])); }
    active=pty_create();assert(active && !pty_state(handles[0]));
    uint8_t bytes[32];pump=send_line;
    assert(pty_read(active,false,bytes,sizeof(bytes),false)==5 && !memcmp(bytes,"wake\n",5) && waits==1);
    drain_output();pump=interrupt;
    assert(pty_read(active,false,bytes,sizeof(bytes),false)==SRV_ERR_INTR && waits==2);
    interrupted=false;struct pty_model *p=pty_state(active);p->foreground_group=73;
    assert(pty_write(active,true,(const uint8_t *)"\003",1,true)==1 && signal_group==-73 && signal_number==SRV_SIGNAL_INT && !p->signals);
    drain_output();
    static uint8_t full[PTY_BYTES];memset(full,'a',sizeof(full));
    assert(pty_write(active,false,full,sizeof(full),true)==sizeof(full));
    assert(pty_write(active,false,(const uint8_t *)"x",1,true)==SRV_ERR_AGAIN);
    pump=drain_output;assert(pty_write(active,false,(const uint8_t *)"\n",1,false)==1 && waits==3);
    assert(pty_read(active,true,bytes,sizeof(bytes),true)==2 && !memcmp(bytes,"\r\n",2));
    assert(pty_write(active,false,(const uint8_t *)"pending",7,true)==7);
    struct srv_termios raw=p->attributes;raw.lflag=0;
    assert(!pty_setattr(active,&raw,SRV_TCSADRAIN) && waits==4 && !p->output.size && !p->attributes.lflag);
    /* Last-master close wakes an already blocked reader and produces EOF. */
    pump=close_master;unsigned before=hups;
    assert(pty_read(active,false,bytes,sizeof(bytes),false)==0 && hups==before+1);
    assert(pty_poll(active,false,SRV_POLLIN)&SRV_POLLHUP);
    assert(!pty_ref(active,true));pty_close(active,false);assert(!pty_state(active));
    assert(wakes>0);
    puts("pty-manager: PASS capacity/generations, wait/wake, interruption, output backpressure/drain, foreground signals and hangup");
}
