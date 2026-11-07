#ifndef SRVROS_INTEL_DISPLAY_CAPTURE_H
#define SRVROS_INTEL_DISPLAY_CAPTURE_H
#include <stdbool.h>
#include <stdint.h>

/* Exact Air7,2 observation, not a display owner. No write callback exists. */
#define INTEL_CAPTURE_PAGES 9u
#define INTEL_CAPTURE_PIPES 3u
#define INTEL_CAPTURE_TRANS 4u
#define INTEL_CAPTURE_PIPE_WORDS 9u
#define INTEL_CAPTURE_TRANS_WORDS 5u
enum intel_capture_status {
    INTEL_CAPTURE_INVALID, INTEL_CAPTURE_OBSERVED, INTEL_CAPTURE_IDENTITY,
    INTEL_CAPTURE_ACCESS, INTEL_CAPTURE_FRAMEBUFFER, INTEL_CAPTURE_MAP,
    INTEL_CAPTURE_WELL_OFF, INTEL_CAPTURE_REGISTER, INTEL_CAPTURE_UNSTABLE,
    INTEL_CAPTURE_CLOCK, INTEL_CAPTURE_RETAINED_MAPPING
};
struct intel_capture_identity {
    uint32_t id, class_rev, subsystem, pch, command, pm, power_valid;
    uint32_t bar[4], firmware_match, mmio_safe;
};
struct intel_capture_fb {
    uint64_t physical, width, height, pitch;
    uint32_t bpp, model, red_size, red_shift, green_size, green_shift, blue_size, blue_shift;
};
struct intel_capture_sample {
    uint64_t begin_us, end_us;
    uint32_t well_before[4], well_after[4];
    uint32_t source[INTEL_CAPTURE_PIPES];
    uint32_t pipe[INTEL_CAPTURE_PIPES][INTEL_CAPTURE_PIPE_WORDS];
    uint32_t trans[INTEL_CAPTURE_TRANS][INTEL_CAPTURE_TRANS_WORDS];
    uint32_t reads;
};
struct intel_capture_result {
    enum intel_capture_status status;
    struct intel_capture_identity before, after;
    struct intel_capture_fb fb;
    struct intel_capture_sample sample[2];
    uint64_t mmio_base;
    uint32_t active_planes, linear_relation, mapped, retained;
};
struct intel_capture_io {
    void *ctx;
    bool (*identity)(void *, struct intel_capture_identity *);
    bool (*map)(void *, unsigned page, uint64_t physical);
    bool (*unmap)(void *, unsigned page);
    bool (*read)(void *, uint32_t offset, uint32_t *);
    uint64_t (*now)(void *);
};
extern const uint32_t intel_capture_pages[INTEL_CAPTURE_PAGES];
extern const uint32_t intel_capture_pipe_offsets[INTEL_CAPTURE_PIPE_WORDS];
extern const uint32_t intel_capture_trans_offsets[INTEL_CAPTURE_TRANS_WORDS];
bool intel_capture_offset_allowed(uint32_t offset);
bool intel_display_capture(struct intel_capture_result *, const struct intel_capture_fb *,
                           const struct intel_capture_io *);
const char *intel_capture_status_name(enum intel_capture_status);
struct limine_framebuffer;
void intel_display_probe_boot(struct limine_framebuffer *);
bool intel_display_probe_report(void);
#endif
