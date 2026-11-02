#include <assert.h>
#include <stdio.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static bool console_fd,busy,user_safe,startup_killed,kill_on_wait,invalidate_on_wait;
bool process_should_exit_current(void) { return startup_killed; }
bool scheduler_io_active(void) {return busy;}
uint64_t latency_clock(void) {return 1;}
static unsigned gate_samples;
void latency_gate(uint64_t at,uint64_t call) {assert(at==1 && (call==SYS_MMAP || call==SYS_THREAD_CREATE));gate_samples++;}
static unsigned turn_requests;
static bool retired;
static bool wait_pointer_ok=true;static unsigned wait_attempts;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool write) {(void)address;(void)length;(void)write;return wait_pointer_ok;}
bool process_try_wait_retired(uint64_t pid,uint64_t *status,int64_t *result) {assert(pid==42);wait_attempts++;if(!retired)return false;*status=9;*result=42;return true;}
int64_t process_wait(uint64_t pid,uint64_t *status,bool nohang) {assert(pid==42 && !busy && !user_safe && nohang);*status=0;return 0;}
int64_t process_wait_posix(int64_t pid,uint64_t *status,bool nohang) {(void)pid;(void)status;(void)nohang;assert(0);return -1;}
static bool byte_safe;
bool process_file_pipe_io_safe(struct process *p,uint64_t fd,uint64_t length,bool writing) {
    (void)p;(void)writing;return fd==10 && length<=4096 && byte_safe;
}
void scheduler_io_request_turn(void) { assert(busy);turn_requests++; }
static struct process_file pipe_file;
struct process_file *process_file_at(struct process *p,uint64_t fd) { (void)p; return fd==10?&pipe_file:NULL; }
bool process_file_nonblocking(struct process *p,uint64_t fd) { struct process_file *f=process_file_at(p,fd);return f && (f->flags&SRV_FD_NONBLOCK); }
bool scheduler_io_busy(void) { return busy; }
void scheduler_io_set_user_safe(bool value) { user_safe=value; }
struct process *process_current(void) { return (struct process *)1; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) {
    assert(p==(struct process *)1);
    if(fd==10) return false;
    assert(input ? fd==0 : fd==1);
    return console_fd;
}
static bool close_pipe;
static unsigned close_attempts,close_waits,slow_closes;
bool process_file_try_close_pipe(struct process *p,uint64_t fd,int64_t *result) {
    assert(p==(struct process *)1);close_attempts++;
    if(fd!=10 || !close_pipe)return false;
    *result=0;return true;
}
void scheduler_io_wait(void) {
    assert(!user_safe);close_waits++;busy=false;pipe_file.handle=456;
    if(kill_on_wait)startup_killed=true;
    if(invalidate_on_wait)wait_pointer_ok=false;
}
int64_t process_file_close(struct process *p,uint64_t fd) {
    assert(p==(struct process *)1 && fd==10 && !user_safe && !busy && pipe_file.handle==456);
    slow_closes++;return -7;
}
static void test_close_admission(void) {
    struct isr_frame frame={.rax=SYS_CLOSE,.rdi=10};
    assert(syscall_io_safe(&frame)); /* Prelude only, not general close permission. */
    busy=user_safe=close_pipe=true;assert(syscall_close(10)==0);
    assert(busy && user_safe && close_attempts==1 && !close_waits && !slow_closes);
    close_pipe=false;pipe_file.handle=123;
    assert(syscall_close(10)==-7);
    assert(!busy && !user_safe && close_attempts==2 && close_waits==1 && slow_closes==1);
    puts("pipe-close-admission: PASS atomic attempt or gated fresh-lookup fallback");
}
static bool private_unmap;
static unsigned unmap_calls,unmap_slow,unmap_handoffs;
void scheduler_io_handoff(void) { assert(user_safe && !busy);unmap_handoffs++; }
bool process_try_munmap_private(struct process *p,uint64_t address,uint64_t length,int64_t *result) {
    assert(p==(struct process *)1 && address==0x400000 && length==8192);unmap_calls++;
    if(!private_unmap)return false;
    *result=0;return true;
}
int64_t process_munmap(struct process *p,uint64_t address,uint64_t length) {
    assert(p==(struct process *)1 && address==0x400000 && length==8192 && !busy && !user_safe);
    unmap_slow++;return -8;
}
static void test_unmap_admission(void) {
    struct isr_frame f={.rax=SYS_MUNMAP};assert(syscall_io_safe(&f));
    unsigned before=close_waits;busy=user_safe=private_unmap=true;
    assert(!syscall_munmap(0x400000,8192) && busy && user_safe && close_waits==before && unmap_calls==1 && !unmap_handoffs);
    private_unmap=false;assert(syscall_munmap(0x400000,8192)==-8);
    assert(!busy && !user_safe && close_waits==before+1 && unmap_calls==2 && unmap_slow==1 && unmap_handoffs==1);
    puts("private-unmap-admission: PASS atomic attempt or ordinary gated fallback; EXIT remains gated");
}

