#include <srvros/path_limits.h>
#include <srvros/console.h>
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/vfs.h>
#include <srvros/scheduler.h>
#include <srvros/process.h>
#include <srvros/timer.h>

#include <stddef.h>
#include <stdint.h>

#define EXFAT_MAX_VOLUMES 4
#define EXFAT_MAX_FILES 8192
#define EXFAT_MAX_DIRS 2048
#define EXFAT_MAX_PATH SRV_PATH_MAX
#define EXFAT_MAX_FILE_ENTRIES 19
/* Every mounted node can require a path, eight full-width decimal fields,
 * separators and a newline. Keep serialization capacity tied to node limits. */
#define EXFAT_METADATA_MAX_BYTES ((EXFAT_MAX_FILES + EXFAT_MAX_DIRS) * (EXFAT_MAX_PATH + 8 * 21 + 1) + 16)
#define EXFAT_ENTRY_SIZE 32
#define EXFAT_ENTRY_END 0x00
#define EXFAT_ENTRY_BITMAP 0x81
#define EXFAT_ENTRY_UPCASE 0x82
#define EXFAT_ENTRY_FILE 0x85
#define EXFAT_ENTRY_STREAM 0xc0
#define EXFAT_ENTRY_NAME 0xc1
#define EXFAT_ATTR_DIRECTORY 0x0010
#define EXFAT_FAT_CHAIN_END 0xfffffff8u

