/* Exercise real syscall validation/copy/chunking with bounded user mappings. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SERVICE_WINDOW_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static struct srv_gfx_blit_scaled_rect request;
static uint32_t pixels[4096*20];
static uint64_t address_space=7,rows_done,calls;
static bool fail_alloc,fail_blit,deny_pixels;
static unsigned service_depth;
uint64_t service_window_test_irq_save(void) {return 0;}
void service_window_test_irq_restore(uint64_t flags) {(void)flags;}
bool scheduler_io_begin(void) {service_depth++;return true;}
void scheduler_io_end(void) {assert(service_depth);service_depth--;}
void scheduler_io_checkpoint(void) {assert(service_depth && address_space==7);}
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(!writable);
    uint64_t starts[]={(uint64_t)&request,(uint64_t)pixels};
    uint64_t sizes[]={sizeof(request),deny_pixels?0:sizeof(pixels)};
    for(unsigned i=0;i<2;i++) if(address>=starts[i] && address-starts[i]<=sizes[i] && length<=sizes[i]-(address-starts[i])) return true;
    return false;
}
void *kmalloc(size_t size) { assert(size<=65536);return fail_alloc?NULL:malloc(size); }
void kfree(void *ptr) { free(ptr); }
uint64_t vmm_current_address_space(void) { return address_space; }
uint64_t vmm_kernel_address_space(void) { return 11; }
void vmm_switch_address_space(uint64_t space) { address_space=space; }
bool console_blit_rgb_scaled(uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *copy,uint64_t stride,uint64_t scale) {
    assert(address_space==11 && x==request.source.x && y==request.source.y+rows_done*scale);
    assert(w==request.source.width && stride==w && scale==request.scale);
    for(uint64_t row=0;row<h;row++) for(uint64_t col=0;col<w;col++)
        assert(copy[row*stride+col]==pixels[(rows_done+row)*request.source.stride+col]);
    rows_done+=h;calls++;return !fail_blit;
}
static void setup(void) {
    request=(struct srv_gfx_blit_scaled_rect){.source={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(request),
        .x=7,.y=9,.width=4000,.height=20,.stride=4096,.pixels=pixels},.scale=2};
    for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=i;
    rows_done=calls=0;fail_alloc=fail_blit=deny_pixels=false;address_space=7;
}
static void rejected(void) { assert(syscall_gfx_blit_scaled_rect(&request)==-1 && !calls && address_space==7); }
int main(void) {
    setup();assert(!syscall_gfx_blit_scaled_rect(&request));assert(calls==5 && rows_done==20 && address_space==7);
    setup();request.scale=1;request.source.struct_size=sizeof(request.source);
    assert(!syscall_gfx_blit_rect(&request.source) && rows_done==20 && address_space==7);
    setup();request.source.abi_version++;rejected();
    setup();request.source.struct_size--;rejected();
    setup();request.scale=0;rejected();
    setup();request.scale=3;rejected();
    setup();request.source.width=4097;rejected();
    setup();request.source.height=0;rejected();
    setup();request.source.stride=3999;rejected();
    setup();request.source.stride=UINT64_MAX;rejected();
    setup();request.source.x=UINT64_MAX;rejected();
    setup();request.source.y=UINT64_MAX;rejected();
    setup();request.source.pixels=NULL;rejected();
    setup();deny_pixels=true;rejected();
    setup();fail_alloc=true;rejected();
    setup();fail_blit=true;assert(syscall_gfx_blit_scaled_rect(&request)==-1 && calls==1 && address_space==7);
    puts("gfx-blit-host-test: PASS (ABI, scales, mapped bounds, stride, overflow, chunk copy, restore on failure)");
}
