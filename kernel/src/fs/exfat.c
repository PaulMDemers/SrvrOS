#include <srvros/console.h>
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/vfs.h>

#include <stddef.h>
#include <stdint.h>

#define EXFAT_MAX_VOLUMES 4
#define EXFAT_MAX_FILES 512
#define EXFAT_MAX_DIRS 128
#define EXFAT_MAX_PATH 160
#define EXFAT_MAX_FILE_ENTRIES 19
#define EXFAT_METADATA_MAX_BYTES 65536
#define EXFAT_ENTRY_SIZE 32
#define EXFAT_ENTRY_END 0x00
#define EXFAT_ENTRY_BITMAP 0x81
#define EXFAT_ENTRY_UPCASE 0x82
#define EXFAT_ENTRY_FILE 0x85
#define EXFAT_ENTRY_STREAM 0xc0
#define EXFAT_ENTRY_NAME 0xc1
#define EXFAT_ATTR_DIRECTORY 0x0010
#define EXFAT_FAT_CHAIN_END 0xfffffff8u

struct exfat_boot_sector {
    uint8_t jump_boot[3];
    char filesystem_name[8];
    uint8_t must_be_zero[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_directory_cluster;
    uint32_t volume_serial_number;
    uint16_t filesystem_revision;
    uint16_t volume_flags;
    uint8_t bytes_per_sector_shift;
    uint8_t sectors_per_cluster_shift;
    uint8_t number_of_fats;
    uint8_t drive_select;
    uint8_t percent_in_use;
    uint8_t reserved[7];
    uint8_t boot_code[390];
    uint16_t boot_signature;
} __attribute__((packed));

struct exfat_file_entry {
    uint8_t entry_type;
    uint8_t secondary_count;
    uint16_t set_checksum;
    uint16_t file_attributes;
    uint8_t reserved[26];
} __attribute__((packed));

struct exfat_stream_entry {
    uint8_t entry_type;
    uint8_t flags;
    uint8_t reserved0;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved1;
    uint64_t valid_data_length;
    uint32_t reserved2;
    uint32_t first_cluster;
    uint64_t data_length;
} __attribute__((packed));

struct exfat_name_entry {
    uint8_t entry_type;
    uint8_t flags;
    uint16_t name[15];
} __attribute__((packed));

struct exfat_volume {
    const uint8_t *image;
    struct block_device *device;
    uint64_t image_size;
    uint64_t sector_size;
    uint64_t sectors_per_cluster;
    uint64_t cluster_size;
    uint64_t fat_offset_bytes;
    uint64_t cluster_heap_offset_bytes;
    uint32_t allocation_bitmap_cluster;
    uint64_t allocation_bitmap_size;
    uint32_t upcase_table_cluster;
    uint64_t upcase_table_size;
    uint32_t cluster_count;
    uint32_t root_directory_cluster;
};

struct exfat_file {
    struct exfat_volume *volume;
    uint32_t first_cluster;
    uint64_t size;
    uint64_t allocated_clusters;
    uint64_t file_entry_offset;
    uint64_t stream_entry_offset;
    uint8_t secondary_count;
    bool no_fat_chain;
};

struct exfat_directory {
    char path[EXFAT_MAX_PATH];
    uint32_t first_cluster;
    uint64_t data_length;
    uint64_t allocated_clusters;
    uint64_t file_entry_offset;
    uint8_t secondary_count;
    bool no_fat_chain;
};

struct exfat_mount {
    bool used;
    struct exfat_volume volume;
    struct exfat_file files[EXFAT_MAX_FILES];
    char paths[EXFAT_MAX_FILES][EXFAT_MAX_PATH];
    struct exfat_directory dirs[EXFAT_MAX_DIRS];
    char mountpoint[EXFAT_MAX_PATH];
    uint64_t file_count;
    uint64_t dir_count;
};

static struct exfat_mount mounts[EXFAT_MAX_VOLUMES];
static bool metadata_syncing;
static char metadata_buffer[EXFAT_METADATA_MAX_BYTES];

static bool streq(const char *a, const char *b);
static bool allocate_contiguous_clusters(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster);
static bool allocate_clusters(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster, bool *no_fat_chain);
static bool free_file_clusters(struct exfat_file *file);

static void copy_cstr(char *dst, uint64_t capacity, const char *src) {
    uint64_t i = 0;
    if (dst == NULL || capacity == 0) {
        return;
    }
    while (src != NULL && src[i] != '\0' && i + 1 < capacity) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool streqn(const char *a, const char *b, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static uint16_t read_u16(const void *data) {
    const uint8_t *bytes = data;
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_u32(const void *data) {
    const uint8_t *bytes = data;
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

static uint64_t read_u64(const void *data) {
    const uint8_t *bytes = data;
    return (uint64_t)bytes[0] |
        ((uint64_t)bytes[1] << 8) |
        ((uint64_t)bytes[2] << 16) |
        ((uint64_t)bytes[3] << 24) |
        ((uint64_t)bytes[4] << 32) |
        ((uint64_t)bytes[5] << 40) |
        ((uint64_t)bytes[6] << 48) |
        ((uint64_t)bytes[7] << 56);
}

static void write_u16(void *data, uint16_t value) {
    uint8_t *bytes = data;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_u32(void *data, uint32_t value) {
    uint8_t *bytes = data;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_u64(void *data, uint64_t value) {
    uint8_t *bytes = data;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
    bytes[4] = (uint8_t)(value >> 32);
    bytes[5] = (uint8_t)(value >> 40);
    bytes[6] = (uint8_t)(value >> 48);
    bytes[7] = (uint8_t)(value >> 56);
}

static void copy_memory(void *dst, const void *src, uint64_t length) {
    uint8_t *out = dst;
    const uint8_t *in = src;
    for (uint64_t i = 0; i < length; i++) {
        out[i] = in[i];
    }
}

static bool volume_read(const struct exfat_volume *volume, uint64_t offset, void *buffer, uint64_t length) {
    if (volume == NULL || buffer == NULL || offset > volume->image_size || length > volume->image_size - offset) {
        return false;
    }

    if (volume->image != NULL) {
        copy_memory(buffer, volume->image + offset, length);
        return true;
    }

    return block_read(volume->device, offset, buffer, length);
}

static bool volume_write(const struct exfat_volume *volume, uint64_t offset, const void *buffer, uint64_t length) {
    if (volume == NULL || buffer == NULL || offset > volume->image_size || length > volume->image_size - offset) {
        return false;
    }
    if (volume->image != NULL) {
        return false;
    }
    return block_write(volume->device, offset, buffer, length);
}

static bool cluster_offset(const struct exfat_volume *volume, uint32_t cluster, uint64_t *offset) {
    if (cluster < 2 || cluster >= volume->cluster_count + 2) {
        return false;
    }

    uint64_t result = volume->cluster_heap_offset_bytes +
        ((uint64_t)cluster - 2) * volume->cluster_size;
    if (result >= volume->image_size || volume->cluster_size > volume->image_size - result) {
        return false;
    }

    *offset = result;
    return true;
}

static bool next_cluster(const struct exfat_volume *volume, uint32_t cluster, uint32_t *next) {
    uint64_t offset = volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t);
    uint8_t bytes[sizeof(uint32_t)];
    if (!volume_read(volume, offset, bytes, sizeof(bytes))) {
        return false;
    }

    *next = read_u32(bytes);
    return true;
}

static bool write_fat_entry(const struct exfat_volume *volume, uint32_t cluster, uint32_t value) {
    uint8_t bytes[sizeof(uint32_t)];
    write_u32(bytes, value);
    return volume_write(volume,
        volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t),
        bytes,
        sizeof(bytes));
}

static uint64_t append_mount_path(char *path, uint64_t capacity, const char *mountpoint) {
    uint64_t written = 0;
    while (mountpoint[written] != '\0' && written + 1 < capacity) {
        path[written] = mountpoint[written];
        written++;
    }
    if (written + 1 < capacity && (written == 0 || path[written - 1] != '/')) {
        path[written++] = '/';
    }
    return written;
}

static uint64_t append_parent_path(char *path, uint64_t capacity, const char *parent) {
    uint64_t written = 0;
    while (parent[written] != '\0' && written + 1 < capacity) {
        path[written] = parent[written];
        written++;
    }
    if (written + 1 < capacity && (written == 0 || path[written - 1] != '/')) {
        path[written++] = '/';
    }
    return written;
}

static void append_ascii_name(char *path, uint64_t *offset, uint64_t capacity, const uint16_t *name, uint64_t count) {
    for (uint64_t i = 0; i < count && *offset + 1 < capacity; i++) {
        uint16_t c = read_u16(&name[i]);
        path[(*offset)++] = (c >= 32 && c < 127) ? (char)c : '?';
    }
}

static void build_child_path(char *path,
    uint64_t capacity,
    const char *parent,
    const struct exfat_stream_entry *stream,
    const struct exfat_name_entry *names,
    uint64_t name_entry_count) {
    uint64_t path_offset = append_parent_path(path, capacity, parent);
    uint64_t remaining = stream->name_length;
    for (uint64_t i = 0; i < name_entry_count && remaining > 0; i++) {
        uint64_t count = remaining > 15 ? 15 : remaining;
        append_ascii_name(path, &path_offset, capacity, names[i].name, count);
        remaining -= count;
    }
    path[path_offset] = '\0';
}

static bool exfat_read_all(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    const struct exfat_file *file = node->private_data;
    uint8_t *buffer = kmalloc(file->size == 0 ? 1 : file->size);
    uint64_t copied = 0;
    uint32_t cluster = file->first_cluster;

    if (buffer == NULL) {
        return false;
    }

    while (copied < file->size) {
        uint64_t source_offset;
        if (!cluster_offset(file->volume, cluster, &source_offset)) {
            kfree(buffer);
            return false;
        }

        uint64_t chunk = file->volume->cluster_size;
        if (chunk > file->size - copied) {
            chunk = file->size - copied;
        }

        if (!volume_read(file->volume, source_offset, buffer + copied, chunk)) {
            kfree(buffer);
            return false;
        }
        copied += chunk;

        if (copied < file->size) {
            if (file->no_fat_chain) {
                cluster++;
            } else if (!next_cluster(file->volume, cluster, &cluster) || cluster >= EXFAT_FAT_CHAIN_END) {
                kfree(buffer);
                return false;
            }
        }
    }

    *data = buffer;
    *size = file->size;
    return true;
}

static void exfat_release_data(const struct vfs_node *node, const uint8_t *data) {
    (void)node;
    kfree((void *)data);
}

static uint64_t clusters_for_size(const struct exfat_volume *volume, uint64_t size) {
    if (size == 0) {
        return 0;
    }
    return (size + volume->cluster_size - 1) / volume->cluster_size;
}

static bool mount_has_file_slot(const struct exfat_mount *mount) {
    if (mount == NULL) {
        return false;
    }
    if (mount->file_count < EXFAT_MAX_FILES) {
        return true;
    }
    for (uint64_t i = 0; i < EXFAT_MAX_FILES; i++) {
        if (mount->paths[i][0] == '\0') {
            return true;
        }
    }
    return false;
}

static bool register_file(struct exfat_mount *mount,
    const char *parent,
    const struct exfat_file_entry *file_entry,
    const struct exfat_stream_entry *stream,
    const struct exfat_name_entry *names,
    uint64_t name_entry_count,
    uint64_t file_entry_offset,
    uint64_t stream_entry_offset) {
    if (mount == NULL ||
        (file_entry->file_attributes & EXFAT_ATTR_DIRECTORY) != 0 ||
        (stream->data_length != 0 && stream->first_cluster < 2)) {
        return false;
    }

    uint64_t index = EXFAT_MAX_FILES;
    for (uint64_t i = 0; i < mount->file_count; i++) {
        if (mount->paths[i][0] == '\0') {
            index = i;
            break;
        }
    }
    if (index == EXFAT_MAX_FILES) {
        if (mount->file_count >= EXFAT_MAX_FILES) {
            return false;
        }
        index = mount->file_count;
    }

    build_child_path(mount->paths[index],
        sizeof(mount->paths[index]),
        parent,
        stream,
        names,
        name_entry_count);

    mount->files[index] = (struct exfat_file) {
        .volume = &mount->volume,
        .first_cluster = stream->first_cluster,
        .size = stream->data_length,
        .allocated_clusters = clusters_for_size(&mount->volume, stream->data_length),
        .file_entry_offset = file_entry_offset,
        .stream_entry_offset = stream_entry_offset,
        .secondary_count = file_entry->secondary_count,
        .no_fat_chain = (stream->flags & 0x02) != 0,
    };

    if (!vfs_register_file_with_release(mount->paths[index],
            mount->files[index].size,
            &mount->files[index],
            exfat_read_all,
            exfat_release_data)) {
        return false;
    }

    if (index == mount->file_count) {
        mount->file_count++;
    }
    return true;
}

static bool register_directory(struct exfat_mount *mount,
    const char *path,
    uint32_t first_cluster,
    bool no_fat_chain,
    uint64_t data_length,
    uint64_t file_entry_offset,
    uint8_t secondary_count) {
    if (mount == NULL || path == NULL || path[0] == '\0') {
        return false;
    }
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        if (streq(mount->dirs[i].path, path)) {
            return vfs_register_directory(mount->dirs[i].path);
        }
    }
    if (mount->dir_count >= EXFAT_MAX_DIRS) {
        return false;
    }

    uint64_t index = mount->dir_count++;
    uint64_t i = 0;
    while (path[i] != '\0' && i + 1 < sizeof(mount->dirs[index].path)) {
        mount->dirs[index].path[i] = path[i];
        i++;
    }
    mount->dirs[index].path[i] = '\0';
    mount->dirs[index].first_cluster = first_cluster;
    mount->dirs[index].data_length = data_length;
    mount->dirs[index].allocated_clusters = data_length == 0 ? 1 : clusters_for_size(&mount->volume, data_length);
    if (mount->dirs[index].allocated_clusters == 0) {
        mount->dirs[index].allocated_clusters = 1;
    }
    mount->dirs[index].no_fat_chain = no_fat_chain;
    mount->dirs[index].file_entry_offset = file_entry_offset;
    mount->dirs[index].secondary_count = secondary_count;
    return vfs_register_directory(mount->dirs[index].path);
}

static bool scan_directory(const char *path,
    struct exfat_mount *mount,
    uint32_t first_cluster,
    bool no_fat_chain,
    uint64_t data_length,
    uint64_t depth) {
    if (depth > 8) {
        return false;
    }

    struct exfat_volume *volume = &mount->volume;
    uint32_t cluster = first_cluster;
    uint64_t visited = 0;
    uint64_t scanned = 0;
    bool ok = true;
    uint8_t *cluster_buffer = kmalloc(volume->cluster_size);

    if (cluster_buffer == NULL) {
        return false;
    }

    while (cluster >= 2 && cluster < EXFAT_FAT_CHAIN_END && visited++ < volume->cluster_count) {
        uint64_t offset;
        if (!cluster_offset(volume, cluster, &offset)) {
            ok = false;
            break;
        }

        uint64_t cluster_limit = volume->cluster_size;
        if (data_length != 0 && data_length - scanned < cluster_limit) {
            cluster_limit = data_length - scanned;
        }

        if (!volume_read(volume, offset, cluster_buffer, cluster_limit)) {
            ok = false;
            break;
        }

        for (uint64_t entry_offset = 0; entry_offset + EXFAT_ENTRY_SIZE <= cluster_limit; entry_offset += EXFAT_ENTRY_SIZE) {
            const uint8_t *entry = cluster_buffer + entry_offset;
            if (entry[0] == EXFAT_ENTRY_END) {
                goto out;
            }

            if (entry[0] == EXFAT_ENTRY_BITMAP && depth == 0) {
                volume->allocation_bitmap_cluster = read_u32(entry + 20);
                volume->allocation_bitmap_size = read_u64(entry + 24);
                continue;
            }

            if (entry[0] == EXFAT_ENTRY_UPCASE && depth == 0) {
                volume->upcase_table_cluster = read_u32(entry + 20);
                volume->upcase_table_size = read_u64(entry + 24);
                continue;
            }

            if (entry[0] != EXFAT_ENTRY_FILE) {
                continue;
            }

            const struct exfat_file_entry *file_entry = (const struct exfat_file_entry *)entry;
            uint64_t remaining_entries = cluster_limit - entry_offset;
            if ((uint64_t)(file_entry->secondary_count + 1) * EXFAT_ENTRY_SIZE > remaining_entries) {
                ok = false;
                goto out;
            }

            const struct exfat_stream_entry *stream = NULL;
            const struct exfat_name_entry *names = NULL;
            uint64_t name_entry_count = 0;

            for (uint64_t i = 1; i <= file_entry->secondary_count; i++) {
                const uint8_t *secondary = entry + i * EXFAT_ENTRY_SIZE;
                if (secondary[0] == EXFAT_ENTRY_STREAM) {
                    stream = (const struct exfat_stream_entry *)secondary;
                } else if (secondary[0] == EXFAT_ENTRY_NAME) {
                    if (names == NULL) {
                        names = (const struct exfat_name_entry *)secondary;
                    }
                    name_entry_count++;
                }
            }

            if (stream != NULL && names != NULL) {
                uint64_t absolute_file_entry_offset = offset + entry_offset;
                uint64_t absolute_stream_entry_offset = (const uint8_t *)stream - cluster_buffer + offset;
                if ((file_entry->file_attributes & EXFAT_ATTR_DIRECTORY) != 0) {
                    char child_path[EXFAT_MAX_PATH];
                    build_child_path(child_path, sizeof(child_path), path, stream, names, name_entry_count);
                    (void)register_directory(mount,
                        child_path,
                        stream->first_cluster,
                        (stream->flags & 0x02) != 0,
                        stream->data_length,
                        absolute_file_entry_offset,
                        file_entry->secondary_count);
                    if (!scan_directory(child_path,
                            mount,
                            stream->first_cluster,
                            (stream->flags & 0x02) != 0,
                            stream->data_length,
                            depth + 1)) {
                        ok = false;
                        goto out;
                    }
                } else {
                    register_file(mount,
                        path,
                        file_entry,
                        stream,
                        names,
                        name_entry_count,
                        absolute_file_entry_offset,
                        absolute_stream_entry_offset);
                }
            }

            entry_offset += (uint64_t)file_entry->secondary_count * EXFAT_ENTRY_SIZE;
        }

        scanned += cluster_limit;
        if (data_length != 0 && scanned >= data_length) {
            goto out;
        }

        if (no_fat_chain) {
            cluster++;
        } else if (!next_cluster(volume, cluster, &cluster)) {
            ok = false;
            break;
        }
    }

out:
    kfree(cluster_buffer);
    return ok;
}

static bool scan_root_directory(struct exfat_mount *mount) {
    char root_path[EXFAT_MAX_PATH];
    uint64_t root_length = append_mount_path(root_path, sizeof(root_path), mount->mountpoint);
    if (root_length > 0 && root_path[root_length - 1] == '/') {
        root_path[root_length - 1] = '\0';
    } else {
        root_path[root_length] = '\0';
    }

    (void)register_directory(mount, root_path, mount->volume.root_directory_cluster, false, 0, 0, 0);
    return scan_directory(root_path, mount, mount->volume.root_directory_cluster, false, 0, 0);
}

static void remember_mountpoint(struct exfat_mount *mount, const char *mountpoint) {
    uint64_t i = 0;
    while (mount != NULL && mountpoint != NULL && mountpoint[i] != '\0' && i + 1 < sizeof(mount->mountpoint)) {
        mount->mountpoint[i] = mountpoint[i];
        i++;
    }
    if (mount != NULL) {
        mount->mountpoint[i] = '\0';
    }
}

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

static bool mount_matches(const struct exfat_mount *mount, const char *mountpoint) {
    return mount != NULL &&
        mount->used &&
        (mountpoint == NULL || mountpoint[0] == '\0' || streq(mount->mountpoint, mountpoint));
}

static bool valid_mountpoint(const char *mountpoint) {
    return mountpoint != NULL && mountpoint[0] == '/' && mountpoint[1] != '\0';
}

static bool mountpoint_in_use(const char *mountpoint) {
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (mounts[i].used && streq(mounts[i].mountpoint, mountpoint)) {
            return true;
        }
    }
    return false;
}

static struct exfat_file *find_file(const char *path) {
    for (uint64_t mount_index = 0; mount_index < EXFAT_MAX_VOLUMES; mount_index++) {
        struct exfat_mount *mount = &mounts[mount_index];
        if (!mount->used) {
            continue;
        }
        for (uint64_t i = 0; i < mount->file_count; i++) {
            if (streq(mount->paths[i], path)) {
                return &mount->files[i];
            }
        }
    }
    return NULL;
}

static struct exfat_mount *find_mount_and_index_for_file(const char *path, uint64_t *index_out) {
    for (uint64_t mount_index = 0; mount_index < EXFAT_MAX_VOLUMES; mount_index++) {
        struct exfat_mount *mount = &mounts[mount_index];
        if (!mount->used) {
            continue;
        }
        for (uint64_t i = 0; i < mount->file_count; i++) {
            if (streq(mount->paths[i], path)) {
                if (index_out != NULL) {
                    *index_out = i;
                }
                return mount;
            }
        }
    }
    return NULL;
}

static const char *path_basename(const char *path) {
    const char *name = path;
    for (uint64_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            name = path + i + 1;
        }
    }
    return name[0] != '\0' ? name : NULL;
}

static bool parent_path_of(const char *path, char *parent, uint64_t capacity) {
    if (path == NULL || parent == NULL || capacity == 0 || path[0] != '/') {
        return false;
    }

    uint64_t length = 0;
    uint64_t last_slash = 0;
    while (path[length] != '\0') {
        if (path[length] == '/') {
            last_slash = length;
        }
        length++;
    }
    if (length == 0 || last_slash == 0 || last_slash + 1 >= capacity) {
        return false;
    }
    for (uint64_t i = 0; i < last_slash; i++) {
        parent[i] = path[i];
    }
    parent[last_slash] = '\0';
    return true;
}

static struct exfat_directory *find_directory_in_mount(struct exfat_mount *mount, const char *path) {
    if (mount == NULL || path == NULL) {
        return NULL;
    }
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        if (streq(mount->dirs[i].path, path)) {
            return &mount->dirs[i];
        }
    }
    return NULL;
}

static struct exfat_mount *find_mount_and_index_for_directory(const char *path, uint64_t *index_out) {
    for (uint64_t mount_index = 0; mount_index < EXFAT_MAX_VOLUMES; mount_index++) {
        struct exfat_mount *mount = &mounts[mount_index];
        if (!mount->used) {
            continue;
        }
        for (uint64_t i = 0; i < mount->dir_count; i++) {
            if (mount->dirs[i].path[0] != '\0' && streq(mount->dirs[i].path, path)) {
                if (index_out != NULL) {
                    *index_out = i;
                }
                return mount;
            }
        }
    }
    return NULL;
}

static struct exfat_mount *find_mount_and_parent_directory(const char *path,
    struct exfat_directory **directory_out,
    const char **name_out) {
    char parent[EXFAT_MAX_PATH];
    if (!parent_path_of(path, parent, sizeof(parent))) {
        return NULL;
    }
    const char *name = path_basename(path);
    if (name == NULL) {
        return NULL;
    }
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mounts[i].used) {
            continue;
        }
        struct exfat_directory *directory = find_directory_in_mount(&mounts[i], parent);
        if (directory != NULL) {
            if (directory_out != NULL) {
                *directory_out = directory;
            }
            if (name_out != NULL) {
                *name_out = name;
            }
            return &mounts[i];
        }
    }
    return NULL;
}

