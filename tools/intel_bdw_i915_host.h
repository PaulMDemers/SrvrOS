#ifndef SRVROS_TOOLS_INTEL_BDW_I915_HOST_H
#define SRVROS_TOOLS_INTEL_BDW_I915_HOST_H
#include <drm/i915_drm.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>

enum {
    MI_STORE_QWORD_PPGTT = (0x20 << 23) | (1 << 21) | 3,
    MI_FLUSH_QWORD_PPGTT = (0x26 << 23) | (1 << 14) | 3,
    MI_BATCH_BUFFER_END = 0x0a << 23,
    PRIVATE_PAGE_BYTES = 4096,
    RESULT_MARKER_OFFSET = 64,
    COMPLETION_TIMEOUT_US = 100000,
};

static void call(int fd, unsigned long op, void *args, const char *label)
{
    if (ioctl(fd, op, args) < 0) {
        perror(label);
        exit(EXIT_FAILURE);
    }
}

static int parameter(int fd, int param)
{
    int value = 0;
    struct drm_i915_getparam get = {.param = param, .value = &value};
    call(fd, DRM_IOCTL_I915_GETPARAM, &get, "i915 getparam");
    return value;
}

static uint32_t create_page(int fd)
{
    struct drm_i915_gem_create page = {.size = PRIVATE_PAGE_BYTES};
    call(fd, DRM_IOCTL_I915_GEM_CREATE, &page, "i915 create private page");
    return page.handle;
}

static uint64_t monotonic_us(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_nsec / 1000;
}

static uint64_t *map_result(int fd, uint32_t handle)
{
    struct drm_i915_gem_mmap_offset mapping = {
        .handle = handle, .flags = I915_MMAP_OFFSET_WB,
    };
    call(fd, DRM_IOCTL_I915_GEM_MMAP_OFFSET, &mapping, "i915 map private result");
    uint64_t *view = mmap(NULL, PRIVATE_PAGE_BYTES, PROT_READ, MAP_SHARED,
                          fd, (off_t)mapping.offset);
    if (view == MAP_FAILED) {
        perror("mmap private result");
        exit(EXIT_FAILURE);
    }
    /* Fault the WB page in before submitting work to the GPU. */
    if (__atomic_load_n(&view[RESULT_MARKER_OFFSET / sizeof(*view)], __ATOMIC_ACQUIRE)) {
        fputs("Private result already contains a marker\n", stderr);
        exit(EXIT_FAILURE);
    }
    return view;
}

static uint64_t observe_marker(const uint64_t *view, uint64_t data, uint64_t marker)
{
    const uint64_t *slot = &view[RESULT_MARKER_OFFSET / sizeof(*view)];
    const struct timespec retry = {.tv_nsec = 50000};
    uint64_t begin = monotonic_us();
    for (;;) {
        uint64_t actual = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
        uint64_t elapsed = monotonic_us() - begin;
        if (elapsed > COMPLETION_TIMEOUT_US) {
            fputs("Timed out observing private GPU marker\n", stderr);
            exit(EXIT_FAILURE);
        }
        if (actual == marker) {
            if (__atomic_load_n(view, __ATOMIC_ACQUIRE) != data) {
                fputs("Earlier GPU write was missing when the marker was observed\n", stderr);
                exit(EXIT_FAILURE);
            }
            return elapsed;
        }
        if (actual) {
            fputs("Unexpected value in private GPU marker\n", stderr);
            exit(EXIT_FAILURE);
        }
        nanosleep(&retry, NULL);
    }
}

static void check_result(int fd, uint32_t handle, uint64_t data, uint64_t marker,
                         const char *phase)
{
    uint64_t actual[RESULT_MARKER_OFFSET / sizeof(uint64_t) + 1];
    uint64_t expected[sizeof(actual) / sizeof(actual[0])] = {0};
    expected[0] = data;
    expected[RESULT_MARKER_OFFSET / sizeof(uint64_t)] = marker;
    struct drm_i915_gem_pread read = {
        .handle = handle, .size = sizeof(actual), .data_ptr = (uintptr_t)actual,
    };
    call(fd, DRM_IOCTL_I915_GEM_PREAD, &read, "i915 read private result");
    if (memcmp(actual, expected, sizeof(actual)) != 0) {
        fprintf(stderr, "%s result mismatch. Data 0x%016llx, marker 0x%016llx\n",
                phase, (unsigned long long)actual[0],
                (unsigned long long)actual[RESULT_MARKER_OFFSET / sizeof(uint64_t)]);
        exit(EXIT_FAILURE);
    }
}

#endif
