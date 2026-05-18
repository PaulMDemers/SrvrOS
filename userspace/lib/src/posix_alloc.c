#include <errno.h>
#include <spawn.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALIGNMENT 16
#define HEAP_CHUNK_SIZE (64 * 1024)
#define HEAP_MAX_BLOCKS 8192

struct block_header {
    size_t size;
    int free;
    struct block_header *next;
};

static struct block_header *heap_head;
static struct block_header *heap_tail;
static int heap_lock_word;

struct aligned_allocation {
    void *aligned;
    void *raw;
};

static struct aligned_allocation aligned_allocations[32];

static size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static void recompute_tail(void) {
    heap_tail = heap_head;
    for (size_t guard = 0; heap_tail != 0 && heap_tail->next != 0 && guard < HEAP_MAX_BLOCKS; guard++) {
        heap_tail = heap_tail->next;
    }
}

static void heap_lock(void) {
    int expected = 0;
    while (!__atomic_compare_exchange_n(&heap_lock_word,
            &expected,
            1,
            0,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED)) {
        expected = 0;
        (void)srv_futex_wait((uint32_t *)&heap_lock_word, 1, 0);
    }
}

static void heap_unlock(void) {
    __atomic_store_n(&heap_lock_word, 0, __ATOMIC_RELEASE);
    (void)srv_futex_wake((uint32_t *)&heap_lock_word, UINT64_MAX);
}

static void split_block(struct block_header *block, size_t size) {
    if (block->size < size + sizeof(struct block_header) + ALIGNMENT) {
        return;
    }

    int was_tail = block == heap_tail;
    struct block_header *next = (struct block_header *)((unsigned char *)(block + 1) + size);
    next->size = block->size - size - sizeof(struct block_header);
    next->free = 1;
    next->next = block->next;
    block->size = size;
    block->next = next;
    if (was_tail) {
        heap_tail = next;
    }
}

static struct block_header *extend_heap(size_t size) {
    size_t needed = align_size(size + sizeof(struct block_header));
    size_t chunk = needed < HEAP_CHUNK_SIZE ? HEAP_CHUNK_SIZE : needed;
    void *memory = sbrk((intptr_t)chunk);
    if (memory == (void *)-1) {
        errno = ENOMEM;
        return 0;
    }

    struct block_header *block = memory;
    block->size = chunk - sizeof(struct block_header);
    block->free = 1;
    block->next = 0;

    if (heap_tail != 0 && heap_tail->free &&
        (unsigned char *)(heap_tail + 1) + heap_tail->size == (unsigned char *)block) {
        heap_tail->size += sizeof(struct block_header) + block->size;
        return heap_tail;
    }

    if (heap_head == 0) {
        heap_head = block;
    } else {
        heap_tail->next = block;
    }
    heap_tail = block;
    return block;
}

void *malloc(size_t size) {
    if (size == 0) {
        return 0;
    }
    size = align_size(size);
    heap_lock();
    for (;;) {
        size_t guard = 0;
        for (struct block_header *block = heap_head; block != 0; block = block->next) {
            if (++guard > HEAP_MAX_BLOCKS ||
                (block->next != 0 && (uintptr_t)block->next <= (uintptr_t)block)) {
                errno = ENOMEM;
                heap_unlock();
                return 0;
            }
            if (!block->free || block->size < size) {
                continue;
            }
            split_block(block, size);
            block->free = 0;
            heap_unlock();
            return block + 1;
        }
        if (extend_heap(size) == 0) {
            errno = ENOMEM;
            heap_unlock();
            return 0;
        }
    }
}

static void coalesce(void) {
    size_t guard = 0;
    for (struct block_header *block = heap_head; block != 0 && block->next != 0;) {
        if (++guard > HEAP_MAX_BLOCKS || (uintptr_t)block->next <= (uintptr_t)block) {
            return;
        }
        unsigned char *block_end = (unsigned char *)(block + 1) + block->size;
        if (block->free && block->next->free && block_end == (unsigned char *)block->next) {
            struct block_header *next = block->next;
            block->size += sizeof(struct block_header) + next->size;
            block->next = next->next;
            if (heap_tail == next) {
                heap_tail = block;
            }
            continue;
        }
        block = block->next;
    }
}

