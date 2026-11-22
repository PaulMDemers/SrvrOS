#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
static bool irq_enabled = true;
static bool pending, arm;
static unsigned char *region, *victim;
static uint64_t syscall_copy_test_irq_save(void) { bool was=irq_enabled; irq_enabled=false; return was; }
static void syscall_copy_test_irq_restore(uint64_t flags) {
    irq_enabled=(bool)flags;
    if (irq_enabled && pending) { assert(!mprotect(victim,4096,PROT_NONE)); pending=false; }
}
#define SRVROS_SYSCALL_COPY_HOST_TEST
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool write) {
    (void)write;
    uintptr_t first=(uintptr_t)region, end=first+8192;
    if (address<first || address>end || length>end-address) return false;
    if (victim && address >= (uintptr_t)victim) return false;
    if (arm) {
        arm=false;
        if (irq_enabled) {
            /* A sibling invalidates the very page just validated. Old helpers
             * immediately fault; the corrected helpers have IRQs masked. */
            assert(!mprotect(region,4096,PROT_NONE));
        } else pending=true;
    }
    return true;
}
static struct process_file file;
static unsigned transfers;
struct process *process_current(void) { return (struct process *)1; }
uint64_t process_pid(const struct process *p) { assert(p == (struct process *)1); return 7; }
struct process_file *process_file_at(struct process *p,uint64_t fd) {
    assert(p == (struct process *)1); return fd == 3 ? &file : NULL;
}
static int64_t file_transfer(struct process *p,uint64_t fd,uint64_t n) {
    assert(p == (struct process *)1 && fd == 3 && !irq_enabled); transfers++; return n;
}
int64_t process_file_read(struct process *p,uint64_t fd,uint8_t *data,uint64_t n) {
    (void)data; return file_transfer(p,fd,n);
}
int64_t process_file_write(struct process *p,uint64_t fd,const uint8_t *data,uint64_t n) {
    (void)data; return file_transfer(p,fd,n);
}
int64_t process_file_pread(struct process *p,uint64_t fd,uint8_t *data,uint64_t n,int64_t offset) {
    (void)data; assert(offset==0); return file_transfer(p,fd,n);
}
int64_t process_file_pwrite(struct process *p,uint64_t fd,const uint8_t *data,uint64_t n,int64_t offset) {
    (void)data; assert(offset==0); return file_transfer(p,fd,n);
}
int64_t audio_stream_request(uint64_t pid,unsigned op,uint64_t handle,void *data,uint64_t n) {
    (void)data; assert(pid==7 && op==SRV_PCM_WRITE && handle==3 && !irq_enabled); transfers++;return n;
}
/* Exercise the real status prelude/copy with the same IRQ instrumentation:
 * no callback/allocator/gate is permitted in the proved empty-stack case. */
