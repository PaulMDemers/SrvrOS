#include <srvros/acpi.h>
#include <srvros/ahci.h>
#include <srvros/arch.h>
#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/e1000.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/halt.h>
#include <srvros/initramfs.h>
#include <srvros/keyboard.h>
#include <srvros/monitor.h>
#include <srvros/mouse.h>
#include <srvros/net.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vmm.h>
#include <srvros/workqueue.h>

#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern volatile uint64_t limine_base_revision[];
extern volatile struct limine_bootloader_info_request bootloader_info_request;
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_module_request module_request;
bool lapic_init(void);
void lapic_timer_init(uint8_t vector, uint32_t initial_count);
bool ioapic_init(void);
bool ioapic_route_isa_irq(uint8_t irq, uint8_t vector);
bool ioapic_route_pci_irq(uint8_t irq, uint8_t vector);

static const char *memmap_type_name(uint64_t type) {
    switch (type) {
    case LIMINE_MEMMAP_USABLE:
        return "usable";
    case LIMINE_MEMMAP_RESERVED:
        return "reserved";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return "acpi reclaim";
    case LIMINE_MEMMAP_ACPI_NVS:
        return "acpi nvs";
    case LIMINE_MEMMAP_BAD_MEMORY:
        return "bad";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return "boot reclaim";
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return "kernel/modules";
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return "framebuffer";
    case LIMINE_MEMMAP_RESERVED_MAPPED:
        return "reserved mapped";
    default:
        return "unknown";
    }
}

static void print_memmap(void) {
    struct limine_memmap_response *memmap = memmap_request.response;
    if (memmap == NULL) {
        console_write("memmap: unavailable\n");
        return;
    }

    console_printf("memmap: %u entries\n", memmap->entry_count);
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        console_printf("  [%u] base=%x length=%x type=%s\n",
            i,
            entry->base,
            entry->length,
            memmap_type_name(entry->type));
    }
}

static void test_pmm(void) {
    uint64_t before = pmm_free_frames();
    uint64_t frame = pmm_alloc_frame();

    if (frame == 0) {
        console_write("pmm: test allocation failed\n");
        halt_forever();
    }

    console_printf("pmm: test alloc phys=%x virt=%x free_before=%u free_after=%u\n",
        frame,
        (uint64_t)pmm_phys_to_virt(frame),
        before,
        pmm_free_frames());

    pmm_free_frame(frame);
    console_printf("pmm: test free complete free=%u\n", pmm_free_frames());
}

static void test_heap(void) {
    uint64_t before = heap_free_bytes();
    uint64_t *numbers = kmalloc(sizeof(uint64_t) * 8);
    char *zeroes = kcalloc(32, 1);

    if (numbers == NULL || zeroes == NULL) {
        console_write("heap: test allocation failed\n");
        halt_forever();
    }

    for (uint64_t i = 0; i < 8; i++) {
        numbers[i] = 0x1000 + i;
    }

    for (uint64_t i = 0; i < 32; i++) {
        if (zeroes[i] != 0) {
            console_write("heap: calloc test failed\n");
            halt_forever();
        }
    }

    console_printf("heap: test alloc a=%x b=%x first=%x last=%x free_before=%u free_after=%u\n",
        (uint64_t)numbers,
        (uint64_t)zeroes,
        numbers[0],
        numbers[7],
        before,
        heap_free_bytes());

    kfree(numbers);
    kfree(zeroes);

    console_printf("heap: test free complete free=%u used=%u\n",
        heap_free_bytes(),
        heap_used_bytes());
}

static void test_vmm(void) {
    uint64_t physical = pmm_alloc_frame();
    uint64_t mapped = 0xffffc00290000000ull;
    uint64_t translated = 0;

    if (physical == 0) {
        console_write("vmm: test frame allocation failed\n");
        halt_forever();
    }

    if (!vmm_map_page(mapped, physical, VMM_PAGE_WRITABLE | VMM_PAGE_NO_EXECUTE)) {
        console_write("vmm: test mapping failed\n");
        halt_forever();
    }

    volatile uint64_t *slot = (volatile uint64_t *)mapped;
    *slot = 0x737276726f73564dull;

    if (*slot != 0x737276726f73564dull ||
        !vmm_virt_to_phys(mapped, &translated) ||
        translated != physical) {
        console_write("vmm: test mapping verification failed\n");
        halt_forever();
    }

    console_printf("vmm: test map virt=%x phys=%x value=%x\n",
        mapped,
        physical,
        *slot);
}

