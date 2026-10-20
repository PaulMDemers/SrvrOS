/* Diagnostic filesystem inside an exclusive, preallocated USB file. */
#include <srvros/workspace_image.h>
#include <srvros/workspace.h>
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/console.h>
#include <stddef.h>
#include <stdint.h>

static struct {
    bool open, poisoned, mounted;
    struct block_device *volume, *parent;
    struct block_device view;
    block_write_fn write;
    block_flush_fn flush;
    uint64_t offset;
} image;

struct image_scope { uint64_t flags; bool services; };
static bool enter(struct image_scope *s) {
#ifdef SRVROS_WORKSPACE_IMAGE_HOST_TEST
    s->flags = 0;
#else
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(s->flags) : : "memory");
#endif
    s->services = scheduler_io_begin();
    return s->services || !scheduler_io_active();
}
static void leave(struct image_scope *s) {
    if (s->services) scheduler_io_end();
#ifndef SRVROS_WORKSPACE_IMAGE_HOST_TEST
    if (s->flags & (1ull << 9)) __asm__ volatile("sti" : : : "memory");
#endif
}
bool workspace_image_busy(void) { return image.open || image.poisoned; }
bool workspace_image_ready(void) {
    return image.open && !image.poisoned &&
        workspace_parked_device() == image.volume;
}
static bool valid_io(struct block_device *d, uint64_t off, const void *p, uint64_t n) {
    return d == &image.view && workspace_image_ready() && p && n &&
        off <= WORKSPACE_IMAGE_BYTES && n <= WORKSPACE_IMAGE_BYTES - off &&
        !(off % d->block_size) && !(n % d->block_size);
}
static bool image_read(struct block_device *d, uint64_t off, void *p, uint64_t n) {
    if (!valid_io(d, off, p, n)) return false;
    bool ok = image.parent->read(image.parent, image.offset + off, p, n);
    if (!ok) image.poisoned = true;
    return ok;
}
static bool image_write(struct block_device *d, uint64_t off, const void *p, uint64_t n) {
    if (!valid_io(d, off, p, n) || !image.mounted) return false;
    bool ok = image.write(image.parent, image.offset + off, p, n);
    /* The parent cache must never retain bytes changed through this view. */
    block_forget(image.parent);
    if (!ok) image.poisoned = true;
    return ok;
}
static bool image_flush(struct block_device *d) {
    if (d != &image.view || !workspace_image_ready()) return false;
    bool ok = image.flush(image.parent);
    if (!ok) image.poisoned = true;
    return ok;
}
static void revoke(void) {
    block_forget(&image.view);
    block_forget(image.parent);
    image.view.used = false;
    image.view.read = NULL; image.view.write = NULL; image.view.flush = NULL;
    image.write = NULL; image.flush = NULL;
    image.open = false; image.volume = NULL; image.parent = NULL;
}
static bool marker_valid(void) {
    static const char expected[] = "SRVROS-BATTERY-FS-V1\n";
    const struct vfs_node *n = vfs_lookup("/battery-test/fixture/.battery-test");
    const uint8_t *data; uint64_t size;
    if (!n || n->type != VFS_NODE_FILE || n->size != sizeof(expected)-1 ||
        !vfs_read_all(n, &data, &size)) return false;
    bool ok = size == sizeof(expected)-1;
    for (unsigned i = 0; ok && i < size; i++) ok = data[i] == (uint8_t)expected[i];
    vfs_release_data(n, data);
    return ok;
}
bool workspace_image_open(void) {
    struct image_scope scope;
    if (!enter(&scope)) { leave(&scope); return false; }
    bool ok = false;
    struct block_device *volume, *parent;
    uint64_t offset, raw;
    if (workspace_image_busy() || !workspace_active() || vfs_lookup("/battery-test") ||
        process_has_open_vfs_prefix("/work") ||
        !workspace_sync() ||
        !exfat_exclusive_range(WORKSPACE_IMAGE_PATH, WORKSPACE_IMAGE_BYTES, &volume, &offset) ||
        !block_raw_range(volume, offset, WORKSPACE_IMAGE_BYTES, &parent, &raw) ||
        !parent->read || !parent->write || !parent->flush ||
        !parent->block_size || raw % parent->block_size ||
        WORKSPACE_IMAGE_BYTES % parent->block_size) goto out;
    block_write_fn write = parent->write;
    block_flush_fn flush = parent->flush;
    if (!workspace_unmount() || workspace_parked_device() != volume) goto out;
    image.volume = volume; image.parent = parent; image.offset = raw;
    image.write = write; image.flush = flush; image.open = true;
    image.view = (struct block_device){.used=true, .name="battery-image",
        .block_size=parent->block_size, .block_count=WORKSPACE_IMAGE_BYTES/parent->block_size,
        .max_transfer_bytes=4096, .read=image_read};
    block_forget(parent);
    uint8_t boot[512];
    /* Prepared fixture only: clean, 512-byte sectors, fixed diagnostic identity.
     * Read via block_read for hosts with larger physical sector sizes. */
    if (!block_read(&image.view, 0, boot, sizeof(boot)) ||
        boot[100]!=0x31 || boot[101]!=0x54 || boot[102]!=0x41 || boot[103]!=0x42 ||
        boot[106] || boot[107] || boot[108]!=9) goto refused;
    if (!exfat_mount_block_device("/battery-test", &image.view)) goto refused;
    image.mounted = true;
    struct exfat_check_result check;
    if (!marker_valid() || vfs_lookup("/battery-test/fixture/fs-proof") ||
        !exfat_check("/battery-test", &check)) goto refused;
    image.view.write = image_write; image.view.flush = image_flush;
    if (!exfat_is_writable("/battery-test")) goto refused;
    console_printf("workspace-image: prepared path=%s raw_offset=%u bytes=%u; unmounted=yes aliases=revoked\n",
        WORKSPACE_IMAGE_PATH, raw, (uint64_t)WORKSPACE_IMAGE_BYTES);
    ok = true;
    goto out;
refused:
    if (image.mounted) { exfat_unmount("/battery-test"); image.mounted = false; }
    revoke();
out:
    leave(&scope); return ok;
}
bool workspace_image_flush(void) {
    struct image_scope scope;
    if (!enter(&scope)) { leave(&scope); return false; }
    bool ok = image_flush(&image.view);
    leave(&scope); return ok;
}
bool workspace_image_close(void) {
    struct image_scope scope;
    if (!enter(&scope)) { leave(&scope); return false; }
    bool ok = false;
    if (!image.open || process_has_open_vfs_prefix("/battery-test")) goto out;
    ok = workspace_image_ready() && image_flush(&image.view);
    if (image.mounted && !exfat_unmount("/battery-test")) ok = false;
    image.mounted = false;
    if (!ok) image.poisoned = true;
    revoke();
    console_printf("workspace-image: revoked clean=%s\n", ok ? "yes" : "NO");
out:
    leave(&scope); return ok;
}

