#include <srvros/block.h>
#include <srvros/console.h>

#include <stddef.h>

#define BLOCK_CACHE_ENTRIES 32
#define BLOCK_CACHE_MAX_BLOCK_SIZE 4096

struct memory_block_device {
    const uint8_t *data;
    uint64_t size;
};

struct block_cache_entry {
    bool valid;
    struct block_device *device;
    uint64_t block_index;
    uint64_t last_used;
    uint8_t data[BLOCK_CACHE_MAX_BLOCK_SIZE];
};

static struct block_device devices[BLOCK_MAX_DEVICES];
static struct memory_block_device memory_devices[BLOCK_MAX_DEVICES];
static struct block_cache_entry cache[BLOCK_CACHE_ENTRIES];
static uint64_t device_count;
static uint64_t cache_clock;
static uint64_t cache_hits;
static uint64_t cache_misses;
static uint64_t cache_write_throughs;

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

static void copy_memory(void *dst, const void *src, uint64_t length) {
    uint8_t *out = dst;
    const uint8_t *in = src;
    for (uint64_t i = 0; i < length; i++) {
        out[i] = in[i];
    }
}

static bool memory_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t length) {
    if (device == 0 || buffer == 0) {
        return false;
    }

    struct memory_block_device *memory = device->private_data;
    if (memory == 0 || offset > memory->size || length > memory->size - offset) {
        return false;
    }

    uint8_t *dst = buffer;
    for (uint64_t i = 0; i < length; i++) {
        dst[i] = memory->data[offset + i];
    }
    return true;
}

void block_init(void) {
    device_count = 0;
    cache_clock = 0;
    cache_hits = 0;
    cache_misses = 0;
    cache_write_throughs = 0;
    for (uint64_t i = 0; i < BLOCK_MAX_DEVICES; i++) {
        devices[i].used = false;
    }
    for (uint64_t i = 0; i < BLOCK_CACHE_ENTRIES; i++) {
        cache[i].valid = false;
    }
    console_printf("block: registry ready slots=%u cache_entries=%u\n",
        (uint64_t)BLOCK_MAX_DEVICES,
        (uint64_t)BLOCK_CACHE_ENTRIES);
}

struct block_device *block_register_memory(const char *name,
    const uint8_t *data,
    uint64_t size,
    uint64_t block_size) {
    if (name == 0 || data == 0 || size == 0 || block_size == 0 || device_count >= BLOCK_MAX_DEVICES) {
        return 0;
    }

    uint64_t index = device_count;
    memory_devices[index] = (struct memory_block_device) {
        .data = data,
        .size = size,
    };
    struct block_device *device = block_register(name,
        block_size,
        (size + block_size - 1) / block_size,
        memory_read,
        NULL,
        &memory_devices[index]);
    if (device == 0) {
        return 0;
    }

    console_printf("block: registered %s size=%u block=%u\n",
        name,
        size,
        block_size);
    return device;
}

struct block_device *block_register(const char *name,
    uint64_t block_size,
    uint64_t block_count,
    block_read_fn read,
    block_write_fn write,
    void *private_data) {
    if (name == 0 ||
        block_size == 0 ||
        block_count == 0 ||
        read == 0 ||
        device_count >= BLOCK_MAX_DEVICES) {
        return 0;
    }

    uint64_t index = device_count++;
    devices[index] = (struct block_device) {
        .used = true,
        .name = name,
        .block_size = block_size,
        .block_count = block_count,
        .read = read,
        .write = write,
        .private_data = private_data,
    };
    return &devices[index];
}

struct block_device *block_find(const char *name) {
    if (name == 0) {
        return 0;
    }

    for (uint64_t i = 0; i < device_count; i++) {
        if (devices[i].used && streq(devices[i].name, name)) {
            return &devices[i];
        }
    }
    return 0;
}

static bool block_range_valid(struct block_device *device, uint64_t offset, uint64_t length) {
    uint64_t size = block_device_size(device);
    return offset <= size && length <= size - offset;
}

static bool cache_supported(struct block_device *device) {
    return device->block_size > 0 && device->block_size <= BLOCK_CACHE_MAX_BLOCK_SIZE;
}

static struct block_cache_entry *cache_lookup(struct block_device *device, uint64_t block_index) {
    for (uint64_t i = 0; i < BLOCK_CACHE_ENTRIES; i++) {
        if (cache[i].valid && cache[i].device == device && cache[i].block_index == block_index) {
            return &cache[i];
        }
    }
    return NULL;
}

