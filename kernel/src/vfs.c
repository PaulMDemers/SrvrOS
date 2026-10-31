#include <srvros/console.h>
#include <srvros/initramfs.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/heap.h>
#include <srvros/syscall_numbers.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Node addresses remain stable while capacity grows. Retain allocated blocks
 * until vfs_init: callers may still inspect an unregistered node's used flag. */
#define VFS_MAX_NODES 32768
#define VFS_NODE_BLOCK 256
static struct vfs_node boot_nodes[VFS_NODE_BLOCK];
static struct vfs_node *node_blocks[VFS_MAX_NODES / VFS_NODE_BLOCK] = {boot_nodes};
static uint64_t node_capacity = VFS_NODE_BLOCK;
static uint64_t node_count;
/* Sorted slot IDs preserve enumeration order and make node_at constant-time. */
static uint32_t live_slots[VFS_MAX_NODES];
static struct vfs_node *node_slot(uint64_t i) {
    return &node_blocks[i / VFS_NODE_BLOCK][i % VFS_NODE_BLOCK];
}
bool vfs_reserve_nodes(uint64_t additional) {
    if(additional > VFS_MAX_NODES-node_count) return false;
    while(node_capacity-node_count < additional) {
        struct vfs_node *block=kcalloc(VFS_NODE_BLOCK,sizeof(*block));
        if(!block) return false;
        node_blocks[node_capacity/VFS_NODE_BLOCK]=block;
        node_capacity+=VFS_NODE_BLOCK;
    }
    return true;
}
static void publish_slot(uint64_t slot) {
    uint64_t at=node_count;
    while(at && live_slots[at-1]>slot) { live_slots[at]=live_slots[at-1];at--; }
    live_slots[at]=(uint32_t)slot;node_count++;
}
static void remove_slot(uint64_t slot) {
    for(uint64_t i=0;i<node_count;i++) if(live_slots[i]==slot) {
        for(uint64_t j=i+1;j<node_count;j++) live_slots[j-1]=live_slots[j];
        node_slot(slot)->used=false;node_count--;return;
    }
}
/* The boot archive has bounded paths and file count. Keep synthesized directory
 * names alive independently of the temporary prefix used while walking a path. */
#define VFS_BOOT_DIRECTORIES 256
#define VFS_BOOT_PATH 256
static char boot_directories[VFS_BOOT_DIRECTORIES][VFS_BOOT_PATH];
static unsigned boot_directory_count;
static uint64_t next_inode = 1;
static vfs_metadata_changed_fn metadata_changed_callback;

static bool streq(const char *a, const char *b);

