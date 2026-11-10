#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
#include <srvros/file_verify.h>

/* Actual syscall user-copy/namespace prelude and verification backend, with only
 * media and user mappings substituted. Real process/cache/exFAT composition is
 * covered by test-capture-media-integration. No ordinary FD is allocated. */
static struct srv_file_verify_request q;
static char path[96];
static const char storage_path[]="/vol/Screenshots/proof.bmp";
static unsigned char *expected;
static struct vfs_node node;
static unsigned lookups,reads,releases,copies,path_resolutions;
static uint64_t mapped_length,revoke_offset;
static int read_failure,read_null,read_size_delta,mutate_metadata,mutate_request;
static uint64_t corrupt_offset;
static bool missing,deny_request,deny_path,stream_busy,mutate_during_copy;
static const uint8_t *outstanding;

static bool range_contains(uint64_t base,uint64_t size,uint64_t address,uint64_t length) {
    return address>=base && address-base<=size && length<=size-(address-base);
}
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(!writable);
    if(length>UINT64_MAX-address)return false;
    if(!deny_request && range_contains((uintptr_t)&q,sizeof(q),address,length))return true;
    if(!deny_path && range_contains((uintptr_t)path,sizeof(path),address,length))return true;
    if(range_contains((uintptr_t)expected,mapped_length,address,length)) {
        copies++;
        if(reads && mutate_during_copy) {node.metadata.mtime++;mutate_during_copy=false;}
        if(reads && address-(uintptr_t)expected>=revoke_offset)return false;
        return true;
    }
    return false;
}
int64_t process_resolve_storage_path(const char *input,char *out,uint64_t capacity) {
    assert(!strcmp(input,"/Home/Screenshots/proof.bmp"));
    path_resolutions++;
    if(sizeof(storage_path)>capacity)return SRV_ERR_NAMETOOLONG;
    strcpy(out,storage_path);return 0;
}
int64_t process_path_context(uint64_t operation,const char *input,char *out,uint64_t capacity) {
    (void)operation;(void)input;(void)out;(void)capacity;assert(0);return SRV_ERR_INVAL;
}
int64_t vfs_namespace_system_path(const char *input,char *out,size_t capacity) {
    (void)input;(void)out;(void)capacity;assert(0);return SRV_ERR_INVAL;
}
bool exfat_stream_busy(const char *input,bool subtree) {
    assert(!strcmp(input,storage_path) && !subtree);return stream_busy;
}
const struct vfs_node *vfs_lookup(const char *input) {
    assert(!strcmp(input,storage_path));lookups++;return missing?NULL:&node;
}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *length) {
    assert(n==&node && !outstanding);reads++;
    uint8_t *snapshot=read_null?NULL:malloc(node.size?node.size:1);assert(read_null || snapshot);
    if(snapshot && node.size)memcpy(snapshot,expected,node.size);
    if(snapshot && corrupt_offset<node.size)snapshot[corrupt_offset]^=0x80;
    outstanding=snapshot;*data=snapshot;
    *length=read_size_delta<0?node.size-1:node.size+(unsigned)read_size_delta;
    if(mutate_metadata==1)node.metadata.inode++;
    if(mutate_metadata==2)node.metadata.mtime++;
    if(mutate_metadata==3)node.size++;
    if(mutate_metadata==4)node.type=VFS_NODE_DIRECTORY;
    if(mutate_request) {
        q.data=(const void *)(uintptr_t)1;q.length=UINT64_MAX;q.expected_inode++;
        memset(path,'x',sizeof(path));
    }
    return !read_failure;
}
void vfs_release_data(const struct vfs_node *n,const uint8_t *data) {
    assert(n==&node && data==outstanding);releases++;
    free((void *)data);outstanding=NULL;
}
/* Calling any descriptor/cache API would defeat the fresh-media contract. */
struct process *process_current(void) { return (struct process *)(uintptr_t)1; }
struct process_file *process_file_at(struct process *p,uint64_t fd) { (void)p;(void)fd;assert(0);return NULL; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) { (void)p;(void)fd;(void)input;assert(0);return false; }
bool process_file_nonblocking(struct process *p,uint64_t fd) { (void)p;(void)fd;assert(0);return false; }
int64_t process_file_alloc(struct process *p,const struct vfs_node *n,const uint8_t *data,uint64_t size) { (void)p;(void)n;(void)data;(void)size;assert(0);return -1; }

