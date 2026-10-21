#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>

#include <stddef.h>

#define BLOCK_CACHE_ENTRIES 32
#define BLOCK_CACHE_MAX_BLOCK_SIZE 4096
#define BLOCK_MAX_PARTITIONS 32

struct memory_block_device {
    const uint8_t *data;
    uint64_t size;
};

struct partition_block_device {
    struct block_device *parent;
    uint64_t offset;
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
static struct partition_block_device partition_devices[BLOCK_MAX_PARTITIONS];
static char partition_names[BLOCK_MAX_PARTITIONS][16];
static struct block_cache_entry cache[BLOCK_CACHE_ENTRIES];
static uint64_t device_count;
static uint64_t partition_count;
static uint64_t cache_clock;
static uint64_t cache_hits;
static uint64_t cache_misses;
static uint64_t cache_write_throughs;
static unsigned service_blocks;
static uint64_t last_service_us, service_windows;

static uint64_t irq_save(void) {
#ifdef SRVROS_BLOCK_HOST_TEST
    return block_test_irq_save();
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_BLOCK_HOST_TEST
    block_test_irq_restore(flags);
#else
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
}

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

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static uint64_t read_le64(const uint8_t *data) {
    return (uint64_t)read_le32(data) | ((uint64_t)read_le32(data + 4) << 32);
}

static bool guid_is_zero(const uint8_t *guid) {
    for (uint64_t i = 0; i < 16; i++) {
        if (guid[i] != 0) {
            return false;
        }
    }
    return true;
}

static void make_partition_name(char *out, uint64_t out_size, const char *base, uint64_t number) {
    uint64_t pos = 0;
    while (base[pos] != '\0' && pos + 1 < out_size) {
        out[pos] = base[pos];
        pos++;
    }
    if (pos + 1 < out_size) {
        out[pos++] = 'p';
    }

    char digits[20];
    uint64_t digit_count = 0;
    do {
        digits[digit_count++] = (char)('0' + (number % 10));
        number /= 10;
    } while (number != 0 && digit_count < sizeof(digits));

    while (digit_count > 0 && pos + 1 < out_size) {
        out[pos++] = digits[--digit_count];
    }
    out[pos] = '\0';
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

static bool partition_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t length) {
    if (device == 0 || buffer == 0) {
        return false;
    }

    struct partition_block_device *partition = device->private_data;
    if (partition == 0 || partition->parent == 0 ||
        offset > partition->size || length > partition->size - offset) {
        return false;
    }

    return block_read(partition->parent, partition->offset + offset, buffer, length);
}

static bool partition_write(struct block_device *device, uint64_t offset, const void *buffer, uint64_t length) {
    if (device == 0 || buffer == 0) {
        return false;
    }

    struct partition_block_device *partition = device->private_data;
    if (partition == 0 || partition->parent == 0 || partition->parent->write == 0 ||
        offset > partition->size || length > partition->size - offset) {
        return false;
    }

    return block_write(partition->parent, partition->offset + offset, buffer, length);
}

void block_init(void) {
    device_count = 0;
    partition_count = 0;
    cache_clock = 0;
    cache_hits = 0;
    cache_misses = 0;
    cache_write_throughs = 0;
    service_blocks = 0;
    last_service_us = service_windows = 0;
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
    if (name == 0 || data == 0 || size == 0 || block_size == 0) {
        return 0;
    }
    if (device_count >= BLOCK_MAX_DEVICES) {
        console_printf("block: registry full, cannot register memory device %s\n", name);
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
        block_count > UINT64_MAX / block_size ||
        read == 0) {
        return 0;
    }
    if (device_count >= BLOCK_MAX_DEVICES) {
        console_printf("block: registry full, cannot register %s\n", name);
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

struct block_device *block_device_at(uint64_t index) {
    return index < device_count ? &devices[index] : NULL;
}

struct block_device *block_parent(struct block_device *device) {
    if (device && device->read == partition_read) {
        struct partition_block_device *partition = device->private_data;
        return partition->parent;
    }
    return device;
}

bool block_raw_range(struct block_device *device,uint64_t offset,uint64_t length,
    struct block_device **parent,uint64_t *raw_offset) {
    if(!device||!device->used||!parent||!raw_offset||!length||
       !device->block_size||device->block_count>UINT64_MAX/device->block_size)return false;
    uint64_t size=block_device_size(device);
    if(offset>size||length>size-offset)return false;
    struct block_device *raw=block_parent(device);
    if(device!=raw) {
        struct partition_block_device *partition=device->private_data;
        if(!partition||offset>partition->size||length>partition->size-offset||
           partition->offset>UINT64_MAX-offset)return false;
        offset+=partition->offset;
    }
    if(!raw||!raw->used||!raw->block_size||raw->block_count>UINT64_MAX/raw->block_size||
       offset>block_device_size(raw)||length>block_device_size(raw)-offset)return false;
    *parent=raw;*raw_offset=offset;return true;
}
void block_forget(struct block_device *device) {
    if(!device)return;
    uint64_t flags=irq_save();
    struct block_device *parent=block_parent(device);
    for(unsigned i=0;i<BLOCK_CACHE_ENTRIES;i++)
        if(cache[i].valid&&block_parent(cache[i].device)==parent)cache[i].valid=false;
    irq_restore(flags);
}

bool block_enable_partition_writes(struct block_device *device) {
    struct block_device *parent = block_parent(device);
    if (!parent || !parent->write) return false;
    if (device != parent) device->write = partition_write;
    return true;
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

uint64_t block_register_gpt_partitions(struct block_device *device) {
    if (device == 0 || !device->used || device->block_size < 512) {
        return 0;
    }

    uint8_t header[512];
    if (!block_read(device, device->block_size, header, sizeof(header)) ||
        header[0] != 'E' || header[1] != 'F' || header[2] != 'I' || header[3] != ' ' ||
        header[4] != 'P' || header[5] != 'A' || header[6] != 'R' || header[7] != 'T') {
        return 0;
    }

    uint64_t entries_lba = read_le64(header + 72);
    uint32_t entry_count = read_le32(header + 80);
    uint32_t entry_size = read_le32(header + 84);
    if (entries_lba == 0 || entry_size < 128 || entry_size > 512) {
        return 0;
    }

    uint64_t registered = 0;
    uint8_t entry[512];
    for (uint32_t i = 0; i < entry_count && partition_count < BLOCK_MAX_PARTITIONS; i++) {
        uint64_t entry_offset = entries_lba * device->block_size + (uint64_t)i * entry_size;
        if (!block_read(device, entry_offset, entry, entry_size)) {
            break;
        }
        if (guid_is_zero(entry)) {
            continue;
        }

        uint64_t first_lba = read_le64(entry + 32);
        uint64_t last_lba = read_le64(entry + 40);
        if (last_lba < first_lba) {
            continue;
        }

        uint64_t start = first_lba * device->block_size;
        uint64_t size = (last_lba - first_lba + 1) * device->block_size;
        if (start >= block_device_size(device) || size > block_device_size(device) - start) {
            continue;
        }

        uint64_t slot = partition_count++;
        partition_devices[slot] = (struct partition_block_device) {
            .parent = device,
            .offset = start,
            .size = size,
        };
        make_partition_name(partition_names[slot], sizeof(partition_names[slot]), device->name, i + 1);
        struct block_device *partition = block_register(partition_names[slot],
            device->block_size,
            size / device->block_size,
            partition_read,
            device->write != 0 ? partition_write : NULL,
            &partition_devices[slot]);
        if (partition == 0) {
            partition_count--;
            console_printf("block: partition registration stopped at %s entry=%u\n",
                device->name,
                (uint64_t)i + 1);
            break;
        }
        console_printf("block: registered partition %s offset=%u size=%u\n",
            partition->name,
            start,
            size);
        registered++;
    }
    if (partition_count >= BLOCK_MAX_PARTITIONS) {
        console_printf("block: partition registry full slots=%u\n", (uint64_t)BLOCK_MAX_PARTITIONS);
    }

    return registered;
}

/* Primary MBR partitions only. Reject ambiguous/overlapping maps as a unit;
 * protective GPT, extended containers and hybrid maps are not writable aliases. */
uint64_t block_register_mbr_partitions(struct block_device *device) {
    if (!device || !device->used || device->block_size < 512 || device->read == partition_read) return 0;
    uint8_t mbr[512];
    if (!block_read(device,0,mbr,512) || mbr[510]!=0x55 || mbr[511]!=0xaa) return 0;
    uint64_t first[4]={0}, count[4]={0};
    for (unsigned i=0;i<4;i++) {
        const uint8_t *p=mbr+446+i*16;
        if(p[4]==0xee) return 0; /* Do not expose a protective/hybrid MBR. */
        if(!p[4]) continue;
        if(p[0]!=0 && p[0]!=0x80) return 0;
        first[i]=read_le32(p+8); count[i]=read_le32(p+12);
        if(!first[i] || !count[i] || first[i]>=device->block_count || count[i]>device->block_count-first[i]) return 0;
        for(unsigned j=0;j<i;j++)
            if(count[j] && first[i]<first[j]+count[j] && first[j]<first[i]+count[i]) return 0;
    }
    uint64_t added=0;
    for(unsigned i=0;i<4 && partition_count<BLOCK_MAX_PARTITIONS;i++) {
        uint8_t type=mbr[446+i*16+4];
        if(!count[i] || type==5 || type==0xf || type==0x85) continue;
        uint64_t slot=partition_count;
        make_partition_name(partition_names[slot],sizeof(partition_names[slot]),device->name,i+1);
        if(block_find(partition_names[slot])) continue;
        partition_devices[slot]=(struct partition_block_device){.parent=device,
            .offset=first[i]*device->block_size,.size=count[i]*device->block_size};
        struct block_device *p=block_register(partition_names[slot],device->block_size,count[i],
            partition_read,device->write?partition_write:NULL,&partition_devices[slot]);
        if(!p) break;
        partition_count++; added++;
        console_printf("block: registered MBR partition %s offset=%u size=%u\n",p->name,
            partition_devices[slot].offset,partition_devices[slot].size);
    }
    return added;
}

static bool block_range_valid(struct block_device *device, uint64_t offset, uint64_t length) {
    uint64_t size = block_device_size(device);
    return offset <= size && length <= size - offset;
}

static bool cache_supported(struct block_device *device) {
    /* Partition aliases use the parent cache so either name sees each write. */
    return device->read != partition_read && device->block_size > 0 &&
        device->block_size <= BLOCK_CACHE_MAX_BLOCK_SIZE;
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

/* RAM-backed devices and cache hits never reach the AHCI wait loop. Admit
 * the same restricted services between completed blocks, with ownership still
 * held. Check the reference clock every 32 blocks to bound polling overhead. */
static void service_checkpoint(void) {
    if (++service_blocks < 32) return;
    uint64_t now = timer_monotonic_us();
    if (timer_reference_ready()) {
        service_blocks = 0;
        if (now - last_service_us < 1000) return;
    } else if (service_blocks < 256) {
        return;
    }
    service_blocks = 0;
    if (scheduler_io_yield()) {
        service_windows++;
        last_service_us = timer_monotonic_us();
    }
}

static uint64_t batch_bytes(struct block_device *device, uint64_t remaining) {
    uint64_t limit=device->max_transfer_bytes;
    if (limit>BLOCK_MAX_BATCH_BYTES) limit=BLOCK_MAX_BATCH_BYTES;
    if (limit>remaining) limit=remaining;
    return limit-limit%device->block_size;
}

/* Publish only completed I/O. Ownership excludes other cache users even when
 * the driver yields to keyboard/console services while DMA is in flight. */
static void cache_publish(struct block_device *device, uint64_t first,
    const uint8_t *data, uint64_t bytes) {
    for (uint64_t done=0; done<bytes; done+=device->block_size, first++) {
        struct block_cache_entry *entry=cache_lookup(device,first);
        if (!entry) entry=cache_pick_victim();
        copy_memory(entry->data,data+done,device->block_size);
        entry->device=device; entry->block_index=first;
        entry->last_used=++cache_clock; entry->valid=true;
        service_checkpoint();
    }
}

static void cache_invalidate_range(struct block_device *device, uint64_t first, uint64_t count) {
    for (unsigned i=0;i<BLOCK_CACHE_ENTRIES;i++) {
        if (cache[i].valid && cache[i].device==device &&
            cache[i].block_index>=first && cache[i].block_index-first<count) cache[i].valid=false;
    }
}

static bool read_locked(struct block_device *device, uint64_t offset, void *buffer, uint64_t length) {
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
        uint64_t batch = block_offset ? 0 : batch_bytes(device,length-copied);
        if (batch && !cache_lookup(device,block_index)) {
            /* Stop before a cached sector; never replace a hit with disk I/O. */
            uint64_t bytes=device->block_size;
            while (bytes<batch && !cache_lookup(device,block_index+bytes/device->block_size))
                bytes+=device->block_size;
            cache_misses+=bytes/device->block_size;
            if (!device->read(device,absolute,out+copied,bytes)) return false;
            cache_publish(device,block_index,out+copied,bytes);
            copied+=bytes;
            continue;
        }
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
        service_checkpoint();
    }

    return true;
}

static bool write_locked(struct block_device *device, uint64_t offset, const void *buffer, uint64_t length) {
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
        uint64_t batch = block_offset ? 0 : batch_bytes(device,length-copied);
        if (batch) {
            /* Full sectors need no read-before-write. A failed multi-sector
             * write may have changed any prefix: invalidate the entire range. */
            cache_invalidate_range(device,block_index,batch/device->block_size);
            if (!device->write(device,absolute,in+copied,batch)) return false;
            cache_write_throughs+=batch/device->block_size;
            cache_publish(device,block_index,in+copied,batch);
            copied+=batch;
            continue;
        }
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
            /* The write may have partially reached media. Reload on next read. */
            entry->valid = false;
            return false;
        }
        cache_write_throughs++;
        copied += chunk;
        service_checkpoint();
    }

    return true;
}

/* A cache victim remains private until its data, tag and caller copy are all
 * complete. Driver-level ownership alone leaves a preemption gap at return.
 * Nested partition/driver calls inherit this scope. Drivers may still admit
 * keyboard/console service threads, which must not enter the block cache. */
bool block_read(struct block_device *device, uint64_t offset, void *buffer, uint64_t length) {
    uint64_t flags = irq_save();
    bool services = scheduler_io_begin();
    bool ok = false;
    if (services || !scheduler_io_active()) ok = read_locked(device, offset, buffer, length);
    if (services) scheduler_io_end();
    irq_restore(flags);
    return ok;
}

bool block_write(struct block_device *device, uint64_t offset, const void *buffer, uint64_t length) {
    uint64_t flags = irq_save();
    bool services = scheduler_io_begin();
    bool ok = false;
    if (services || !scheduler_io_active()) ok = write_locked(device, offset, buffer, length);
    if (services) scheduler_io_end();
    irq_restore(flags);
    return ok;
}

bool block_flush_all(void) {
    bool success = true;
    for (uint64_t i = 0; i < device_count; i++) {
        struct block_device *device = &devices[i];
        if (!device->used || device->write == NULL || device->read == partition_read) continue;
        if (device->flush == NULL || !device->flush(device)) success = false;
    }
    return success;
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
    console_printf("block: cache entries=%u block_max=%u hits=%u misses=%u writes=%u service_windows=%u\n",
        (uint64_t)BLOCK_CACHE_ENTRIES,
        (uint64_t)BLOCK_CACHE_MAX_BLOCK_SIZE,
        cache_hits,
        cache_misses,
        cache_write_throughs,
        service_windows);
}
