#include <srvros/pty.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct process { int unused; };
static struct process test_process;
static struct process_file test_file;
static uint64_t test_generation=1;
static bool mapped=true;
static uint64_t valid_prefix=UINT64_MAX;
static unsigned copies;
struct process_file *process_file_at(struct process *p,uint64_t fd) { return p==&test_process && fd==0 && test_file.used?&test_file:NULL; }
uint64_t process_fd_generation(struct process *p,uint64_t fd) { return process_file_at(p,fd)?test_generation:UINT64_MAX; }
bool process_file_nonblocking(struct process *p,uint64_t fd) { (void)p;(void)fd;return false; }
static bool checked_out(void *ctx,const uint8_t *a,uint64_t an,const uint8_t *b,uint64_t bn) {
    if(!mapped) return false;
    memcpy(ctx,a,an);if(bn) memcpy((uint8_t *)ctx+an,b,bn);return true;
}
static bool checked_in(void *ctx,uint64_t offset,uint8_t *a,uint64_t an,uint8_t *b,uint64_t bn) {
    copies++;
    if(!mapped || offset+an+bn>valid_prefix) return false;
    memcpy(a,(uint8_t *)ctx+offset,an);if(bn) memcpy(b,(uint8_t *)ctx+offset+an,bn);return true;
}
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
static void drain_output(void);
static void revoke_and_send(void) { mapped=false;send_line(); }
static void replace_fd(void) { test_generation++; }
static void revoke_and_drain(void) { mapped=false;drain_output(); }
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
    active=pty_create();test_file=(struct process_file){.used=true,.type=PROCESS_FILE_PTY_SLAVE,.handle=active};
    pump=revoke_and_send;mapped=true;
    assert(process_file_pty_read_checked(&test_process,0,sizeof(bytes),checked_out,bytes)==SRV_ERR_FAULT);
    mapped=true;assert(process_file_pty_read_checked(&test_process,0,2,checked_out,bytes)==2 && !memcmp(bytes,"wa",2));
    assert(process_file_pty_read_checked(&test_process,0,sizeof(bytes),checked_out,bytes)==3 && !memcmp(bytes,"ke\n",3));
    drain_output();pump=replace_fd;
    assert(process_file_pty_read_checked(&test_process,0,sizeof(bytes),checked_out,bytes)==SRV_ERR_BADF);
    assert(pty_write(active,false,full,sizeof(full),true)==sizeof(full));
    pump=revoke_and_drain;mapped=true;
    assert(process_file_pty_write_checked(&test_process,0,1,checked_in,"x")==SRV_ERR_FAULT);
    mapped=true;assert(process_file_pty_write_checked(&test_process,0,1,checked_in,"x")==1);
    drain_output();
    unsigned wakes_before=wakes;copies=0;
    assert(process_file_pty_write_checked(&test_process,0,4096,checked_in,full)==4096);
    assert(wakes-wakes_before<=32 && copies<=16);
    drain_output();valid_prefix=267;copies=0;
    assert(process_file_pty_write_checked(&test_process,0,512,checked_in,full)==267);
    assert(copies<32);drain_output();
    valid_prefix=1;
    assert(process_file_pty_write_checked(&test_process,0,3,checked_in,"xyz")==1);
    assert(pty_read(active,true,bytes,sizeof(bytes),true)==1 && bytes[0]=='x');valid_prefix=UINT64_MAX;
    assert(pty_write(active,false,full,PTY_BYTES-1,true)==PTY_BYTES-1);
    pump=replace_fd;
    assert(process_file_pty_write_checked(&test_process,0,2,checked_in,"yz")==1);
    drain_output();
    test_file.type=PROCESS_FILE_PTY_MASTER;pump=replace_fd;
    assert(process_file_pty_read_checked(&test_process,0,sizeof(bytes),checked_out,bytes)==SRV_ERR_BADF);
    assert(pty_write(active,false,(const uint8_t *)"pending",7,true)==7);
    pump=replace_fd;struct srv_termios changed=pty_state(active)->attributes;changed.lflag=0;
    assert(process_file_pty_setattr(&test_process,0,&changed,SRV_TCSADRAIN)==SRV_ERR_BADF);
    pump=drain_output;assert(process_file_pty_setattr(&test_process,0,&changed,SRV_TCSADRAIN)==0);
    test_file.type=PROCESS_FILE_PTY_MASTER;
    assert(process_file_pty_write_checked(&test_process,0,1,checked_in,"z")==1);
    test_file.type=PROCESS_FILE_PTY_SLAVE;
    assert(process_file_pty_read_checked(&test_process,0,sizeof(bytes),checked_out,bytes)==1 && bytes[0]=='z');
    pty_close(active,true);pty_close(active,false);
    assert(wakes>0);
    puts("pty-manager: PASS capacity/generations, wait/wake, interruption, output backpressure/drain, foreground signals and hangup");
}
