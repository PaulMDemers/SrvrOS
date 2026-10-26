#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYSCALL_POLL_HOST_TEST
#define SRVROS_SERVICE_WINDOW_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static unsigned depth,checkpoints,allocations,calls;
static bool foreign,fail_alloc,deny_after_first,fail_surface;
static uint64_t address_space=0x2000;
uint64_t service_window_test_irq_save(void) {return 0;}
void service_window_test_irq_restore(uint64_t flags) {(void)flags;}
bool scheduler_io_begin(void) {if(foreign)return false;depth++;return true;}
void scheduler_io_end(void) {assert(depth && !allocations);depth--;}
void scheduler_io_checkpoint(void) {assert(depth && address_space==0x2000);checkpoints++;}
bool vmm_user_range_mapped(uint64_t p,uint64_t bytes,bool writable) {
    (void)writable;return p && bytes && !(deny_after_first && checkpoints);
}
void *kmalloc(size_t bytes) {if(fail_alloc)return NULL;void *p=malloc(bytes);assert(p);allocations++;return p;}
void kfree(void *p) {if(p){assert(allocations && (depth||foreign));allocations--;free(p);}}
struct process *process_current(void) {return (struct process *)1;}
uint64_t process_pid(const struct process *p) {assert(p==(struct process *)1);return 7;}
uint64_t vmm_current_address_space(void) {return address_space;}
uint64_t vmm_kernel_address_space(void) {return 0x1000;}
void vmm_switch_address_space(uint64_t space) {assert(space==0x1000 || space==0x2000);address_space=space;}
bool console_blit_rgb_scaled(uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *pixels,uint64_t stride,uint64_t scale) {
    (void)x;(void)y;assert(address_space==0x1000 && w*h*4<=65536 && stride==w && scale==1);
    assert(pixels[0]==0x12345678);calls++;return true;
}
int64_t gui_surface_blit(uint64_t pid,uint64_t id,uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *pixels,uint64_t stride) {
    (void)x;(void)y;assert(pid==7 && id==99 && w*h*4<=65536 && stride==w && pixels[0]==0x12345678);calls++;
    return fail_surface && checkpoints?-1:0;
}
int64_t gui_surface_copy(uint64_t pid,uint64_t id,uint64_t x,uint64_t y,uint64_t w,uint64_t h,uint32_t *pixels,uint64_t stride) {
    (void)x;(void)y;assert(pid==7 && id==99 && w*h*4<=65536 && stride==w);calls++;
    if(fail_surface && checkpoints)return -1;
    for(uint64_t i=0;i<w*h;i++)pixels[i]=0xabcdef12;
    return 0;
}
static void reset(void) {assert(!depth && !allocations && address_space==0x2000);checkpoints=calls=0;foreign=fail_alloc=deny_after_first=fail_surface=false;}
int main(void) {
    uint32_t *pixels=malloc(512*128*4);assert(pixels);
    for(unsigned i=0;i<512*128;i++)pixels[i]=0x12345678;
    struct srv_gfx_blit_rect gfx={.pixels=pixels,.width=512,.height=128,.stride=512};
    struct srv_gui_surface_rect surface={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(surface),.pixels=pixels,.surface_id=99,.width=512,.height=128,.stride=512};
    reset();assert(!syscall_gfx_blit_copy(gfx,1));assert(calls==4 && checkpoints==4 && !depth && !allocations);
    reset();assert(!syscall_gui_surface_blit(&surface));assert(calls==4 && checkpoints==4 && !depth && !allocations);
    reset();assert(!syscall_gui_surface_copy(&surface));assert(calls==4 && checkpoints==4 && !depth && !allocations);
    for(unsigned i=0;i<512*128;i++)assert(pixels[i]==0xabcdef12);
    reset();fail_alloc=true;assert(syscall_gui_surface_copy(&surface)==-1);assert(!calls && !depth && !allocations);
    reset();fail_surface=true;assert(syscall_gui_surface_copy(&surface)==-1);assert(calls==2 && checkpoints==1 && !depth && !allocations);
    reset();deny_after_first=true;assert(syscall_gui_surface_copy(&surface)==-1);assert(checkpoints==1 && !depth && !allocations);
    reset();foreign=true;assert(!syscall_gui_surface_copy(&surface));assert(calls==4 && !checkpoints && !depth && !allocations);
    free(pixels);puts("graphics-service: PASS bounded chunks, restored CR3, revalidation/failure cleanup, private-buffer lifetime and foreign-owner fallback");
}
