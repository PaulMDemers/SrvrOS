#include <srvros/acpi.h>
#include <srvros/autotest.h>
#include <srvros/workspace.h>
#include <srvros/workspace_image.h>
#include <srvros/workspace_log.h>
#include <srvros/battery_diagnostic.h>
#include <srvros/laptop_probe.h>
#include <srvros/audio.h>
#include <srvros/thermal.h>
#include <srvros/power.h>
#include <srvros/smc.h>
#include <srvros/aml_probe.h>
#include <srvros/trackpad_test.h>
#include <srvros/ahci.h>
#include <srvros/arch.h>
#include <srvros/block.h>
#include <srvros/bootlog.h>
#include <srvros/console.h>
#include <srvros/e1000.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/halt.h>
#include <srvros/initramfs.h>
#include <srvros/intel_gfx.h>
#include <srvros/keyboard.h>
#include <srvros/lpss_spi.h>
#include <srvros/monitor.h>
#include <srvros/mouse.h>
#include <srvros/net.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vmm.h>
#include <srvros/workqueue.h>
#include <srvros/xhci.h>

#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern volatile uint64_t limine_base_revision[];
extern volatile struct limine_bootloader_info_request bootloader_info_request;
extern volatile struct limine_executable_cmdline_request executable_cmdline_request;
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

static bool boot_verbose;

