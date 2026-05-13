#include <srvros/console.h>
#include <srvros/heap.h>
#include <srvros/halt.h>
#include <srvros/pmm.h>

#include <stdbool.h>

#define HEAP_MAGIC 0x5158565248534f53ull
#define HEAP_MIN_CHUNK_SIZE (PMM_FRAME_SIZE * 4)
#define HEAP_ALIGNMENT 16

struct heap_block {
    uint64_t magic;
    uint64_t size;
    bool free;
    struct heap_block *prev;
    struct heap_block *next;
};

static struct heap_block *head;
static uint64_t total_bytes;

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static uint64_t align_up_u64(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static void *block_payload(struct heap_block *block) {
    return (void *)((uint8_t *)block + sizeof(*block));
}

static struct heap_block *payload_block(void *ptr) {
    return (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
}

static bool block_is_valid(struct heap_block *block) {
    return block != NULL && block->magic == HEAP_MAGIC;
}

static bool heap_pointer_looks_kernel(void *ptr) {
    uint64_t value = (uint64_t)ptr;
    return (value >> 47) == 0x1ffff;
}

static void heap_validate_locked(const char *where) {
    struct heap_block *previous = NULL;
    uint64_t blocks = 0;

    for (struct heap_block *block = head; block != NULL; block = block->next) {
        if (!heap_pointer_looks_kernel(block) || !block_is_valid(block)) {
            console_printf("heap: corrupt block where=%s block=%x prev=%x blocks=%u\n",
                where,
                (uint64_t)block,
                (uint64_t)previous,
                blocks);
            halt_forever();
        }
        if (block->prev != previous) {
            console_printf("heap: corrupt prev where=%s block=%x prev=%x expected=%x\n",
                where,
                (uint64_t)block,
                (uint64_t)block->prev,
                (uint64_t)previous);
            halt_forever();
        }
        if (block->next != NULL && !heap_pointer_looks_kernel(block->next)) {
            console_printf("heap: corrupt next where=%s block=%x next=%x size=%u free=%u\n",
                where,
                (uint64_t)block,
                (uint64_t)block->next,
                block->size,
                block->free ? 1 : 0);
            halt_forever();
        }
        if (block->next != NULL &&
            (block->next <= block ||
                (uint8_t *)block + sizeof(*block) + block->size > (uint8_t *)block->next)) {
            console_printf("heap: overlapping blocks where=%s block=%x size=%u next=%x\n",
                where,
                (uint64_t)block,
                block->size,
                (uint64_t)block->next);
            halt_forever();
        }
        previous = block;
        blocks++;
        if (blocks > 4096) {
            console_printf("heap: corrupt cycle where=%s block=%x\n",
                where,
                (uint64_t)block);
            halt_forever();
        }
    }
}

static void insert_block_sorted(struct heap_block *block) {
    if (head == NULL || block < head) {
        block->prev = NULL;
        block->next = head;
        if (head != NULL) {
            head->prev = block;
        }
        head = block;
        return;
    }

    struct heap_block *current = head;
    while (current->next != NULL && current->next < block) {
        current = current->next;
    }

    block->prev = current;
    block->next = current->next;
    if (current->next != NULL) {
        current->next->prev = block;
    }
    current->next = block;
}

static bool blocks_are_adjacent(struct heap_block *left, struct heap_block *right) {
    return (uint8_t *)left + sizeof(*left) + left->size == (uint8_t *)right;
}

static bool coalesce_with_next(struct heap_block *block) {
    struct heap_block *next = block->next;
    if (next == NULL || !next->free || !blocks_are_adjacent(block, next)) {
        return false;
    }

    block->size += sizeof(*next) + next->size;
    block->next = next->next;
    if (next->next != NULL) {
        next->next->prev = block;
    }
    return true;
}

static void split_block(struct heap_block *block, uint64_t wanted) {
    uint64_t required = wanted + sizeof(struct heap_block) + HEAP_ALIGNMENT;
    if (block->size < required) {
        return;
    }

    struct heap_block *rest = (struct heap_block *)((uint8_t *)block_payload(block) + wanted);
    rest->magic = HEAP_MAGIC;
    rest->size = block->size - wanted - sizeof(*rest);
    rest->free = true;
    rest->prev = block;
    rest->next = block->next;

    if (block->next != NULL) {
        block->next->prev = rest;
    }

    block->size = wanted;
    block->next = rest;
}

static struct heap_block *find_free_block(uint64_t size) {
    for (struct heap_block *block = head; block != NULL; block = block->next) {
        if (block->free && block->size >= size) {
            return block;
        }
    }

    return NULL;
}

static struct heap_block *extend_heap(uint64_t payload_size) {
    uint64_t needed = align_up_u64(payload_size + sizeof(struct heap_block), PMM_FRAME_SIZE);
    if (needed < HEAP_MIN_CHUNK_SIZE) {
        needed = HEAP_MIN_CHUNK_SIZE;
    }

    uint64_t frames = needed / PMM_FRAME_SIZE;
    uint64_t physical = pmm_alloc_frames_tagged(frames, PMM_FRAME_HEAP);
    if (physical == 0) {
        return NULL;
    }

    struct heap_block *block = pmm_phys_to_virt(physical);
    block->magic = HEAP_MAGIC;
    block->size = needed - sizeof(*block);
    block->free = true;
    block->prev = NULL;
    block->next = NULL;

    total_bytes += needed;
    insert_block_sorted(block);

    if (block->prev != NULL && block->prev->free && coalesce_with_next(block->prev)) {
        block = block->prev;
    }
    coalesce_with_next(block);

    return block;
}

void heap_init(void) {
    if (head != NULL) {
        return;
    }

    if (extend_heap(HEAP_MIN_CHUNK_SIZE - sizeof(struct heap_block)) == NULL) {
        console_write("heap: failed to reserve initial chunk\n");
        halt_forever();
    }

    console_printf("heap: initialized total=%u free=%u used=%u\n",
        heap_total_bytes(),
        heap_free_bytes(),
        heap_used_bytes());
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    uint64_t flags = irq_save();
    heap_validate_locked("kmalloc-enter");
    uint64_t wanted = align_up_u64(size, HEAP_ALIGNMENT);
    struct heap_block *block = find_free_block(wanted);
    if (block == NULL) {
        block = extend_heap(wanted);
        if (block == NULL) {
            irq_restore(flags);
            return NULL;
        }
    }

    split_block(block, wanted);
    block->free = false;
    heap_validate_locked("kmalloc-exit");
    void *payload = block_payload(block);
    irq_restore(flags);
    return payload;
}

void *kcalloc(size_t count, size_t size) {
    if (count != 0 && size > UINT64_MAX / count) {
        return NULL;
    }

    uint64_t bytes = (uint64_t)count * (uint64_t)size;
    uint8_t *ptr = kmalloc(bytes);
    if (ptr == NULL) {
        return NULL;
    }

    for (uint64_t i = 0; i < bytes; i++) {
        ptr[i] = 0;
    }

    return ptr;
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    uint64_t flags = irq_save();
    heap_validate_locked("kfree-enter");
    struct heap_block *block = payload_block(ptr);
    if (!block_is_valid(block) || block->free) {
        console_printf("heap: invalid free ptr=%x\n", (uint64_t)ptr);
        halt_forever();
    }

    block->free = true;
    coalesce_with_next(block);

    if (block->prev != NULL && block->prev->free) {
        coalesce_with_next(block->prev);
    }
    heap_validate_locked("kfree-exit");
    irq_restore(flags);
}

uint64_t heap_total_bytes(void) {
    return total_bytes;
}

uint64_t heap_free_bytes(void) {
    uint64_t total = 0;

    for (struct heap_block *block = head; block != NULL; block = block->next) {
        if (block->free) {
            total += block->size;
        }
    }

    return total;
}

uint64_t heap_used_bytes(void) {
    uint64_t used = 0;

    for (struct heap_block *block = head; block != NULL; block = block->next) {
        if (!block->free) {
            used += block->size;
        }
    }

    return used;
}