#define TEST_BYTES (256u * 1024u + 37u)
static uint8_t test_data[TEST_BYTES];
static bool matches(const char *path, uint64_t length) {
    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data; uint64_t size;
    if (!node || !vfs_read_all(node, &data, &size)) return false;
    bool ok = size == length;
    for (uint64_t i = 0; ok && i < size; i++) ok = data[i] == test_data[i];
    vfs_release_data(node, data);
    return ok;
}
bool workspace_image_cycle(uint64_t cycle) {
    struct image_scope scope;
    if (!enter(&scope)) { leave(&scope); return false; }
    const char *dir = "/battery-test/fixture/fs-proof";
    const char *draft = "/battery-test/fixture/fs-proof/draft.bin";
    const char *payload = "/battery-test/fixture/fs-proof/payload.bin";
    const char *empty = "/battery-test/fixture/fs-proof/empty.bin";
    const char *tmp = "/battery-test/fixture/fs-proof/transient";
    const char *moved = "/battery-test/fixture/fs-proof/moved";
    bool ok = false;
    if (!workspace_image_ready() || !image.mounted) goto out;
    if (!cycle && (vfs_lookup(dir) || !exfat_create_directory(dir))) goto out;
    for (unsigned i = 0; i < TEST_BYTES; i++) test_data[i] = (uint8_t)(i*37u+(i>>8)+cycle);
    ok = exfat_write_file(draft,test_data,TEST_BYTES) && matches(draft,TEST_BYTES) &&
        exfat_write_file(draft,test_data,37) && matches(draft,37) &&
        exfat_write_file(draft,test_data,TEST_BYTES) &&
        (!cycle || exfat_delete_file(payload)) && exfat_rename(draft,payload) && !vfs_lookup(draft) &&
        matches(payload,TEST_BYTES) && exfat_write_file(empty,test_data,4096) &&
        exfat_write_file(empty,NULL,0) && matches(empty,0) &&
        exfat_create_directory(tmp) && exfat_rename(tmp,moved) && !vfs_lookup(tmp) &&
        exfat_delete_directory(moved) && !vfs_lookup(moved) && image_flush(&image.view);
out:
    leave(&scope); return ok;
}
