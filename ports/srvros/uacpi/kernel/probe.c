#include <srvros/aml_probe.h>
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/heap.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>
#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>
#include <uacpi/context.h>
#include <uacpi/osi.h>
#include <uacpi/tables.h>
#include <stddef.h>
#include <stdint.h>

#define COPY_COUNT 64
#define COPY_BUDGET (4u*1024*1024)
#define HEAP_BUDGET (4u*1024*1024)
struct memory_copy { uint64_t physical;size_t size;uint8_t *data;bool region; };
static struct memory_copy copies[COPY_COUNT];
static size_t copy_count,copy_bytes,heap_bytes;
static struct { const struct pci_device *device;uint8_t bytes[256]; } pci_copies[32];
static size_t pci_count;
static uint64_t root_physical,errors,denied,writes;
static bool ran;
static struct limine_memmap_response *memory_map;
static void bytes_copy(void *to,const void *from,size_t size) { uint8_t *d=to;const uint8_t *s=from;while(size--) *d++=*s++; }
static bool contains(uint64_t base,uint64_t size,uint64_t address,uint64_t length) { return address>=base && address-base<=size && length<=size-(address-base); }
static struct memory_copy *copy_memory(uint64_t address,size_t size,bool region,const void *source) {
    if(!size||size>1024*1024||copy_count==COPY_COUNT||size>COPY_BUDGET-copy_bytes) return NULL;
    uint8_t *buffer=kmalloc(size);if(!buffer) return NULL;
    bytes_copy(buffer,source,size);
    struct memory_copy *copy=&copies[copy_count++];*copy=(struct memory_copy){address,size,buffer,region};copy_bytes+=size;return copy;
}
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out) { *out=root_physical;return UACPI_STATUS_OK; }
void *uacpi_kernel_map(uacpi_phys_addr address,uacpi_size length) {
    if(!length) goto fail;
    for(size_t i=0;i<copy_count;i++) if(contains(copies[i].physical,copies[i].size,address,length)) return copies[i].data+(address-copies[i].physical);
    /* Only firmware-described ACPI storage can supply dynamic AML regions.
     * Copy an entire bounded range once so overlapping fields stay coherent. */
    if(memory_map) for(uint64_t i=0;i<memory_map->entry_count;i++) {
        const struct limine_memmap_entry *e=memory_map->entries[i];
        if((e->type==LIMINE_MEMMAP_ACPI_NVS || e->type==LIMINE_MEMMAP_ACPI_RECLAIMABLE) && contains(e->base,e->length,address,length)) {
            struct memory_copy *c=copy_memory(e->base,e->length,true,pmm_phys_to_virt(e->base));
            if(c) return c->data+(address-c->physical);
            break;
        }
    }
fail:
    denied++;console_printf("aml-probe: denied mapping address=%x bytes=%u\n",address,(uint64_t)length);return UACPI_MAP_FAILED;
}
void uacpi_kernel_unmap(void *p,uacpi_size length) { (void)p;(void)length; }
void uacpi_kernel_log(uacpi_log_level level,const char *text) {
    if(level==UACPI_LOG_ERROR) errors++;
    if(level<=UACPI_LOG_WARN) { console_write("aml-probe: ");console_write(text); }
}
struct allocation_header { size_t charged; uint64_t padding; };
_Static_assert(sizeof(struct allocation_header)==16,"preserve kernel heap alignment");
void *uacpi_kernel_alloc(uacpi_size size) {
    if(!size) size=1;
    if(size>HEAP_BUDGET-sizeof(struct allocation_header)) return NULL;
    size_t charged=size+sizeof(struct allocation_header);
    if(charged>HEAP_BUDGET-heap_bytes) return NULL;
    struct allocation_header *p=kmalloc(charged);if(!p) return NULL;
    p->charged=charged;heap_bytes+=charged;return p+1;
}
void uacpi_kernel_free(void *p) {
    if(p) { struct allocation_header *h=(struct allocation_header *)p-1;heap_bytes-=h->charged;kfree(h); }
}
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) { return timer_monotonic_us()*1000; }
void uacpi_kernel_stall(uacpi_u8 us) { timer_delay_us(us); }
void uacpi_kernel_sleep(uacpi_u64 ms) {
    /* The resource probe never needs long firmware sleeps. Record unsupported
     * behavior rather than trusting the result of a skipped sleep. */
    if(ms>100) { errors++;return; }
    timer_delay_us(ms*1000);scheduler_yield();
}
uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 units) { uacpi_u32 *s=uacpi_kernel_alloc(sizeof(*s));if(s) *s=units;return s; }
void uacpi_kernel_free_semaphore(uacpi_handle s) { uacpi_kernel_free(s); }
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle,uacpi_u16 timeout) {
    uacpi_u32 *s=handle;if(*s) { --*s;return UACPI_STATUS_OK; }
    if(timeout) { errors++;return UACPI_STATUS_DENIED; }
    return UACPI_STATUS_TIMEOUT;
}
void uacpi_kernel_signal_semaphore(uacpi_handle handle) { uacpi_u32 *s=handle;if(*s!=UINT32_MAX) ++*s; }
uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (void *)1; }
uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) { uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f; }
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state s) { if(s&(1ull<<9)) __asm__ volatile("sti":::"memory"); }
uacpi_handle uacpi_kernel_create_spinlock(void) { return (void *)1; }
void uacpi_kernel_free_spinlock(uacpi_handle h) { (void)h; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) { (void)h;return uacpi_kernel_disable_interrupts(); }
void uacpi_kernel_unlock_spinlock(uacpi_handle h,uacpi_cpu_flags f) { (void)h;uacpi_kernel_restore_interrupts(f); }
uacpi_handle uacpi_kernel_create_work_item(void) { return (void *)1; }
void uacpi_kernel_free_work_item(uacpi_handle h) { (void)h; }
void uacpi_kernel_schedule_work(uacpi_work_type t,uacpi_handle item,uacpi_work_handler fn,uacpi_handle ctx) {
    (void)t;(void)item;(void)fn;(void)ctx;errors++;denied++;
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) { return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r) { (void)r;denied++;return UACPI_STATUS_DENIED; }
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq,uacpi_interrupt_handler fn,uacpi_handle ctx,uacpi_handle *out) { (void)irq;(void)fn;(void)ctx;(void)out;denied++;return UACPI_STATUS_DENIED; }
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler fn,uacpi_handle h) { (void)fn;(void)h;return UACPI_STATUS_DENIED; }
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address a,uacpi_handle *out) {
    for(size_t i=0;i<pci_count;i++) { const struct pci_device *d=pci_copies[i].device;if(!a.segment&&a.bus==d->bus&&a.device==d->device&&a.function==d->function) { *out=&pci_copies[i];return UACPI_STATUS_OK; } }
    if(a.segment || pci_count==32) goto fail;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *d=pci_device_at(i);
        if(a.bus!=d->bus||a.device!=d->device||a.function!=d->function) continue;
        pci_copies[pci_count].device=d;
        for(unsigned offset=0;offset<256;offset+=4) { uint32_t v=pci_read_config32(d,offset);bytes_copy(pci_copies[pci_count].bytes+offset,&v,4); }
        *out=&pci_copies[pci_count++];return UACPI_STATUS_OK;
    }