struct exfat_boot_sector {
    uint8_t jump_boot[3];
    char filesystem_name[8];
    uint8_t must_be_zero[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_directory_cluster;
    uint32_t volume_serial_number;
    uint16_t filesystem_revision;
    uint16_t volume_flags;
    uint8_t bytes_per_sector_shift;
    uint8_t sectors_per_cluster_shift;
    uint8_t number_of_fats;
    uint8_t drive_select;
    uint8_t percent_in_use;
    uint8_t reserved[7];
    uint8_t boot_code[390];
    uint16_t boot_signature;
} __attribute__((packed));

struct exfat_file_entry {
    uint8_t entry_type;
    uint8_t secondary_count;
    uint16_t set_checksum;
    uint16_t file_attributes;
    uint8_t reserved[26];
} __attribute__((packed));

struct exfat_stream_entry {
    uint8_t entry_type;
    uint8_t flags;
    uint8_t reserved0;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved1;
    uint64_t valid_data_length;
    uint32_t reserved2;
    uint32_t first_cluster;
    uint64_t data_length;
} __attribute__((packed));

struct exfat_name_entry {
    uint8_t entry_type;
    uint8_t flags;
    uint16_t name[15];
} __attribute__((packed));

/* Bounded observation only. No paths, delayed I/O or dynamic allocation. */
struct exfat_cost_phase { uint64_t reads,read_bytes,read_us,writes,write_bytes,write_us,flushes,flush_us; };
struct exfat_cost { uint64_t start,end;const char *operation;bool active,ok;struct exfat_cost_phase phase[EXFAT_IO_PHASE_COUNT]; };
#define EXFAT_COST_ROWS 4
static struct exfat_cost slow_costs[EXFAT_COST_ROWS];
static uint64_t cost_clock(void) {
#ifdef SRVROS_EXFAT_HOST_TEST
#ifdef SRVROS_EXFAT_COST_TEST
    return timer_monotonic_us();
#else
    return 0; /* Ordinary host fixtures do not emulate a kernel clock. */
#endif
#else
    return timer_monotonic_us();
#endif
}
static uint64_t cost_elapsed(uint64_t start) { uint64_t end=cost_clock();return start && end>=start?end-start:0; }
static void cost_begin(struct exfat_cost *c,const char *operation) {
    *c=(struct exfat_cost){.start=cost_clock(),.operation=operation,.active=true};
}
static void cost_end(struct exfat_cost *c,bool ok) {
    c->end=cost_clock();c->ok=ok;c->active=false;
    if(!c->start || c->end<c->start || c->end-c->start<100000)return;
    unsigned at=0;
    for(unsigned i=1;i<EXFAT_COST_ROWS;i++)
        if(slow_costs[i].end-slow_costs[i].start<slow_costs[at].end-slow_costs[at].start)at=i;
    if(c->end-c->start>slow_costs[at].end-slow_costs[at].start)slow_costs[at]=*c;
}

struct exfat_volume {
    struct exfat_cost cost;
    enum exfat_io_phase io_phase;
    const uint8_t *image;
    struct block_device *device;
    uint64_t image_size;
    uint64_t sector_size;
    uint64_t sectors_per_cluster;
    uint64_t cluster_size;
    uint64_t fat_offset_bytes;
    uint64_t cluster_heap_offset_bytes;
    uint32_t allocation_bitmap_cluster;
    uint64_t allocation_bitmap_size;
    uint32_t upcase_table_cluster;
    uint64_t upcase_table_size;
    uint32_t upcase_table_checksum;
    bool bitmap_seen, upcase_seen;
    uint32_t cluster_count;
    uint32_t root_directory_cluster;
    uint16_t flags;
    bool write_blocked;
    bool mutation_written;
    unsigned mutation_depth;
};

struct exfat_file {
    struct exfat_volume *volume;
    uint32_t first_cluster;
    uint64_t size;
    uint64_t valid_size;
    uint64_t allocated_clusters;
    uint64_t file_entry_offset;
    uint64_t stream_entry_offset;
    uint8_t secondary_count;
    bool no_fat_chain;
};

struct exfat_directory {
    char path[EXFAT_MAX_PATH];
    uint32_t first_cluster;
    uint64_t data_length;
    uint64_t allocated_clusters;
    uint64_t file_entry_offset;
    uint8_t secondary_count;
    bool no_fat_chain;
};

#define EXFAT_NODE_BLOCK 64
struct exfat_file_slot { struct exfat_file file; char path[EXFAT_MAX_PATH]; };
struct exfat_mount {
    bool used;
    uint64_t block_claim;
    struct exfat_volume volume;
    struct exfat_file_slot *file_blocks[EXFAT_MAX_FILES/EXFAT_NODE_BLOCK];
    struct exfat_directory *dir_blocks[EXFAT_MAX_DIRS/EXFAT_NODE_BLOCK];
    uint64_t file_capacity, dir_capacity;
    char *metadata_buffer;
    uint64_t metadata_capacity;
    char mountpoint[EXFAT_MAX_PATH];
    uint64_t file_count;
    uint64_t dir_count;
};

static struct exfat_mount mounts[EXFAT_MAX_VOLUMES];
enum exfat_io_phase exfat_io_phase(const struct block_device *device) {
    if (device) for (unsigned i=0;i<EXFAT_MAX_VOLUMES;i++)
        if (mounts[i].used && mounts[i].volume.device==device)
            return mounts[i].volume.io_phase;
    return EXFAT_IO_METADATA;
}

static uint64_t fs_irq_save(void) {
#ifdef SRVROS_EXFAT_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}
static void fs_irq_restore(uint64_t flags) {
#ifndef SRVROS_EXFAT_HOST_TEST
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#else
    (void)flags;
#endif
}
/* Hold storage ownership through publication/retirement, not only individual
 * block transfers. The count also rejects same-owner callback teardown. */
struct fs_scope { uint64_t irq;bool services; };
static uint64_t fs_operations;
static bool fs_enter(struct fs_scope *scope) {
    scope->irq=fs_irq_save();scope->services=scheduler_io_begin();
    if((!scope->services && scheduler_io_active()) || fs_operations==UINT64_MAX) {
        if(scope->services)scheduler_io_end();
        fs_irq_restore(scope->irq);return false;
    }
    fs_operations++;return true;
}
static void fs_leave(struct fs_scope *scope) {
    fs_operations--;
    if(scope->services)scheduler_io_end();
    fs_irq_restore(scope->irq);
}

static bool metadata_syncing;
/* Per-mount blocks never move: VFS paths/private_data can outlive a growth.
 * Allocated blocks and metadata scratch are reclaimed together at unmount. */
static struct exfat_file *file_slot(const struct exfat_mount *m,uint64_t i) {
    return &m->file_blocks[i/EXFAT_NODE_BLOCK][i%EXFAT_NODE_BLOCK].file;
}
static char *path_slot(const struct exfat_mount *m,uint64_t i) {
    return m->file_blocks[i/EXFAT_NODE_BLOCK][i%EXFAT_NODE_BLOCK].path;
}
static struct exfat_directory *dir_slot(const struct exfat_mount *m,uint64_t i) {
    return &m->dir_blocks[i/EXFAT_NODE_BLOCK][i%EXFAT_NODE_BLOCK];
}
static bool reserve_tables(struct exfat_mount *m,uint64_t files,uint64_t dirs) {
    if(files>EXFAT_MAX_FILES || dirs>EXFAT_MAX_DIRS) return false;
    while(m->file_capacity<files) {
        struct exfat_file_slot *block=kcalloc(EXFAT_NODE_BLOCK,sizeof(*block));
        if(!block) return false;
        m->file_blocks[m->file_capacity/EXFAT_NODE_BLOCK]=block;
        m->file_capacity+=EXFAT_NODE_BLOCK;
    }
    while(m->dir_capacity<dirs) {
        struct exfat_directory *block=kcalloc(EXFAT_NODE_BLOCK,sizeof(*block));
        if(!block) return false;
        m->dir_blocks[m->dir_capacity/EXFAT_NODE_BLOCK]=block;
        m->dir_capacity+=EXFAT_NODE_BLOCK;
    }
    return true;
}
static uint64_t used_files(const struct exfat_mount *m) {
    uint64_t n=0;for(uint64_t i=0;i<m->file_count;i++) n+=path_slot(m,i)[0]!=0;
    return n;
}
static uint64_t used_dirs(const struct exfat_mount *m) {
    uint64_t n=0;for(uint64_t i=0;i<m->dir_count;i++) n+=dir_slot(m,i)->path[0]!=0;
    return n;
}
static bool reserve_metadata(struct exfat_mount *m,uint64_t nodes) {
    uint64_t needed=nodes*(EXFAT_MAX_PATH+8*21+1)+16;
    if(needed>EXFAT_METADATA_MAX_BYTES) return false;
    if(needed<=m->metadata_capacity) return true;
    uint64_t capacity=m->metadata_capacity?m->metadata_capacity:4096;
    while(capacity<needed) {
        capacity*=2;
        if(capacity>EXFAT_METADATA_MAX_BYTES) capacity=EXFAT_METADATA_MAX_BYTES;
    }
    char *buffer=kmalloc(capacity);if(!buffer) return false;
    kfree(m->metadata_buffer);m->metadata_buffer=buffer;m->metadata_capacity=capacity;
    return true;
}

static uint16_t entry_set_checksum(const uint8_t *entries, uint64_t entry_count);
static bool streq(const char *a, const char *b);
static bool name_conflicts(const char *path, const char *ignore);
static bool allocate_contiguous_clusters(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster);
static bool allocate_clusters(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster, bool *no_fat_chain);
static bool free_file_clusters(struct exfat_file *file);

static void copy_cstr(char *dst, uint64_t capacity, const char *src) {
    uint64_t i = 0;
    if (dst == NULL || capacity == 0) {
        return;
    }
    while (src != NULL && src[i] != '\0' && i + 1 < capacity) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool streqn(const char *a, const char *b, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static uint16_t read_u16(const void *data) {
    const uint8_t *bytes = data;
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_u32(const void *data) {
    const uint8_t *bytes = data;
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

static uint64_t read_u64(const void *data) {
    const uint8_t *bytes = data;
    return (uint64_t)bytes[0] |
        ((uint64_t)bytes[1] << 8) |
        ((uint64_t)bytes[2] << 16) |
        ((uint64_t)bytes[3] << 24) |
        ((uint64_t)bytes[4] << 32) |
        ((uint64_t)bytes[5] << 40) |
        ((uint64_t)bytes[6] << 48) |
        ((uint64_t)bytes[7] << 56);
}

static void write_u16(void *data, uint16_t value) {
    uint8_t *bytes = data;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_u32(void *data, uint32_t value) {
    uint8_t *bytes = data;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_u64(void *data, uint64_t value) {
    uint8_t *bytes = data;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
    bytes[4] = (uint8_t)(value >> 32);
    bytes[5] = (uint8_t)(value >> 40);
    bytes[6] = (uint8_t)(value >> 48);
    bytes[7] = (uint8_t)(value >> 56);
}

static void copy_memory(void *dst, const void *src, uint64_t length) {
    uint8_t *out = dst;
    const uint8_t *in = src;
    for (uint64_t i = 0; i < length; i++) {
        out[i] = in[i];
    }
}

/* Only the serialized outer mutation owns these read-through copies. No
 * delayed writes: overlap invalidation precedes even a failed media write.
 * Exact/contained reads avoid repeated directory/bitmap transfers; requests
 * larger than a target cluster bypass the bounded cache. */
#define EXFAT_READ_CACHE_ROWS 4
#define EXFAT_READ_CACHE_BYTES (128u*1024u)
static struct {
    const struct exfat_volume *volume;
    unsigned next;
    uint64_t epoch;
    struct {uint64_t offset,length;uint8_t bytes[EXFAT_READ_CACHE_BYTES];} rows[EXFAT_READ_CACHE_ROWS];
} mutation_reads;
static void mutation_reads_advance(void) {
    if(mutation_reads.epoch!=UINT64_MAX)mutation_reads.epoch++;
    if(mutation_reads.epoch==UINT64_MAX)mutation_reads.volume=NULL; /* Never ABA. */
}
static void mutation_reads_reset(const struct exfat_volume *volume) {
    mutation_reads_advance();
    mutation_reads.volume=mutation_reads.epoch==UINT64_MAX?NULL:volume;mutation_reads.next=0;
    for(unsigned i=0;i<EXFAT_READ_CACHE_ROWS;i++)mutation_reads.rows[i].length=0;
}
static void mutation_reads_invalidate(const struct exfat_volume *volume,uint64_t offset,uint64_t length) {
    if(mutation_reads.volume!=volume || !length)return;
    mutation_reads_advance();
    for(unsigned i=0;i<EXFAT_READ_CACHE_ROWS;i++) {
        uint64_t base=mutation_reads.rows[i].offset,size=mutation_reads.rows[i].length;
        /* Subtraction comparisons do not overflow on an invalid write range. */
        if(size && (offset>=base?offset-base<size:base-offset<length))mutation_reads.rows[i].length=0;
    }
}
static bool volume_read(const struct exfat_volume *volume, uint64_t offset, void *buffer, uint64_t length) {
    if (volume == NULL || buffer == NULL || offset > volume->image_size || length > volume->image_size - offset) {
        return false;
    }

    if (volume->image != NULL) {
        copy_memory(buffer, volume->image + offset, length);
        return true;
    }

    bool cached=mutation_reads.volume==volume && volume->mutation_depth &&
        length && length<=EXFAT_READ_CACHE_BYTES && !volume->write_blocked;
#if defined(SRVROS_EXFAT_HOST_TEST) && defined(SRVROS_EXFAT_READ_CACHE_OFF)
    cached=false; /* Disposable comparative host driver only. */
#endif
    if(cached)for(unsigned i=0;i<EXFAT_READ_CACHE_ROWS;i++) {
        uint64_t base=mutation_reads.rows[i].offset,size=mutation_reads.rows[i].length;
        if(offset>=base && offset-base<=size && length<=size-(offset-base)) {
            copy_memory(buffer,mutation_reads.rows[i].bytes+(offset-base),length);return true;
        }
    }
    uint64_t epoch=mutation_reads.epoch;
    uint64_t start=cost_clock();
    bool ok=block_read(volume->device, offset, buffer, length);
    if(volume->cost.active) {
        struct exfat_cost_phase *p=&((struct exfat_volume *)volume)->cost.phase[volume->io_phase];
        p->reads++;p->read_bytes+=length;p->read_us+=cost_elapsed(start);
    }
    if(ok && cached && mutation_reads.volume==volume && epoch==mutation_reads.epoch) {
        mutation_reads_advance();
        unsigned i=mutation_reads.next++%EXFAT_READ_CACHE_ROWS;
        mutation_reads.rows[i].offset=offset;mutation_reads.rows[i].length=length;
        copy_memory(mutation_reads.rows[i].bytes,buffer,length);
    }
    return ok;
}

static bool volume_write(struct exfat_volume *volume, uint64_t offset, const void *buffer, uint64_t length) {
    if (volume == NULL || buffer == NULL || offset > volume->image_size || length > volume->image_size - offset) {
        return false;
    }
    if (volume->image != NULL || volume->write_blocked) return false;
    uint64_t saved[EXFAT_READ_CACHE_ROWS];
    bool cached=mutation_reads.volume==volume && volume->mutation_depth;
    for(unsigned i=0;i<EXFAT_READ_CACHE_ROWS;i++)saved[i]=cached?mutation_reads.rows[i].length:0;
    mutation_reads_invalidate(volume,offset,length);
    uint64_t epoch=mutation_reads.epoch;
    volume->mutation_written = true;
    uint64_t start=cost_clock();
    bool ok = block_write(volume->device, offset, buffer, length);
    if(volume->cost.active) {
        struct exfat_cost_phase *p=&volume->cost.phase[volume->io_phase];
        p->writes++;p->write_bytes+=length;p->write_us+=cost_elapsed(start);
    }
    /* Restore only after complete success and no intervening cache/transaction
     * activity. A failed/partial or reentrant transfer leaves overlap invalid.
     * Successful partial directory writes patch the owned full-cluster copy. */
    if(ok && cached && mutation_reads.volume==volume && epoch!=UINT64_MAX && epoch==mutation_reads.epoch) {
        for(unsigned i=0;i<EXFAT_READ_CACHE_ROWS;i++)if(saved[i]) {
            uint64_t base=mutation_reads.rows[i].offset;
            uint64_t first=offset>base?offset:base,last=offset+length<base+saved[i]?offset+length:base+saved[i];
            if(last>first)copy_memory(mutation_reads.rows[i].bytes+(first-base),(const uint8_t *)buffer+(first-offset),last-first);
            mutation_reads.rows[i].length=saved[i];
        }
    }
    if (!ok) volume->write_blocked = true;
    return ok;
}

static bool volume_flush(struct exfat_volume *volume) {
    struct block_device *parent = block_parent(volume->device);
    uint64_t start=cost_clock();
    bool ok = parent && parent->flush && parent->flush(parent);
    if(volume->cost.active) {
        struct exfat_cost_phase *p=&volume->cost.phase[volume->io_phase];
        p->flushes++;p->flush_us+=cost_elapsed(start);
    }
    if (!ok) volume->write_blocked = true;
    return ok;
}

static bool cluster_offset(const struct exfat_volume *volume, uint32_t cluster, uint64_t *offset) {
    if (cluster < 2 || cluster >= volume->cluster_count + 2) {
        return false;
    }

    uint64_t result = volume->cluster_heap_offset_bytes +
        ((uint64_t)cluster - 2) * volume->cluster_size;
    if (result >= volume->image_size || volume->cluster_size > volume->image_size - result) {
        return false;
    }

    *offset = result;
    return true;
}

static bool next_cluster(const struct exfat_volume *volume, uint32_t cluster, uint32_t *next) {
    uint64_t offset = volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t);
    uint8_t bytes[sizeof(uint32_t)];
    if (!volume_read(volume, offset, bytes, sizeof(bytes))) {
        return false;
    }

    *next = read_u32(bytes);
    return true;
}

static bool write_fat_entry(struct exfat_volume *volume, uint32_t cluster, uint32_t value) {
    uint8_t bytes[sizeof(uint32_t)];
    write_u32(bytes, value);
    return volume_write(volume,
        volume->fat_offset_bytes + (uint64_t)cluster * sizeof(uint32_t),
        bytes,
        sizeof(bytes));
}

/* Keep the existing FAT values/ordering, but avoid one sector RMW for every
 * four-byte entry in a contiguous run. Only the requested bytes are passed to
 * block_write: its partial-sector handling preserves neighboring entries even
 * when the device has sectors larger than this bounded scratch buffer. */
static bool fill_fat_range(struct exfat_volume *volume, uint32_t first,
    uint64_t count, uint32_t value) {
    if (volume == NULL || first < 2 || first >= volume->cluster_count + 2 ||
        count > volume->cluster_count - ((uint64_t)first - 2)) return false;
    uint8_t entries[512];
    for (unsigned i = 0; i < sizeof(entries); i += 4) write_u32(entries + i, value);
    uint64_t offset = volume->fat_offset_bytes + (uint64_t)first * 4;
    uint64_t bytes = count * 4;
    while (bytes) {
        uint64_t chunk = sizeof(entries) - offset % sizeof(entries);
        if (chunk > bytes) chunk = bytes;
        if (!volume_write(volume, offset, entries, chunk)) return false;
        offset += chunk;
        bytes -= chunk;
    }
    return true;
}

static uint64_t append_mount_path(char *path, uint64_t capacity, const char *mountpoint) {
    uint64_t written = 0;
    while (mountpoint[written] != '\0' && written + 1 < capacity) {
        path[written] = mountpoint[written];
        written++;
    }
    if (written + 1 < capacity && (written == 0 || path[written - 1] != '/')) {
        path[written++] = '/';
    }
    return written;
}

static uint64_t append_parent_path(char *path, uint64_t capacity, const char *parent) {
    uint64_t written = 0;
    while (parent[written] != '\0' && written + 1 < capacity) {
        path[written] = parent[written];
        written++;
    }
    if (written + 1 < capacity && (written == 0 || path[written - 1] != '/')) {
        path[written++] = '/';
    }
    return written;
}

static void append_ascii_name(char *path, uint64_t *offset, uint64_t capacity, const uint8_t *name, uint64_t count) {
    for (uint64_t i = 0; i < count && *offset + 1 < capacity; i++) {
        uint16_t c = read_u16(name + i * 2);
        path[(*offset)++] = (c >= 32 && c < 127) ? (char)c : '?';
    }
}

static bool build_child_path(char *path,
    uint64_t capacity,
    const char *parent,
    const struct exfat_stream_entry *stream,
    const struct exfat_name_entry *names,
    uint64_t name_entry_count) {
    uint64_t parent_length=0;while(parent[parent_length]) parent_length++;
    uint64_t separator=parent_length && parent[parent_length-1]!='/'?1:0;
    if(!stream->name_length || stream->name_length>name_entry_count*15 ||
       parent_length+separator+stream->name_length>=capacity) return false;
    uint64_t path_offset = append_parent_path(path, capacity, parent);
    uint64_t remaining = stream->name_length;
    for (uint64_t i = 0; i < name_entry_count && remaining > 0; i++) {
        uint64_t count = remaining > 15 ? 15 : remaining;
        append_ascii_name(path, &path_offset, capacity, (const uint8_t *)&names[i] + 2, count);
        remaining -= count;
    }
    path[path_offset] = '\0';
    return true;
}

static bool exfat_read_all_impl(const struct vfs_node *node, const uint8_t **data, uint64_t *size) {
    const struct exfat_file *file = node->private_data;
    if (file->valid_size > file->size) return false;
    uint8_t *buffer = kmalloc(file->size == 0 ? 1 : file->size);
    uint64_t copied = 0;
    uint32_t cluster = file->first_cluster;

    if (buffer == NULL) {
        return false;
    }

    while (copied < file->valid_size) {
        uint64_t source_offset;
        if (!cluster_offset(file->volume, cluster, &source_offset)) {
            kfree(buffer);
            return false;
        }

        uint64_t chunk = file->volume->cluster_size;
        if (chunk > file->valid_size - copied) {
            chunk = file->valid_size - copied;
        }

        if (!volume_read(file->volume, source_offset, buffer + copied, chunk)) {
            kfree(buffer);
            return false;
        }
        copied += chunk;

        if (copied < file->valid_size) {
            if (file->no_fat_chain) {
                cluster++;
            } else if (!next_cluster(file->volume, cluster, &cluster) || cluster >= EXFAT_FAT_CHAIN_END) {
                kfree(buffer);
                return false;
            }
        }
    }

    /* Allocated bytes beyond ValidDataLength may contain previous disk data. */
    for (uint64_t i = copied; i < file->size; i++) buffer[i] = 0;
    *data = buffer;
    *size = file->size;
    return true;
}

static bool exfat_read_all(const struct vfs_node *node,const uint8_t **data,uint64_t *size) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    bool ok=exfat_read_all_impl(node,data,size);fs_leave(&scope);return ok;
}

static void exfat_release_data(const struct vfs_node *node, const uint8_t *data) {
    (void)node;
    kfree((void *)data);
}

static uint64_t clusters_for_size(const struct exfat_volume *volume, uint64_t size) {
    if (size == 0) {
        return 0;
    }
    return size / volume->cluster_size + (size % volume->cluster_size != 0);
}

static bool mount_has_file_slot(const struct exfat_mount *mount) {
    if (mount == NULL) {
        return false;
    }
    if (mount->file_count < EXFAT_MAX_FILES) {
        return true;
    }
    for (uint64_t i = 0; i < EXFAT_MAX_FILES; i++) {
        if (path_slot(mount, i)[0] == '\0') {
            return true;
        }
    }
    return false;
}

static bool register_file(struct exfat_mount *mount,
    const char *parent,
    const struct exfat_file_entry *file_entry,
    const struct exfat_stream_entry *stream,
    const struct exfat_name_entry *names,
    uint64_t name_entry_count,
    uint64_t file_entry_offset,
    uint64_t stream_entry_offset) {
    if (mount == NULL ||
        (file_entry->file_attributes & EXFAT_ATTR_DIRECTORY) != 0 ||
        (stream->data_length != 0 && stream->first_cluster < 2)) {
        return false;
    }

    char child_path[EXFAT_MAX_PATH];
    if (!build_child_path(child_path, sizeof(child_path), parent, stream, names,
            name_entry_count) || name_conflicts(child_path, NULL)) return false;

    uint64_t index = EXFAT_MAX_FILES;
    for (uint64_t i = 0; i < mount->file_count; i++) {
        if (path_slot(mount, i)[0] == '\0') {
            index = i;
            break;
        }
    }
    if (index == EXFAT_MAX_FILES) {
        if (mount->file_count >= EXFAT_MAX_FILES) {
            return false;
        }
        index = mount->file_count;
    }

    if(!reserve_tables(mount,index+1,mount->dir_count) || !vfs_reserve_nodes(1)) return false;
    copy_cstr(path_slot(mount, index), EXFAT_MAX_PATH, child_path);

    (*file_slot(mount, index)) = (struct exfat_file) {
        .volume = &mount->volume,
        .first_cluster = stream->first_cluster,
        .size = stream->data_length,
        .valid_size = stream->valid_data_length,
        .allocated_clusters = clusters_for_size(&mount->volume, stream->data_length),
        .file_entry_offset = file_entry_offset,
        .stream_entry_offset = stream_entry_offset,
        .secondary_count = file_entry->secondary_count,
        .no_fat_chain = (stream->flags & 0x02) != 0,
    };

    if (!vfs_register_file_with_release(path_slot(mount, index),
            file_slot(mount, index)->size,
            file_slot(mount, index),
            exfat_read_all,
            exfat_release_data)) {
        path_slot(mount,index)[0]=0;
        return false;
    }

    if (index == mount->file_count) {
        mount->file_count++;
    }
    return true;
}

static bool register_directory(struct exfat_mount *mount,
    const char *path,
    uint32_t first_cluster,
    bool no_fat_chain,
    uint64_t data_length,
    uint64_t file_entry_offset,
    uint8_t secondary_count) {
    if (mount == NULL || path == NULL || path[0] == '\0') {
        return false;
    }
    if (name_conflicts(path, NULL)) return false;
    uint64_t index=0;
    while(index<mount->dir_count && dir_slot(mount,index)->path[0]) index++;
    if(index>=EXFAT_MAX_DIRS || !reserve_tables(mount,mount->file_count,index+1) ||
       !vfs_reserve_nodes(1)) return false;
    uint64_t i = 0;
    while (path[i] != '\0' && i + 1 < sizeof(dir_slot(mount, index)->path)) {
        dir_slot(mount, index)->path[i] = path[i];
        i++;
    }
    dir_slot(mount, index)->path[i] = '\0';
    dir_slot(mount, index)->first_cluster = first_cluster;
    dir_slot(mount, index)->data_length = data_length;
    dir_slot(mount, index)->allocated_clusters = data_length == 0 ? 1 : clusters_for_size(&mount->volume, data_length);
    if (dir_slot(mount, index)->allocated_clusters == 0) {
        dir_slot(mount, index)->allocated_clusters = 1;
    }
    dir_slot(mount, index)->no_fat_chain = no_fat_chain;
    dir_slot(mount, index)->file_entry_offset = file_entry_offset;
    dir_slot(mount, index)->secondary_count = secondary_count;
    if(!vfs_register_directory(dir_slot(mount,index)->path)) {
        dir_slot(mount,index)->path[0]=0;return false;
    }
    if(index==mount->dir_count) mount->dir_count++;
    return true;
}

/* This driver can mutate only the basic stream + ASCII name entry set.
 * Refuse extensions rather than silently discard their metadata on rename. */
static bool valid_import_entry_set(const uint8_t *entry, uint64_t available) {
    uint64_t count = (uint64_t)entry[1] + 1;
    if (count < 3 || count > EXFAT_MAX_FILE_ENTRIES ||
        count * EXFAT_ENTRY_SIZE > available ||
        read_u16(entry + 2) != entry_set_checksum(entry, count)) return false;
    const struct exfat_stream_entry *stream = (const void *)(entry + EXFAT_ENTRY_SIZE);
    uint64_t names = ((uint64_t)stream->name_length + 14) / 15;
    if (stream->entry_type != EXFAT_ENTRY_STREAM || !stream->name_length ||
        count != 2 + names || !(stream->flags & 1) ||
        (!stream->data_length && (stream->first_cluster || (stream->flags & 2)))) return false;
    uint16_t hash = 0;
    for (uint64_t i = 0; i < names; i++) {
        const uint8_t *name = entry + (2 + i) * EXFAT_ENTRY_SIZE;
        if (name[0] != EXFAT_ENTRY_NAME || (name[1] & 3)) return false;
        for (uint64_t j = 0; j < 15 && i * 15 + j < stream->name_length; j++) {
            uint16_t c = read_u16(name + 2 + j * 2);
            if (c < 0x20 || c >= 0x7f || c == '"' || c == '*' || c == '/' ||
                c == ':' || c == '<' || c == '>' || c == '?' || c == '\\' || c == '|') return false;
            uint16_t upper = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
            hash = (uint16_t)(((hash >> 1) | ((hash & 1u) << 15)) + upper);
            hash = (uint16_t)((hash >> 1) | ((hash & 1u) << 15));
        }
    }
    if (read_u16(entry + 2 * EXFAT_ENTRY_SIZE + 2) == '.' &&
        (stream->name_length == 1 || (stream->name_length == 2 &&
         read_u16(entry + 2 * EXFAT_ENTRY_SIZE + 4) == '.'))) return false;
    return hash == stream->name_hash;
}

static bool scan_directory(const char *path,
    struct exfat_mount *mount,
    uint32_t first_cluster,
    bool no_fat_chain,
    uint64_t data_length,
    uint64_t depth) {
    if (depth > 8) {
        return false;
    }

    struct exfat_volume *volume = &mount->volume;
    uint32_t cluster = first_cluster;
    uint64_t visited = 0;
    uint64_t scanned = 0;
    /* Brent cycle detection: bounded state, without a volume-sized bitmap. */
    uint32_t chain_anchor = first_cluster;
    uint64_t chain_power = 1, chain_distance = 0;
    bool ok = true;
    uint8_t *cluster_buffer = kmalloc(volume->cluster_size);

    if (cluster_buffer == NULL) {
        return false;
    }

    while (cluster >= 2 && cluster < EXFAT_FAT_CHAIN_END && visited++ < volume->cluster_count) {
        uint64_t offset;
        if (!cluster_offset(volume, cluster, &offset)) {
            ok = false;
            break;
        }

        uint64_t cluster_limit = volume->cluster_size;
        if (data_length != 0 && data_length - scanned < cluster_limit) {
            cluster_limit = data_length - scanned;
        }

        if (!volume_read(volume, offset, cluster_buffer, cluster_limit)) {
            ok = false;
            break;
        }

        for (uint64_t entry_offset = 0; entry_offset + EXFAT_ENTRY_SIZE <= cluster_limit; entry_offset += EXFAT_ENTRY_SIZE) {
            const uint8_t *entry = cluster_buffer + entry_offset;
            if (entry[0] == EXFAT_ENTRY_END) {
                goto out;
            }

            if (entry[0] == EXFAT_ENTRY_BITMAP) {
                if (depth || volume->bitmap_seen || (entry[1] & 1)) { ok=false;goto out; }
                volume->bitmap_seen = true;
                volume->allocation_bitmap_cluster = read_u32(entry + 20);
                volume->allocation_bitmap_size = read_u64(entry + 24);
                continue;
            }

            if (entry[0] == EXFAT_ENTRY_UPCASE) {
                if (depth || volume->upcase_seen) { ok=false;goto out; }
                volume->upcase_seen = true;
                volume->upcase_table_checksum = read_u32(entry + 4);
                volume->upcase_table_cluster = read_u32(entry + 20);
                volume->upcase_table_size = read_u64(entry + 24);
                continue;
            }

            if (entry[0] != EXFAT_ENTRY_FILE) {
                /* Unknown active critical entries may own storage or alter
                 * interpretation. Only benign extensions can be skipped. */
                if ((entry[0] & 0x80) && !(entry[0] & 0x20) &&
                    (entry[0] != 0x83 || depth)) { ok=false;goto out; }
                continue;
            }

            const struct exfat_file_entry *file_entry = (const struct exfat_file_entry *)entry;
            if (!valid_import_entry_set(entry, cluster_limit - entry_offset)) {
                ok = false;
                goto out;
            }
            const struct exfat_stream_entry *stream = (const void *)(entry + EXFAT_ENTRY_SIZE);
            const struct exfat_name_entry *names = (const void *)(entry + 2 * EXFAT_ENTRY_SIZE);
            uint64_t name_entry_count = ((uint64_t)stream->name_length + 14) / 15;

            {
                uint64_t heap_bytes = (uint64_t)volume->cluster_count * volume->cluster_size;
                bool directory = (file_entry->file_attributes & EXFAT_ATTR_DIRECTORY) != 0;
                if (stream->valid_data_length > stream->data_length ||
                    stream->data_length > heap_bytes ||
                    (directory && (!stream->data_length ||
                        stream->valid_data_length != stream->data_length ||
                        stream->data_length % volume->cluster_size)) ||
                    (stream->data_length && (stream->first_cluster < 2 ||
                        stream->first_cluster >= volume->cluster_count + 2 ||
                        ((stream->flags & 2) && clusters_for_size(volume, stream->data_length) >
                            (uint64_t)volume->cluster_count + 2 - stream->first_cluster)))) {
                    ok = false; goto out;
                }
                uint64_t absolute_file_entry_offset = offset + entry_offset;
                uint64_t absolute_stream_entry_offset = (const uint8_t *)stream - cluster_buffer + offset;
                if ((file_entry->file_attributes & EXFAT_ATTR_DIRECTORY) != 0) {
                    char child_path[EXFAT_MAX_PATH];
                    if(!build_child_path(child_path, sizeof(child_path), path, stream, names, name_entry_count)) { ok=false;goto out; }
                    if (!register_directory(mount,
                        child_path,
                        stream->first_cluster,
                        (stream->flags & 0x02) != 0,
                        stream->data_length,
                        absolute_file_entry_offset,
                        file_entry->secondary_count)) { ok = false; goto out; }
                    if (!scan_directory(child_path,
                            mount,
                            stream->first_cluster,
                            (stream->flags & 0x02) != 0,
                            stream->data_length,
                            depth + 1)) {
                        ok = false;
                        goto out;
                    }
                } else {
                    if (!register_file(mount,
                        path,
                        file_entry,
                        stream,
                        names,
                        name_entry_count,
                        absolute_file_entry_offset,
                        absolute_stream_entry_offset)) { ok = false; goto out; }
                }
            }

            entry_offset += (uint64_t)file_entry->secondary_count * EXFAT_ENTRY_SIZE;
        }

        scanned += cluster_limit;
        if (data_length != 0 && scanned >= data_length) {
            goto out;
        }

        if (no_fat_chain) {
            cluster++;
        } else {
            if (!next_cluster(volume, cluster, &cluster)) { ok = false; goto out; }
            if (cluster >= EXFAT_FAT_CHAIN_END) {
                ok = data_length == 0; /* Otherwise the declared directory was short. */
                goto out;
            }
            if (cluster < 2 || cluster >= volume->cluster_count + 2 ||
                cluster == chain_anchor) { ok = false; goto out; }
            if (++chain_distance == chain_power) {
                chain_anchor = cluster;
                chain_power *= 2;
                chain_distance = 0;
            }
        }
    }
    ok = false; /* Invalid start or exhausted traversal budget is not an EOF. */

out:
    kfree(cluster_buffer);
    return ok;
}

/* System streams use FAT chains even when their clusters happen to be
 * adjacent. Validate exact termination before any sidecar replay or writes. */
static bool validate_system_stream(struct exfat_volume *volume, uint32_t cluster,
    uint64_t size, bool upcase) {
    uint64_t remaining = size, mapped = 0;
    uint32_t checksum = 0;
    bool skip = false;
    if (!size || size > (uint64_t)volume->cluster_count * volume->cluster_size ||
        (upcase && ((size & 1) || size > 131072))) return false;
    while (remaining) {
        uint64_t base;
        if (!cluster_offset(volume, cluster, &base)) return false;
        uint64_t bytes = remaining < volume->cluster_size ? remaining : volume->cluster_size;
        for (uint64_t off = 0; upcase && off < bytes;) {
            uint8_t buffer[512];
            uint64_t n = bytes-off < sizeof(buffer) ? bytes-off : sizeof(buffer);
            if (!volume_read(volume, base+off, buffer, n)) return false;
            for (uint64_t i=0;i<n;i++) checksum=(checksum>>1)|(checksum<<31),checksum+=buffer[i];
            for (uint64_t i=0;i<n;i+=2) {
                uint16_t value=read_u16(buffer+i);
                if (skip) {
                    if (!value || value > 65536-mapped) return false;
                    /* Mandatory ASCII lower-case characters cannot be skipped. */
                    if (mapped < 123 && mapped+value > 97) return false;
                    mapped += value;skip=false;
                } else if (value==0xffff && mapped!=65535) {
                    skip=true;
                } else {
                    if (mapped>=65536 || (mapped<128 && value !=
                        (mapped>='a' && mapped<='z' ? mapped-32 : mapped))) return false;
                    mapped++;
                }
            }
            off += n;
        }
        remaining -= bytes;
        uint32_t next;
        if (!next_cluster(volume, cluster, &next)) return false;
        if (!remaining) return next>=EXFAT_FAT_CHAIN_END &&
            (!upcase || (!skip && mapped==65536 && checksum==volume->upcase_table_checksum));
        cluster=next;
    }
    return false;
}

static bool validate_system_streams(struct exfat_volume *volume) {
    return volume->bitmap_seen && volume->upcase_seen &&
        volume->allocation_bitmap_size == ((uint64_t)volume->cluster_count+7)/8 &&
        validate_system_stream(volume,volume->allocation_bitmap_cluster,volume->allocation_bitmap_size,false) &&
        validate_system_stream(volume,volume->upcase_table_cluster,volume->upcase_table_size,true);
}

static bool scan_root_directory(struct exfat_mount *mount) {
    char root_path[EXFAT_MAX_PATH];
    uint64_t root_length = append_mount_path(root_path, sizeof(root_path), mount->mountpoint);
    if (root_length > 0 && root_path[root_length - 1] == '/') {
        root_path[root_length - 1] = '\0';
    } else {
        root_path[root_length] = '\0';
    }

    if (!register_directory(mount, root_path, mount->volume.root_directory_cluster, false, 0, 0, 0)) return false;
    return scan_directory(root_path, mount, mount->volume.root_directory_cluster, false, 0, 0) &&
        validate_system_streams(&mount->volume);
}

static void remember_mountpoint(struct exfat_mount *mount, const char *mountpoint) {
    uint64_t i = 0;
    while (mount != NULL && mountpoint != NULL && mountpoint[i] != '\0' && i + 1 < sizeof(mount->mountpoint)) {
        mount->mountpoint[i] = mountpoint[i];
        i++;
    }
    if (mount != NULL) {
        mount->mountpoint[i] = '\0';
    }
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

static bool mount_matches(const struct exfat_mount *mount, const char *mountpoint) {
    return mount != NULL &&
        mount->used &&
        (mountpoint == NULL || mountpoint[0] == '\0' || streq(mount->mountpoint, mountpoint));
}

static bool valid_mountpoint(const char *mountpoint) {
    if (!mountpoint || mountpoint[0] != '/' || !mountpoint[1]) return false;
    uint64_t start = 1;
    for (uint64_t i = 1; i < EXFAT_MAX_PATH; i++) {
        if (mountpoint[i] && mountpoint[i] != '/') continue;
        uint64_t length = i - start;
        if (!length || (length == 1 && mountpoint[start] == '.') ||
            (length == 2 && mountpoint[start] == '.' && mountpoint[start + 1] == '.')) return false;
        if (!mountpoint[i]) return true;
        start = i + 1;
    }
    return false;
}

static bool path_in_subtree(const char *path, const char *prefix) {
    uint64_t i = 0;
    while (prefix[i]) {
        if (path[i] != prefix[i]) return false;
        i++;
    }
    return !path[i] || path[i] == '/';
}

static bool mountpoint_in_use(const char *mountpoint) {
    /* No stacking: rollback/unmount unregisters the complete subtree. */
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (mounts[i].used && (path_in_subtree(mountpoint, mounts[i].mountpoint) ||
                path_in_subtree(mounts[i].mountpoint, mountpoint))) return true;
    }
    for (uint64_t i = 0; i < vfs_count(); i++) {
        const struct vfs_node *node = vfs_node_at(i);
        if (node && (path_in_subtree(node->path, mountpoint) ||
                (node->type != VFS_NODE_DIRECTORY && path_in_subtree(mountpoint, node->path)))) return true;
    }
    return false;
}

static struct exfat_file *find_file(const char *path) {
    for (uint64_t mount_index = 0; mount_index < EXFAT_MAX_VOLUMES; mount_index++) {
        struct exfat_mount *mount = &mounts[mount_index];
        if (!mount->used) {
            continue;
        }
        for (uint64_t i = 0; i < mount->file_count; i++) {
            if (streq(path_slot(mount, i), path)) {
                return file_slot(mount, i);
            }
        }
    }
    return NULL;
}

static struct exfat_mount *find_mount_and_index_for_file(const char *path, uint64_t *index_out) {
    for (uint64_t mount_index = 0; mount_index < EXFAT_MAX_VOLUMES; mount_index++) {
        struct exfat_mount *mount = &mounts[mount_index];
        if (!mount->used) {
            continue;
        }
        for (uint64_t i = 0; i < mount->file_count; i++) {
            if (streq(path_slot(mount, i), path)) {
                if (index_out != NULL) {
                    *index_out = i;
                }
                return mount;
            }
        }
    }
    return NULL;
}

static bool valid_ascii_basename(const char *path) {
    if (!path) return false;
    uint64_t length=0;while(length<EXFAT_MAX_PATH && path[length]) length++;
    if(length==EXFAT_MAX_PATH) return false;
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/') name = p + 1;
    if (!*name || streq(name, ".") || streq(name, "..")) return false;
    for (const char *p = name; *p; p++) {
        uint8_t c = (uint8_t)*p;
        if (c < 0x20 || c >= 0x7f || c == '"' || c == '*' || c == ':' ||
            c == '<' || c == '>' || c == '?' || c == '\\' || c == '|') return false;
    }
    return true;
}
static bool ascii_case_equal(const char *a, const char *b) {
    while (*a && *b) {
        char x = *a++, y = *b++;
        if (x >= 'a' && x <= 'z') x -= 'a' - 'A';
        if (y >= 'a' && y <= 'z') y -= 'a' - 'A';
        if (x != y) return false;
    }
    return *a == *b;
}
static bool name_conflicts(const char *path, const char *ignore) {
    for (uint64_t i = 0; i < vfs_count(); i++) {
        const struct vfs_node *node = vfs_node_at(i);
        if (node && (!ignore || !streq(node->path, ignore)) && ascii_case_equal(node->path, path)) return true;
    }
    return false;
}

static const char *path_basename(const char *path) {
    const char *name = path;
    for (uint64_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            name = path + i + 1;
        }
    }
    return name[0] != '\0' ? name : NULL;
}

static bool parent_path_of(const char *path, char *parent, uint64_t capacity) {
    if (path == NULL || parent == NULL || capacity == 0 || path[0] != '/') {
        return false;
    }

    uint64_t length = 0;
    uint64_t last_slash = 0;
    while (path[length] != '\0') {
        if (path[length] == '/') {
            last_slash = length;
        }
        length++;
    }
    if (length == 0 || last_slash == 0 || last_slash + 1 >= capacity) {
        return false;
    }
    for (uint64_t i = 0; i < last_slash; i++) {
        parent[i] = path[i];
    }
    parent[last_slash] = '\0';
    return true;
}

static struct exfat_directory *find_directory_in_mount(struct exfat_mount *mount, const char *path) {
    if (mount == NULL || path == NULL) {
        return NULL;
    }
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        if (streq(dir_slot(mount, i)->path, path)) {
            return dir_slot(mount, i);
        }
    }
    return NULL;
}

static struct exfat_mount *find_mount_and_index_for_directory(const char *path, uint64_t *index_out) {
    for (uint64_t mount_index = 0; mount_index < EXFAT_MAX_VOLUMES; mount_index++) {
        struct exfat_mount *mount = &mounts[mount_index];
        if (!mount->used) {
            continue;
        }
        for (uint64_t i = 0; i < mount->dir_count; i++) {
            if (dir_slot(mount, i)->path[0] != '\0' && streq(dir_slot(mount, i)->path, path)) {
                if (index_out != NULL) {
                    *index_out = i;
                }
                return mount;
            }
        }
    }
    return NULL;
}

static struct exfat_mount *find_mount_and_parent_directory(const char *path,
    struct exfat_directory **directory_out,
    const char **name_out) {
    char parent[EXFAT_MAX_PATH];
    if (!parent_path_of(path, parent, sizeof(parent))) {
        return NULL;
    }
    const char *name = path_basename(path);
    if (name == NULL) {
        return NULL;
    }
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mounts[i].used) {
            continue;
        }
        struct exfat_directory *directory = find_directory_in_mount(&mounts[i], parent);
        if (directory != NULL) {
            if (directory_out != NULL) {
                *directory_out = directory;
            }
            if (name_out != NULL) {
                *name_out = name;
            }
            return &mounts[i];
        }
    }
    return NULL;
}

static uint64_t ascii_length(const char *s) {
    uint64_t length = 0;
    while (s[length] != '\0') {
        length++;
    }
    return length;
}

static bool append_text(char *out, uint64_t capacity, uint64_t *offset, const char *text) {
    if (out == NULL || offset == NULL || text == NULL) {
        return false;
    }
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (*offset + 1 >= capacity) {
            return false;
        }
        out[(*offset)++] = text[i];
    }
    out[*offset] = '\0';
    return true;
}

static bool append_char(char *out, uint64_t capacity, uint64_t *offset, char ch) {
    if (out == NULL || offset == NULL || *offset + 1 >= capacity) {
        return false;
    }
    out[(*offset)++] = ch;
    out[*offset] = '\0';
    return true;
}

static bool append_u64(char *out, uint64_t capacity, uint64_t *offset, uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && count < sizeof(digits));

