#include <assert.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static bool console_fd,busy,user_safe;
static struct process_file pipe_file;
struct process_file *process_file_at(struct process *p,uint64_t fd) { (void)p; return fd==10?&pipe_file:NULL; }
bool scheduler_io_busy(void) { return busy; }
void scheduler_io_set_user_safe(bool value) { user_safe=value; }
struct process *process_current(void) { return (struct process *)1; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) {
    assert(p==(struct process *)1);
    if(fd==10) return false;
    assert(input ? fd==0 : fd==1);
    return console_fd;
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
    frame.rax=SYS_GUI_SERVER_WAIT;assert(syscall_io_safe(&frame));
    const uint64_t blocked[]={SYS_OPEN,SYS_CLOSE,SYS_FS_WRITE,SYS_SYNC,SYS_EXIT,
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
    puts("syscall-io-host: PASS (bounded desktop admission, bounded retained-snapshot reads, nonblocking pipe deferral, redirected I/O/lifecycle/mapping/filesystem/default-deny)");
}