void free(void *ptr) {
    if (ptr == 0) {
        return;
    }
    heap_lock();
    for (size_t i = 0; i < sizeof(aligned_allocations) / sizeof(aligned_allocations[0]); i++) {
        if (aligned_allocations[i].aligned == ptr) {
            ptr = aligned_allocations[i].raw;
            aligned_allocations[i].aligned = 0;
            aligned_allocations[i].raw = 0;
            break;
        }
    }
    struct block_header *block = ((struct block_header *)ptr) - 1;
    block->free = 1;
    coalesce();
    recompute_tail();
    heap_unlock();
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

    heap_lock();
    struct block_header *block = ((struct block_header *)ptr) - 1;
    size_t old_size = block->size;
    if (block->size >= size) {
        heap_unlock();
        return ptr;
    }
    heap_unlock();

    void *next = malloc(size);
    if (next == 0) {
        return 0;
    }
    memcpy(next, ptr, old_size);
    free(ptr);
    return next;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    if (memptr == 0 ||
        alignment < sizeof(void *) ||
        (alignment & (alignment - 1)) != 0) {
        errno = EINVAL;
        return EINVAL;
    }
    size_t total = size + alignment - 1 + sizeof(void *);
    void *raw = malloc(total);
    if (raw == 0) {
        return ENOMEM;
    }
    uintptr_t start = (uintptr_t)raw + sizeof(void *);
    uintptr_t aligned = (start + alignment - 1) & ~(uintptr_t)(alignment - 1);
    heap_lock();
    for (size_t i = 0; i < sizeof(aligned_allocations) / sizeof(aligned_allocations[0]); i++) {
        if (aligned_allocations[i].aligned == 0) {
            aligned_allocations[i].aligned = (void *)aligned;
            aligned_allocations[i].raw = raw;
            *memptr = (void *)aligned;
            heap_unlock();
            return 0;
        }
    }
    heap_unlock();
    free(raw);
    errno = ENOMEM;
    return ENOMEM;
}

void *aligned_alloc(size_t alignment, size_t size) {
    void *ptr = 0;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return 0;
    }
    return ptr;
}

int atoi(const char *text) {
    return (int)atol(text);
}

double atof(const char *text) {
    return strtod(text, 0);
}

int abs(int value) {
    return value < 0 ? -value : value;
}

long labs(long value) {
    return value < 0 ? -value : value;
}

long long llabs(long long value) {
    return value < 0 ? -value : value;
}

div_t div(int numer, int denom) {
    div_t result = { numer / denom, numer % denom };
    return result;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t result = { numer / denom, numer % denom };
    return result;
}

lldiv_t lldiv(long long numer, long long denom) {
    lldiv_t result = { numer / denom, numer % denom };
    return result;
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
    const char *start = text;
    long sign = 1;
    long long value = 0;
    int any = 0;
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
        any = 1;
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? text : start);
    }
    return value * sign;
}

long strtol(const char *text, char **endptr, int base) {
    return (long)strtoll(text, endptr, base);
}

unsigned long strtoul(const char *text, char **endptr, int base) {
    return (unsigned long)strtoll(text, endptr, base);
}

unsigned long long strtoull(const char *text, char **endptr, int base) {
    return (unsigned long long)strtoll(text, endptr, base);
}

static double pow10_int(int exponent) {
    double value = 1.0;
    while (exponent > 0) {
        value *= 10.0;
        exponent--;
    }
    while (exponent < 0) {
        value /= 10.0;
        exponent++;
    }
    return value;
}

double strtod(const char *text, char **endptr) {
    const char *start = text;
    double sign = 1.0;
    double value = 0.0;
    int any = 0;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == '-') {
        sign = -1.0;
        text++;
    } else if (*text == '+') {
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10.0 + (double)(*text - '0');
        text++;
        any = 1;
    }
    if (*text == '.') {
        double scale = 0.1;
        text++;
        while (*text >= '0' && *text <= '9') {
            value += (double)(*text - '0') * scale;
            scale *= 0.1;
            text++;
            any = 1;
        }
    }
    if (any && (*text == 'e' || *text == 'E')) {
        const char *exponent_start = text;
        int exponent_sign = 1;
        int exponent = 0;
        int exponent_any = 0;
        text++;
        if (*text == '-') {
            exponent_sign = -1;
            text++;
        } else if (*text == '+') {
            text++;
        }
        while (*text >= '0' && *text <= '9') {
            exponent = exponent * 10 + (*text - '0');
            text++;
            exponent_any = 1;
        }
        if (exponent_any) {
            value *= pow10_int(exponent * exponent_sign);
        } else {
            text = exponent_start;
        }
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? text : start);
    }
    return value * sign;
}

float strtof(const char *text, char **endptr) {
    return (float)strtod(text, endptr);
}

static unsigned rand_state = 1;

int rand(void) {
    rand_state = rand_state * 1103515245u + 12345u;
    return (int)((rand_state / 65536u) % 32768u);
}

void srand(unsigned seed) {
    rand_state = seed == 0 ? 1 : seed;
}