fail:
    denied++;return UACPI_STATUS_DENIED;
}
void uacpi_kernel_pci_device_close(uacpi_handle h) { (void)h; }
#define SNAPSHOT_PCI(bits) \
uacpi_status uacpi_kernel_pci_read##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits *v) { if(o>256-sizeof(*v)) { denied++;return UACPI_STATUS_DENIED; }bytes_copy(v,((typeof(pci_copies[0]) *)h)->bytes+o,sizeof(*v));return UACPI_STATUS_OK; } \
uacpi_status uacpi_kernel_pci_write##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits v) { if(o>256-sizeof(v)) { denied++;return UACPI_STATUS_DENIED; }bytes_copy(((typeof(pci_copies[0]) *)h)->bytes+o,&v,sizeof(v));writes++;return UACPI_STATUS_OK; }
SNAPSHOT_PCI(8) SNAPSHOT_PCI(16) SNAPSHOT_PCI(32)
uacpi_status uacpi_kernel_io_map(uacpi_io_addr b,uacpi_size n,uacpi_handle *out) { (void)b;(void)n;(void)out;denied++;return UACPI_STATUS_DENIED; }
void uacpi_kernel_io_unmap(uacpi_handle h) { (void)h; }
#define DENIED_IO(bits) \
uacpi_status uacpi_kernel_io_read##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits *v) { (void)h;(void)o;(void)v;denied++;return UACPI_STATUS_DENIED; } \
uacpi_status uacpi_kernel_io_write##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits v) { (void)h;(void)o;(void)v;denied++;return UACPI_STATUS_DENIED; }
DENIED_IO(8) DENIED_IO(16) DENIED_IO(32)
static bool region_access(void *p,size_t size) {
    for(size_t i=0;i<copy_count;i++) if(copies[i].region && contains((uintptr_t)copies[i].data,copies[i].size,(uintptr_t)p,size)) return true;
    denied++;return false;
}
#define SNAPSHOT_MMIO(bits) \
uacpi_u##bits uacpi_kernel_mmio_read##bits(void *p) { uacpi_u##bits v=0;if(region_access(p,sizeof(v))) bytes_copy(&v,p,sizeof(v));return v; } \
void uacpi_kernel_mmio_write##bits(void *p,uacpi_u##bits v) { if(region_access(p,sizeof(v))) { bytes_copy(p,&v,sizeof(v));writes++; } }
SNAPSHOT_MMIO(8) SNAPSHOT_MMIO(16) SNAPSHOT_MMIO(32) SNAPSHOT_MMIO(64)
static void evaluate(const char *path) {
    console_printf("aml-probe: evaluating %s start_us=%u\n",path,timer_monotonic_us());
    uacpi_object *object=NULL;uacpi_status st=uacpi_eval_simple(NULL,path,&object);
    console_printf("aml-probe: %s = ",path);
    if(st) { console_printf("%s\n",uacpi_status_to_string(st));errors++;return; }
    uacpi_u64 integer;uacpi_data_view data;
    if(!object) console_write("no return\n");
    else if(!uacpi_object_get_integer(object,&integer)) console_printf("%x\n",integer);
    else if(!uacpi_object_get_string(object,&data)) { console_write(data.text);console_write("\n"); }
    else if(!uacpi_object_get_buffer(object,&data)) {
        console_printf("buffer bytes=%u:",(uint64_t)data.length);
        for(size_t i=0;i<data.length && i<64;i++) console_printf(" %u",(uint64_t)data.bytes[i]);
        console_write("\n");
    } else console_printf("type=%s\n",uacpi_object_type_to_string(uacpi_object_get_type(object)));
    if(object) uacpi_object_unref(object);
}
bool aml_probe_run(void *rsdp,struct limine_memmap_response *map) {
    if(ran || !rsdp || !map || !timer_reference_ready()) return false;
    ran=true;memory_map=map;
    console_write("aml-probe: isolated uACPI 7.0.0; private firmware/PCI snapshots, no EC/SCI or hardware writes\n");
    uint64_t started=timer_monotonic_us();
    root_physical=(uintptr_t)rsdp-pmm_hhdm_offset();
    if(!copy_memory(root_physical,sizeof(struct acpi_rsdp),false,rsdp)) { errors++;goto done; }
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *table=acpi_table_at(i);
        if(!copy_memory(table->physical_address,table->length,false,pmm_phys_to_virt(table->physical_address))) { errors++;goto done; }
    }
    console_printf("aml-probe: stage=initialize copied_bytes=%u copies=%u start_us=%u\n",(uint64_t)copy_bytes,(uint64_t)copy_count,timer_monotonic_us());
    uacpi_status st=uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE|UACPI_FLAG_BAD_CSUM_FATAL|UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL|UACPI_FLAG_PROACTIVE_TBL_CSUM);
    if(st) { console_printf("aml-probe: initialize=%s\n",uacpi_status_to_string(st));errors++;goto reset; }
    console_printf("aml-probe: stage=initialized heap_bytes=%u elapsed_us=%u\n",(uint64_t)heap_bytes,timer_monotonic_us()-started);
    uacpi_context_set_loop_timeout(1);uacpi_context_set_max_call_stack_depth(32);
    /* Explicitly select only the Apple firmware branch for this probe. This
     * does not initialize native ACPI services or make it general OS policy. */
    uacpi_bulk_configure_interfaces(UACPI_INTERFACE_ACTION_DISABLE,UACPI_INTERFACE_KIND_VENDOR);
    uacpi_install_interface("Darwin",UACPI_INTERFACE_KIND_VENDOR);
    console_write("aml-probe: stage=namespace-load\n");
    st=uacpi_namespace_load();
    if(st) { console_printf("aml-probe: load=%s\n",uacpi_status_to_string(st));errors++;goto reset; }
    console_printf("aml-probe: stage=namespace-loaded heap_bytes=%u elapsed_us=%u\n",(uint64_t)heap_bytes,timer_monotonic_us()-started);
    uacpi_namespace_node *node=NULL;
    if(uacpi_namespace_node_find(NULL,"\\_SB.PCI0.LPCB.EC",&node)==UACPI_STATUS_OK) {
        static const char *const paths[]={"\\PINI","\\OSDW","\\_SB.PCI0.LPCB.EC._HID","\\_SB.PCI0.LPCB.EC._CRS",
            "\\_SB.PCI0.LPCB.EC._GPE","\\_SB.PCI0.LPCB.EC.SMB0._HID","\\_SB.PCI0.LPCB.EC.SMB0._STA",
            "\\_SB.PCI0.LPCB.EC.SMB0._EC","\\_SB.PCI0.LPCB.EC.SMB0.SBS0._HID","\\_SB.PCI0.LPCB.EC.SMB0.SBS0._SBS"};
        for(size_t i=0;i<sizeof(paths)/sizeof(*paths);i++) evaluate(paths[i]);
    } else console_write("aml-probe: MacBook EC namespace absent; no battery provider\n");
reset:
    console_write("aml-probe: stage=reset\n");
    uacpi_state_reset();
done:
    console_printf("aml-probe: completed elapsed_us=%u copied_bytes=%u errors=%u denied=%u snapshot_writes=%u heap_remaining=%u; physical battery NOT_TESTED\n",
        timer_monotonic_us()-started,(uint64_t)copy_bytes,errors,denied,writes,(uint64_t)heap_bytes);
    for(size_t i=0;i<copy_count;i++) kfree(copies[i].data);
    copy_count=copy_bytes=0;memory_map=NULL;
    return errors==0 && denied==0 && heap_bytes==0;
}