static struct block_cache_entry *cache_pick_victim(void) {
    struct block_cache_entry *victim = &cache[0];
    for (uint64_t i = 0; i < BLOCK_CACHE_ENTRIES; i++) {
        if (!cache[i].valid) {
            return &cache[i];
        }
        if (cache[i].last_used < victim->last_used) {
            victim = &cache[i];
        }
    }
    return victim;
}

static struct block_cache_entry *cache_get(struct block_device *device, uint64_t block_index) {
    struct block_cache_entry *entry = cache_lookup(device, block_index);
    if (entry != NULL) {
        cache_hits++;
        entry->last_used = ++cache_clock;
        return entry;
    }

    cache_misses++;
    entry = cache_pick_victim();
    uint64_t offset = block_index * device->block_size;
    if (!device->read(device, offset, entry->data, device->block_size)) {
        entry->valid = false;
        return NULL;
    }

    entry->valid = true;
    entry->device = device;
    entry->block_index = block_index;
    entry->last_used = ++cache_clock;
    return entry;
}

bool block_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t length) {
    if (device == 0 || !device->used || device->read == 0 || buffer == 0) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (!block_range_valid(device, offset, length)) {
        return false;
    }
    if (!cache_supported(device)) {
        return device->read(device, offset, buffer, length);
    }

    uint8_t *out = buffer;
    uint64_t copied = 0;
    while (copied < length) {
        uint64_t absolute = offset + copied;
        uint64_t block_index = absolute / device->block_size;
        uint64_t block_offset = absolute % device->block_size;
        uint64_t chunk = device->block_size - block_offset;
        if (chunk > length - copied) {
            chunk = length - copied;
        }

        struct block_cache_entry *entry = cache_get(device, block_index);
        if (entry == NULL) {
            return false;
        }
        copy_memory(out + copied, entry->data + block_offset, chunk);
        copied += chunk;
    }

    return true;
}

bool block_write(struct block_device *device, uint64_t offset, const void *buffer, uint64_t length) {
    if (device == 0 || !device->used || device->write == 0 || buffer == 0) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (!block_range_valid(device, offset, length)) {
        return false;
    }
    if (!cache_supported(device)) {
        return device->write(device, offset, buffer, length);
    }

    const uint8_t *in = buffer;
    uint64_t copied = 0;
    while (copied < length) {
        uint64_t absolute = offset + copied;
        uint64_t block_index = absolute / device->block_size;
        uint64_t block_offset = absolute % device->block_size;
        uint64_t chunk = device->block_size - block_offset;
        if (chunk > length - copied) {
            chunk = length - copied;
        }

        struct block_cache_entry *entry = cache_get(device, block_index);
        if (entry == NULL) {
            return false;
        }
        copy_memory(entry->data + block_offset, in + copied, chunk);

        uint64_t block_start = block_index * device->block_size;
        if (!device->write(device, block_start, entry->data, device->block_size)) {
            return false;
        }
        cache_write_throughs++;
        copied += chunk;
    }

    return true;
}

uint64_t block_device_size(const struct block_device *device) {
    if (device == 0 || !device->used) {
        return 0;
    }
    return device->block_size * device->block_count;
}

const uint8_t *block_memory_data(struct block_device *device, uint64_t *size_out) {
    if (device == 0 || !device->used || device->read != memory_read) {
        return 0;
    }

    struct memory_block_device *memory = device->private_data;
    if (memory == 0) {
        return 0;
    }

    if (size_out != 0) {
        *size_out = memory->size;
    }
    return memory->data;
}

void block_print_status(void) {
    console_printf("block: devices=%u\n", device_count);
    for (uint64_t i = 0; i < device_count; i++) {
        if (devices[i].used) {
            char fs_name[9];
            for (uint64_t j = 0; j < sizeof(fs_name); j++) {
                fs_name[j] = '\0';
            }
            bool has_boot_name = block_read(&devices[i], 3, fs_name, 8);
            for (uint64_t j = 0; j < 8; j++) {
                if (fs_name[j] < 32 || fs_name[j] > 126) {
                    fs_name[j] = '.';
                }
            }

            console_printf("block: %s blocks=%u block_size=%u size=%u fs=%s write=%s\n",
                devices[i].name,
                devices[i].block_count,
                devices[i].block_size,
                block_device_size(&devices[i]),
                has_boot_name ? fs_name : "unreadable",
                devices[i].write != 0 ? "yes" : "no");
        }
    }
    console_printf("block: cache entries=%u block_max=%u hits=%u misses=%u writes=%u\n",
        (uint64_t)BLOCK_CACHE_ENTRIES,
        (uint64_t)BLOCK_CACHE_MAX_BLOCK_SIZE,
        cache_hits,
        cache_misses,
        cache_write_throughs);
}
