#ifndef SRVROS_EXFAT_H
#define SRVROS_EXFAT_H

#include <stdbool.h>
#include <stdint.h>

struct block_device;

/* Diagnostic observation only, while the caller owns filesystem I/O exclusion.
 * Labels describe the current synchronous operation, not permission to reenter. */
enum exfat_io_phase { EXFAT_IO_METADATA, EXFAT_IO_ALLOCATE, EXFAT_IO_DATA,
    EXFAT_IO_FREE, EXFAT_IO_PHASE_COUNT };
enum exfat_io_phase exfat_io_phase(const struct block_device *device);

struct exfat_check_result {
    uint64_t mounts_checked;
    uint64_t files_checked;
    uint64_t clusters_checked;
    uint64_t leaked_clusters;
    /* Referenced clusters missing from the allocation bitmap; free clusters
     * may legally retain old FAT values. */
    uint64_t fat_bitmap_mismatches;
    uint64_t errors;
};

struct exfat_fsinfo {
    uint64_t block_size;
    uint64_t blocks;
    uint64_t blocks_free;
    uint64_t blocks_available;
    uint64_t files;
    uint64_t files_free;
    char filesystem[16];
    char mountpoint[160];
};

bool exfat_mount_image(const char *mountpoint, const uint8_t *image, uint64_t image_size);
bool exfat_mount_block_device(const char *mountpoint, struct block_device *device);
bool exfat_unmount(const char *mountpoint);
bool exfat_is_writable(const char *mountpoint);
/* Diagnostic preparation only: verified, exclusive single-cluster 4096-byte
 * file. Caller must serialize against mutations and unmount before bypass I/O. */
bool exfat_scratch_range(const char *path, struct block_device **device, uint64_t *offset);
/* Diagnostic-only admission of an exact-sized, fully allocated contiguous file.
 * Caller holds filesystem exclusion and must unmount before using the range. */
bool exfat_exclusive_range(const char *path, uint64_t bytes,
    struct block_device **device, uint64_t *offset);
bool exfat_write_file(const char *path, const uint8_t *data, uint64_t size);
bool exfat_delete_file(const char *path);
bool exfat_create_directory(const char *path);
bool exfat_delete_directory(const char *path);
bool exfat_rename(const char *old_path, const char *new_path);
/* Read-only node-table admission, including metadata replacement and VFS slots.
 * Caller owns filesystem exclusion. This is a point-in-time check, not a lease. */
bool exfat_creation_capacity(const char *path, uint64_t files, uint64_t dirs);
bool exfat_statfs(const char *path, struct exfat_fsinfo *info);
bool exfat_check(const char *mountpoint, struct exfat_check_result *result);
void exfat_check_print(const char *mountpoint);
void exfat_print_mounts(void);

#endif
