#ifndef SRVROS_EXFAT_H
#define SRVROS_EXFAT_H

#include <stdbool.h>
#include <stdint.h>

struct block_device;

struct exfat_check_result {
    uint64_t mounts_checked;
    uint64_t files_checked;
    uint64_t clusters_checked;
    uint64_t leaked_clusters;
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
bool exfat_write_file(const char *path, const uint8_t *data, uint64_t size);
bool exfat_delete_file(const char *path);
bool exfat_create_directory(const char *path);
bool exfat_delete_directory(const char *path);
bool exfat_rename(const char *old_path, const char *new_path);
bool exfat_statfs(const char *path, struct exfat_fsinfo *info);
bool exfat_check(const char *mountpoint, struct exfat_check_result *result);
void exfat_check_print(const char *mountpoint);
void exfat_print_mounts(void);

#endif
