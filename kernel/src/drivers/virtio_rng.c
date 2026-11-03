/* VirtIO 1.2 section 5.4: one device-writable request, accept only used bytes.
 * https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html */
#include <srvros/virtio_rng.h>
#include <srvros/virtio_pci.h>
#include <srvros/pmm.h>
#include <srvros/console.h>

#define RNG_BYTES 4096u
static struct virtio_pci_device *rng;
static uint64_t dma, received, consumed;
static uint8_t pool[RNG_BYTES];
static uint32_t position, available;
static uint16_t pending_token;
static bool attempted, ready, pending;

static void clear(void *buffer, unsigned count) {
    volatile uint8_t *p=buffer;
    while(count--) *p++=0;
}
static void retire(void) {
    ready=false; pending=false; position=available=0; clear(pool,sizeof(pool));
    /* A failed reset leaves the request pinned permanently. */
    if(rng && virtio_pci_stop(rng) && dma) {
        clear(pmm_phys_to_virt(dma),RNG_BYTES);
        pmm_free_frame(dma); dma=0;
    }
}
static bool submit(void) {
    struct virtio_pci_buffer b={dma,RNG_BYTES,true};
    if(!virtio_pci_submit(rng,0,&b,1,&pending_token)) return false;
    pending=true; return true;
}
bool virtio_rng_init(void) {
    if(attempted) return ready;
    attempted=true;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *pci=pci_device_at(i);
        if(!pci || pci->vendor_id!=0x1af4 || pci->device_id!=0x1044) continue;
        if(!virtio_pci_open(pci,4,0,0,0,&rng)) return false;
        dma=pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        if(!dma) { retire(); return false; }
        clear(pmm_phys_to_virt(dma),RNG_BYTES);
        if(!virtio_pci_queue_setup(rng,0,8) || !submit() || !virtio_pci_start(rng)) {
            retire(); return false;
        }
        ready=true; return true;
    }
    return false;
}
bool virtio_rng_ready(void) { return ready; }
void virtio_rng_stop(void) { retire(); }
void virtio_rng_poll(void) {
    if(!ready || available) return;
    if(!pending) { if(!submit()) retire(); return; }
    struct virtio_pci_completion completion;
    int result=virtio_pci_poll(rng,0,&completion);
    if(!result) return;
    if(result<0 || completion.token!=pending_token || completion.length>RNG_BYTES) {
        retire(); return;
    }
    pending=false;
    /* A zero or short reply is legal; never expose the untouched tail. */
    uint8_t *source=pmm_phys_to_virt(dma);
    for(uint32_t i=0;i<completion.length;i++) pool[i]=source[i];
    clear(source,RNG_BYTES);
    position=0; available=completion.length; received+=completion.length;
}
bool virtio_rng_take_u64(uint64_t *value) {
    if(!value || !ready || available<8) {
        /* Discard a final short tail so it cannot stall future replenishment. */
        if(available && available<8) { clear(pool+position,available); available=0; position=0; }
        return false;
    }
    uint64_t out=0;
    for(unsigned i=0;i<8;i++) { out|=(uint64_t)pool[position+i]<<(i*8); pool[position+i]=0; }
    position+=8; available-=8; consumed+=8; *value=out;
    return true;
}
void virtio_rng_print_status(void) {
    console_printf("virtio-rng: ready=%u received=%u consumed=%u buffered=%u quarantined=%u\n",
        (uint64_t)ready,received,consumed,(uint64_t)available,
        (uint64_t)(rng && virtio_pci_quarantined(rng)));
}
