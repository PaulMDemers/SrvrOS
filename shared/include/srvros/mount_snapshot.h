#ifndef SRVROS_MOUNT_SNAPSHOT_H
#define SRVROS_MOUNT_SNAPSHOT_H

#include <stdint.h>
#include <srvros/path_limits.h>

#define SRV_MOUNT_SNAPSHOT_VERSION 1u
/* Base VFS, four mounted volumes, and four installed-root projections. */
#define SRV_MOUNT_SNAPSHOT_MAX 9u
#define SRV_MOUNT_VFS 1u
#define SRV_MOUNT_EXFAT 2u
#define SRV_MOUNT_READONLY 1u
#define SRV_MOUNT_PROJECTION 2u

struct srv_mount_entry {
    uint32_t filesystem;
    uint32_t flags;
    char mount_path[SRV_PATH_MAX];
    char root_path[8]; /* Backing-relative root of an installed projection. */
    char source[32];  /* Opaque native backing label. */
};

struct srv_mount_snapshot {
    uint32_t version;
    uint32_t count;
    struct srv_mount_entry entries[SRV_MOUNT_SNAPSHOT_MAX];
};

#endif