static uint64_t ascii_length(const char *s) {
    uint64_t length = 0;
    while (s[length] != '\0') {
        length++;
    }
    return length;
}

static bool append_text(char *out, uint64_t capacity, uint64_t *offset, const char *text) {
    if (out == NULL || offset == NULL || text == NULL) {
        return false;
    }
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (*offset + 1 >= capacity) {
            return false;
        }
        out[(*offset)++] = text[i];
    }
    out[*offset] = '\0';
    return true;
}

static bool append_char(char *out, uint64_t capacity, uint64_t *offset, char ch) {
    if (out == NULL || offset == NULL || *offset + 1 >= capacity) {
        return false;
    }
    out[(*offset)++] = ch;
    out[*offset] = '\0';
    return true;
}

static bool append_u64(char *out, uint64_t capacity, uint64_t *offset, uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && count < sizeof(digits));

    while (count > 0) {
        if (!append_char(out, capacity, offset, digits[--count])) {
            return false;
        }
    }
    return true;
}

static bool append_path_piece(char *out, uint64_t capacity, const char *piece) {
    uint64_t offset = ascii_length(out);
    return append_text(out, capacity, &offset, piece);
}

static bool path_under_mount(const struct exfat_mount *mount, const char *path) {
    if (mount == NULL || path == NULL || !mount->used) {
        return false;
    }
    uint64_t i = 0;
    while (mount->mountpoint[i] != '\0') {
        if (path[i] != mount->mountpoint[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static bool metadata_directory_path(const struct exfat_mount *mount, char *out, uint64_t capacity) {
    if (mount == NULL || out == NULL || capacity == 0) {
        return false;
    }
    uint64_t offset = append_mount_path(out, capacity, mount->mountpoint);
    out[offset] = '\0';
    return append_path_piece(out, capacity, ".srvros");
}

static bool metadata_file_path(const struct exfat_mount *mount, char *out, uint64_t capacity) {
    if (!metadata_directory_path(mount, out, capacity)) {
        return false;
    }
    return append_path_piece(out, capacity, "/meta");
}

static bool metadata_temp_path(const struct exfat_mount *mount, char *out, uint64_t capacity) {
    if (!metadata_directory_path(mount, out, capacity)) {
        return false;
    }
    return append_path_piece(out, capacity, "/meta.tmp");
}

static bool metadata_path_is_internal(const struct exfat_mount *mount, const char *path) {
    char directory[EXFAT_MAX_PATH];
    if (!metadata_directory_path(mount, directory, sizeof(directory))) {
        return false;
    }
    uint64_t i = 0;
    while (directory[i] != '\0') {
        if (path[i] != directory[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static struct exfat_mount *find_mount_for_path(const char *path) {
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (path_under_mount(&mounts[i], path)) {
            return &mounts[i];
        }
    }
    return NULL;
}

static bool parse_u64_field(const uint8_t *data,
    uint64_t size,
    uint64_t *offset,
    uint64_t *value_out,
    bool final_field) {
    if (data == NULL || offset == NULL || value_out == NULL || *offset >= size) {
        return false;
    }
    uint64_t value = 0;
    uint64_t digits = 0;
    while (*offset < size && data[*offset] >= '0' && data[*offset] <= '9') {
        value = value * 10 + (uint64_t)(data[*offset] - '0');
        (*offset)++;
        digits++;
    }
    if (digits == 0) {
        return false;
    }
    if (final_field) {
        while (*offset < size && data[*offset] == '\r') {
            (*offset)++;
        }
        if (*offset < size && data[*offset] == '\n') {
            (*offset)++;
        } else if (*offset != size) {
            return false;
        }
    } else {
        if (*offset >= size || data[*offset] != '\t') {
            return false;
        }
        (*offset)++;
    }
    *value_out = value;
    return true;
}

static bool serialize_metadata_record(char *out,
    uint64_t capacity,
    uint64_t *offset,
    const struct vfs_node *node) {
    if (!append_text(out, capacity, offset, node->path) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.inode) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.mode) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.nlink) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.uid) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.gid) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.atime) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.mtime) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.ctime) ||
        !append_char(out, capacity, offset, '\n')) {
        return false;
    }
    return true;
}

