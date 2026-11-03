#include <srvros/virtio_net.h>
#include <srvros/virtio_pci.h>
#include <srvros/net_device.h>
#include <srvros/net.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/console.h>
#include <srvros/vmm.h>
#include <stddef.h>

/* VirtIO 1.2 sections 5.1.6 and 5.1.6.4: modern headers include
 * num_buffers even without MRG_RXBUF, and non-mergeable RX must set it to 1.
 * QEMU 11.1.2 leaves that field unwritten without MRG_RXBUF; accept its
 * zero as well. A single bounded descriptor still contains the whole frame.
 * https://raw.githubusercontent.com/qemu/qemu/v11.1.2/hw/net/virtio-net.c
 * https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html */
#define NET_F_MAC (1ull << 5)
#define NET_F_STATUS (1ull << 16)
#define RX_QUEUE 0u
#define TX_QUEUE 1u
#define SLOTS 16u
#define HEADER_SIZE 12u
#define BUFFER_SIZE (HEADER_SIZE + NET_DEVICE_FRAME_MAX)
struct slot { uint64_t phys; uint8_t *data; uint16_t token; bool pending; };
static struct slot rx[SLOTS], tx[SLOTS];
static struct virtio_pci_device *device;
static uint8_t mac[6];
static bool ready, attempted, quarantined;
static bool lock, polling, has_status;

/* The transport and its DMA mappings are visible in kernel CR3. Keep the
 * caller address space and IRQ state across every bounded hardware access. */
