#include <assert.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static bool console_fd,busy,user_safe;
static unsigned turn_requests;
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
                    assert((write?io_defer_pipe_write(10):io_defer_pipe_read(10))==eligible);
                    assert(user_safe && turn_requests==before+(eligible?retry+1:0));
                }
                before=turn_requests;busy=false;user_safe=true;
                assert(!(write?io_defer_pipe_write(10):io_defer_pipe_read(10)));
                assert(turn_requests==before && user_safe==!eligible);
            }
        }
    }
    busy=true;user_safe=true;unsigned before=turn_requests;
    assert(!io_defer_pipe_read(99) && !io_defer_pipe_write(99));
    assert(user_safe && turn_requests==before); /* Invalid descriptors are not contenders. */
    busy=false;
    puts("pipe-retry-admission: PASS read/write/duplex/PTY, repeated requests, blocking/invalid/nonpipe exclusion and cleanup safety");
}
int main(void) {
    struct isr_frame frame={.rax=SYS_READ,.rdi=0};
    assert(!syscall_io_safe(&frame));console_fd=true;assert(syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;frame.rdi=1;assert(syscall_io_safe(&frame));
    console_fd=false;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_POINTER_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_BATTERY_STATUS;assert(syscall_io_safe(&frame));
    frame.rax=SYS_BATTERY_CONTROL;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_WAIT_IO;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_CANCEL_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_SERVER_WAIT;assert(syscall_io_safe(&frame));
    const uint64_t blocked[]={SYS_OPEN,SYS_CLOSE,SYS_FS_WRITE,SYS_SYNC,SYS_EXIT,SYS_PREAD,SYS_PWRITE,SYS_VECTOR_IO,SYS_PIPE2,SYS_DUP3,SYS_PIPE_PAIR_FLAGS,SYS_THREAD_SIGNAL,
        SYS_KILL,SYS_SPAWN,SYS_MMAP,SYS_MUNMAP,SYS_FILE_CHANGE,UINT64_MAX};
    for(unsigned i=0;i<sizeof(blocked)/sizeof(*blocked);i++) {
        frame.rax=blocked[i];assert(!syscall_io_safe(&frame));
    }
    const uint64_t admitted[]={SYS_POLL,SYS_CONSOLE_INFO,SYS_CONSOLE_CLEAR,SYS_CONSOLE_GOTOXY,SYS_GUI_SEND,SYS_GUI_RECV,SYS_MOUSE_SCAN,
        SYS_GUI_SURFACE_BLIT,SYS_GUI_SURFACE_COPY,SYS_GUI_SURFACE_CREATE,SYS_GUI_SURFACE_DESTROY,
        SYS_GFX_BLIT_RECT,SYS_GFX_BLIT_SCALED_RECT,SYS_SCHEDULER_STATUS,SYS_PROC_LIST,SYS_MEMINFO,SYS_SLEEP_TICKS,SYS_REALTIME_US,SYS_MONOTONIC_US};
    for(unsigned i=0;i<sizeof(admitted)/sizeof(*admitted);i++) { frame.rax=admitted[i]; assert(syscall_io_safe(&frame)); }
    frame.rax=SYS_WAIT; frame.rdx=0; assert(!syscall_io_safe(&frame));
    frame.rdx=SRV_WAIT_NOHANG; assert(syscall_io_safe(&frame));
    busy=true;user_safe=true;
    assert(io_defer_wait(SRV_WAIT_NOHANG) && turn_requests==1 && user_safe);
    assert(io_defer_wait(SRV_WAIT_NOHANG|SRV_WAIT_POSIX) && turn_requests==2 && user_safe);
    assert(!io_defer_wait(0) && turn_requests==2); /* Blocking path retains its gate. */
    busy=false;assert(!io_defer_wait(SRV_WAIT_NOHANG) && turn_requests==2);
    frame.rax=SYS_READ; frame.rdi=10;
    pipe_file.type=PROCESS_FILE_PIPE_READ; assert(!syscall_io_safe(&frame));
    pipe_file.flags=SRV_FD_NONBLOCK; assert(syscall_io_safe(&frame));
    busy=true; user_safe=true; assert(io_defer_pipe_read(10) && user_safe);
    busy=false; assert(!io_defer_pipe_read(10) && !user_safe);
    pipe_file.type=PROCESS_FILE_VFS;frame.rdx=16384;assert(syscall_io_safe(&frame));
    frame.rdx=16385;assert(!syscall_io_safe(&frame));frame.rdx=UINT64_MAX;assert(!syscall_io_safe(&frame));
    frame.rdx=0;assert(syscall_io_safe(&frame));
    assert(!io_snapshot_read(0,16) && !io_snapshot_read(1,16) && !io_snapshot_read(2,16));
    pipe_file.type=PROCESS_FILE_VFS_WRITE;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_NET_CONNECTION; assert(!syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;pipe_file.type=PROCESS_FILE_PIPE_WRITE;pipe_file.flags=0;
    assert(!syscall_io_safe(&frame));pipe_file.flags=SRV_FD_NONBLOCK;assert(syscall_io_safe(&frame));
    busy=true;user_safe=true;assert(io_defer_pipe_write(10) && user_safe);
    busy=false;assert(!io_defer_pipe_write(10) && !user_safe);
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
    frame.rax=SYS_FUTEX_WAIT;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_KEY_SCAN;assert(syscall_io_safe(&frame));
    frame.rax=SYS_TTY_GETATTR;assert(syscall_io_safe(&frame));
    test_pipe_retry_admission();
    puts("syscall-io-host: PASS (bounded desktop admission, bounded retained-snapshot reads, nonblocking pipe deferral, redirected I/O/lifecycle/mapping/filesystem/default-deny)");
}