static void load_initramfs(void) {
    if (module_request.response == NULL || module_request.response->module_count == 0) {
        initramfs_init(NULL, 0);
        return;
    }

    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file *module = module_request.response->modules[i];
        if (module->string != NULL && module->string[0] == 'i') {
            initramfs_init(module->address, module->size);
            return;
        }
    }

    struct limine_file *module = module_request.response->modules[0];
    initramfs_init(module->address, module->size);
}

static void mount_exfat_volume(void) {
    struct block_device *ahci = block_find("ahci0");
    if (ahci != NULL && exfat_mount_block_device("/fat", ahci)) {
        return;
    }

    const struct vfs_node *node = vfs_lookup("/srvros.exfat");
    const uint8_t *data;
    uint64_t size;

    if (node == NULL || !vfs_read_all(node, &data, &size)) {
        console_write("exfat: demo image not found\n");
        return;
    }

    struct block_device *device = block_register_memory("initramfs-exfat", data, size, 512);
    if (device == NULL || !exfat_mount_block_device("/fat", device)) {
        console_write("exfat: demo block mount failed\n");
    }
}

void kmain(void) {
    serial_init();
    console_write("\n\nsrvros: entering kernel\n");

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        console_write("limine: requested base revision is unsupported\n");
        halt_forever();
    }

    if (framebuffer_request.response != NULL &&
        framebuffer_request.response->framebuffer_count > 0) {
        console_init(framebuffer_request.response->framebuffers[0]);
    }

    if (bootloader_info_request.response != NULL) {
        console_printf("bootloader: %s %s\n",
            bootloader_info_request.response->name,
            bootloader_info_request.response->version);
    }

    if (hhdm_request.response != NULL) {
        console_printf("hhdm offset: %x\n", hhdm_request.response->offset);
    }

    block_init();
    arch_init();
    arch_test_breakpoint();
    pmm_init(memmap_request.response,
        hhdm_request.response != NULL ? hhdm_request.response->offset : 0);
    acpi_init(rsdp_request.response != NULL ? rsdp_request.response->address : NULL);
    load_initramfs();
    vfs_init();
    test_pmm();
    heap_init();
    test_heap();
    scheduler_init();
    workqueue_init();
    if (!scheduler_start_demo_worker()) {
        console_write("scheduler: demo worker failed\n");
    }
    vmm_init();
    lapic_init();
    lapic_timer_init(32, 10000000);
    keyboard_init();
    bool mouse_ready = mouse_init();
    ioapic_init();
    ioapic_route_isa_irq(1, 33);
    if (ioapic_route_isa_irq(4, 36)) {
        serial_enable_interrupts();
    }
    if (mouse_ready) {
        ioapic_route_isa_irq(12, 44);
    }
    pci_init();
    ahci_init();
    mount_exfat_volume();
    e1000_init();
    if (e1000_available()) {
        uint8_t irq = e1000_irq_line();
        if (ioapic_route_pci_irq(irq, 32 + irq)) {
            e1000_enable_interrupts();
        }
    }
    net_init();
    if (!net_start_worker()) {
        console_write("net: worker not started\n");
    }
    test_vmm();
    arch_enable_interrupts();
    if (timer_wait_ticks_bounded(5, 100000000)) {
        console_printf("timer: observed ticks=%u\n", timer_ticks());
    } else {
        console_printf("timer: no ticks observed yet ticks=%u\n", timer_ticks());
    }

    if (framebuffer_request.response != NULL &&
        framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
        console_printf("framebuffer: %ux%u pitch=%u bpp=%u\n",
            fb->width,
            fb->height,
            fb->pitch,
            (uint64_t)fb->bpp);
    } else {
        console_write("framebuffer: unavailable\n");
    }

    print_memmap();

    console_write("srvros: kernel bootstrap complete\n");
    monitor_run();
}
