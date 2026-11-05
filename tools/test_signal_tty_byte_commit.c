/* Actual TTY callback: one current byte, independent of later mappings. */
#include <assert.h>
#include <stdio.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
struct process *process_current(void) {return (struct process *)1;}
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) {assert(p==(struct process *)1 && fd==0 && input);return true;}
bool process_should_exit_current(void) {return false;}
bool process_signal_pending_current(void) {return false;}
static uint64_t allowed_address,last_address,last_length;
static unsigned mapping_checks;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(writable);mapping_checks++;last_address=address;last_length=length;
    return address==allowed_address && length==1;
}
uint64_t process_fd_generation(struct process *p,uint64_t fd) {assert(p==(struct process *)1 && fd==0);return 7;}
int main(void) {
    char output[8]={0};
    struct tty_checked_read r={.guard={.process=(struct process *)1,.fd=0,.generation=7},
        .buffer=output,.length=MAX_SYSCALL_COPY,.count=3};
    allowed_address=(uint64_t)(uintptr_t)(output+3);
    assert(tty_read_eligible(&r)==0 && mapping_checks==1 && last_length==1 && last_address==allowed_address);
    tty_read_commit(&r,'A');assert(output[3]=='A' && r.count==4);
    /* An owned sink performs no user page walk. */
    r.owned=true;r.count=2;mapping_checks=0;
    assert(tty_read_eligible(&r)==0 && !mapping_checks);tty_read_commit(&r,0);assert(output[2]==0 && r.count==3);
    /* Zero-length and later unmapped iovs do not affect the current byte. */
    struct srv_iovec iov[3]={{.base=(void *)1,.length=0},{.base=output,.length=8},{.base=(void *)1,.length=MAX_SYSCALL_COPY}};
    r=(struct tty_checked_read){.guard={.process=(struct process *)1,.fd=0,.generation=7},
        .length=MAX_SYSCALL_COPY,.count=5,.iov=iov,.iov_count=3};
    allowed_address=(uint64_t)(uintptr_t)(output+5);mapping_checks=0;
    assert(tty_read_eligible(&r)==0 && mapping_checks==1 && last_length==1);
    tty_read_commit(&r,'V');assert(output[5]=='V' && r.count==6);
    r.count=8;assert(tty_read_eligible(&r)==SRV_ERR_FAULT && last_address==1 && last_length==1);
    r.iov_count=SRV_IOV_MAX+1;mapping_checks=0;assert(tty_read_eligible(&r)==SRV_ERR_FAULT && !mapping_checks);
    r.iov_count=3;r.count=r.length;assert(tty_read_eligible(&r)==SRV_ERR_FAULT && !mapping_checks);
    r.iov=NULL;r.buffer=(char *)(uintptr_t)(UINT64_MAX-1);r.count=2;
    assert(tty_read_eligible(&r)==SRV_ERR_FAULT && !mapping_checks);
    puts("signal-tty: bounded current-byte mapping and commit PASS");return 0;
}