    while (count > 0) {
        if (!append_char(out, capacity, offset, digits[--count])) {
            return false;
        }
    }
    return true;
}

static bool append_path_piece(char *out, uint64_t capacity, const char *piece) {
    uint64_t offset = ascii_length(out);
    return append_text(out, capacity, &offset, piece);
}

static bool path_under_mount(const struct exfat_mount *mount, const char *path) {
    if (mount == NULL || path == NULL || !mount->used) {
        return false;
    }
    uint64_t i = 0;
    while (mount->mountpoint[i] != '\0') {
        if (path[i] != mount->mountpoint[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static bool metadata_directory_path(const struct exfat_mount *mount, char *out, uint64_t capacity) {
    if (mount == NULL || out == NULL || capacity == 0) {
        return false;
    }
    uint64_t offset = append_mount_path(out, capacity, mount->mountpoint);
    out[offset] = '\0';
    return append_path_piece(out, capacity, ".srvros");
}

static bool metadata_file_path(const struct exfat_mount *mount, char *out, uint64_t capacity) {
    if (!metadata_directory_path(mount, out, capacity)) {
        return false;
    }
    return append_path_piece(out, capacity, "/meta");
}

static bool metadata_temp_path(const struct exfat_mount *mount, char *out, uint64_t capacity) {
    if (!metadata_directory_path(mount, out, capacity)) {
        return false;
    }
    return append_path_piece(out, capacity, "/meta.tmp");
}

static bool metadata_path_is_internal(const struct exfat_mount *mount, const char *path) {
    char directory[EXFAT_MAX_PATH];
    if (!metadata_directory_path(mount, directory, sizeof(directory))) {
        return false;
    }
    uint64_t i = 0;
    while (directory[i] != '\0') {
        if (path[i] != directory[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static struct exfat_mount *find_mount_for_path(const char *path) {
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (path_under_mount(&mounts[i], path)) {
            return &mounts[i];
        }
    }
    return NULL;
}

static bool parse_u64_field(const uint8_t *data,
    uint64_t size,
    uint64_t *offset,
    uint64_t *value_out,
    bool final_field) {
    if (data == NULL || offset == NULL || value_out == NULL || *offset >= size) {
        return false;
    }
    uint64_t value = 0;
    uint64_t digits = 0;
    while (*offset < size && data[*offset] >= '0' && data[*offset] <= '9') {
        uint64_t digit = (uint64_t)(data[*offset] - '0');
        if (value > (UINT64_MAX - digit) / 10) return false;
        value = value * 10 + digit;
        (*offset)++;
        digits++;
    }
    if (digits == 0) {
        return false;
    }
    if (final_field) {
        while (*offset < size && data[*offset] == '\r') {
            (*offset)++;
        }
        if (*offset < size && data[*offset] == '\n') {
            (*offset)++;
        } else {
            return false; /* Every serialized record ends in a newline. */
        }
    } else {
        if (*offset >= size || data[*offset] != '\t') {
            return false;
        }
        (*offset)++;
    }
    *value_out = value;
    return true;
}

static bool serialize_metadata_record(char *out,
    uint64_t capacity,
    uint64_t *offset,
    const struct vfs_node *node) {
    if (!append_text(out, capacity, offset, node->path) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.inode) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.mode) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.nlink) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.uid) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.gid) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.atime) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.mtime) ||
        !append_char(out, capacity, offset, '\t') ||
        !append_u64(out, capacity, offset, node->metadata.ctime) ||
        !append_char(out, capacity, offset, '\n')) {
        return false;
    }
    return true;
}

/* Serialize a complete mutation while allowing only the existing I/O service
 * threads to run during disk waits. Nested sidecar changes share its dirty bit. */
struct mutation_scope { struct fs_scope access; struct exfat_volume *volume; };
static bool write_volume_flags(struct exfat_volume *volume, uint16_t flags) {
    uint8_t bytes[2]; write_u16(bytes, flags);
    mutation_reads_invalidate(volume,106,sizeof(bytes));
    uint64_t start=cost_clock();
    bool ok=block_write(volume->device,106,bytes,sizeof(bytes));
    if(volume->cost.active) {
        struct exfat_cost_phase *p=&volume->cost.phase[EXFAT_IO_METADATA];
        p->writes++;p->write_bytes+=sizeof(bytes);p->write_us+=cost_elapsed(start);
    }
    if(!ok)return false;
    volume->flags = flags;
    return volume_flush(volume);
}
static bool reserve_mutation(struct exfat_mount *,const char *,uint64_t,uint64_t);
static bool mutation_begin_result(struct exfat_mount *mount, struct mutation_scope *scope,
    const char *path,uint64_t files,uint64_t dirs,bool *retryable,const char *operation) {
    if(!fs_enter(&scope->access))return false;
    if(!mount || !mount->used)goto denied;
    struct exfat_volume *volume = &mount->volume;
    struct block_device *parent = block_parent(volume->device);
    if (volume->image || !block_device_online(volume->device) || !volume->device->write ||
        !parent || !parent->flush || volume->write_blocked) goto denied;
    if(!reserve_mutation(mount,path,files,dirs)) {
        /* Capacity/heap reservation has no media effects. Only an admitted,
         * clean outer operation can promise this before dirty marking. */
        if(retryable && !volume->mutation_depth && !(volume->flags & 2u)) *retryable=true;
        goto denied;
    }
    scope->volume = volume;
    if (volume->mutation_depth == 0) {
        volume->mutation_written = false;
        cost_begin(&volume->cost,operation);
        /* Main VolumeFlags are authoritative; backup flags may be stale and
         * neither flags field participates in the boot-region checksum. */
        if (!write_volume_flags(volume, (volume->flags | 2u) & ~8u)) {
            volume->write_blocked = true;
            cost_end(&volume->cost,false);
            goto denied;
        }
    }
    if(volume->mutation_depth++==0)mutation_reads_reset(volume);
    return true;
denied:
    fs_leave(&scope->access);
    return false;
}
static bool mutation_begin(struct exfat_mount *mount, struct mutation_scope *scope,
    const char *path,uint64_t files,uint64_t dirs,const char *operation) {
    return mutation_begin_result(mount,scope,path,files,dirs,NULL,operation);
}
static bool mutation_end_result(struct mutation_scope *scope, bool ok, bool *retryable) {
    struct exfat_volume *volume = scope->volume;
    if (!ok && volume->mutation_written) volume->write_blocked = true;
    if (--volume->mutation_depth == 0 && !volume->write_blocked) {
        /* Data/metadata must be durable before publishing the clean marker. */
        if (!volume_flush(volume) || !write_volume_flags(volume, volume->flags & ~2u)) {
            volume->write_blocked = true;
            /* Best effort only: a quarantined transport may reject this. */
            (void)write_volume_flags(volume, volume->flags | 2u);
        }
    }
    if (volume->write_blocked) ok = false;
    /* Capture the result before releasing storage ownership. A later writable
     * probe cannot describe this transaction's writes or clean-marker flush. */
    if (retryable) *retryable = !ok && !volume->mutation_depth &&
        !volume->mutation_written && !volume->write_blocked && !(volume->flags & 2u) &&
        block_device_online(volume->device);
    if(!volume->mutation_depth) { mutation_reads_reset(NULL);cost_end(&volume->cost,ok); }
    fs_leave(&scope->access);
    return ok;
}

static bool mutation_end(struct mutation_scope *scope, bool ok) {
    return mutation_end_result(scope, ok, NULL);
}

/* Reserve transient sidecar nodes as well as the caller's new node. Check
 * under the mutation owner before any data/directory writes: a full table is
 * an ordinary refusal, not a partially published operation and quarantine. */
static bool mutation_capacity(struct exfat_mount *mount, const char *path,
    uint64_t files, uint64_t dirs) {
    if (!mount) return false;
    if (!path || !metadata_path_is_internal(mount, path)) {
        char name[EXFAT_MAX_PATH];
        if (!metadata_directory_path(mount, name, sizeof(name))) return false;
        if (!vfs_lookup(name)) dirs++;
        if (!metadata_file_path(mount, name, sizeof(name))) return false;
        if (!vfs_lookup(name)) files++;
        if (!metadata_temp_path(mount, name, sizeof(name))) return false;
        if (!vfs_lookup(name)) files++;
    }
    uint64_t free_files = EXFAT_MAX_FILES - used_files(mount);
    bool ok = free_files >= files && EXFAT_MAX_DIRS - used_dirs(mount) >= dirs &&
        vfs_free_nodes() >= files + dirs;
    if (!ok) console_write("exfat: node capacity exhausted; mutation refused\n");
    return ok;
}

static bool reserve_mutation(struct exfat_mount *mount,const char *path,uint64_t files,uint64_t dirs) {
    if(!mutation_capacity(mount,path,files,dirs)) return false;
    if(!path || !metadata_path_is_internal(mount,path)) {
        char name[EXFAT_MAX_PATH];
        if(!metadata_directory_path(mount,name,sizeof(name))) return false;
        if(!vfs_lookup(name)) dirs++;
        if(!metadata_file_path(mount,name,sizeof(name))) return false;
        if(!vfs_lookup(name)) files++;
        if(!metadata_temp_path(mount,name,sizeof(name))) return false;
        if(!vfs_lookup(name)) files++;
    }
    uint64_t f=used_files(mount),d=used_dirs(mount);
    uint64_t fh=mount->file_count-f,dh=mount->dir_count-d;
    bool ok=reserve_tables(mount,mount->file_count+(files>fh?files-fh:0),
        mount->dir_count+(dirs>dh?dirs-dh:0)) && vfs_reserve_nodes(files+dirs) &&
        (metadata_syncing || reserve_metadata(mount,f+d+files+dirs));
    if(!ok) console_write("exfat: node memory unavailable; mutation refused before writes\n");
    return ok;
}

bool exfat_creation_capacity(const char *path, uint64_t files, uint64_t dirs) {
    if (!path || files > EXFAT_MAX_FILES || dirs > EXFAT_MAX_DIRS) return false;
    return mutation_capacity(find_mount_for_path(path), path, files, dirs);
}

static bool exfat_sync_mount_metadata_impl(struct exfat_mount *mount) {
    if (mount == NULL || mount->volume.image != NULL || metadata_syncing) {
        return true;
    }

    metadata_syncing = true;

    char directory[EXFAT_MAX_PATH];
    char path[EXFAT_MAX_PATH];
    char temp_path[EXFAT_MAX_PATH];
    bool ok = metadata_directory_path(mount, directory, sizeof(directory)) &&
        metadata_file_path(mount, path, sizeof(path)) &&
        metadata_temp_path(mount, temp_path, sizeof(temp_path));
    if (ok && vfs_lookup(directory) == NULL) {
        ok = exfat_create_directory(directory);
    }

    uint64_t offset = 0;
    if (ok) {
        ok = append_text(mount->metadata_buffer, mount->metadata_capacity, &offset, "SRVROSMETA1\n");
    }
    for (uint64_t i = 0; ok && i < vfs_count(); i++) {
        const struct vfs_node *node = vfs_node_at(i);
        if (node == NULL ||
            !path_under_mount(mount, node->path) ||
            metadata_path_is_internal(mount, node->path)) {
            continue;
        }
        ok = serialize_metadata_record(mount->metadata_buffer, mount->metadata_capacity, &offset, node);
    }
    if (ok) {
        if (vfs_lookup(temp_path) != NULL) {
            ok = exfat_delete_file(temp_path);
        }
        if (ok) {
            ok = exfat_write_file(temp_path, (const uint8_t *)mount->metadata_buffer, offset);
        }
        if (ok) {
            if (vfs_lookup(path) != NULL) {
                bool published;
                ok = exfat_replace_file(temp_path, path, &published);
            } else {
                ok = exfat_rename(temp_path, path);
            }
        }
    }

    metadata_syncing = false;
    return ok;
}