static bool exfat_sync_mount_metadata(struct exfat_mount *mount) {
    if (mount == NULL || mount->volume.image != NULL || metadata_syncing) {
        return true;
    }

    metadata_syncing = true;

    char directory[EXFAT_MAX_PATH];
    char path[EXFAT_MAX_PATH];
    char temp_path[EXFAT_MAX_PATH];
    bool ok = metadata_directory_path(mount, directory, sizeof(directory)) &&
        metadata_file_path(mount, path, sizeof(path)) &&
        metadata_temp_path(mount, temp_path, sizeof(temp_path));
    if (ok && vfs_lookup(directory) == NULL) {
        ok = exfat_create_directory(directory);
    }

    uint64_t offset = 0;
    if (ok) {
        ok = append_text(metadata_buffer, sizeof(metadata_buffer), &offset, "SRVROSMETA1\n");
    }
    for (uint64_t i = 0; ok && i < vfs_count(); i++) {
        const struct vfs_node *node = vfs_node_at(i);
        if (node == NULL ||
            !path_under_mount(mount, node->path) ||
            metadata_path_is_internal(mount, node->path)) {
            continue;
        }
        ok = serialize_metadata_record(metadata_buffer, sizeof(metadata_buffer), &offset, node);
    }
    if (ok) {
        if (vfs_lookup(temp_path) != NULL) {
            ok = exfat_delete_file(temp_path);
        }
        if (ok) {
            ok = exfat_write_file(temp_path, (const uint8_t *)metadata_buffer, offset);
        }
        if (ok && vfs_lookup(path) != NULL) {
            ok = exfat_delete_file(path);
        }
        if (ok) {
            ok = exfat_rename(temp_path, path);
        }
    }

    metadata_syncing = false;
    return ok;
}

static void exfat_metadata_changed(const char *path, const struct vfs_metadata *metadata) {
    (void)metadata;
    if (metadata_syncing || path == NULL) {
        return;
    }

    struct exfat_mount *mount = find_mount_for_path(path);
    if (mount == NULL ||
        mount->volume.image != NULL ||
        metadata_path_is_internal(mount, path)) {
        return;
    }
    (void)exfat_sync_mount_metadata(mount);
}

static bool parse_metadata_file(struct exfat_mount *mount, const char *path, bool apply) {
    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data = NULL;
    uint64_t size = 0;
    const char *magic = "SRVROSMETA1\n";
    uint64_t magic_length = ascii_length(magic);
    if (node == NULL ||
        !vfs_read_all(node, &data, &size) ||
        size < magic_length ||
        !streqn((const char *)data, magic, magic_length)) {
        if (node != NULL && data != NULL) {
            vfs_release_data(node, data);
        }
        return false;
    }

    bool ok = true;
    uint64_t offset = magic_length;
    while (ok && offset < size) {
        while (offset < size && (data[offset] == '\n' || data[offset] == '\r')) {
            offset++;
        }
        if (offset >= size) {
            break;
        }

        char node_path[EXFAT_MAX_PATH];
        uint64_t path_length = 0;
        while (offset < size && data[offset] != '\t' && path_length + 1 < sizeof(node_path)) {
            node_path[path_length++] = (char)data[offset++];
        }
        node_path[path_length] = '\0';
        if (offset >= size || data[offset] != '\t') {
            ok = false;
            break;
        }
        offset++;

        struct vfs_metadata metadata;
        if (!parse_u64_field(data, size, &offset, &metadata.inode, false) ||
            !parse_u64_field(data, size, &offset, &metadata.mode, false) ||
            !parse_u64_field(data, size, &offset, &metadata.nlink, false) ||
            !parse_u64_field(data, size, &offset, &metadata.uid, false) ||
            !parse_u64_field(data, size, &offset, &metadata.gid, false) ||
            !parse_u64_field(data, size, &offset, &metadata.atime, false) ||
            !parse_u64_field(data, size, &offset, &metadata.mtime, false) ||
            !parse_u64_field(data, size, &offset, &metadata.ctime, true)) {
            ok = false;
            break;
        }

        if (!apply) {
            continue;
        }
        const struct vfs_node *target = vfs_lookup(node_path);
        if (target == NULL || metadata_path_is_internal(mount, node_path)) {
            continue;
        }
        uint64_t type = target->type == VFS_NODE_DIRECTORY ? VFS_MODE_IFDIR : VFS_MODE_IFREG;
        metadata.mode = (metadata.mode & ~VFS_MODE_IFMT) | type;
        (void)vfs_set_metadata(node_path, &metadata);
    }

    vfs_release_data(node, data);
    return ok;
}

static void load_mount_metadata(struct exfat_mount *mount) {
    char path[EXFAT_MAX_PATH];
    char temp_path[EXFAT_MAX_PATH];
    if (mount == NULL ||
        !metadata_file_path(mount, path, sizeof(path)) ||
        !metadata_temp_path(mount, temp_path, sizeof(temp_path))) {
        return;
    }

    metadata_syncing = true;
    if (parse_metadata_file(mount, temp_path, false)) {
        (void)parse_metadata_file(mount, temp_path, true);
        if (vfs_lookup(path) != NULL) {
            (void)exfat_delete_file(path);
        }
        (void)exfat_rename(temp_path, path);
    } else {
        (void)parse_metadata_file(mount, path, true);
        if (vfs_lookup(temp_path) != NULL) {
            (void)exfat_delete_file(temp_path);
        }
    }
    metadata_syncing = false;
}

static uint16_t entry_set_checksum(const uint8_t *entries, uint64_t entry_count) {
    uint16_t checksum = 0;
    uint64_t byte_count = entry_count * EXFAT_ENTRY_SIZE;
    for (uint64_t i = 0; i < byte_count; i++) {
        if (i == 2 || i == 3) {
            continue;
        }
        checksum = (uint16_t)(((checksum & 1) != 0 ? 0x8000 : 0) + (checksum >> 1) + entries[i]);
    }
    return checksum;
}

