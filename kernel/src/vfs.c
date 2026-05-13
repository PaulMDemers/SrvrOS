#include <srvros/console.h>
#include <srvros/initramfs.h>
#include <srvros/vfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VFS_MAX_NODES 256

static struct vfs_node nodes[VFS_MAX_NODES];
static uint64_t node_count;

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
    if (node_count >= VFS_MAX_NODES || path == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && streq(nodes[i].path, path)) {
            return true;
        }
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i] = (struct vfs_node) {
                .used = true,
                .path = path,
                .type = VFS_NODE_DIRECTORY,
                .size = 0,
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
    if (node_count >= VFS_MAX_NODES || path == NULL || read_all == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i] = (struct vfs_node) {
                .used = true,
                .path = path,
                .type = VFS_NODE_FILE,
                .size = size,
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
    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && streq(nodes[i].path, path)) {
            return &nodes[i];
        }
    }

    return NULL;
}

bool vfs_read_all(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    if (node == NULL || !node->used || node->type != VFS_NODE_FILE || node->read_all == NULL) {
        return false;
    }

    return node->read_all(node, data, size);
}

void vfs_release_data(const struct vfs_node *node, const uint8_t *data) {
    if (node != NULL && data != NULL && node->release_data != NULL) {
        node->release_data(node, data);
    }
}

bool vfs_update_file_size(const char *path, uint64_t size) {
    if (path == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && streq(nodes[i].path, path)) {
            nodes[i].size = size;
            return true;
        }
    }
    return false;
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
