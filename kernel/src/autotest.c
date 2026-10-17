/* Explicit, single-use USB test requests. Normal boots never enable writes. */
#include <srvros/autotest.h>
#include <srvros/autotest_storage.h>
#include <srvros/autotest_shell.h>
#include <srvros/process.h>
#include <srvros/arch.h>
#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/fpu.h>
#include <srvros/keyboard.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/usb_storage.h>
#include <srvros/vfs.h>
#include <srvros/xhci.h>

static char report[2048];
static unsigned report_size;
static bool report_overflow;

static bool separator(char c) { return c == 0 || c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool hex_option(const char *cmdline, const char *key, char *out, unsigned digits) {
    unsigned key_length = 0, matches = 0;
    while (key[key_length]) key_length++;
    for (const char *p = cmdline; *p;) {
        while (*p && separator(*p)) p++;
        const char *end = p;
        while (*end && !separator(*end)) end++;
        unsigned n = 0;
        while (n < key_length && p + n < end && p[n] == key[n]) n++;
        if (n == key_length) {
            if ((unsigned)(end - p) != key_length + digits || ++matches > 1) return false;
            for (unsigned i = 0; i < digits; i++) {
                char c = p[key_length + i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
                out[i] = c;
            }
            out[digits] = 0;
        }
        p = end;
    }
    return matches == 1;
}

static uint32_t hex_value(const char *text) {
    uint32_t n = 0;
    for (unsigned i = 0; i < 8; i++) n = n * 16 + (text[i] <= '9' ? text[i] - '0' : text[i] - 'a' + 10);
    return n;
}
static void append(char *out, unsigned *length, const char *text) {
    while (*text) out[(*length)++] = *text++;
    out[*length] = 0;
}
static void make_path(char out[80], const char *run, const char *suffix) {
    unsigned n = 0;
    append(out, &n, "/testlog/srvros-tests/");
    append(out, &n, run);
    append(out, &n, suffix);
}
static void field(const char *key, const char *value) {
    unsigned n = 0;
    while (key[n]) n++;
    unsigned v = 0;
    while (value[v]) v++;
    if (report_size + n + v + 2 >= sizeof(report)) { report_overflow = true; return; }
    append(report, &report_size, key);
    append(report, &report_size, "=");
    append(report, &report_size, value);
    append(report, &report_size, "\n");
}
static void number(const char *key, uint64_t n) {
    char reversed[20], value[21];
    unsigned count = 0, used = 0;
    do { reversed[count++] = '0' + n % 10; n /= 10; } while (n);
    while (count) value[used++] = reversed[--count];
    value[used] = 0;
    field(key, value);
}
static uint32_t crc32(const uint8_t *data, unsigned size) {
    uint32_t crc = 0xffffffff;
    for (unsigned i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
static bool file_equals(const char *path, const char *text, unsigned length) {
    const struct vfs_node *node = vfs_lookup(path);
    if (!node || node->type != VFS_NODE_FILE || node->size != length) return false;
    const uint8_t *data;
    uint64_t size;
    if (!vfs_read_all(node, &data, &size)) return false;
    bool same = size == length;
    for (unsigned i = 0; same && i < length; i++) same = data[i] == (uint8_t)text[i];
    vfs_release_data(node, data);
    return same;
}
static struct block_device *find_volume(uint32_t serial) {
    struct block_device *found = 0;
    for (unsigned i = 0; i < BLOCK_MAX_DEVICES; i++) {
        struct block_device *device = block_device_at(i);
        if (!device || !usb_storage_is_device(device)) continue;
        uint8_t sector[512];
        if (!block_read(device, 0, sector, sizeof(sector))) continue;
        static const char name[] = "EXFAT   ";
        bool valid = sector[510] == 0x55 && sector[511] == 0xaa;
        for (unsigned j = 0; j < 8; j++) valid = valid && sector[3 + j] == (uint8_t)name[j];
        uint32_t id = sector[100] | (uint32_t)sector[101] << 8 |
            (uint32_t)sector[102] << 16 | (uint32_t)sector[103] << 24;
        if (!valid || id != serial) continue;
        if (found) { console_write("autotest: duplicate USB volume identity\n"); return 0; }
        found = device;
    }
    return found;
}
static bool flush_volume(struct block_device *device) {
    struct block_device *parent = block_parent(device);
    return parent && parent->flush && parent->flush(parent);
}
static bool memory_test(void) {
    uint64_t frames[16] = {0};
    bool ok = true;
    for (unsigned i = 0; i < 16; i++) {
        frames[i] = pmm_alloc_frame();
        if (!frames[i]) { ok = false; break; }
        volatile uint64_t *p = pmm_phys_to_virt(frames[i]);
        for (unsigned j = 0; j < 512; j++) p[j] = 0xa55a000000000000ull | ((uint64_t)i << 32) | j;
    }
    for (unsigned i = 0; i < 16; i++) {
        if (!frames[i]) continue;
        volatile uint64_t *p = pmm_phys_to_virt(frames[i]);
        for (unsigned j = 0; j < 512; j++)
            if (p[j] != (0xa55a000000000000ull | ((uint64_t)i << 32) | j)) ok = false;
        pmm_free_frame(frames[i]);
    }
    return ok;
}
static bool render_test(void) {
    uint64_t width = 0, height = 0, pitch = 0;
    bool available = console_framebuffer_info(&width, &height, &pitch) && timer_reference_ready();
    if (!available || !console_sync()) return false;
    bool serial = console_set_serial_output(false);
    struct console_stats before, after;
    console_get_stats(&before);
    uint64_t start = timer_monotonic_us();
    for (unsigned i = 0; i < 1000; i++)
        console_write("AaZz 0123456789 []{} !@#$%^&*() The quick brown fox jumps over the lazy dog. 123\n");
    bool ok = console_sync();
    uint64_t elapsed = timer_monotonic_us() - start;
    console_get_stats(&after);
    console_set_serial_output(serial);
    number("render_lines", 1000);
    number("render_us", elapsed);
    number("framebuffer_width", width);
    number("framebuffer_height", height);
    number("framebuffer_pitch", pitch);
    number("render_presents", after.presents - before.presents);
    number("render_fb_write_bytes", after.framebuffer_bytes_written - before.framebuffer_bytes_written);
    return ok && elapsed > 0 && after.framebuffer_bytes_written > before.framebuffer_bytes_written;
}
static void return_to_host(void) {
    console_write("autotest: returning through normal firmware/GRUB boot\n");
    timer_delay_us(250000);
    arch_reboot();
}

static int autotest_once(const char *cmdline, bool boot_ready) {
    char uuid[9], run[17], release[17];
    if (!hex_option(cmdline, "srvros.autotest-volume=", uuid, 8)) return 0;
    if (!hex_option(cmdline, "srvros.autotest-run=", run, 16) ||
        !hex_option(cmdline, "srvros.autotest-release=", release, 16)) {
        console_write("autotest: invalid run identity; staying at monitor\n");
        return 0;
    }
    console_printf("autotest: run=%s release=%s USB-volume=%s\n", run, release, uuid);
    struct block_device *device = find_volume(hex_value(uuid));
    if (!device || !exfat_mount_block_device("/testlog", device)) {
        console_write("autotest: target unavailable; no report saved\n");
        return 1;
    }
    char path[80], request[64];
    unsigned length = 0;
    append(request, &length, "SRVROS-AUTOTEST-V3 ");
    append(request, &length, run);
    append(request, &length, "\n");
    make_path(path, run, ".req");
    if (!file_equals(path, request, length)) {
        console_write("autotest: request absent, invalid or already consumed; staying at monitor\n");
        return 0; /* A stale boot profile cannot cause an endless test/reboot loop. */
    }
    char done_path[80];
    make_path(done_path, run, ".done");
    if (vfs_lookup(done_path) || !usb_storage_enable_volume_writes(device)) {
        console_write("autotest: refusing completed request or unavailable write path\n");
        return 0;
    }
    length = 0;
    append(request, &length, "STARTED ");
    append(request, &length, run);
    append(request, &length, "\n");
    if (!exfat_write_file(path, (const uint8_t *)request, length) || !flush_volume(device)) {
        console_write("autotest: request-consumption write/flush failed; no tests run\n");
        return 1;
    }
    report_size = 0;
    report_overflow = false;
    field("format", "srvros-autotest-v3");
    field("run", run);
    field("release", release);
    field("volume", uuid);
    field("boot_ready", boot_ready ? "PASS" : "FAIL");
    bool ok = boot_ready;
    bool clock = timer_reference_ready() && timer_wait_ticks_bounded(5, 100000000);
    field("timer", clock ? "PASS" : "FAIL");
    ok = ok && clock;
    bool memory = memory_test();
    field("memory", memory ? "PASS" : "FAIL");
    ok = ok && memory;
    bool fpu = clock && fpu_selftest();
    field("fpu_traps", fpu ? "PASS" : "FAIL");
    ok = ok && fpu;
    bool render = render_test();
    field("render", render ? "PASS" : "FAIL");
    ok = ok && render;
    number("usb_devices", xhci_device_count());
    number("usb_keyboards", xhci_keyboard_count());
    bool keyboard = xhci_keyboard_count() > 0;
    field("usb_keyboard_enumeration", keyboard ? "PASS" : "FAIL");
    field("physical_keypress", "NOT_TESTED");
    ok = ok && keyboard;
    /* Prove a small mixed-case payload survives write, explicit flush and read.
     * The host collector checks report durability after the actual reboot. */
    static const char probe[] = "SrvrOS USB log probe: AaZz 0123456789\n";
    make_path(path, run, ".probe");
    bool storage = exfat_write_file(path, (const uint8_t *)probe, sizeof(probe) - 1) &&
        flush_volume(device) && file_equals(path, probe, sizeof(probe) - 1);
    field("usb_write_flush_read", storage ? "PASS" : "FAIL");
    ok = ok && storage;
    bool files = storage && autotest_storage_run(device, run);
    field("usb_file_operations", files ? "PASS" : "FAIL");
    ok = ok && files;
    struct autotest_shell_stats shell_stats = {0};
    bool shell = files && autotest_shell_run(device, run, &shell_stats);
    field("usb_shell_workspace", shell ? "PASS" : "FAIL");
    number("shell_elapsed_us", shell_stats.elapsed_us);
    number("shell_input_poll_count", shell_stats.input_poll_count);
    number("shell_input_poll_max_us", shell_stats.input_poll_max_us);
    number("shell_tick_max_us", shell_stats.tick_max_us);
    field("physical_key_latency", "NOT_TESTED");
    ok = ok && shell;
    field("status", ok ? "PASS" : "FAIL");
    console_write_n(report, report_size);
    make_path(path, run, ".txt");
    const char *save_failure = NULL;
    if (report_overflow) save_failure = "report-overflow";
    else if (!exfat_write_file(path, (const uint8_t *)report, report_size)) save_failure = "report-write";
    else if (!flush_volume(device)) save_failure = "report-flush";
    bool saved = save_failure == NULL;
    if (saved) {
        uint32_t crc = crc32((const uint8_t *)report, report_size);
        char checksum[9];
        for (unsigned i = 0; i < 8; i++) checksum[i] = "0123456789abcdef"[(crc >> ((7 - i) * 4)) & 15];
        checksum[8] = 0;
        length = 0;
        append(request, &length, "COMPLETE ");
        append(request, &length, run);
        append(request, &length, " ");
        append(request, &length, checksum);
        append(request, &length, "\n");
        if (!exfat_write_file(done_path, (const uint8_t *)request, length)) save_failure = "completion-write";
        else if (!flush_volume(device)) save_failure = "completion-flush";
        saved = save_failure == NULL;
    }
    console_printf("autotest: %s report=%s run=%s\n", ok ? "PASS" : "FAIL", saved ? "flushed" : "SAVE_FAILED", run);
    if (!saved) {
        /* Do not attempt more writes to a possibly quarantined volume. Keep
         * the failure visible for a photograph before the automatic return. */
        usb_storage_print_status(device);
        console_printf("autotest: SAVE_FAILED stage=%s exfat-writable=%s\n",
            save_failure, exfat_is_writable("/testlog") ? "yes" : "no");
    }
    if (process_has_open_vfs_prefix("/testlog")) {
        console_write("autotest: files remain open; staying at monitor\n");
        return 0;
    }
    if (!saved) {
        console_write("autotest: photograph this screen; returning to Ubuntu in 15 seconds\n");
        console_sync();
        timer_delay_us(15000000);
    }
    return ok && saved ? 2 : 1;
}

/* Five independent, pre-authorized V3 requests. A partial batch never passes. */
void autotest_run(const char *cmdline, bool boot_ready) {
    const char *key = "srvros.autotest-repeat=";
    bool present = false;
    for (const char *p = cmdline; *p; p++) {
        if (p != cmdline && !separator(p[-1])) continue;
        unsigned n = 0;
        while (key[n] && p[n] == key[n]) n++;
        if (!key[n]) present = true;
    }
    if (!present) {
        if (autotest_once(cmdline, boot_ready)) return_to_host();
        return;
    }
    char repeat[2], uuid[9], seed[17], release[17];
    if (!hex_option(cmdline, key, repeat, 1) || repeat[0] != '5' ||
        !hex_option(cmdline, "srvros.autotest-volume=", uuid, 8) ||
        !hex_option(cmdline, "srvros.autotest-run=", seed, 16) ||
        !hex_option(cmdline, "srvros.autotest-release=", release, 16)) {
        console_write("autotest: invalid repeat profile; staying at monitor\n");
        return;
    }
    unsigned last = seed[15] <= '9' ? seed[15] - '0' : seed[15] - 'a' + 10;
    for (unsigned round = 0; round < 5; round++) {
        char run[17], command[160]; unsigned n = 0;
        for (unsigned i = 0; i < 17; i++) run[i] = seed[i];
        run[15] = "0123456789abcdef"[last ^ round];
        append(command, &n, "srvros.autotest-volume="); append(command, &n, uuid);
        append(command, &n, " srvros.autotest-run="); append(command, &n, run);
        append(command, &n, " srvros.autotest-release="); append(command, &n, release);
        console_printf("autotest: stress round %u/5 run=%s\n", (uint64_t)round + 1, run);
        int result = autotest_once(command, boot_ready);
        if (result != 2) {
            console_write("autotest: stress INCOMPLETE; stopping at failed round\n");
            if (result) return_to_host();
            return;
        }
        if (round < 4 && !exfat_unmount("/testlog")) {
            console_write("autotest: stress unmount FAILED; staying at monitor\n");
            return;
        }
    }
    console_write("autotest: stress 5/5 PASS; host must verify all five reports\n");
    return_to_host();
}
