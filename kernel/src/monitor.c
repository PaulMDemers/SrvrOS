#include <srvros/ahci.h>
#include <srvros/audio.h>
#include <srvros/thermal.h>
#include <srvros/power.h>
#include <srvros/smc.h>
#include <srvros/audio_abi.h>
#include <srvros/arch.h>
#include <srvros/latency.h>
#include <srvros/fpu.h>
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/bootlog.h>
#include <srvros/block.h>
#include <srvros/e1000.h>
#include <srvros/elf.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/semaphore.h>
#include <srvros/acpi_work_test.h>
#include <srvros/acpi_runtime_test.h>
#include <srvros/intel_gfx.h>
#include <srvros/keyboard.h>
#include <srvros/lpss_spi.h>
#include <srvros/monitor.h>
#include <srvros/net.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/process_lifetime_test.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/workqueue.h>
#include <srvros/workspace.h>
#include <srvros/xhci.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINE_MAX 128
#define LS_CHILD_NAME_MAX 64
#define LS_CHILDREN_MAX 64
#define MONITOR_HISTORY_MAX 16

static char monitor_history[MONITOR_HISTORY_MAX][LINE_MAX];
static uint64_t monitor_history_count;

static bool streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }

    return *a == *b;
}

static uint64_t cstrlen(const char *text) {
    uint64_t length = 0;
    if (text == NULL) {
        return 0;
    }
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static uint64_t parse_decimal(const char *text) {
    uint64_t value = 0;
    if (text == NULL) {
        return 0;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (uint64_t)(*text - '0');
        text++;
    }
    return value;
}

static bool starts_with(const char *text, const char *prefix) {
    if (text == NULL || prefix == NULL) {
        return false;
    }
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return false;
        }
    }
    return true;
}

static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

static char *trim(char *line) {
    while (is_space(*line)) {
        line++;
    }

    char *end = line;
    while (*end != '\0') {
        end++;
    }

    while (end > line && is_space(end[-1])) {
        end--;
    }
    *end = '\0';

    return line;
}

static char *next_token(char **cursor) {
    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }

    char *token = trim(*cursor);
    if (*token == '\0') {
        *cursor = token;
        return NULL;
    }

    char *end = token;
    while (*end != '\0' && !is_space(*end)) {
        end++;
    }
    if (*end != '\0') {
        *end++ = '\0';
    }
    *cursor = end;
    return token;
}

