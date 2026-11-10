/* Exercise the production generic IF1 writeback return edge, not only raw
 * syscalls. The IRQ restore hook dispatches a queued last close immediately. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t irq_flags;
static void restored_dispatch(void);
static uint64_t process_test_irq_save(void) {uint64_t f=irq_flags;irq_flags=0;return f;}
static void process_test_irq_restore(uint64_t f) {irq_flags=f;if(f&0x200)restored_dispatch();}
static uint64_t process_test_rflags(void) {return irq_flags;}
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
#include "../kernel/src/process.c"
static struct process caller;
static bool dispatch_close,dispatching;
static unsigned backend_calls,close_turns,freed;
void *scheduler_current_user_context(void) {return &caller;}
void *scheduler_current_user_thread_context(void) {return NULL;}
void scheduler_wake_all(struct scheduler_wait_queue *q) {(void)q;}
void scheduler_yield(void) {assert(!"unexpected yield");}
bool scheduler_wait(struct scheduler_wait_queue *q,bool (*ready)(void *),void *arg) {(void)q;(void)ready;(void)arg;assert(0);return false;}
void *kmalloc(size_t n) {return malloc(n);}
void kfree(void *p) {assert(!file_contents[0].writeback.active);free(p);freed++;}
int64_t net_hold(uint64_t h) {(void)h;assert(0);return SRV_ERR_IO;}
bool pty_ref(uint64_t h,bool master) {(void)h;(void)master;assert(0);return false;}
int64_t net_close(uint64_t h) {(void)h;assert(0);return SRV_ERR_IO;}
void pty_close(uint64_t h,bool master) {(void)h;(void)master;assert(0);}
void vfs_release_data(const struct vfs_node *node,const uint8_t *data) {(void)node;(void)data;assert(0);}
bool exfat_write_file_retryable(const char *path,const uint8_t *data,uint64_t bytes,bool *retryable) {
    assert(!irq_flags && !strcmp(path,"/test/irq") && bytes==4 && !memcmp(data,"ABCD",4));
    assert(file_contents[0].writeback.active && file_contents[0].writeback.data==data && file_contents[0].writeback.size==bytes);
    backend_calls++;*retryable=false;
    /* The real fs_scope returns its incoming IF0. Restoring that state must
     * not dispatch the pending unsafe close before prefix retirement. */
    process_test_irq_restore(0);return true;
}
static void restored_dispatch(void) {
    if(!dispatch_close || dispatching)return;
    /* Any early IF1 restore is a regression, even if a defensive BUSY guard
     * would happen to avoid freeing the borrowed buffer. */
    assert(!file_contents[0].writeback.active && !file_contents[0].dirty && file_contents[0].committed_size==4);
    dispatching=true;close_turns++;
    assert(process_file_close(&caller,3)==0 && !caller.files[3].used && !write_files[0].used && !file_contents[0].refs);
    dispatching=false;dispatch_close=false;
}
static void setup(void) {
    memset(&caller,0,sizeof(caller));memset(file_contents,0,sizeof(file_contents));
    memset(write_files,0,sizeof(write_files));memset(read_files,0,sizeof(read_files));
    caller.allocated=caller.active=true;caller.image_generation=1;caller.pid=1;
    caller.files[3]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=1};
    uint8_t *data=malloc(4);assert(data);memcpy(data,"ABCD",4);
    file_contents[0]=(struct process_file_content){.refs=1,.data=data,.size=4,.capacity=4,.owned=true,.dirty=true,.dirty_generation=7};
    strcpy(file_contents[0].path,"/test/irq");
    write_files[0]=(struct process_write_file){.used=true,.refs=1,.open_flags=SRV_OPEN_WRITE,.content=&file_contents[0]};
}
int main(void) {
    setup();irq_flags=0x200;dispatch_close=true;
    assert(content_flush(&file_contents[0])==0 && irq_flags==0x200 && close_turns==1 && freed==1 && backend_calls==1);
    setup();irq_flags=0x200;file_contents[0].writeback=(__typeof__(file_contents[0].writeback)){
        .active=true,.data=file_contents[0].data,.size=4,.generation=7};
    caller.files[4]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=1};
    struct process_file fd=caller.files[3],source=caller.files[4];struct process_write_file description=write_files[0];
    struct process_file_content content=file_contents[0];uint64_t generation=caller.file_generations[3];
    assert(process_file_close(&caller,3)==SRV_ERR_BUSY && write_file_close(1)==SRV_ERR_BUSY);
    assert(process_file_dup2(&caller,4,3)==SRV_ERR_BUSY && process_file_dup3(&caller,4,3,SRV_FD_CLOEXEC)==SRV_ERR_BUSY);
    assert(process_file_dup2(&caller,3,3)==3 && process_file_dup3(&caller,3,3,0)==SRV_ERR_INVAL);
    assert(!memcmp(&fd,&caller.files[3],sizeof(fd)) && !memcmp(&description,&write_files[0],sizeof(description)) &&
        !memcmp(&content,&file_contents[0],sizeof(content)) && !memcmp(&source,&caller.files[4],sizeof(source)) && caller.file_generations[3]==generation && irq_flags==0x200);
    file_contents[0].writeback.active=false;assert(process_file_close(&caller,3)==0 && freed==2);
    setup();irq_flags=0;assert(content_flush(&file_contents[0])==0 && !irq_flags && !file_contents[0].writeback.active);
    assert(process_file_close(&caller,3)==0 && !irq_flags && freed==3);
    puts("reserved-append-irq: PASS generic IF1 borrow closure, queued last-close only after retirement, BUSY FD/ref/generation preservation and original IF0 restoration");
    return 0;
}
