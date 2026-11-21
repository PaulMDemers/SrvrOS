/* On-device i915 reference for the five Gen8 private submission queues. */
#define _POSIX_C_SOURCE 200809L
#include "intel_bdw_i915_host.h"
#include <srvros/intel_bdw_bcs_flush.h>
#include <limits.h>
#include <linux/sync_file.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static const struct {
    const char *name;
    unsigned flags;
} engines[BDW_BARRIER_ENGINES] = {
    [BDW_BARRIER_RCS] = {"rcs0", I915_EXEC_RENDER},
    [BDW_BARRIER_BCS] = {"bcs0", I915_EXEC_BLT},
    [BDW_BARRIER_VCS0] = {"vcs0", I915_EXEC_BSD | I915_EXEC_BSD_RING1},
    [BDW_BARRIER_VCS1] = {"vcs1", I915_EXEC_BSD | I915_EXEC_BSD_RING2},
    [BDW_BARRIER_VECS] = {"vecs0", I915_EXEC_VEBOX},
};

struct sw_fence { uint32_t value; char name[32]; int32_t fence; };
struct submission {
    uint32_t result;
    uint64_t *view;
    uint64_t data, marker;
    int fence;
};

static void require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static struct submission submit(int fd, uint32_t context, enum bdw_barrier_engine engine,
                                unsigned sequence, uint64_t seed, int dependency)
{
    uint32_t result = create_page(fd), command = create_page(fd);
    uint32_t address = 0x4000000 + sequence * 2 * PRIVATE_PAGE_BYTES;
    struct submission s = {.result = result, .view = map_result(fd, result),
                           .marker = seed ^ sequence, .data = ~(seed ^ sequence)};
    require(!__atomic_load_n(s.view, __ATOMIC_ACQUIRE), "Stale private data before submission");
    /* Both GPU VAs are private, pinned by execbuf, and disjoint from every
     * other in-flight batch. No relocation may change the tested packet. */
    uint32_t batch[14] = {MI_STORE_QWORD_PPGTT, address, 0, (uint32_t)s.data,
                          (uint32_t)(s.data >> 32), 0};
    require(intel_bdw_ppgtt_barrier(engine, address + RESULT_MARKER_OFFSET, s.marker, &batch[6]),
            "Cannot encode private GPU barrier");
    batch[12] = MI_BATCH_BUFFER_END;
    struct drm_i915_gem_pwrite write = {.handle = command, .size = sizeof(batch),
                                        .data_ptr = (uintptr_t)batch};
    call(fd, DRM_IOCTL_I915_GEM_PWRITE, &write, "upload batch");
    struct drm_i915_gem_exec_object2 objects[] = {
        {.handle = result, .offset = address, .flags = EXEC_OBJECT_WRITE | EXEC_OBJECT_PINNED},
        {.handle = command, .offset = address + PRIVATE_PAGE_BYTES, .flags = EXEC_OBJECT_PINNED},
    };
    struct drm_i915_gem_execbuffer2 exec = {.buffers_ptr = (uintptr_t)objects, .buffer_count = 2,
        .batch_len = sizeof(batch), .flags = engines[engine].flags | I915_EXEC_FENCE_OUT,
        .rsvd1 = context};
    if (dependency >= 0) {
        exec.flags |= I915_EXEC_FENCE_IN;
        exec.rsvd2 = (uint32_t)dependency;
    }
    call(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2_WR, &exec, "submit private batch");
    require(objects[0].offset == address && objects[1].offset == address + PRIVATE_PAGE_BYTES,
            "i915 returned a different private GPU address");
    s.fence = (int)(exec.rsvd2 >> 32);
    return s;
}

static int fence_status(int fd)
{
    struct sync_file_info info = {0};
    call(fd, SYNC_IOC_FILE_INFO, &info, "read sync fence");
    return info.status;
}

static void verify(int fd, struct submission *s)
{
    observe_marker(s->view, s->data, s->marker);
    struct pollfd p = {.fd = s->fence, .events = POLLIN};
    require(poll(&p, 1, 3000) == 1 && fence_status(s->fence) == 1,
            "Engine fence failed after CPU observed its post-sync marker");
    check_result(fd, s->result, s->data, s->marker, "independent GEM read");
}

