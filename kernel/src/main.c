#include <srvros/intel_display_hda.h>
#include <srvros/intel_hda_owner.h>
#include <srvros/sync_timing.h>
#include <srvros/ap_probe.h>
#include <srvros/intel_boot_claim.h>
#include <srvros/intel_handoff.h>
#include <srvros/intel_gt_preflight.h>
#include <srvros/intel_gt_firmware.h>
#include <srvros/intel_gt_boot.h>
#include <srvros/intel_memory_trial.h>
#include <srvros/haswell_backing.h>
#include <srvros/haswell_firmware.h>
#include <srvros/haswell_cache_context.h>
#include <srvros/ecam_boot.h>
#include <srvros/air61_window.h>
#ifdef SRVROS_AIR61_READ_QUALIFICATION
#include <srvros/air61_read_qualification.h>
#endif
#include <srvros/posix_capture.h>
#include <srvros/acpi.h>
#include <srvros/autotest.h>
#include <srvros/workspace.h>
#include <srvros/workspace_image.h>
#include <srvros/workspace_log.h>
#include <srvros/battery_diagnostic.h>
#include <srvros/desktop_session.h>
#include <srvros/laptop_probe.h>
#include <srvros/intel_display_capture.h>
#include <srvros/intel_dma_observe.h>
#include <srvros/x86_cache.h>
#include <srvros/intel_ggtt_window.h>
#include <srvros/intel_display_backing.h>
#include <srvros/intel_scanout_trial.h>
#include <srvros/audio.h>
#include <srvros/capture_write_timing.h>
#include <srvros/audio_abi.h>
#include <srvros/audio_stream_abi.h>
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
#include <srvros/installed_root.h>
#include <srvros/intel_gfx.h>
#include <srvros/virtio_gpu_probe.h>
#include <srvros/virtio_input.h>
#include <srvros/virtio_rng.h>
#include <srvros/virtio_net.h>
#include <srvros/virtio_block.h>
#include <srvros/display_present.h>
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
#include <srvros/vfs_namespace.h>
#include <srvros/vmm.h>
#include <srvros/workqueue.h>
#include <srvros/xhci.h>

#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boot_system_init.inc"

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

static bool virtio_service_ready(void *arg) {
    (void)arg; return virtio_input_ready() || virtio_rng_ready();
}
static bool virtio_poll_due(void *arg) { return timer_ticks()>=*(uint64_t *)arg; }
static void virtio_service(void *arg) {
    (void)arg;
    struct scheduler_wait_queue wake={0};
    for(;;) {
        /* Service threads own kernel CR3. Exclude IRQ reentry while publishing
         * device events and while the syscall consumes the private RNG pool. */
        uint64_t flags;
        __asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
        virtio_input_poll();
        virtio_rng_poll();
        if(flags&(1ull<<9)) __asm__ volatile("sti":::"memory");
        uint64_t due=timer_ticks()+1;
        if(!scheduler_wait_timeout(&wake,virtio_poll_due,&due,due)) scheduler_yield();
    }
}

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