static bool startup_fast;static unsigned startup_attempts,startup_slow;
bool process_try_mmap_private(struct process *p,uint64_t address,uint64_t length,uint64_t prot,uint64_t flags,int64_t fd,int64_t offset,int64_t *result) {
    (void)p;(void)address;(void)length;(void)prot;(void)flags;(void)fd;(void)offset;startup_attempts++;
    if(!startup_fast)return false;
    *result=0x500000;return true;
}
int64_t process_mmap(struct process *p,uint64_t address,uint64_t length,uint64_t prot,uint64_t flags,int64_t fd,int64_t offset) {
    (void)p;(void)address;(void)length;(void)prot;(void)flags;(void)fd;(void)offset;
    assert(!busy && !user_safe);startup_slow++;return -9;
}
bool process_try_thread_create_private(uint64_t e,uint64_t a,uint64_t s,uint64_t f,int64_t *result) {
    (void)e;(void)a;(void)s;(void)f;startup_attempts++;if(!startup_fast)return false;*result=2;return true;
}
int64_t process_thread_create(uint64_t e,uint64_t a,uint64_t s,uint64_t f) {
    (void)e;(void)a;(void)s;(void)f;assert(!busy && !user_safe);startup_slow++;return -10;
}
static void test_startup_admission(void) {
    struct isr_frame f={.rax=SYS_MMAP};assert(syscall_io_safe(&f));
    f.rax=SYS_THREAD_CREATE;assert(syscall_io_safe(&f));
    unsigned waits=close_waits,handoffs=unmap_handoffs;
    busy=user_safe=startup_fast=true;
    assert(syscall_mmap(0,4096,3,9,-1,0)==0x500000);
    assert(syscall_thread_create(0x400000,123,0x500800,0)==2);
    assert(busy && user_safe && startup_attempts==2 && !startup_slow && close_waits==waits && unmap_handoffs==handoffs);
    startup_fast=false;
    assert(syscall_mmap(0,4096,3,9,-1,0)==-9);
    busy=user_safe=true;assert(syscall_thread_create(0x400000,123,0x500800,0)==-10);
    assert(!busy && !user_safe && startup_slow==2 && close_waits==waits+2 && unmap_handoffs==handoffs+2);
    busy=user_safe=true;wait_pointer_ok=false;
    assert(syscall_thread_create(0x400000,123,0x500800,0)==-1 && !busy && !user_safe && unmap_handoffs==handoffs+3);
    wait_pointer_ok=true;
    busy=user_safe=kill_on_wait=true;unsigned slow=startup_slow;
    assert(syscall_thread_create(0x400000,123,0x500800,0)==-1 && startup_killed && startup_slow==slow);
    startup_killed=false;busy=user_safe=true;
    assert(syscall_mmap(0,4096,3,9,-1,0)==-1 && startup_killed && startup_slow==slow);
    assert(gate_samples==5);startup_killed=kill_on_wait=false;
    busy=user_safe=invalidate_on_wait=true;
    assert(syscall_thread_create(0x400000,123,0x500800,0)==-1 && startup_slow==slow && !wait_pointer_ok);
    assert(gate_samples==6);invalidate_on_wait=false;wait_pointer_ok=true;
    puts("startup-admission: PASS sole-process attempt or ordinary gated fallback, invalid-buffer handoff, no foreign gate mutation");
}
static void test_pipe_retry_admission(void) {
    const struct { enum process_file_type type; bool read,write; } cases[]={
        {PROCESS_FILE_PIPE_READ,true,false},{PROCESS_FILE_PIPE_WRITE,false,true},
        {PROCESS_FILE_PIPE_DUPLEX,true,true},{PROCESS_FILE_PTY_MASTER,true,true},
        {PROCESS_FILE_PTY_SLAVE,true,true},{PROCESS_FILE_VFS,false,false},
        {PROCESS_FILE_VFS_WRITE,false,false},{PROCESS_FILE_NET_CONNECTION,false,false}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        pipe_file.type=cases[i].type;
        for(unsigned nonblock=0;nonblock<2;nonblock++) {
            pipe_file.flags=nonblock?SRV_FD_NONBLOCK:0;
            for(unsigned write=0;write<2;write++) {
                bool eligible=nonblock && (write?cases[i].write:cases[i].read);
                unsigned before=turn_requests;
                busy=true;user_safe=true;
                /* Each retry must request quiescence without blocking or
                 * entering descriptor cleanup under the other I/O owner. */
                for(unsigned retry=0;retry<64;retry++) {
                    assert((write?io_defer_pipe_write(10,4096):io_defer_pipe_read(10,4096))==eligible);
                    assert(user_safe && turn_requests==before+(eligible?retry+1:0));
                }
                before=turn_requests;busy=false;user_safe=true;
                assert(!(write?io_defer_pipe_write(10,4096):io_defer_pipe_read(10,4096)));
                assert(turn_requests==before && user_safe==!eligible);
            }
        }
    }
    busy=true;user_safe=true;unsigned before=turn_requests;
    assert(!io_defer_pipe_read(99,4096) && !io_defer_pipe_write(99,4096));
    assert(user_safe && turn_requests==before); /* Invalid descriptors are not contenders. */
    busy=false;
    puts("pipe-retry-admission: PASS read/write/duplex/PTY, repeated requests, blocking/invalid/nonpipe exclusion and cleanup safety");
}
int main(void) {
    test_close_admission();
    test_unmap_admission();
    struct isr_frame frame={.rax=SYS_READ,.rdi=0};
    assert(!syscall_io_safe(&frame));console_fd=true;assert(syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;frame.rdi=1;assert(syscall_io_safe(&frame));
    console_fd=false;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_DIRECTORY;frame.rdi=SRV_DIRECTORY_READ_INFO;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_POINTER_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_BATTERY_STATUS;assert(syscall_io_safe(&frame));
    frame.rax=SYS_BATTERY_CONTROL;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_WAIT_IO;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_CANCEL_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_SCHED_TRACE;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_SERVER_WAIT;assert(syscall_io_safe(&frame));
    const uint64_t blocked[]={SYS_SIGNAL_POLL,SYS_SIGNAL_CONSUME,SYS_OPEN,SYS_FS_WRITE,SYS_SYNC,SYS_EXIT,SYS_PREAD,SYS_PWRITE,SYS_VECTOR_IO,SYS_PIPE2,SYS_DUP3,SYS_PIPE_PAIR_FLAGS,SYS_THREAD_SIGNAL,
        SYS_SOCKET_TRY_IO,SYS_KILL,SYS_SPAWN,SYS_FILE_CHANGE,UINT64_MAX};
    for(unsigned i=0;i<sizeof(blocked)/sizeof(*blocked);i++) {
        frame.rax=blocked[i];assert(!syscall_io_safe(&frame));
    }
    const uint64_t admitted[]={SYS_SIGNAL_CONFIG,SYS_SIGNAL_PENDING,SYS_SIGNAL_MASK,SYS_POLL,SYS_CONSOLE_INFO,SYS_CONSOLE_CLEAR,SYS_CONSOLE_GOTOXY,SYS_GUI_SEND,SYS_GUI_RECV,SYS_MOUSE_SCAN,
        SYS_GUI_SURFACE_BLIT,SYS_GUI_SURFACE_COPY,SYS_GUI_SURFACE_CREATE,SYS_GUI_SURFACE_DESTROY,
        SYS_GFX_BLIT_RECT,SYS_GFX_BLIT_SCALED_RECT,SYS_SCHEDULER_STATUS,SYS_PROC_LIST,SYS_MEMINFO,SYS_SLEEP_TICKS,SYS_REALTIME_US,SYS_MONOTONIC_US};
    for(unsigned i=0;i<sizeof(admitted)/sizeof(*admitted);i++) { frame.rax=admitted[i]; assert(syscall_io_safe(&frame)); }
    frame.rax=SYS_WAIT; frame.rdx=0; assert(syscall_io_safe(&frame));
    frame.rdx=SRV_WAIT_NOHANG; assert(syscall_io_safe(&frame));
    busy=true;user_safe=true;
    assert(io_defer_wait(SRV_WAIT_NOHANG) && turn_requests==1 && user_safe);
    assert(io_defer_wait(SRV_WAIT_NOHANG|SRV_WAIT_POSIX) && turn_requests==2 && user_safe);
    assert(!io_defer_wait(0) && turn_requests==2); /* Blocking path retains its gate. */
    busy=false;assert(!io_defer_wait(SRV_WAIT_NOHANG) && turn_requests==2);
    busy=user_safe=retired=true;uint64_t wait_status=0;wait_pointer_ok=false;
    assert(syscall_wait(42,&wait_status,0)==SRV_ERR_FAULT && !wait_attempts);
    wait_pointer_ok=true;assert(syscall_wait(42,&wait_status,0)==42 && wait_status==9 && busy && user_safe);
    unsigned attempts=wait_attempts;
    assert(syscall_wait(42,NULL,~(SRV_WAIT_NOHANG|SRV_WAIT_POSIX))==SRV_ERR_INVAL && wait_attempts==attempts);
    assert(!syscall_wait(42,NULL,SRV_WAIT_NOHANG|SRV_WAIT_POSIX) && wait_attempts==attempts);
    retired=false;unsigned turns=turn_requests;assert(!syscall_wait(42,NULL,SRV_WAIT_NOHANG) && turn_requests==turns+1 && user_safe);
    busy=false;assert(!syscall_wait(42,NULL,SRV_WAIT_NOHANG) && !user_safe);
    frame.rax=SYS_READ; frame.rdi=10;
    pipe_file.type=PROCESS_FILE_PIPE_READ; assert(!syscall_io_safe(&frame));
    pipe_file.flags=SRV_FD_NONBLOCK; assert(syscall_io_safe(&frame));
    busy=true; user_safe=true; assert(io_defer_pipe_read(10,4096) && user_safe);
    busy=false; assert(!io_defer_pipe_read(10,4096) && !user_safe);
    pipe_file.type=PROCESS_FILE_VFS;frame.rdx=16384;assert(syscall_io_safe(&frame));
    frame.rdx=16385;assert(!syscall_io_safe(&frame));frame.rdx=UINT64_MAX;assert(!syscall_io_safe(&frame));
    frame.rdx=0;assert(syscall_io_safe(&frame));
    assert(!io_snapshot_read(0,16) && !io_snapshot_read(1,16) && !io_snapshot_read(2,16));
    pipe_file.type=PROCESS_FILE_VFS_WRITE;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_NET_CONNECTION; assert(!syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;pipe_file.type=PROCESS_FILE_PIPE_WRITE;pipe_file.flags=0;
    assert(!syscall_io_safe(&frame));pipe_file.flags=SRV_FD_NONBLOCK;assert(syscall_io_safe(&frame));
    busy=true;user_safe=true;assert(io_defer_pipe_write(10,4096) && user_safe);
    busy=false;assert(!io_defer_pipe_write(10,4096) && !user_safe);
    pipe_file.type=PROCESS_FILE_PIPE_READ;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_PIPE_DUPLEX;assert(syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_VFS_WRITE;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_PTY_MASTER;assert(syscall_io_safe(&frame));
    frame.rax=SYS_READ;assert(syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_PTY_SLAVE;assert(syscall_io_safe(&frame));
    pipe_file.flags=0;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_TTY_SETATTR;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_PTY_CREATE;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_FUTEX_WAKE;assert(syscall_io_safe(&frame));
    frame.rax=SYS_FUTEX_WAIT;assert(syscall_io_safe(&frame));
    busy=user_safe=true;unsigned identity_waits=close_waits,identity_turns=turn_requests;
    frame.rax=SYS_THREAD_SELF;assert(syscall_io_safe(&frame));
    assert(busy && user_safe && close_waits==identity_waits && turn_requests==identity_turns);
    const uint64_t thread_gated[]={SYS_THREAD_JOIN,SYS_THREAD_DETACH,SYS_THREAD_STATUS,SYS_SBRK,SYS_EXIT};
    for(unsigned n=0;n<sizeof(thread_gated)/sizeof(thread_gated[0]);n++) {
        frame.rax=thread_gated[n];assert(!syscall_io_safe(&frame));
    }

    frame.rax=SYS_KEY_SCAN;assert(syscall_io_safe(&frame));
    frame.rax=SYS_TTY_GETATTR;assert(syscall_io_safe(&frame));
    test_startup_admission();
    test_pipe_retry_admission();
    pipe_file.type=PROCESS_FILE_PIPE_DUPLEX;pipe_file.flags=SRV_FD_NONBLOCK;
    busy=true;user_safe=true;byte_safe=true;unsigned before=turn_requests;
    for(unsigned i=0;i<1024;i++) {
        assert(!io_defer_pipe_write(10,4096) && !io_defer_pipe_read(10,32));
        assert(user_safe && turn_requests==before);
    }
    assert(io_defer_pipe_write(10,4097) && io_defer_pipe_read(10,4097));
    byte_safe=false;assert(io_defer_pipe_write(10,4096) && io_defer_pipe_read(10,32));
    assert(user_safe && turn_requests==before+4);

    puts("syscall-io-host: PASS (bounded desktop admission, bounded retained-snapshot reads, nonblocking pipe deferral, redirected I/O/lifecycle/mapping/filesystem/default-deny)");
}
