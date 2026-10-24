#include <srvros/bootlog.h>
#include <srvros/console.h>
#include <srvros/exfat.h>

#include <stdint.h>

static char ring[BOOTLOG_CAPTURE_CAPACITY];
static char linear[BOOTLOG_CAPTURE_CAPACITY];
_Static_assert((BOOTLOG_DEFAULT_CAPACITY & (BOOTLOG_DEFAULT_CAPACITY-1))==0 &&
    (BOOTLOG_CAPTURE_CAPACITY & (BOOTLOG_CAPTURE_CAPACITY-1))==0,"log capacities must be powers of two");
static uint64_t ring_capacity=BOOTLOG_DEFAULT_CAPACITY;
static uint64_t write_index;
static uint64_t bytes_recorded;
static uint64_t total_bytes;
static bool suppress_recording;

bool bootlog_extend_capture(void) {
#ifndef SRVROS_BOOTLOG_HOST_TEST
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
    bool ok=total_bytes==bytes_recorded;
    if(ok && ring_capacity==BOOTLOG_DEFAULT_CAPACITY) {
        ring_capacity=BOOTLOG_CAPTURE_CAPACITY;
        /* At the exact old boundary write_index wrapped to zero, but no byte
         * has been lost. The preserved prefix is still contiguous. */
        write_index=bytes_recorded;
    }
#ifndef SRVROS_BOOTLOG_HOST_TEST
    if(flags&(1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
    return ok;
}

void bootlog_putc(char c) {
    if (suppress_recording) {
        return;
    }

    total_bytes++;
    ring[write_index] = c;
    write_index = (write_index + 1) & (ring_capacity - 1);
    if (bytes_recorded < ring_capacity) {
        bytes_recorded++;
    }
}

uint64_t bootlog_size(void) {
    return bytes_recorded;
}

static uint64_t oldest_index(void) {
    if (bytes_recorded < ring_capacity) {
        return 0;
    }
    return write_index;
}

static uint64_t copy_log(char *out, uint64_t capacity, uint64_t max_bytes) {
    if (out == 0 || capacity == 0) {
        return 0;
    }

    uint64_t count = bytes_recorded;
    if (max_bytes != 0 && count > max_bytes) {
        count = max_bytes;
    }
    if (count > capacity) {
        count = capacity;
    }

    uint64_t first = oldest_index();
    uint64_t skip = bytes_recorded - count;
    for (uint64_t i = 0; i < count; i++) {
        out[i] = ring[(first + skip + i) & (ring_capacity - 1)];
    }
    return count;
}

void bootlog_dump(uint64_t max_bytes) {
    uint64_t count = bytes_recorded;
    if (max_bytes != 0 && count > max_bytes) {
        count = max_bytes;
    }

    uint64_t first = oldest_index();
    uint64_t skip = bytes_recorded - count;
    suppress_recording = true;
    for (uint64_t i = 0; i < count; i++) {
        console_putc(ring[(first + skip + i) & (ring_capacity - 1)]);
    }
    if (count == 0 || ring[(first + skip + count - 1) & (ring_capacity - 1)] != '\n') {
        console_putc('\n');
    }
    suppress_recording = false;
}

bool bootlog_persist(const char *path) {
    uint64_t count = copy_log(linear, sizeof(linear), 0);
    if (count == 0) {
        return false;
    }
    return exfat_write_file(path, (const uint8_t *)linear, count);
}

/* Console writers hold IRQ exclusion. Take a coherent bounded snapshot before
 * doing storage I/O; never leave interrupts masked while writing a file. */
uint64_t bootlog_snapshot(char *out,uint64_t capacity,uint64_t *total) {
#ifndef SRVROS_BOOTLOG_HOST_TEST
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
    uint64_t count=copy_log(out,capacity,0);
    if(total) *total=total_bytes;
#ifndef SRVROS_BOOTLOG_HOST_TEST
    if(flags&(1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
    return count;
}
uint64_t bootlog_total(void) { return total_bytes; }
