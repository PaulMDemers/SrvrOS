#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../kernel/src/drivers/virtio_rng.c"
static uint8_t memory[RNG_BYTES];
static struct pci_device pci={.vendor_id=0x1af4,.device_id=0x1044};
static int reply;
static uint16_t reply_token=7;
static uint32_t reply_length;
static unsigned frees,stops,submissions;
static bool reset_ok=true,setup_ok=true,start_ok=true,submit_ok=true;
uint64_t pci_device_count(void) { return 1; }
const struct pci_device *pci_device_at(uint64_t index) { assert(index==0); return &pci; }
bool virtio_pci_open(const struct pci_device *p,uint16_t type,uint64_t allowed,uint64_t required,uint32_t config,struct virtio_pci_device **out) {
    assert(p==&pci && type==4 && !allowed && !required && !config); *out=(void *)1; return true;
}
bool virtio_pci_queue_setup(struct virtio_pci_device *p,uint16_t q,uint16_t size) { assert(p==(void *)1 && !q && size==8);return setup_ok; }
bool virtio_pci_start(struct virtio_pci_device *p) { assert(p==(void *)1);return start_ok; }
bool virtio_pci_submit(struct virtio_pci_device *p,uint16_t q,const struct virtio_pci_buffer *b,uint16_t count,uint16_t *token) {
    assert(p==(void *)1 && !q && count==1 && b->device_write && b->physical==4096 && b->length==RNG_BYTES);
    submissions++; *token=7;return submit_ok;
}
int virtio_pci_poll(struct virtio_pci_device *p,uint16_t q,struct virtio_pci_completion *c) {
    assert(p==(void *)1 && !q);*c=(struct virtio_pci_completion){reply_token,reply_length};int r=reply;reply=0;return r;
}
bool virtio_pci_stop(struct virtio_pci_device *p) { assert(p==(void *)1);stops++;return reset_ok; }
bool virtio_pci_quarantined(const struct virtio_pci_device *p) { (void)p;return !reset_ok; }
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) { assert(tag==PMM_FRAME_DMA);return 4096; }
void *pmm_phys_to_virt(uint64_t p) { assert(p==4096);return memory; }
void pmm_free_frame(uint64_t p) { assert(p==4096);frees++; }
void console_printf(const char *format,...) { (void)format; }
static void fresh(void) {
    rng=NULL;dma=received=consumed=0;position=available=0;pending_token=0;attempted=ready=pending=false;
    reply=0;reply_token=7;reply_length=0;frees=stops=submissions=0;
    reset_ok=setup_ok=start_ok=submit_ok=true;memset(pool,0,sizeof(pool));memset(memory,0xa5,sizeof(memory));
}
static void deliver(unsigned size) {
    for(unsigned i=0;i<RNG_BYTES;i++) memory[i]=(uint8_t)(i+1);
    reply=1;reply_length=size;virtio_rng_poll();
}
int main(void) {
    uint64_t value=99;fresh();assert(virtio_rng_init() && submissions==1);
    assert(!virtio_rng_take_u64(&value) && value==99);virtio_rng_poll();assert(!available);
    deliver(17);assert(received==17 && available==17);
    assert(virtio_rng_take_u64(&value) && value==0x0807060504030201ull);
    assert(virtio_rng_take_u64(&value) && value==0x100f0e0d0c0b0a09ull);
    assert(!virtio_rng_take_u64(&value) && !available && consumed==16);
    for(unsigned i=0;i<RNG_BYTES;i++) assert(!memory[i] && !pool[i]);
    virtio_rng_poll();assert(submissions==2);deliver(0);assert(ready && !available);
    virtio_rng_poll();deliver(RNG_BYTES);for(unsigned i=0;i<RNG_BYTES/8;i++) assert(virtio_rng_take_u64(&value));
    assert(!virtio_rng_take_u64(&value));virtio_rng_poll();reply=-1;virtio_rng_poll();assert(!ready && frees==1 && stops==1);
    fresh();assert(virtio_rng_init());reply_token=99;deliver(8);assert(!ready && frees==1);
    fresh();assert(virtio_rng_init());deliver(RNG_BYTES+1);assert(!ready && frees==1);
    fresh();assert(virtio_rng_init());reset_ok=false;reply=-1;virtio_rng_poll();assert(!ready && !frees && dma);
    fresh();setup_ok=false;assert(!virtio_rng_init() && stops==1 && frees==1);
    fresh();start_ok=false;assert(!virtio_rng_init() && stops==1 && frees==1);
    fresh();submit_ok=false;assert(!virtio_rng_init() && stops==1 && frees==1);
    puts("virtio-rng: PASS partial/zero replies, exhaustion/refill, consumed-byte erasure, bad completions, reset/quarantine");
}
