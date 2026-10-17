/* Bounded, repeatable file operations in a fresh per-run USB directory.
 * The host independently verifies the retained tree after reboot. */
#include <srvros/autotest_storage.h>
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/vfs.h>
#include <stddef.h>
#include <stdint.h>

#define LARGE_BYTES (384u * 1024u + 37u)
static const char anchor[] = "Unchanged anchor: AaZz 0123456789\n";
static const char replacement[] = "Overwritten: AaZz\n";
static const char appended[] = "Appended: 0123456789\n";

static void path_join(char out[160], const char *root, const char *suffix) {
    unsigned n = 0;
    while (*root) out[n++] = *root++;
    while (*suffix) out[n++] = *suffix++;
    out[n] = 0; /* Callers use a validated 16-digit run and constant suffixes. */
}
static uint8_t pattern(unsigned i) { return (uint8_t)(i * 37u + (i >> 8) + 11u); }
static bool contents(const char *path, const uint8_t *expected, unsigned length) {
    const struct vfs_node *node = vfs_lookup(path);
    if (!node || node->type != VFS_NODE_FILE || node->size != length) return false;
    const uint8_t *data;
    uint64_t size;
    if (!vfs_read_all(node, &data, &size)) return false;
    bool ok = size == length;
    for (unsigned i = 0; ok && i < length; i++) ok = data[i] == expected[i];
    vfs_release_data(node, data);
    return ok;
}
static bool is_directory(const char *path) {
    const struct vfs_node *node = vfs_lookup(path);
    return node && node->type == VFS_NODE_DIRECTORY;
}
static bool append_file(const char *path) {
    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data;
    uint64_t size;
    if (!node || !vfs_read_all(node, &data, &size)) return false;
    uint8_t combined[sizeof(replacement) + sizeof(appended) - 2];
    bool ok = size == sizeof(replacement) - 1;
    if (ok) {
        for (unsigned i = 0; i < size; i++) combined[i] = data[i];
        for (unsigned i = 0; i < sizeof(appended) - 1; i++) combined[size + i] = appended[i];
    }
    vfs_release_data(node, data);
    /* exFAT currently implements append as read/extend/replace, as the shell does. */
    return ok && exfat_write_file(path, combined, sizeof(combined));
}

bool autotest_storage_run(struct block_device *device, const char *run) {
    if (!run) return false;
    for (unsigned i = 0; i < 16; i++)
        if (!((run[i] >= '0' && run[i] <= '9') || (run[i] >= 'a' && run[i] <= 'f'))) return false;
    if (run[16]) return false;
    char root[160], path[160], other[160];
    path_join(root, "/testlog/srvros-tests/", run);
    path_join(path, root, ".files");
    path_join(root, path, "");
    struct block_device *parent = block_parent(device);
    if (!parent || !parent->flush || vfs_lookup(root)) return false;
    struct exfat_fsinfo info;
    /* Leave room for COW replacements, sidecar updates and the final report. */
    if (!exfat_statfs("/testlog", &info) || !info.block_size ||
        info.blocks_available < (8u * 1024u * 1024u + info.block_size - 1) / info.block_size + 32) return false;
    uint8_t *large = kmalloc(LARGE_BYTES);
    if (!large) return false;
    for (unsigned i = 0; i < LARGE_BYTES; i++) large[i] = pattern(i);
    bool ok = false;
    if (!exfat_create_directory(root)) goto out;
    path_join(path, root, "/nested");
    if (!exfat_create_directory(path)) goto out;
    path_join(path, root, "/nested/deep");
    if (!exfat_create_directory(path)) goto out;
    path_join(path, root, "/anchor.txt");
    if (!exfat_write_file(path, (const uint8_t *)anchor, sizeof(anchor) - 1)) goto out;
    path_join(path, root, "/nested/deep/edit.txt");
    if (!exfat_write_file(path, (const uint8_t *)"initial", 7) ||
        !exfat_write_file(path, (const uint8_t *)replacement, sizeof(replacement) - 1) ||
        !contents(path, (const uint8_t *)replacement, sizeof(replacement) - 1) || !append_file(path)) goto out;
    path_join(other, root, "/Renamed-MixedCase.txt");
    if (!exfat_rename(path, other) || vfs_lookup(path)) goto out;
    path_join(path, root, "/empty.txt");
    if (!exfat_write_file(path, NULL, 0)) goto out;
    path_join(path, root, "/truncated.txt");
    if (!exfat_write_file(path, large, 8193) || !exfat_write_file(path, NULL, 0)) goto out;
    path_join(path, root, "/nested/deep/large.bin");
    if (!exfat_write_file(path, large, 97) || !exfat_write_file(path, large, LARGE_BYTES)) goto out;
    path_join(path, root, "/shrunk.bin");
    if (!exfat_write_file(path, large, 8193) || !exfat_write_file(path, large, 17)) goto out;
    path_join(path, root, "/deleted.txt");
    if (!exfat_write_file(path, large, 8193) || !exfat_delete_file(path) || vfs_lookup(path)) goto out;
    path_join(path, root, "/removed");
    if (!exfat_create_directory(path) || !exfat_delete_directory(path) || vfs_lookup(path)) goto out;
    path_join(path, root, "/before");
    path_join(other, root, "/after");
    if (!exfat_create_directory(path) || !exfat_rename(path, other) || vfs_lookup(path)) goto out;
    /* Discard cached VFS names/lengths before verifying directory mutations. */
    if (!parent->flush(parent) || !exfat_unmount("/testlog") ||
        !exfat_mount_block_device("/testlog", device)) goto out;
    path_join(path, root, "/anchor.txt");
    if (!contents(path, (const uint8_t *)anchor, sizeof(anchor) - 1)) goto out;
    uint8_t combined[sizeof(replacement) + sizeof(appended) - 2];
    for (unsigned i = 0; i < sizeof(replacement) - 1; i++) combined[i] = replacement[i];
    for (unsigned i = 0; i < sizeof(appended) - 1; i++) combined[sizeof(replacement) - 1 + i] = appended[i];
    path_join(path, root, "/Renamed-MixedCase.txt");
    if (!contents(path, combined, sizeof(combined))) goto out;
    path_join(path, root, "/nested/deep/large.bin");
    if (!contents(path, large, LARGE_BYTES)) goto out;
    path_join(path, root, "/shrunk.bin");
    if (!contents(path, large, 17)) goto out;
    const char *empty[] = {"/empty.txt", "/truncated.txt"};
    for (unsigned i = 0; i < 2; i++) {
        path_join(path, root, empty[i]);
        if (!contents(path, NULL, 0)) goto out;
    }
    const char *absent[] = {"/nested/deep/edit.txt", "/deleted.txt", "/removed", "/before"};
    for (unsigned i = 0; i < 4; i++) {
        path_join(path, root, absent[i]);
        if (vfs_lookup(path)) goto out;
    }
    path_join(path, root, "/after");
    if (!is_directory(path)) goto out;
    ok = true;
out:
    kfree(large);
    return ok;
}