struct access_scope { uint64_t flags, space; };
#ifdef SRVROS_VIRTIO_NET_HOST_TEST
uint64_t virtio_net_test_irq_save(void);
void virtio_net_test_irq_restore(uint64_t flags);
#endif
static struct access_scope access_begin(void) {
    struct access_scope s;
#ifdef SRVROS_VIRTIO_NET_HOST_TEST
    s.flags=virtio_net_test_irq_save();
#else
    __asm__ volatile("pushfq; popq %0; cli":"=r"(s.flags)::"memory");
#endif
    s.space=vmm_current_address_space();
    uint64_t kernel=vmm_kernel_address_space();
    if(s.space!=kernel) vmm_switch_address_space(kernel);
    return s;
}
static void access_end(struct access_scope s) {
    if(vmm_current_address_space()!=s.space) vmm_switch_address_space(s.space);
#ifdef SRVROS_VIRTIO_NET_HOST_TEST
    virtio_net_test_irq_restore(s.flags);
#else
    if(s.flags & (1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
}
static uint64_t received, transmitted, dropped;

static void clear(void *p, unsigned n) { uint8_t *b=p; while(n--) *b++=0; }
static bool acquire(void) { return !__atomic_test_and_set(&lock,__ATOMIC_ACQUIRE); }
static void release(void) { __atomic_clear(&lock,__ATOMIC_RELEASE); }
static void free_buffers(void) {
    for(unsigned i=0;i<SLOTS;i++) {
        if(rx[i].phys) pmm_free_frame(rx[i].phys);
        if(tx[i].phys) pmm_free_frame(tx[i].phys);
        rx[i]=(struct slot){0}; tx[i]=(struct slot){0};
    }
}
/* Caller holds lock or is the sole boot-time owner. */
static void fail(void) {
    __atomic_store_n(&ready,false,__ATOMIC_RELEASE);
    if(device && !virtio_pci_stop(device)) { quarantined=true; return; }
    free_buffers();
}
static bool post_rx(struct slot *s) {
    struct virtio_pci_buffer b={s->phys,BUFFER_SIZE,true};
    clear(s->data,HEADER_SIZE);
    if(!virtio_pci_submit(device,RX_QUEUE,&b,1,&s->token)) return false;
    s->pending=true; return true;
}
static struct slot *find_token(struct slot *slots,uint16_t token) {
    for(unsigned i=0;i<SLOTS;i++) if(slots[i].pending && slots[i].token==token) return &slots[i];
    return NULL;
}
static bool reap_tx(void) {
    for(unsigned i=0;i<SLOTS;i++) {
        struct virtio_pci_completion c;
        int result=virtio_pci_poll(device,TX_QUEUE,&c);
        if(result<0) return false;
        if(!result) return true;
        struct slot *s=find_token(tx,c.token);
        if(!s || c.length!=0) return false;
        s->pending=false;
    }
    return true;
}
static bool provider_send(void *context,const uint8_t *frame,uint16_t length) {
    (void)context; return virtio_net_send_frame(frame,length);
}
static uint64_t provider_poll(void *context,uint64_t budget,bool verbose) {
    (void)context; return virtio_net_poll(budget,verbose);
}
static bool provider_link(void *context,struct net_device_link *out) {
    (void)context; if(!out) return false;
    struct access_scope scope=access_begin();
    if(!acquire()) { access_end(scope); return false; }
    bool ok=ready; uint16_t status=1;
    if(ok && has_status && !virtio_pci_config_read16(device,6,&status)) {
        fail(); ok=false;
    }
    if(ok) *out=(struct net_device_link){.up=(status&1)!=0};
    release(); access_end(scope); return ok;
}
bool virtio_net_init(void) {
    if(attempted) return ready;
    attempted=true;
    const struct pci_device *pci=NULL;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(p && p->vendor_id==0x1af4 && p->device_id==0x1041) { pci=p; break; }
    }
    if(!pci) return false;
    if(!virtio_pci_open(pci,1,NET_F_MAC|NET_F_STATUS,NET_F_MAC,6,&device)) { fail(); return false; }
    if(!virtio_pci_config_read(device,0,mac,6) || (mac[0]&1)) { fail(); return false; }
    has_status=(virtio_pci_features(device)&NET_F_STATUS)!=0;
    uint16_t initial_status;
    if(has_status && !virtio_pci_config_read16(device,6,&initial_status)) { fail(); return false; }
    unsigned any=0; for(unsigned i=0;i<6;i++) any|=mac[i];
    if(!any || !virtio_pci_queue_setup(device,RX_QUEUE,SLOTS) ||
        !virtio_pci_queue_setup(device,TX_QUEUE,SLOTS)) { fail(); return false; }
    for(unsigned i=0;i<SLOTS;i++) {
        rx[i].phys=pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        tx[i].phys=pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        if(!rx[i].phys || !tx[i].phys) { fail(); return false; }
        rx[i].data=pmm_phys_to_virt(rx[i].phys); tx[i].data=pmm_phys_to_virt(tx[i].phys);
        if(!rx[i].data || !tx[i].data || !post_rx(&rx[i])) { fail(); return false; }
    }
    if(!virtio_pci_start(device)) { fail(); return false; }
    struct net_device_ops ops={.name="virtio-net",.priority=90,.send=provider_send,
        .poll=provider_poll,.link=provider_link};
    for(unsigned i=0;i<6;i++) ops.mac[i]=mac[i];
    __atomic_store_n(&ready,true,__ATOMIC_RELEASE);
    if(!net_device_register(&ops)) { fail(); return false; }
    return true;
}
bool virtio_net_available(void) { return __atomic_load_n(&ready,__ATOMIC_ACQUIRE); }
bool virtio_net_send_frame(const uint8_t *frame,uint16_t length) {
    if(!frame || length<14 || length>NET_DEVICE_FRAME_MAX) return false;
    /* The caller buffer may exist only in its address space. */
    uint8_t packet[NET_DEVICE_FRAME_MAX];
    for(unsigned i=0;i<length;i++) packet[i]=frame[i];
    struct access_scope scope=access_begin();
    if(!acquire()) { access_end(scope); return false; }
    bool sent=false;
    if(!ready) goto out;
    if(!reap_tx()) { fail(); goto out; }
    struct slot *s=NULL;
    for(unsigned i=0;i<SLOTS;i++) if(!tx[i].pending) { s=&tx[i]; break; }
    if(!s) goto out;
    clear(s->data,HEADER_SIZE);
    for(unsigned i=0;i<length;i++) s->data[HEADER_SIZE+i]=packet[i];
    unsigned wire_length=length<60?60:length;
    for(unsigned i=length;i<wire_length;i++) s->data[HEADER_SIZE+i]=0;
    struct virtio_pci_buffer b={s->phys,HEADER_SIZE+wire_length,false};
    if(!virtio_pci_submit(device,TX_QUEUE,&b,1,&s->token)) {
        if(!virtio_pci_active(device)) fail();
        goto out;
    }
    s->pending=true; transmitted++; sent=true;
out:
    release(); access_end(scope); return sent;
}
uint64_t virtio_net_poll(uint64_t budget,bool verbose) {
    (void)verbose;
    if(!budget || __atomic_test_and_set(&polling,__ATOMIC_ACQUIRE)) return 0;
    if(budget>SLOTS) budget=SLOTS;
    uint64_t count=0;
    while(count<budget) {
        uint8_t packet[NET_DEVICE_FRAME_MAX]; uint16_t length=0;
        struct access_scope scope=access_begin();
        if(!acquire()) { access_end(scope); break; }
        if(!ready) { release(); access_end(scope); break; }
        if(!reap_tx()) { fail(); release(); access_end(scope); break; }
        struct virtio_pci_completion c;
        int result=virtio_pci_poll(device,RX_QUEUE,&c);
        if(result<0) { fail(); release(); access_end(scope); break; }
        if(!result) { release(); access_end(scope); break; }
        struct slot *s=find_token(rx,c.token);
        if(!s) { fail(); release(); access_end(scope); break; }
        s->pending=false;
        if(c.length>=HEADER_SIZE+14 && c.length<=BUFFER_SIZE &&
           s->data[0]==0 && s->data[1]==0 && s->data[10]<=1 && s->data[11]==0) {
            length=(uint16_t)(c.length-HEADER_SIZE);
            for(unsigned i=0;i<length;i++) packet[i]=s->data[HEADER_SIZE+i];
            received++;
        } else dropped++;
        if(!post_rx(s)) { fail(); release(); access_end(scope); break; }
        release(); access_end(scope);
        count++;
        if(length) net_handle_ethernet_frame(packet,length);
    }
    __atomic_clear(&polling,__ATOMIC_RELEASE);
    return count;
}
void virtio_net_print_status(void) {
    console_printf("virtio-net: %s rx=%u tx=%u dropped=%u%s\n",
        virtio_net_available()?"ready":"down",received,transmitted,dropped,
        quarantined?" quarantined":"");
}