static void reset(uint64_t length) {
    assert(!outstanding && length<=SRV_FILE_VERIFY_MAX);
    strcpy(path,"/Home/Screenshots/proof.bmp");
    node=(struct vfs_node){.used=true,.path=storage_path,.type=VFS_NODE_FILE,.size=length,.metadata={.inode=71,.mtime=123}};
    q=(struct srv_file_verify_request){.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(q),.path=path,.data=expected,.length=length,.expected_inode=71,.expected_mtime=123};
    lookups=reads=releases=copies=path_resolutions=0;
    mapped_length=SRV_FILE_VERIFY_MAX;revoke_offset=corrupt_offset=UINT64_MAX;
    read_failure=read_null=read_size_delta=mutate_metadata=mutate_request=0;
    missing=deny_request=deny_path=stream_busy=mutate_during_copy=false;
}
static void no_read_failure(void) {
    assert(syscall_file_verify(&q)<0 && !reads && !releases && !outstanding);
}
static void read_failure_released(void) {
    assert(syscall_file_verify(&q)<0 && reads==1 && releases==1 && !outstanding);
}
int main(void) {
    _Static_assert(sizeof(struct srv_file_verify_request)==64,"fixed fresh-media ABI");
    _Static_assert(MAX_SYSCALL_COPY==256*1024,"ordinary copy bound is unchanged");
    _Static_assert(SRV_FILE_VERIFY_CHUNK==4096,"bounded comparison copy");
    expected=malloc(SRV_FILE_VERIFY_MAX);assert(expected);
    for(uint64_t i=0;i<SRV_FILE_VERIFY_MAX;i++)expected[i]=(unsigned char)(i*37+i/97);

    reset(3);assert(!syscall_file_verify(&q) && reads==1 && releases==1 && path_resolutions==1 && !outstanding);
    reset(0);q.data=NULL;assert(!syscall_file_verify(&q) && reads==1 && releases==1);
    reset(SRV_FILE_VERIFY_CHUNK+17);assert(!syscall_file_verify(&q) && reads==1 && releases==1 && copies>1);
    reset(SRV_FILE_VERIFY_MAX);assert(!syscall_file_verify(&q) && reads==1 && releases==1 && copies>=SRV_FILE_VERIFY_MAX/4096);

    reset(3);deny_request=true;no_read_failure();
    reset(3);deny_path=true;no_read_failure();
    reset(3);q.path=NULL;no_read_failure();
    reset(3);q.abi_version++;no_read_failure();
    reset(3);q.struct_size--;no_read_failure();
    reset(3);q.struct_size++;no_read_failure();
    reset(3);q.reserved=1;no_read_failure();
    reset(3);q.length=SRV_FILE_VERIFY_MAX+1;no_read_failure();
    reset(3);q.data=NULL;no_read_failure();
    reset(3);q.data=(const void *)(uintptr_t)(UINT64_MAX-1);no_read_failure();
    reset(3);q.expected_inode=0;no_read_failure();
    reset(3);mapped_length=2;no_read_failure();
    reset(3);missing=true;no_read_failure();
    reset(3);stream_busy=true;no_read_failure();assert(!lookups);
    reset(3);node.type=VFS_NODE_DIRECTORY;no_read_failure();
    reset(3);node.metadata.inode++;no_read_failure();
    reset(3);node.metadata.mtime++;no_read_failure();
    reset(3);node.size++;no_read_failure();
    reset(3);node.size--;no_read_failure();

    reset(3);read_failure=1;read_failure_released();
    reset(3);read_null=1;read_failure_released();
    reset(3);read_size_delta=1;read_failure_released();
    reset(3);read_size_delta=-1;read_failure_released();
    reset(3);corrupt_offset=0;read_failure_released();
    reset(SRV_FILE_VERIFY_CHUNK+17);corrupt_offset=SRV_FILE_VERIFY_CHUNK;read_failure_released();
    reset(SRV_FILE_VERIFY_MAX);corrupt_offset=SRV_FILE_VERIFY_MAX-1;read_failure_released();
    for(int change=1;change<=4;change++) { reset(3);mutate_metadata=change;read_failure_released(); }
    reset(SRV_FILE_VERIFY_CHUNK*3);revoke_offset=SRV_FILE_VERIFY_CHUNK;read_failure_released();
    reset(3);mutate_during_copy=true;assert(syscall_file_verify(&q)==SRV_ERR_AGAIN && reads==1 && releases==1);
    reset(3);mutate_request=1;assert(!syscall_file_verify(&q) && reads==1 && releases==1 && path_resolutions==1);

    struct isr_frame frame={.rax=SYS_FILE_VERIFY};
    assert(namespace_syscall(SYS_FILE_VERIFY) && !syscall_io_safe(&frame));
    free(expected);
    puts("file-verify: PASS fresh full-media bytes, empty/8MiB bounds, copied request/path, namespace admission, no shared-cache allocation, stale metadata, corruption, user faults and every snapshot release");
    return 0;
}
