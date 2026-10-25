/* Explicit boot-lifetime, volatile scratch disk. Never aliases the immutable
 * initramfs bytes and never enables writes on a physical block device. */
#include <srvros/block.h>
#include <srvros/pmm.h>
#include <srvros/heap.h>
#include <stddef.h>

struct ram_copy { uint8_t *bytes; uint64_t size; };
static bool range(struct block_device *device,uint64_t offset,uint64_t length) {
    if(!device || !device->private_data)return false;
    struct ram_copy *ram=device->private_data;
    return offset<=ram->size && length<=ram->size-offset;
}
static bool ram_read(struct block_device *device,uint64_t offset,void *data,uint64_t length) {
    if(!data || !range(device,offset,length))return false;
    struct ram_copy *ram=device->private_data;
    for(uint64_t i=0;i<length;i++)((uint8_t *)data)[i]=ram->bytes[offset+i];
    return true;
}
static bool ram_write(struct block_device *device,uint64_t offset,const void *data,uint64_t length) {
    if(!data || !range(device,offset,length))return false;
    struct ram_copy *ram=device->private_data;
    for(uint64_t i=0;i<length;i++)ram->bytes[offset+i]=((const uint8_t *)data)[i];
    return true;
}
/* Writes are already visible in this volatile copy. No persistence is claimed. */
static bool ram_flush(struct block_device *device) { return range(device,0,0); }
struct block_device *block_register_ram_copy(const char *name,const uint8_t *data,uint64_t size) {
    if(!name || !data || !size || size>128ull*1024*1024 || size%512)return NULL;
    struct ram_copy *ram=kmalloc(sizeof(*ram));
    if(!ram)return NULL;
    uint64_t pages=(size+PMM_FRAME_SIZE-1)/PMM_FRAME_SIZE;
    uint64_t physical=pmm_alloc_frames(pages);
    if(!physical) { kfree(ram);return NULL; }
    ram->bytes=pmm_phys_to_virt(physical);ram->size=size;
    for(uint64_t i=0;i<size;i++)ram->bytes[i]=data[i];
    struct block_device *device=block_register(name,512,size/512,ram_read,ram_write,ram);
    if(!device) { pmm_free_frame_range(physical,pages);kfree(ram);return NULL; }
    device->flush=ram_flush;
    return device;
}
