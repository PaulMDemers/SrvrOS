#include <srvros/vfs_namespace.h>
#include <srvros/syscall_numbers.h>

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

/* The global view owns no VFS nodes and changes only under filesystem
 * exclusion. Retired mappings are removed, rather than rediscovered by name. */
static struct vfs_namespace system_view;

static void system_retire(const char *path, bool prefix) {
    (void)prefix;
    unsigned kept = 0;
    for (unsigned i = 0; i < system_view.count; i++) {
        struct vfs_namespace_mapping *m = &system_view.mappings[i];
        if (under(m->source, path) || under(m->target, path)) continue;
        if (kept != i) system_view.mappings[kept] = *m;
        kept++;
    }
    system_view.count = kept;
}

void vfs_namespace_system_reset(void) {
    system_view.count = 0;
    vfs_set_retire_callback(NULL);
}

bool vfs_namespace_system_activate(void) {
    static const char *const sources[] = {"/bin", "/usr", "/var", "/home"};
    static const char *const targets[] = {"/fat/bin", "/fat", "/fat/var", "/fat/home"};
    struct vfs_namespace candidate;
    vfs_namespace_init(&candidate);
    for (unsigned i = 0; i < sizeof(sources)/sizeof(sources[0]); i++) {
        if (!vfs_namespace_bind(&candidate, sources[i], targets[i])) return false;
    }
    system_view = candidate;
    vfs_set_retire_callback(system_retire);
    return true;
}

static bool system_resolve(const char *path, char *out, size_t capacity) {
    if (!system_view.count) {
        size_t length = 0;
        if (out && capacity) out[0] = '\0';
        if (!path || !out || !capacity) return false;
        while (length < capacity && path[length]) length++;
        if (length == capacity) return false;
        copy_path(out, path, length);
        return true;
    }
    return vfs_namespace_resolve(&system_view, path, out, capacity);
}

int64_t vfs_namespace_system_path(const char *path, char *out, size_t capacity) {
    if (out && capacity) out[0] = '\0';
    if (!path || !out || !capacity) return SRV_ERR_FAULT;
    size_t length = 0;
    while (length < VFS_NAMESPACE_PATH_MAX && path[length]) length++;
    if (length == VFS_NAMESPACE_PATH_MAX) return SRV_ERR_NAMETOOLONG;
    size_t result_length = length;
    for (unsigned i = 0; i < system_view.count; i++) {
        const struct vfs_namespace_mapping *m = &system_view.mappings[i];
        if (!under(path, m->source)) continue;
        size_t source_length = 0, target_length = 0;
        while (m->source[source_length]) source_length++;
        while (m->target[target_length]) target_length++;
        result_length = target_length + length - source_length;
        break;
    }
    if (result_length >= VFS_NAMESPACE_PATH_MAX || result_length >= capacity)
        return SRV_ERR_NAMETOOLONG;
    return system_resolve(path, out, capacity) ? 0 : SRV_ERR_NOENT;
}

bool vfs_namespace_system_resolve(const char *path, char *out, size_t capacity) {
    return vfs_namespace_system_path(path, out, capacity) == 0;
}

const struct vfs_node *vfs_namespace_system_lookup(const char *path) {
    char resolved[VFS_NAMESPACE_PATH_MAX];
    if (!vfs_namespace_system_resolve(path, resolved, sizeof(resolved))) return NULL;
    return vfs_lookup(resolved);
}

/* Return a valid binding's target only if the source remains usable. */
static const struct vfs_node *system_mapping_node(unsigned i) {
    char resolved[VFS_NAMESPACE_PATH_MAX];
    const struct vfs_namespace_mapping *m = &system_view.mappings[i];
    if (!vfs_namespace_resolve(&system_view, m->source, resolved, sizeof(resolved))) return NULL;
    return vfs_lookup(resolved);
}

bool vfs_namespace_system_protects(const char *canonical_path) {
    size_t length;
    if (!canonical(canonical_path, &length)) return false;
    for (unsigned i = 0; i < system_view.count; i++) {
        if (system_mapping_node(i) && under(system_view.mappings[i].target, canonical_path)) return true;
    }
    return false;
}

static int list_result(const char *path, const struct vfs_node *node, char *out,
    size_t capacity, const struct vfs_node **node_out) {
    size_t length = 0;
    while (length < capacity && path[length]) length++;
    if (length == capacity) return -1;
    copy_path(out, path, length);
    *node_out = node;
    return 1;
}

int vfs_namespace_system_root(unsigned index, char *out, size_t capacity,
    const struct vfs_node **node_out) {
    if (out && capacity) out[0] = '\0';
    if (node_out) *node_out = NULL;
    if (!out || !capacity || !node_out) return -1;
    for (unsigned i = 0; i < system_view.count; i++) {
        const struct vfs_node *node = system_mapping_node(i);
        if (!node) continue;
        if (index-- == 0) return list_result(system_view.mappings[i].source, node, out, capacity, node_out);
    }
    return 0;
}

int vfs_namespace_system_list(uint64_t index, char *out, size_t capacity,
    const struct vfs_node **node_out) {
    if (out && capacity) out[0] = '\0';
    if (node_out) *node_out = NULL;
    if (!out || !capacity || !node_out) return -1;
    uint64_t raw_count = vfs_count();
    if (index < raw_count) {
        const struct vfs_node *node = vfs_node_at(index);
        return node ? list_result(node->path, node, out, capacity, node_out) : -1;
    }
    index -= raw_count;
    for (unsigned i = 0; i < system_view.count; i++) {
        const struct vfs_namespace_mapping *m = &system_view.mappings[i];
        if (!system_mapping_node(i)) continue;
        size_t source_length = 0, target_length = 0;
        while (m->source[source_length]) source_length++;
        while (m->target[target_length]) target_length++;
        for (uint64_t j = 0; j < raw_count; j++) {
            const struct vfs_node *node = vfs_node_at(j);
            if (!node || !under(node->path, m->target)) continue;
            if (index--) continue;
            size_t suffix_length = 0;
            while (node->path[target_length + suffix_length]) suffix_length++;
            size_t length = source_length + suffix_length;
            if (length >= capacity) return -1;
            for (size_t k = 0; k < source_length; k++) out[k] = m->source[k];
            for (size_t k = 0; k <= suffix_length; k++) out[source_length+k] = node->path[target_length+k];
            *node_out = node;
            return 1;
        }
    }
    return 0;
}