static uint64_t metadata_time(void) {
    int64_t now = timer_realtime_us();
    /* Zero denotes unknown before a valid RTC seed; uptime is not a date. */
    return now >= 0 ? (uint64_t)now / 1000000 : 0;
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

/* Persisted metadata and freshly imported files share one live identity space.
 * Never let an imported ID alias another node (including a different mount).
 * The bounded node table guarantees a free nonzero ID even after wraparound. */
static bool inode_in_use(uint64_t inode, const struct vfs_node *self) {
    for (uint64_t i = 0; i < node_capacity; i++) {
        if (node_slot(i)->used && node_slot(i) != self && node_slot(i)->metadata.inode == inode) return true;
    }
    return false;
}

static uint64_t allocate_inode(void) {
    while (!next_inode || inode_in_use(next_inode, NULL)) next_inode++;
    return next_inode++;
}

static struct vfs_metadata import_metadata(const struct vfs_metadata *metadata,
    const struct vfs_node *self) {
    struct vfs_metadata result = *metadata;
    if (!result.inode || inode_in_use(result.inode, self)) result.inode = allocate_inode();
    if (result.inode >= next_inode) next_inode = result.inode + 1;
    return result;
}

static struct vfs_metadata default_metadata(const char *path, enum vfs_node_type type) {
    uint64_t now = metadata_time();
    uint64_t permissions = type == VFS_NODE_DIRECTORY ? 0755 : 0644;
    if (type == VFS_NODE_FILE && executable_default_path(path)) {
        permissions = 0755;
    }
    return (struct vfs_metadata) {
        .inode = allocate_inode(),
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
    for (uint64_t i = 0; i < node_capacity; i++) {
        if (node_slot(i)->used && streq(node_slot(i)->path, path)) {
            return node_slot(i);
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

    for (uint64_t i = 0; i < node_capacity; i++) {
        if (node_slot(i)->used && streq(node_slot(i)->path, path)) {
            if (metadata != NULL) {
                node_slot(i)->metadata = import_metadata(metadata, node_slot(i));
            }
            return true;
        }
    }

    if (!vfs_reserve_nodes(1)) {
        return false;
    }

    for (uint64_t i = 0; i < node_capacity; i++) {
        if (!node_slot(i)->used) {
            (*node_slot(i)) = (struct vfs_node) {
                .used = true,
                .path = path,
                .type = VFS_NODE_DIRECTORY,
                .size = 0,
                .metadata = metadata != NULL ? import_metadata(metadata, NULL) : default_metadata(path, VFS_NODE_DIRECTORY),
                .private_data = NULL,
                .read_all = NULL,
                .release_data = NULL,
            };
            publish_slot(i);
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
            existing->metadata = import_metadata(metadata, existing);
        }
        return true;
    }

    if (!vfs_reserve_nodes(1)) {
        return false;
    }

    for (uint64_t i = 0; i < node_capacity; i++) {
        if (!node_slot(i)->used) {
            (*node_slot(i)) = (struct vfs_node) {
                .used = true,
                .path = path,
                .type = VFS_NODE_FILE,
                .size = size,
                .metadata = metadata != NULL ? import_metadata(metadata, NULL) : default_metadata(path, VFS_NODE_FILE),
                .private_data = private_data,
                .read_all = read_all,
                .release_data = release_data,
            };
            publish_slot(i);
            return true;
        }
    }
    return false;
}

static bool boot_parents(const char *path) {
    if(path == NULL || path[0] != '/' || !path[1]) return false;
    char prefix[VFS_BOOT_PATH];unsigned length=0;
    while(path[length]) {
        if(length+1>=sizeof(prefix)) return false;
        prefix[length]=path[length];length++;
    }
    prefix[length]=0;
    unsigned component=1;
    for(unsigned i=1;i<=length;i++) if(!prefix[i] || prefix[i]=='/') {
        unsigned count=i-component;
        if(!count || (count==1 && prefix[component]=='.') ||
           (count==2 && prefix[component]=='.' && prefix[component+1]=='.')) return false;
        component=i+1;
    }
    for(unsigned i=1;i<length;i++) {
        if(prefix[i]!='/') continue;
        if(prefix[i-1]=='/' || i+1==length) return false;
        prefix[i]=0;
        const struct vfs_node *node=vfs_lookup(prefix);
        if(node && node->type!=VFS_NODE_DIRECTORY) return false;
        if(!node) {
            if(boot_directory_count==VFS_BOOT_DIRECTORIES) return false;
            char *name=boot_directories[boot_directory_count];
            for(unsigned j=0;j<=i;j++) name[j]=prefix[j];
            if(!vfs_register_directory(name)) return false;
            boot_directory_count++;
        }
        prefix[i]='/';
    }
    const struct vfs_node *existing=vfs_lookup(path);
    return !existing || existing->type==VFS_NODE_FILE;
}

void vfs_init(void) {
    node_count = 0;
    next_inode = 1;
    boot_directory_count = 0;
    metadata_changed_callback = NULL;
    for(uint64_t i=1;i<node_capacity/VFS_NODE_BLOCK;i++) {
        kfree(node_blocks[i]);node_blocks[i]=NULL;
    }
    node_capacity=VFS_NODE_BLOCK;
    for(uint64_t i=0;i<VFS_NODE_BLOCK;i++) boot_nodes[i].used=false;

    (void)vfs_register_directory("/");
    uint64_t skipped=0;
    for (uint64_t i = 0; i < initramfs_file_count() && node_count < VFS_MAX_NODES; i++) {
        const struct initramfs_file *file = initramfs_file_at(i);
        if(!file || !boot_parents(file->path)) { skipped++;continue; }
        struct vfs_metadata metadata=default_metadata(file->path,VFS_NODE_FILE);
        metadata.mode=VFS_MODE_IFREG|file->mode;
        if(!vfs_register_file_with_metadata(file->path,file->size,file,initramfs_read_all,NULL,&metadata)) skipped++;
    }

    console_printf("vfs: mounted initramfs nodes=%u skipped=%u\n", node_count, skipped);
}

uint64_t vfs_count(void) {
    return node_count;
}

uint64_t vfs_free_nodes(void) {
    return VFS_MAX_NODES - node_count;
}

const struct vfs_node *vfs_node_at(uint64_t index) {
    return index < node_count ? node_slot(live_slots[index]) : NULL;
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

static bool notify_metadata_changed(const struct vfs_node *node) {
    return metadata_changed_callback == NULL || node == NULL ||
        metadata_changed_callback(node->path, &node->metadata);
}

bool vfs_update_file_size(const char *path, uint64_t size) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL) {
        return false;
    }

    node->size = size;
    node->metadata.mtime = metadata_time();
    node->metadata.ctime = node->metadata.mtime;
    return notify_metadata_changed(node);
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
    node->metadata = import_metadata(metadata, node);
    return true;
}

bool vfs_chmod(const char *path, uint64_t mode) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL) {
        return false;
    }
    struct vfs_metadata before = node->metadata;
    node->metadata.mode = (node->metadata.mode & VFS_MODE_IFMT) | (mode & 07777);
    node->metadata.ctime = metadata_time();
    if (notify_metadata_changed(node)) return true;
    /* Restore live state only: a backend failure may have changed media and
     * the backend owns quarantine rather than promising a durable rollback. */
    node->metadata = before;
    return false;
}

void vfs_metadata_utime(struct vfs_metadata *metadata,uint64_t atime,uint64_t mtime) {
    if(atime==SRV_TIME_OMIT && mtime==SRV_TIME_OMIT) return;
    uint64_t now=metadata_time();
    if(atime!=SRV_TIME_OMIT) metadata->atime=atime==SRV_TIME_NOW?now:atime;
    if(mtime!=SRV_TIME_OMIT) metadata->mtime=mtime==SRV_TIME_NOW?now:mtime;
    metadata->ctime=now;
}
bool vfs_utime(const char *path, uint64_t atime, uint64_t mtime) {
    struct vfs_node *node = mutable_lookup(path);
    if (node == NULL) {
        return false;
    }
    if(atime==SRV_TIME_OMIT && mtime==SRV_TIME_OMIT) return true;
    struct vfs_metadata before = node->metadata;
    vfs_metadata_utime(&node->metadata,atime,mtime);
    if (notify_metadata_changed(node)) return true;
    node->metadata = before;
    return false;
}

bool vfs_replace_file_path(const char *old_path, const char *new_path,
    char *storage, uint64_t capacity) {
    if(!old_path || !new_path || !storage || !capacity) return false;
    struct vfs_node *source=mutable_lookup(old_path), *target=mutable_lookup(new_path);
    if(!source || !target || source==target || source->type!=VFS_NODE_FILE ||
        target->type!=VFS_NODE_FILE) return false;
    uint64_t length=0;while(length<capacity && new_path[length]) length++;
    if(length==capacity) return false;
    if(!vfs_unregister_path(new_path)) return false;
    for(uint64_t i=0;i<=length;i++) storage[i]=new_path[i];
    source->path=storage;
    source->metadata.ctime=metadata_time();
    return true;
}

bool vfs_unregister_path(const char *path) {
    if (path == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < node_capacity; i++) {
        if (node_slot(i)->used && streq(node_slot(i)->path, path)) {
            remove_slot(i);
            return true;
        }
    }
    return false;
}

uint64_t vfs_unregister_prefix(const char *prefix) {
    uint64_t kept=0,old=node_count;
    for(uint64_t i=0;i<old;i++) {
        struct vfs_node *node=node_slot(live_slots[i]);
        if(path_under_prefix(node->path,prefix)) node->used=false;
        else live_slots[kept++]=live_slots[i];
    }
    node_count=kept;
    return old-kept;
}
