#ifndef SRVROS_VFS_NAMESPACE_H
#define SRVROS_VFS_NAMESPACE_H

#include <stdbool.h>
#include <stddef.h>

#include <srvros/vfs.h>

/* Path capacity includes the terminating NUL. The table owns its strings. */
#define VFS_NAMESPACE_MAX_MAPPINGS 8
#define VFS_NAMESPACE_PATH_MAX 160

struct vfs_namespace_mapping {
    char source[VFS_NAMESPACE_PATH_MAX];
    char target[VFS_NAMESPACE_PATH_MAX];
};

struct vfs_namespace {
    struct vfs_namespace_mapping mappings[VFS_NAMESPACE_MAX_MAPPINGS];
    unsigned count;
};

/* This is an explicit view, not a global mount or a permission boundary.
 * Inputs must be canonical absolute paths: no empty components, dot or
 * dot-dot, and no trailing slash except for root. No symlink semantics are
 * provided. Initialize before use; callers must not modify the table directly.
 *
 * The caller owns filesystem exclusion across bind or resolve and any operation
 * that uses its result. Keep the resolved path under that exclusion until the
 * operation completes. The table caches no nodes; changes to the raw VFS can
 * invalidate a binding. The caller also serializes access to its table. */
void vfs_namespace_init(struct vfs_namespace *ns);
/* Source must be unoccupied and non-root; target must be a live directory.
 * Both paths' raw VFS ancestors must be live directories when binding. */
bool vfs_namespace_bind(struct vfs_namespace *ns, const char *source, const char *target);

/* Resolve once to a canonical raw VFS path. Existing intermediate components
 * must be directories; the final component may be absent for creation. A
 * binding's source ancestors must still be directories, its raw source tree
 * unoccupied, and its target a live directory. On failure, out[0] is NUL
 * when out is non-NULL and capacity is nonzero. Input and output must not alias. */
bool vfs_namespace_resolve(const struct vfs_namespace *ns, const char *path,
    char *out, size_t capacity);
/* Returns the raw node itself, preserving its pointer and inode identity. */
const struct vfs_node *vfs_namespace_lookup(const struct vfs_namespace *ns, const char *path);

#endif
