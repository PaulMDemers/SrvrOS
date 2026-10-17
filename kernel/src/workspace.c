/* One explicit USB workspace. Boot/root files remain in RAM. */
#include <srvros/workspace.h>
#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/usb_storage.h>
#include <srvros/vfs.h>
#include <stddef.h>
#include <stdint.h>

static struct block_device *active;
static char active_uuid[9];
static bool failed;

struct scope { uint64_t flags; bool services; };
static bool begin(struct scope *scope) {
#ifdef SRVROS_WORKSPACE_HOST_TEST
    scope->flags = 0;
#else
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(scope->flags) : : "memory");
#endif
    scope->services = scheduler_io_begin();
    return scope->services || !scheduler_io_active();
}
static void end(struct scope *scope) {
    if (scope->services) scheduler_io_end();
#ifndef SRVROS_WORKSPACE_HOST_TEST
    if (scope->flags & (1ull << 9)) __asm__ volatile("sti" : : : "memory");
#endif
}
static bool parse_uuid(const char *text, char compact[9], uint32_t *serial) {
    if (!text) return false;
    unsigned n = 0;
    *serial = 0;
    for (unsigned i = 0; text[i]; i++) {
        char c = text[i];
        if (i == 4 && c == '-') continue;
        if (n == 8) return false;
        if (c >= 'A' && c <= 'F') c += 'a' - 'A';
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        compact[n++] = c;
        *serial = *serial * 16 + (c <= '9' ? c - '0' : c - 'a' + 10);
    }
    compact[n] = 0;
    return n == 8;
}
static struct block_device *find_volume(uint32_t serial) {
    struct block_device *found = NULL;
    for (uint64_t i = 0; i < BLOCK_MAX_DEVICES; i++) {
        struct block_device *device = block_device_at(i);
        if (!device || !usb_storage_is_device(device)) continue;
        uint8_t b[512];
        if (!block_read(device, 0, b, sizeof(b))) continue;
        static const char magic[] = "EXFAT   ";
        bool valid = b[510] == 0x55 && b[511] == 0xaa;
        for (unsigned j = 0; j < 8; j++) valid = valid && b[3 + j] == magic[j];
        uint32_t id = b[100] | (uint32_t)b[101] << 8 | (uint32_t)b[102] << 16 | (uint32_t)b[103] << 24;
        if (!valid || id != serial) continue;
        if (found) return NULL;
        found = device;
    }
    return found;
}
static bool marker_valid(const char uuid[9]) {
    const struct vfs_node *dir = vfs_lookup("/work/srvros-work");
    const struct vfs_node *node = vfs_lookup("/work/srvros-work/.workspace");
    char expected[] = "SRVROS-WORKSPACE-V1 00000000\n";
    for (unsigned i = 0; i < 8; i++) expected[sizeof("SRVROS-WORKSPACE-V1 ") - 1 + i] = uuid[i];
    if (!dir || dir->type != VFS_NODE_DIRECTORY || !node || node->type != VFS_NODE_FILE ||
        node->size != sizeof(expected) - 1) return false;
    const uint8_t *data;
    uint64_t size;
    if (!vfs_read_all(node, &data, &size)) return false;
    bool ok = size == sizeof(expected) - 1;
    for (unsigned i = 0; ok && i < size; i++) ok = data[i] == (uint8_t)expected[i];
    vfs_release_data(node, data);
    return ok;
}
bool workspace_active(void) { return active != NULL; }
void workspace_status(void) {
    if (!active) console_write("workspace: not mounted\n");
    else console_printf("workspace: UUID=%s path=/work/srvros-work state=%s\n",
        active_uuid, failed || !exfat_is_writable("/work") ? "write-failed" : "ready");
}
bool workspace_mount(const char *uuid) {
    char compact[9]; uint32_t serial;
    if (!parse_uuid(uuid, compact, &serial)) { console_write("workspace: invalid UUID\n"); return false; }
    struct scope scope;
    if (!begin(&scope)) { end(&scope); return false; }
    bool ok = false;
    if (active || vfs_lookup("/work")) { console_write("workspace: /work is busy\n"); goto out; }
    struct block_device *device = find_volume(serial);
    struct block_device *parent = block_parent(device);
    if (!device || !parent || parent->write) {
        console_write("workspace: USB volume absent, ambiguous or already writable\n"); goto out;
    }
    if (!exfat_mount_block_device("/work", device)) goto out;
    if (!marker_valid(compact)) {
        console_write("workspace: prepared marker missing or invalid\n");
        exfat_unmount("/work"); goto out;
    }
    if (!usb_storage_enable_volume_writes(device) || !exfat_is_writable("/work")) {
        console_write("workspace: volume cannot be written; check it offline\n");
        usb_storage_disable_volume_writes(device); exfat_unmount("/work"); goto out;
    }
    active = device; failed = false;
    for (unsigned i = 0; i < 9; i++) active_uuid[i] = compact[i];
    console_write("workspace: ready at /work/srvros-work\n");
    console_write("workspace: run /fat/bin/sh, then cd /work/srvros-work\n");
    ok = true;
out:
    end(&scope); return ok;
}
static bool sync_locked(void) {
    if (!active || failed) return false;
    struct block_device *parent = block_parent(active);
    if (process_file_sync_all() < 0 || !exfat_is_writable("/work") ||
        !parent || !parent->flush || !parent->flush(parent)) {
        failed = true; console_write("workspace: flush failed; remains mounted\n"); return false;
    }
    return true;
}
bool workspace_sync(void) {
    struct scope scope;
    if (!begin(&scope)) { end(&scope); return false; }
    bool ok = sync_locked();
    if (ok) console_write("workspace: flushed\n");
    end(&scope); return ok;
}
bool workspace_unmount(void) {
    struct scope scope;
    if (!begin(&scope)) { end(&scope); return false; }
    bool ok = false;
    if (!active) { console_write("workspace: not mounted\n"); goto out; }
    if (process_has_open_vfs_prefix("/work")) { console_write("workspace: busy; close files first\n"); goto out; }
    if (!sync_locked()) goto out;
    if (!exfat_unmount("/work") || !usb_storage_disable_volume_writes(active)) {
        failed = true; goto out;
    }
    active = NULL; failed = false;
    console_write("workspace: unmounted; USB writes disabled\n");
    ok = true;
out:
    end(&scope); return ok;
}
void workspace_boot(const char *cmdline) {
    static const char key[] = "srvros.workspace-volume=";
    char uuid[10]; unsigned matches = 0;
    for (const char *p = cmdline; p && *p;) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        unsigned i = 0;
        while (i < sizeof(key) - 1 && start + i < p && start[i] == key[i]) i++;
        if (i != sizeof(key) - 1) continue;
        if (++matches != 1 || p - start - i != 8) {
            console_write("workspace: invalid boot option\n"); return;
        }
        for (unsigned j = 0; j < 8; j++) uuid[j] = start[i + j];
        uuid[8] = 0;
    }
    if (matches == 1) workspace_mount(uuid);
}