static void swap_bytes(unsigned char *a, unsigned char *b, size_t size) {
    while (size-- > 0) {
        unsigned char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (base == 0 || compar == 0 || size == 0 || nmemb < 2) {
        return;
    }
    unsigned char *bytes = base;
    for (size_t i = 1; i < nmemb; i++) {
        size_t j = i;
        while (j > 0 &&
            compar(bytes + (j - 1) * size, bytes + j * size) > 0) {
            swap_bytes(bytes + (j - 1) * size, bytes + j * size, size);
            j--;
        }
    }
}

void *bsearch(const void *key,
    const void *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *)) {
    const unsigned char *bytes = base;
    size_t low = 0;
    size_t high = nmemb;
    if (key == 0 || base == 0 || compar == 0 || size == 0) {
        return 0;
    }
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void *item = bytes + mid * size;
        int cmp = compar(key, item);
        if (cmp == 0) {
            return (void *)item;
        }
        if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return 0;
}

static char *environment[32];
char **environ = environment;

static void ensure_environment_writable(void) {
    if (environ == environment) {
        return;
    }
    size_t out = 0;
    while (environ != 0 &&
        environ[out] != 0 &&
        out + 1 < sizeof(environment) / sizeof(environment[0])) {
        environment[out] = environ[out];
        out++;
    }
    environment[out] = 0;
    environ = environment;
}

static size_t name_length(const char *entry) {
    size_t length = 0;
    while (entry[length] != '\0' && entry[length] != '=') {
        length++;
    }
    return length;
}

static int env_match(const char *entry, const char *name) {
    size_t len = name_length(entry);
    return strncmp(entry, name, len) == 0 && name[len] == '\0' && entry[len] == '=';
}

char *getenv(const char *name) {
    if (name == 0 || name[0] == '\0') {
        return 0;
    }
    char **env = environ != 0 ? environ : environment;
    for (size_t i = 0; env[i] != 0; i++) {
        if (env_match(env[i], name)) {
            return env[i] + name_length(env[i]) + 1;
        }
    }
    return 0;
}

int putenv(char *string) {
    if (string == 0 || string[0] == '\0' || strchr(string, '=') == 0) {
        errno = EINVAL;
        return -1;
    }
    ensure_environment_writable();
    size_t len = name_length(string);
    for (size_t i = 0; environment[i] != 0; i++) {
        if (strncmp(environment[i], string, len) == 0 && environment[i][len] == '=') {
            environment[i] = string;
            return 0;
        }
    }
    for (size_t i = 0; i + 1 < sizeof(environment) / sizeof(environment[0]); i++) {
        if (environment[i] == 0) {
            environment[i] = string;
            environment[i + 1] = 0;
            return 0;
        }
    }
    errno = ENOMEM;
    return -1;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (name == 0 || name[0] == '\0' || strchr(name, '=') != 0) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name) != 0) {
        return 0;
    }
    size_t name_len = strlen(name);
    size_t value_len = value != 0 ? strlen(value) : 0;
    char *entry = malloc(name_len + value_len + 2);
    if (entry == 0) {
        return -1;
    }
    memcpy(entry, name, name_len);
    entry[name_len] = '=';
    if (value_len != 0) {
        memcpy(entry + name_len + 1, value, value_len);
    }
    entry[name_len + value_len + 1] = '\0';
    return putenv(entry);
}

int unsetenv(const char *name) {
    if (name == 0 || name[0] == '\0' || strchr(name, '=') != 0) {
        errno = EINVAL;
        return -1;
    }
    ensure_environment_writable();
    for (size_t i = 0; environment[i] != 0; i++) {
        if (env_match(environment[i], name)) {
            for (size_t j = i; environment[j] != 0; j++) {
                environment[j] = environment[j + 1];
            }
            i--;
        }
    }
    return 0;
}

int clearenv(void) {
    ensure_environment_writable();
    for (size_t i = 0; i < sizeof(environment) / sizeof(environment[0]); i++) {
        environment[i] = 0;
    }
    return 0;
}

static void (*atexit_handlers[16])(void);
static size_t atexit_count;

int atexit(void (*function)(void)) {
    if (function == 0 || atexit_count >= sizeof(atexit_handlers) / sizeof(atexit_handlers[0])) {
        return -1;
    }
    atexit_handlers[atexit_count++] = function;
    return 0;
}

int system(const char *command) {
    if (command == 0) {
        return 1;
    }
    pid_t child = 0;
    int status = 0;
    char *argv[] = {"sh", "-c", (char *)command, 0};
    int error = posix_spawnp(&child, "sh", 0, 0, argv, environ);
    if (error != 0) {
        errno = error;
        return -1;
    }
    if (waitpid(child, &status, 0) < 0) {
        return -1;
    }
    return status;
}

void abort(void) {
    _exit(127);
}

void exit(int status) {
    while (atexit_count > 0) {
        atexit_handlers[--atexit_count]();
    }
    _exit(status);
}
