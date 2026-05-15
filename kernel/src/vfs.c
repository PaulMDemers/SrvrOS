#include <srvros/console.h>
#include <srvros/initramfs.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VFS_MAX_NODES 512

static struct vfs_node nodes[VFS_MAX_NODES];
static uint64_t node_count;
static uint64_t next_inode = 1;
static vfs_metadata_changed_fn metadata_changed_callback;

static bool streq(const char *a, const char *b);

static uint64_t metadata_time(void) {
    return timer_ticks() / 100;
}

static bool starts_with(const char *text, const char *prefix) {
    uint64_t i = 0;
    if (text == NULL || prefix == NULL) {
        return false;
    }
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

static bool executable_default_path(const char *path) {
    return starts_with(path, "/fat/bin/") ||
        streq(path, "/init") ||
        streq(path, "/sh") ||
        streq(path, "/ls") ||
        streq(path, "/echo");
}

static struct vfs_metadata default_metadata(const char *path, enum vfs_node_type type) {
    uint64_t now = metadata_time();
    uint64_t permissions = type == VFS_NODE_DIRECTORY ? 0755 : 0644;
    if (type == VFS_NODE_FILE && executable_default_path(path)) {
        permissions = 0755;
    }
    return (struct vfs_metadata) {
        .inode = next_inode++,
        .mode = (type == VFS_NODE_DIRECTORY ? VFS_MODE_IFDIR : VFS_MODE_IFREG) | permissions,
        .nlink = type == VFS_NODE_DIRECTORY ? 2 : 1,
        .uid = 0,
        .gid = 0,
        .atime = now,
        .mtime = now,
        .ctime = now,
    };
}

static struct vfs_node *mutable_lookup(const char *path) {
    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && streq(nodes[i].path, path)) {
            return &nodes[i];
        }
    }
    return NULL;
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

static bool path_under_prefix(const char *path, const char *prefix) {
    if (path == NULL || prefix == NULL || prefix[0] == '\0') {
        return false;
    }

    uint64_t i = 0;
    while (prefix[i] != '\0') {
        if (path[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static bool initramfs_read_all(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    if (node == NULL || node->type != VFS_NODE_FILE || data == NULL || size == NULL) {
        return false;
    }

    const struct initramfs_file *file = node->private_data;
    *data = file->data;
    *size = file->size;
    return true;
}

bool vfs_register_file(const char *path, uint64_t size, const void *private_data, vfs_read_all_fn read_all) {
    return vfs_register_file_with_release(path, size, private_data, read_all, NULL);
}

bool vfs_register_directory(const char *path) {
    return vfs_register_directory_with_metadata(path, NULL);
}

bool vfs_register_directory_with_metadata(const char *path, const struct vfs_metadata *metadata) {
    if (path == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && streq(nodes[i].path, path)) {
            if (metadata != NULL) {
                nodes[i].metadata = *metadata;
            }
            return true;
        }
    }

    if (node_count >= VFS_MAX_NODES) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i] = (struct vfs_node) {
                .used = true,
                .path = path,
                .type = VFS_NODE_DIRECTORY,
                .size = 0,
                .metadata = metadata != NULL ? *metadata : default_metadata(path, VFS_NODE_DIRECTORY),
                .private_data = NULL,
                .read_all = NULL,
                .release_data = NULL,
            };
            node_count++;
            return true;
        }
    }
    return false;
}

bool vfs_register_file_with_release(const char *path,
    uint64_t size,
    const void *private_data,
    vfs_read_all_fn read_all,
    vfs_release_data_fn release_data) {
    return vfs_register_file_with_metadata(path, size, private_data, read_all, release_data, NULL);
}

