#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct srv_file_stream_request q;
static struct srv_file_stream_stat out;
static unsigned char bytes[65537];
static char path[]="/fat/stream";
static unsigned calls,copy_calls;
static int deny_copy;
static size_t mapped_bytes=sizeof(bytes);
static int64_t admitted_override=INT64_MIN;
static bool path_busy;
static unsigned lookups,reads,chmods,utimes,allocations,revocations;
static struct vfs_node node={.type=VFS_NODE_FILE};
static const uint8_t file_data[]={1,2,3};
bool exfat_stream_busy(const char *p,bool subtree){assert(!strcmp(p,path) && !subtree);return path_busy;}
const struct vfs_node *vfs_lookup(const char *p){assert(!strcmp(p,path));lookups++;return &node;}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *size){assert(n==&node);reads++;*data=file_data;*size=sizeof(file_data);return true;}
void process_revoke_reserved_append_path(const char *p){assert(!strcmp(p,path) && !path_busy && lookups==2 && !chmods);revocations++;}
bool vfs_chmod(const char *p,uint64_t mode){assert(!strcmp(p,path) && mode==0600 && revocations==1);chmods++;return true;}
int64_t process_path_utime(const char *p,uint64_t a,uint64_t m){assert(!strcmp(p,path) && a==11 && m==12);utimes++;return 0;}
int64_t process_file_alloc(struct process *p,const struct vfs_node *n,const uint8_t *data,uint64_t size){(void)p;assert(n==&node && data==file_data && size==sizeof(file_data));allocations++;return 9;}
static int64_t backend_result;
bool vmm_user_range_mapped(uint64_t a,uint64_t n,bool w){
    (void)w;if(n>UINT64_MAX-a)return false;
    if(deny_copy && a>=(uint64_t)(uintptr_t)bytes && a<(uint64_t)(uintptr_t)(bytes+sizeof(bytes)) && ++copy_calls>1)return false;
    uint64_t starts[]={(uint64_t)(uintptr_t)&q,(uint64_t)(uintptr_t)&out,(uint64_t)(uintptr_t)bytes,(uint64_t)(uintptr_t)path};
    uint64_t sizes[]={sizeof(q),sizeof(out),mapped_bytes,sizeof(path)};
    for(unsigned i=0;i<4;i++)if(a>=starts[i] && a-starts[i]<=sizes[i] && n<=sizes[i]-(a-starts[i]))return true;
    return false;
}
void *kmalloc(size_t n){return malloc((size_t)n);} void kfree(void *p){free(p);}
struct process *process_current(void){return (struct process *)(uintptr_t)1;}
uint64_t process_stream_identity(struct process *p){(void)p;return 7;}
uint64_t process_creation_mode(uint64_t m){return m&~0022u;}
int64_t process_path_context(uint64_t op,const char *p,char *outp,uint64_t n){(void)op;if(strlen(p)>=n)return SRV_ERR_INVAL;strcpy(outp,p);return 0;}
int64_t process_resolve_storage_path(const char *p,char *outp,uint64_t n){if(strlen(p)>=n)return SRV_ERR_INVAL;strcpy(outp,p);return 0;}
int64_t vfs_namespace_system_path(const char *p,char *outp,size_t n){if(strlen(p)>=n)return SRV_ERR_INVAL;strcpy(outp,p);return 0;}
int64_t file_stream_open(uint64_t o,const char *p,bool create,uint64_t mode){assert(o==7 && !strcmp(p,path));if(create)assert(mode==0644);calls++;return 42;}
int64_t file_stream_pread(uint64_t o,uint64_t h,void *b,uint64_t n,uint64_t offset){(void)o;(void)h;(void)offset;calls++;if(b)memset(b,0x5a,n);return backend_result?backend_result:(int64_t)n;}
int64_t file_stream_append_count(uint64_t o,uint64_t h,uint64_t n){(void)o;(void)h;return admitted_override==INT64_MIN?(int64_t)n:admitted_override;}
int64_t file_stream_append(uint64_t o,uint64_t h,const void *b,uint64_t n){(void)o;(void)h;calls++;if(n)assert(b && ((const unsigned char *)b)[0]==0x39);return backend_result?backend_result:(int64_t)n;}
int64_t file_stream_stat(uint64_t o,uint64_t h,struct srv_file_stream_stat *s){(void)o;(void)h;calls++;memset(s,0,sizeof(*s));s->size=123;return backend_result;}
int64_t file_stream_sync(uint64_t o,uint64_t h){(void)o;(void)h;calls++;return backend_result;}
int64_t file_stream_close(uint64_t o,uint64_t h){(void)o;(void)h;calls++;return backend_result;}
struct process_file *process_file_at(struct process *p,uint64_t fd){(void)p;(void)fd;return NULL;}
bool process_fd_is_console(struct process *p,uint64_t fd,bool input){(void)p;(void)fd;(void)input;return false;}
bool process_file_nonblocking(struct process *p,uint64_t fd){(void)p;(void)fd;return false;}
static void reset(unsigned op){memset(&q,0,sizeof(q));q.version=1;q.size=sizeof(q);q.operation=op;calls=copy_calls=0;deny_copy=0;backend_result=0;admitted_override=INT64_MIN;mapped_bytes=sizeof(bytes);}
int main(void){
    assert(syscall_file_stream((void *)(uintptr_t)1)==SRV_ERR_FAULT);
    reset(SRV_FILE_STREAM_OPEN_READ);q.path=(uint64_t)(uintptr_t)path;
    assert(syscall_file_stream(&q)==42 && calls==1);
    q.version=2;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.version=1;q.size--;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.size++;
    q.flags=1;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.flags=0;q.reserved[1]=1;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.reserved[1]=0;
    q.mode=1;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.mode=0;q.handle=1;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);
    reset(SRV_FILE_STREAM_CREATE_APPEND);q.path=(uint64_t)(uintptr_t)path;q.mode=0666;assert(syscall_file_stream(&q)==42);
    reset(SRV_FILE_STREAM_APPEND);q.handle=42;q.buffer=(uint64_t)(uintptr_t)bytes;q.length=65536;bytes[0]=0x39;
    assert(syscall_file_stream(&q)==65536 && calls==1);
    q.length=65537;assert(syscall_file_stream(&q)==SRV_ERR_INVAL && calls==1);
    q.length=4;q.buffer=1;assert(syscall_file_stream(&q)==SRV_ERR_FAULT && calls==1);
    q.buffer=(uint64_t)(uintptr_t)bytes;deny_copy=1;copy_calls=0;assert(syscall_file_stream(&q)==SRV_ERR_FAULT && calls==1);
    deny_copy=0;q.length=0;q.buffer=0;assert(syscall_file_stream(&q)==0 && calls==1);
    q.length=4;q.buffer=(uint64_t)(uintptr_t)bytes;mapped_bytes=1;admitted_override=1;
    assert(syscall_file_stream(&q)==1 && calls==2);
    admitted_override=SRV_ERR_FBIG;q.buffer=1;assert(syscall_file_stream(&q)==SRV_ERR_FBIG && calls==2);
    admitted_override=5;assert(syscall_file_stream(&q)==SRV_ERR_IO && calls==2);
    reset(SRV_FILE_STREAM_PREAD);q.handle=42;q.buffer=(uint64_t)(uintptr_t)bytes;q.length=65536;
    assert(syscall_file_stream(&q)==65536 && bytes[0]==0x5a && bytes[65535]==0x5a);
    q.offset=UINT64_MAX;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.offset=0;backend_result=65537;assert(syscall_file_stream(&q)==SRV_ERR_IO);
    backend_result=0;q.length=1;deny_copy=1;copy_calls=0;assert(syscall_file_stream(&q)==SRV_ERR_FAULT);
    reset(SRV_FILE_STREAM_STAT);q.handle=42;q.buffer=(uint64_t)(uintptr_t)&out;q.length=sizeof(out);assert(syscall_file_stream(&q)==0 && out.size==123);
    q.length--;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);q.length++;q.buffer=1;assert(syscall_file_stream(&q)==SRV_ERR_FAULT);
    reset(SRV_FILE_STREAM_SYNC);q.handle=42;backend_result=SRV_ERR_IO;assert(syscall_file_stream(&q)==SRV_ERR_IO);q.buffer=1;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);
    reset(SRV_FILE_STREAM_CLOSE);q.handle=42;assert(syscall_file_stream(&q)==0);q.operation=999;assert(syscall_file_stream(&q)==SRV_ERR_INVAL);
    path_busy=true;
    assert(syscall_open(path)==SRV_ERR_BUSY);
    assert(syscall_chmod(path,0600)==SRV_ERR_BUSY);
    assert(syscall_utime(path,11,12)==SRV_ERR_BUSY);
    assert(!lookups && !reads && !allocations && !chmods && !utimes && !revocations);
    path_busy=false;
    assert(syscall_open(path)==9 && lookups==1 && reads==1 && allocations==1);
    assert(syscall_chmod(path,0600)==0 && lookups==2 && chmods==1 && revocations==1);
    assert(syscall_utime(path,11,12)==0 && utimes==1);
    struct isr_frame frame={.rax=SYS_FILE_STREAM};assert(namespace_syscall(SYS_FILE_STREAM) && !syscall_io_safe(&frame));
    puts("file stream syscall: PASS");return 0;
}