static bool write_file_data(struct exfat_file *file, const uint8_t *data, uint64_t size) {
    if (file != NULL && size == 0) {
        return true;
    }
    if (file == NULL || data == NULL || size > file->allocated_clusters * file->volume->cluster_size) {
        return false;
    }

    uint8_t *cluster_buffer = kcalloc(file->volume->cluster_size, 1);
    if (cluster_buffer == NULL) {
        return false;
    }

    uint64_t copied = 0;
    uint32_t cluster = file->first_cluster;
    uint64_t clusters = file->allocated_clusters;
    for (uint64_t i = 0; i < clusters; i++) {
        uint64_t offset;
        if (!cluster_offset(file->volume, cluster, &offset)) {
            kfree(cluster_buffer);
            return false;
        }

        for (uint64_t j = 0; j < file->volume->cluster_size; j++) {
            cluster_buffer[j] = 0;
        }

        uint64_t chunk = file->volume->cluster_size;
        if (chunk > size - copied) {
            chunk = size - copied;
        }
        for (uint64_t j = 0; j < chunk; j++) {
            cluster_buffer[j] = data[copied + j];
        }

        if (!volume_write(file->volume, offset, cluster_buffer, file->volume->cluster_size)) {
            kfree(cluster_buffer);
            return false;
        }

        copied += chunk;
        if (copied >= size) {
            break;
        }

        if (file->no_fat_chain) {
            cluster++;
        } else if (!next_cluster(file->volume, cluster, &cluster) || cluster >= EXFAT_FAT_CHAIN_END) {
            kfree(cluster_buffer);
            return false;
        }
    }

    kfree(cluster_buffer);
    return true;
}

static bool update_file_length(struct exfat_file *file, const char *path, uint64_t size) {
    uint8_t stream[EXFAT_ENTRY_SIZE];
    if (!volume_read(file->volume, file->stream_entry_offset, stream, sizeof(stream))) {
        return false;
    }
    stream[1] = file->no_fat_chain ? 0x02 : 0x00;
    write_u64(stream + 8, size);
    write_u32(stream + 20, file->first_cluster);
    write_u64(stream + 24, size);
    if (!volume_write(file->volume, file->stream_entry_offset, stream, sizeof(stream))) {
        return false;
    }

    file->size = size;
    (void)vfs_update_file_size(path, size);
    return true;
}

static bool overwrite_file(const char *path, const uint8_t *data, uint64_t size) {
    struct exfat_file *file = find_file(path);
    if (file == NULL || file->volume->image != NULL) {
        return false;
    }

    uint64_t cluster_count = clusters_for_size(file->volume, size);
    if (size == 0) {
        uint32_t old_first_cluster = file->first_cluster;
        uint64_t old_allocated_clusters = file->allocated_clusters;
        bool old_no_fat_chain = file->no_fat_chain;
        file->first_cluster = 0;
        file->allocated_clusters = 0;
        file->no_fat_chain = true;
        if (!update_file_length(file, path, 0)) {
            file->first_cluster = old_first_cluster;
            file->allocated_clusters = old_allocated_clusters;
            file->no_fat_chain = old_no_fat_chain;
            return false;
        }
        struct exfat_file old = {
            .volume = file->volume,
            .first_cluster = old_first_cluster,
            .allocated_clusters = old_allocated_clusters,
            .no_fat_chain = old_no_fat_chain,
        };
        return free_file_clusters(&old);
    }

    if (cluster_count == file->allocated_clusters) {
        if (!write_file_data(file, data, size)) {
            return false;
        }
        return update_file_length(file, path, size);
    }

    uint32_t old_first_cluster = file->first_cluster;
    uint64_t old_allocated_clusters = file->allocated_clusters;
    bool old_no_fat_chain = file->no_fat_chain;
    uint32_t first_cluster = 0;
    bool no_fat_chain = true;
    if (!allocate_clusters(file->volume, cluster_count, &first_cluster, &no_fat_chain)) {
        return false;
    }

    struct exfat_file scratch = {
        .volume = file->volume,
        .first_cluster = first_cluster,
        .size = size,
        .allocated_clusters = cluster_count,
        .no_fat_chain = no_fat_chain,
    };
    if (!write_file_data(&scratch, data, size)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    file->first_cluster = first_cluster;
    file->allocated_clusters = cluster_count;
    file->no_fat_chain = no_fat_chain;
    if (!update_file_length(file, path, size)) {
        (void)free_file_clusters(&scratch);
        file->first_cluster = old_first_cluster;
        file->allocated_clusters = old_allocated_clusters;
        file->no_fat_chain = old_no_fat_chain;
        return false;
    }

    struct exfat_file old = {
        .volume = file->volume,
        .first_cluster = old_first_cluster,
        .allocated_clusters = old_allocated_clusters,
        .no_fat_chain = old_no_fat_chain,
    };
    return free_file_clusters(&old);
}

static bool bitmap_bit_is_set(const uint8_t *bitmap, uint64_t index) {
    return (bitmap[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

static uint64_t count_used_bitmap_bits(const uint8_t *bitmap, uint64_t start_bit, uint64_t bit_count) {
    uint64_t used = 0;
    for (uint64_t i = 0; i < bit_count; i++) {
        if (bitmap_bit_is_set(bitmap, start_bit + i)) {
            used++;
        }
    }
    return used;
}

static void bitmap_set_bit(uint8_t *bitmap, uint64_t index) {
    bitmap[index / 8] |= (uint8_t)(1u << (index % 8));
}

static void bitmap_clear_bit(uint8_t *bitmap, uint64_t index) {
    bitmap[index / 8] &= (uint8_t)~(1u << (index % 8));
}

static uint64_t allocation_bitmap_bytes(const struct exfat_volume *volume) {
    if (volume == NULL) {
        return 0;
    }

    uint64_t bytes = (volume->cluster_count + 7) / 8;
    if (volume->allocation_bitmap_size != 0 && volume->allocation_bitmap_size < bytes) {
        bytes = volume->allocation_bitmap_size;
    }
    if (bytes > volume->cluster_size) {
        bytes = volume->cluster_size;
    }
    return bytes;
}

static bool allocate_contiguous_clusters(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster) {
    if (volume->allocation_bitmap_cluster < 2 || count == 0 || first_cluster == NULL) {
        return false;
    }

    uint8_t *bitmap = kmalloc(volume->cluster_size);
    if (bitmap == NULL) {
        return false;
    }

    uint64_t bitmap_offset;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) ||
        !volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        kfree(bitmap);
        return false;
    }

    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_clusters = bitmap_bytes * 8;
    if (count > bitmap_clusters) {
        kfree(bitmap);
        return false;
    }

    uint64_t run_start = 0;
    uint64_t run_length = 0;
    for (uint64_t index = 0; index < volume->cluster_count && index < bitmap_clusters; index++) {
        if (!bitmap_bit_is_set(bitmap, index)) {
            if (run_length == 0) {
                run_start = index;
            }
            run_length++;
            if (run_length == count) {
                for (uint64_t i = 0; i < count; i++) {
                    uint32_t cluster = (uint32_t)(run_start + i + 2);
                    bitmap_set_bit(bitmap, run_start + i);
                    uint8_t fat_entry[4];
                    write_u32(fat_entry, 0xffffffffu);
                    if (!volume_write(volume,
                            volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t),
                            fat_entry,
                            sizeof(fat_entry))) {
                        for (uint64_t j = 0; j < i; j++) {
                            uint32_t written_cluster = (uint32_t)(run_start + j + 2);
                            (void)volume_write(volume,
                                volume->fat_offset_bytes + (uint64_t)written_cluster * sizeof(uint32_t),
                                (uint8_t[4]) {0, 0, 0, 0},
                                sizeof(fat_entry));
                        }
                        kfree(bitmap);
                        return false;
                    }
                }
                if (!volume_write(volume, bitmap_offset, bitmap, volume->cluster_size)) {
                    for (uint64_t i = 0; i < count; i++) {
                        uint32_t cluster = (uint32_t)(run_start + i + 2);
                        (void)volume_write(volume,
                            volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t),
                            (uint8_t[4]) {0, 0, 0, 0},
                            sizeof(uint32_t));
                    }
                    kfree(bitmap);
                    return false;
                }
                *first_cluster = (uint32_t)(run_start + 2);
                kfree(bitmap);
                return true;
            }
        } else {
            run_length = 0;
        }
    }

    kfree(bitmap);
    return false;
}

static void clear_fat_entries(struct exfat_volume *volume, const uint32_t *clusters, uint64_t count) {
    uint8_t fat_entry[4];
    write_u32(fat_entry, 0);
    for (uint64_t i = 0; i < count; i++) {
        (void)volume_write(volume,
            volume->fat_offset_bytes + (uint64_t)clusters[i] * sizeof(uint32_t),
            fat_entry,
            sizeof(fat_entry));
    }
}

static bool allocate_fragmented_clusters(struct exfat_volume *volume,
    uint64_t count,
    uint32_t *first_cluster,
    bool *no_fat_chain) {
    if (volume->allocation_bitmap_cluster < 2 ||
        count == 0 ||
        first_cluster == NULL ||
        no_fat_chain == NULL ||
        count > UINT64_MAX / sizeof(uint32_t)) {
        return false;
    }

    uint64_t cluster_array_bytes = count * sizeof(uint32_t);
    uint32_t *clusters = kmalloc(cluster_array_bytes);
    uint8_t *bitmap = kmalloc(volume->cluster_size);
    if (clusters == NULL || bitmap == NULL) {
        if (clusters != NULL) {
            kfree(clusters);
        }
        if (bitmap != NULL) {
            kfree(bitmap);
        }
        return false;
    }

    uint64_t bitmap_offset;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) ||
        !volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        kfree(clusters);
        kfree(bitmap);
        return false;
    }

    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_clusters = bitmap_bytes * 8;
    uint64_t found = 0;
    for (uint64_t index = 0; index < volume->cluster_count && index < bitmap_clusters && found < count; index++) {
        if (!bitmap_bit_is_set(bitmap, index)) {
            clusters[found++] = (uint32_t)(index + 2);
            bitmap_set_bit(bitmap, index);
        }
    }
    if (found != count) {
        kfree(clusters);
        kfree(bitmap);
        return false;
    }

    for (uint64_t i = 0; i < count; i++) {
        uint32_t next = (i + 1 < count) ? clusters[i + 1] : 0xffffffffu;
        if (!write_fat_entry(volume, clusters[i], next)) {
            clear_fat_entries(volume, clusters, i);
            kfree(clusters);
            kfree(bitmap);
            return false;
        }
    }
    if (!volume_write(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        clear_fat_entries(volume, clusters, count);
        kfree(clusters);
        kfree(bitmap);
        return false;
    }

    *first_cluster = clusters[0];
    *no_fat_chain = false;
    kfree(clusters);
    kfree(bitmap);
    return true;
}

static bool allocate_clusters(struct exfat_volume *volume,
    uint64_t count,
    uint32_t *first_cluster,
    bool *no_fat_chain) {
    if (first_cluster == NULL || no_fat_chain == NULL) {
        return false;
    }
    if (count == 0) {
        *first_cluster = 0;
        *no_fat_chain = true;
        return true;
    }
    if (allocate_contiguous_clusters(volume, count, first_cluster)) {
        *no_fat_chain = true;
        return true;
    }
    return allocate_fragmented_clusters(volume, count, first_cluster, no_fat_chain);
}

