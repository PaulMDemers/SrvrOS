/* Actual syscall poll implementation with explicit scheduler/descriptor model. */
#define SRVROS_SIGNAL_PUBLIC_GUEST_TEST
#define SRVROS_SIGNAL_EXPERIMENTAL_INFO
#define SRVROS_SYSCALL_SLEEP_HOST_TEST
#define SRVROS_SYSCALL_IO_HOST_TEST
#define main legacy_poll_fixture
#include "test_syscall_poll.c"
#undef main
static bool info_pending;
static uint64_t generations[32];
uint64_t process_fd_generation(struct process *p,uint64_t fd) {assert(p==(struct process *)1);return fd<32?generations[fd]:UINT64_MAX;}
bool process_signal_info_pending_current(void) {return info_pending;}
bool process_signal_experimental_fd_allowed(uint64_t fd) {return fd!=31;}
bool timer_reference_ready(void) {return true;}
static bool wait_child_ready,wait_fast;static unsigned wait_claims;
bool process_try_wait_retired_checked(uint64_t pid,process_wait_copy_fn copy,void *ctx,int64_t *result) {
    if(!wait_fast)return false;
    if(!copy(ctx,0x12345678)) {*result=SRV_ERR_FAULT;return true;}
    wait_claims++;*result=(int64_t)pid;return true;
}
int64_t process_wait_checked(int64_t selector,uint64_t flags,process_wait_copy_fn copy,void *ctx) {
    (void)flags;if(!wait_child_ready)return 0;
    if(!copy(ctx,0x12345678))return SRV_ERR_FAULT;
    wait_claims++;return selector;
}
void scheduler_io_handoff(void) {}
void scheduler_io_request_turn(void) {}
static bool inject_retire,allow_input;static unsigned consumed_chars;
int keyboard_read_char_commit(char *out,bool wait,int (*eligible)(void *),void (*commit)(void *,char),void *ctx) {
    (void)wait;if(inject_retire)generations[0]++;
    int status=eligible(ctx);if(status)return status;
    if(!allow_input)return 0;*out=0;consumed_chars++;if(commit)commit(ctx,*out);return 1;
}
size_t console_write_n_checked(const char *data,size_t length,bool (*eligible)(void *),void *ctx) {
    (void)data;if(!eligible(ctx))return 0;
    size_t accepted=length>2?2:length;
    if(inject_retire)generations[0]++;
    return accepted;
}
void console_putc(char c) {(void)c;}

