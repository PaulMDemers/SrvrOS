#ifndef SRVROS_INITRAMFS_H
#define SRVROS_INITRAMFS_H

#include <stddef.h>
#include <stdint.h>

struct initramfs_file {
    const char *path;
    const uint8_t *data;
    uint64_t size;
};

void initramfs_init(const void *address, uint64_t size);
uint64_t initramfs_file_count(void);
const struct initramfs_file *initramfs_file_at(uint64_t index);
const struct initramfs_file *initramfs_find(const char *path);

#endif

