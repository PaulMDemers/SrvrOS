#include <assert.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static bool console_fd;
struct process *process_current(void) { return (struct process *)1; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) {
    assert(p==(struct process *)1);
    assert(input ? fd==0 : fd==1);
    return console_fd;
}
int main(void) {
    struct isr_frame frame={.rax=SYS_READ,.rdi=0};
    assert(!syscall_io_safe(&frame));console_fd=true;assert(syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;frame.rdi=1;assert(syscall_io_safe(&frame));
    console_fd=false;assert(!syscall_io_safe(&frame));
    const uint64_t blocked[]={SYS_OPEN,SYS_CLOSE,SYS_FS_WRITE,SYS_SYNC,SYS_EXIT,
        SYS_KILL,SYS_SPAWN,SYS_POLL,SYS_MMAP,SYS_MUNMAP,UINT64_MAX};
    for(unsigned i=0;i<sizeof(blocked)/sizeof(*blocked);i++) {
        frame.rax=blocked[i];assert(!syscall_io_safe(&frame));
    }
    frame.rax=SYS_FUTEX_WAKE;assert(syscall_io_safe(&frame));
    frame.rax=SYS_FUTEX_WAIT;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_KEY_SCAN;assert(syscall_io_safe(&frame));
    frame.rax=SYS_TTY_GETATTR;assert(syscall_io_safe(&frame));
    puts("syscall-io-host: PASS (console-only admission, redirected I/O gated, lifecycle/mapping/filesystem/default-deny)");
}