int main(void) {
    legacy_poll_fixture();
    redirected=false;redirect_on_wait=false;safe=true;buffered=0;serial_ready=false;signal_pending=false;
    struct syscall_pollfd captured={.fd=0,.events=SRV_POLLIN};uint64_t expected=9;generations[0]=9;
    user[0]=captured;info_pending=true;
    assert(syscall_poll_prepared(user,1,0,&captured,&expected)==0);
    user[0]=captured;assert(syscall_poll_prepared(user,1,100,&captured,&expected)==SRV_ERR_INTR);
    buffered=1;user[0]=captured;assert(syscall_poll_prepared(user,1,100,&captured,&expected)==1 && user[0].revents==SRV_POLLIN);
    buffered=0;generations[0]=10;user[0]=captured;
    assert(syscall_poll_prepared(user,1,100,&captured,&expected)==1 && user[0].revents==SRV_POLLNVAL);
    generations[0]=9;expected=9;info_pending=false;arrive=true;mutate=true;user[0]=captured;
    assert(syscall_poll_prepared(user,1,100,&captured,&expected)==1 && user[0].fd==0 && user[0].revents==SRV_POLLIN);
    arrive=mutate=false;buffered=0;info_pending=true;
    generations[0]=9;expected=UINT64_MAX;user[0]=captured;
    assert(syscall_poll_prepared(user,1,100,&captured,&expected)==1 && user[0].revents==SRV_POLLNVAL);
    const uint64_t denied[]={SYS_NET_LISTEN,SYS_NET_ACCEPT,SYS_NET_DHCP,SYS_NET_STATUS,SYS_NET_DNS,
        SYS_NET_CONNECT,SYS_NET_UDP_OPEN,SYS_NET_UDP_BIND,SYS_NET_UDP_SENDTO,SYS_NET_UDP_RECVFROM,
        SYS_NET_SOCKNAME,SYS_NET_PEERNAME,SYS_NET_SHUTDOWN,SYS_NET_ERROR,SYS_NET_LIST,SYS_NET_STATUS_INFO,
        SYS_NET_ARP_LIST,SYS_NET_PING,SYS_NET_PEEK,SYS_UNIX_BIND,SYS_UNIX_LISTEN,SYS_UNIX_CONNECT,
        SYS_UNIX_ACCEPT,SYS_UNIX_UNLINK,SYS_SOCKET_TRY_IO,SYS_PREAD,SYS_PWRITE,SYS_PIPE_MESSAGE,
        SYS_PIPE_SEND_RIGHTS,SYS_PIPE_RECV_RIGHTS,SYS_PIPE_PAIR,SYS_PIPE_PAIR_FLAGS};
    struct isr_frame policy={0};
    for(unsigned i=0;i<sizeof(denied)/sizeof(denied[0]);i++) {policy.rax=denied[i];assert(!signal_experimental_raw_allowed(&policy));}
    policy.rax=SYS_FCNTL;policy.rdi=0;
    policy.rsi=SRV_F_GETFD;assert(signal_experimental_raw_allowed(&policy));
    policy.rsi=SRV_F_GETFL;assert(signal_experimental_raw_allowed(&policy));
    policy.rsi=SRV_F_SETLKW;assert(!signal_experimental_raw_allowed(&policy));
    policy.rdi=31;policy.rsi=SRV_F_GETFD;assert(!signal_experimental_raw_allowed(&policy));
    policy.rsi=SRV_F_GETFL;assert(!signal_experimental_raw_allowed(&policy));
    const uint64_t byfd[]={SYS_READ,SYS_WRITE,SYS_VECTOR_IO};
    for(unsigned i=0;i<sizeof(byfd)/sizeof(byfd[0]);i++) {policy.rax=byfd[i];policy.rdi=31;assert(!signal_experimental_raw_allowed(&policy));policy.rdi=0;assert(signal_experimental_raw_allowed(&policy));}
    struct srv_pipe_message rights={.fds={0,31},.count=2};assert(!signal_experimental_rights_allowed(&rights));
    rights.fds[1]=4;assert(signal_experimental_rights_allowed(&rights));rights.count=SRV_RIGHTS_MAX+1;
    assert(!signal_experimental_rights_allowed(&rights));
    struct signal_io_snapshot s={0};struct srv_signal_io_v1 w={.raw_syscall=SYS_PIPE_MESSAGE,.descriptor_count=3};
    w.args[0]=4;w.args[3]=SRV_PIPE_MESSAGE_SEND;s.pipe.message.count=2;s.pipe.message.fds[0]=7;s.pipe.message.fds[1]=8;
    s.expected[0]=(struct srv_signal_descriptor_v1){4,1};s.expected[1]=(struct srv_signal_descriptor_v1){7,2};s.expected[2]=(struct srv_signal_descriptor_v1){8,3};
    assert(signal_io_coverage(&w,&s));s.expected[2].fd=7;assert(!signal_io_coverage(&w,&s));
    s.pipe.message.fds[1]=7;assert(!signal_io_coverage(&w,&s));s.expected[2].value=2;assert(signal_io_coverage(&w,&s));
    w=(struct srv_signal_io_v1){.raw_syscall=SYS_POLL,.descriptor_count=2};w.args[1]=3;
    s.poll[0].fd=4;s.poll[1].fd=-1;s.poll[2].fd=7;assert(signal_io_coverage(&w,&s));
    assert(s.poll_generation[0]==1 && s.poll_generation[1]==UINT64_MAX && s.poll_generation[2]==2);
    s.expected[1].fd=8;assert(!signal_io_coverage(&w,&s));
    /* Actual checked user-copy helper normalizes aliased descriptor/iovec
     * metadata before any later user mutation. */
    struct srv_signal_descriptor_v1 alias={.fd=(int64_t)(uintptr_t)user,.value=0};memcpy(user,&alias,sizeof alias);
    struct srv_signal_io_v1 vector_wire={.raw_syscall=SYS_VECTOR_IO,.descriptor_count=1,.descriptor_vector=(uint64_t)user};
    vector_wire.args[0]=(uint64_t)user;vector_wire.args[1]=(uint64_t)user;vector_wire.args[2]=1;
    struct signal_io_snapshot frozen={0};assert(signal_io_snapshot_operands(&vector_wire,&frozen)==0);
    memset(user,0,sizeof user);assert(frozen.expected[0].fd==(int64_t)(uintptr_t)user && frozen.expected[0].value==0 &&
        frozen.iov[0].base==(void *)user && frozen.iov[0].length==0);
    vector_wire.descriptor_vector=0;assert(signal_io_snapshot_operands(&vector_wire,&frozen)==SRV_ERR_FAULT);
    struct signal_sleep_elapsed elapsed={0};signal_pending=false;arrive=mutate=false;
    assert(syscall_sleep_ticks_measured(3,&elapsed)==0 && elapsed.ticks==3 && elapsed.mono_valid && elapsed.us==30000);
    elapsed=(struct signal_sleep_elapsed){0};signal_pending=true;
    assert(syscall_sleep_ticks_measured(100,&elapsed)==SRV_ERR_INTR && elapsed.ticks==0 && elapsed.us==0 && elapsed.mono_valid);
    signal_pending=false;
    wait_child_ready=true;wait_fast=false;memset(user,0,sizeof user);
    assert(syscall_wait(42,(uint64_t *)user,0)==42 && wait_claims==1);
    uint64_t wait_status;memcpy(&wait_status,user,sizeof wait_status);assert(wait_status==0x12345678);
    assert(syscall_wait(42,(uint64_t *)1,0)==SRV_ERR_FAULT && wait_claims==1);
    wait_fast=true;assert(syscall_wait(42,NULL,0)==42 && wait_claims==2);
    wait_fast=false;wait_child_ready=false;memset(user,0x5a,sizeof user);
    assert(syscall_wait(42,(uint64_t *)user,SRV_WAIT_NOHANG)==0 && wait_claims==2 && ((unsigned char *)user)[0]==0x5a);
    /* Checked TTY consumes NUL as real data, and retirement/signal refuses
     * before the primitive consumes; checked console returns committed prefix. */
    char byte=9;generations[0]=9;signal_pending=false;inject_retire=false;allow_input=true;
    console_termios.lflag=0;console_termios.cc[SRV_TTY_VMIN]=1;
    assert(tty_read_console(0,&byte,1,true)==1 && byte==0 && consumed_chars==1);
    signal_pending=true;byte=9;assert(tty_read_console(0,&byte,1,true)==SRV_ERR_INTR && byte==9 && consumed_chars==1);
    signal_pending=false;inject_retire=true;assert(tty_read_console(0,&byte,1,true)==SRV_ERR_BADF && consumed_chars==1);
    inject_retire=false;generations[0]=9;struct srv_iovec viov={.base=user,.length=1};
    memset(user,0x5a,sizeof user);assert(tty_read_console_sink(0,NULL,1,false,&viov,1)==1 && ((char *)user)[0]==0 && consumed_chars==2);
    viov.base=(void *)1;assert(tty_read_console_sink(0,NULL,1,false,&viov,1)==SRV_ERR_FAULT && consumed_chars==2);
    /* This model treats fd 0 as console output for the eligibility callback. */
    puts("signal-io-backend: actual poll and descriptor coverage model PASS");
}