static bool free_file_clusters(struct exfat_file *file) {
    if (file == NULL ||
        file->volume == NULL ||
        file->volume->allocation_bitmap_cluster < 2 ||
        file->first_cluster < 2 ||
        file->allocated_clusters == 0) {
        return true;
    }

    struct exfat_volume *volume = file->volume;
    uint8_t *bitmap = kmalloc(volume->cluster_size);
    if (bitmap == NULL) {
        return false;
    }

    uint64_t bitmap_offset;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) ||
        !volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        kfree(bitmap);
        return false;
    }

    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_clusters = bitmap_bytes * 8;
    uint32_t cluster = file->first_cluster;
    for (uint64_t i = 0; i < file->allocated_clusters; i++) {
        if (cluster < 2 || cluster >= volume->cluster_count + 2) {
            kfree(bitmap);
            return false;
        }

        uint32_t next = cluster + 1;
        if (!file->no_fat_chain && i + 1 < file->allocated_clusters) {
            if (!next_cluster(volume, cluster, &next)) {
                kfree(bitmap);
                return false;
            }
        }

        uint64_t bit_index = cluster - 2;
        if (bit_index < bitmap_clusters) {
            bitmap_clear_bit(bitmap, bit_index);
        }

        uint8_t fat_entry[4];
        write_u32(fat_entry, 0);
        if (!volume_write(volume,
                volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t),
                fat_entry,
                sizeof(fat_entry))) {
            kfree(bitmap);
            return false;
        }

        if (file->no_fat_chain) {
            cluster++;
        } else {
            cluster = next;
        }
    }

    bool ok = volume_write(volume, bitmap_offset, bitmap, volume->cluster_size);
    kfree(bitmap);
    return ok;
}

static bool root_entry_is_free(uint8_t entry_type) {
    return entry_type == EXFAT_ENTRY_END || (entry_type & 0x80u) == 0;
}

static void release_created_clusters(struct exfat_volume *volume, uint32_t first_cluster, uint64_t cluster_count);

static bool scan_directory_cluster_for_free(const uint8_t *cluster_buffer,
    uint64_t cluster_size,
    uint64_t needed_entries,
    uint64_t *offset_out) {
    uint64_t needed_bytes = needed_entries * EXFAT_ENTRY_SIZE;
    uint64_t run_start = 0;
    uint64_t run_length = 0;
    for (uint64_t offset = 0; offset + needed_bytes <= cluster_size; offset += EXFAT_ENTRY_SIZE) {
        if (root_entry_is_free(cluster_buffer[offset])) {
            if (run_length == 0) {
                run_start = offset;
            }
            if (cluster_buffer[offset] == EXFAT_ENTRY_END) {
                *offset_out = run_start;
                return true;
            }
            run_length++;
            if (run_length == needed_entries) {
                *offset_out = run_start;
                return true;
            }
        } else {
            run_length = 0;
        }
    }

    if (cluster_size >= needed_bytes &&
        run_length > 0 &&
        run_start + needed_bytes <= cluster_size) {
        *offset_out = run_start;
        return true;
    }

    return false;
}

static bool directory_free_offset(struct exfat_volume *volume,
    struct exfat_directory *directory,
    uint64_t needed_entries,
    uint64_t *offset_out) {
    if (volume == NULL || directory == NULL || needed_entries == 0 || offset_out == NULL) {
        return false;
    }

    uint8_t *cluster_buffer = kmalloc(volume->cluster_size);
    if (cluster_buffer == NULL) {
        return false;
    }

    uint32_t cluster = directory->first_cluster;
    uint32_t previous = 0;
    uint64_t visited = 0;
    while (cluster >= 2 && cluster < EXFAT_FAT_CHAIN_END && visited++ < volume->cluster_count) {
        uint64_t cluster_base;
        if (!cluster_offset(volume, cluster, &cluster_base) ||
            !volume_read(volume, cluster_base, cluster_buffer, volume->cluster_size)) {
            kfree(cluster_buffer);
            return false;
        }

        uint64_t free_offset;
        if (scan_directory_cluster_for_free(cluster_buffer,
                volume->cluster_size,
                needed_entries,
                &free_offset)) {
            *offset_out = cluster_base + free_offset;
            kfree(cluster_buffer);
            return true;
        }

        previous = cluster;
        if (directory->no_fat_chain) {
            if (visited >= directory->allocated_clusters) {
                cluster = EXFAT_FAT_CHAIN_END;
            } else {
                cluster++;
            }
        } else if (!next_cluster(volume, cluster, &cluster)) {
            kfree(cluster_buffer);
            return false;
        }
    }

    if (directory->no_fat_chain ||
        previous < 2 ||
        needed_entries * EXFAT_ENTRY_SIZE > volume->cluster_size) {
        kfree(cluster_buffer);
        return false;
    }

    uint32_t new_cluster = 0;
    if (!allocate_contiguous_clusters(volume, 1, &new_cluster)) {
        kfree(cluster_buffer);
        return false;
    }
    for (uint64_t i = 0; i < volume->cluster_size; i++) {
        cluster_buffer[i] = 0;
    }
    uint64_t new_cluster_base;
    if (!cluster_offset(volume, new_cluster, &new_cluster_base) ||
        !volume_write(volume, new_cluster_base, cluster_buffer, volume->cluster_size) ||
        !write_fat_entry(volume, previous, new_cluster)) {
        release_created_clusters(volume, new_cluster, 1);
        kfree(cluster_buffer);
        return false;
    }

    *offset_out = new_cluster_base;
    kfree(cluster_buffer);
    return true;
}

static void release_created_clusters(struct exfat_volume *volume, uint32_t first_cluster, uint64_t cluster_count) {
    struct exfat_file scratch = {
        .volume = volume,
        .first_cluster = first_cluster,
        .allocated_clusters = cluster_count,
        .no_fat_chain = true,
    };
    (void)free_file_clusters(&scratch);
}

static bool register_created_file(struct exfat_mount *mount,
    const char *parent,
    uint64_t directory_offset,
    const uint8_t *entries,
    uint64_t name_entry_count) {
    const struct exfat_file_entry *file_entry = (const struct exfat_file_entry *)entries;
    const struct exfat_stream_entry *stream = (const struct exfat_stream_entry *)(entries + EXFAT_ENTRY_SIZE);
    const struct exfat_name_entry *names = (const struct exfat_name_entry *)(entries + EXFAT_ENTRY_SIZE * 2);
    return register_file(mount,
        parent,
        file_entry,
        stream,
        names,
        name_entry_count,
        directory_offset,
        directory_offset + EXFAT_ENTRY_SIZE);
}

static bool read_entry_run(struct exfat_volume *volume, uint64_t offset, uint64_t entry_count, uint8_t *out) {
    if (volume == NULL || out == NULL || entry_count == 0 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return false;
    }
    return volume_read(volume, offset, out, entry_count * EXFAT_ENTRY_SIZE);
}

static bool restore_entry_run(struct exfat_volume *volume, uint64_t offset, const uint8_t *entries, uint64_t entry_count) {
    if (volume == NULL || entries == NULL || entry_count == 0 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return false;
    }
    return volume_write(volume, offset, entries, entry_count * EXFAT_ENTRY_SIZE);
}