static void line_copy(char *destination, size_t capacity, const char *source) {
    size_t i = 0;
    if (capacity == 0) {
        return;
    }
    while (source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static bool line_empty(const char *line) {
    return line == NULL || line[0] == '\0';
}

static void monitor_history_add(const char *line) {
    if (line_empty(line)) {
        return;
    }
    if (monitor_history_count > 0 &&
        streq(monitor_history[monitor_history_count - 1], line)) {
        return;
    }
    if (monitor_history_count == MONITOR_HISTORY_MAX) {
        for (uint64_t i = 1; i < MONITOR_HISTORY_MAX; i++) {
            line_copy(monitor_history[i - 1], sizeof(monitor_history[i - 1]), monitor_history[i]);
        }
        monitor_history_count--;
    }
    line_copy(monitor_history[monitor_history_count], sizeof(monitor_history[0]), line);
    monitor_history_count++;
}

static bool read_scan_timeout(char *out) {
    uint64_t deadline = timer_ticks() + 2;
    do {
        if (keyboard_scan_char(out)) {
            return true;
        }
        scheduler_yield();
    } while (timer_ticks() <= deadline);
    return false;
}

static char read_pending_or_keyboard(int *pending) {
    if (*pending >= 0) {
        char c = (char)*pending;
        *pending = -1;
        return c;
    }
    return keyboard_read_char();
}

static bool read_pending_or_scan(int *pending, char *out) {
    if (*pending >= 0) {
        *out = (char)*pending;
        *pending = -1;
        return true;
    }
    return read_scan_timeout(out);
}

static void refresh_monitor_line(const char *prompt,
    const char *line,
    size_t length,
    size_t position) {
    console_putc('\r');
    console_write(prompt);
    console_write(line);
    console_write("\x1b[K\r");
    console_write(prompt);
    for (size_t i = 0; i < position && i < length; i++) {
        console_putc(line[i]);
    }
}

static void line_replace(char *line,
    size_t capacity,
    size_t *length,
    size_t *position,
    const char *source) {
    line_copy(line, capacity, source);
    *length = cstrlen(line);
    *position = *length;
}

static void line_insert(char *line,
    size_t capacity,
    size_t *length,
    size_t *position,
    char c) {
    if (*length + 1 >= capacity) {
        return;
    }
    for (size_t i = *length + 1; i > *position; i--) {
        line[i] = line[i - 1];
    }
    line[*position] = c;
    (*length)++;
    (*position)++;
}

static void line_delete_previous(char *line, size_t *length, size_t *position) {
    if (*position == 0) {
        return;
    }
    (*position)--;
    for (size_t i = *position; i < *length; i++) {
        line[i] = line[i + 1];
    }
    (*length)--;
}

static bool handle_monitor_arrow(char code,
    char *line,
    size_t capacity,
    size_t *length,
    size_t *position,
    uint64_t *history_index,
    char *draft,
    bool *draft_saved) {
    if (code == 'A') {
        if (monitor_history_count == 0 || *history_index == 0) {
            return true;
        }
        if (!*draft_saved && *history_index == monitor_history_count) {
            line_copy(draft, capacity, line);
            *draft_saved = true;
        }
        (*history_index)--;
        line_replace(line, capacity, length, position, monitor_history[*history_index]);
        return true;
    }
    if (code == 'B') {
        if (*history_index >= monitor_history_count) {
            return true;
        }
        (*history_index)++;
        if (*history_index < monitor_history_count) {
            line_replace(line, capacity, length, position, monitor_history[*history_index]);
        } else {
            line_replace(line, capacity, length, position, *draft_saved ? draft : "");
        }
        return true;
    }
    if (code == 'C') {
        if (*position < *length) {
            (*position)++;
        }
        return true;
    }
    if (code == 'D') {
        if (*position > 0) {
            (*position)--;
        }
        return true;
    }
    return false;
}

static bool consume_monitor_escape(int *pending,
    char *line,
    size_t capacity,
    size_t *length,
    size_t *position,
    uint64_t *history_index,
    char *draft,
    bool *draft_saved) {
    char first = 0;
    char code = 0;
    if (!read_pending_or_scan(pending, &first)) {
        return true;
    }
    if (first != '[' && first != 'O') {
        *pending = (unsigned char)first;
        return true;
    }
    if (!read_pending_or_scan(pending, &code)) {
        *pending = (unsigned char)first;
        return true;
    }
    if (!handle_monitor_arrow(code,
            line,
            capacity,
            length,
            position,
            history_index,
            draft,
            draft_saved)) {
        *pending = (unsigned char)code;
    }
    return true;
}

static void read_line(const char *prompt, char *line, size_t capacity) {
    size_t length = 0;
    size_t position = 0;
    uint64_t history_index = monitor_history_count;
    char draft[LINE_MAX];
    int pending = -1;
    bool draft_saved = false;

    if (capacity == 0) {
        return;
    }
    line[0] = '\0';
    draft[0] = '\0';

    for (;;) {
        char c = read_pending_or_keyboard(&pending);

        if (c == '\n') {
            console_putc('\n');
            line[length] = '\0';
            monitor_history_add(line);
            return;
        }

        if (c == 27) {
            (void)consume_monitor_escape(&pending,
                line,
                capacity,
                &length,
                &position,
                &history_index,
                draft,
                &draft_saved);
            refresh_monitor_line(prompt, line, length, position);
            continue;
        }

        if (c == '[') {
            char code = 0;
            if (read_pending_or_scan(&pending, &code) &&
                handle_monitor_arrow(code,
                    line,
                    capacity,
                    &length,
                    &position,
                    &history_index,
                    draft,
                    &draft_saved)) {
                refresh_monitor_line(prompt, line, length, position);
                continue;
            }
            if (code != 0) {
                pending = (unsigned char)code;
            }
        }

        if (c == '\b' || c == 127) {
            line_delete_previous(line, &length, &position);
            refresh_monitor_line(prompt, line, length, position);
            continue;
        }

        if (c == 1) {
            position = 0;
            refresh_monitor_line(prompt, line, length, position);
            continue;
        }

        if (c == 5) {
            position = length;
            refresh_monitor_line(prompt, line, length, position);
            continue;
        }

        if (c >= 32 && c < 127 && length + 1 < capacity) {
            line_insert(line, capacity, &length, &position, c);
            refresh_monitor_line(prompt, line, length, position);
        }
    }
}

static void print_render_stats(void) {
    struct console_stats stats;
    console_get_stats(&stats);
    console_printf("render: bytes=%u scrolls=%u presents=%u cells=%u fb_write_bytes=%u fb_read_bytes=0\n",
        stats.bytes_received, stats.scrolls, stats.presents, stats.cells_painted,
        stats.framebuffer_bytes_written);
}

static void render_benchmark(bool serial) {
    if (!timer_reference_ready()) {
        console_write("renderbench: calibrated reference unavailable\n");
        return;
    }
    if (!console_sync()) { console_write("renderbench: presenter busy\n"); return; }
    uint64_t width = 0, height = 0, pitch = 0;
    (void)console_framebuffer_info(&width, &height, &pitch);
    bool saved_serial = console_set_serial_output(serial);
    struct console_stats before, after;
    console_get_stats(&before);
    uint64_t start = timer_monotonic_us();
    static const char line[] =
        "AaZz 0123456789 []{} !@#$%^&*() The quick brown fox jumps over the lazy dog. 123\n";
    _Static_assert(sizeof(line) == 82, "benchmark must contain 80 columns plus newline");
    for (unsigned i = 0; i < 1000; i++) console_write_n(line, sizeof(line) - 1);
    bool drained = console_sync();
    uint64_t elapsed = timer_monotonic_us() - start;
    console_get_stats(&after);
    console_set_serial_output(saved_serial);
    console_printf("renderbench: lines=1000 columns=80 serial=%s complete=%s us=%u\n",
        serial ? "on" : "off", drained ? "yes" : "no", elapsed);
    console_printf("renderbench: mode=%ux%u pitch=%u scrolls=%u presents=%u fb_write_bytes=%u fb_read_bytes=0\n",
        width, height, pitch, after.scrolls - before.scrolls, after.presents - before.presents,
        after.framebuffer_bytes_written - before.framebuffer_bytes_written);
}

static void print_latency_sample(const char *name, const struct latency_sample *s) {
    console_printf("latency: %s count=%u max_us=%u total_us=%u over_1ms=%u over_10ms=%u\n",
        name, s->count, s->max_us, s->total_us, s->over_1ms, s->over_10ms);
}

static void command_latency(const char *args) {
    if (streq(args, "reset")) {
        console_printf("latency: reset active=%s\n", latency_reset() ? "yes" : "no");
        return;
    }
    if (streq(args, "off")) latency_stop();
    else if (*args) { console_write("usage: latency [reset|off]\n"); return; }
    struct latency_stats snapshot;
    latency_snapshot(&snapshot);
    console_printf("latency: active=%s reference=%s (sampled sections; gaps include scheduling)\n",
        snapshot.enabled ? "yes" : "no", timer_reference_ready() ? "acpi-pm" : "unavailable");
    static const char *names[] = {"console", "heap", "ahci", "serial", "pmm"};
    for (unsigned i = 0; i < LATENCY_SITE_COUNT; i++) print_latency_sample(names[i], &snapshot.critical[i]);
    print_latency_sample("timer-gap", &snapshot.tick_gap);
    print_latency_sample("usb-poll-gap", &snapshot.input_poll_gap);
    keyboard_print_status();
}

static void render_stress(void) {
    if (!timer_reference_ready()) { console_write("renderstress: calibrated reference unavailable\n"); return; }
    console_write("renderstress: running for 3 seconds; type letters, Esc stops\n");
    if (!console_sync()) { console_write("renderstress: presenter busy\n"); return; }
    bool serial = console_set_serial_output(false);
    uint64_t start = timer_monotonic_us(), ticks = timer_ticks();
    uint64_t dropped = keyboard_dropped_count(), keys = 0, lines = 0;
    char input[65];
    unsigned captured = 0;
    bool stopped = false;
    static const char line[] = "AaZz sustained output: type while scrolling; Escape ends the test.\n";
    /* A finite iteration guard also stops the test if the reference stalls. */
    for (unsigned batch = 0; batch < 100000 && timer_monotonic_us() - start < 3000000; batch++) {
        for (unsigned i = 0; i < 32; i++) { console_write(line); lines++; }
        char c;
        while (keyboard_try_read_char(&c)) {
            if (c == 27) { stopped = true; break; }
            keys++;
            if (c >= 32 && c <= 126 && captured < sizeof(input) - 1) input[captured++] = c;
        }
        if (stopped) break;
        scheduler_yield();
    }
    bool drained = console_sync();
    uint64_t elapsed = timer_monotonic_us() - start;
    ticks = timer_ticks() - ticks;
    dropped = keyboard_dropped_count() - dropped;
    console_set_serial_output(serial);
    input[captured] = 0;
    console_printf("renderstress: lines=%u us=%u ticks=%u keys=%u dropped=%u complete=%s stopped=%s\n",
        lines, elapsed, ticks, keys, dropped, drained ? "yes" : "no", stopped ? "yes" : "no");
    console_printf("renderstress: input=%s\n", input);
}

static void print_help(void) {
    console_write("commands: help clear bootinfo hwdiag dmesg mem memstat ticks renderstat renderbench [serial] renderstress fpustress latency [reset|off] heap workers semaphoretest acpiworktest scitest [arm|status|stop] block mount unmount workspace fsck pci gpu spi spiregs acpiinput xhci audio [test|stop|speakers|headphones] thermal smc power net ps bg kill echo ls [dir] cat write elf run [args]\n");
    console_write("workspace: status | mount <UUID> | shell | sync | unmount\n");
    console_write("reboot: flush storage and return through the normal boot entry\n");
}

static void command_pci(void);
static void command_ps(void);

static void command_bootinfo(void) {
    struct console_display_info display;
    if (console_display_info(&display)) {
        console_printf("display: %ux%u pitch=%u bpp=%u model=%u rgb=%u:%u %u:%u %u:%u\n",
            display.width,
            display.height,
            display.pitch,
            (uint64_t)display.bpp,
            (uint64_t)display.memory_model,
            (uint64_t)display.red_mask_size,
            (uint64_t)display.red_mask_shift,
            (uint64_t)display.green_mask_size,
            (uint64_t)display.green_mask_shift,
            (uint64_t)display.blue_mask_size,
            (uint64_t)display.blue_mask_shift);
    } else {
        console_write("display: unavailable\n");
    }

    console_printf("timer: ticks=%u\n", timer_ticks());
    console_printf("pci: devices=%u config=%s\n", pci_device_count(), pci_config_backend_name());
    acpi_print_status();
    acpi_print_input_diagnostics();
    intel_gfx_print_status();
    lpss_spi_print_status();
    pci_print_input_bus_candidates();
    xhci_print_status();
    block_print_status();
}

static void command_hwdiag(void) {
    console_write("hwdiag: begin\n");
    console_write("== bootinfo ==\n");
    command_bootinfo();
    console_write("== pci ==\n");
    command_pci();
    console_write("== memory ==\n");
    pmm_print_status();
    console_printf("heap: total=%u free=%u used=%u bytes\n",
        heap_total_bytes(),
        heap_free_bytes(),
        heap_used_bytes());
    console_write("== scheduler ==\n");
    scheduler_print_status();
    workqueue_print_status();
    console_write("== network ==\n");
    e1000_print_status();
    net_print_status();
    console_write("== mounts ==\n");
    exfat_print_mounts();
    console_write("== fsck ==\n");
    exfat_check_print("/fat");
    console_write("== processes ==\n");
    command_ps();
    console_write("== dmesg tail ==\n");
    bootlog_dump(4096);
    console_write("hwdiag: end\n");
}

static bool child_seen(char children[LS_CHILDREN_MAX][LS_CHILD_NAME_MAX], uint64_t count, const char *name) {
    for (uint64_t i = 0; i < count; i++) {
        if (streq(children[i], name)) {
            return true;
        }
    }
    return false;
}

static void remember_child(char children[LS_CHILDREN_MAX][LS_CHILD_NAME_MAX], uint64_t *count, const char *name) {
    if (*count >= LS_CHILDREN_MAX || child_seen(children, *count, name)) {
        return;
    }

    uint64_t i = 0;
    while (name[i] != '\0' && i + 1 < LS_CHILD_NAME_MAX) {
        children[*count][i] = name[i];
        i++;
    }
    children[*count][i] = '\0';
    (*count)++;
}

static void command_ls(const char *args) {
    uint64_t count = vfs_count();
    char directory[160];
    char prefix[160];
    char children[LS_CHILDREN_MAX][LS_CHILD_NAME_MAX];
    uint64_t child_count = 0;
    bool found = false;

    if (count == 0) {
        console_write("vfs is empty\n");
        return;
    }

    char *trimmed_args = args != NULL ? trim((char *)args) : NULL;
    if (trimmed_args == NULL || trimmed_args[0] == '\0') {
        trimmed_args = "/";
    }

    uint64_t directory_length = 0;
    while (trimmed_args[directory_length] != '\0' && directory_length + 1 < sizeof(directory)) {
        directory[directory_length] = trimmed_args[directory_length];
        directory_length++;
    }
    directory[directory_length] = '\0';
    while (directory_length > 1 && directory[directory_length - 1] == '/') {
        directory[--directory_length] = '\0';
    }

    if (streq(directory, "/")) {
        prefix[0] = '/';
        prefix[1] = '\0';
    } else {
        uint64_t i = 0;
        while (directory[i] != '\0' && i + 2 < sizeof(prefix)) {
            prefix[i] = directory[i];
            i++;
        }
        if (i == 0 || i + 2 >= sizeof(prefix)) {
            console_write("ls: path too long\n");
            return;
        }
        prefix[i++] = '/';
        prefix[i] = '\0';
    }

    for (uint64_t i = 0; i < count; i++) {
        const struct vfs_node *node = vfs_node_at(i);
        if (node == NULL) {
            continue;
        }

        if (streq(node->path, directory)) {
            console_printf("%s %u bytes\n", node->path, node->size);
            found = true;
            continue;
        }

        if (!starts_with(node->path, prefix)) {
            continue;
        }

        const char *rest = node->path + cstrlen(prefix);
        if (rest[0] == '\0') {
            continue;
        }

        char child[LS_CHILD_NAME_MAX];
        uint64_t child_length = 0;
        while (rest[child_length] != '\0' &&
            rest[child_length] != '/' &&
            child_length + 1 < sizeof(child)) {
            child[child_length] = rest[child_length];
            child_length++;
        }
        child[child_length] = '\0';

        if (child[0] == '\0') {
            continue;
        }

        if (rest[child_length] == '/') {
            if (!child_seen(children, child_count, child)) {
                console_printf("%s/\n", child);
                remember_child(children, &child_count, child);
            }
            found = true;
        } else {
            console_printf("%s %u bytes\n", child, node->size);
            found = true;
        }
    }

    if (!found) {
        console_write("ls: not found\n");
    }
}

static void command_cat(const char *path) {
    if (path == NULL || path[0] == '\0') {
        console_write("usage: cat /path\n");
        return;
    }

    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data;
    uint64_t size;

    if (node == NULL || !vfs_read_all(node, &data, &size)) {
        console_printf("cat: not found: %s\n", path);
        return;
    }

    for (uint64_t i = 0; i < size; i++) {
        console_putc((char)data[i]);
    }

    if (size == 0 || data[size - 1] != '\n') {
        console_putc('\n');
    }
    vfs_release_data(node, data);
}

static void command_write(char *args) {
    if (args == NULL || args[0] == '\0') {
        console_write("usage: write /fat/name text\n");
        return;
    }

    char *path = args;
    while (*args != '\0' && !is_space(*args)) {
        args++;
    }
    if (*args == '\0') {
        console_write("usage: write /fat/name text\n");
        return;
    }
    *args++ = '\0';
    args = trim(args);
    if (*args == '\0') {
        console_write("usage: write /fat/name text\n");
        return;
    }

    uint64_t length = 0;
    while (args[length] != '\0') {
        length++;
    }

    if (!exfat_write_file(path, (const uint8_t *)args, length)) {
        console_write("write: failed\n");
        return;
    }
    console_printf("write: %s %u bytes\n", path, length);
}

static void command_elf(const char *path) {
    if (path == NULL || path[0] == '\0') {
        console_write("usage: elf /path\n");
        return;
    }

    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data;
    uint64_t size;
    struct elf64_info info;

    if (node == NULL || !vfs_read_all(node, &data, &size)) {
        console_printf("elf: not found: %s\n", path);
        return;
    }

    if (!elf64_probe(data, size, &info)) {
        console_printf("elf: invalid elf64: %s\n", path);
        vfs_release_data(node, data);
        return;
    }

    console_printf("elf: type=%u machine=%u entry=%x phoff=%u phnum=%u\n",
        (uint64_t)info.type,
        (uint64_t)info.machine,
        info.entry,
        info.program_header_offset,
        (uint64_t)info.program_header_count);
    vfs_release_data(node, data);
}

static void command_pci(void) {
    uint64_t count = pci_device_count();
    console_printf("pci: devices=%u\n", count);

    for (uint64_t i = 0; i < count; i++) {
        const struct pci_device *dev = pci_device_at(i);
        console_printf("bus=%u dev=%u fn=%u vendor=%x device=%x class=%x subclass=%x if=%x %s irq=%u bar0=%x\n",
            (uint64_t)dev->bus,
            (uint64_t)dev->device,
            (uint64_t)dev->function,
            (uint64_t)dev->vendor_id,
            (uint64_t)dev->device_id,
            (uint64_t)dev->class_code,
            (uint64_t)dev->subclass,
            (uint64_t)dev->prog_if,
            pci_class_name(dev->class_code, dev->subclass),
            (uint64_t)dev->interrupt_line,
            (uint64_t)dev->bar[0]);
    }
}

static void command_mount(char *args) {
    if (args == NULL || args[0] == '\0') {
        exfat_print_mounts();
        return;
    }

    char *cursor = args;
    char *first = next_token(&cursor);
    char *device_name = NULL;
    char *mountpoint = NULL;

    if (first != NULL && streq(first, "exfat")) {
        device_name = next_token(&cursor);
        mountpoint = next_token(&cursor);
    } else {
        device_name = first;
        mountpoint = next_token(&cursor);
    }

    if (device_name == NULL || mountpoint == NULL) {
        console_write("usage: mount [exfat] <block-device> /mountpoint\n");
        return;
    }

    struct block_device *device = block_find(device_name);
    if (device == NULL) {
        console_printf("mount: no such block device: %s\n", device_name);
        return;
    }

    if (!exfat_mount_block_device(mountpoint, device)) {
        console_write("mount: failed\n");
        return;
    }
    console_printf("mount: %s on %s type=exfat\n", device_name, mountpoint);
}

static void command_unmount(const char *mountpoint) {
    if (mountpoint == NULL || mountpoint[0] == '\0') {
        console_write("usage: unmount /mountpoint\n");
        return;
    }

    if (streq(mountpoint, "/work") && workspace_active()) {
        workspace_unmount();
        return;
    }
    if (process_has_open_vfs_prefix(mountpoint)) {
        console_printf("unmount: busy: %s\n", mountpoint);
        return;
    }

    if (!exfat_unmount(mountpoint)) {
        console_printf("unmount: not mounted: %s\n", mountpoint);
        return;
    }
}

static void command_workspace(char *args) {
    char *cursor = args;
    char *action = args ? next_token(&cursor) : NULL;
    char *uuid = action ? next_token(&cursor) : NULL;
    if (!action || (streq(action, "status") && !uuid)) workspace_status();
    else if (streq(action, "mount") && uuid && !next_token(&cursor)) workspace_mount(uuid);
    else if (streq(action, "shell") && !uuid) {
        if (!workspace_active() || !exfat_is_writable("/work"))
            console_write("workspace: mount a writable workspace first\n");
        else process_run_elf_args("/fat/bin/sh", "--workspace /work/srvros-work");
    }
    else if (streq(action, "sync") && !uuid) {
        if (!workspace_sync()) console_write("workspace: sync failed\n");
    } else if (streq(action, "unmount") && !uuid) workspace_unmount();
    else console_write("usage: workspace [status|mount UUID|shell|sync|unmount]\n");
}

static void command_net(const char *args) {
    if (args == NULL || args[0] == '\0') {
        e1000_print_status();
        net_print_status();
        return;
    }

    if (streq(args, "poll")) {
        uint64_t count = e1000_poll(16, true);
        console_printf("net: poll received=%u\n", count);
    } else if (streq(args, "pollq")) {
        uint64_t count = e1000_poll(16, false);
        console_printf("net: poll received=%u\n", count);
    } else if (streq(args, "tx")) {
        e1000_send_test_frame();
    } else if (streq(args, "stack")) {
        net_print_status();
    } else {
        console_write("usage: net [poll|pollq|tx|stack]\n");
    }
}

static void command_ps(void) {
    console_write("pid name status\n");
    uint64_t cursor = 0;
    bool any = false;
    for (;;) {
        uint64_t pid = 0;
        const char *name = "";
        const char *state = "";
        cursor = process_list(cursor, &pid, &name, &state);
        if (cursor == 0) {
            break;
        }
        any = true;
        console_printf("%u %s %s\n", pid, name, state);
    }
    if (!any) {
        console_write("no userspace processes\n");
    }
}

void monitor_run_a1466_capture(void) {
    console_write("\n");
    console_write("a1466-capture: begin\n");
    console_write("a1466-capture: automatic post-boot diagnostics; keyboard input not required\n");

    console_write("== hwdiag ==\n");
    command_hwdiag();

    console_write("== dmesg 8192 ==\n");
    bootlog_dump(8192);

    console_write("== spi ==\n");
    lpss_spi_print_status();

    console_write("== spiregs ==\n");
    lpss_spi_print_registers();

    console_write("== acpiinput ==\n");
    acpi_print_input_diagnostics();

    console_write("== pci ==\n");
    command_pci();

    console_write("== xhci ==\n");
    xhci_print_status();

    console_write("== block ==\n");
    block_print_status();

    console_write("== final spi/input summary ==\n");
    lpss_spi_print_status();
    lpss_spi_print_registers();
    keyboard_print_status();
    xhci_print_input_summary();

    console_write("a1466-capture: end\n");
    console_write("a1466-capture: persisted in /fat/var/log/boot.log when /fat is writable\n");
}

static void command_bg(const char *path) {
    if (path == NULL || path[0] == '\0') {
        console_write("usage: bg /path\n");
        return;
    }

    if (!process_start_background(path)) {
        console_write("bg: failed\n");
    }
}

static void command_run(char *args) {
    if (args == NULL || args[0] == '\0') {
        console_write("usage: run /path [args]\n");
        return;
    }

    char *path = args;
    while (*args != '\0' && !is_space(*args)) {
        args++;
    }
    if (*args != '\0') {
        *args++ = '\0';
        args = trim(args);
    }

    process_run_elf_args(path, args);
}

static void command_kill(const char *target) {
    if (target != NULL && target[0] != '\0') {
        uint64_t pid = 0;
        for (uint64_t i = 0; target[i] >= '0' && target[i] <= '9'; i++) {
            pid = pid * 10 + (uint64_t)(target[i] - '0');
        }
        if (pid == 0 || !process_kill_pid(pid)) {
            console_write("kill: no such process\n");
            return;
        }
        console_write("kill: requested process stop\n");
        return;
    }

    if (!process_kill_background()) {
        console_write("kill: no background process\n");
        return;
    }

    console_write("kill: requested background stop\n");
}

static void run_command(char *line) {
    char *command = trim(line);

    if (*command == '\0') {
        return;
    }

    char *args = command;
    while (*args != '\0' && !is_space(*args)) {
        args++;
    }
    if (*args != '\0') {
        *args++ = '\0';
        args = trim(args);
    }

    if (streq(command, "help")) {
        print_help();
    } else if (streq(command, "bootinfo")) {
        command_bootinfo();
    } else if (streq(command, "hwdiag")) {
        command_hwdiag();
    } else if (streq(command, "dmesg")) {
        uint64_t max = parse_decimal(args);
        bootlog_dump(max);
    } else if (streq(command, "clear")) {
        console_clear();
    } else if (streq(command, "mem")) {
        console_printf("pmm: total=%u free=%u used=%u frames\n",
            pmm_total_frames(),
            pmm_free_frames(),
            pmm_used_frames());
    } else if (streq(command, "memstat")) {
        pmm_print_status();
        console_printf("heap: total=%u free=%u used=%u bytes\n",
            heap_total_bytes(),
            heap_free_bytes(),
            heap_used_bytes());
    } else if (streq(command, "heap")) {
        console_printf("heap: total=%u free=%u used=%u bytes\n",
            heap_total_bytes(),
            heap_free_bytes(),
            heap_used_bytes());
    } else if (streq(command, "ticks")) {
        console_printf("ticks: %u\n", timer_ticks());
    } else if (streq(command, "lifetimetest")) {
        process_lifetime_test_run();
    } else if (streq(command, "semaphoretest")) {
        semaphore_test_run();
    } else if (streq(command, "acpiworktest")) {
        acpi_work_test_run();
    } else if (streq(command, "scitest")) {
        acpi_sci_test_command(args);
    } else if (streq(command, "acpiruntime")) {
        acpi_runtime_test_command(args);
    } else if (streq(command, "workers")) {
        scheduler_print_status();
        workqueue_print_status();
    } else if (streq(command, "block")) {
        block_print_status();
        ahci_print_status();
    } else if (streq(command, "mount")) {
        command_mount(args);
    } else if (streq(command, "unmount")) {
        command_unmount(args);
    } else if (streq(command, "workspace")) {
        command_workspace(args);
    } else if (streq(command, "reboot")) {
        if (workspace_active() && !workspace_unmount()) {
            console_write("reboot: workspace could not be unmounted\n");
        } else if (process_file_sync_all() < 0 || !block_flush_all()) {
            console_write("reboot: storage flush failed\n");
        } else {
            arch_reboot();
        }
    } else if (streq(command, "fsck")) {
        exfat_check_print(args);
    } else if (streq(command, "pci")) {
        command_pci();
    } else if (streq(command, "gpu")) {
        intel_gfx_print_status();
    } else if (streq(command, "spi")) {
        lpss_spi_print_status();
    } else if (streq(command, "spiregs")) {
        lpss_spi_print_registers();
    } else if (streq(command, "acpiinput")) {
        acpi_print_input_diagnostics();
    } else if (streq(command, "renderstat")) {
        print_render_stats();
    } else if (streq(command, "latency")) {
        command_latency(args);
    } else if (streq(command, "fpustress")) {
        fpu_selftest();
    } else if (streq(command, "renderstress")) {
        render_stress();
    } else if (streq(command, "renderbench")) {
        render_benchmark(streq(args, "serial"));
    } else if (streq(command, "xhci") || streq(command, "usb")) {
        xhci_print_status();
    } else if (streq(command, "smc")) {
        smc_print_status();
    } else if (streq(command, "thermal")) {
        thermal_print_status();
    } else if (streq(command, "power")) {
        power_print_status();
    } else if (streq(command, "audio")) {
        unsigned op=0,value=0;
        if(streq(args,"test")) op=SRV_AUDIO_TEST;
        else if(streq(args,"stop")) op=SRV_AUDIO_STOP;
        else if(streq(args,"headphones")) { op=SRV_AUDIO_OUTPUT;value=1; }
        else if(streq(args,"speakers")) op=SRV_AUDIO_OUTPUT;
        if(op) console_printf("audio: request %s\n",audio_request(op,value)?"rejected (unavailable or busy)":"queued");
        else if(*args) console_write("audio: usage: audio [test|stop|speakers|headphones]\n");
        audio_print_status();
    } else if (streq(command, "net")) {
        command_net(args);
    } else if (streq(command, "ps")) {
        command_ps();
    } else if (streq(command, "bg")) {
        command_bg(args);
    } else if (streq(command, "kill")) {
        command_kill(args);
    } else if (streq(command, "echo")) {
        console_write(args);
        console_putc('\n');
    } else if (streq(command, "ls")) {
        command_ls(args);
    } else if (streq(command, "cat")) {
        command_cat(args);
    } else if (streq(command, "write")) {
        command_write(args);
    } else if (streq(command, "elf")) {
        command_elf(args);
    } else if (streq(command, "run")) {
        command_run(args);
    } else {
        console_printf("unknown command: %s\n", command);
    }
}

void monitor_run(void) {
    char line[LINE_MAX];

    console_write("\nsrvros monitor ready. Type 'help'.\n");
    for (;;) {
        console_write("srv> ");
        read_line("srv> ", line, sizeof(line));
        run_command(line);
    }
}
