/* Exercises the production SYS_LIST body and syscall admission classification.
 * The namespace iterator is deliberately mocked so this test isolates the ABI
 * from traversal order and the independently tested namespace implementation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"

static char long_path[SRV_PATH_MAX];
static char user_path[SRV_PATH_MAX];
static uint64_t user_size;
static unsigned iterator_calls;
static struct vfs_node raw_node={.path=long_path,.size=27,.used=true,.type=VFS_NODE_FILE};
static struct vfs_node projected_node={.path="/fat/bin/tool",.size=40,.used=true,.type=VFS_NODE_FILE};

/* Keep the fake mapping boundary opaque, as it is in the separately linked
 * kernel. This fixture intentionally refuses every readable user range. */
__attribute__((noinline)) bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    uint64_t first=(uint64_t)(uintptr_t)user_path;
    uint64_t scalar=(uint64_t)(uintptr_t)&user_size;
    if(!writable || length>UINT64_MAX-address) return false;
    return (address>=first && address+length<=first+sizeof(user_path)) ||
        (address>=scalar && address+length<=scalar+sizeof(user_size));
}
int vfs_namespace_system_list(uint64_t index,char *out,size_t capacity,const struct vfs_node **node) {
    iterator_calls++;
    const char *path;
    if(index==0) { path=long_path;*node=&raw_node; }
    else if(index==1) { path="/usr/bin/tool";*node=&projected_node; }
    else { *node=NULL;return 0; }
    size_t length=strlen(path);
    if(length>=capacity) { *node=NULL;return -1; }
    memcpy(out,path,length+1);return 1;
}
bool process_open_file_size(const char *path,uint64_t *size) {
    if(!strcmp(path,"/fat/bin/tool")) { *size=91;return true; }
    assert(!strcmp(path,long_path));return false;
}
struct process *process_current(void) { return NULL; }
struct process_file *process_file_at(struct process *p,uint64_t fd) { (void)p;(void)fd;return NULL; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) { (void)p;(void)fd;(void)input;return false; }
bool process_file_nonblocking(struct process *p,uint64_t fd) { (void)p;(void)fd;return false; }

int main(void) {
    long_path[0]='/';memset(long_path+1,'x',sizeof(long_path)-2);long_path[sizeof(long_path)-1]=0;
    memset(user_path,0,sizeof(user_path));user_size=0;
    assert(syscall_list(0,user_path,sizeof(user_path),&user_size)==1);
    assert(!strcmp(user_path,long_path) && user_size==27 && strlen(user_path)==SRV_PATH_MAX-1);
    memset(user_path,'z',sizeof(user_path));user_size=0;
    assert(syscall_list(0,user_path,1,&user_size)==1 && !user_path[0] && user_path[1]=='z' && user_size==27);
    memset(user_path,'z',sizeof(user_path));
    assert(syscall_list(0,user_path,8,&user_size)==1 && !memcmp(user_path,long_path,7) && !user_path[7] && user_path[8]=='z');
    assert(syscall_list(1,user_path,sizeof(user_path),&user_size)==1 && !strcmp(user_path,"/usr/bin/tool") && user_size==91);
    assert(syscall_list(2,user_path,sizeof(user_path),&user_size)==0);
    unsigned before=iterator_calls;
    assert(syscall_list(2,(char *)(uintptr_t)1,8,&user_size)==SRV_ERR_FAULT && iterator_calls==before);
    assert(syscall_list(2,user_path,sizeof(user_path),(uint64_t *)(uintptr_t)1)==SRV_ERR_FAULT && iterator_calls==before);
    assert(syscall_list(2,user_path,0,&user_size)==SRV_ERR_FAULT && iterator_calls==before);

    static const uint64_t selected[]={SYS_OPEN,SYS_OPEN_MODE,SYS_LIST,SYS_FS_WRITE,SYS_FS_APPEND,SYS_STAT,SYS_STATFS,
        SYS_CHMOD,SYS_UTIME,SYS_UNLINK,SYS_MKDIR,SYS_MKDIR_MODE,SYS_RMDIR,SYS_RENAME,SYS_FILE_CHANGE,SYS_PATH_CONTEXT,SYS_DIRECTORY};
    for(unsigned i=0;i<sizeof(selected)/sizeof(*selected);i++) {
        struct isr_frame frame={.rax=selected[i]};
        assert(namespace_syscall(selected[i]) && !syscall_io_safe(&frame));
    }
    static const uint64_t unselected[]={SYS_EXEC,SYS_SPAWN,SYS_FORK,SYS_CLOSE,SYS_READ,SYS_WRITE,SYS_POLL};
    for(unsigned i=0;i<sizeof(unselected)/sizeof(*unselected);i++) assert(!namespace_syscall(unselected[i]));
    struct isr_frame prelude={.rax=SYS_POLL};assert(syscall_io_safe(&prelude));
    puts("syscall-namespace: PASS list bounds, validation, projected identity and syscall admission");
    return 0;
}
