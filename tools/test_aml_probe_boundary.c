/* Exercise the actual native adapter's allocation and snapshot boundary. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/kernel/probe.c"
static unsigned allocated,frees;
static bool fail_alloc;
static uint8_t firmware[256];
static struct pci_device device={.bus=0,.device=31,.function=3};
void *kmalloc(size_t size) { if(fail_alloc)return NULL;void *p=malloc(size);if(p)allocated++;return p; }
void kfree(void *p) { if(p) frees++;free(p); }
void console_printf(const char *fmt,...) { (void)fmt; }
void *pmm_phys_to_virt(uint64_t address) { assert(address==0x1000);return firmware; }
uint64_t pci_device_count(void) { return 1; }
const struct pci_device *pci_device_at(uint64_t index) { return index?NULL:&device; }
uint32_t pci_read_config32(const struct pci_device *d,uint8_t offset) { assert(d==&device && !(offset&3));return 0x10203040+offset; }
int main(void) {
    for(unsigned n=0;n<64;n++) { void *p=uacpi_kernel_alloc(n);assert(p && !((uintptr_t)p&15));assert(heap_bytes>=n+16);uacpi_kernel_free(p);assert(!heap_bytes); }
    assert(!uacpi_kernel_alloc(SIZE_MAX) && !uacpi_kernel_alloc(HEAP_BUDGET));
    void *all=uacpi_kernel_alloc(HEAP_BUDGET-16);assert(all && heap_bytes==HEAP_BUDGET);
    assert(!uacpi_kernel_alloc(0));uacpi_kernel_free(all);
    fail_alloc=true;assert(!uacpi_kernel_alloc(12) && !heap_bytes);fail_alloc=false;
    assert(!contains(UINT64_MAX-10,10,UINT64_MAX-5,6));
    struct limine_memmap_entry entry={.base=0x1000,.length=sizeof(firmware),.type=LIMINE_MEMMAP_ACPI_NVS};
    struct limine_memmap_entry *entries[]={&entry};
    struct limine_memmap_response map={.entry_count=1,.entries=entries};memory_map=&map;
    for(unsigned i=0;i<sizeof(firmware);i++)firmware[i]=(uint8_t)i;
    uint8_t *p=uacpi_kernel_map(0x1010,8);assert(p!=UACPI_MAP_FAILED && p[0]==16 && copy_count==1);
    assert(uacpi_kernel_map(0x1014,4)==p+4 && copy_count==1);
    uacpi_kernel_mmio_write32(p,0xabcdef12);assert(uacpi_kernel_mmio_read32(p)==0xabcdef12);
    assert(firmware[16]==16 && firmware[17]==17);
    assert(uacpi_kernel_map(0x10ff,2)==UACPI_MAP_FAILED);
    assert(uacpi_kernel_map(UINT64_MAX-1,4)==UACPI_MAP_FAILED);
    assert(uacpi_kernel_map(0x1000,0)==UACPI_MAP_FAILED);
    unsigned before=denied;uacpi_kernel_mmio_write64(firmware,0);assert(denied==before+1 && firmware[1]==1);
    uacpi_handle handle=NULL;
    assert(uacpi_kernel_io_map(0x62,1,&handle)==UACPI_STATUS_DENIED);
    assert(uacpi_kernel_pci_device_open((uacpi_pci_address){.segment=1,.device=31,.function=3},&handle)==UACPI_STATUS_DENIED);
    assert(uacpi_kernel_pci_device_open((uacpi_pci_address){.device=31,.function=3},&handle)==UACPI_STATUS_OK);
    uint32_t value=0;assert(!uacpi_kernel_pci_read32(handle,0,&value) && value==0x10203040);
    assert(!uacpi_kernel_pci_write32(handle,0,0xabcdef12));assert(!uacpi_kernel_pci_read32(handle,0,&value) && value==0xabcdef12);
    assert(pci_read_config32(&device,0)==0x10203040);
    assert(uacpi_kernel_pci_write32(handle,253,0)==UACPI_STATUS_DENIED);
    assert(uacpi_kernel_pci_read32(handle,SIZE_MAX,&value)==UACPI_STATUS_DENIED);
    for(size_t i=0;i<copy_count;i++) kfree(copies[i].data);
    assert(!heap_bytes && allocated==frees);
    puts("aml-boundary: PASS aligned budgeted allocations, overflow bounds, coherent private snapshots, denied I/O and PCI write isolation");
}