static bool write_deleted_entries(struct exfat_volume *volume, uint64_t file_entry_offset, uint8_t secondary_count) {
    uint64_t entry_count = (uint64_t)secondary_count + 1;
    if (entry_count < 3 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return false;
    }

    uint8_t deleted[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t byte_count = entry_count * EXFAT_ENTRY_SIZE;
    for (uint64_t i = 0; i < byte_count; i++) {
        deleted[i] = 0;
    }
    deleted[0] = EXFAT_ENTRY_FILE & 0x7f;
    deleted[EXFAT_ENTRY_SIZE] = EXFAT_ENTRY_STREAM & 0x7f;
    for (uint64_t i = 2; i < entry_count; i++) {
        deleted[EXFAT_ENTRY_SIZE * i] = EXFAT_ENTRY_NAME & 0x7f;
    }

    return volume_write(volume, file_entry_offset, deleted, byte_count);
}

static bool write_inactive_entry_run(struct exfat_volume *volume, uint64_t offset, uint64_t entry_count) {
    if (entry_count == 0 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return true;
    }
    uint8_t deleted[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t byte_count = entry_count * EXFAT_ENTRY_SIZE;
    for (uint64_t i = 0; i < byte_count; i++) {
        deleted[i] = 0;
    }
    for (uint64_t i = 0; i < entry_count; i++) {
        deleted[i * EXFAT_ENTRY_SIZE] = 0x01;
    }
    return volume_write(volume, offset, deleted, byte_count);
}

static bool write_deleted_entry_set(struct exfat_file *file) {
    return write_deleted_entries(file->volume, file->file_entry_offset, file->secondary_count);
}

static void clear_file_slot(struct exfat_mount *mount, uint64_t index) {
    mount->paths[index][0] = '\0';
    uint8_t *file_bytes = (uint8_t *)&mount->files[index];
    for (uint64_t i = 0; i < sizeof(mount->files[index]); i++) {
        file_bytes[i] = 0;
    }

    while (mount->file_count > 0 && mount->paths[mount->file_count - 1][0] == '\0') {
        mount->file_count--;
    }
}

static void clear_directory_slot(struct exfat_mount *mount, uint64_t index) {
    uint8_t *dir_bytes = (uint8_t *)&mount->dirs[index];
    for (uint64_t i = 0; i < sizeof(mount->dirs[index]); i++) {
        dir_bytes[i] = 0;
    }

    while (mount->dir_count > 0 && mount->dirs[mount->dir_count - 1].path[0] == '\0') {
        mount->dir_count--;
    }
}

static bool path_is_child_of(const char *path, const char *parent) {
    if (path == NULL || parent == NULL || parent[0] == '\0') {
        return false;
    }
    uint64_t i = 0;
    while (parent[i] != '\0') {
        if (path[i] != parent[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '/';
}

static bool directory_is_empty(const struct exfat_mount *mount, const char *path) {
    for (uint64_t i = 0; i < mount->file_count; i++) {
        if (mount->paths[i][0] != '\0' && path_is_child_of(mount->paths[i], path)) {
            return false;
        }
    }
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        if (mount->dirs[i].path[0] != '\0' && path_is_child_of(mount->dirs[i].path, path)) {
            return false;
        }
    }
    return true;
}

static bool build_file_entry_set(uint8_t *entries,
    const char *name,
    uint32_t first_cluster,
    uint64_t data_length,
    uint16_t attributes,
    bool no_fat_chain,
    uint64_t *entry_count_out,
    uint64_t *name_entry_count_out) {
    uint64_t name_length = ascii_length(name);
    uint64_t name_entry_count = (name_length + 14) / 15;
    uint64_t entry_count = 2 + name_entry_count;
    if (name_length == 0 ||
        name_length > 255 ||
        entry_count > EXFAT_MAX_FILE_ENTRIES ||
        entry_count_out == NULL ||
        name_entry_count_out == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < EXFAT_ENTRY_SIZE * entry_count; i++) {
        entries[i] = 0;
    }

    uint8_t *primary = entries;
    uint8_t *stream = entries + EXFAT_ENTRY_SIZE;

    primary[0] = EXFAT_ENTRY_FILE;
    primary[1] = (uint8_t)(entry_count - 1);
    write_u16(primary + 4, attributes);

    stream[0] = EXFAT_ENTRY_STREAM;
    stream[1] = no_fat_chain ? 0x02 : 0x00;
    stream[3] = (uint8_t)name_length;
    write_u64(stream + 8, data_length);
    write_u32(stream + 20, first_cluster);
    write_u64(stream + 24, data_length);

    for (uint64_t i = 0; i < name_length; i++) {
        uint64_t entry_index = i / 15;
        uint64_t slot = i % 15;
        uint8_t *name_entry = entries + EXFAT_ENTRY_SIZE * (2 + entry_index);
        name_entry[0] = EXFAT_ENTRY_NAME;
        write_u16(name_entry + 2 + slot * 2, (uint16_t)name[i]);
    }

    write_u16(primary + 2, entry_set_checksum(entries, entry_count));
    *entry_count_out = entry_count;
    *name_entry_count_out = name_entry_count;
    return true;
}

static bool create_file(const char *path, const uint8_t *data, uint64_t size) {
    const char *name = NULL;
    struct exfat_directory *parent = NULL;
    struct exfat_mount *mount = find_mount_and_parent_directory(path, &parent, &name);
    if (mount == NULL ||
        parent == NULL ||
        name == NULL ||
        !mount_has_file_slot(mount) ||
        mount->volume.image != NULL ||
        vfs_lookup(path) != NULL) {
        return false;
    }

    uint64_t cluster_count = clusters_for_size(&mount->volume, size);
    uint32_t first_cluster = 0;
    bool no_fat_chain = true;
    if (!allocate_clusters(&mount->volume, cluster_count, &first_cluster, &no_fat_chain)) {
        return false;
    }

    struct exfat_file scratch = {
        .volume = &mount->volume,
        .first_cluster = first_cluster,
        .size = size,
        .allocated_clusters = cluster_count,
        .no_fat_chain = no_fat_chain,
    };
    if (!write_file_data(&scratch, data, size)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries, name, first_cluster, size, 0x20, no_fat_chain, &entry_count, &name_entry_count)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    uint64_t directory_offset;
    if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, directory_offset, entry_count, old_entries)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    if (!volume_write(&mount->volume,
            directory_offset,
            entries,
            entry_count * EXFAT_ENTRY_SIZE)) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        (void)free_file_clusters(&scratch);
        return false;
    }

    if (!register_created_file(mount, parent->path, directory_offset, entries, name_entry_count)) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        (void)free_file_clusters(&scratch);
        return false;
    }
    if (!metadata_path_is_internal(mount, path)) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

bool exfat_create_directory(const char *path) {
    const char *name = NULL;
    struct exfat_directory *parent = NULL;
    struct exfat_mount *mount = find_mount_and_parent_directory(path, &parent, &name);
    if (mount == NULL ||
        parent == NULL ||
        name == NULL ||
        mount->dir_count >= EXFAT_MAX_DIRS ||
        mount->volume.image != NULL ||
        vfs_lookup(path) != NULL) {
        return false;
    }

    uint32_t first_cluster = 0;
    if (!allocate_contiguous_clusters(&mount->volume, 1, &first_cluster)) {
        return false;
    }

    uint8_t *empty_directory = kcalloc(mount->volume.cluster_size, 1);
    if (empty_directory == NULL) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    struct exfat_file scratch = {
        .volume = &mount->volume,
        .first_cluster = first_cluster,
        .size = mount->volume.cluster_size,
        .allocated_clusters = 1,
        .no_fat_chain = true,
    };
    if (!write_file_data(&scratch, empty_directory, mount->volume.cluster_size)) {
        kfree(empty_directory);
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }
    kfree(empty_directory);

    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries,
            name,
            first_cluster,
            mount->volume.cluster_size,
            EXFAT_ATTR_DIRECTORY,
            true,
            &entry_count,
            &name_entry_count)) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }
    (void)name_entry_count;

    uint64_t directory_offset;
    if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, directory_offset, entry_count, old_entries)) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    if (!volume_write(&mount->volume,
            directory_offset,
            entries,
            entry_count * EXFAT_ENTRY_SIZE)) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    bool ok = register_directory(mount,
        path,
        first_cluster,
        true,
        mount->volume.cluster_size,
        directory_offset,
        (uint8_t)(entry_count - 1));
    if (!ok) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }
    if (ok && !metadata_path_is_internal(mount, path)) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

static void clear_mount(struct exfat_mount *mount) {
    uint8_t *bytes = (uint8_t *)mount;
    for (uint64_t i = 0; i < sizeof(*mount); i++) {
        bytes[i] = 0;
    }
}

static struct exfat_mount *alloc_mount(void) {
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mounts[i].used) {
            clear_mount(&mounts[i]);
            mounts[i].used = true;
            return &mounts[i];
        }
    }
    return NULL;
}

void exfat_print_mounts(void) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mounts[i].used) {
            continue;
        }
        count++;
        const char *device_name = "image";
        bool writable = false;
        if (mounts[i].volume.device != NULL) {
            device_name = mounts[i].volume.device->name;
            writable = mounts[i].volume.device->write != NULL;
        }
        console_printf("exfat: %s device=%s files=%u cluster=%u write=%s\n",
            mounts[i].mountpoint,
            device_name,
            mounts[i].file_count,
            mounts[i].volume.cluster_size,
            writable ? "yes" : "no");
    }
    if (count == 0) {
        console_write("exfat: no mounts\n");
    }
}

static void check_error(struct exfat_check_result *result, const char *mountpoint, const char *message) {
    if (result != NULL) {
        result->errors++;
    }
    console_printf("exfat-check: %s: %s\n", mountpoint != NULL ? mountpoint : "?", message);
}

static void check_file_error(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const char *path,
    const char *message) {
    if (result != NULL) {
        result->errors++;
    }
    console_printf("exfat-check: %s: %s: %s\n",
        mount != NULL ? mount->mountpoint : "?",
        path != NULL && path[0] != '\0' ? path : "?",
        message);
}

static bool mark_checked_cluster(uint8_t *seen, uint64_t seen_bytes, uint64_t bit_index) {
    if (bit_index / 8 >= seen_bytes) {
        return false;
    }
    uint8_t mask = (uint8_t)(1u << (bit_index % 8));
    bool already_seen = (seen[bit_index / 8] & mask) != 0;
    seen[bit_index / 8] |= mask;
    return !already_seen;
}

static bool checked_cluster_seen(const uint8_t *seen, uint64_t seen_bytes, uint64_t bit_index) {
    if (bit_index / 8 >= seen_bytes) {
        return false;
    }
    return (seen[bit_index / 8] & (uint8_t)(1u << (bit_index % 8))) != 0;
}

static void check_cluster_allocation(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const char *path,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    uint32_t cluster,
    bool allow_duplicate) {
    uint64_t bit_index = cluster - 2;
    if (bit_index >= bitmap_bits || !bitmap_bit_is_set(bitmap, bit_index)) {
        check_file_error(result, mount, path, "cluster not marked allocated in bitmap");
    }
    if (!mark_checked_cluster(seen, seen_bytes, bit_index) && !allow_duplicate) {
        check_file_error(result, mount, path, "duplicate cluster allocation");
    }
    result->clusters_checked++;
}

static bool file_has_existing_alias(const struct exfat_mount *mount, uint64_t file_index) {
    const struct exfat_file *file = &mount->files[file_index];
    for (uint64_t i = 0; i < file_index; i++) {
        const struct exfat_file *other = &mount->files[i];
        if (mount->paths[i][0] != '\0' &&
            other->first_cluster == file->first_cluster &&
            other->size == file->size &&
            other->allocated_clusters == file->allocated_clusters) {
            return true;
        }
    }
    return false;
}

static void check_file_clusters(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    uint64_t file_index) {
    const struct exfat_file *file = &mount->files[file_index];
    const char *path = mount->paths[file_index];
    uint64_t expected_clusters = clusters_for_size(&mount->volume, file->size);
    uint32_t cluster = file->first_cluster;
    bool alias = file_has_existing_alias(mount, file_index);

    if (path[0] == '\0') {
        return;
    }
    result->files_checked++;

    if (file->volume != &mount->volume) {
        check_file_error(result, mount, path, "volume pointer mismatch");
        return;
    }
    if (file->allocated_clusters != expected_clusters) {
        check_file_error(result, mount, path, "allocated cluster count mismatch");
    }
    if (expected_clusters == 0) {
        return;
    }
    if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
        check_file_error(result, mount, path, "first cluster out of range");
        return;
    }

    for (uint64_t i = 0; i < expected_clusters; i++) {
        if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
            check_file_error(result, mount, path, "cluster chain left volume");
            return;
        }

        check_cluster_allocation(result, mount, path, bitmap, bitmap_bits, seen, seen_bytes, cluster, alias);

        if (i + 1 >= expected_clusters) {
            if (!file->no_fat_chain) {
                uint32_t next = 0;
                if (!next_cluster(file->volume, cluster, &next)) {
                    check_file_error(result, mount, path, "could not read FAT chain");
                } else if (next < EXFAT_FAT_CHAIN_END) {
                    check_file_error(result, mount, path, "FAT chain longer than file allocation");
                }
            }
            break;
        }
        if (file->no_fat_chain) {
            cluster++;
        } else if (!next_cluster(file->volume, cluster, &cluster)) {
            check_file_error(result, mount, path, "could not read FAT chain");
            return;
        }
    }
}

static void check_directory_clusters(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    uint64_t directory_index) {
    const struct exfat_directory *directory = &mount->dirs[directory_index];
    const char *path = directory->path;
    uint32_t cluster = directory->first_cluster;
    bool follow_to_end = directory->file_entry_offset == 0 &&
        directory->data_length == 0 &&
        !directory->no_fat_chain;
    uint64_t expected_clusters = follow_to_end ? mount->volume.cluster_count : directory->allocated_clusters;

    if (path[0] == '\0' || expected_clusters == 0) {
        return;
    }
    if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
        check_file_error(result, mount, path, "directory first cluster out of range");
        return;
    }

    for (uint64_t i = 0; i < expected_clusters; i++) {
        if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
            check_file_error(result, mount, path, "directory chain left volume");
            return;
        }

        check_cluster_allocation(result, mount, path, bitmap, bitmap_bits, seen, seen_bytes, cluster, false);

        if (directory->no_fat_chain) {
            if (i + 1 >= expected_clusters) {
                break;
            }
            cluster++;
            continue;
        }

        uint32_t next = 0;
        if (!next_cluster(&mount->volume, cluster, &next)) {
            check_file_error(result, mount, path, "could not read directory FAT chain");
            return;
        }
        if (follow_to_end) {
            if (next >= EXFAT_FAT_CHAIN_END) {
                break;
            }
            cluster = next;
            continue;
        }
        if (i + 1 >= expected_clusters) {
            if (next < EXFAT_FAT_CHAIN_END) {
                check_file_error(result, mount, path, "directory FAT chain longer than allocation");
            }
            break;
        }
        cluster = next;
    }
}

static void check_system_cluster_range(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    const char *label,
    uint32_t first_cluster,
    uint64_t byte_length) {
    if (first_cluster < 2 || byte_length == 0) {
        return;
    }
    uint64_t clusters = clusters_for_size(&mount->volume, byte_length);
    uint32_t cluster = first_cluster;
    for (uint64_t i = 0; i < clusters; i++) {
        if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
            check_file_error(result, mount, label, "system cluster out of range");
            return;
        }
        check_cluster_allocation(result, mount, label, bitmap, bitmap_bits, seen, seen_bytes, cluster, false);
        cluster++;
    }
}