static uint32_t active_edp(int card)
{
    struct stat info;
    require(fstat(card, &info) == 0, "Cannot identify DRM device");
    char path[80];
    snprintf(path, sizeof(path), "/sys/class/drm/card%u-eDP-1/connector_id", minor(info.st_rdev));
    FILE *id = fopen(path, "r");
    unsigned connector_id;
    require(id && fscanf(id, "%u", &connector_id) == 1, "Cannot identify internal panel");
    fclose(id);
    /* A nonzero mode count requests cached metadata without reprobe. */
    struct drm_mode_modeinfo mode;
    struct drm_mode_get_connector con = {.connector_id = connector_id, .count_modes = 1,
                                         .modes_ptr = (uintptr_t)&mode};
    call(card, DRM_IOCTL_MODE_GETCONNECTOR, &con, "panel connector");
    require(con.connector_type == DRM_MODE_CONNECTOR_eDP && con.encoder_id, "Internal panel disconnected");
    struct drm_mode_get_encoder enc = {.encoder_id = con.encoder_id};
    call(card, DRM_IOCTL_MODE_GETENCODER, &enc, "panel encoder");
    require(enc.crtc_id != 0, "Panel encoder has no CRTC");
    return enc.crtc_id;
}

static uint64_t frame(int card, uint32_t crtc)
{
    struct drm_crtc_get_sequence get = {.crtc_id = crtc};
    call(card, DRM_IOCTL_CRTC_GET_SEQUENCE, &get, "read panel frame sequence");
    require(get.active, "Panel CRTC became inactive");
    return get.sequence;
}

int main(int argc, char **argv)
{
    require(argc == 2 && geteuid() != 0, "Usage as the render user: intel-bdw-engine-drain-host TIMELINE_FD");
    char *end;
    long timeline = strtol(argv[1], &end, 10);
    require(end != argv[1] && !*end && timeline >= 3 && timeline <= INT_MAX, "Invalid timeline FD");
    int fd = open("/dev/dri/by-path/pci-0000:00:02.0-render", O_RDWR | O_CLOEXEC);
    require(fd >= 0 && parameter(fd, I915_PARAM_CHIPSET_ID) == 0x1626 &&
            parameter(fd, I915_PARAM_HAS_ALIASING_PPGTT) >= 2 && parameter(fd, I915_PARAM_HAS_LLC) &&
            parameter(fd, I915_PARAM_HAS_EXEC_SOFTPIN), "Requires the Air7,2 i915 device with private softpin");
    int card = open("/dev/dri/by-path/pci-0000:00:02.0-card", O_RDONLY | O_CLOEXEC);
    require(card >= 0, "Cannot open integrated display card");
    uint32_t crtc = active_edp(card);
    struct drm_i915_gem_context_create context = {0};
    call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &context, "create private context");
    struct sw_fence gate = {.value = 1, .name = "SrvrOS private successors"};
    call((int)timeline, _IOWR('W', 0, struct sw_fence), &gate, "create software fence");
    require(fence_status(gate.fence) == 0, "Timeline must be fresh");
    uint64_t seed;
    require(getrandom(&seed, sizeof(seed), 0) == sizeof(seed) && (uint32_t)(seed >> 32) != 0 &&
            (uint32_t)(seed >> 32) != UINT32_MAX, "Cannot obtain fresh markers");
    struct submission first[BDW_BARRIER_ENGINES], next[BDW_BARRIER_ENGINES];
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        first[i] = submit(fd, context.ctx_id, i, i, seed, -1);
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        next[i] = submit(fd, context.ctx_id, i, i + BDW_BARRIER_ENGINES, seed, gate.fence);

    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        verify(fd, &first[i]);
    uint64_t first_frame = frame(card, crtc), last_frame = first_frame;
    uint64_t begin = monotonic_us();
    while (last_frame == first_frame && monotonic_us() - begin < COMPLETION_TIMEOUT_US) {
        const struct timespec delay = {.tv_nsec = 1000000};
        nanosleep(&delay, NULL);
        last_frame = frame(card, crtc);
    }
    require(last_frame > first_frame && crtc == active_edp(card), "Panel frame did not advance under hold");
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i) {
        require(fence_status(next[i].fence) == 0 &&
                !__atomic_load_n(next[i].view, __ATOMIC_ACQUIRE) &&
                !__atomic_load_n(&next[i].view[RESULT_MARKER_OFFSET / 8], __ATOMIC_ACQUIRE),
                "Gated successor started before release");
    }
    uint32_t increment = 1;
    call((int)timeline, _IOW('W', 1, uint32_t), &increment, "release all private successors");
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i) {
        verify(fd, &next[i]);
        printf("%s prior data and marker visible, successor held and verified after release\n", engines[i].name);
    }
    require(crtc == active_edp(card) && frame(card, crtc) >= last_frame, "Lost original panel CRTC");
    printf("eDP CRTC %u advanced from %llu to %llu under the shared fence\n", crtc,
           (unsigned long long)first_frame, (unsigned long long)last_frame);
    return EXIT_SUCCESS;
}
