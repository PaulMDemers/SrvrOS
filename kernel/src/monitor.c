#include <srvros/console.h>
#include <srvros/block.h>
#include <srvros/e1000.h>
#include <srvros/elf.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/keyboard.h>
#include <srvros/monitor.h>
#include <srvros/net.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/workqueue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINE_MAX 128
#define LS_CHILD_NAME_MAX 64
#define LS_CHILDREN_MAX 64

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

static void read_line(char *line, size_t capacity) {
    size_t length = 0;

    for (;;) {
        char c = keyboard_read_char();

        if (c == '\n') {
            console_putc('\n');
            line[length] = '\0';
            return;
        }

        if (c == '\b') {
            if (length > 0) {
                length--;
                console_putc('\b');
            }
            continue;
        }

        if (c >= 32 && c < 127 && length + 1 < capacity) {
            line[length++] = c;
            console_putc(c);
        }
    }
}

static void print_help(void) {
    console_write("commands: help clear mem memstat ticks heap workers block mount unmount fsck pci net ps bg kill echo ls [dir] cat write elf run [args]\n");
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

    if (process_has_open_vfs_prefix(mountpoint)) {
        console_printf("unmount: busy: %s\n", mountpoint);
        return;
    }

    if (!exfat_unmount(mountpoint)) {
        console_printf("unmount: not mounted: %s\n", mountpoint);
        return;
    }
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
    } else if (streq(command, "workers")) {
        console_printf("scheduler: threads=%u preemptions=%u demo_counter=%u\n",
            scheduler_thread_count(),
            scheduler_preemptions(),
            scheduler_demo_counter());
        workqueue_print_status();
    } else if (streq(command, "block")) {
        block_print_status();
    } else if (streq(command, "mount")) {
        command_mount(args);
    } else if (streq(command, "unmount")) {
        command_unmount(args);
    } else if (streq(command, "fsck")) {
        exfat_check_print(args);
    } else if (streq(command, "pci")) {
        command_pci();
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
        read_line(line, sizeof(line));
        run_command(line);
    }
}
