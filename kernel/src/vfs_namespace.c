#include <srvros/vfs_namespace.h>

/* Scan at most the public path capacity, including the terminator. */
static bool canonical(const char *path, size_t *length_out) {
    size_t length = 0, component = 1;
    if (!path || path[0] != '/') return false;
    while (length < VFS_NAMESPACE_PATH_MAX && path[length]) length++;
    if (length == VFS_NAMESPACE_PATH_MAX) return false;
    if (length == 1) {
        *length_out = length;
        return true;
    }
    for (size_t i = 1; i <= length; i++) {
        if (path[i] != '/' && path[i] != '\0') continue;
        size_t n = i - component;
        if (!n || (n == 1 && path[component] == '.') ||
            (n == 2 && path[component] == '.' && path[component + 1] == '.')) return false;
        component = i + 1;
    }
    *length_out = length;
    return true;
}

static bool under(const char *path, const char *prefix) {
    size_t i = 0;
    if (prefix[0] == '/' && prefix[1] == '\0') return true;
    while (prefix[i]) {
        if (path[i] != prefix[i]) return false;
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static bool overlaps(const char *a, const char *b) {
    return under(a, b) || under(b, a);
}

static void copy_path(char *out, const char *in, size_t length) {
    for (size_t i = 0; i <= length; i++) out[i] = in[i];
}

static bool directory(const char *path) {
    const struct vfs_node *node = vfs_lookup(path);
    return node && node->used && node->type == VFS_NODE_DIRECTORY;
}

/* Check every ancestor, including root, but allow an absent final leaf. */
static bool parents_are_directories(char *path, size_t length) {
    if (length == 1) return directory("/");
    if (!directory("/")) return false;
    for (size_t i = 1; i < length; i++) {
        if (path[i] != '/') continue;
        path[i] = '\0';
        bool ok = directory(path);
        path[i] = '/';
        if (!ok) return false;
    }
    return true;
}

/* A raw descendant would be hidden even if the source node itself is absent. */
static bool source_unoccupied(const char *source) {
    for (uint64_t i = 0; i < vfs_count(); i++) {
        const struct vfs_node *node = vfs_node_at(i);
        if (node && node->used && under(node->path, source)) return false;
    }
    return true;
}

void vfs_namespace_init(struct vfs_namespace *ns) {
    if (ns) ns->count = 0;
}

bool vfs_namespace_bind(struct vfs_namespace *ns, const char *source, const char *target) {
    size_t source_length, target_length;
    if (!ns || ns->count >= VFS_NAMESPACE_MAX_MAPPINGS ||
        !canonical(source, &source_length) || !canonical(target, &target_length) ||
        source_length == 1 || overlaps(source, target) || !directory(target)) return false;

    /* Attachment and storage must both be reachable through raw directories. */
    char checked[VFS_NAMESPACE_PATH_MAX];
    copy_path(checked, source, source_length);
    if (!parents_are_directories(checked, source_length)) return false;
    copy_path(checked, target, target_length);
    if (!parents_are_directories(checked, target_length)) return false;

    if (!source_unoccupied(source)) return false;
    for (unsigned i = 0; i < ns->count; i++) {
        const struct vfs_namespace_mapping *m = &ns->mappings[i];
        if (overlaps(source, m->source) || overlaps(source, m->target) ||
            overlaps(target, m->source)) return false;
    }
    struct vfs_namespace_mapping *m = &ns->mappings[ns->count];
    copy_path(m->source, source, source_length);
    copy_path(m->target, target, target_length);
    ns->count++;
    return true;
}

bool vfs_namespace_resolve(const struct vfs_namespace *ns, const char *path,
    char *out, size_t capacity) {
    size_t length;
    if (out && capacity) out[0] = '\0';
    if (!ns || ns->count > VFS_NAMESPACE_MAX_MAPPINGS || !out || !capacity ||
        !canonical(path, &length)) return false;
    const struct vfs_namespace_mapping *mapping = NULL;
    for (unsigned i = 0; i < ns->count; i++) {
        if (under(path, ns->mappings[i].source)) {
            mapping = &ns->mappings[i];
            break;
        }
    }
    char resolved[VFS_NAMESPACE_PATH_MAX];
    size_t result_length = length;
    if (mapping) {
        size_t source_length = 0, target_length = 0;
        while (mapping->source[source_length]) source_length++;
        while (mapping->target[target_length]) target_length++;
        copy_path(resolved, mapping->source, source_length);
        if (!parents_are_directories(resolved, source_length) ||
            !source_unoccupied(mapping->source) || !directory(mapping->target)) return false;
        result_length = target_length + length - source_length;
        if (result_length >= sizeof(resolved)) return false;
        for (size_t i = 0; i < target_length; i++) resolved[i] = mapping->target[i];
        for (size_t i = source_length; i <= length; i++)
            resolved[target_length + i - source_length] = path[i];
    } else {
        copy_path(resolved, path, length);
    }
    if (result_length >= capacity || !parents_are_directories(resolved, result_length)) return false;
    copy_path(out, resolved, result_length);
    return true;
}

const struct vfs_node *vfs_namespace_lookup(const struct vfs_namespace *ns, const char *path) {
    char resolved[VFS_NAMESPACE_PATH_MAX];
    if (!vfs_namespace_resolve(ns, path, resolved, sizeof(resolved))) return NULL;
    return vfs_lookup(resolved);
}