static bool char_is_separator(char c) {
    return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Explicit, bounded physical typing trial. Never consume serial input here. */
static void keyboard_capture(void) {
    static const struct { const char *prompt, *expected; } steps[] = {
        {"Press a (lowercase)", "a"},
        {"Hold Shift and press a, then release both", "A"},
        {"Press Caps Lock, release it, then press a", "A"},
        {"Press Caps Lock again, release it, then press a", "a"},
        {"Press Backspace (the Mac Delete key)", "\b"},
        {"Press Enter / Return", "\n"},
        {"Press Up arrow", "\x1b[A"}, {"Press Down arrow", "\x1b[B"},
        {"Press Left arrow", "\x1b[D"}, {"Press Right arrow", "\x1b[C"},
    };
    console_clear();
    console_write("SRVROS KEYBOARD TEST - use the built-in keyboard only\n"
        "Follow one instruction at a time. Release each key promptly.\n"
        "No shell commands needed. Each step allows 90 seconds.\n"
        "Wrong key? Try again. Only a timeout ends the test.\n"
        "Results are saved automatically; do not power off.\n\n");
    char c;
    while (keyboard_try_read_char(&c)) {}
    uint64_t dropped = keyboard_dropped_count();
    unsigned passed = 0;
    uint64_t retries = 0;
    bool ok = timer_reference_ready();
    for (unsigned i = 0; ok && i < sizeof(steps)/sizeof(steps[0]); i++) {
        console_printf("keyboard-test: step=%u %s\n", (uint64_t)i + 1, steps[i].prompt);
        console_sync();
        uint64_t deadline = timer_monotonic_us() + 90000000;
        unsigned n = 0;
        while (steps[i].expected[n] && timer_monotonic_us() < deadline) {
            if (keyboard_try_read_char(&c)) {
                if (c != steps[i].expected[n]) {
                    retries++;
                    console_printf("keyboard-test: retry step=%u received=%u; %s\n",
                        (uint64_t)i + 1, (uint64_t)(uint8_t)c, steps[i].prompt);
                    /* Restart escape-sequence matching, retaining a new ESC. */
                    n = c == steps[i].expected[0] ? 1 : 0;
                } else n++;
            } else scheduler_yield();
        }
        if (steps[i].expected[n]) {
            ok = false;
            console_printf("keyboard-test: timeout step=%u limit_seconds=90\n", (uint64_t)i + 1);
        }
        if (ok) { passed++; console_write("keyboard-test: step PASS\n"); }
    }
    ok = ok && passed == sizeof(steps)/sizeof(steps[0]) && keyboard_dropped_count() == dropped;
    console_printf("keyboard-test: result=%s passed=%u total=10 dropped=%u\n",
        ok ? "PASS" : "FAIL", (uint64_t)passed, keyboard_dropped_count() - dropped);
    console_printf("keyboard-test: retries=%u\n", retries);
    xhci_print_input_summary();
    console_write("Keyboard test finished. Saving results and returning to Ubuntu...\n");
}

static bool cmdline_has_token(const char *cmdline, const char *token) {
    if (cmdline == NULL || token == NULL || token[0] == '\0') {
        return false;
    }

    uint64_t token_len = 0;
    while (token[token_len] != '\0') {
        token_len++;
    }

    const char *p = cmdline;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }

        const char *start = p;
        uint64_t len = 0;
        while (!char_is_separator(p[len])) {
            len++;
        }

        if (len == token_len) {
            bool match = true;
            for (uint64_t i = 0; i < len; i++) {
                if (start[i] != token[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }

        p += len;
    }
    return false;
}

static const char *kernel_cmdline(void) {
    if (executable_cmdline_request.response == NULL) {
        return "";
    }
    const char *cmdline = executable_cmdline_request.response->cmdline;
    return cmdline != NULL ? cmdline : "";
}

static bool boot_debug_requested(const char *cmdline) {
    return cmdline_has_token(cmdline, "debug") ||
        cmdline_has_token(cmdline, "srvros.debug") ||
        cmdline_has_token(cmdline, "srvros.debug=1") ||
        cmdline_has_token(cmdline, "srvros.log=debug") ||
        cmdline_has_token(cmdline, "srvros.log=trace") ||
        cmdline_has_token(cmdline, "srvros.log=verbose");
}

static bool boot_a1466_capture_requested(const char *cmdline) {
    return cmdline_has_token(cmdline, "srvros.a1466.capture") ||
        cmdline_has_token(cmdline, "srvros.a1466.capture=1") ||
        cmdline_has_token(cmdline, "srvros.capture=a1466") ||
        cmdline_has_token(cmdline, "a1466.capture");
}

static void boot_set_framebuffer_quiet(bool quiet) {
    console_set_framebuffer_muted(quiet);
}

static void boot_status(const char *message) {
    bool muted = console_framebuffer_muted();
    console_set_framebuffer_muted(false);
    console_write(message);
    console_set_framebuffer_muted(muted);
}

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

static void partition_name(char *out, uint64_t out_size, const char *base, uint64_t number) {
    uint64_t pos = 0;
    while (base[pos] != '\0' && pos + 1 < out_size) {
        out[pos] = base[pos];
        pos++;
    }
    if (pos + 1 < out_size) {
        out[pos++] = 'p';
    }

    char digits[20];
    uint64_t digit_count = 0;
    do {
        digits[digit_count++] = (char)('0' + (number % 10));
        number /= 10;
    } while (number != 0 && digit_count < sizeof(digits));

    while (digit_count > 0 && pos + 1 < out_size) {
        out[pos++] = digits[--digit_count];
    }
    out[pos] = '\0';
}

static void mount_exfat_volume(void) {
    struct block_device *ahci = block_find("ahci0");
    if (ahci != NULL && exfat_mount_block_device("/fat", ahci)) {
        return;
    }
    if (ahci != NULL) {
        block_register_gpt_partitions(ahci);
        uint64_t order[] = {
            2, 1, 3, 4, 5, 6, 7, 8,
            9, 10, 11, 12, 13, 14, 15, 16,
            17, 18, 19, 20, 21, 22, 23, 24,
            25, 26, 27, 28, 29, 30, 31, 32,
        };
        for (uint64_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
            char name[16];
            partition_name(name, sizeof(name), "ahci0", order[i]);
            struct block_device *partition = block_find(name);
            if (partition != NULL && exfat_mount_block_device("/fat", partition)) {
                return;
            }
        }
    }

    console_write("exfat: trying initramfs fallback /srvros.exfat\n");
    const struct vfs_node *node = vfs_lookup("/srvros.exfat");
    const uint8_t *data;
    uint64_t size;

    if (node == NULL || !vfs_read_all(node, &data, &size)) {
        console_write("exfat: initramfs fallback /srvros.exfat not found\n");
        return;
    }

    console_printf("exfat: fallback image size=%u bytes\n", size);
    struct block_device *device = block_register_memory("initramfs-exfat", data, size, 512);
    if (device == NULL) {
        console_write("exfat: fallback block registration failed\n");
        return;
    }
    if (!exfat_mount_block_device("/fat", device)) {
        console_write("exfat: fallback block mount failed\n");
    }
}

static void persist_bootlog(void) {
    exfat_create_directory("/fat/var");
    exfat_create_directory("/fat/var/log");
    if (bootlog_persist("/fat/var/log/boot.log")) {
        console_write("bootlog: persisted /fat/var/log/boot.log\n");
    } else {
        console_write("bootlog: persist skipped\n");
    }
}

static bool idle_capture_due(void *arg) { return timer_ticks()>=*(uint64_t *)arg; }

void kmain(void) {
    serial_init();
    console_write("\n\nsrvros: entering kernel\n");
    const char *cmdline = kernel_cmdline();
    boot_verbose = boot_debug_requested(cmdline);
    bool a1466_capture = boot_a1466_capture_requested(cmdline);

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        console_write("limine: requested base revision is unsupported\n");
        halt_forever();
    }

    if (framebuffer_request.response != NULL &&
        framebuffer_request.response->framebuffer_count > 0) {
        console_init(framebuffer_request.response->framebuffers[0]);
        boot_set_framebuffer_quiet(!boot_verbose);
        if (!boot_verbose) {
            boot_status("srvros: booting (quiet mode)\n");
        }
    }

    if (cmdline[0] != '\0') {
        console_printf("cmdline: %s\n", cmdline);
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
    if (!scheduler_start_idle_worker()) {
        console_write("scheduler: idle worker failed\n");
    }
    vmm_init();
    timer_reference_init();
    timer_wall_init();
    lapic_init();
    lapic_timer_init(32, 10000000);
    bool keyboard_ready = keyboard_init();
    bool mouse_ready = keyboard_ready ? mouse_init() : false;
    ioapic_init();
    if (keyboard_ready) {
        ioapic_route_isa_irq(1, 33);
    }
    if (ioapic_route_isa_irq(4, 36)) {
        serial_enable_interrupts();
    }
    if (mouse_ready) {
        ioapic_route_isa_irq(12, 44);
    }
    pci_init();
    /* SPI transport is experimental; this laptop's active input is USB. */
    bool lpss_spi_ready = false;
    if (cmdline_has_token(cmdline, "srvros.spi=experimental")) {
        lpss_spi_ready = lpss_spi_init();
    } else {
        console_write("lpss-spi: disabled (experimental transport)\n");
    }
    xhci_set_storage_writable(cmdline_has_token(cmdline, "srvros.usb-storage=rw"));
    bool gestures=cmdline_has_token(cmdline,"srvros.trackpad=gestures");
    xhci_set_trackpad_gestures(gestures);
    xhci_set_trackpad_pointer(gestures || cmdline_has_token(cmdline, "srvros.trackpad=pointer"));
    xhci_set_trackpad_capture(cmdline_has_token(cmdline, "srvros.trackpad=capture"));
    xhci_set_network_enabled(cmdline_has_token(cmdline,"srvros.usb-network=ecm"));
    xhci_set_wifi_probe_enabled(cmdline_has_token(cmdline,"srvros.usb-wifi=probe"));
    xhci_set_wifi_mac_startup(cmdline_has_token(cmdline,"srvros.capture=wifi-firmware") &&
        cmdline_has_token(cmdline,"srvros.wifi-mac-startup=1"));
    xhci_set_wifi_firmware_enabled(cmdline_has_token(cmdline,"srvros.capture=wifi-firmware"));
    xhci_init();
    console_printf("input: ps2_keyboard=%s ps2_mouse=%s usb_keyboards=%u usb_mice=%u usb_devices=%u usb_hubs=%u\n",
        keyboard_ready ? "yes" : "no",
        mouse_ready ? "yes" : "no",
        xhci_keyboard_count(),
        xhci_mouse_count(),
        xhci_device_count(),
        xhci_hub_count());
    keyboard_print_status();
    if (xhci_device_count() == 0 || (!keyboard_ready && xhci_keyboard_count() == 0)) {
        console_write("input: no ps2/usb keyboard path; pci input/xhci diagnostics follow\n");
        console_write("input: lpss spi/topcase diagnostics follow\n");
        lpss_spi_print_status();
        acpi_print_input_diagnostics();
        pci_print_input_bus_candidates();
        pci_print_usb_controllers();
        xhci_print_status();
    }
    intel_gfx_init();
    if(cmdline_has_token(cmdline,"srvros.sensors=cpu")) thermal_init();
    if(cmdline_has_token(cmdline,"srvros.power=read-only")) power_init();
    if (cmdline_has_token(cmdline, "srvros.storage=ram")) {
        console_write("storage: RAM-only mode; AHCI disabled\n");
    } else {
        ahci_init();
    }
    mount_exfat_volume();
    persist_bootlog();
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
    console_enable_deferred();
    if (lpss_spi_ready && !lpss_spi_start_worker()) {
        console_write("lpss-spi: polling worker not started\n");
    }
    bool timer_running = timer_wait_ticks_bounded(5, 100000000);
    if (timer_running) {
        scheduler_enable_io_services();
        console_printf("timer: observed ticks=%u\n", timer_ticks());
    } else {
        console_printf("timer: no ticks observed yet ticks=%u\n", timer_ticks());
    }
    keyboard_print_status();
    xhci_print_input_summary();
    if (cmdline_has_token(cmdline, "srvros.inputprobe=1")) {
        xhci_probe_input(2000);
    }
    keyboard_print_status();
    xhci_print_input_summary();

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

    if (boot_verbose) {
        print_memmap();
    }

    console_write("srvros: kernel bootstrap complete\n");
    bool init_ready = false;
    int64_t init_pid = process_spawn_background_elf_args_quiet("/init", "--system");
    if (init_pid >= 0) {
        (void)process_set_reaper((uint64_t)init_pid);
        console_printf("init: started pid=%u\n", (uint64_t)init_pid);
        uint64_t init_start = timer_monotonic_us();
        bool ready = timer_running && process_wait_startup((uint64_t)init_pid, 300);
        init_ready = ready;
        console_printf("init: readiness=%s elapsed_us=%u\n", ready ? "ready" : (timer_running ? "timeout-or-exit" : "clock-unavailable"),
            timer_monotonic_us() - init_start);
    } else {
        console_write("init: start failed\n");
    }
    keyboard_print_status();
    xhci_print_input_summary();
    if (!boot_verbose) {
        boot_set_framebuffer_quiet(false);
        console_clear();
        console_write("srvros: boot complete\n");
        if (a1466_capture) {
            console_write("diagnostics: running automatic A1466 capture\n");
        } else {
            console_write("diagnostics: use dmesg, bootinfo, or reboot with srvros.log=debug\n\n");
        }
    }
    if (a1466_capture) {
        monitor_run_a1466_capture();
        persist_bootlog();
        console_write("\n");
    }
    if (cmdline_has_token(cmdline, "srvros.inputdiag=1")) {
        console_clear();
        xhci_print_boot_diagnostic();
    }
    bool guided_battery=cmdline_has_token(cmdline,"srvros.capture=battery-transitions");
    bool home_session_battery=cmdline_has_token(cmdline,"srvros.capture=battery-home-session");
    bool home_battery=home_session_battery || cmdline_has_token(cmdline,"srvros.capture=battery-home");
    bool smbus_files_battery=cmdline_has_token(cmdline,"srvros.capture=battery-smbus-files");
    bool smbus_battery=cmdline_has_token(cmdline,"srvros.capture=battery-smbus");
    bool query_battery=cmdline_has_token(cmdline,"srvros.capture=battery-query");
    bool controls_battery=cmdline_has_token(cmdline,"srvros.capture=battery-controls");
    bool app_files_battery=cmdline_has_token(cmdline,"srvros.capture=battery-app-files");
    bool load_battery=cmdline_has_token(cmdline,"srvros.capture=battery-load");
    bool text_battery=cmdline_has_token(cmdline,"srvros.capture=battery-text");
    bool desktop_battery=cmdline_has_token(cmdline,"srvros.capture=battery-desktop");
    bool filesystem_battery=smbus_files_battery||app_files_battery||load_battery||cmdline_has_token(cmdline,"srvros.capture=battery-filesystem");
    bool usb_write_battery=cmdline_has_token(cmdline,"srvros.capture=battery-usb-write");
    bool usb_read_battery=cmdline_has_token(cmdline,"srvros.capture=battery-usb-read");
    bool service_battery=filesystem_battery||usb_write_battery||usb_read_battery||cmdline_has_token(cmdline,"srvros.capture=battery-service");
    bool automatic_battery=home_battery||smbus_battery||query_battery||controls_battery||text_battery||desktop_battery||service_battery||guided_battery||cmdline_has_token(cmdline,"srvros.capture=battery");
    bool automatic_load=cmdline_has_token(cmdline,"srvros.capture=cpu-load");
    bool automatic_aml=cmdline_has_token(cmdline,"srvros.capture=aml");
    bool automatic_desktop_idle=cmdline_has_token(cmdline,"srvros.capture=desktop-idle");
    bool automatic_idle=cmdline_has_token(cmdline,"srvros.capture=idle");
    bool automatic_wifi_firmware=cmdline_has_token(cmdline,"srvros.capture=wifi-firmware");
    bool automatic_wifi_calibration=cmdline_has_token(cmdline,"srvros.capture=wifi-calibration");
    bool automatic_wifi=automatic_wifi_firmware || automatic_wifi_calibration || cmdline_has_token(cmdline,"srvros.capture=wifi");
    bool automatic_probe=cmdline_has_token(cmdline,"srvros.laptop-services=probe-autoreboot");
    bool return_on_failure=cmdline_has_token(cmdline,"srvros.capture-failure=reboot");
    if(automatic_probe || cmdline_has_token(cmdline,"srvros.laptop-services=probe")) laptop_probe();
    if(cmdline_has_token(cmdline,"srvros.laptop-services=brightness")) laptop_brightness_init();
    if(cmdline_has_token(cmdline,"srvros.audio=hda")) audio_init();
    if(!automatic_aml && cmdline_has_token(cmdline,"srvros.aml=probe")) aml_probe_run(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
    if(cmdline_has_token(cmdline,"srvros.smc=read-only")) smc_init();
    workspace_boot(cmdline);
    if (automatic_probe || automatic_wifi) {
        /* Capture enumeration/setup failures even if firmware left a misleading
         * PS/2-ready flag. This unattended profile must not require typing. */
        xhci_print_boot_diagnostic();
        xhci_print_status();
        xhci_print_input_summary();
    }
    if(automatic_probe || automatic_idle || automatic_aml || automatic_desktop_idle || automatic_battery || automatic_load || automatic_wifi) {
        console_printf("laptop-capture: boot options=%s\n",cmdline);
        bool started=workspace_active() && workspace_log_start();
        if(started) console_write("laptop-capture: diagnostics collected; physical input acceptance remains pending\n");
        bool sampled=!automatic_idle && !automatic_aml && !automatic_desktop_idle && !automatic_battery && !automatic_load;
        if(automatic_wifi) {
            xhci_print_status();
            sampled=xhci_wifi_identity_ready();
            if(automatic_wifi_firmware) {
                const struct initramfs_file *firmware=initramfs_find("/firmware/mt7610e.bin");
                sampled=started && init_ready && timer_running && sampled &&
                    xhci_wifi_capture_firmware(firmware?firmware->data:0,firmware?firmware->size:0);
                if(sampled && cmdline_has_token(cmdline,"srvros.wifi-bbp-batch=1"))
                    sampled=xhci_wifi_capture_bbp_batch();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-bbp-agc=1"))
                    sampled=xhci_wifi_capture_bbp_agc();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-bbp-front=1"))
                    sampled=xhci_wifi_capture_bbp_front();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-bbp-core=1"))
                    sampled=xhci_wifi_capture_bbp_core();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-bbp-ready=1"))
                    sampled=xhci_wifi_capture_bbp_ready();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-mac-format=1"))
                    sampled=xhci_wifi_capture_mac_format();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-mac-basics=1"))
                    sampled=xhci_wifi_capture_mac_basics();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-timing-probe=1"))
                    sampled=xhci_wifi_capture_timing_probe();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-pair-probe=1"))
                    sampled=xhci_wifi_capture_pair_probe();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-write-probe=1"))
                    sampled=xhci_wifi_capture_write_probe();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-radio-reset=1"))
                    sampled=xhci_wifi_capture_radio_reset();
                else if(sampled && cmdline_has_token(cmdline,"srvros.wifi-mcu-probe=1"))
                    sampled=xhci_wifi_capture_mcu();
                console_printf("wifi-firmware-capture: completed=%s; RF and connectivity NOT_TESTED\n",sampled?"yes":"NO");
            }
            if(automatic_wifi_calibration) {
                sampled=started && init_ready && timer_running && sampled && xhci_wifi_capture_calibration();
                console_printf("wifi-calibration-capture: completed=%s; calibration validity and connectivity NOT_TESTED\n",sampled?"yes":"NO");
            }
            console_printf("wifi-capture: identity=%s; connectivity NOT_TESTED\n",xhci_wifi_identity_ready()?"read":"unavailable");
        }
        bool battery_prepared=true,workspace_parked=false;
        if(automatic_battery && !home_battery && return_on_failure && started) {
            console_write("laptop-capture: preparing unattended battery probe; missing final report means failure\n");
            battery_prepared=workspace_log_pause() && (filesystem_battery?workspace_image_open():usb_write_battery?workspace_scratch_prepare():workspace_unmount());
            workspace_parked=workspace_parked_device()!=NULL;
            battery_prepared=battery_prepared && battery_diagnostic_auto_return(true);
            console_printf("laptop-capture: battery media parked=%s fault-reset=%s\n",
                workspace_parked?"yes":"NO",battery_prepared?"armed":"NO");
        }
        if(home_battery && started) {
            battery_prepared=workspace_log_pause() && battery_diagnostic_auto_return(false);
            console_printf("laptop-capture: battery Home mounted=%s logging-paused=%s fault-reset=disabled\n",
                workspace_active()?"yes":"NO",battery_prepared?"yes":"NO");
        }
        if(automatic_battery && battery_prepared && started && init_ready && timer_running) {
            sampled=(home_session_battery?battery_home_session_diagnostic_run:home_battery?battery_home_diagnostic_run:smbus_files_battery?battery_smbus_files_diagnostic_run:smbus_battery?battery_smbus_diagnostic_run:query_battery?battery_query_diagnostic_run:controls_battery?battery_controls_diagnostic_run:app_files_battery?battery_app_files_diagnostic_run:load_battery?battery_load_diagnostic_run:text_battery?battery_text_diagnostic_run:desktop_battery?battery_desktop_diagnostic_run:filesystem_battery?battery_filesystem_diagnostic_run:usb_write_battery?battery_usb_write_diagnostic_run:usb_read_battery?battery_usb_read_diagnostic_run:service_battery?battery_service_diagnostic_run:guided_battery?battery_transition_diagnostic_run:battery_diagnostic_run)(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
            console_printf("%s: returned=yes clean=%s; physical accuracy NOT_TESTED\n",home_session_battery?"battery-home-session-capture":home_battery?"battery-home-capture":smbus_files_battery?"battery-smbus-files-capture":smbus_battery?"battery-smbus-capture":query_battery?"battery-query-capture":controls_battery?"battery-controls-capture":app_files_battery?"battery-app-files-capture":load_battery?"battery-load-capture":text_battery?"battery-text-capture":desktop_battery?"battery-desktop-capture":service_battery?"battery-service-capture":guided_battery?"battery-transition-capture":"battery-capture",sampled?"yes":"NO");
        }
        if(automatic_battery && return_on_failure) {
            if(filesystem_battery&&workspace_image_busy())(void)workspace_image_close();
            (void)battery_diagnostic_auto_return(false);
            if(workspace_parked&&!workspace_active())workspace_boot(cmdline);
        }
        if(automatic_load && started && init_ready && timer_running) {
            sampled=process_run_elf_args("/fat/bin/threadstress","--cpu-load-capture");
            console_printf("cpu-load-capture: returned=yes process_ok=%s\n",sampled?"yes":"NO");
            thermal_print_status();smc_print_status();
        }
        if (started && cmdline_has_token(cmdline,"srvros.capture=keyboard")) keyboard_capture();
        if(automatic_desktop_idle && started && init_ready && timer_running) {
            sampled=process_run_elf_args("/fat/bin/deskd","--idle-capture");
            console_printf("desktop-idle-capture: returned=yes clean=%s\n",sampled?"yes":"NO");
            thermal_print_status();smc_print_status();audio_print_status();
        }
        if(automatic_aml && started && init_ready && timer_running) {
            sampled=aml_probe_run(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
            console_printf("aml-capture: returned=yes clean=%s; isolated snapshot probe only\n",sampled?"yes":"NO");
        }
        if(automatic_idle && started && init_ready && timer_running) {
            struct scheduler_stats before,after;
            static struct scheduler_wait_queue idle_capture_wait;
            scheduler_snapshot(&before);
            uint64_t start=timer_monotonic_us(),deadline=timer_ticks()+6000;
            console_write("idle-capture: measuring 60 seconds; no user input required\n");
            /* Block the bootstrap thread; yielding here would measure our own
             * busy loop instead of the otherwise idle system. */
            bool waited=true;
            while(timer_ticks()<deadline) {
                if(!scheduler_wait_timeout(&idle_capture_wait,idle_capture_due,&deadline,deadline)) {
                    waited=false;break;
                }
            }
            uint64_t elapsed=timer_monotonic_us()-start;
            scheduler_snapshot(&after);
            sampled=waited && elapsed>=59000000 && after.sampled_ticks>before.sampled_ticks;
            console_printf("idle-capture: elapsed_us=%u sampled_ticks=%u idle_ticks=%u switches=%u preemptions=%u completed=%s\n",
                elapsed,after.sampled_ticks-before.sampled_ticks,after.idle_ticks-before.idle_ticks,
                after.switches-before.switches,after.preemptions-before.preemptions,sampled?"yes":"NO");
            thermal_print_status();smc_print_status();audio_print_status();
        }
        console_printf("laptop-capture: completed=%s\n",sampled?"yes":"NO");
        bool saved=started && workspace_log_stop();
        bool unmounted=workspace_active() && workspace_unmount();
        console_printf("laptop-capture: saved=%s unmounted=%s\n",saved?"yes":"NO",unmounted?"yes":"NO");
        if(return_on_failure || (saved && (sampled || automatic_load) && unmounted)) {
            console_write("laptop-capture: returning to Ubuntu through normal firmware/GRUB boot\n");
            console_sync();timer_delay_us(250000);
            if(return_on_failure)arch_reboot_unattended();
            arch_reboot();
        }
        if (automatic_wifi_firmware) {
            console_printf("wifi-held: logging=%u init=%u timer=%u sampled=%u saved=%u unmounted=%u\n",
                (uint64_t)started,(uint64_t)init_ready,(uint64_t)timer_running,
                (uint64_t)sampled,(uint64_t)saved,(uint64_t)unmounted);
            console_write("Wi-Fi diagnostic stopped. Photograph this screen, including USB errors above.\n");
            console_write("Hold the power button until off, then power on for Ubuntu. No commands needed.\n");
        }
        console_write("laptop-capture: HALTED - photograph this screen; restart manually\n");
        console_sync();__asm__ volatile("cli":::"memory");halt_forever();
    }
    trackpad_test_boot(cmdline);
    autotest_run(cmdline, init_ready && timer_running);
    if (cmdline_has_token(cmdline, "srvros.desktop=trial")) {
        if (!workspace_active()) {
            console_write("desktop-trial: USB workspace unavailable; trial not started. Photograph this screen.\n");
        } else {
            console_write("desktop-trial: opening the native desktop checklist; no timed exit.\n");
            console_printf("desktop-trial: boot options=%s\n",cmdline);
            (void)workspace_log_start();
            bool network_trial=cmdline_has_token(cmdline,"srvros.usb-network=ecm");
            bool scaled=cmdline_has_token(cmdline,"srvros.desktop-scale=2");
            bool exited = process_run_elf_args("/fat/bin/deskd", network_trial ?
                (scaled?"--network-trial --scale=2":"--network-trial") :
                (scaled?"--hardware-trial --scale=2":"--hardware-trial"));
            audio_print_status();
            thermal_print_status();
            smc_print_status();
            xhci_print_input_summary(); /* Include gesture counters in the saved exit snapshot. */
            bool logs_saved=workspace_log_stop();
            console_printf("desktop-trial: diagnostics saved=%s\n",logs_saved?"yes":"NO");
            bool unmounted = workspace_unmount();
            console_printf("desktop-trial: desktop returned=%s USB unmounted=%s\n", exited?"yes":"NO", unmounted?"yes":"NO");
            console_write("This is a manual trial, not an automated pass. Photograph any failure.\n");
            if (unmounted) console_write("When finished, type reboot to return to Ubuntu.\n");
            else console_write("USB sync/unmount failed. Photograph this screen before restarting.\n");
        }
    }
    monitor_run();
}