static bool exfat_sync_mount_metadata(struct exfat_mount *mount) {
    if (!mount || metadata_syncing || mount->volume.image) return true;
    struct mutation_scope scope;
    if (!mutation_begin(mount, &scope, NULL, 0, 0,"metadata")) return false;
    return mutation_end(&scope, mutation_capacity(mount, NULL, 0, 0) &&
        exfat_sync_mount_metadata_impl(mount));
}

static bool exfat_metadata_changed(const char *path, const struct vfs_metadata *metadata) {
    (void)metadata;
    if (metadata_syncing || path == NULL) {
        return true;
    }

    struct exfat_mount *mount = find_mount_for_path(path);
    if (mount == NULL ||
        mount->volume.image != NULL ||
        metadata_path_is_internal(mount, path)) {
        return true;
    }
    return exfat_sync_mount_metadata(mount);
}

/* Validate and apply the same immutable read buffer. A malformed suffix must
 * not partially install permissions/identities, nor may one volume name nodes
 * belonging to another filesystem. */
static bool parse_metadata_records(struct exfat_mount *mount, const uint8_t *data,
    uint64_t size, uint64_t magic_length, bool apply) {
    bool ok = true;
    uint64_t offset = magic_length;
    while (ok && offset < size) {
        while (offset < size && (data[offset] == '\n' || data[offset] == '\r')) {
            offset++;
        }
        if (offset >= size) {
            break;
        }

        char node_path[EXFAT_MAX_PATH];
        uint64_t path_length = 0;
        while (offset < size && data[offset] != '\t' && path_length + 1 < sizeof(node_path)) {
            if (data[offset] < 32 || data[offset] > 126) return false;
            node_path[path_length++] = (char)data[offset++];
        }
        node_path[path_length] = '\0';
        if (offset >= size || data[offset] != '\t') {
            ok = false;
            break;
        }
        offset++;
        if (!path_under_mount(mount, node_path) ||
            metadata_path_is_internal(mount, node_path)) return false;

        struct vfs_metadata metadata;
        if (!parse_u64_field(data, size, &offset, &metadata.inode, false) ||
            !parse_u64_field(data, size, &offset, &metadata.mode, false) ||
            !parse_u64_field(data, size, &offset, &metadata.nlink, false) ||
            !parse_u64_field(data, size, &offset, &metadata.uid, false) ||
            !parse_u64_field(data, size, &offset, &metadata.gid, false) ||
            !parse_u64_field(data, size, &offset, &metadata.atime, false) ||
            !parse_u64_field(data, size, &offset, &metadata.mtime, false) ||
            !parse_u64_field(data, size, &offset, &metadata.ctime, true)) {
            ok = false;
            break;
        }

        if (!apply) {
            continue;
        }
        const struct vfs_node *target = vfs_lookup(node_path);
        if (target == NULL || metadata_path_is_internal(mount, node_path)) {
            continue;
        }
        uint64_t type = target->type == VFS_NODE_DIRECTORY ? VFS_MODE_IFDIR : VFS_MODE_IFREG;
        metadata.mode = (metadata.mode & ~VFS_MODE_IFMT) | type;
        (void)vfs_set_metadata(node_path, &metadata);
    }

    return ok;
}

static bool parse_metadata_file(struct exfat_mount *mount, const char *path, bool apply) {
    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data = NULL;
    uint64_t size = 0;
    const char *magic = "SRVROSMETA1\n";
    uint64_t magic_length = ascii_length(magic);
    if (node == NULL ||
        !vfs_read_all(node, &data, &size) ||
        size < magic_length ||
        !streqn((const char *)data, magic, magic_length)) {
        if (node != NULL && data != NULL) {
            vfs_release_data(node, data);
        }
        return false;
    }

    bool ok = parse_metadata_records(mount, data, size, magic_length, false);
    if (ok && apply) ok = parse_metadata_records(mount, data, size, magic_length, true);
    vfs_release_data(node, data);
    return ok;
}

static void load_mount_metadata(struct exfat_mount *mount) {
    char path[EXFAT_MAX_PATH];
    char temp_path[EXFAT_MAX_PATH];
    if (mount == NULL ||
        !metadata_file_path(mount, path, sizeof(path)) ||
        !metadata_temp_path(mount, temp_path, sizeof(temp_path))) {
        return;
    }

    metadata_syncing = true;
    /* Registration time is not a file modification date. Until native exFAT
     * timestamp/UTC-offset decoding is implemented, imported entries without
     * our persisted metadata have unknown dates. New mutations still get UTC. */
    for(uint64_t i=0;i<vfs_count();i++) {
        const struct vfs_node *node=vfs_node_at(i);
        if(node && find_mount_for_path(node->path)==mount) {
            struct vfs_metadata metadata=node->metadata;
            metadata.atime=metadata.mtime=metadata.ctime=0;
            (void)vfs_set_metadata(node->path,&metadata);
        }
    }
    /* Path-keyed sidecars cannot establish which inode owns a name after an
     * interrupted replacement. Keep default metadata on quarantined media;
     * neither replay nor cleanup is an offline recovery protocol. */
    if (mount->volume.write_blocked) {
        metadata_syncing = false;
        return;
    }
    if (parse_metadata_file(mount, temp_path, true)) {
        if (vfs_lookup(path) != NULL) {
            bool published;
            (void)exfat_replace_file(temp_path, path, &published);
        } else {
            (void)exfat_rename(temp_path, path);
        }
    } else {
        (void)parse_metadata_file(mount, path, true);
        /* A failed read is not proof of an invalid temporary file. Preserve
         * it for retry/offline inspection; the next explicit sync can replace
         * it after serializing the current namespace. */
    }
    metadata_syncing = false;
}

static uint16_t entry_set_checksum(const uint8_t *entries, uint64_t entry_count) {
    uint16_t checksum = 0;
    uint64_t byte_count = entry_count * EXFAT_ENTRY_SIZE;
    for (uint64_t i = 0; i < byte_count; i++) {
        if (i == 2 || i == 3) {
            continue;
        }
        checksum = (uint16_t)(((checksum & 1) != 0 ? 0x8000 : 0) + (checksum >> 1) + entries[i]);
    }
    return checksum;
}

static bool write_file_data_impl(struct exfat_file *file, const uint8_t *data, uint64_t size) {
    if (file != NULL && size == 0) {
        return true;
    }
    if (file == NULL || data == NULL || size > file->allocated_clusters * file->volume->cluster_size) {
        return false;
    }

    /* Group only a proven contiguous allocation. Fragmented FAT chains retain
     * one-cluster traversal, and a large cluster retains its existing buffer.
     * The block layer and driver still enforce their own transfer limits. */
    uint64_t cluster_size=file->volume->cluster_size, buffer_bytes=cluster_size;
    if(file->no_fat_chain && file->volume->device) {
        struct block_device *device=block_parent(file->volume->device);
        uint64_t limit=device?device->max_transfer_bytes:0;
        if(limit>BLOCK_MAX_BATCH_BYTES)limit=BLOCK_MAX_BATCH_BYTES;
        if(limit>cluster_size)buffer_bytes=limit-limit%cluster_size;
    }
    uint8_t *cluster_buffer = kcalloc(buffer_bytes, 1);
    if (cluster_buffer == NULL) return false;

    uint64_t copied = 0;
    uint32_t cluster = file->first_cluster;
    uint64_t clusters = file->allocated_clusters;
    for (uint64_t i = 0; i < clusters;) {
        uint64_t span=buffer_bytes/cluster_size;
        if(span>clusters-i)span=clusters-i;
        uint64_t needed=(size-copied+cluster_size-1)/cluster_size;
        if(span>needed)span=needed;
        uint64_t offset,last_offset;
        if(span-1>UINT32_MAX-cluster ||
           !cluster_offset(file->volume,cluster,&offset) ||
           !cluster_offset(file->volume,cluster+(uint32_t)span-1,&last_offset)) {
            kfree(cluster_buffer);return false;
        }
        uint64_t bytes=span*cluster_size;
        for (uint64_t j=0;j<bytes;j++)cluster_buffer[j]=0;
        uint64_t chunk=bytes;
        if(chunk>size-copied)chunk=size-copied;
        for(uint64_t j=0;j<chunk;j++)cluster_buffer[j]=data[copied+j];
        if(!volume_write(file->volume,offset,cluster_buffer,bytes)) {
            kfree(cluster_buffer);return false;
        }
        copied+=chunk;i+=span;
        if(copied>=size)break;
        if(file->no_fat_chain)cluster+=(uint32_t)span;
        else if(!next_cluster(file->volume,cluster,&cluster) || cluster>=EXFAT_FAT_CHAIN_END) {
            kfree(cluster_buffer);return false;
        }
    }
    kfree(cluster_buffer);
    return true;
}

static bool write_file_data(struct exfat_file *file, const uint8_t *data, uint64_t size) {
    struct exfat_volume *volume=file?file->volume:NULL;
    enum exfat_io_phase previous=volume?volume->io_phase:EXFAT_IO_METADATA;
    if(volume)volume->io_phase=EXFAT_IO_DATA;
    bool ok=write_file_data_impl(file,data,size);
    if(volume)volume->io_phase=previous;
    return ok;
}

static bool update_file_length(struct exfat_file *file, const char *path, uint64_t size) {
    /* Length/cluster/flags belong to the checksummed entry set. Preserve the
     * name and any secondary records while publishing the updated checksum. */
    uint64_t count = (uint64_t)file->secondary_count + 1;
    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (count < 3 || count > EXFAT_MAX_FILE_ENTRIES ||
        file->stream_entry_offset != file->file_entry_offset + EXFAT_ENTRY_SIZE ||
        !volume_read(file->volume, file->file_entry_offset, entries, count * EXFAT_ENTRY_SIZE) ||
        entries[0] != EXFAT_ENTRY_FILE || entries[1] != file->secondary_count ||
        entries[EXFAT_ENTRY_SIZE] != EXFAT_ENTRY_STREAM) {
        return false;
    }
    uint8_t *stream = entries + EXFAT_ENTRY_SIZE;
    stream[1] = file->no_fat_chain && size != 0 ? 0x03 : 0x01; /* AllocationPossible is mandatory. */
    write_u64(stream + 8, size);
    write_u32(stream + 20, file->first_cluster);
    write_u64(stream + 24, size);
    write_u16(entries + 2, entry_set_checksum(entries, count));
    if (!volume_write(file->volume, file->file_entry_offset, entries, count * EXFAT_ENTRY_SIZE)) {
        return false;
    }

    file->size = size;
    file->valid_size = size;
    /* The owning operation persists metadata after retiring the old extent (or
     * finishing directory growth). An eager sidecar allocation can otherwise
     * exhaust space that this very operation is about to release. The VFS
     * update and suppressed callback do not yield; retain an outer sync guard. */
    bool syncing = metadata_syncing;
    metadata_syncing = true;
    (void)vfs_update_file_size(path, size);
    metadata_syncing = syncing;
    return true;
}

/* A full-buffer write may be a pure append (including global sync of another
 * writer). Prove the old prefix from media; never trust caller dirty hints.
 * Old payload sectors must remain untouched even on a torn new-data write. */
enum append_result { APPEND_FALLBACK, APPEND_OK, APPEND_FAILED };
static enum append_result append_file(struct exfat_file *file,const char *path,
    const uint8_t *data,uint64_t size) {
#if defined(SRVROS_EXFAT_HOST_TEST) && defined(SRVROS_EXFAT_APPEND_OFF)
    (void)file;(void)path;(void)data;(void)size;return APPEND_FALLBACK;
#else
    struct exfat_volume *v=file->volume;
    uint64_t block=v->device?v->device->block_size:0;
    if(!file->size || size<=file->size || file->valid_size!=file->size ||
       !block || v->cluster_size%block || v->cluster_heap_offset_bytes%block ||
       file->size%block || file->allocated_clusters!=clusters_for_size(v,file->size))return APPEND_FALLBACK;
    /* Imported exact aliases must keep private replacement semantics. */
    for(unsigned m=0;m<EXFAT_MAX_VOLUMES;m++)if(mounts[m].used && &mounts[m].volume==v)
        for(uint64_t i=0;i<mounts[m].file_count;i++) {
            struct exfat_file *other=file_slot(&mounts[m],i);
            if(other!=file && path_slot(&mounts[m],i)[0] && other->first_cluster==file->first_cluster)return APPEND_FALLBACK;
        }
    uint8_t *buffer=kmalloc(16384);if(!buffer)return APPEND_FAILED;
    uint64_t checked=0;uint32_t last=file->first_cluster;
    while(checked<file->size) {
        uint64_t base;if(!cluster_offset(v,last,&base)){kfree(buffer);return APPEND_FAILED;}
        uint64_t bytes=file->size-checked;if(bytes>v->cluster_size)bytes=v->cluster_size;
        for(uint64_t n=0;n<bytes;) {
            uint64_t count=bytes-n;if(count>16384)count=16384;
            if(!volume_read(v,base+n,buffer,count)){kfree(buffer);return APPEND_FAILED;}
            for(uint64_t j=0;j<count;j++)if(buffer[j]!=data[checked+n+j]){kfree(buffer);return APPEND_FALLBACK;}
            n+=count;
        }
        checked+=bytes;
        if(checked<file->size) {
            if(file->no_fat_chain)last++;
            else if(!next_cluster(v,last,&last) || last>=EXFAT_FAT_CHAIN_END){kfree(buffer);return APPEND_FAILED;}
        }
    }
    kfree(buffer);
    if(v->cost.active && !metadata_syncing)v->cost.operation="append";
    uint64_t clusters=clusters_for_size(v,size),added=clusters-file->allocated_clusters;
    uint32_t first=0;bool contiguous=true;
    if(added && !allocate_clusters(v,added,&first,&contiguous))return APPEND_FAILED;
    /* Convert only the allocation links; a NoFatChain old stream still ignores
     * them until its final stream record publishes the new FAT-backed extent. */
    if(added && contiguous)for(uint64_t i=0;i+1<added;i++)
        if(!write_fat_entry(v,first+(uint32_t)i,first+(uint32_t)i+1))return APPEND_FAILED;
    if(added && file->no_fat_chain)for(uint64_t i=0;i+1<file->allocated_clusters;i++)
        if(!write_fat_entry(v,file->first_cluster+(uint32_t)i,file->first_cluster+(uint32_t)i+1))return APPEND_FAILED;
    uint64_t done=file->size,within=done%v->cluster_size;
    uint32_t cluster=within?last:first;bool old_tail=within!=0;
    enum exfat_io_phase phase=v->io_phase;v->io_phase=EXFAT_IO_DATA;
    while(done<size) {
        uint64_t offset,bytes=v->cluster_size-within;if(bytes>size-done)bytes=size-done;
        if(!cluster_offset(v,cluster,&offset) || !volume_write(v,offset+within,data+done,bytes)) {
            v->io_phase=phase;return APPEND_FAILED;
        }
        done+=bytes;within=0;
        if(done<size) {
            if(old_tail){cluster=first;old_tail=false;}
            else if(contiguous)cluster++;
            else if(!next_cluster(v,cluster,&cluster) || cluster>=EXFAT_FAT_CHAIN_END){v->io_phase=phase;return APPEND_FAILED;}
        }
    }
    v->io_phase=phase;
    if(!volume_flush(v))return APPEND_FAILED;
    if(added && (!write_fat_entry(v,last,first) || !volume_flush(v)))return APPEND_FAILED;
    uint64_t old_clusters=file->allocated_clusters;bool old_contiguous=file->no_fat_chain;
    file->allocated_clusters=clusters;if(added)file->no_fat_chain=false;
    if(!update_file_length(file,path,size)) {
        file->allocated_clusters=old_clusters;file->no_fat_chain=old_contiguous;return APPEND_FAILED;
    }
    if(!volume_flush(v) || !exfat_metadata_changed(path,NULL))return APPEND_FAILED;
    return APPEND_OK;
#endif
}

