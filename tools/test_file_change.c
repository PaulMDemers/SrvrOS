#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static struct vfs_node node={.used=true,.path="/fat/old",.type=VFS_NODE_FILE,.size=3,.metadata={.inode=71,.mtime=123}};
static int exists,busy,fail,calls,operation;static const void *invalid;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) { (void)length;(void)writable;return address && (const void *)address!=invalid; }
void *kmalloc(uint64_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
const struct vfs_node *vfs_lookup(const char *path) { if(!strcmp(path,"/fat/old")) return &node;if(!strcmp(path,"/fat/new") && exists) return &node;return NULL; }
bool process_has_open_vfs_prefix(const char *path) { (void)path;return busy; }
bool exfat_rename(const char *old,const char *next) { assert(!strcmp(old,"/fat/old") && !strcmp(next,"/fat/new"));calls++;operation=1;return !fail; }
bool exfat_delete_file(const char *path) { assert(!strcmp(path,"/fat/old"));calls++;operation=2;return !fail; }
bool exfat_delete_directory(const char *path) { assert(!strcmp(path,"/fat/old"));calls++;operation=4;return !fail; }
bool exfat_write_file(const char *path,const uint8_t *data,uint64_t length) { assert(!strcmp(path,"/fat/new") && length==3 && !memcmp(data,"abc",3));calls++;operation=3;return !fail; }
int main(void) {
    struct srv_file_change q={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(q),.operation=SRV_FILE_RENAME,.expected_inode=71,.expected_size=3,.expected_mtime=123,.path="/fat/old",.destination="/fat/new"};
    assert(!syscall_file_change(&q) && calls==1 && operation==1);
    q.expected_inode++;assert(syscall_file_change(&q)==SRV_FILE_CHANGED && calls==1);q.expected_inode--;
    q.expected_size++;assert(syscall_file_change(&q)==SRV_FILE_CHANGED);q.expected_size--;
    q.expected_mtime++;assert(syscall_file_change(&q)==SRV_FILE_CHANGED);q.expected_mtime--;
    exists=1;assert(syscall_file_change(&q)==SRV_FILE_EXISTS && calls==1);exists=0;
    busy=1;assert(syscall_file_change(&q)==SRV_FILE_BUSY && calls==1);busy=0;
    q.operation=SRV_FILE_DELETE;assert(!syscall_file_change(&q) && operation==2);
    node.type=VFS_NODE_DIRECTORY;assert(syscall_file_change(&q)==SRV_FILE_CHANGED);q.expected_type=VFS_NODE_DIRECTORY;
    assert(!syscall_file_change(&q) && operation==4);q.operation=SRV_FILE_COPY;assert(syscall_file_change(&q)==SRV_FILE_LIMIT);
    node.type=VFS_NODE_FILE;q.expected_type=VFS_NODE_FILE;q.length=3;q.data="abc";assert(!syscall_file_change(&q) && operation==3);
    int previous=calls;invalid=q.data;assert(syscall_file_change(&q)<0 && calls==previous);invalid=NULL;
    q.length=MAX_SYSCALL_COPY+1;assert(syscall_file_change(&q)==SRV_FILE_LIMIT);q.length=3;
    fail=1;assert(syscall_file_change(&q)<0);q.abi_version=99;assert(syscall_file_change(&q)==SRV_FILE_INVALID);
    q.abi_version=SRV_ABI_VERSION;q.operation=99;assert(syscall_file_change(&q)==SRV_FILE_INVALID);
    puts("file-change: PASS copied ABI, stale identity/size/date/type, destination collision, open-file gate, rename/delete/directory/copy and bounds");
}
