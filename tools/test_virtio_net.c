#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_VIRTIO_NET_HOST_TEST 1
#include "../kernel/src/drivers/virtio_net.c"

struct virtio_pci_device { int unused; };
static struct virtio_pci_device fake;
static struct pci_device pci={.vendor_id=0x1af4,.device_id=0x1041};
static bool offered=true, stop_ok=true, active=true, register_ok=true;
static bool fail_poll, fail_alloc;
static unsigned allocs, frees, delivered;
static struct virtio_pci_completion completions[2][64];
static unsigned heads[2], tails[2], next_token[2];
static struct net_device_ops registered;
static uint64_t space=1, irq=0x200;
static bool runtime, status_feature=true;
static uint16_t link_status=1;
static void check_access(void) { if(runtime) assert(space==1 && irq==0); }
uint64_t vmm_current_address_space(void) { return space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
void vmm_switch_address_space(uint64_t new_space) { assert(!irq); space=new_space; }
uint64_t virtio_net_test_irq_save(void) { uint64_t old=irq; irq=0; return old; }
void virtio_net_test_irq_restore(uint64_t old) { irq=old; }
uint64_t virtio_pci_features(const struct virtio_pci_device *d) { (void)d; return NET_F_MAC|(status_feature?NET_F_STATUS:0); }
bool virtio_pci_config_read16(struct virtio_pci_device *d,uint32_t off,uint16_t *v) {
    (void)d; assert(off==6); check_access(); *v=link_status; return true;
}
uint64_t pci_device_count(void) { return offered?1:0; }
const struct pci_device *pci_device_at(uint64_t i) { return i?NULL:&pci; }
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag t) {
    (void)t; if(fail_alloc) return 0; void *p=calloc(1,4096); assert(p); allocs++; return (uint64_t)(uintptr_t)p;
}
void *pmm_phys_to_virt(uint64_t p) { return (void *)(uintptr_t)p; }
void pmm_free_frame(uint64_t p) { free((void *)(uintptr_t)p); frees++; }
bool virtio_pci_open(const struct pci_device *p,uint16_t type,uint64_t allowed,uint64_t required,uint32_t len,struct virtio_pci_device **out) {
    assert(p==&pci && type==1 && allowed==(NET_F_MAC|NET_F_STATUS) && required==NET_F_MAC && len==6); *out=&fake; active=true; return true;
}
bool virtio_pci_config_read(struct virtio_pci_device *d,uint32_t off,void *dst,uint32_t n) {
    (void)d; assert(off==0 && n==6); const uint8_t m[6]={2,1,2,3,4,5}; memcpy(dst,m,n); return true;
}
bool virtio_pci_queue_setup(struct virtio_pci_device *d,uint16_t q,uint16_t n) { (void)d; assert(q<2 && n==SLOTS); return true; }
bool virtio_pci_start(struct virtio_pci_device *d) { (void)d; return true; }
bool virtio_pci_submit(struct virtio_pci_device *d,uint16_t q,const struct virtio_pci_buffer *b,uint16_t n,uint16_t *token) {
    (void)d; check_access(); assert(q<2 && n==1 && b->physical && b->device_write==(q==0)); *token=(uint16_t)next_token[q]++; return true;
}
int virtio_pci_poll(struct virtio_pci_device *d,uint16_t q,struct virtio_pci_completion *c) {
    (void)d; check_access(); if(fail_poll) return -1; if(heads[q]==tails[q]) return 0; *c=completions[q][heads[q]++]; return 1;
}
bool virtio_pci_stop(struct virtio_pci_device *d) { (void)d; check_access(); active=false; return stop_ok; }
bool virtio_pci_active(const struct virtio_pci_device *d) { (void)d; return active; }
bool net_device_register(const struct net_device_ops *ops) { registered=*ops; return register_ok; }
void net_handle_ethernet_frame(const uint8_t *p,uint16_t n) {
    assert(n==60 && p[0]==42 && space==2 && irq==0x200); delivered++;
    assert(virtio_net_poll(1,false)==0); /* nested poll cannot consume DMA */
    assert(virtio_net_send_frame(p,n)); /* delivery permits nested TX */
}
void console_printf(const char *fmt,...) { (void)fmt; }
static void complete(unsigned q,uint16_t token,uint32_t len) { completions[q][tails[q]++]=(struct virtio_pci_completion){token,len}; }
static void reset_fixture(void) {
    memset(rx,0,sizeof(rx)); memset(tx,0,sizeof(tx)); memset(heads,0,sizeof(heads)); memset(tails,0,sizeof(tails)); memset(next_token,0,sizeof(next_token));
    device=NULL; ready=attempted=quarantined=lock=polling=has_status=false; space=1; irq=0x200; runtime=false; status_feature=true; link_status=1; allocs=frees=delivered=0; offered=stop_ok=active=register_ok=true; fail_poll=fail_alloc=false;
}
int main(void) {
    reset_fixture(); offered=false; assert(!virtio_net_init() && !allocs);
    reset_fixture(); status_feature=false; assert(virtio_net_init());
    space=2; runtime=true; struct net_device_link no_status={0};
    assert(registered.link(NULL,&no_status) && no_status.up && space==2 && irq==0x200);
    runtime=false; space=1; assert(virtio_pci_stop(device)); free_buffers();
    reset_fixture(); assert(virtio_net_init() && registered.priority==90 && registered.rx_wake_ready==NULL);
    space=2; runtime=true;
    struct net_device_link link={0}; assert(registered.link(NULL,&link) && link.up && space==2 && irq==0x200);
    link_status=0; assert(registered.link(NULL,&link) && !link.up && space==2 && irq==0x200);
    uint8_t frame[60]={42}; assert(virtio_net_send_frame(frame,60));
    assert(space==2 && irq==0x200);
    assert(tx[0].pending && tx[0].data[0]==0 && tx[0].data[10]==0 && tx[0].data[12]==42);
    complete(TX_QUEUE,tx[0].token,0);
    rx[0].data[10]=1; memcpy(rx[0].data+12,frame,60); complete(RX_QUEUE,rx[0].token,72);
    assert(virtio_net_poll(1,false)==1 && delivered==1 && tx[0].pending && space==2 && irq==0x200);
    /* QEMU non-mergeable receive leaves num_buffers zero; both valid wire
     * lengths are accepted, but multi-buffer values remain malformed. */
    memcpy(rx[1].data+12,frame,60); complete(RX_QUEUE,rx[1].token,72);
    assert(virtio_net_poll(1,false)==1 && delivered==2);
    rx[2].data[10]=2; complete(RX_QUEUE,rx[2].token,72);
    assert(virtio_net_poll(1,false)==1 && delivered==2 && dropped);
    complete(TX_QUEUE,tx[0].token,1); assert(virtio_net_poll(1,false)==0 && !virtio_net_available() && frees==allocs && space==2 && irq==0x200);
    reset_fixture(); assert(virtio_net_init()); complete(RX_QUEUE,999,72);
    assert(virtio_net_poll(1,false)==0 && !virtio_net_available() && frees==allocs);
    reset_fixture(); fail_alloc=true; assert(!virtio_net_init() && frees==allocs);
    reset_fixture(); register_ok=false; assert(!virtio_net_init() && frees==allocs);
    reset_fixture(); assert(virtio_net_init()); space=2; irq=0; runtime=true; stop_ok=false; fail_poll=true;
    assert(virtio_net_poll(1,false)==0 && !virtio_net_available() && quarantined && frees==0 && space==2 && irq==0);
    /* Quarantined DMA is intentionally retained until reboot; free only test memory. */
    stop_ok=true; free_buffers();
    puts("virtio-net: PASS RX, TX, malformed, nested callbacks, fallback and reset quarantine");
}