static bool overwrite_file(const char *path, const uint8_t *data, uint64_t size) {
    struct exfat_file *file = find_file(path);
    if (file == NULL || file->volume->image != NULL) {
        return false;
    }

    uint64_t cluster_count = clusters_for_size(file->volume, size);
    if (size == 0) {
        uint32_t old_first_cluster = file->first_cluster;
        uint64_t old_allocated_clusters = file->allocated_clusters;
        bool old_no_fat_chain = file->no_fat_chain;
        file->first_cluster = 0;
        file->allocated_clusters = 0;
        file->no_fat_chain = true;
        if (!update_file_length(file, path, 0)) {
            file->first_cluster = old_first_cluster;
            file->allocated_clusters = old_allocated_clusters;
            file->no_fat_chain = old_no_fat_chain;
            return false;
        }
        struct exfat_file old = {
            .volume = file->volume,
            .first_cluster = old_first_cluster,
            .allocated_clusters = old_allocated_clusters,
            .no_fat_chain = old_no_fat_chain,
        };
        if (!volume_flush(file->volume)) return false;
        return free_file_clusters(&old) && exfat_metadata_changed(path, NULL);
    }

    enum append_result appended=append_file(file,path,data,size);
    if(appended!=APPEND_FALLBACK)return appended==APPEND_OK;

    /* Always write replacement data into new clusters. In-place overwrite
     * destroys the old contents before the directory update can be published. */

    uint32_t old_first_cluster = file->first_cluster;
    uint64_t old_allocated_clusters = file->allocated_clusters;
    bool old_no_fat_chain = file->no_fat_chain;
    uint32_t first_cluster = 0;
    bool no_fat_chain = true;
    if (!allocate_clusters(file->volume, cluster_count, &first_cluster, &no_fat_chain)) {
        return false;
    }

    struct exfat_file scratch = {
        .volume = file->volume,
        .first_cluster = first_cluster,
        .size = size,
        .valid_size = size,
        .allocated_clusters = cluster_count,
        .no_fat_chain = no_fat_chain,
    };
    if (!write_file_data(&scratch, data, size)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    if (!volume_flush(file->volume)) return false;
    file->first_cluster = first_cluster;
    file->allocated_clusters = cluster_count;
    file->no_fat_chain = no_fat_chain;
    if (!update_file_length(file, path, size)) {
        file->first_cluster = old_first_cluster;
        file->allocated_clusters = old_allocated_clusters;
        file->no_fat_chain = old_no_fat_chain;
        (void)free_file_clusters(&scratch);
        return false;
    }

    struct exfat_file old = {
        .volume = file->volume,
        .first_cluster = old_first_cluster,
        .allocated_clusters = old_allocated_clusters,
        .no_fat_chain = old_no_fat_chain,
    };
    if (!volume_flush(file->volume)) return false;
    return free_file_clusters(&old) && exfat_metadata_changed(path, NULL);
}

static bool bitmap_bit_is_set(const uint8_t *bitmap, uint64_t index) {
    return (bitmap[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

static uint64_t count_used_bitmap_bits(const uint8_t *bitmap, uint64_t start_bit, uint64_t bit_count) {
    uint64_t used = 0;
    for (uint64_t i = 0; i < bit_count; i++) {
        if (bitmap_bit_is_set(bitmap, start_bit + i)) {
            used++;
        }
    }
    return used;
}

static void bitmap_set_bit(uint8_t *bitmap, uint64_t index) {
    bitmap[index / 8] |= (uint8_t)(1u << (index % 8));
}

static void bitmap_clear_bit(uint8_t *bitmap, uint64_t index) {
    bitmap[index / 8] &= (uint8_t)~(1u << (index % 8));
}

static uint64_t allocation_bitmap_bytes(const struct exfat_volume *volume) {
    if (volume == NULL) {
        return 0;
    }

    uint64_t bytes = (volume->cluster_count + 7) / 8;
    if (volume->allocation_bitmap_size != 0 && volume->allocation_bitmap_size < bytes) {
        bytes = volume->allocation_bitmap_size;
    }
    if (bytes > volume->cluster_size) {
        bytes = volume->cluster_size;
    }
    return bytes;
}

/* Only bitmap bytes covering changed bits are published. block_write retains
 * its partial-sector read/modify/write protection for neighbouring bits/sectors.
 * The owner still holds the same mutation, FAT ordering and flush boundaries. */
static bool write_bitmap_span(struct exfat_volume *volume, uint64_t offset,
    const uint8_t *bitmap, uint64_t first_bit, uint64_t last_bit) {
    uint64_t bytes=allocation_bitmap_bytes(volume);
    if (!bytes || first_bit>last_bit || last_bit/8>=bytes) return false;
#if defined(SRVROS_EXFAT_HOST_TEST) && defined(SRVROS_EXFAT_BITMAP_SPAN_OFF)
    return volume_write(volume,offset,bitmap,volume->cluster_size);
#else
    uint64_t first=first_bit/8, last=last_bit/8;
    return volume_write(volume,offset+first,bitmap+first,last-first+1);
#endif
}

static bool allocate_contiguous_clusters_impl(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster) {
    if (volume->allocation_bitmap_cluster < 2 || count == 0 || first_cluster == NULL) {
        return false;
    }

    uint8_t *bitmap = kmalloc(volume->cluster_size);
    if (bitmap == NULL) {
        return false;
    }

    uint64_t bitmap_offset;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) ||
        !volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        kfree(bitmap);
        return false;
    }

    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_clusters = bitmap_bytes * 8;
    if (count > bitmap_clusters) {
        kfree(bitmap);
        return false;
    }

    uint64_t run_start = 0;
    uint64_t run_length = 0;
    for (uint64_t index = 0; index < volume->cluster_count && index < bitmap_clusters; index++) {
        if (!bitmap_bit_is_set(bitmap, index)) {
            if (run_length == 0) {
                run_start = index;
            }
            run_length++;
            if (run_length == count) {
                for (uint64_t i = 0; i < count; i++) bitmap_set_bit(bitmap, run_start + i);
                uint32_t first = (uint32_t)(run_start + 2);
                if (!fill_fat_range(volume, first, count, 0xffffffffu) ||
                    !write_bitmap_span(volume,bitmap_offset,bitmap,run_start,run_start+count-1)) {
                    /* A failed write blocks further mutation, as before. The
                     * dirty volume must be recovered before it can be reused. */
                    kfree(bitmap);
                    return false;
                }
                *first_cluster = (uint32_t)(run_start + 2);
                kfree(bitmap);
                return true;
            }
        } else {
            run_length = 0;
        }
    }

    kfree(bitmap);
    return false;
}

static bool allocate_contiguous_clusters(struct exfat_volume *volume, uint64_t count, uint32_t *first_cluster) {
    enum exfat_io_phase previous=volume->io_phase;volume->io_phase=EXFAT_IO_ALLOCATE;
    bool ok=allocate_contiguous_clusters_impl(volume,count,first_cluster);
    volume->io_phase=previous;return ok;
}

static void clear_fat_entries(struct exfat_volume *volume, const uint32_t *clusters, uint64_t count) {
    uint8_t fat_entry[4];
    write_u32(fat_entry, 0);
    for (uint64_t i = 0; i < count; i++) {
        (void)volume_write(volume,
            volume->fat_offset_bytes + (uint64_t)clusters[i] * sizeof(uint32_t),
            fat_entry,
            sizeof(fat_entry));
    }
}

static bool allocate_fragmented_clusters_impl(struct exfat_volume *volume,
    uint64_t count,
    uint32_t *first_cluster,
    bool *no_fat_chain) {
    if (volume->allocation_bitmap_cluster < 2 ||
        count == 0 ||
        first_cluster == NULL ||
        no_fat_chain == NULL ||
        count > UINT64_MAX / sizeof(uint32_t)) {
        return false;
    }

    uint64_t cluster_array_bytes = count * sizeof(uint32_t);
    uint32_t *clusters = kmalloc(cluster_array_bytes);
    uint8_t *bitmap = kmalloc(volume->cluster_size);
    if (clusters == NULL || bitmap == NULL) {
        if (clusters != NULL) {
            kfree(clusters);
        }
        if (bitmap != NULL) {
            kfree(bitmap);
        }
        return false;
    }

    uint64_t bitmap_offset;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) ||
        !volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        kfree(clusters);
        kfree(bitmap);
        return false;
    }

    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_clusters = bitmap_bytes * 8;
    uint64_t found = 0;
    for (uint64_t index = 0; index < volume->cluster_count && index < bitmap_clusters && found < count; index++) {
        if (!bitmap_bit_is_set(bitmap, index)) {
            clusters[found++] = (uint32_t)(index + 2);
            bitmap_set_bit(bitmap, index);
        }
    }
    if (found != count) {
        kfree(clusters);
        kfree(bitmap);
        return false;
    }

    for (uint64_t i = 0; i < count; i++) {
        uint32_t next = (i + 1 < count) ? clusters[i + 1] : 0xffffffffu;
        if (!write_fat_entry(volume, clusters[i], next)) {
            clear_fat_entries(volume, clusters, i);
            kfree(clusters);
            kfree(bitmap);
            return false;
        }
    }
    if (!write_bitmap_span(volume,bitmap_offset,bitmap,clusters[0]-2,clusters[count-1]-2)) {
        clear_fat_entries(volume, clusters, count);
        kfree(clusters);
        kfree(bitmap);
        return false;
    }

    *first_cluster = clusters[0];
    *no_fat_chain = false;
    kfree(clusters);
    kfree(bitmap);
    return true;
}

static bool allocate_fragmented_clusters(struct exfat_volume *volume, uint64_t count,
    uint32_t *first_cluster, bool *no_fat_chain) {
    enum exfat_io_phase previous=volume->io_phase;volume->io_phase=EXFAT_IO_ALLOCATE;
    bool ok=allocate_fragmented_clusters_impl(volume,count,first_cluster,no_fat_chain);
    volume->io_phase=previous;return ok;
}

static bool allocate_clusters(struct exfat_volume *volume,
    uint64_t count,
    uint32_t *first_cluster,
    bool *no_fat_chain) {
    if (first_cluster == NULL || no_fat_chain == NULL || count > volume->cluster_count) {
        return false;
    }
    if (count == 0) {
        *first_cluster = 0;
        *no_fat_chain = true;
        return true;
    }
    if (allocate_contiguous_clusters(volume, count, first_cluster)) {
        *no_fat_chain = true;
        return true;
    }
    return !volume->write_blocked && allocate_fragmented_clusters(volume, count, first_cluster, no_fat_chain);
}

static bool free_file_clusters_impl(struct exfat_file *file) {
    if (file == NULL ||
        file->volume == NULL ||
        file->volume->allocation_bitmap_cluster < 2 ||
        file->first_cluster < 2 ||
        file->allocated_clusters == 0) {
        return true;
    }

    /* Published exact aliases own the allocation until their last name retires.
     * Callers remove/switch the retiring record before releasing its old extent. */
    for (uint64_t m = 0; m < EXFAT_MAX_VOLUMES; m++) {
        const struct exfat_mount *mount = &mounts[m];
        if (!mount->used || &mount->volume != file->volume) continue;
        for (uint64_t i = 0; i < mount->file_count; i++) {
            const struct exfat_file *live = file_slot(mount, i);
            if (path_slot(mount, i)[0] && live->first_cluster == file->first_cluster &&
                live->allocated_clusters == file->allocated_clusters &&
                live->no_fat_chain == file->no_fat_chain) return true;
        }
    }

    struct exfat_volume *volume = file->volume;
    uint8_t *bitmap = kmalloc(volume->cluster_size);
    if (bitmap == NULL) {
        return false;
    }

    uint64_t bitmap_offset;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) ||
        !volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        kfree(bitmap);
        return false;
    }

    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_clusters = bitmap_bytes * 8;
    uint32_t cluster = file->first_cluster;
    uint64_t first_bit=UINT64_MAX,last_bit=0;
    for (uint64_t i = 0; i < file->allocated_clusters; i++) {
        if (cluster < 2 || cluster >= volume->cluster_count + 2) {
            kfree(bitmap);
            return false;
        }

        uint32_t next = cluster + 1;
        if (!file->no_fat_chain && i + 1 < file->allocated_clusters) {
            if (!next_cluster(volume, cluster, &next)) {
                kfree(bitmap);
                return false;
            }
        }

        uint64_t bit_index = cluster - 2;
        if (bit_index < bitmap_clusters) {
            bitmap_clear_bit(bitmap, bit_index);
            if(bit_index<first_bit)first_bit=bit_index;
            if(bit_index>last_bit)last_bit=bit_index;
        }

        if (!file->no_fat_chain && !write_fat_entry(volume, cluster, 0)) {
            kfree(bitmap);
            return false;
        }

        if (file->no_fat_chain) {
            cluster++;
        } else {
            cluster = next;
        }
    }

    if (file->no_fat_chain && !fill_fat_range(volume, file->first_cluster,
            file->allocated_clusters, 0)) {
        kfree(bitmap);
        return false;
    }
    bool ok = write_bitmap_span(volume,bitmap_offset,bitmap,first_bit,last_bit);
    kfree(bitmap);
    return ok;
}

static bool free_file_clusters(struct exfat_file *file) {
    struct exfat_volume *volume=file?file->volume:NULL;
    enum exfat_io_phase previous=volume?volume->io_phase:EXFAT_IO_METADATA;
    if(volume)volume->io_phase=EXFAT_IO_FREE;
    bool ok=free_file_clusters_impl(file);
    if(volume)volume->io_phase=previous;
    return ok;
}

static bool root_entry_is_free(uint8_t entry_type) {
    return entry_type == EXFAT_ENTRY_END || (entry_type & 0x80u) == 0;
}

static void release_created_clusters(struct exfat_volume *volume, uint32_t first_cluster, uint64_t cluster_count);

static bool scan_directory_cluster_for_free(const uint8_t *cluster_buffer,
    uint64_t cluster_size,
    uint64_t needed_entries,
    uint64_t *offset_out) {
    if (!needed_entries || needed_entries > cluster_size / EXFAT_ENTRY_SIZE) return false;
    uint64_t run_start = 0, run_length = 0;
    for (uint64_t offset = 0; offset + EXFAT_ENTRY_SIZE <= cluster_size; offset += EXFAT_ENTRY_SIZE) {
        if (root_entry_is_free(cluster_buffer[offset])) {
            if (!run_length) run_start = offset;
            if (cluster_buffer[offset] == EXFAT_ENTRY_END) {
                if (needed_entries * EXFAT_ENTRY_SIZE > cluster_size - run_start) return false;
                *offset_out = run_start;
                return true;
            }
            if (++run_length == needed_entries) {
                *offset_out = run_start;
                return true;
            }
        } else run_length = 0;
    }

    return false;
}

/* Prepare only the prefix that will actually be published. Allocation
 * exhaustion must not retire END markers. Bytes following END are undefined,
 * so clear them when consuming the marker rather than resurrecting stale names. */
static bool prepare_directory_prefix(struct exfat_volume *volume,
    const struct exfat_directory *directory,uint32_t target,uint64_t slot,
    uint64_t entries,uint8_t *buffer) {
    uint32_t cluster=directory->first_cluster;
    bool after_end=false;
    for(uint64_t visited=0;visited<volume->cluster_count;visited++) {
        uint64_t base;
        if(!cluster_offset(volume,cluster,&base) ||
            !volume_read(volume,base,buffer,volume->cluster_size))return false;
        for(uint64_t end=0;end<volume->cluster_size;end+=EXFAT_ENTRY_SIZE) {
            if(!after_end && buffer[end]!=EXFAT_ENTRY_END)continue;
            after_end=true;
            if(cluster!=target || slot+entries*EXFAT_ENTRY_SIZE>end) {
                bool changed=false;
                uint8_t fill=cluster==target?0:0x05;
                for(uint64_t i=end;i<volume->cluster_size;i++) {
                    if(buffer[i]!=fill)changed=true;
                    buffer[i]=fill;
                }
                if(changed && (!volume_write(volume,base+end,buffer+end,volume->cluster_size-end) ||
                    !volume_flush(volume)))return false;
            }
            break;
        }
        if(cluster==target)return true;
        if(directory->no_fat_chain) {
            if(visited+1==directory->allocated_clusters)return target==0;
            cluster++;
        } else {
            if(!next_cluster(volume,cluster,&cluster))return false;
            if(cluster>=EXFAT_FAT_CHAIN_END)return target==0;
        }
    }
    return false;
}

static bool directory_free_offset(struct exfat_volume *volume,
    struct exfat_directory *directory,
    uint64_t needed_entries,
    uint64_t *offset_out) {
    if (volume == NULL || directory == NULL || needed_entries == 0 || needed_entries > volume->cluster_size / EXFAT_ENTRY_SIZE || offset_out == NULL) {
        return false;
    }

    uint8_t *cluster_buffer = kmalloc(volume->cluster_size);
    if (cluster_buffer == NULL) {
        return false;
    }

    uint32_t cluster = directory->first_cluster;
    uint32_t previous = 0;
    uint64_t visited = 0;
    bool after_end = false;
    while (cluster >= 2 && cluster < EXFAT_FAT_CHAIN_END && visited++ < volume->cluster_count) {
        uint64_t cluster_base;
        if (!cluster_offset(volume, cluster, &cluster_base) ||
            !volume_read(volume, cluster_base, cluster_buffer, volume->cluster_size)) {
            kfree(cluster_buffer);
            return false;
        }

        if(after_end)for(uint64_t i=0;i<volume->cluster_size;i++)cluster_buffer[i]=0;
        uint64_t free_offset;
        if (scan_directory_cluster_for_free(cluster_buffer,
                volume->cluster_size,
                needed_entries,
                &free_offset)) {
            bool ok=prepare_directory_prefix(volume,directory,cluster,free_offset,needed_entries,cluster_buffer);
            if(ok)*offset_out = cluster_base + free_offset;
            kfree(cluster_buffer);
            return ok;
        }

        for(uint64_t i=0;i<volume->cluster_size;i+=EXFAT_ENTRY_SIZE)
            if(cluster_buffer[i]==EXFAT_ENTRY_END) {after_end=true;break;}
        previous = cluster;
        if (directory->no_fat_chain) {
            if (visited >= directory->allocated_clusters) {
                cluster = EXFAT_FAT_CHAIN_END;
            } else {
                cluster++;
            }
        } else if (!next_cluster(volume, cluster, &cluster)) {
            kfree(cluster_buffer);
            return false;
        }
    }

    if (previous < 2 || needed_entries * EXFAT_ENTRY_SIZE > volume->cluster_size) {
        kfree(cluster_buffer);
        return false;
    }
    uint32_t new_cluster = 0;
    if (!allocate_contiguous_clusters(volume, 1, &new_cluster)) {
        kfree(cluster_buffer);
        return false;
    }
    for (uint64_t i = 0; i < volume->cluster_size; i++) cluster_buffer[i] = 0;
    uint64_t new_cluster_base;
    if (!cluster_offset(volume, new_cluster, &new_cluster_base) ||
        !volume_write(volume, new_cluster_base, cluster_buffer, volume->cluster_size) ||
        !volume_flush(volume)) {
        release_created_clusters(volume, new_cluster, 1);
        kfree(cluster_buffer);
        return false;
    }
    if(!prepare_directory_prefix(volume,directory,0,0,0,cluster_buffer)) {
        release_created_clusters(volume,new_cluster,1);kfree(cluster_buffer);return false;
    }
    /* A contiguous directory can grow into a fragmented chain. Materialize its
     * old links before publishing the stream's NoFatChain transition. */
    if (directory->no_fat_chain) {
        for (uint64_t i = 0; i < directory->allocated_clusters; i++) {
            uint32_t c = directory->first_cluster + i;
            uint32_t next = i + 1 == directory->allocated_clusters ? new_cluster : c + 1;
            if (!write_fat_entry(volume, c, next)) { kfree(cluster_buffer); return false; }
        }
    } else if (!write_fat_entry(volume, previous, new_cluster)) {
        kfree(cluster_buffer); return false;
    }
    if (!volume_flush(volume)) { kfree(cluster_buffer); return false; }
    if (directory->file_entry_offset != 0) {
        struct exfat_file descriptor = {
            .volume = volume, .first_cluster = directory->first_cluster,
            .file_entry_offset = directory->file_entry_offset,
            .stream_entry_offset = directory->file_entry_offset + EXFAT_ENTRY_SIZE,
            .secondary_count = directory->secondary_count, .no_fat_chain = false,
        };
        if (!update_file_length(&descriptor, directory->path, directory->data_length + volume->cluster_size)) {
            kfree(cluster_buffer); return false;
        }
        directory->data_length += volume->cluster_size;
    }
    directory->allocated_clusters++;
    directory->no_fat_chain = false;

    *offset_out = new_cluster_base;
    kfree(cluster_buffer);
    return true;
}

static void release_created_clusters(struct exfat_volume *volume, uint32_t first_cluster, uint64_t cluster_count) {
    struct exfat_file scratch = {
        .volume = volume,
        .first_cluster = first_cluster,
        .allocated_clusters = cluster_count,
        .no_fat_chain = true,
    };
    (void)free_file_clusters(&scratch);
}

static bool register_created_file(struct exfat_mount *mount,
    const char *parent,
    uint64_t directory_offset,
    const uint8_t *entries,
    uint64_t name_entry_count) {
    const struct exfat_file_entry *file_entry = (const struct exfat_file_entry *)entries;
    const struct exfat_stream_entry *stream = (const struct exfat_stream_entry *)(entries + EXFAT_ENTRY_SIZE);
    const struct exfat_name_entry *names = (const struct exfat_name_entry *)(entries + EXFAT_ENTRY_SIZE * 2);
    return register_file(mount,
        parent,
        file_entry,
        stream,
        names,
        name_entry_count,
        directory_offset,
        directory_offset + EXFAT_ENTRY_SIZE);
}

static bool read_entry_run(struct exfat_volume *volume, uint64_t offset, uint64_t entry_count, uint8_t *out) {
    if (volume == NULL || out == NULL || entry_count == 0 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return false;
    }
    return volume_read(volume, offset, out, entry_count * EXFAT_ENTRY_SIZE);
}

static bool restore_entry_run(struct exfat_volume *volume, uint64_t offset, const uint8_t *entries, uint64_t entry_count) {
    if (volume == NULL || entries == NULL || entry_count == 0 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return false;
    }
    return volume_write(volume, offset, entries, entry_count * EXFAT_ENTRY_SIZE);
}