static void check_bitmap_leaks(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    const uint8_t *seen,
    uint64_t seen_bytes) {
    uint64_t leak_count = 0;
    uint64_t first_leak = 0;
    uint64_t limit = mount->volume.cluster_count < bitmap_bits ? mount->volume.cluster_count : bitmap_bits;
    for (uint64_t bit = 0; bit < limit; bit++) {
        if (bitmap_bit_is_set(bitmap, bit) && !checked_cluster_seen(seen, seen_bytes, bit)) {
            if (leak_count == 0) {
                first_leak = bit + 2;
            }
            leak_count++;
        }
    }
    if (leak_count != 0) {
        result->leaked_clusters += leak_count;
        result->errors++;
        console_printf("exfat-check: %s: leaked allocated clusters=%u first=%u\n",
            mount != NULL ? mount->mountpoint : "?",
            leak_count,
            first_leak);
    }
}

static void check_fat_bitmap_agreement(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits) {
    uint64_t mismatch_count = 0;
    uint64_t first_cluster = 0;
    uint32_t first_value = 0;
    uint64_t limit = mount->volume.cluster_count < bitmap_bits ? mount->volume.cluster_count : bitmap_bits;
    for (uint64_t bit = 0; bit < limit; bit++) {
        if (bitmap_bit_is_set(bitmap, bit)) {
            continue;
        }
        uint32_t cluster = (uint32_t)(bit + 2);
        uint32_t value = 0;
        if (!next_cluster(&mount->volume, cluster, &value)) {
            check_error(result, mount->mountpoint, "could not read FAT for bitmap agreement");
            return;
        }
        if (value != 0) {
            if (mismatch_count == 0) {
                first_cluster = cluster;
                first_value = value;
            }
            mismatch_count++;
        }
    }
    if (mismatch_count != 0) {
        result->fat_bitmap_mismatches += mismatch_count;
        result->errors++;
        console_printf("exfat-check: %s: free clusters with FAT entries=%u first=%u value=%x\n",
            mount != NULL ? mount->mountpoint : "?",
            mismatch_count,
            first_cluster,
            first_value);
    }
}

static bool check_mount(struct exfat_mount *mount, struct exfat_check_result *result) {
    struct exfat_volume *volume = &mount->volume;
    uint64_t bitmap_offset = 0;
    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_bits = bitmap_bytes * 8;
    uint64_t seen_bytes = (volume->cluster_count + 7) / 8;
    uint8_t *bitmap = NULL;
    uint8_t *seen = NULL;
    bool ok = true;

    result->mounts_checked++;
    if (volume->cluster_size == 0 || volume->cluster_count == 0) {
        check_error(result, mount->mountpoint, "invalid cluster geometry");
        return false;
    }
    if (volume->allocation_bitmap_cluster < 2) {
        check_error(result, mount->mountpoint, "missing allocation bitmap");
        return false;
    }
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) || bitmap_bytes == 0) {
        check_error(result, mount->mountpoint, "allocation bitmap out of range");
        return false;
    }

    bitmap = kmalloc(volume->cluster_size);
    seen = kcalloc(seen_bytes == 0 ? 1 : seen_bytes, 1);
    if (bitmap == NULL || seen == NULL) {
        check_error(result, mount->mountpoint, "out of memory");
        ok = false;
        goto out;
    }
    if (!volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        check_error(result, mount->mountpoint, "could not read allocation bitmap");
        ok = false;
        goto out;
    }

    check_system_cluster_range(result,
        mount,
        bitmap,
        bitmap_bits,
        seen,
        seen_bytes,
        "/allocation-bitmap",
        volume->allocation_bitmap_cluster,
        bitmap_bytes);
    check_system_cluster_range(result,
        mount,
        bitmap,
        bitmap_bits,
        seen,
        seen_bytes,
        "/upcase-table",
        volume->upcase_table_cluster,
        volume->upcase_table_size);
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        check_directory_clusters(result, mount, bitmap, bitmap_bits, seen, seen_bytes, i);
    }
    for (uint64_t i = 0; i < mount->file_count; i++) {
        if (mount->paths[i][0] != '\0') {
            check_file_clusters(result, mount, bitmap, bitmap_bits, seen, seen_bytes, i);
        }
    }
    check_bitmap_leaks(result, mount, bitmap, bitmap_bits, seen, seen_bytes);
    check_fat_bitmap_agreement(result, mount, bitmap, bitmap_bits);

    ok = result->errors == 0;

out:
    if (bitmap != NULL) {
        kfree(bitmap);
    }
    if (seen != NULL) {
        kfree(seen);
    }
    return ok;
}

bool exfat_check(const char *mountpoint, struct exfat_check_result *result) {
    struct exfat_check_result local = {0};
    struct exfat_check_result *out = result != NULL ? result : &local;
    uint8_t *bytes = (uint8_t *)out;
    bool any = false;

    for (uint64_t i = 0; i < sizeof(*out); i++) {
        bytes[i] = 0;
    }

    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mount_matches(&mounts[i], mountpoint)) {
            continue;
        }
        any = true;
        (void)check_mount(&mounts[i], out);
    }

    if (!any) {
        out->errors++;
        console_printf("exfat-check: no mounted exFAT volume%s%s\n",
            mountpoint != NULL && mountpoint[0] != '\0' ? ": " : "",
            mountpoint != NULL && mountpoint[0] != '\0' ? mountpoint : "");
        return false;
    }
    return out->errors == 0;
}

void exfat_check_print(const char *mountpoint) {
    struct exfat_check_result result;
    bool ok = exfat_check(mountpoint, &result);
    console_printf("exfat-check: mounts=%u files=%u clusters=%u leaks=%u fat=%u errors=%u %s\n",
        result.mounts_checked,
        result.files_checked,
        result.clusters_checked,
        result.leaked_clusters,
        result.fat_bitmap_mismatches,
        result.errors,
        ok ? "ok" : "failed");
}

bool exfat_unmount(const char *mountpoint) {
    if (!valid_mountpoint(mountpoint)) {
        return false;
    }

    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        struct exfat_mount *mount = &mounts[i];
        if (!mount->used || !streq(mount->mountpoint, mountpoint)) {
            continue;
        }

        uint64_t removed = vfs_unregister_prefix(mountpoint);
        console_printf("exfat: unmounted %s removed=%u\n", mountpoint, removed);
        clear_mount(mount);
        return true;
    }

    return false;
}

bool exfat_write_file(const char *path, const uint8_t *data, uint64_t size) {
    if (path == NULL || (data == NULL && size != 0)) {
        return false;
    }

    if (find_file(path) != NULL) {
        return overwrite_file(path, data, size);
    }
    return create_file(path, data, size);
}

