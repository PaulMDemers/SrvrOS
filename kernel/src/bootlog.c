#include <srvros/bootlog.h>
#include <srvros/console.h>
#include <srvros/exfat.h>

#include <stdint.h>

#define BOOTLOG_CAPACITY 32768

static char ring[BOOTLOG_CAPACITY];
static char linear[BOOTLOG_CAPACITY];
static uint64_t write_index;
static uint64_t bytes_recorded;
static bool suppress_recording;

void bootlog_putc(char c) {
    if (suppress_recording) {
        return;
    }

    ring[write_index] = c;
    write_index = (write_index + 1) % BOOTLOG_CAPACITY;
    if (bytes_recorded < BOOTLOG_CAPACITY) {
        bytes_recorded++;
    }
}

uint64_t bootlog_size(void) {
    return bytes_recorded;
}

static uint64_t oldest_index(void) {
    if (bytes_recorded < BOOTLOG_CAPACITY) {
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
        out[i] = ring[(first + skip + i) % BOOTLOG_CAPACITY];
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
        console_putc(ring[(first + skip + i) % BOOTLOG_CAPACITY]);
    }
    if (count == 0 || ring[(first + skip + count - 1) % BOOTLOG_CAPACITY] != '\n') {
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