static bool write_deleted_entries(struct exfat_volume *volume, uint64_t file_entry_offset, uint8_t secondary_count) {
    uint64_t entry_count = (uint64_t)secondary_count + 1;
    if (entry_count < 3 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return false;
    }

    uint8_t deleted[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t byte_count = entry_count * EXFAT_ENTRY_SIZE;
    for (uint64_t i = 0; i < byte_count; i++) {
        deleted[i] = 0;
    }
    deleted[0] = EXFAT_ENTRY_FILE & 0x7f;
    deleted[EXFAT_ENTRY_SIZE] = EXFAT_ENTRY_STREAM & 0x7f;
    for (uint64_t i = 2; i < entry_count; i++) {
        deleted[EXFAT_ENTRY_SIZE * i] = EXFAT_ENTRY_NAME & 0x7f;
    }

    /* A relocated rename must persist its replacement before removing the
     * old name. This also orders earlier allocation updates before deletion. */
    if (!volume_flush(volume)) return false;
    return volume_write(volume, file_entry_offset, deleted, byte_count);
}

static bool write_inactive_entry_run(struct exfat_volume *volume, uint64_t offset, uint64_t entry_count) {
    if (entry_count == 0 || entry_count > EXFAT_MAX_FILE_ENTRIES) {
        return true;
    }
    uint8_t deleted[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t byte_count = entry_count * EXFAT_ENTRY_SIZE;
    for (uint64_t i = 0; i < byte_count; i++) {
        deleted[i] = 0;
    }
    for (uint64_t i = 0; i < entry_count; i++) {
        deleted[i * EXFAT_ENTRY_SIZE] = 0x01;
    }
    return volume_write(volume, offset, deleted, byte_count);
}

static bool write_deleted_entry_set(struct exfat_file *file) {
    return write_deleted_entries(file->volume, file->file_entry_offset, file->secondary_count);
}

static void clear_file_slot(struct exfat_mount *mount, uint64_t index) {
    path_slot(mount, index)[0] = '\0';
    uint8_t *file_bytes = (uint8_t *)file_slot(mount, index);
    for (uint64_t i = 0; i < sizeof((*file_slot(mount, index))); i++) {
        file_bytes[i] = 0;
    }

    while (mount->file_count > 0 && path_slot(mount, mount->file_count - 1)[0] == '\0') {
        mount->file_count--;
    }
}

static void clear_directory_slot(struct exfat_mount *mount, uint64_t index) {
    uint8_t *dir_bytes = (uint8_t *)dir_slot(mount, index);
    for (uint64_t i = 0; i < sizeof((*dir_slot(mount, index))); i++) {
        dir_bytes[i] = 0;
    }

    while (mount->dir_count > 0 && dir_slot(mount, mount->dir_count - 1)->path[0] == '\0') {
        mount->dir_count--;
    }
}

static bool path_is_child_of(const char *path, const char *parent) {
    if (path == NULL || parent == NULL || parent[0] == '\0') {
        return false;
    }
    uint64_t i = 0;
    while (parent[i] != '\0') {
        if (path[i] != parent[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '/';
}

static bool directory_is_empty(const struct exfat_mount *mount, const char *path) {
    for (uint64_t i = 0; i < mount->file_count; i++) {
        if (path_slot(mount, i)[0] != '\0' && path_is_child_of(path_slot(mount, i), path)) {
            return false;
        }
    }
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        if (dir_slot(mount, i)->path[0] != '\0' && path_is_child_of(dir_slot(mount, i)->path, path)) {
            return false;
        }
    }
    return true;
}

static bool build_file_entry_set(uint8_t *entries,
    const char *name,
    uint32_t first_cluster,
    uint64_t data_length,
    uint16_t attributes,
    bool no_fat_chain,
    uint64_t *entry_count_out,
    uint64_t *name_entry_count_out) {
    uint64_t name_length = ascii_length(name);
    uint64_t name_entry_count = (name_length + 14) / 15;
    uint64_t entry_count = 2 + name_entry_count;
    if (name_length == 0 ||
        name_length > 255 ||
        entry_count > EXFAT_MAX_FILE_ENTRIES ||
        entry_count_out == NULL ||
        name_entry_count_out == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < EXFAT_ENTRY_SIZE * entry_count; i++) {
        entries[i] = 0;
    }

    uint8_t *primary = entries;
    uint8_t *stream = entries + EXFAT_ENTRY_SIZE;

    primary[0] = EXFAT_ENTRY_FILE;
    primary[1] = (uint8_t)(entry_count - 1);
    write_u16(primary + 4, attributes);

    stream[0] = EXFAT_ENTRY_STREAM;
    stream[1] = no_fat_chain && data_length != 0 ? 0x03 : 0x01;
    stream[3] = (uint8_t)name_length;
    write_u64(stream + 8, data_length);
    write_u32(stream + 20, first_cluster);
    write_u64(stream + 24, data_length);

    /* exFAT's mandatory ASCII up-case mapping; do not emit unsupported UTF-8
     * bytes as UTF-16 or invent hashes for non-ASCII names. */
    uint16_t name_hash = 0;
    for (uint64_t i = 0; i < name_length; i++) {
        uint8_t c = (uint8_t)name[i];
        if (c < 0x20 || c >= 0x7f || c == '"' || c == '*' || c == '/' ||
            c == ':' || c == '<' || c == '>' || c == '?' || c == '\\' || c == '|') return false;
        uint8_t upper = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
        name_hash = (uint16_t)((name_hash >> 1) | (name_hash << 15));
        name_hash = (uint16_t)(name_hash + upper);
        name_hash = (uint16_t)((name_hash >> 1) | (name_hash << 15)); /* UTF-16 high byte is zero. */
        uint64_t entry_index = i / 15;
        uint64_t slot = i % 15;
        uint8_t *name_entry = entries + EXFAT_ENTRY_SIZE * (2 + entry_index);
        name_entry[0] = EXFAT_ENTRY_NAME;
        write_u16(name_entry + 2 + slot * 2, (uint16_t)name[i]);
    }

    write_u16(stream + 4, name_hash);
    write_u16(primary + 2, entry_set_checksum(entries, entry_count));
    *entry_count_out = entry_count;
    *name_entry_count_out = name_entry_count;
    return true;
}

/* Set creation permissions before any service window after VFS publication.
 * vfs_set_metadata deliberately does not trigger a second metadata transaction;
 * the surrounding mutation persists the initialized metadata below. */
static bool initialize_mode(const char *path,uint64_t mode) {
    if(mode==UINT64_MAX) return true;
    const struct vfs_node *node=vfs_lookup(path);if(!node) return false;
    struct vfs_metadata metadata=node->metadata;
    metadata.mode=(metadata.mode&VFS_MODE_IFMT)|(mode&07777);
    return vfs_set_metadata(path,&metadata);
}
static bool create_file(const char *path, const uint8_t *data, uint64_t size,uint64_t mode) {
    const char *name = NULL;
    struct exfat_directory *parent = NULL;
    struct exfat_mount *mount = find_mount_and_parent_directory(path, &parent, &name);
    if (mount == NULL ||
        parent == NULL ||
        name == NULL ||
        !mount_has_file_slot(mount) ||
        mount->volume.image != NULL ||
        vfs_lookup(path) != NULL) {
        return false;
    }

    uint64_t cluster_count = clusters_for_size(&mount->volume, size);
    uint32_t first_cluster = 0;
    bool no_fat_chain = true;
    if (!allocate_clusters(&mount->volume, cluster_count, &first_cluster, &no_fat_chain)) {
        return false;
    }

    struct exfat_file scratch = {
        .volume = &mount->volume,
        .first_cluster = first_cluster,
        .size = size,
        .valid_size = size,
        .allocated_clusters = cluster_count,
        .no_fat_chain = no_fat_chain,
    };
    if (!write_file_data(&scratch, data, size)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    /* Publish a new name only after its allocation and contents are durable. */
    if (!volume_flush(&mount->volume)) return false;

    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries, name, first_cluster, size, 0x20, no_fat_chain, &entry_count, &name_entry_count)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    uint64_t directory_offset;
    if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, directory_offset, entry_count, old_entries)) {
        (void)free_file_clusters(&scratch);
        return false;
    }

    if (!volume_write(&mount->volume,
            directory_offset,
            entries,
            entry_count * EXFAT_ENTRY_SIZE)) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        (void)free_file_clusters(&scratch);
        return false;
    }

    if (!register_created_file(mount, parent->path, directory_offset, entries, name_entry_count)) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        (void)free_file_clusters(&scratch);
        return false;
    }
    if(!initialize_mode(path,mode)) return false;
    if (!metadata_path_is_internal(mount, path)) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

static bool exfat_create_directory_impl(const char *path,uint64_t mode) {
    const char *name = NULL;
    struct exfat_directory *parent = NULL;
    struct exfat_mount *mount = find_mount_and_parent_directory(path, &parent, &name);
    if (mount == NULL ||
        parent == NULL ||
        name == NULL ||
        used_dirs(mount) >= EXFAT_MAX_DIRS ||
        mount->volume.image != NULL ||
        vfs_lookup(path) != NULL) {
        return false;
    }

    uint32_t first_cluster = 0;
    if (!allocate_contiguous_clusters(&mount->volume, 1, &first_cluster)) {
        return false;
    }

    uint8_t *empty_directory = kcalloc(mount->volume.cluster_size, 1);
    if (empty_directory == NULL) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    struct exfat_file scratch = {
        .volume = &mount->volume,
        .first_cluster = first_cluster,
        .size = mount->volume.cluster_size,
        .valid_size = mount->volume.cluster_size,
        .allocated_clusters = 1,
        .no_fat_chain = true,
    };
    if (!write_file_data(&scratch, empty_directory, mount->volume.cluster_size)) {
        kfree(empty_directory);
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }
    kfree(empty_directory);
    if (!volume_flush(&mount->volume)) return false;

    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries,
            name,
            first_cluster,
            mount->volume.cluster_size,
            EXFAT_ATTR_DIRECTORY,
            true,
            &entry_count,
            &name_entry_count)) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }
    (void)name_entry_count;

    uint64_t directory_offset;
    if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, directory_offset, entry_count, old_entries)) {
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    if (!volume_write(&mount->volume,
            directory_offset,
            entries,
            entry_count * EXFAT_ENTRY_SIZE)) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }

    bool ok = register_directory(mount,
        path,
        first_cluster,
        true,
        mount->volume.cluster_size,
        directory_offset,
        (uint8_t)(entry_count - 1));
    if (!ok) {
        (void)restore_entry_run(&mount->volume, directory_offset, old_entries, entry_count);
        release_created_clusters(&mount->volume, first_cluster, 1);
        return false;
    }
    if(!initialize_mode(path,mode)) return false;
    if (ok && !metadata_path_is_internal(mount, path)) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

static void clear_mount(struct exfat_mount *mount) {
    if(mount->block_claim)block_claim_release(mount->block_claim);
    for(uint64_t i=0;i<mount->file_capacity/EXFAT_NODE_BLOCK;i++) kfree(mount->file_blocks[i]);
    for(uint64_t i=0;i<mount->dir_capacity/EXFAT_NODE_BLOCK;i++) kfree(mount->dir_blocks[i]);
    kfree(mount->metadata_buffer);
    uint8_t *bytes = (uint8_t *)mount;
    for (uint64_t i = 0; i < sizeof(*mount); i++) {
        bytes[i] = 0;
    }
}

static struct exfat_mount *alloc_mount(void) {
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mounts[i].used) {
            clear_mount(&mounts[i]);
            mounts[i].used = true;
            return &mounts[i];
        }
    }
    return NULL;
}

void exfat_print_mounts(void) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mounts[i].used) {
            continue;
        }
        count++;
        const char *device_name = "image";
        bool writable = false;
        if (mounts[i].volume.device != NULL) {
            device_name = mounts[i].volume.device->name;
            writable = mounts[i].volume.device->write != NULL && !mounts[i].volume.write_blocked;
        }
        console_printf("exfat: %s device=%s files=%u cluster=%u write=%s\n",
            mounts[i].mountpoint,
            device_name,
            mounts[i].file_count,
            mounts[i].volume.cluster_size,
            writable ? "yes" : "no");
    }
    if (count == 0) {
        console_write("exfat: no mounts\n");
    }
}

static void check_error(struct exfat_check_result *result, const char *mountpoint, const char *message) {
    if (result != NULL) {
        result->errors++;
    }
    console_printf("exfat-check: %s: %s\n", mountpoint != NULL ? mountpoint : "?", message);
}

static void check_file_error(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const char *path,
    const char *message) {
    if (result != NULL) {
        result->errors++;
    }
    console_printf("exfat-check: %s: %s: %s\n",
        mount != NULL ? mount->mountpoint : "?",
        path != NULL && path[0] != '\0' ? path : "?",
        message);
}

static bool mark_checked_cluster(uint8_t *seen, uint64_t seen_bytes, uint64_t bit_index) {
    if (bit_index / 8 >= seen_bytes) {
        return false;
    }
    uint8_t mask = (uint8_t)(1u << (bit_index % 8));
    bool already_seen = (seen[bit_index / 8] & mask) != 0;
    seen[bit_index / 8] |= mask;
    return !already_seen;
}

static bool checked_cluster_seen(const uint8_t *seen, uint64_t seen_bytes, uint64_t bit_index) {
    if (bit_index / 8 >= seen_bytes) {
        return false;
    }
    return (seen[bit_index / 8] & (uint8_t)(1u << (bit_index % 8))) != 0;
}

static void check_cluster_allocation(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const char *path,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    uint32_t cluster,
    bool allow_duplicate) {
    uint64_t bit_index = cluster - 2;
    if (bit_index >= bitmap_bits || !bitmap_bit_is_set(bitmap, bit_index)) {
        result->fat_bitmap_mismatches++;
        check_file_error(result, mount, path, "cluster not marked allocated in bitmap");
    }
    if (!mark_checked_cluster(seen, seen_bytes, bit_index) && !allow_duplicate) {
        check_file_error(result, mount, path, "duplicate cluster allocation");
    }
    result->clusters_checked++;
}

static bool file_has_existing_alias(const struct exfat_mount *mount, uint64_t file_index) {
    const struct exfat_file *file = file_slot(mount, file_index);
    for (uint64_t i = 0; i < file_index; i++) {
        const struct exfat_file *other = file_slot(mount, i);
        if (path_slot(mount, i)[0] != '\0' &&
            other->first_cluster == file->first_cluster &&
            other->size == file->size && other->valid_size == file->valid_size &&
            other->no_fat_chain == file->no_fat_chain &&
            other->allocated_clusters == file->allocated_clusters) {
            return true;
        }
    }
    return false;
}

static void check_file_clusters(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    uint64_t file_index) {
    const struct exfat_file *file = file_slot(mount, file_index);
    const char *path = path_slot(mount, file_index);
    uint64_t expected_clusters = clusters_for_size(&mount->volume, file->size);
    uint32_t cluster = file->first_cluster;
    bool alias = file_has_existing_alias(mount, file_index);

    if (path[0] == '\0') {
        return;
    }
    result->files_checked++;

    if (file->volume != &mount->volume) {
        check_file_error(result, mount, path, "volume pointer mismatch");
        return;
    }
    if (file->allocated_clusters != expected_clusters) {
        check_file_error(result, mount, path, "allocated cluster count mismatch");
    }
    if (expected_clusters == 0) {
        return;
    }
    if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
        check_file_error(result, mount, path, "first cluster out of range");
        return;
    }

    for (uint64_t i = 0; i < expected_clusters; i++) {
        if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
            check_file_error(result, mount, path, "cluster chain left volume");
            return;
        }

        check_cluster_allocation(result, mount, path, bitmap, bitmap_bits, seen, seen_bytes, cluster, alias);

        if (i + 1 >= expected_clusters) {
            if (!file->no_fat_chain) {
                uint32_t next = 0;
                if (!next_cluster(file->volume, cluster, &next)) {
                    check_file_error(result, mount, path, "could not read FAT chain");
                } else if (next < EXFAT_FAT_CHAIN_END) {
                    check_file_error(result, mount, path, "FAT chain longer than file allocation");
                }
            }
            break;
        }
        if (file->no_fat_chain) {
            cluster++;
        } else if (!next_cluster(file->volume, cluster, &cluster)) {
            check_file_error(result, mount, path, "could not read FAT chain");
            return;
        }
    }
}

static void check_directory_clusters(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    uint64_t directory_index) {
    const struct exfat_directory *directory = dir_slot(mount, directory_index);
    const char *path = directory->path;
    uint32_t cluster = directory->first_cluster;
    bool follow_to_end = directory->file_entry_offset == 0 &&
        directory->data_length == 0 &&
        !directory->no_fat_chain;
    uint64_t expected_clusters = follow_to_end ? mount->volume.cluster_count : directory->allocated_clusters;

    if (path[0] == '\0' || expected_clusters == 0) {
        return;
    }
    if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
        check_file_error(result, mount, path, "directory first cluster out of range");
        return;
    }

    for (uint64_t i = 0; i < expected_clusters; i++) {
        if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
            check_file_error(result, mount, path, "directory chain left volume");
            return;
        }

        check_cluster_allocation(result, mount, path, bitmap, bitmap_bits, seen, seen_bytes, cluster, false);

        if (directory->no_fat_chain) {
            if (i + 1 >= expected_clusters) {
                break;
            }
            cluster++;
            continue;
        }

        uint32_t next = 0;
        if (!next_cluster(&mount->volume, cluster, &next)) {
            check_file_error(result, mount, path, "could not read directory FAT chain");
            return;
        }
        if (follow_to_end) {
            if (next >= EXFAT_FAT_CHAIN_END) {
                break;
            }
            cluster = next;
            continue;
        }
        if (i + 1 >= expected_clusters) {
            if (next < EXFAT_FAT_CHAIN_END) {
                check_file_error(result, mount, path, "directory FAT chain longer than allocation");
            }
            break;
        }
        cluster = next;
    }
}

static void check_system_cluster_range(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    uint8_t *seen,
    uint64_t seen_bytes,
    const char *label,
    uint32_t first_cluster,
    uint64_t byte_length) {
    if (first_cluster < 2 || byte_length == 0) {
        return;
    }
    uint64_t clusters = clusters_for_size(&mount->volume, byte_length);
    uint32_t cluster = first_cluster;
    for (uint64_t i = 0; i < clusters; i++) {
        if (cluster < 2 || cluster >= mount->volume.cluster_count + 2) {
            check_file_error(result, mount, label, "system cluster out of range");
            return;
        }
        check_cluster_allocation(result, mount, label, bitmap, bitmap_bits, seen, seen_bytes, cluster, false);
        uint32_t next;
        if (!next_cluster(&mount->volume,cluster,&next) ||
            (i+1==clusters && next<EXFAT_FAT_CHAIN_END)) {
            check_file_error(result,mount,label,"invalid system FAT chain");return;
        }
        cluster=next;
    }
}

static void check_bitmap_leaks(struct exfat_check_result *result,
    const struct exfat_mount *mount,
    const uint8_t *bitmap,
    uint64_t bitmap_bits,
    const uint8_t *seen,
    uint64_t seen_bytes) {
    uint64_t leak_count = 0;
    uint64_t first_leak = 0;
    uint64_t limit = mount->volume.cluster_count < bitmap_bits ? mount->volume.cluster_count : bitmap_bits;
    for (uint64_t bit = 0; bit < limit; bit++) {
        if (bitmap_bit_is_set(bitmap, bit) && !checked_cluster_seen(seen, seen_bytes, bit)) {
            if (leak_count == 0) {
                first_leak = bit + 2;
            }
            leak_count++;
        }
    }
    if (leak_count != 0) {
        result->leaked_clusters += leak_count;
        result->errors++;
        console_printf("exfat-check: %s: leaked allocated clusters=%u first=%u\n",
            mount != NULL ? mount->mountpoint : "?",
            leak_count,
            first_leak);
    }
}

/* exFAT 5.1/7.1.5: the allocation bitmap, not the FAT, defines whether
 * a cluster is allocated. Linux deletion and exfatprogs recovery can leave
 * stale FAT entries in bitmap-free clusters. Validate every referenced cluster
 * against the bitmap above, and detect unreferenced allocated clusters below;
 * do not interpret an unreferenced free cluster's old FAT value as ownership. */
