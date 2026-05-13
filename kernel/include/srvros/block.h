#ifndef SRVROS_BLOCK_H
#define SRVROS_BLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define BLOCK_MAX_DEVICES 8

struct block_device;

typedef bool (*block_read_fn)(struct block_device *device,
    uint64_t offset,
    void *buffer,
    uint64_t length);
typedef bool (*block_write_fn)(struct block_device *device,
    uint64_t offset,
    const void *buffer,
    uint64_t length);

struct block_device {
    bool used;
    const char *name;
    uint64_t block_size;
    uint64_t block_count;
    block_read_fn read;
    block_write_fn write;
    void *private_data;
};

void block_init(void);
struct block_device *block_register_memory(const char *name,
    const uint8_t *data,
    uint64_t size,
    uint64_t block_size);
struct block_device *block_register(const char *name,
    uint64_t block_size,
    uint64_t block_count,
    block_read_fn read,
    block_write_fn write,
    void *private_data);
struct block_device *block_find(const char *name);
bool block_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t length);
bool block_write(struct block_device *device, uint64_t offset, const void *buffer, uint64_t length);
uint64_t block_device_size(const struct block_device *device);
const uint8_t *block_memory_data(struct block_device *device, uint64_t *size_out);
void block_print_status(void);

#endif