bool vfs_register_file_with_metadata(const char *path,
    uint64_t size,
    const void *private_data,
    vfs_read_all_fn read_all,
    vfs_release_data_fn release_data,
    const struct vfs_metadata *metadata) {
    if (path == NULL || read_all == NULL) {
        return false;
    }

    struct vfs_node *existing = mutable_lookup(path);
    if (existing != NULL) {
        if (existing->type != VFS_NODE_FILE) {
            return false;
        }
        existing->size = size;
        existing->private_data = private_data;
        existing->read_all = read_all;
        existing->release_data = release_data;
        if (metadata != NULL) {
            existing->metadata = *metadata;
        }
        return true;
    }

    if (node_count >= VFS_MAX_NODES) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i] = (struct vfs_node) {
                .used = true,
                .path = path,
                .type = VFS_NODE_FILE,
                .size = size,
                .metadata = metadata != NULL ? *metadata : default_metadata(path, VFS_NODE_FILE),
                .private_data = private_data,
                .read_all = read_all,
                .release_data = release_data,
            };
            node_count++;
            return true;
        }
    }
    return false;
}

void vfs_init(void) {
    node_count = 0;
    next_inode = 1;
    metadata_changed_callback = NULL;
    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        nodes[i].used = false;
    }

    for (uint64_t i = 0; i < initramfs_file_count() && node_count < VFS_MAX_NODES; i++) {
        const struct initramfs_file *file = initramfs_file_at(i);
        vfs_register_file(file->path, file->size, file, initramfs_read_all);
    }

    console_printf("vfs: mounted initramfs nodes=%u\n", node_count);
}

uint64_t vfs_count(void) {
    return node_count;
}

const struct vfs_node *vfs_node_at(uint64_t index) {
    uint64_t seen = 0;
    if (index >= node_count) {
        return NULL;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            continue;
        }
        if (seen == index) {
            return &nodes[i];
        }
        seen++;
    }

    return NULL;
}

const struct vfs_node *vfs_lookup(const char *path) {
    return mutable_lookup(path);
}

bool vfs_read_all(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    if (node == NULL || !node->used || node->type != VFS_NODE_FILE || node->read_all == NULL) {
        return false;
    }

    bool ok = node->read_all(node, data, size);
    if (ok) {
        ((struct vfs_node *)node)->metadata.atime = metadata_time();
    }
    return ok;
}

void vfs_release_data(const struct vfs_node *node, const uint8_t *data) {
    if (node != NULL && data != NULL && node->release_data != NULL) {
        node->release_data(node, data);
    }
}

void vfs_set_metadata_changed_callback(vfs_metadata_changed_fn callback) {
    metadata_changed_callback = callback;
}

static void notify_metadata_changed(const struct vfs_node *node) {
    if (metadata_changed_callback != NULL && node != NULL) {
        metadata_changed_callback(node->path, &node->metadata);
    }
}

bool vfs_update_file_size(const char *path, uint64_t size) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL) {
        return false;
    }

    node->size = size;
    node->metadata.mtime = metadata_time();
    node->metadata.ctime = node->metadata.mtime;
    notify_metadata_changed(node);
    return true;
}

bool vfs_stat(const char *path, struct vfs_metadata *metadata_out, uint64_t *size_out, uint64_t *type_out) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL) {
        return false;
    }
    if (metadata_out != NULL) {
        *metadata_out = node->metadata;
    }
    if (size_out != NULL) {
        *size_out = node->size;
    }
    if (type_out != NULL) {
        *type_out = (uint64_t)node->type;
    }
    return true;
}

bool vfs_set_metadata(const char *path, const struct vfs_metadata *metadata) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL || metadata == NULL) {
        return false;
    }
    node->metadata = *metadata;
    if (node->metadata.inode >= next_inode) {
        next_inode = node->metadata.inode + 1;
    }
    return true;
}

bool vfs_chmod(const char *path, uint64_t mode) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL) {
        return false;
    }
    node->metadata.mode = (node->metadata.mode & VFS_MODE_IFMT) | (mode & 07777);
    node->metadata.ctime = metadata_time();
    notify_metadata_changed(node);
    return true;
}

bool vfs_unregister_path(const char *path) {
    if (path == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && streq(nodes[i].path, path)) {
            nodes[i].used = false;
            if (node_count > 0) {
                node_count--;
            }
            return true;
        }
    }
    return false;
}

uint64_t vfs_unregister_prefix(const char *prefix) {
    uint64_t removed = 0;
    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used || !path_under_prefix(nodes[i].path, prefix)) {
            continue;
        }
        nodes[i].used = false;
        removed++;
        if (node_count > 0) {
            node_count--;
        }
    }
    return removed;
}