static void mount_exfat_volume(bool diagnostic_ram) {
    struct block_device *disk = diagnostic_ram ? NULL : block_find("ahci0");
    if (!disk && !diagnostic_ram) disk=block_find("virtio0");
    if (disk != NULL && exfat_mount_block_device("/fat", disk)) {
        return;
    }
    if (disk != NULL) {
        block_register_gpt_partitions(disk);
        uint64_t order[] = {
            2, 1, 3, 4, 5, 6, 7, 8,
            9, 10, 11, 12, 13, 14, 15, 16,
            17, 18, 19, 20, 21, 22, 23, 24,
            25, 26, 27, 28, 29, 30, 31, 32,
        };
        for (uint64_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
            char name[16];
            partition_name(name, sizeof(name), disk->name, order[i]);
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
    struct block_device *device = diagnostic_ram
        ? block_register_ram_copy("posix-scratch", data, size)
        : block_register_memory("initramfs-exfat", data, size, 512);
    if(diagnostic_ram) console_printf("posix-capture: scratch_copy=%s bytes=%u volatile=yes\n",device?"ready":"FAILED",size);
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

/* Explicit listening capture only. Five immutable six-second channel fixtures;
 * no disk stress, desktop or firmware probing. Failed retirement retains state. */
static void audio_listen_pause(void) {
    static struct scheduler_wait_queue wait;
    uint64_t tick=timer_ticks()+1;
    if(!scheduler_wait_timeout(&wait,idle_capture_due,&tick,tick))scheduler_yield();
}
static bool audio_listen_control(unsigned op,unsigned value) {
    if(audio_request(op,value))return false;
    uint64_t until=timer_monotonic_us()+2000000;
    struct srv_audio_status s;
    do {audio_status(&s);if(!s.pending)return s.state==SRV_AUDIO_READY && !s.error;
        audio_listen_pause();}while(timer_monotonic_us()<until);
    return false;
}
static bool audio_listen_capture(void) {
    struct srv_audio_status s;audio_status(&s);
    if(s.state!=SRV_AUDIO_READY || s.pending || (s.capabilities&SRV_AUDIO_CAP_PCM_BUSY)) {
        console_write("audio-listen: REFUSED output unavailable/busy\n");return false;
    }
    console_write("Audio listening check: five 6-second LEFT / RIGHT / STEREO cycles at25%.\n"
        "Listen only. Built-in quiet gaps are intentional. Automatic Ubuntu return.\n");
    bool ok=audio_listen_control(SRV_AUDIO_OUTPUT,SRV_AUDIO_SPEAKERS) &&
        audio_listen_control(SRV_AUDIO_VOLUME,25);
    uint64_t begin=timer_monotonic_us(),deadline=begin+35000000,bytes=0,underruns=0;
    unsigned completed=0;
    for(unsigned i=0;ok && i<5;i++) {
        console_printf("audio-listen: play=%u/5 seconds=6 volume=25\n",(uint64_t)i+1);
        int64_t pid=process_spawn_background_elf_args_quiet("/fat/bin/playwav","/fat/share/examples/stereo.wav");
        if(pid<1){ok=false;break;}
        uint64_t status=1;int64_t result;
        while((result=process_wait((uint64_t)pid,&status,true))==0 && timer_monotonic_us()<deadline)audio_listen_pause();
        if(result==0) {
            (void)audio_request(SRV_AUDIO_STOP,0);
            (void)process_signal_target(pid,9);
            uint64_t retire=timer_monotonic_us()+5000000;
            while((result=process_wait((uint64_t)pid,&status,true))==0 && timer_monotonic_us()<retire)audio_listen_pause();
            ok=false;
        }
        if(result==0) {
            console_write("audio-listen: HALTED player retirement incomplete; live state retained\n");
            console_sync();__asm__ volatile("cli":::"memory");halt_forever();
        }
        struct srv_pcm_status pcm;uint64_t owner;
        audio_stream_snapshot(&pcm,&owner);
        ok=ok && result==pid && status==0 && owner==0 && pcm.state==SRV_PCM_DONE &&
            pcm.submitted_bytes==1152000 && pcm.underruns==0;
        bytes+=pcm.submitted_bytes;underruns+=pcm.underruns;
        if(ok)completed++;
    }
    bool stopped=audio_listen_control(SRV_AUDIO_STOP,0);
    bool quiet=stopped && audio_listen_control(SRV_AUDIO_VOLUME,1);
    uint64_t elapsed=timer_monotonic_us()-begin;
    ok=ok && completed==5 && elapsed>=30000000 && elapsed<35000000 && quiet;
    console_printf("audio-listen: result=%s plays=%u seconds=30 volume=25 bytes=%u underruns=%u elapsed_us=%u stopped=%s quiet=%s\n",
        ok?"PASS":"FAIL",(uint64_t)completed,bytes,underruns,elapsed,stopped?"yes":"NO",quiet?"yes":"NO");
    audio_print_status();return ok;
}


/* Explicit boot-only diagnostic, before runtime drivers or user processes.
 * The secondary VirtIO device never becomes the inherited display owner. */
static void boot_virtio_gpu_probe(void) {
    uint64_t physical=0;
    if(!framebuffer_request.response || !framebuffer_request.response->framebuffer_count ||
       !vmm_virt_to_phys((uint64_t)framebuffer_request.response->framebuffers[0]->address,&physical)) {
        console_write("virtio-gpu-probe: SKIP status=no-inherited-framebuffer\n");
        return;
    }
    struct virtio_gpu_probe_result result={0};
    bool ok=virtio_gpu_probe_run(physical,&result);
    console_printf("virtio-gpu-probe: %s status=%s width=%u height=%u fence=%u reset=%u quarantined=%u\n",
        ok?"PASS":result.status==VGPU_PROBE_UNAVAILABLE?"SKIP":"FAIL",
        virtio_gpu_probe_status_name(result.status),(uint64_t)result.width,(uint64_t)result.height,
        (uint64_t)result.fence_ok,(uint64_t)result.reset_ok,(uint64_t)result.quarantined);
}

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

#ifdef SRVROS_AIR61_READ_QUALIFICATION
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
    if(!cmdline_has_token(cmdline,"srvros.vtd-window=air61-two-pass-read-only")){console_write("Register sample kernel requires explicit read-only token\n");halt_forever();}
#else
    if(!cmdline_has_token(cmdline,"srvros.vtd-window=air61-two-pass-no-read")){console_write("Two-pass kernel requires explicit qualification token\n");halt_forever();}
#endif
#endif
#ifdef SRVROS_AP_PROBE
    if(!cmdline_has_token(cmdline,"srvros.capture=haswell-backing") &&
       !cmdline_has_token(cmdline,"srvros.capture=haswell-firmware") &&
       !cmdline_has_token(cmdline,"srvros.capture=ap-ownership") &&
       !(cmdline_has_token(cmdline,"srvros.capture=cpu-load") && cmdline_has_token(cmdline,"srvros.ap=c3-test"))) {
        console_write("ap-owner: dedicated diagnostic requires capture token; memory retained\n");
        halt_forever();
    }
#endif
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
    test_pmm();
    heap_init();
    test_heap();
    /* VFS imports may grow beyond the bootstrap node block. */
    vfs_init();
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
    bool window_requested=cmdline_has_token(cmdline,"srvros.vtd-window=air61-no-read");
#ifdef SRVROS_AIR61_READ_QUALIFICATION
    if(window_requested)halt_forever();
    const bool window_test=false;
#else
    bool window_test=window_requested;
#endif
    bool ecam_test=cmdline_has_token(cmdline,"srvros.ecam=air61-2m-test");
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
    if(cmdline_has_token(cmdline,"srvros.vtd-window=air61-two-pass-no-read"))halt_forever();
    bool read_test=cmdline_has_token(cmdline,"srvros.vtd-window=air61-two-pass-read-only");
#else
    if(cmdline_has_token(cmdline,"srvros.vtd-window=air61-two-pass-read-only"))halt_forever();
    bool read_test=cmdline_has_token(cmdline,"srvros.vtd-window=air61-two-pass-no-read");
#endif
    if(read_test && (window_test || !ecam_test))halt_forever();
#ifndef SRVROS_AIR61_READ_QUALIFICATION
    if(read_test){console_write("Two-pass qualification requires its isolated kernel\n");halt_forever();}
#endif
    if(window_test && !ecam_test)halt_forever();
    if(ecam_test) {
#ifdef SRVROS_AP_PROBE
        if(!cmdline_has_token(cmdline,"srvros.capture=haswell-firmware") || !cmdline_has_token(cmdline,"srvros.hsw-cache=alias-nx-preinit"))haswell_ecam_boot_fail_closed("invalid opt-in profile");
        boot_status("ecam-boot-progress: fresh pre-PCI admission; no user input required\n");
        bool prepared=haswell_ecam_boot_prepare();
        if((prepared && !haswell_ecam_boot_handoff()) || vmm_ecam_boot_result()->state==ECAM_POISONED)haswell_ecam_boot_fail_closed("pre-PCI publication failed");
#else
        boot_set_framebuffer_quiet(false);console_write("ECAM test requires the isolated diagnostic kernel\n");halt_forever();
#endif
    }
    pci_init();
    bool intel_display_capture_requested=cmdline_has_token(cmdline,"srvros.capture=intel-display");
    unsigned intel_client=intel_display_capture_requested?INTEL_BOOT_OBSERVER:
        (cmdline_has_token(cmdline,"srvros.capture=haswell-backing") ||
         cmdline_has_token(cmdline,"srvros.capture=haswell-firmware"))?INTEL_BOOT_BLOCKED:INTEL_BOOT_LEGACY;
    if(!intel_boot_select(intel_client))console_write("intel-boot-claim: policy selection refused\n");
    if(intel_display_capture_requested) {
        struct limine_framebuffer_response *r=framebuffer_request.response;
        intel_display_probe_boot(r && r->framebuffer_count && r->framebuffers?r->framebuffers[0]:NULL);
        if(cmdline_has_token(cmdline,"srvros.intel-backing=read-only"))
            intel_display_backing_probe_boot(r && r->framebuffer_count && r->framebuffers?r->framebuffers[0]:NULL);
    }
    if(intel_display_capture_requested && cmdline_has_token(cmdline,"srvros.intel-dma=read-only")) intel_dma_probe_boot();
    if(intel_display_capture_requested && cmdline_has_token(cmdline,"srvros.intel-cache=read-only")) intel_cache_probe_boot();
    bool virtio_baseline=cmdline_has_token(cmdline,"srvros.virtio=on");
    if(virtio_baseline) {
        bool input=virtio_input_init();
        bool rng=virtio_rng_init();
        console_printf("virtio: input=%s rng=%s\n",input?"ready":"unavailable",rng?"ready":"unavailable");
        if((input || rng) && !scheduler_spawn_io_service("virtio-input",virtio_service,0,virtio_service_ready)) {
            console_write("virtio: input/rng worker failed\n");
            virtio_input_stop();
            virtio_rng_stop();
        }
    }
    if (cmdline_has_token(cmdline, "srvros.virtio-gpu=present")) {
        struct limine_framebuffer_response *r = framebuffer_request.response;
        bool ready = r && r->framebuffer_count && display_present_prepare(r->framebuffers[0]);
        console_printf("virtio-gpu-present: %s\n", ready ? "ready" : "unavailable");
    }
    if(cmdline_has_token(cmdline,"srvros.virtio-gpu=probe")) boot_virtio_gpu_probe();
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
    if(!intel_display_capture_requested && !cmdline_has_token(cmdline,"srvros.capture=haswell-backing") && !cmdline_has_token(cmdline,"srvros.capture=haswell-firmware")) intel_gfx_init();
    else if(intel_display_capture_requested) console_write("intel-display-observe: ordinary Intel initializer skipped; no capture PCI writes\n");
    console_printf("intel-boot-claim: v=1 policy=%u client=%u immutable=1 hardware_ownership=0 managed_pages=0\n",
        (uint64_t)intel_boot_policy(),(uint64_t)intel_boot_claimed_client());
    if(cmdline_has_token(cmdline,"srvros.sensors=cpu")) thermal_init();
    if(cmdline_has_token(cmdline,"srvros.power=read-only")) power_init();
    if (cmdline_has_token(cmdline, "srvros.storage=ram")) {
        console_write("storage: RAM-only mode; AHCI disabled\n");
    } else {
        ahci_init();
        if(virtio_baseline) {
            virtio_block_init();
            virtio_block_print_status();
        }
    }
    struct block_device *installed_volume = NULL;
    const char *installed_reason = NULL;
    enum installed_root_result installed = installed_root_select(cmdline,
        &installed_volume, &installed_reason);
    if (installed == INSTALLED_ROOT_SELECTED) {
        if (!exfat_mount_block_device_clean("/fat", installed_volume)) {
            installed = INSTALLED_ROOT_INVALID;
            installed_reason = "filesystem-recovery";
        } else if (!exfat_is_writable("/fat")) {
            installed = INSTALLED_ROOT_INVALID;
            installed_reason = "filesystem-recovery";
        } else {
            console_printf("installed-root: selected device=%s format=exfat-v1 role=system\n",
                installed_volume->name);
        }
    }
    if (installed == INSTALLED_ROOT_INVALID) {
        /* Recovery is deliberately before boot logs, init, workspace services
         * or the general monitor (whose commands can mount and write disks). */
        boot_set_framebuffer_quiet(false);
        console_clear();
        console_printf("installed-root: recovery reason=%s\n", installed_reason);
        console_write("SrvrOS could not select the installed system.\n"
            "No applications were started. Automatic disk writes are stopped.\n"
            "Check the boot manifest and attached disks, then restart this VM.\n"
            "No fallback volume or repair was selected.\n");
        halt_forever();
    }
    if (installed == INSTALLED_ROOT_LEGACY)
        mount_exfat_volume(cmdline_has_token(cmdline,"srvros.capture=posix") &&
            cmdline_has_token(cmdline,"srvros.storage=ram"));
    persist_bootlog();
    console_write(vfs_namespace_system_activate()?
        "namespace: /bin /usr /var /home active; /fat /work retained\n":
        "namespace: activation refused; legacy storage paths retained\n");
    e1000_init();
    if(virtio_baseline) {
        virtio_net_init();
        virtio_net_print_status();
        virtio_rng_print_status();
    }
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
#ifdef SRVROS_AP_PROBE
    if(timer_running && cmdline_has_token(cmdline,"srvros.capture=haswell-firmware") &&
       cmdline_has_token(cmdline,"srvros.hsw-cache=alias-nx-preinit"))
    {
        uint64_t window_irq=0;
        if(window_test || read_test){
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
            boot_status("sample-progress: two bounded read-only register samples; no input needed\n");
#else
            boot_status("window-progress: fresh bounded mapping/retirement, zero target loads; no input needed\n");
#endif
            __asm__ volatile("pushfq; popq %0; cli":"=r"(window_irq)::"memory");
        }
        bool aliases_ready=haswell_alias_context_prepare();
        if(window_test || read_test){
#ifdef SRVROS_AIR61_READ_QUALIFICATION
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
            if(read_test && aliases_ready)(void)haswell_air61_register_sample();
#else
            if(read_test && aliases_ready)(void)haswell_air61_read_qualify();
#endif
#endif
            if(window_test && aliases_ready)(void)haswell_air61_window_run();
            if(window_irq&512)__asm__ volatile("sti":::"memory");
            boot_status("window-progress: safe outcome recorded; saving results and returning to Ubuntu\n");
        }
    }
#endif
    bool init_ready = boot_system_init(cmdline, timer_running);
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
    bool home_transitions_battery=cmdline_has_token(cmdline,"srvros.capture=battery-home-transitions");
    bool home_work_battery=cmdline_has_token(cmdline,"srvros.capture=battery-home-work");
    bool home_desktop_battery=home_work_battery || home_transitions_battery || cmdline_has_token(cmdline,"srvros.capture=battery-home-desktop");
    bool home_session_battery=cmdline_has_token(cmdline,"srvros.capture=battery-home-session");
    bool home_battery=home_desktop_battery || home_session_battery || cmdline_has_token(cmdline,"srvros.capture=battery-home");
    bool smbus_files_battery=cmdline_has_token(cmdline,"srvros.capture=battery-smbus-files");
    bool smbus_transitions_battery=cmdline_has_token(cmdline,"srvros.capture=battery-smbus-transitions");
    bool smbus_battery=smbus_transitions_battery || cmdline_has_token(cmdline,"srvros.capture=battery-smbus");
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
    bool automatic_audio_listen=cmdline_has_token(cmdline,"srvros.capture=audio-listen-30");
    bool automatic_posix=cmdline_has_token(cmdline,"srvros.capture=posix");
    bool automatic_storage_wait=cmdline_has_token(cmdline,"srvros.capture=storage-wait");
    bool automatic_hfw=cmdline_has_token(cmdline,"srvros.capture=haswell-firmware");
    bool automatic_hsw=cmdline_has_token(cmdline,"srvros.capture=haswell-backing");
    bool automatic_ap=cmdline_has_token(cmdline,"srvros.capture=ap-ownership");
    bool automatic_load=cmdline_has_token(cmdline,"srvros.capture=cpu-load");
    bool automatic_aml=cmdline_has_token(cmdline,"srvros.capture=aml");
    bool automatic_desktop_idle=cmdline_has_token(cmdline,"srvros.capture=desktop-idle");
    bool automatic_idle=cmdline_has_token(cmdline,"srvros.capture=idle");
    bool automatic_probe=cmdline_has_token(cmdline,"srvros.laptop-services=probe-autoreboot");
    bool return_on_failure=cmdline_has_token(cmdline,"srvros.capture-failure=reboot");
    if(automatic_probe && cmdline_has_token(cmdline,"srvros.battery-preflight=air61")) laptop_battery_preflight();
    else if(automatic_probe || cmdline_has_token(cmdline,"srvros.laptop-services=probe")) laptop_probe();
    if(cmdline_has_token(cmdline,"srvros.laptop-services=brightness")) laptop_brightness_init();
    if(cmdline_has_token(cmdline,"srvros.audio=hda")) audio_init();
    if(!automatic_aml && cmdline_has_token(cmdline,"srvros.aml=probe")) aml_probe_run(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
    if(cmdline_has_token(cmdline,"srvros.smc=read-only")) smc_init();
    if(cmdline_has_token(cmdline,"srvros.capture-write-timing=on")) {
        console_printf("capture-write-timing: enabled=%s capacity=8\n",process_capture_write_timing_enable()?"yes":"NO");
        console_printf("sync-timing: enabled=%s capacity=8\n",sync_timing_enable()?"yes":"NO");
    }
    workspace_boot(cmdline);
    if (automatic_probe) {
        /* Capture enumeration/setup failures even if firmware left a misleading
         * PS/2-ready flag. This unattended profile must not require typing. */
        xhci_print_boot_diagnostic();
        xhci_print_status();
        xhci_print_input_summary();
    }
    if(automatic_audio_listen || intel_display_capture_requested || automatic_probe || automatic_idle || automatic_aml || automatic_desktop_idle || automatic_battery || automatic_load || automatic_storage_wait || automatic_posix || automatic_ap || automatic_hsw || automatic_hfw) {
        console_printf("laptop-capture: boot options=%s\n",cmdline);
        bool started=workspace_active() && workspace_log_start();
        if(started) console_write("laptop-capture: diagnostics collected; physical input acceptance remains pending\n");
        bool sampled=!automatic_audio_listen && !automatic_idle && !automatic_aml && !automatic_desktop_idle && !automatic_battery && !automatic_load && !automatic_storage_wait && !automatic_posix && !automatic_ap && !automatic_hsw && !automatic_hfw;
        if(intel_display_capture_requested) {
            sampled=intel_display_probe_report();
            bool backing_ok=intel_display_backing_probe_report();sampled=sampled && backing_ok;
            if(cmdline_has_token(cmdline,"srvros.intel-memory=backing") || cmdline_has_token(cmdline,"srvros.intel-memory=fixture")) {
                struct limine_framebuffer_response *r=framebuffer_request.response;
                console_write("intel-memory: saving prerequisite checkpoint before private allocation\n");
                bool checkpoint=started && workspace_log_pause();
                if(checkpoint)console_write("intel-memory: preflight checkpoint saved\n");
                else console_write("intel-memory: checkpoint failed; allocation refused\n");
                bool memory_ok=checkpoint && intel_memory_trial(r && r->framebuffer_count && r->framebuffers?r->framebuffers[0]:NULL,cmdline);
                /* Fixture identity refusal is expected; native keeps its full
                 * actual display/backing observation prerequisites. */
                sampled=cmdline_has_token(cmdline,"srvros.intel-memory=fixture")?memory_ok:(sampled && memory_ok);
            }
            bool dma_ok=intel_dma_probe_report();sampled=sampled && dma_ok;
            bool cache_ok=intel_cache_probe_report();sampled=sampled && cache_ok;
            if(cmdline_has_token(cmdline,"srvros.intel-window=read-only")){bool window_ok=intel_ggtt_window_probe_report();sampled=sampled && window_ok;}
            if(cmdline_has_token(cmdline,"srvros.intel-handoff=qualify")){bool handoff_ok=started && intel_handoff_probe_report();sampled=sampled && handoff_ok;}
            if(cmdline_has_token(cmdline,"srvros.intel-gt-preflight=read-only")){bool gt_ok=started && intel_gt_preflight_probe_report();sampled=sampled && gt_ok;}
            if(cmdline_has_token(cmdline,"srvros.intel-firmware=read-only")){bool fw_ok=started && intel_gt_firmware_probe_report();sampled=sampled && fw_ok;}
            if(cmdline_has_token(cmdline,"srvros.intel-wake=trial")) {
                bool held_reset=cmdline_has_token(cmdline,"srvros.intel-bcs-reset=held-blitter-only");
#ifdef SRVROS_INTEL_GPU_HOLD
                if(cmdline_has_token(cmdline,"srvros.intel-gpu-requester=hold"))console_write("intel-gpu-hold: automatic original-source PCI requester inhibit, pending-drain and exact restoration. No input needed. Saved safe outcomes return to Ubuntu; uncertainty halts.\n");
#endif
                if(cmdline_has_token(cmdline,"srvros.intel-ring=reset-v1"))console_write("intel-reset: automatic original-owner entry/reset; no input needed. Only zero geometry may reset. Safe saved outcomes return; uncertainty halts. No memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-ring=entry-v1"))console_write("intel-ring-entry: automatic state-aware original-owner entry; no input needed. Enabled shutdown and already-disabled observation are reported separately. Safe saved outcomes return; uncertainty halts. No reset or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-ring=handoff-v1"))console_write("intel-ring: automatic original-owner enabled-ring transition; no input needed. Safe saved outcomes return; retained uncertainty halts. No reset or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-admission=disabled-bcs-v1"))console_write("intel-compose: automatic original-owner disabled BCS observation, then existing reset admission. No input needed; safe saved outcomes return; retained uncertainty halts. No enabled shutdown or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-joint=hda-claim"))console_write("intel-joint: automatic HDA inhibit, original GPU claim and restoration. No input needed. Safe saved outcomes return to Ubuntu; retained uncertainty halts visibly.\n");
                else if(cmdline_has_token(cmdline,"srvros.intel-rehold=request-cancel"))console_write("intel-rehold: automatic post-reset BCS re-arm, original-entry child, restore and preservation. No input needed, no PTE write or memory grant. Safe saved outcomes return to Ubuntu; retained uncertainty halts visibly.\n");
                else if(cmdline_has_token(cmdline,"srvros.intel-interval=read-only"))console_write("intel-interval: automatic original GGTT entry capture inside held reset, then preservation. No input needed, no PTE writes or memory grant. Safe saved outcomes return to Ubuntu; uncertainty halts visibly.\n");
                else if(held_reset)console_write("intel-held-reset: automatic five-engine hold, PCI capture, BCS8 reset and preservation; no input needed. Safe saved outcomes return to Ubuntu; uncertain state halts visibly.\n");
                if(!held_reset && cmdline_has_token(cmdline,"srvros.intel-held-requester=read-only"))console_write("intel-held-requester: automatic PCI AF capture under maintained five-engine preparation; no input needed, no reset or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-requester=read-only"))console_write("intel-requester: automatic read-only PCI AF requester capture; no input needed, no BME/FLR/reset or memory grant.\n");
                if(!held_reset && cmdline_has_token(cmdline,"srvros.intel-engines-prepare=request-cancel"))console_write("intel-prepare: automatic five-engine prepare, hold and cancellation; no input needed, no reset or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-takeover=preflight"))console_write("intel-takeover: automatic read-only reset-control prerequisites; no input needed, no reset or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-hda-bcs=joint-owner-v1"))console_write("intel-hda-bcs: automatic original-owner HDA inhibit, BCS shutdown and preservation, then HDA restore. No input needed. Safe saved outcomes return to Ubuntu; retained uncertainty halts visibly.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-shutdown=qualify"))console_write("intel-shutdown: automatic idle blitter shutdown and fresh engine/reader checks; no input needed, no reset or memory grant. Unsafe state requires manual restart.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-bcs-reset=blitter-only"))console_write("intel-bcs: automatic BCS-only reset with preservation; no input needed. Unsafe state halts here; photograph then restart manually.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-reset-ready=request-cancel"))console_write("intel-reset-ready: automatic bounded BCS request/ready/cancel; no input needed; no actual reset or memory grant.\n");
                if(cmdline_has_token(cmdline,"srvros.intel-completion=read-only"))console_write("intel-completion: automatic engine/flip-wait/cache prerequisites; no input required, no invalidation/reset or memory grant.\n");
                console_write("intel-wake: saving preflight checkpoint; no input needed. If this screen stops, photograph it and restart manually.\n");
                bool checkpoint=started && workspace_log_pause();
                if(checkpoint)console_write("intel-wake: preflight checkpoint saved\n");
                else console_write("intel-wake: checkpoint failed; wake refused\n");
                bool wake_ok=checkpoint && intel_gt_wake_probe_report();sampled=sampled && wake_ok;
            }
            if(cmdline_has_token(cmdline,"srvros.intel-hda=read-only")) {
                console_write("intel-hda: saving preflight checkpoint; automatic read-only census, no input needed. Photograph any halt.\n");
                bool checkpoint=started && workspace_log_pause();
                if(checkpoint)console_write("intel-hda: preflight checkpoint saved\n");
                bool hda_ok=checkpoint && intel_display_hda_probe_report();sampled=sampled && hda_ok;
            }
            if(cmdline_has_token(cmdline,"srvros.intel-hda=owner-trial") || cmdline_has_token(cmdline,"srvros.intel-hda=lease-trial")) {
                bool lease=cmdline_has_token(cmdline,"srvros.intel-hda=lease-trial");
                if(cmdline_has_token(cmdline,"srvros.intel-readers=read-only"))console_write("intel-pinned: automatic TXP and three-pipe/stereo audit while HDA inhibited; no input required. Saving results before return.\n");
                console_printf("%s: saving preflight; automatic requester inhibit/restore, no input needed. Uncertain state halts on this screen; photograph it.\n",lease?"intel-hda-lease":"intel-hda-owner");
                bool checkpoint=started && workspace_log_pause();
                if(checkpoint)console_printf("%s: preflight checkpoint saved\n",lease?"intel-hda-lease":"intel-hda-owner");
                bool hda_ok=checkpoint && intel_display_hda_owner_report();sampled=sampled && hda_ok;
            }
            bool fixture=cmdline_has_token(cmdline,"srvros.intel-lifetime=fixture");
            if(fixture || cmdline_has_token(cmdline,"srvros.intel-lifetime=shadow")) {
                struct limine_framebuffer_response *r=framebuffer_request.response;
                bool lifetime=started && init_ready && timer_running && intel_scanout_lifetime_trial(r && r->framebuffer_count && r->framebuffers?r->framebuffers[0]:NULL,fixture);
                sampled=fixture?lifetime:(sampled && lifetime);
            }
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
        if(home_battery && !home_desktop_battery && started) {
            battery_prepared=workspace_log_pause() && battery_diagnostic_auto_return(false);
            console_printf("laptop-capture: battery Home mounted=%s logging-paused=%s fault-reset=disabled\n",
                workspace_active()?"yes":"NO",battery_prepared?"yes":"NO");
        }
        if(automatic_battery && battery_prepared && started && init_ready && timer_running) {
            sampled=(smbus_transitions_battery?battery_smbus_transition_diagnostic_run:home_work_battery?battery_home_work_diagnostic_run:home_transitions_battery?battery_home_transitions_diagnostic_run:home_desktop_battery?battery_home_desktop_diagnostic_run:home_session_battery?battery_home_session_diagnostic_run:home_battery?battery_home_diagnostic_run:smbus_files_battery?battery_smbus_files_diagnostic_run:smbus_battery?battery_smbus_diagnostic_run:query_battery?battery_query_diagnostic_run:controls_battery?battery_controls_diagnostic_run:app_files_battery?battery_app_files_diagnostic_run:load_battery?battery_load_diagnostic_run:text_battery?battery_text_diagnostic_run:desktop_battery?battery_desktop_diagnostic_run:filesystem_battery?battery_filesystem_diagnostic_run:usb_write_battery?battery_usb_write_diagnostic_run:usb_read_battery?battery_usb_read_diagnostic_run:service_battery?battery_service_diagnostic_run:guided_battery?battery_transition_diagnostic_run:battery_diagnostic_run)(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
            console_printf("%s: returned=yes clean=%s; physical accuracy NOT_TESTED\n",smbus_transitions_battery?"battery-smbus-transitions-capture":home_work_battery?"battery-home-work-capture":home_transitions_battery?"battery-home-transitions-capture":home_desktop_battery?"battery-home-desktop-capture":home_session_battery?"battery-home-session-capture":home_battery?"battery-home-capture":smbus_files_battery?"battery-smbus-files-capture":smbus_battery?"battery-smbus-capture":query_battery?"battery-query-capture":controls_battery?"battery-controls-capture":app_files_battery?"battery-app-files-capture":load_battery?"battery-load-capture":text_battery?"battery-text-capture":desktop_battery?"battery-desktop-capture":service_battery?"battery-service-capture":guided_battery?"battery-transition-capture":"battery-capture",sampled?"yes":"NO");
        }
        if(automatic_battery && return_on_failure) {
            if(filesystem_battery&&workspace_image_busy())(void)workspace_image_close();
            (void)battery_diagnostic_auto_return(false);
            if(workspace_parked&&!workspace_active())workspace_boot(cmdline);
        }
        if(automatic_audio_listen && started && init_ready && timer_running)sampled=audio_listen_capture();
        if(automatic_posix && started && init_ready && timer_running) sampled=posix_capture_run();
        if(automatic_storage_wait && started && init_ready && timer_running) {
            sampled=workspace_storage_wait_capture();
            console_printf("storage-wait-capture: returned=yes clean=%s\n",sampled?"yes":"NO");
        }
        if(automatic_hfw && started && init_ready && timer_running) {
#ifdef SRVROS_AP_PROBE
            if(ecam_test) {
                bool quiet=workspace_log_quiesce(5000);
                const char *directory=workspace_log_directory();
                bool proof=quiet && haswell_ecam_boot_report(directory);
                console_printf("laptop-capture: boot options=%s\n",cmdline);
                bool saved=quiet && workspace_log_ecam_snapshot();
                bool window_proof=true,window_saved=true;
                if(window_test){window_proof=haswell_air61_window_report(directory);window_saved=workspace_log_window_snapshot();}
#ifdef SRVROS_AIR61_READ_QUALIFICATION
                if(read_test){window_proof=haswell_air61_read_report(directory);window_saved=workspace_log_window_snapshot();}
#endif
                bool aliases=haswell_alias_context_report();
                if(window_test)haswell_air61_window_reference(directory,window_saved);
#ifdef SRVROS_AIR61_READ_QUALIFICATION
                if(read_test)haswell_air61_read_reference(directory,window_saved);
#endif
                haswell_ecam_boot_reference(directory,saved);
                console_printf("laptop-capture: boot options=%s\n",cmdline);
                sampled=proof && saved && aliases && window_proof && window_saved;
            } else sampled=cmdline_has_token(cmdline,"srvros.hsw-cache=alias-nx-preinit")?haswell_alias_context_report():cmdline_has_token(cmdline,"srvros.hsw-cache=alias-nx")?haswell_alias_context_run():cmdline_has_token(cmdline,"srvros.hsw-cache=context-only")?haswell_cache_context_run():haswell_firmware_run();
#else
            console_write("hsw-firmware: ordinary-kernel refused\n");sampled=false;
#endif
        }
        if(automatic_hsw && started && init_ready && timer_running) {
#ifdef SRVROS_AP_PROBE
            sampled=platform_haswell_backing_run(cmdline_has_token(cmdline,"srvros.hsw-cpu-control=read-only"),cmdline_has_token(cmdline,"srvros.hsw-fences=linux-model-32"));
#else
            console_write("hsw-backing: ordinary-kernel refused\n");sampled=false;
#endif
        }
        if(automatic_ap && started && init_ready && timer_running) {
#ifdef SRVROS_AP_PROBE
            sampled=ap_probe_run(cmdline);
#else
            console_write("ap-owner: v=1 admitted=0 reason=ordinary-kernel\n");
            sampled=false;
#endif
        }
        if(automatic_load && started && init_ready && timer_running) {
            bool deep=cmdline_has_token(cmdline,"srvros.idle=mwait-c3-test");
            bool mwait=deep || cmdline_has_token(cmdline,"srvros.idle=mwait-wakeup-test");
            bool coord=cmdline_has_token(cmdline,"srvros.ap=c3-test");
            bool owned=!coord;
#ifdef SRVROS_AP_PROBE
            if(coord)owned=deep && ap_probe_run(cmdline);
#endif
            bool captured=owned && (deep?power_capture_deep_start():mwait?power_capture_mwait_start():power_capture_start());
            if(mwait && !captured) console_printf("cpu-mwait: v=%u admitted=0 fallback=hlt\n",(uint64_t)(deep?2:1));
            sampled=process_run_elf_args("/fat/bin/threadstress","--cpu-load-capture");
            power_capture_stop();
#ifdef SRVROS_AP_PROBE
            if(coord){bool retired=ap_c3_stop();sampled=sampled && retired;}
#endif
            if(mwait) sampled=sampled && captured && power_capture_mwait_passed();
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
        /* The new read-only observation may save a handled refusal and return,
         * but never resets past a failed log save or workspace unmount. */
        if(!intel_gt_wake_retained() && !intel_display_hda_owner_retained() && !intel_memory_retained() &&
           (!intel_display_capture_requested || (saved && unmounted)) &&
           (return_on_failure || (saved && (sampled || automatic_load) && unmounted))) {
            console_write("laptop-capture: returning to Ubuntu through normal firmware/GRUB boot\n");
            console_sync();timer_delay_us(250000);
            if(return_on_failure)arch_reboot_unattended();
            arch_reboot();
        }
        console_write("laptop-capture: HALTED - photograph this screen; restart manually\n");
        console_sync();__asm__ volatile("cli":::"memory");halt_forever();
    }
    trackpad_test_boot(cmdline);
    autotest_run(cmdline, init_ready && timer_running);
    if (cmdline_has_token(cmdline, "srvros.desktop=session")) {
        console_printf("desktop-session: providers brightness=%u audio=%u thermal=%u smc=%u ethernet=%u; kernel services persist across desktop exit\n",
            (uint64_t)cmdline_has_token(cmdline,"srvros.laptop-services=brightness"),
            (uint64_t)cmdline_has_token(cmdline,"srvros.audio=hda"),
            (uint64_t)cmdline_has_token(cmdline,"srvros.sensors=cpu"),
            (uint64_t)cmdline_has_token(cmdline,"srvros.smc=read-only"),
            (uint64_t)cmdline_has_token(cmdline,"srvros.usb-network=ecm"));
        if(cmdline_has_token(cmdline,"srvros.battery=on") && cmdline_has_token(cmdline,"srvros.desktop-power=on")) {
            bool ok=desktop_battery_power_session_run();
            bool unmounted=workspace_active() && workspace_unmount();
            console_printf("desktop-power: result=%s unmounted=%s; returning to Ubuntu\n",ok?"PASS":"FAIL",unmounted?"yes":"NO");
            if(unmounted){console_sync();arch_reboot_unattended();}
            console_write("desktop-power: HALTED unmount failed; restart manually\n");
            console_sync();__asm__ volatile("cli":::"memory");halt_forever();
        }
        if(cmdline_has_token(cmdline,"srvros.battery=on") &&
           (cmdline_has_token(cmdline,"srvros.desktop-audio-io=on") || cmdline_has_token(cmdline,"srvros.desktop-audio-io=smoke"))) {
            bool ok=desktop_battery_audio_io_session_run(cmdline_has_token(cmdline,"srvros.desktop-audio-io=smoke"));
            bool unmounted=workspace_active() && workspace_unmount();
            console_printf("desktop-audio-io: result=%s unmounted=%s; returning to Ubuntu\n",ok?"PASS":"FAIL",unmounted?"yes":"NO");
            if(unmounted){console_sync();arch_reboot_unattended();}
            console_write("desktop-audio-io: HALTED unmount failed; restart manually\n");
            console_sync();__asm__ volatile("cli":::"memory");halt_forever();
        }
        bool capacity_test=cmdline_has_token(cmdline,"srvros.desktop-capacity-test=on");
        if(cmdline_has_token(cmdline,"srvros.battery=on") && (capacity_test || cmdline_has_token(cmdline,"srvros.desktop-reopen-test=on"))) {
            bool ok=true;
            for(unsigned round=1;round<=2;round++) {
                console_printf("desktop-reopen-test: round=%u begin\n",(uint64_t)round);
                bool complete=capacity_test?desktop_battery_capacity_session_run():desktop_battery_reopen_session_run();
                console_printf("desktop-reopen-test: round=%u complete=%s\n",(uint64_t)round,complete?"yes":"NO");
                ok=ok&&complete;
            }
            bool unmounted=workspace_active() && workspace_unmount();
            console_printf("desktop-reopen-test: result=%s unmounted=%s; returning to Ubuntu\n",ok?"PASS":"FAIL",unmounted?"yes":"NO");
            if(unmounted){console_sync();arch_reboot_unattended();}
            console_write("desktop-reopen-test: HALTED unmount failed; restart manually\n");
            console_sync();__asm__ volatile("cli":::"memory");halt_forever();
        }
        else if(cmdline_has_token(cmdline,"srvros.battery=on")) {
            if(cmdline_has_token(cmdline,"srvros.desktop-audio-io=attended") || cmdline_has_token(cmdline,"srvros.desktop-audio-io=attended-smoke"))
                desktop_battery_audio_io_attended_run(cmdline_has_token(cmdline,"srvros.desktop-audio-io=attended-smoke"));
            else if(cmdline_has_token(cmdline,"srvros.desktop-guide=window-navigation"))desktop_battery_navigation_session_run();
            else if(cmdline_has_token(cmdline,"srvros.desktop-guide=application-lifecycle"))desktop_battery_lifecycle_session_run();
            else desktop_battery_session_run();
        }
        else console_write("desktop-session: REFUSED explicit srvros.battery=on required\n");
    } else if (cmdline_has_token(cmdline, "srvros.desktop=trial")) {
        if (!workspace_active()) {
            console_write("desktop-trial: USB workspace unavailable; trial not started. Photograph this screen.\n");
        } else {
            console_write("desktop-trial: opening the native desktop checklist; no timed exit.\n");
            console_printf("desktop-trial: boot options=%s\n",cmdline);
            if(!workspace_log_start()) {
                console_write("desktop-trial: NOT STARTED; diagnostics unavailable. Archive old logs in Ubuntu and retry.\n");
                bool unmounted=workspace_unmount();
                console_printf("desktop-trial: USB unmounted=%s\n",unmounted?"yes":"NO");
                if(unmounted) console_write("Type reboot to return to Ubuntu.\n");
                else console_write("USB sync/unmount failed. Photograph this screen before restarting.\n");
            } else {
                /* Device checks retain ECM but start Help; legacy network trials keep their script. */
                bool network_trial=cmdline_has_token(cmdline,"srvros.usb-network=ecm") &&
                    !cmdline_has_token(cmdline,"srvros.desktop-check=devices");
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
    }
    monitor_run();
}