static bool check_mount(struct exfat_mount *mount, struct exfat_check_result *result) {
    struct exfat_volume *volume = &mount->volume;
    uint64_t bitmap_offset = 0;
    uint64_t bitmap_bytes = allocation_bitmap_bytes(volume);
    uint64_t bitmap_bits = bitmap_bytes * 8;
    uint64_t seen_bytes = (volume->cluster_count + 7) / 8;
    uint8_t *bitmap = NULL;
    uint8_t *seen = NULL;
    bool ok = true;

    result->mounts_checked++;
    if (volume->cluster_size == 0 || volume->cluster_count == 0) {
        check_error(result, mount->mountpoint, "invalid cluster geometry");
        return false;
    }
    if (volume->allocation_bitmap_cluster < 2) {
        check_error(result, mount->mountpoint, "missing allocation bitmap");
        return false;
    }
    if (volume->allocation_bitmap_size > volume->cluster_size) {
        check_error(result,mount->mountpoint,"multi-cluster bitmap checking unsupported");
        return false;
    }
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) || bitmap_bytes == 0) {
        check_error(result, mount->mountpoint, "allocation bitmap out of range");
        return false;
    }

    bitmap = kmalloc(volume->cluster_size);
    seen = kcalloc(seen_bytes == 0 ? 1 : seen_bytes, 1);
    if (bitmap == NULL || seen == NULL) {
        check_error(result, mount->mountpoint, "out of memory");
        ok = false;
        goto out;
    }
    if (!volume_read(volume, bitmap_offset, bitmap, volume->cluster_size)) {
        check_error(result, mount->mountpoint, "could not read allocation bitmap");
        ok = false;
        goto out;
    }

    check_system_cluster_range(result,
        mount,
        bitmap,
        bitmap_bits,
        seen,
        seen_bytes,
        "/allocation-bitmap",
        volume->allocation_bitmap_cluster,
        bitmap_bytes);
    check_system_cluster_range(result,
        mount,
        bitmap,
        bitmap_bits,
        seen,
        seen_bytes,
        "/upcase-table",
        volume->upcase_table_cluster,
        volume->upcase_table_size);
    for (uint64_t i = 0; i < mount->dir_count; i++) {
        check_directory_clusters(result, mount, bitmap, bitmap_bits, seen, seen_bytes, i);
    }
    for (uint64_t i = 0; i < mount->file_count; i++) {
        if (path_slot(mount, i)[0] != '\0') {
            check_file_clusters(result, mount, bitmap, bitmap_bits, seen, seen_bytes, i);
        }
    }
    check_bitmap_leaks(result, mount, bitmap, bitmap_bits, seen, seen_bytes);

    ok = result->errors == 0;

out:
    if (bitmap != NULL) {
        kfree(bitmap);
    }
    if (seen != NULL) {
        kfree(seen);
    }
    return ok;
}

static bool exclusive_range(const char *path,uint64_t bytes,bool single,
    struct block_device **device,uint64_t *offset) {
    uint64_t index;
    if(!path||!device||!offset||!bytes)return false;
    struct exfat_mount *mount=find_mount_and_index_for_file(path,&index);
    if(!mount)return false;
    struct exfat_file *file=file_slot(mount, index);
    struct exfat_volume *v=&mount->volume;
    if(!v->device||v->image||v->write_blocked||v->mutation_depth||v->flags||
       file->volume!=v||file->size!=bytes||!file->allocated_clusters||
       (single&&(file->allocated_clusters!=1||v->cluster_size<bytes))||
       !v->sector_size||bytes%v->sector_size)return false;
    struct exfat_stream_entry stream;
    if(!volume_read(v,file->stream_entry_offset,&stream,sizeof(stream))||
       stream.valid_data_length!=bytes||stream.data_length!=bytes)return false;
    /* The general checker permits VFS aliases. A raw scratch lease must not. */
    for(uint64_t i=0;i<mount->file_count;i++)
        if(i!=index && path_slot(mount, i)[0] && file_slot(mount, i)->first_cluster==file->first_cluster)return false;
    struct exfat_check_result result={0};
    uint64_t start;
    if(!check_mount(mount,&result)||!cluster_offset(v,file->first_cluster,&start)||
       start%v->sector_size)return false;
    if(!file->no_fat_chain) {
        for(uint64_t i=1;i<file->allocated_clusters;i++) {
            uint32_t next;
            if(!next_cluster(v,file->first_cluster+(uint32_t)i-1,&next)||
               next!=file->first_cluster+i)return false;
        }
    }
    *device=v->device;*offset=start;return true;
}
bool exfat_scratch_range(const char *path,struct block_device **device,uint64_t *offset) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    bool ok=exclusive_range(path,4096,true,device,offset);fs_leave(&scope);return ok;
}
bool exfat_exclusive_range(const char *path,uint64_t bytes,
    struct block_device **device,uint64_t *offset) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    bool ok=exclusive_range(path,bytes,false,device,offset);fs_leave(&scope);return ok;
}

static bool check_impl(const char *mountpoint, struct exfat_check_result *result) {
    struct exfat_check_result local = {0};
    struct exfat_check_result *out = result != NULL ? result : &local;
    uint8_t *bytes = (uint8_t *)out;
    bool any = false;

    for (uint64_t i = 0; i < sizeof(*out); i++) {
        bytes[i] = 0;
    }

    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (!mount_matches(&mounts[i], mountpoint)) {
            continue;
        }
        any = true;
        (void)check_mount(&mounts[i], out);
    }

    if (!any) {
        out->errors++;
        console_printf("exfat-check: no mounted exFAT volume%s%s\n",
            mountpoint != NULL && mountpoint[0] != '\0' ? ": " : "",
            mountpoint != NULL && mountpoint[0] != '\0' ? mountpoint : "");
        return false;
    }
    return out->errors == 0;
}

bool exfat_check(const char *mountpoint,struct exfat_check_result *result) {
    struct fs_scope scope;if(!fs_enter(&scope)) {
        if(result)*result=(struct exfat_check_result){.errors=1};
        return false;
    }
    bool ok=check_impl(mountpoint,result);fs_leave(&scope);return ok;
}

void exfat_check_print(const char *mountpoint) {
    struct exfat_check_result result;
    bool ok = exfat_check(mountpoint, &result);
    console_printf("exfat-check: mounts=%u files=%u clusters=%u leaks=%u fat=%u errors=%u %s\n",
        result.mounts_checked,
        result.files_checked,
        result.clusters_checked,
        result.leaked_clusters,
        result.fat_bitmap_mismatches,
        result.errors,
        ok ? "ok" : "failed");
}

bool exfat_is_writable(const char *mountpoint) {
    if (!mountpoint) return false;
    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        struct exfat_mount *mount = &mounts[i];
        if (mount->used && streq(mount->mountpoint, mountpoint)) {
            struct block_device *parent = block_parent(mount->volume.device);
            return !mount->volume.write_blocked && block_device_online(mount->volume.device) &&
                mount->volume.device->write && parent && parent->flush;
        }
    }
    return false;
}

static bool unmount_impl(const char *mountpoint) {
    if (!valid_mountpoint(mountpoint)) {
        return false;
    }

    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        struct exfat_mount *mount = &mounts[i];
        if (!mount->used || !streq(mount->mountpoint, mountpoint)) {
            continue;
        }

        uint64_t removed = vfs_unregister_prefix(mountpoint);
        console_printf("exfat: unmounted %s removed=%u\n", mountpoint, removed);
        clear_mount(mount);
        return true;
    }

    return false;
}

bool exfat_unmount(const char *mountpoint) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    bool ok=fs_operations==1 && valid_mountpoint(mountpoint) &&
        !process_has_open_vfs_prefix(mountpoint) && unmount_impl(mountpoint);
    fs_leave(&scope);return ok;
}

static bool exfat_write_file_impl(const char *path, const uint8_t *data, uint64_t size,uint64_t mode) {
    if (path == NULL || (data == NULL && size != 0)) {
        return false;
    }

    if (find_file(path) != NULL) {
        return overwrite_file(path, data, size);
    }
    return create_file(path, data, size,mode);
}

static bool exfat_delete_file_impl(const char *path) {
    uint64_t index = 0;
    struct exfat_mount *mount = find_mount_and_index_for_file(path, &index);
    if (mount == NULL || mount->volume.image != NULL) {
        return false;
    }
    bool internal_path = metadata_path_is_internal(mount, path);

    struct exfat_file *file = file_slot(mount, index);
    struct exfat_file old_file = *file;
    uint64_t entry_count = (uint64_t)file->secondary_count + 1;
    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, file->file_entry_offset, entry_count, old_entries)) {
        return false;
    }

    if (!write_deleted_entry_set(file)) {
        return false;
    }

    if (!volume_flush(&mount->volume)) return false;
    if (!vfs_unregister_path(path)) {
        (void)restore_entry_run(&mount->volume, old_file.file_entry_offset, old_entries, entry_count);
        return false;
    }

    clear_file_slot(mount, index);
    if (!free_file_clusters(&old_file)) return false;
    if (!internal_path) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

static bool exfat_delete_directory_impl(const char *path) {
    uint64_t index = 0;
    struct exfat_mount *mount = find_mount_and_index_for_directory(path, &index);
    if (mount == NULL ||
        mount->volume.image != NULL ||
        streq(path, mount->mountpoint) ||
        !directory_is_empty(mount, path)) {
        return false;
    }

    struct exfat_directory *directory = dir_slot(mount, index);
    bool internal_path = metadata_path_is_internal(mount, path);
    struct exfat_directory old_directory = *directory;
    uint64_t entry_count = (uint64_t)directory->secondary_count + 1;
    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, directory->file_entry_offset, entry_count, old_entries)) {
        return false;
    }

    if (!write_deleted_entries(&mount->volume,
            directory->file_entry_offset,
            directory->secondary_count)) {
        return false;
    }

    struct exfat_file scratch = {
        .volume = &mount->volume,
        .first_cluster = directory->first_cluster,
        .allocated_clusters = directory->allocated_clusters,
        .no_fat_chain = directory->no_fat_chain,
    };
    if (!volume_flush(&mount->volume)) return false;
    if (!vfs_unregister_path(path)) {
        (void)restore_entry_run(&mount->volume, old_directory.file_entry_offset, old_entries, entry_count);
        return false;
    }
    clear_directory_slot(mount, index);
    if (!free_file_clusters(&scratch)) return false;
    if (!internal_path) {
        (void)exfat_sync_mount_metadata(mount);
    }
    return true;
}

