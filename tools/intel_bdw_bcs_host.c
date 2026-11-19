/* Linux/i915 reference probe for the Air7,2 Broadwell blitter. */
#define _POSIX_C_SOURCE 200809L

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
            fputs("Timed out observing private blitter marker\n", stderr);
            exit(EXIT_FAILURE);
        }
        if (actual == marker) {
            if (__atomic_load_n(view, __ATOMIC_ACQUIRE) != data) {
                fputs("Earlier blitter write was missing when the marker was observed\n", stderr);
                exit(EXIT_FAILURE);
            }
            return elapsed;
        }
        if (actual) {
            fputs("Unexpected value in private blitter marker\n", stderr);
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

static void exercise(bool write_marker, uint64_t data, uint64_t marker)
{
    /* This symlink selects the integrated GPU even when the EFI framebuffer
     * currently owns card0. i915 provides all mappings and scheduling. */
    int fd = open("/dev/dri/by-path/pci-0000:00:02.0-render", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open integrated GPU render node");
        exit(EXIT_FAILURE);
    }
    if (parameter(fd, I915_PARAM_CHIPSET_ID) != 0x1626 ||
        !parameter(fd, I915_PARAM_HAS_BLT) ||
        !parameter(fd, I915_PARAM_HAS_LLC) ||
        parameter(fd, I915_PARAM_HAS_ALIASING_PPGTT) < 2) {
        fputs("Requires Broadwell 0x1626 with blitter, LLC and full PPGTT\n", stderr);
        exit(EXIT_FAILURE);
    }
    struct drm_i915_gem_context_create context = {0};
    call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &context, "i915 create private context");
    uint32_t result = create_page(fd);
    uint32_t command = create_page(fd);
    check_result(fd, result, 0, 0, "before submission");
    uint64_t *view = write_marker ? map_result(fd, result) : NULL;

    /* Keep the first write and the post-sync marker on different cache lines.
     * Both relocations target this fresh, private GEM page. */
    uint32_t batch[] = {
        MI_STORE_QWORD_PPGTT, 0, 0, (uint32_t)data, (uint32_t)(data >> 32),
        MI_FLUSH_QWORD_PPGTT, 0, 0, (uint32_t)marker, (uint32_t)(marker >> 32),
        MI_BATCH_BUFFER_END, 0,
    };
    if (!write_marker) {
        batch[5] = MI_BATCH_BUFFER_END;
        batch[6] = batch[7] = batch[8] = batch[9] = 0;
    }
    struct drm_i915_gem_pwrite write = {
        .handle = command, .size = sizeof(batch), .data_ptr = (uintptr_t)batch,
    };
    call(fd, DRM_IOCTL_I915_GEM_PWRITE, &write, "i915 upload private batch");

    struct drm_i915_gem_relocation_entry relocations[] = {
        {.target_handle = result, .offset = sizeof(uint32_t), .presumed_offset = UINT64_MAX,
         .read_domains = I915_GEM_DOMAIN_RENDER, .write_domain = I915_GEM_DOMAIN_RENDER},
        {.target_handle = result, .delta = RESULT_MARKER_OFFSET, .offset = 6 * sizeof(uint32_t),
         .presumed_offset = UINT64_MAX, .read_domains = I915_GEM_DOMAIN_RENDER,
         .write_domain = I915_GEM_DOMAIN_RENDER},
    };
    struct drm_i915_gem_exec_object2 objects[] = {
        {.handle = result, .flags = EXEC_OBJECT_WRITE},
        {.handle = command, .relocation_count = write_marker ? 2 : 1,
         .relocs_ptr = (uintptr_t)relocations},
    };
    struct drm_i915_gem_execbuffer2 execute = {
        .buffers_ptr = (uintptr_t)objects, .buffer_count = 2, .batch_len = sizeof(batch),
        .flags = I915_EXEC_BLT, .rsvd1 = context.ctx_id,
    };
    call(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execute, "i915 submit blitter batch");
    uint64_t elapsed = view ? observe_marker(view, data, marker) : 0;
    struct drm_i915_gem_wait wait = {
        .bo_handle = command, .timeout_ns = (int64_t)COMPLETION_TIMEOUT_US * 1000,
    };
    call(fd, DRM_IOCTL_I915_GEM_WAIT, &wait, "i915 wait for blitter batch");
    check_result(fd, result, data, write_marker ? marker : 0, "after completion");
    if (view)
        munmap(view, PRIVATE_PAGE_BYTES);
    /* Closing the render FD drops our references. i915 owns their retirement. */
    close(fd);
    if (write_marker)
        printf("Post-sync marker and earlier write verified before GEM_WAIT (%llu us polling)\n",
               (unsigned long long)elapsed);
    else
        puts("Control verified earlier write with an untouched marker");
}

int main(void)
{
    uint64_t nonce[2];
    if (getrandom(nonce, sizeof(nonce), GRND_NONBLOCK) != (ssize_t)sizeof(nonce) ||
        !(uint32_t)nonce[0] || !(uint32_t)nonce[1] ||
        !(nonce[0] >> 32) || !(nonce[1] >> 32) || nonce[0] == nonce[1]) {
        fputs("Could not obtain two distinct nonzero markers with both halves populated\n", stderr);
        return EXIT_FAILURE;
    }
    exercise(true, UINT64_C(0x23f1d5bcdad9e70a), nonce[0]);
    exercise(false, UINT64_C(0x3424db22a30ae878), 0);
    exercise(true, UINT64_C(0x9e275db4bdbb3b8e), nonce[1]);
    return EXIT_SUCCESS;
}