bool exfat_delete_file(const char *path) {
    uint64_t index = 0;
    struct exfat_mount *mount = find_mount_and_index_for_file(path, &index);
    if (mount == NULL || mount->volume.image != NULL) {
        return false;
    }
    bool internal_path = metadata_path_is_internal(mount, path);

    struct exfat_file *file = &mount->files[index];
    if (!write_deleted_entry_set(file)) {
        return false;
    }

    (void)free_file_clusters(file);
    if (!vfs_unregister_path(path)) {
        return false;
    }

    clear_file_slot(mount, index);
    if (!internal_path) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

bool exfat_delete_directory(const char *path) {
    uint64_t index = 0;
    struct exfat_mount *mount = find_mount_and_index_for_directory(path, &index);
    if (mount == NULL ||
        mount->volume.image != NULL ||
        streq(path, mount->mountpoint) ||
        !directory_is_empty(mount, path)) {
        return false;
    }

    struct exfat_directory *directory = &mount->dirs[index];
    bool internal_path = metadata_path_is_internal(mount, path);
    if (!write_deleted_entries(&mount->volume,
            directory->file_entry_offset,
            directory->secondary_count)) {
        return false;
    }

    struct exfat_file scratch = {
        .volume = &mount->volume,
        .first_cluster = directory->first_cluster,
        .allocated_clusters = directory->allocated_clusters,
        .no_fat_chain = directory->no_fat_chain,
    };
    (void)free_file_clusters(&scratch);
    if (!vfs_unregister_path(path)) {
        return false;
    }
    clear_directory_slot(mount, index);
    if (!internal_path) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

static bool rename_file_in_mount(struct exfat_mount *mount,
    uint64_t index,
    const char *old_path,
    const char *new_path) {
    struct exfat_directory *parent = NULL;
    const char *name = NULL;
    struct vfs_metadata metadata;
    bool have_metadata = vfs_stat(old_path, &metadata, NULL, NULL);
    struct exfat_mount *new_mount = find_mount_and_parent_directory(new_path, &parent, &name);
    if (new_mount != mount || parent == NULL || name == NULL || vfs_lookup(new_path) != NULL) {
        return false;
    }

    struct exfat_file *file = &mount->files[index];
    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries,
            name,
            file->first_cluster,
            file->size,
            0x20,
            file->no_fat_chain,
            &entry_count,
            &name_entry_count)) {
        return false;
    }

    uint64_t old_entry_count = (uint64_t)file->secondary_count + 1;
    uint64_t old_directory_offset = file->file_entry_offset;
    uint64_t directory_offset = file->file_entry_offset;
    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint8_t displaced_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, old_directory_offset, old_entry_count, old_entries)) {
        return false;
    }
    if (entry_count > old_entry_count) {
        if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
            return false;
        }
        if (!read_entry_run(&mount->volume, directory_offset, entry_count, displaced_entries)) {
            return false;
        }
    } else if (!read_entry_run(&mount->volume, directory_offset, old_entry_count, displaced_entries)) {
        return false;
    }

    if (!volume_write(&mount->volume, directory_offset, entries, entry_count * EXFAT_ENTRY_SIZE)) {
        if (entry_count > old_entry_count) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    if (entry_count < old_entry_count) {
        uint64_t extra_offset = directory_offset + entry_count * EXFAT_ENTRY_SIZE;
        if (!write_inactive_entry_run(&mount->volume,
                extra_offset,
                old_entry_count - entry_count)) {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
            return false;
        }
    } else if (entry_count > old_entry_count) {
        if (!write_deleted_entry_set(file)) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            return false;
        }
    }

    if (!vfs_unregister_path(old_path)) {
        if (entry_count > old_entry_count) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    char old_path_copy[EXFAT_MAX_PATH];
    copy_cstr(old_path_copy, sizeof(old_path_copy), mount->paths[index]);
    uint64_t old_file_entry_offset = file->file_entry_offset;
    uint64_t old_stream_entry_offset = file->stream_entry_offset;
    uint8_t old_secondary_count = file->secondary_count;
    file->file_entry_offset = directory_offset;
    file->stream_entry_offset = directory_offset + EXFAT_ENTRY_SIZE;
    file->secondary_count = (uint8_t)(entry_count - 1);
    copy_cstr(mount->paths[index], sizeof(mount->paths[index]), new_path);
    bool ok = vfs_register_file_with_metadata(mount->paths[index],
        file->size,
        file,
        exfat_read_all,
        exfat_release_data,
        have_metadata ? &metadata : NULL);
    if (ok) {
        (void)exfat_sync_mount_metadata(mount);
        return true;
    }

    if (entry_count > old_entry_count) {
        (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    } else {
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    }
    file->file_entry_offset = old_file_entry_offset;
    file->stream_entry_offset = old_stream_entry_offset;
    file->secondary_count = old_secondary_count;
    copy_cstr(mount->paths[index], sizeof(mount->paths[index]), old_path_copy);
    (void)vfs_register_file_with_metadata(mount->paths[index],
        file->size,
        file,
        exfat_read_all,
        exfat_release_data,
        have_metadata ? &metadata : NULL);
    return false;
}

static bool rename_empty_directory_in_mount(struct exfat_mount *mount,
    uint64_t index,
    const char *old_path,
    const char *new_path) {
    if (!directory_is_empty(mount, old_path)) {
        return false;
    }

    struct exfat_directory *parent = NULL;
    const char *name = NULL;
    struct vfs_metadata metadata;
    bool have_metadata = vfs_stat(old_path, &metadata, NULL, NULL);
    struct exfat_mount *new_mount = find_mount_and_parent_directory(new_path, &parent, &name);
    if (new_mount != mount || parent == NULL || name == NULL || vfs_lookup(new_path) != NULL) {
        return false;
    }

    struct exfat_directory *directory = &mount->dirs[index];
    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries,
            name,
            directory->first_cluster,
            directory->data_length,
            EXFAT_ATTR_DIRECTORY,
            directory->no_fat_chain,
            &entry_count,
            &name_entry_count)) {
        return false;
    }
    (void)name_entry_count;

    uint64_t old_entry_count = (uint64_t)directory->secondary_count + 1;
    uint64_t old_directory_offset = directory->file_entry_offset;
    uint64_t directory_offset = directory->file_entry_offset;
    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint8_t displaced_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, old_directory_offset, old_entry_count, old_entries)) {
        return false;
    }
    if (entry_count > old_entry_count) {
        if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
            return false;
        }
        if (!read_entry_run(&mount->volume, directory_offset, entry_count, displaced_entries)) {
            return false;
        }
    } else if (!read_entry_run(&mount->volume, directory_offset, old_entry_count, displaced_entries)) {
        return false;
    }

    if (!volume_write(&mount->volume, directory_offset, entries, entry_count * EXFAT_ENTRY_SIZE)) {
        if (entry_count > old_entry_count) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    if (entry_count < old_entry_count) {
        uint64_t extra_offset = directory_offset + entry_count * EXFAT_ENTRY_SIZE;
        if (!write_inactive_entry_run(&mount->volume,
                extra_offset,
                old_entry_count - entry_count)) {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
            return false;
        }
    } else if (entry_count > old_entry_count) {
        if (!write_deleted_entries(&mount->volume,
                old_directory_offset,
                directory->secondary_count)) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            return false;
        }
    }

    if (!vfs_unregister_path(old_path)) {
        if (entry_count > old_entry_count) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    char old_path_copy[EXFAT_MAX_PATH];
    copy_cstr(old_path_copy, sizeof(old_path_copy), directory->path);
    uint64_t old_file_entry_offset = directory->file_entry_offset;
    uint8_t old_secondary_count = directory->secondary_count;
    directory->file_entry_offset = directory_offset;
    directory->secondary_count = (uint8_t)(entry_count - 1);
    copy_cstr(directory->path, sizeof(directory->path), new_path);
    bool ok = vfs_register_directory_with_metadata(directory->path, have_metadata ? &metadata : NULL);
    if (ok) {
        (void)exfat_sync_mount_metadata(mount);
        return true;
    }

    if (entry_count > old_entry_count) {
        (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    } else {
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    }
    directory->file_entry_offset = old_file_entry_offset;
    directory->secondary_count = old_secondary_count;
    copy_cstr(directory->path, sizeof(directory->path), old_path_copy);
    (void)vfs_register_directory_with_metadata(directory->path, have_metadata ? &metadata : NULL);
    return false;
}

bool exfat_rename(const char *old_path, const char *new_path) {
    if (old_path == NULL ||
        new_path == NULL ||
        old_path[0] == '\0' ||
        new_path[0] == '\0' ||
        vfs_lookup(new_path) != NULL) {
        return false;
    }

    uint64_t index = 0;
    struct exfat_mount *mount = find_mount_and_index_for_file(old_path, &index);
    if (mount != NULL && mount->volume.image == NULL) {
        return rename_file_in_mount(mount, index, old_path, new_path);
    }

    mount = find_mount_and_index_for_directory(old_path, &index);
    if (mount != NULL && mount->volume.image == NULL && !streq(old_path, mount->mountpoint)) {
        return rename_empty_directory_in_mount(mount, index, old_path, new_path);
    }
    return false;
}

bool exfat_statfs(const char *path, struct exfat_fsinfo *info) {
    if (path == NULL || info == NULL) {
        return false;
    }

    struct exfat_mount *mount = find_mount_for_path(path);
    if (mount == NULL ||
        mount->volume.cluster_size == 0 ||
        mount->volume.cluster_count == 0 ||
        mount->volume.allocation_bitmap_cluster < 2) {
        return false;
    }

    struct exfat_volume *volume = &mount->volume;
    uint64_t bitmap_offset = 0;
    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) || bitmap_bytes == 0) {
        return false;
    }

    uint8_t *buffer = kmalloc(volume->cluster_size);
    if (buffer == NULL) {
        return false;
    }

    uint64_t used_clusters = 0;
    uint64_t bit_base = 0;
    uint64_t remaining_bytes = bitmap_bytes;
    uint64_t offset = bitmap_offset;
    while (remaining_bytes > 0 && bit_base < volume->cluster_count) {
        uint64_t chunk = remaining_bytes < volume->cluster_size ? remaining_bytes : volume->cluster_size;
        if (!volume_read(volume, offset, buffer, chunk)) {
            kfree(buffer);
            return false;
        }

        uint64_t bits = chunk * 8;
        if (bits > volume->cluster_count - bit_base) {
            bits = volume->cluster_count - bit_base;
        }
        used_clusters += count_used_bitmap_bits(buffer, 0, bits);
        bit_base += bits;
        remaining_bytes -= chunk;
        offset += chunk;
    }
    kfree(buffer);

    if (used_clusters > volume->cluster_count) {
        used_clusters = volume->cluster_count;
    }

    for (uint64_t i = 0; i < sizeof(*info); i++) {
        ((uint8_t *)info)[i] = 0;
    }
    info->block_size = volume->cluster_size;
    info->blocks = volume->cluster_count;
    info->blocks_free = volume->cluster_count - used_clusters;
    info->blocks_available = info->blocks_free;
    info->files = mount->file_count + mount->dir_count;
    info->files_free = (EXFAT_MAX_FILES - mount->file_count) + (EXFAT_MAX_DIRS - mount->dir_count);

    const char *fs = "exfat";
    uint64_t i = 0;
    while (fs[i] != '\0' && i + 1 < sizeof(info->filesystem)) {
        info->filesystem[i] = fs[i];
        i++;
    }
    info->filesystem[i] = '\0';

    i = 0;
    while (mount->mountpoint[i] != '\0' && i + 1 < sizeof(info->mountpoint)) {
        info->mountpoint[i] = mount->mountpoint[i];
        i++;
    }
    info->mountpoint[i] = '\0';
    return true;
}

bool exfat_mount_image(const char *mountpoint, const uint8_t *image, uint64_t image_size) {
    if (!valid_mountpoint(mountpoint) || image == NULL || image_size < sizeof(struct exfat_boot_sector)) {
        return false;
    }
    if (mountpoint_in_use(mountpoint)) {
        console_printf("exfat: mountpoint busy: %s\n", mountpoint);
        return false;
    }

    const struct exfat_boot_sector *boot = (const struct exfat_boot_sector *)image;
    if (!streqn(boot->filesystem_name, "EXFAT   ", 8) || boot->boot_signature != 0xaa55) {
        console_write("exfat: invalid boot sector\n");
        return false;
    }

    struct exfat_mount *mount = alloc_mount();
    if (mount == NULL) {
        console_write("exfat: mount table full\n");
        return false;
    }

    mount->volume = (struct exfat_volume) {
        .image = image,
        .image_size = image_size,
        .sector_size = 1ull << boot->bytes_per_sector_shift,
        .sectors_per_cluster = 1ull << boot->sectors_per_cluster_shift,
        .cluster_count = boot->cluster_count,
        .root_directory_cluster = boot->root_directory_cluster,
    };
    mount->volume.cluster_size = mount->volume.sector_size * mount->volume.sectors_per_cluster;
    mount->volume.fat_offset_bytes = (uint64_t)boot->fat_offset * mount->volume.sector_size;
    mount->volume.cluster_heap_offset_bytes = (uint64_t)boot->cluster_heap_offset * mount->volume.sector_size;
    remember_mountpoint(mount, mountpoint);

    if (!scan_root_directory(mount)) {
        console_write("exfat: failed to scan root directory\n");
        clear_mount(mount);
        return false;
    }
    load_mount_metadata(mount);
    vfs_set_metadata_changed_callback(exfat_metadata_changed);

    console_printf("exfat: mounted %s files=%u cluster=%u bytes\n",
        mountpoint,
        mount->file_count,
        mount->volume.cluster_size);
    return true;
}

bool exfat_mount_block_device(const char *mountpoint, struct block_device *device) {
    struct exfat_boot_sector boot;
    uint64_t size = block_device_size(device);
    if (!valid_mountpoint(mountpoint) || device == NULL || size < sizeof(boot)) {
        return false;
    }
    if (mountpoint_in_use(mountpoint)) {
        console_printf("exfat: mountpoint busy: %s\n", mountpoint);
        return false;
    }

    if (!block_read(device, 0, &boot, sizeof(boot))) {
        console_write("exfat: failed to read boot sector\n");
        return false;
    }

    if (!streqn(boot.filesystem_name, "EXFAT   ", 8) || boot.boot_signature != 0xaa55) {
        console_printf("exfat: %s has invalid boot sector\n", device->name);
        return false;
    }

    struct exfat_mount *mount = alloc_mount();
    if (mount == NULL) {
        console_write("exfat: mount table full\n");
        return false;
    }

    mount->volume = (struct exfat_volume) {
        .image = NULL,
        .device = device,
        .image_size = size,
        .sector_size = 1ull << boot.bytes_per_sector_shift,
        .sectors_per_cluster = 1ull << boot.sectors_per_cluster_shift,
        .cluster_count = boot.cluster_count,
        .root_directory_cluster = boot.root_directory_cluster,
    };
    mount->volume.cluster_size = mount->volume.sector_size * mount->volume.sectors_per_cluster;
    mount->volume.fat_offset_bytes = (uint64_t)boot.fat_offset * mount->volume.sector_size;
    mount->volume.cluster_heap_offset_bytes = (uint64_t)boot.cluster_heap_offset * mount->volume.sector_size;
    remember_mountpoint(mount, mountpoint);

    if (!scan_root_directory(mount)) {
        console_write("exfat: failed to scan block root directory\n");
        clear_mount(mount);
        return false;
    }
    load_mount_metadata(mount);
    vfs_set_metadata_changed_callback(exfat_metadata_changed);

    console_printf("exfat: mounted %s from %s files=%u cluster=%u bytes\n",
        mountpoint,
        device->name,
        mount->file_count,
        mount->volume.cluster_size);
    return true;
}
