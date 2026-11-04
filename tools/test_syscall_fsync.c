/* File fsync must not depend on an unrelated failed block device. */
#include <assert.h>
#include <stdio.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct process_file descriptor={.used=true,.type=PROCESS_FILE_VFS_WRITE};
static int64_t file_result,global_result;
static unsigned file_calls,barriers;
struct process *process_current(void){return NULL;}
struct process_file *process_file_at(struct process *p,uint64_t fd){(void)p;return fd==3?&descriptor:NULL;}
int64_t process_file_flush(struct process *p,uint64_t fd){(void)p;assert(fd==3);file_calls++;return file_result;}
int64_t process_file_sync_all(void){return global_result;}
bool block_flush_all(void){barriers++;return false;}
int main(void) {
    assert(syscall_fsync(3)==0 && file_calls==1 && !barriers);
    descriptor.type=PROCESS_FILE_VFS;assert(syscall_fsync(3)==0 && !barriers);
    file_result=SRV_ERR_IO;assert(syscall_fsync(3)==SRV_ERR_IO && !barriers);
    file_result=-1;assert(syscall_fsync(3)==SRV_ERR_IO && !barriers);
    unsigned before=file_calls;assert(syscall_fsync(9)==SRV_ERR_BADF && file_calls==before);
    descriptor.type=PROCESS_FILE_PIPE_READ;assert(syscall_fsync(3)==SRV_ERR_INVAL && file_calls==before);
    global_result=0;assert(syscall_sync()<0 && barriers==1);
    global_result=-1;assert(syscall_sync()<0 && barriers==2);
    puts("file-fsync: PASS per-description errors and no unrelated barrier; global sync still fails");
}