static bool rename_file_in_mount(struct exfat_mount *mount,
    uint64_t index,
    const char *old_path,
    const char *new_path) {
    struct exfat_directory *parent = NULL;
    const char *name = NULL;
    struct vfs_metadata metadata;
    bool have_metadata = vfs_stat(old_path, &metadata, NULL, NULL);
    struct exfat_mount *new_mount = find_mount_and_parent_directory(new_path, &parent, &name);
    if (new_mount != mount || parent == NULL || name == NULL || vfs_lookup(new_path) != NULL) {
        return false;
    }

    struct exfat_directory *old_parent=NULL;
    const char *old_name=NULL;
    if(find_mount_and_parent_directory(old_path,&old_parent,&old_name)!=mount || !old_parent) return false;
    struct exfat_file *file = file_slot(mount, index);
    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries,
            name,
            file->first_cluster,
            file->size,
            0x20,
            file->no_fat_chain,
            &entry_count,
            &name_entry_count)) {
        return false;
    }

    /* Renaming does not initialize the allocation's unwritten tail. */
    write_u64(entries + EXFAT_ENTRY_SIZE + 8, file->valid_size);
    write_u16(entries + 2, entry_set_checksum(entries, entry_count));

    uint64_t old_entry_count = (uint64_t)file->secondary_count + 1;
    uint64_t old_directory_offset = file->file_entry_offset;
    uint64_t directory_offset = file->file_entry_offset;
    /* Entry width alone says nothing about its parent directory. Moving even
     * a shorter name must allocate a record in the destination directory. */
    bool relocate=parent!=old_parent || entry_count>old_entry_count;
    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint8_t displaced_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, old_directory_offset, old_entry_count, old_entries)) {
        return false;
    }
    if (relocate) {
        if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
            return false;
        }
        if (!read_entry_run(&mount->volume, directory_offset, entry_count, displaced_entries)) {
            return false;
        }
    } else if (!read_entry_run(&mount->volume, directory_offset, old_entry_count, displaced_entries)) {
        return false;
    }

    if (!volume_write(&mount->volume, directory_offset, entries, entry_count * EXFAT_ENTRY_SIZE)) {
        if (relocate) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    if (!relocate && entry_count < old_entry_count) {
        uint64_t extra_offset = directory_offset + entry_count * EXFAT_ENTRY_SIZE;
        if (!write_inactive_entry_run(&mount->volume,
                extra_offset,
                old_entry_count - entry_count)) {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
            return false;
        }
    } else if (relocate) {
        if (!write_deleted_entry_set(file)) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            return false;
        }
    }

    if (!vfs_unregister_path(old_path)) {
        if (relocate) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    char old_path_copy[EXFAT_MAX_PATH];
    copy_cstr(old_path_copy, sizeof(old_path_copy), path_slot(mount, index));
    uint64_t old_file_entry_offset = file->file_entry_offset;
    uint64_t old_stream_entry_offset = file->stream_entry_offset;
    uint8_t old_secondary_count = file->secondary_count;
    file->file_entry_offset = directory_offset;
    file->stream_entry_offset = directory_offset + EXFAT_ENTRY_SIZE;
    file->secondary_count = (uint8_t)(entry_count - 1);
    copy_cstr(path_slot(mount, index), EXFAT_MAX_PATH, new_path);
    bool ok = vfs_register_file_with_metadata(path_slot(mount, index),
        file->size,
        file,
        exfat_read_all,
        exfat_release_data,
        have_metadata ? &metadata : NULL);
    if (ok) {
        (void)exfat_sync_mount_metadata(mount);
        return true;
    }

    if (relocate) {
        (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    } else {
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    }
    file->file_entry_offset = old_file_entry_offset;
    file->stream_entry_offset = old_stream_entry_offset;
    file->secondary_count = old_secondary_count;
    copy_cstr(path_slot(mount, index), EXFAT_MAX_PATH, old_path_copy);
    (void)vfs_register_file_with_metadata(path_slot(mount, index),
        file->size,
        file,
        exfat_read_all,
        exfat_release_data,
        have_metadata ? &metadata : NULL);
    return false;
}

static bool rename_empty_directory_in_mount(struct exfat_mount *mount,
    uint64_t index,
    const char *old_path,
    const char *new_path) {
    if (!directory_is_empty(mount, old_path)) {
        return false;
    }

    struct exfat_directory *parent = NULL;
    const char *name = NULL;
    struct vfs_metadata metadata;
    bool have_metadata = vfs_stat(old_path, &metadata, NULL, NULL);
    struct exfat_mount *new_mount = find_mount_and_parent_directory(new_path, &parent, &name);
    if (new_mount != mount || parent == NULL || name == NULL || vfs_lookup(new_path) != NULL) {
        return false;
    }

    struct exfat_directory *old_parent=NULL;
    const char *old_name=NULL;
    if(find_mount_and_parent_directory(old_path,&old_parent,&old_name)!=mount || !old_parent) return false;
    struct exfat_directory *directory = dir_slot(mount, index);
    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t entry_count;
    uint64_t name_entry_count;
    if (!build_file_entry_set(entries,
            name,
            directory->first_cluster,
            directory->data_length,
            EXFAT_ATTR_DIRECTORY,
            directory->no_fat_chain,
            &entry_count,
            &name_entry_count)) {
        return false;
    }
    (void)name_entry_count;

    uint64_t old_entry_count = (uint64_t)directory->secondary_count + 1;
    uint64_t old_directory_offset = directory->file_entry_offset;
    uint64_t directory_offset = directory->file_entry_offset;
    /* Entry width alone says nothing about its parent directory. Moving even
     * a shorter name must allocate a record in the destination directory. */
    bool relocate=parent!=old_parent || entry_count>old_entry_count;
    uint8_t old_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint8_t displaced_entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    if (!read_entry_run(&mount->volume, old_directory_offset, old_entry_count, old_entries)) {
        return false;
    }
    if (relocate) {
        if (!directory_free_offset(&mount->volume, parent, entry_count, &directory_offset)) {
            return false;
        }
        if (!read_entry_run(&mount->volume, directory_offset, entry_count, displaced_entries)) {
            return false;
        }
    } else if (!read_entry_run(&mount->volume, directory_offset, old_entry_count, displaced_entries)) {
        return false;
    }

    if (!volume_write(&mount->volume, directory_offset, entries, entry_count * EXFAT_ENTRY_SIZE)) {
        if (relocate) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    if (!relocate && entry_count < old_entry_count) {
        uint64_t extra_offset = directory_offset + entry_count * EXFAT_ENTRY_SIZE;
        if (!write_inactive_entry_run(&mount->volume,
                extra_offset,
                old_entry_count - entry_count)) {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
            return false;
        }
    } else if (relocate) {
        if (!write_deleted_entries(&mount->volume,
                old_directory_offset,
                directory->secondary_count)) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            return false;
        }
    }

    if (!vfs_unregister_path(old_path)) {
        if (relocate) {
            (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        } else {
            (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
        }
        return false;
    }
    char old_path_copy[EXFAT_MAX_PATH];
    copy_cstr(old_path_copy, sizeof(old_path_copy), directory->path);
    uint64_t old_file_entry_offset = directory->file_entry_offset;
    uint8_t old_secondary_count = directory->secondary_count;
    directory->file_entry_offset = directory_offset;
    directory->secondary_count = (uint8_t)(entry_count - 1);
    copy_cstr(directory->path, sizeof(directory->path), new_path);
    bool ok = vfs_register_directory_with_metadata(directory->path, have_metadata ? &metadata : NULL);
    if (ok) {
        (void)exfat_sync_mount_metadata(mount);
        return true;
    }

    if (relocate) {
        (void)restore_entry_run(&mount->volume, directory_offset, displaced_entries, entry_count);
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    } else {
        (void)restore_entry_run(&mount->volume, old_directory_offset, old_entries, old_entry_count);
    }
    directory->file_entry_offset = old_file_entry_offset;
    directory->secondary_count = old_secondary_count;
    copy_cstr(directory->path, sizeof(directory->path), old_path_copy);
    (void)vfs_register_directory_with_metadata(directory->path, have_metadata ? &metadata : NULL);
    return false;
}

static bool exfat_rename_impl(const char *old_path, const char *new_path) {
    if (old_path == NULL ||
        new_path == NULL ||
        old_path[0] == '\0' ||
        new_path[0] == '\0' ||
        vfs_lookup(new_path) != NULL) {
        return false;
    }

    uint64_t index = 0;
    struct exfat_mount *mount = find_mount_and_index_for_file(old_path, &index);
    if (mount != NULL && mount->volume.image == NULL) {
        return rename_file_in_mount(mount, index, old_path, new_path);
    }

    mount = find_mount_and_index_for_directory(old_path, &index);
    if (mount != NULL && mount->volume.image == NULL && !streq(old_path, mount->mountpoint)) {
        return rename_empty_directory_in_mount(mount, index, old_path, new_path);
    }
    return false;
}

static bool statfs_impl(const char *path, struct exfat_fsinfo *info) {
    if (path == NULL || info == NULL) {
        return false;
    }

    struct exfat_mount *mount = find_mount_for_path(path);
    if (mount == NULL ||
        mount->volume.cluster_size == 0 ||
        mount->volume.cluster_count == 0 ||
        mount->volume.allocation_bitmap_cluster < 2) {
        return false;
    }

    struct exfat_volume *volume = &mount->volume;
    uint64_t bitmap_offset = 0;
    uint64_t bitmap_bytes = volume->allocation_bitmap_size;
    if (!cluster_offset(volume, volume->allocation_bitmap_cluster, &bitmap_offset) || bitmap_bytes == 0) {
        return false;
    }

    uint8_t *buffer = kmalloc(volume->cluster_size);
    if (buffer == NULL) {
        return false;
    }

    uint64_t used_clusters = 0;
    uint64_t bit_base = 0;
    uint64_t remaining_bytes = bitmap_bytes;
    uint64_t offset = bitmap_offset;
    uint32_t cluster = volume->allocation_bitmap_cluster;
    while (remaining_bytes > 0 && bit_base < volume->cluster_count) {
        uint64_t chunk = remaining_bytes < volume->cluster_size ? remaining_bytes : volume->cluster_size;
        if (!volume_read(volume, offset, buffer, chunk)) {
            kfree(buffer);
            return false;
        }

        uint64_t bits = chunk * 8;
        if (bits > volume->cluster_count - bit_base) {
            bits = volume->cluster_count - bit_base;
        }
        used_clusters += count_used_bitmap_bits(buffer, 0, bits);
        bit_base += bits;
        remaining_bytes -= chunk;
        if (remaining_bytes && (!next_cluster(volume,cluster,&cluster) ||
            !cluster_offset(volume,cluster,&offset))) { kfree(buffer);return false; }
    }
    kfree(buffer);

    if (used_clusters > volume->cluster_count) {
        used_clusters = volume->cluster_count;
    }

    for (uint64_t i = 0; i < sizeof(*info); i++) {
        ((uint8_t *)info)[i] = 0;
    }
    info->block_size = volume->cluster_size;
    info->blocks = volume->cluster_count;
    info->blocks_free = volume->cluster_count - used_clusters;
    info->blocks_available = info->blocks_free;
    info->files = used_files(mount) + used_dirs(mount);
    info->files_free = EXFAT_MAX_FILES + EXFAT_MAX_DIRS - info->files;

    const char *fs = "exfat";
    uint64_t i = 0;
    while (fs[i] != '\0' && i + 1 < sizeof(info->filesystem)) {
        info->filesystem[i] = fs[i];
        i++;
    }
    info->filesystem[i] = '\0';

    i = 0;
    while (mount->mountpoint[i] != '\0' && i + 1 < sizeof(info->mountpoint)) {
        info->mountpoint[i] = mount->mountpoint[i];
        i++;
    }
    info->mountpoint[i] = '\0';
    return true;
}

bool exfat_statfs(const char *path,struct exfat_fsinfo *info) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    bool ok=statfs_impl(path,info);fs_leave(&scope);return ok;
}

/* Validate untrusted geometry before shifts, allocation, scanning or writes.
 * Only the single-FAT format is supported; TexFAT needs active-table handling.
 * Use the declared volume extent, not any trailing bytes on its backing device. */
static bool volume_from_boot(const struct exfat_boot_sector *boot, uint64_t backing_size,
    struct exfat_volume *volume) {
    if (boot->bytes_per_sector_shift < 9 || boot->bytes_per_sector_shift > 12 ||
        boot->sectors_per_cluster_shift > 25 - boot->bytes_per_sector_shift ||
        boot->number_of_fats != 1 || (boot->volume_flags & 1) ||
        boot->filesystem_revision != 0x100) return false;
    uint64_t sector = 1ull << boot->bytes_per_sector_shift;
    uint64_t sectors_per_cluster = 1ull << boot->sectors_per_cluster_shift;
    uint64_t count = boot->cluster_count;
    uint64_t fat_end = (uint64_t)boot->fat_offset + boot->fat_length;
    uint64_t heap_end = (uint64_t)boot->cluster_heap_offset + count * sectors_per_cluster;
    if (boot->volume_length < (1u << 20) / sector ||
        boot->volume_length > backing_size / sector ||
        !count || count > 0xfffffff5u || boot->root_directory_cluster < 2 ||
        boot->root_directory_cluster >= count + 2 || boot->fat_offset < 24 ||
        (uint64_t)boot->fat_length * sector < (count + 2) * sizeof(uint32_t) ||
        fat_end > boot->cluster_heap_offset || heap_end > boot->volume_length) return false;
    *volume = (struct exfat_volume){
        .image_size = boot->volume_length * sector,
        .sector_size = sector, .sectors_per_cluster = sectors_per_cluster,
        .cluster_size = sector * sectors_per_cluster,
        .fat_offset_bytes = (uint64_t)boot->fat_offset * sector,
        .cluster_heap_offset_bytes = (uint64_t)boot->cluster_heap_offset * sector,
        .cluster_count = boot->cluster_count, .root_directory_cluster = boot->root_directory_cluster,
        .flags = boot->volume_flags, .write_blocked = (boot->volume_flags & 6u) != 0,
    };
    return true;
}

/* Validate complete boot regions in bounded stack chunks. Geometry is not
 * trusted until its checksum (every repeated checksum word) has been checked. */
static bool read_boot_region(struct exfat_volume *source,uint64_t base,
    unsigned expected_shift,struct exfat_boot_sector *boot,struct exfat_volume *volume) {
    if (!volume_read(source,base,boot,sizeof(*boot)) ||
        boot->bytes_per_sector_shift!=expected_shift ||
        !streqn((const char *)boot->jump_boot,"\xeb\x76\x90",3) ||
        !streqn(boot->filesystem_name,"EXFAT   ",8) || boot->boot_signature!=0xaa55 ||
        (boot->percent_in_use>100 && boot->percent_in_use!=255) ||
        !volume_from_boot(boot,source->image_size,volume)) return false;
    for (unsigned i=0;i<sizeof(boot->must_be_zero);i++) if(boot->must_be_zero[i])return false;
    uint64_t sector=volume->sector_size;
    if (base>source->image_size || 12*sector>source->image_size-base) return false;
    uint32_t sum=0;
    for (uint64_t off=0;off<12*sector;off+=512) {
        uint8_t buffer[512];
        if(off==0) {
            /* Checksum the exact header used for geometry, not a later read. */
            const uint8_t *header=(const void *)boot;
            for(unsigned i=0;i<sizeof(buffer);i++)buffer[i]=header[i];
        } else if(!volume_read(source,base+off,buffer,sizeof(buffer)))return false;
        if(off<11*sector) {
            for(unsigned i=0;i<512;i++) {
                uint64_t at=off+i;
                if(at==106 || at==107 || at==112)continue;
                sum=(sum>>1)|(sum<<31);sum+=buffer[i];
            }
            uint64_t number=off/sector;
            if(number>=1 && number<=8 && (off+512)%sector==0 &&
                read_u32(buffer+508)!=0xaa550000)return false;
        } else {
            for(unsigned i=0;i<512;i+=4)if(read_u32(buffer+i)!=sum)return false;
        }
    }
    return true;
}

static bool same_boot_geometry(const struct exfat_boot_sector *a,const struct exfat_boot_sector *b) {
    const uint8_t *x=(const void *)a,*y=(const void *)b;
    for(unsigned i=64;i<=110;i++)if(i!=106 && i!=107 && x[i]!=y[i])return false;
    return true;
}

static bool select_boot(struct exfat_volume *source,struct exfat_volume *volume) {
    struct exfat_boot_sector header,main,backup;
    struct exfat_volume primary,secondary;
    bool have_main=volume_read(source,0,&header,sizeof(header)) &&
        header.bytes_per_sector_shift>=9 && header.bytes_per_sector_shift<=12 &&
        read_boot_region(source,0,header.bytes_per_sector_shift,&main,&primary);
    if(have_main) {
        bool have_backup=read_boot_region(source,12*primary.sector_size,
            main.bytes_per_sector_shift,&backup,&secondary);
        *volume=primary;
        /* Backup flags are stale. Only a valid primary can authorize writes;
         * damaged or conflicting backups require offline inspection. */
        if(!have_backup || !same_boot_geometry(&main,&backup))volume->write_blocked=true;
        return true;
    }
    unsigned found=0;
    /* Do not trust a damaged primary's sector-size byte to locate its backup. */
    for(unsigned shift=9;shift<=12;shift++) {
        if(read_boot_region(source,12ull*(1ull<<shift),shift,&backup,&secondary)) {
            *volume=secondary;found++;
        }
    }
    if(found!=1)return false;
    volume->write_blocked=true;
    return true; /* Read-only fallback, without repairing or replaying metadata. */
}

static bool mount_image_impl(const char *mountpoint, const uint8_t *image, uint64_t image_size) {
    if (!valid_mountpoint(mountpoint) || image == NULL || image_size < sizeof(struct exfat_boot_sector)) {
        return false;
    }
    if (mountpoint_in_use(mountpoint)) {
        console_printf("exfat: mountpoint busy: %s\n", mountpoint);
        return false;
    }

    struct exfat_volume source={.image=image,.image_size=image_size},volume;
    if (!select_boot(&source,&volume)) return false;
    struct exfat_mount *mount = alloc_mount();
    if (mount == NULL) {
        console_write("exfat: mount table full\n");
        return false;
    }

    mount->volume = volume;
    mount->volume.image = image;
    remember_mountpoint(mount, mountpoint);

    if (!scan_root_directory(mount)) {
        console_write("exfat: failed to scan root directory\n");
        vfs_unregister_prefix(mountpoint);
        clear_mount(mount);
        return false;
    }
    load_mount_metadata(mount);
    vfs_set_metadata_changed_callback(exfat_metadata_changed);

    console_printf("exfat: mounted %s files=%u cluster=%u bytes\n",
        mountpoint,
        mount->file_count,
        mount->volume.cluster_size);
    return true;
}

static bool mount_block_device(const char *mountpoint, struct block_device *device, uint64_t claim) {
    uint64_t size = block_device_size(device);
    if (!valid_mountpoint(mountpoint) || device == NULL || size < sizeof(struct exfat_boot_sector)) {
        return false;
    }
    if (mountpoint_in_use(mountpoint)) {
        console_printf("exfat: mountpoint busy: %s\n", mountpoint);
        return false;
    }

    for (uint64_t i = 0; i < EXFAT_MAX_VOLUMES; i++) {
        if (mounts[i].used && mounts[i].volume.device == device) {
            console_write("exfat: device already mounted\n");
            return false;
        }
    }

    struct exfat_volume source={.device=device,.image_size=size},volume;
    if (!select_boot(&source,&volume)) return false;
    struct exfat_mount *mount = alloc_mount();
    if (mount == NULL) {
        console_write("exfat: mount table full\n");
        return false;
    }

    mount->volume = volume;
    mount->volume.device = device;
    mount->block_claim = claim;
    remember_mountpoint(mount, mountpoint);

    if (!scan_root_directory(mount)) {
        console_write("exfat: failed to scan block root directory\n");
        vfs_unregister_prefix(mountpoint);
        clear_mount(mount);
        return false;
    }
    uint64_t bitmap_needed = ((uint64_t)mount->volume.cluster_count + 7) / 8;
    if (mount->volume.allocation_bitmap_size != bitmap_needed ||
        bitmap_needed > mount->volume.cluster_size) mount->volume.write_blocked = true;
    /* Before sidecar recovery or any user mutation, establish allocation
     * ownership. Unsupported or inconsistent media remains available read-only. */
    if (!mount->volume.write_blocked) {
        struct exfat_check_result check = {0};
        if (!check_mount(mount, &check)) mount->volume.write_blocked = true;
    }
    if (mount->volume.write_blocked)
        console_printf("exfat: %s requires filesystem recovery or unsupported bitmap; writes disabled\n", mountpoint);
    load_mount_metadata(mount);
    vfs_set_metadata_changed_callback(exfat_metadata_changed);

    console_printf("exfat: mounted %s from %s files=%u cluster=%u bytes\n",
        mountpoint,
        device->name,
        mount->file_count,
        mount->volume.cluster_size);
    return true;
}

bool exfat_mount_image(const char *mountpoint,const uint8_t *image,uint64_t size) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    bool ok=mount_image_impl(mountpoint,image,size);fs_leave(&scope);return ok;
}
bool exfat_mount_block_device(const char *mountpoint, struct block_device *device) {
    struct fs_scope scope;if(!fs_enter(&scope))return false;
    uint64_t claim=block_claim_mount(device);
    bool ok=claim && mount_block_device(mountpoint,device,claim);
    if(claim && !ok)(void)block_claim_release(claim);
    fs_leave(&scope);return ok;
}

/* The supported entry set must fit one cluster. Reject impossible requests
 * before dirty marking or scratch allocation, including an oversized payload. */
static bool mutation_shape(const char *path,uint64_t size) {
    struct exfat_mount *mount=path?find_mount_for_path(path):NULL;
    if(!mount || size>(uint64_t)mount->volume.cluster_count*mount->volume.cluster_size)return false;
    const char *name=path;
    for(const char *p=path;*p;p++)if(*p=='/')name=p+1;
    return (2+(ascii_length(name)+14)/15)*EXFAT_ENTRY_SIZE<=mount->volume.cluster_size;
}

/* Public mutation boundaries. Failed or interrupted metadata updates are never
 * silently declared clean; recovery is performed offline with a filesystem checker. */
static bool write_file_mode_result(const char *path, const uint8_t *data, uint64_t size,
    uint64_t mode, bool *retryable) {
    if (retryable) *retryable = false;
    if(mode!=UINT64_MAX && (mode&~07777ull)) return false;
    if (!valid_ascii_basename(path) || !mutation_shape(path,size) || (!data && size) ||
        (!find_file(path) && name_conflicts(path, NULL))) return false;
    struct mutation_scope scope;
    if (!mutation_begin_result(find_mount_for_path(path), &scope, path, find_file(path)?0:1, 0, retryable,"write")) return false;
    return mutation_end_result(&scope, mutation_capacity(find_mount_for_path(path), path,
        find_file(path) ? 0 : 1, 0) && exfat_write_file_impl(path, data, size,mode), retryable);
}
bool exfat_write_file(const char *path, const uint8_t *data, uint64_t size) {
    return write_file_mode_result(path,data,size,UINT64_MAX,NULL);
}
bool exfat_write_file_retryable(const char *path, const uint8_t *data, uint64_t size, bool *retryable) {
    return write_file_mode_result(path,data,size,UINT64_MAX,retryable);
}
bool exfat_write_file_mode(const char *path, const uint8_t *data, uint64_t size,uint64_t mode) {
    return write_file_mode_result(path,data,size,mode,NULL);
}
bool exfat_create_directory(const char *path) {
    return exfat_create_directory_mode(path,UINT64_MAX);
}
bool exfat_create_directory_mode(const char *path,uint64_t mode) {
    if(mode!=UINT64_MAX && (mode&~07777ull)) return false;
    if (!valid_ascii_basename(path) || !mutation_shape(path,0) || name_conflicts(path, NULL)) return false;
    struct mutation_scope scope;
    if (!mutation_begin(find_mount_for_path(path), &scope, path, 0, 1,"mkdir")) return false;
    return mutation_end(&scope, mutation_capacity(find_mount_for_path(path), path, 0, 1) &&
        exfat_create_directory_impl(path,mode));
}
bool exfat_delete_file(const char *path) {
    if (!path) return false;
    struct mutation_scope scope;
    if (!mutation_begin(find_mount_for_path(path), &scope, path, 0, 0,"delete-file")) return false;
    return mutation_end(&scope, mutation_capacity(find_mount_for_path(path), path, 0, 0) &&
        exfat_delete_file_impl(path));
}
bool exfat_delete_directory(const char *path) {
    if (!path) return false;
    struct mutation_scope scope;
    if (!mutation_begin(find_mount_for_path(path), &scope, path, 0, 0,"delete-directory")) return false;
    return mutation_end(&scope, mutation_capacity(find_mount_for_path(path), path, 0, 0) &&
        exfat_delete_directory_impl(path));
}
bool exfat_rename(const char *old_path, const char *new_path) {
    if (!old_path || !valid_ascii_basename(new_path) || !mutation_shape(new_path,0) || name_conflicts(new_path, old_path)) return false;
    struct mutation_scope scope;
    if (!mutation_begin(find_mount_for_path(old_path), &scope, old_path, 0, 0,"rename")) return false;
    return mutation_end(&scope, mutation_capacity(find_mount_for_path(old_path), old_path, 0, 0) &&
        exfat_rename_impl(old_path, new_path));
}

/* Replace the destination directory record with the source's allocation, then
 * retire the source record. This is serialized runtime rename, not a journal:
 * interrupted media stays dirty/quarantined and needs offline recovery. Keep
 * both original records until publication so a recoverable write error can
 * restore them. Never implement replacement by deleting the target first. */
bool exfat_replace_file(const char *old_path,const char *new_path,bool *published) {
    if(!published) return false;
    *published=false;
    if(!old_path || !new_path) return false;
    uint64_t source_index=0,target_index=0;
    struct exfat_mount *mount=find_mount_and_index_for_file(old_path,&source_index);
    if(!mount || mount!=find_mount_and_index_for_file(new_path,&target_index) ||
        source_index==target_index || !valid_ascii_basename(new_path)) return false;
    struct exfat_file *source=file_slot(mount,source_index), *target=file_slot(mount,target_index);
    const char *name=new_path;for(const char *p=new_path;*p;p++) if(*p=='/') name=p+1;
    uint8_t entries[EXFAT_ENTRY_SIZE*EXFAT_MAX_FILE_ENTRIES];
    uint8_t old_source[EXFAT_ENTRY_SIZE*EXFAT_MAX_FILE_ENTRIES];
    uint8_t old_target[EXFAT_ENTRY_SIZE*EXFAT_MAX_FILE_ENTRIES];
    uint64_t count,names,source_count=(uint64_t)source->secondary_count+1;
    uint64_t target_count=(uint64_t)target->secondary_count+1;
    if(!build_file_entry_set(entries,name,source->first_cluster,source->size,0x20,
        source->no_fat_chain,&count,&names) || count>target_count ||
        source_count>EXFAT_MAX_FILE_ENTRIES || target_count>EXFAT_MAX_FILE_ENTRIES) return false;
    write_u64(entries + EXFAT_ENTRY_SIZE + 8, source->valid_size);
    write_u16(entries + 2, entry_set_checksum(entries, count));
    struct mutation_scope scope;
    if(!mutation_begin(mount,&scope,old_path,0,0,"replace")) return false;
    bool ok=false;
    uint64_t source_offset=source->file_entry_offset,target_offset=target->file_entry_offset;
    if(!read_entry_run(&mount->volume,source_offset,source_count,old_source) ||
        !read_entry_run(&mount->volume,target_offset,target_count,old_target)) goto done;
    if(!volume_write(&mount->volume,target_offset,entries,count*EXFAT_ENTRY_SIZE) ||
        (count<target_count && !write_inactive_entry_run(&mount->volume,
            target_offset+count*EXFAT_ENTRY_SIZE,target_count-count)) ||
        !volume_flush(&mount->volume) || !write_deleted_entry_set(source) ||
        !volume_flush(&mount->volume)) goto restore;
    if(!vfs_replace_file_path(old_path,new_path,path_slot(mount,source_index),EXFAT_MAX_PATH)) goto restore;
    *published=true;
    struct exfat_file retired=*target;
    source->file_entry_offset=target_offset;
    source->stream_entry_offset=target_offset+EXFAT_ENTRY_SIZE;
    source->secondary_count=(uint8_t)(count-1);
    clear_file_slot(mount,target_index);
    /* Once published, an I/O error can no longer promise unchanged names.
     * Tell the process layer to reconcile open identities even on failure. */
    ok=free_file_clusters(&retired) && exfat_sync_mount_metadata(mount);
    goto done;
restore:
    (void)restore_entry_run(&mount->volume,target_offset,old_target,target_count);
    (void)restore_entry_run(&mount->volume,source_offset,old_source,source_count);
done:
    return mutation_end(&scope,ok);
}

bool exfat_same_mount(const char *first,const char *second) {
    struct exfat_mount *mount=first?find_mount_for_path(first):NULL;
    return mount && second && mount==find_mount_for_path(second);
}

/* Called by the logger before its own filesystem work; snapshot under CPU0 IRQ
 * exclusion, then format without holding that exclusion or storage ownership. */
void exfat_print_costs(void) {
    struct exfat_cost rows[EXFAT_COST_ROWS];uint64_t irq=fs_irq_save();
    for(unsigned i=0;i<EXFAT_COST_ROWS;i++)rows[i]=slow_costs[i];
    fs_irq_restore(irq);
    for(unsigned i=0;i<EXFAT_COST_ROWS;i++) {
        const struct exfat_cost *c=&rows[i];if(!c->start)continue;
        console_printf("exfat-cost: version=1 start_us=%u end_us=%u elapsed_us=%u operation=%s ok=%u\n",
            c->start,c->end,c->end-c->start,c->operation,(uint64_t)c->ok);
        for(unsigned j=0;j<EXFAT_IO_PHASE_COUNT;j++) {
            const struct exfat_cost_phase *p=&c->phase[j];
            console_printf("exfat-cost-phase: version=1 start_us=%u phase=%u reads=%u read_bytes=%u read_us=%u writes=%u write_bytes=%u write_us=%u flushes=%u flush_us=%u\n",
                c->start,(uint64_t)j,p->reads,p->read_bytes,p->read_us,p->writes,p->write_bytes,p->write_us,p->flushes,p->flush_us);
        }
    }
}
