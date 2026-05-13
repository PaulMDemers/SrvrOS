#ifndef SRVROS_VFS_H
#define SRVROS_VFS_H

#include <stdbool.h>
#include <stdint.h>

enum vfs_node_type {
    VFS_NODE_FILE,
    VFS_NODE_DIRECTORY,
};

struct vfs_node;
typedef bool (*vfs_read_all_fn)(const struct vfs_node *node, const uint8_t **data, uint64_t *size);
typedef void (*vfs_release_data_fn)(const struct vfs_node *node, const uint8_t *data);

struct vfs_node {
    bool used;
    const char *path;
    enum vfs_node_type type;
    uint64_t size;
    const void *private_data;
    vfs_read_all_fn read_all;
    vfs_release_data_fn release_data;
};

void vfs_init(void);
bool vfs_register_file(const char *path, uint64_t size, const void *private_data, vfs_read_all_fn read_all);
bool vfs_register_directory(const char *path);
bool vfs_register_file_with_release(const char *path,
    uint64_t size,
    const void *private_data,
    vfs_read_all_fn read_all,
    vfs_release_data_fn release_data);
uint64_t vfs_count(void);
const struct vfs_node *vfs_node_at(uint64_t index);
const struct vfs_node *vfs_lookup(const char *path);
bool vfs_read_all(const struct vfs_node *node, const uint8_t **data, uint64_t *size);
void vfs_release_data(const struct vfs_node *node, const uint8_t *data);
bool vfs_update_file_size(const char *path, uint64_t size);
bool vfs_unregister_path(const char *path);
uint64_t vfs_unregister_prefix(const char *prefix);

#endif
