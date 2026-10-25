#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <srvros/block.h>
#include <srvros/pmm.h>
#include <srvros/heap.h>
static unsigned fail, allocations, frees, frame_frees;
static uint8_t frames[8192];
static struct block_device saved;
void *kmalloc(size_t size) { if(fail==1)return NULL;allocations++;return malloc(size); }
void kfree(void *p) { frees++;free(p); }
uint64_t pmm_alloc_frames(uint64_t count) { assert(count<=2);return fail==2?0:4096; }
void *pmm_phys_to_virt(uint64_t addr) { assert(addr==4096);return frames; }
void pmm_free_frame_range(uint64_t addr,uint64_t count) { assert(addr==4096 && count==2);frame_frees++; }
struct block_device *block_register(const char *name,uint64_t size,uint64_t count,block_read_fn read,block_write_fn write,void *p) {
    if(fail==3)return NULL;
    saved=(struct block_device){.used=true,.name=name,.block_size=size,.block_count=count,.read=read,.write=write,.private_data=p};return &saved;
}
int main(void) {
    uint8_t source[4608], original[4608], data[1024];
    for(unsigned i=0;i<sizeof(source);i++)source[i]=i;
    memcpy(original,source,sizeof(source));
    assert(!block_register_ram_copy("bad",source,513));
    assert(!block_register_ram_copy("bad",source,128ull*1024*1024+512));
    assert(!block_register_ram_copy("bad",NULL,512));
    for(fail=1;fail<=3;fail++)assert(!block_register_ram_copy("fault",source,sizeof(source)));
    assert(allocations==2 && frees==2 && frame_frees==1);
    fail=0;struct block_device *dev=block_register_ram_copy("scratch",source,sizeof(source));assert(dev);
    assert(dev->block_size==512 && dev->block_count==9);
    assert(dev->read(dev,3584,data,1024) && !memcmp(data,source+3584,1024));
    memset(data,0x42,sizeof(data));assert(dev->write(dev,3584,data,1024));
    assert(!memcmp(source,original,sizeof(source)));
    memset(data,0,sizeof(data));assert(dev->read(dev,3584,data,1024));
    for(unsigned i=0;i<sizeof(data);i++)assert(data[i]==0x42);
    assert(!dev->write(dev,4608,data,1) && !dev->read(dev,4608,data,1));
    assert(!dev->write(dev,UINT64_MAX,data,2) && !dev->read(dev,1,data,UINT64_MAX));
    assert(dev->flush(dev));kfree(dev->private_data);
    puts("ram-copy: PASS independent bytes, cross-page writes, exact bounds, overflow, allocation/registry cleanup");
}