bool net_status_info_try_without_device(struct srv_net_status_info *out) {
    assert(!irq_enabled);
    *out=(struct srv_net_status_info){.abi_version=SRV_NET_ABI_VERSION,.struct_size=sizeof(*out),.rx_frames=17};return true;
}
int64_t net_status_info(struct srv_net_status_info *out) {(void)out;assert(0);return -1;}
void *kmalloc(uint64_t n) {(void)n;assert(0);return NULL;}
void kfree(void *p) {(void)p;assert(0);}
bool scheduler_io_active(void) {assert(0);return false;}
void scheduler_io_set_user_safe(bool value) {(void)value;assert(0);}
void scheduler_io_wait(void) {assert(0);}
uint64_t latency_clock(void) {assert(0);return 0;}
void latency_gate(uint64_t at,uint64_t call) {(void)at;(void)call;assert(0);}
bool process_should_exit_current(void) {assert(0);return false;}
bool process_directory_try_read_info(uint64_t fd,void *out,uint64_t cap,int64_t *result) {
    assert(!irq_enabled && fd==3 && cap==sizeof(struct srv_dirent_info));
    *(struct srv_dirent_info *)out=(struct srv_dirent_info){.size=123};*result=1;return true;
}
int64_t process_directory(uint64_t op,uint64_t fd,void *out,uint64_t cap) {
    (void)op;(void)fd;(void)out;(void)cap;assert(0);return -1;
}
static void reset(void) {
    assert(!mprotect(region,8192,PROT_READ|PROT_WRITE));
    memset(region,'x',8192); victim=region+4096; pending=false; arm=false; irq_enabled=true;
}
int main(void) {
    region=mmap(NULL,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(region!=MAP_FAILED);
    reset();arm=true;
    int64_t directory_result;
    assert(syscall_directory_try_read_info(3,region,sizeof(struct srv_dirent_info),&directory_result) && directory_result==1);
    assert(((struct srv_dirent_info *)region)->size==123 && irq_enabled);
    reset();victim=region;
    assert(syscall_directory_try_read_info(3,region,sizeof(struct srv_dirent_info),&directory_result) && directory_result==SRV_ERR_FAULT);
    reset();
    struct srv_net_status_info *status=(void *)region;
    status->abi_version=SRV_NET_ABI_VERSION;status->struct_size=sizeof(*status);
    assert(syscall_net_status_info(status)==0 && status->rx_frames==17 && irq_enabled);
    /* The negotiated short copy may not overwrite the caller's tail. */
    status->struct_size=sizeof(struct srv_abi_header);region[sizeof(struct srv_abi_header)]=0x5a;
    assert(syscall_net_status_info(status)==0 && region[sizeof(struct srv_abi_header)]==0x5a && irq_enabled);
    status->abi_version=0;assert(syscall_net_status_info(status)==-1 && irq_enabled);
    status->abi_version=SRV_NET_ABI_VERSION;status->struct_size=sizeof(*status);victim=region;
    assert(syscall_net_status_info(status)==-1 && irq_enabled);victim=NULL;
    irq_enabled=false;assert(syscall_net_status_info(status)==0 && !irq_enabled);
    reset();
    uint8_t out[8]={0}, src[8]={1,2,3,4,5,6,7,8};
    reset(); victim=NULL; assert(copy_from_user(out,region,8));
    assert(!memcmp(out,"xxxxxxxx",8) && irq_enabled);
    assert(copy_to_user(region,src,8) && !memcmp(region,src,8));
    assert(copy_from_user(NULL,NULL,0) && copy_to_user(NULL,NULL,0));
    assert(!copy_from_user(out,(void *)(uintptr_t)(UINT64_MAX-2),8));
    reset(); arm=true; memset(out,0,sizeof(out));
    assert(!copy_from_user(out,region+4092,8));
    assert(!memcmp(out,"xxxx",4) && out[4]==0 && irq_enabled && !pending);
    reset(); arm=true;
    assert(!copy_to_user(region+4092,src,8));
    assert(!memcmp(region+4092,src,4) && irq_enabled && !pending);
    reset(); arm=true; char text[4]={0};
    assert(!copy_user_string((char *)region+4095,text,sizeof(text)));
    assert(text[0]=='x' && text[1]==0 && irq_enabled && !pending);
    reset(); irq_enabled=false; victim=NULL;
    assert(copy_from_user(out,region,8) && !irq_enabled);
    reset(); victim=NULL; file.type=PROCESS_FILE_VFS_WRITE;
    for(unsigned writing=0;writing<2;writing++) for(unsigned positional=0;positional<2;positional++) {
        unsigned before=transfers;
        assert(regular_user_io(3,region,4,writing,positional,0)==4 && transfers==before+1 && irq_enabled);
        victim=region;
        assert(regular_user_io(3,region,4,writing,positional,0)==SRV_ERR_FAULT && transfers==before+1 && irq_enabled);
        victim=NULL;
    }
    unsigned before=transfers;
    assert(syscall_audio_stream(SRV_PCM_WRITE,3,region,4)==4 && transfers==before+1 && irq_enabled);
    victim=region;
    assert(syscall_audio_stream(SRV_PCM_WRITE,3,region,4)<0 && transfers==before+1 && irq_enabled);
    assert(!munmap(region,8192));
    puts("syscall-user-copy: PASS guarded validation/access, invalidation, bounded pages and IRQ restoration");
}
