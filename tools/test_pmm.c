#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
static bool irq_enabled = true;
static unsigned latency_depth;
static uint64_t pmm_test_irq_save(void) { bool f = irq_enabled; irq_enabled = false; return f; }
static void pmm_test_irq_restore(uint64_t f) { irq_enabled = f; }
#define SRVROS_PMM_HOST_TEST
#include "../kernel/src/memory/pmm.c"
void console_write(const char *s) { (void)s; }
void console_printf(const char *s, ...) { (void)s; }
void halt_forever(void) { abort(); }
void latency_enter(enum latency_site site) { assert(site == LATENCY_PMM); latency_depth++; }
void latency_leave(enum latency_site site) { assert(site == LATENCY_PMM && latency_depth); latency_depth--; }
#define FRAMES 32771 /* Exercise a partial final bitmap byte. */
static uint8_t bits[(FRAMES + 7) / 8];
static uint8_t reference[FRAMES];
static uint64_t naive_steps;
static uint32_t rng = 123;
static uint32_t random32(void) { rng = rng * 1664525 + 1013904223; return rng; }
static void reset(void) {
    bitmap = bits; bitmap_bytes = sizeof(bits); frame_count = FRAMES;
    free_count = 0; search_steps = naive_steps = 0;
    memset(search_hints, 0, sizeof(search_hints)); next_search_hint = 0;
    memset(bits, 0xff, sizeof(bits)); memset(reference, 1, sizeof(reference));
    memset(tag_counts, 0, sizeof(tag_counts));
    memset(frame_tags, PMM_FRAME_RESERVED, FRAMES);
    tag_counts[PMM_FRAME_RESERVED] = FRAMES;
    for (unsigned i = 256; i < FRAMES; i++) {
        bitmap_set(i, false); set_frame_tag(i, PMM_FRAME_FREE); reference[i] = 0;
    }
}
static uint64_t reference_alloc(unsigned count, uint64_t minimum) {
    unsigned run = 0;
    uint64_t start = (minimum + 4095) / 4096;
    for (uint64_t i = start; i < FRAMES; i++) {
        naive_steps++;
        run = reference[i] ? 0 : run + 1;
        if (run == count) {
            for (unsigned j = 0; j < count; j++) reference[i + 1 - count + j] = 1;
            return (i + 1 - count) * 4096;
        }
    }
    return 0;
}
static uint64_t alloc(unsigned n, uint64_t minimum) {
    uint64_t expected = reference_alloc(n, minimum);
    uint64_t got = pmm_alloc_frames_above_tagged(n, minimum, PMM_FRAME_GENERAL);
    assert(got == expected && irq_enabled && !latency_depth);
    return got;
}
static void release(uint64_t address, unsigned count) {
    pmm_free_frame_range(address, count);
    memset(reference + address / 4096, 0, count);
    uint8_t *p = pmm_phys_to_virt(address);
    for (unsigned i = 0; i < count * 4096; i++) assert(p[i] == PMM_POISON_FREE);
}
int main(void) {
    uint8_t *memory = malloc((size_t)FRAMES * 4096); assert(memory); hhdm = (uint64_t)memory;
    const uint64_t floors[] = {0x4000000, 0x2000000, 0x100000};
    for (unsigned f = 0; f < 3; f++) {
        reset();
        for (unsigned i = 0; i < 8192; i++) assert(alloc(1, floors[f]));
        assert(search_steps <= 8192 && naive_steps > 30000000);
        printf("pmm-host: floor=%llu sequential bitmap steps=%llu reference=%llu\n",
               (unsigned long long)floors[f], (unsigned long long)search_steps,
               (unsigned long long)naive_steps);
    }
    release(256 * 4096, 1); assert(alloc(1, 0x100000) == 256 * 4096);
    /* A small hole must remain available after a larger request skips it. */
    release(258 * 4096, 1); assert(alloc(2, 0x100000));
    assert(alloc(1, 0x100000) == 258 * 4096);
    reset();
    struct { uint64_t address; unsigned count; } slots[128] = {0};
    for (unsigned i = 0; i < 12000; i++) {
        unsigned slot = random32() % 128;
        if (slots[slot].address) {
            release(slots[slot].address, slots[slot].count); slots[slot].address = 0;
        } else {
            unsigned count = 1 + random32() % 64;
            uint64_t minimum = (random32() % 3) ? 0x100000 : (random32() % FRAMES) * 4096ull + 1;
            slots[slot].address = alloc(count, minimum); slots[slot].count = count;
        }
    }
    unsigned free_reference = 0;
    for (unsigned i = 0; i < FRAMES; i++) {
        assert(bitmap_get(i) == (bool)reference[i]); free_reference += !reference[i];
    }
    assert(free_reference == free_count && free_count == tag_counts[PMM_FRAME_FREE]);
    assert(!pmm_alloc_frames_above(1, UINT64_MAX));
    assert(!pmm_alloc_frames_above(1, FRAMES * 4096ull));
    assert(!pmm_alloc_frames_above(1, (FRAMES + 500) * 4096ull));
    assert(!pmm_alloc_frames_above(1, (FRAMES + 500) * 4096ull));
    assert(!pmm_alloc_frames(0)); assert(!pmm_alloc_frames(FRAMES + 1));
    irq_enabled = false; assert(pmm_alloc_frame() && !irq_enabled && !latency_depth);
    free(memory);
    puts("pmm-host: PASS (first-fit oracle, fragmentation, minimum bounds, reuse, poison, tags, IRQ restoration)");
}
