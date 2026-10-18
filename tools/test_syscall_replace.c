#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static int allocation_failure,write_failure,calls,allocations,frees;
static const void *invalid;
static unsigned char saved[32];static size_t saved_length;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    (void)length;(void)writable;return address && (const void *)address!=invalid;
}
void *kmalloc(uint64_t length) { if(allocation_failure) return NULL;allocations++;return malloc(length); }
void kfree(void *p) { if(p) frees++;free(p); }
bool exfat_write_file(const char *path,const uint8_t *data,uint64_t length) {
    assert(!strcmp(path,"/fat/test") && length<=sizeof(saved));calls++;
    if(write_failure) return false;
    if(length) memcpy(saved,data,length);
    saved_length=length;return true;
}
int main(void) {
    const char *path="/fat/test";unsigned char content[]={1,2,3,4};
    assert(syscall_fs_write(path,content,4)==4 && calls==1 && saved_length==4 && !memcmp(saved,content,4));
    allocation_failure=1;assert(syscall_fs_write(path,content,4)<0 && calls==1);allocation_failure=0;
    invalid=content;assert(syscall_fs_write(path,content,4)<0 && calls==1);invalid=NULL;
    assert(syscall_fs_write(NULL,content,4)<0 && calls==1);
    assert(syscall_fs_write(path,content,MAX_SYSCALL_COPY+1)<0 && calls==1);
    char long_path[160];memset(long_path,'a',sizeof(long_path));assert(syscall_fs_write(long_path,content,4)<0 && calls==1);
    write_failure=1;assert(syscall_fs_write(path,content,2)<0 && saved_length==4);write_failure=0;
    assert(syscall_fs_write(path,NULL,0)==0 && saved_length==0 && calls==3);
    assert(allocations==frees);
    puts("syscall-replace: PASS single publication, copied snapshot, empty file, invalid/oversize buffer, allocation and filesystem failures");
}
