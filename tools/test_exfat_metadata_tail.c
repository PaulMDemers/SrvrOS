#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST
#include "../kernel/src/fs/exfat.c"

static uint8_t disk[4 * 1024 * 1024], payload[3 * 131072];
static uint64_t written, allocations;
static unsigned write_calls, fail_write;
static bool fail_allocation;

uint64_t timer_monotonic_us(void) { return 0; }

void *kmalloc(uint64_t size) {
    if (fail_allocation) return NULL;
    void *result = malloc(size);
    if (result) allocations++;
    return result;
}
void *kcalloc(uint64_t count, uint64_t size) {
    assert(!size || count <= SIZE_MAX / size);
    void *result = kmalloc(count * size);
    if (result) memset(result, 0, count * size);
    return result;
}
void kfree(void *pointer) {
    if (pointer) { assert(allocations); allocations--; free(pointer); }
}
struct block_device *block_parent(struct block_device *device) { return device; }
bool block_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t size) {
    (void)device;
    assert(offset <= sizeof(disk) && size <= sizeof(disk) - offset);
    memcpy(buffer, disk + offset, size);
    return true;
}
bool block_write(struct block_device *device, uint64_t offset, const void *buffer, uint64_t size) {
    (void)device;
    assert(offset <= sizeof(disk) && size <= sizeof(disk) - offset);
    written += size;
    if (++write_calls == fail_write) return false;
    memcpy(disk + offset, buffer, size);
    return true;
}

static void check(uint64_t cluster, uint64_t fs_sector, uint64_t block,
    uint64_t size, bool metadata, bool contiguous, bool grouped) {
    memset(disk, 0xa5, sizeof(disk));
    written = write_calls = 0;
    struct block_device device = {.block_size = block,
        .max_transfer_bytes = grouped ? BLOCK_MAX_BATCH_BYTES : 0};
    struct exfat_volume volume = {.device = &device, .sector_size = fs_sector,
        .image_size = sizeof(disk), .cluster_size = cluster, .cluster_count = 16,
        .cluster_heap_offset_bytes = 131072, .fat_offset_bytes = 4096};
    uint64_t clusters = (size + cluster - 1) / cluster;
    struct exfat_file file = {.volume = &volume, .first_cluster = size ? 2 : 0,
        .size = size, .valid_size = size, .allocated_clusters = clusters,
        .no_fat_chain = contiguous};
    for (uint64_t i = 0; i < clusters; i++) {
        uint32_t current = (uint32_t)(2 + (contiguous ? i : 2 * i));
        uint32_t next = i + 1 == clusters ? 0xffffffffu : current + (contiguous ? 1 : 2);
        write_u32(disk + volume.fat_offset_bytes + current * 4, next);
    }
    assert(write_file_data_impl(&file, payload, size, metadata));
    uint64_t alignment = fs_sector > block ? fs_sector : block;
    bool trim = metadata && alignment && alignment <= cluster && !(cluster % alignment);
    uint64_t expected = trim ? size + (alignment - size % alignment) % alignment : clusters * cluster;
    assert(written == expected && !allocations);
    for (uint64_t i = 0; i < clusters; i++) {
        uint64_t base = volume.cluster_heap_offset_bytes + (contiguous ? i : 2 * i) * cluster;
        for (uint64_t byte = 0; byte < cluster; byte++) {
            uint64_t logical = i * cluster + byte;
            uint8_t want = logical < size ? payload[logical] : logical < expected ? 0 : 0xa5;
            assert(disk[base + byte] == want);
        }
    }
}

int main(void) {
    for (unsigned i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)(i * 37 + 11);
    for (unsigned geometry = 0; geometry < 2; geometry++) {
        uint64_t cluster = geometry ? 131072 : 4096;
        uint64_t sizes[] = {0, 1, 511, 512, 513, 4095, 4096, 4097,
            cluster - 1, cluster, cluster + 1, 2 * cluster + 513};
        for (unsigned sector = 0; sector < 2; sector++)
            for (unsigned block = 0; block < 2; block++)
                for (unsigned metadata = 0; metadata < 2; metadata++)
                    for (unsigned contiguous = 0; contiguous < 2; contiguous++)
                        for (unsigned grouped = 0; grouped < 2; grouped++)
                            for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
                                check(cluster, sector ? 4096 : 512, block ? 4096 : 512,
                                    sizes[i], metadata, contiguous, grouped);
    }
    check(4096, 512, 3000, 513, true, true, false);
    fail_allocation = true;
    struct block_device device = {.block_size = 512};
    struct exfat_volume volume = {.device = &device, .sector_size = 512,
        .image_size = sizeof(disk), .cluster_size = 4096,
        .cluster_count = 16, .cluster_heap_offset_bytes = 131072};
    struct exfat_file file = {.volume = &volume, .first_cluster = 2,
        .size = 513, .valid_size = 513, .allocated_clusters = 1, .no_fat_chain = true};
    written = write_calls = 0;
    assert(!write_file_data_impl(&file, payload, 513, true) && !write_calls && !allocations);
    fail_allocation = false;
    fail_write = 1;
    assert(!write_file_data_impl(&file, payload, 513, true) && volume.write_blocked && !allocations);
    puts("metadata-tail: PASS exact payload/final-sector zeros, poisoned allocation slack, "
         "ordinary full initialization, 512/4096-byte alignment, FAT/contiguous/grouped writes, "
         "zero/edge lengths and allocation/write failures");
}
