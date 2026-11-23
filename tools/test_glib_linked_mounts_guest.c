/* Compare real GioUnix mount APIs with the native kernel snapshot. */
#include <gio/gunixmounts.h>
#include <srvros/sys.h>
#include <srvros/syscall_numbers.h>
#include <stdio.h>
#include <string.h>

#ifndef GIO_MOUNTS_EXPECT_EXFAT
#error Define GIO_MOUNTS_EXPECT_EXFAT to 0 or 1
#endif
#if GIO_MOUNTS_EXPECT_EXFAT != 0 && GIO_MOUNTS_EXPECT_EXFAT != 1
#error Invalid exFAT expectation
#endif
#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("gio-mounts: failure %s line=%d\n", phase, __LINE__); return 1; \
} } while (0)

static int matches(GUnixMountEntry *entry, const struct srv_mount_entry *native) {
    const char *type = native->filesystem == SRV_MOUNT_VFS ? "srvros-vfs" : "exfat";
    int internal = native->filesystem == SRV_MOUNT_VFS || (native->flags & SRV_MOUNT_PROJECTION);
    int readonly = (native->flags & SRV_MOUNT_READONLY) != 0;
    return strcmp(g_unix_mount_entry_get_mount_path(entry), native->mount_path) == 0 &&
           strcmp(g_unix_mount_entry_get_root_path(entry), native->root_path) == 0 &&
           strcmp(g_unix_mount_entry_get_device_path(entry), native->source) == 0 &&
           strcmp(g_unix_mount_entry_get_fs_type(entry), type) == 0 &&
           strcmp(g_unix_mount_entry_get_options(entry), readonly ? "ro" : "rw") == 0 &&
           !!g_unix_mount_entry_is_readonly(entry) == readonly &&
           !!g_unix_mount_entry_is_system_internal(entry) == !!internal;
}

int main(void) {
    struct srv_mount_snapshot snapshot = {0}, unchanged;
    memset(&unchanged, 0xa5, sizeof(unchanged));
    snapshot = unchanged;
    CHECK("wrong-size", srv_syscall2(SYS_MOUNT_SNAPSHOT, (long)&snapshot, sizeof(snapshot) - 1) == SRV_ERR_INVAL &&
          memcmp(&snapshot, &unchanged, sizeof(snapshot)) == 0);
    CHECK("null", srv_syscall2(SYS_MOUNT_SNAPSHOT, 0, sizeof(snapshot)) == SRV_ERR_FAULT);
    CHECK("snapshot", srv_get_mount_snapshot(&snapshot) == 0 &&
          snapshot.version == SRV_MOUNT_SNAPSHOT_VERSION && snapshot.count == (unsigned)(1 + GIO_MOUNTS_EXPECT_EXFAT));
    const struct srv_mount_entry *root = NULL, *fat = NULL;
    for (unsigned i = 0; i < snapshot.count; ++i) {
        const struct srv_mount_entry *item = &snapshot.entries[i];
        CHECK("native-strings", memchr(item->mount_path, 0, sizeof(item->mount_path)) &&
              memchr(item->root_path, 0, sizeof(item->root_path)) && memchr(item->source, 0, sizeof(item->source)));
        if (strcmp(item->mount_path, "/") == 0) { CHECK("root-unique", !root); root = item; }
        else if (strcmp(item->mount_path, "/fat") == 0) { CHECK("fat-unique", !fat); fat = item; }
        else CHECK("unexpected-native-row", 0);
    }
    CHECK("native-root", root && root->filesystem == SRV_MOUNT_VFS && !(root->flags & SRV_MOUNT_PROJECTION));
    CHECK("native-fat", GIO_MOUNTS_EXPECT_EXFAT ? (fat && fat->filesystem == SRV_MOUNT_EXFAT) : !fat);
    GList *entries = g_unix_mount_entries_get(NULL);
    CHECK("gio-count", g_list_length(entries) == snapshot.count);
    for (unsigned i = 0; i < snapshot.count; ++i) {
        unsigned found = 0;
        for (GList *it = entries; it; it = it->next)
            if (matches(it->data, &snapshot.entries[i])) ++found;
        CHECK("gio-row", found == 1);
    }
    GUnixMountEntry *entry = g_unix_mount_entry_at("/", NULL);
    CHECK("at-root", entry && matches(entry, root));
    g_unix_mount_entry_free(entry);
    entry = g_unix_mount_entry_for("/gio-mounts", NULL);
    CHECK("file-root", entry && matches(entry, root));
    g_unix_mount_entry_free(entry);
    entry = g_unix_mount_entry_for("/gio-mounts-does-not-exist", NULL);
    CHECK("missing", entry == NULL);
    entry = g_unix_mount_entry_at("/fat", NULL);
    CHECK("at-fat", GIO_MOUNTS_EXPECT_EXFAT ? (entry && matches(entry, fat)) : entry == NULL);
    if (entry) g_unix_mount_entry_free(entry);
    if (GIO_MOUNTS_EXPECT_EXFAT) {
        entry = g_unix_mount_entry_for("/fat/hello.html", NULL);
        CHECK("file-fat", entry && matches(entry, fat));
        g_unix_mount_entry_free(entry);
    }
    guint64 timestamp = 123;
    GList *points = g_unix_mount_points_get(&timestamp);
    CHECK("no-fstab", points == NULL && timestamp == 0);
    g_list_free_full(points, (GDestroyNotify)g_unix_mount_point_free);
    size_t count = 123; uint64_t read_time = 123;
    GUnixMountEntry **file_entries = g_unix_mount_entries_get_from_file("/missing-mount-table", &read_time, &count);
    CHECK("no-file-parser", file_entries == NULL && count == 0 && read_time == 0);
    g_list_free_full(entries, (GDestroyNotify)g_unix_mount_entry_free);
    printf("gio-mounts: exfat=%d\n", GIO_MOUNTS_EXPECT_EXFAT);
    puts("gio-mounts: ok");
    return 0;
}
