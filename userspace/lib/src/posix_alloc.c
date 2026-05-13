#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define POSIX_HEAP_SIZE (1024 * 1024)
#define ALIGNMENT 16

struct block_header {
    size_t size;
    int free;
    struct block_header *next;
};

static unsigned char heap_area[POSIX_HEAP_SIZE];
static struct block_header *heap_head;

static size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static void heap_init(void) {
    if (heap_head != 0) {
        return;
    }
    heap_head = (struct block_header *)heap_area;
    heap_head->size = POSIX_HEAP_SIZE - sizeof(struct block_header);
    heap_head->free = 1;
    heap_head->next = 0;
}

static void split_block(struct block_header *block, size_t size) {
    if (block->size < size + sizeof(struct block_header) + ALIGNMENT) {
        return;
    }

    struct block_header *next = (struct block_header *)((unsigned char *)(block + 1) + size);
    next->size = block->size - size - sizeof(struct block_header);
    next->free = 1;
    next->next = block->next;
    block->size = size;
    block->next = next;
}

void *malloc(size_t size) {
    if (size == 0) {
        return 0;
    }
    heap_init();
    size = align_size(size);
    for (struct block_header *block = heap_head; block != 0; block = block->next) {
        if (block->free && block->size >= size) {
            split_block(block, size);
            block->free = 0;
            return block + 1;
        }
    }
    errno = ENOMEM;
    return 0;
}

static void coalesce(void) {
    for (struct block_header *block = heap_head; block != 0 && block->next != 0; block = block->next) {
        if (block->free && block->next->free) {
            block->size += sizeof(struct block_header) + block->next->size;
            block->next = block->next->next;
        }
    }
}

void free(void *ptr) {
    if (ptr == 0) {
        return;
    }
    struct block_header *block = ((struct block_header *)ptr) - 1;
    block->free = 1;
    coalesce();
}

void *calloc(size_t count, size_t size) {
    if (size != 0 && count > ((size_t)-1) / size) {
        errno = ENOMEM;
        return 0;
    }
    size_t total = count * size;
    void *ptr = malloc(total);
    if (ptr != 0) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == 0) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return 0;
    }

    struct block_header *block = ((struct block_header *)ptr) - 1;
    if (block->size >= size) {
        return ptr;
    }
    void *next = malloc(size);
    if (next == 0) {
        return 0;
    }
    memcpy(next, ptr, block->size);
    free(ptr);
    return next;
}

int atoi(const char *text) {
    return (int)atol(text);
}

int abs(int value) {
    return value < 0 ? -value : value;
}

long atol(const char *text) {
    return strtol(text, 0, 10);
}

static int digit_value(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return -1;
}

long long strtoll(const char *text, char **endptr, int base) {
    long sign = 1;
    long long value = 0;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == '-') {
        sign = -1;
        text++;
    } else if (*text == '+') {
        text++;
    }
    if ((base == 0 || base == 16) && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text += 2;
    } else if (base == 0 && text[0] == '0') {
        base = 8;
        text++;
    } else if (base == 0) {
        base = 10;
    }
    while (digit_value(*text) >= 0 && digit_value(*text) < base) {
        value = value * base + digit_value(*text);
        text++;
    }
    if (endptr != 0) {
        *endptr = (char *)text;
    }
    return value * sign;
}

long strtol(const char *text, char **endptr, int base) {
    return (long)strtoll(text, endptr, base);
}

unsigned long strtoul(const char *text, char **endptr, int base) {
    return (unsigned long)strtoll(text, endptr, base);
}

char *getenv(const char *name) {
    (void)name;
    return 0;
}

void abort(void) {
    _exit(127);
}

void exit(int status) {
    _exit(status);
}
