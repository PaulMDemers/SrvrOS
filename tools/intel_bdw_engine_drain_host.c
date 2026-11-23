/* On-device i915 reference for the five Gen8 private submission queues. */
#define _POSIX_C_SOURCE 200809L
#include "intel_bdw_i915_host.h"
#include <srvros/intel_bdw_bcs_flush.h>
#include <srvros/intel_bdw_private_vm.h>
#include <limits.h>
#include <linux/sync_file.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

enum {
    GATE_GPU = 0x6000000,
    SLOT_BO_BYTES = 40 * 1024 * 1024,
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
                                unsigned sequence, uint64_t seed, int dependency, uint32_t gate_handle)
{
    uint32_t result = create_page(fd), command = create_page(fd);
    uint32_t address = 0x4000000 + sequence * 2 * PRIVATE_PAGE_BYTES;
    struct submission s = {.result = result, .view = map_result(fd, result),
                           .marker = seed ^ sequence, .data = ~(seed ^ sequence)};
    require(!__atomic_load_n(s.view, __ATOMIC_ACQUIRE) &&
            !__atomic_load_n(&s.view[RESULT_MARKER_OFFSET / 8], __ATOMIC_ACQUIRE),
            "Stale private data or marker before submission");
    /* Both GPU VAs are private, pinned by execbuf, and disjoint from every
     * other in-flight batch. No relocation may change the tested packet. */
    uint32_t batch[6 + BDW_GATE_WORDS_MAX + 2] = {MI_STORE_QWORD_PPGTT, address, 0, (uint32_t)s.data,
                          (uint32_t)(s.data >> 32), 0};
    unsigned body;
    if (gate_handle) {
        body = intel_bdw_ppgtt_drain_and_hold(engine, address, GATE_GPU, s.marker, batch + 6);
        require(body != 0, "Cannot encode ordered private GPU drain and hold");
    } else {
        require(intel_bdw_ppgtt_barrier(engine, address + RESULT_MARKER_OFFSET, s.marker, batch + 6),
                "Cannot encode private GPU barrier");
        body = BCS_FLUSH_DWORDS;
    }
    unsigned end = 6 + body;
    batch[end] = MI_BATCH_BUFFER_END;
    unsigned bytes = (end + 2) * sizeof(uint32_t);
    struct drm_i915_gem_pwrite write = {.handle = command, .size = bytes,
                                        .data_ptr = (uintptr_t)batch};
    call(fd, DRM_IOCTL_I915_GEM_PWRITE, &write, "upload batch");
    struct drm_i915_gem_exec_object2 objects[] = {
        {.handle = gate_handle, .offset = GATE_GPU, .flags = EXEC_OBJECT_PINNED},
        {.handle = result, .offset = address, .flags = EXEC_OBJECT_WRITE | EXEC_OBJECT_PINNED},
        {.handle = command, .offset = address + PRIVATE_PAGE_BYTES, .flags = EXEC_OBJECT_PINNED},
    };
    struct drm_i915_gem_execbuffer2 exec = {.buffers_ptr = (uintptr_t)&objects[gate_handle ? 0 : 1],
        .buffer_count = gate_handle ? 3u : 2u, .batch_len = bytes, .flags = engines[engine].flags | I915_EXEC_FENCE_OUT,
        .rsvd1 = context};
    if (dependency >= 0) {
        exec.flags |= I915_EXEC_FENCE_IN;
        exec.rsvd2 = (uint32_t)dependency;
    }
    call(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2_WR, &exec, "submit private batch");
    require(objects[1].offset == address && objects[2].offset == address + PRIVATE_PAGE_BYTES &&
            (!gate_handle || objects[0].offset == GATE_GPU),
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

/* The observer checks the real GGTT binding before returning the offset
 * into our private object. i915 owns all page-table changes in this test. */
static uint32_t bind_slot(int fd, uint32_t handle, unsigned phase, uint64_t seed, unsigned bytes)
{
    struct drm_i915_gem_mmap_offset off = {.handle = handle, .flags = I915_MMAP_OFFSET_GTT};
    call(fd, DRM_IOCTL_I915_GEM_MMAP_OFFSET, &off, "GGTT mapping offset");
    volatile uint32_t *map = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off.offset);
    require(map != MAP_FAILED, "Cannot map the private GGTT object");
    (void)map[0];  /* Fault in the binding before asking the observer. */
    printf("BIND %u %u\n", phase, bytes);
    fflush(stdout);
    unsigned offset;
    require(scanf("%u", &offset) == 1 && !(offset & 4095) && offset <= bytes - 4096,
            "Observer could not identify the target in this binding");
    uint32_t words[1024];
    for (unsigned i = 0; i < 1024; ++i) {
        words[i] = (phase == 1 ? ~(uint32_t)seed : (uint32_t)seed) ^ i;
        if (phase < 2) map[offset/4 + i] = words[i];
        else require(map[offset/4 + i] == words[i], "Original slot contents lost on rollback");
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    uint32_t backing[1024];
    struct drm_i915_gem_pread rd = {.handle = handle, .offset = offset, .size = sizeof(backing),
                                     .data_ptr = (uintptr_t)backing};
    call(fd, DRM_IOCTL_I915_GEM_PREAD, &rd, "verify private backing through GEM");
    require(!memcmp(words, backing, sizeof(words)), "GGTT and GEM backing disagree");
    require(!munmap((void *)map, bytes), "Cannot release CPU aperture view");
    return offset;
}

static void set_cache(int fd, uint32_t handle, unsigned level)
{
    struct drm_i915_gem_caching c = {.handle = handle, .caching = level};
    call(fd, DRM_IOCTL_I915_GEM_SET_CACHING, &c, "unbind private GGTT object");
}

/* The trace observer rechecks the raw PTE and the programmed/live plane
 * before replacement and once more after rollback and the vblank wait. */
static void confirm_mapping(unsigned phase)
{
    unsigned reply;
    printf("CHECK %u\n", phase);
    fflush(stdout);
    require(scanf("%u", &reply) == 1 && reply == phase, "Observer lost the mapping or original plane");
}

static void assert_waiting(struct submission work[], const uint32_t *gate)
{
    require(!__atomic_load_n(gate, __ATOMIC_ACQUIRE), "Watchdog released the GPU queues");
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        require(__atomic_load_n(&work[i].view[RESULT_MARKER_OFFSET / 8], __ATOMIC_ACQUIRE) == work[i].marker &&
                __atomic_load_n(&work[i].view[BDW_QUEUE_RESUMED / 8], __ATOMIC_ACQUIRE) == 0 &&
                fence_status(work[i].fence) == 0, "Engine escaped its semaphore");
    require(!__atomic_load_n(gate, __ATOMIC_ACQUIRE), "Watchdog expired during verification");
}

static pid_t start_watchdog(uint32_t *gate,int *signal_fd)
{
    /* A separate process releases the wait even if an ioctl blocks or the
     * test exits. It needs no engine, GEM ioctl or allocation to do so. */
    int finish[2];
    require(!pipe(finish), "Cannot create watchdog pipe");
    pid_t watcher = fork();
    require(watcher >= 0, "Cannot start watchdog");
    if (!watcher) {
        close(finish[1]);
        struct pollfd stop = {.fd = finish[0], .events = POLLIN};
        int ready = poll(&stop, 1, 1500);
        __atomic_store_n(&gate[BDW_GATE_RENDER_START / 4], 1, __ATOMIC_SEQ_CST);
        __atomic_store_n(gate, 1, __ATOMIC_SEQ_CST);
        _exit(ready == 1 ? 0 : 1);
    }
    close(finish[0]);
    *signal_fd=finish[1];
    return watcher;
}

/* Keep small private GGTT bindings live until the observer locates a
 * separator between the protected and target slots in i915's allocator. */
static void separate_slots(int fd)
{
    for (unsigned i = 0; i < 16; ++i) {
        struct drm_i915_gem_create pad = {.size = 4u * 1024 * 1024};
        call(fd, DRM_IOCTL_I915_GEM_CREATE, &pad, "create GGTT separator");
        struct drm_i915_gem_mmap_offset off = {.handle = pad.handle, .flags = I915_MMAP_OFFSET_GTT};
        call(fd, DRM_IOCTL_I915_GEM_MMAP_OFFSET, &off, "separator mmap offset");
        volatile const uint32_t *view = mmap(NULL, pad.size, PROT_READ, MAP_SHARED, fd, off.offset);
        require(view != MAP_FAILED, "Cannot map GGTT separator");
        (void)view[0];
        printf("PAD %llu\n", (unsigned long long)pad.size);
        fflush(stdout);
        unsigned done;
        require(scanf("%u", &done) == 1 && done <= 1, "Observer lost the separator binding");
        if (done) return;
    }
    require(false, "No separator within the trial allocation budget");
}

static void check_competing_context(const struct submission probes[], unsigned phase)
{
    unsigned executed = 0;
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i) {
        const struct submission *s = &probes[i];
        int status = fence_status(s->fence);
        require(status >= 0, "Independent request failed on the GPU");
        if (status || __atomic_load_n(s->view, __ATOMIC_ACQUIRE) ||
            __atomic_load_n(&s->view[RESULT_MARKER_OFFSET / 8], __ATOMIC_ACQUIRE))
            executed |= 1u << i;
    }
    printf("independent_context phase=%u executed_mask=%#x\n", phase, executed);
    fflush(stdout);
    require(!executed, "Independent work ran while the original queues were parked");
}

static int64_t context_priority(int fd, uint32_t context)
{
    struct drm_i915_gem_context_param priority = {.ctx_id = context, .param = I915_CONTEXT_PARAM_PRIORITY};
    call(fd, DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &priority, "read context priority");
    return (int64_t)priority.value;
}

static void lower_holder_priority(int fd, uint32_t context)
{
    struct drm_i915_gem_context_param priority = {
        .ctx_id = context, .param = I915_CONTEXT_PARAM_PRIORITY, .value = I915_CONTEXT_MIN_USER_PRIORITY,
    };
    call(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &priority, "lower holder priority");
}

static void slot_loan(int fd, int card, uint32_t crtc, uint32_t ctx, uint64_t seed, bool compete)
{
    if (compete) lower_holder_priority(fd, ctx);
    /* The occupied separator lies between 34 and 48 MiB. A smaller loan
     * avoids evicting it when the five independent requests are resident. */
    const unsigned bytes = compete ? 16u * 1024 * 1024 : SLOT_BO_BYTES;
    struct drm_i915_gem_create a = {.size = bytes}, b = {.size = bytes};
    call(fd, DRM_IOCTL_I915_GEM_CREATE, &a, "original GGTT object");
    call(fd, DRM_IOCTL_I915_GEM_CREATE, &b, "replacement GGTT object");
    struct drm_i915_gem_caching cache = {.handle = a.handle};
    call(fd, DRM_IOCTL_I915_GEM_GET_CACHING, &cache, "original cache policy");
    set_cache(fd, b.handle, cache.caching);
    unsigned alternate = cache.caching == I915_CACHING_CACHED ? I915_CACHING_NONE : I915_CACHING_CACHED;
    uint32_t initial_offset = bind_slot(fd, a.handle, 0, seed, bytes);
    uint32_t gate_handle = create_page(fd);
    struct drm_i915_gem_mmap_offset off = {.handle = gate_handle, .flags = I915_MMAP_OFFSET_WB};
    call(fd, DRM_IOCTL_I915_GEM_MMAP_OFFSET, &off, "semaphore WB offset");
    uint32_t *gate = mmap(NULL, PRIVATE_PAGE_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off.offset);
    require(gate != MAP_FAILED, "Cannot map coherent private semaphore");
    __atomic_store_n(gate, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&gate[BDW_GATE_RENDER_START / 4], 0, __ATOMIC_RELEASE);
    int signal_fd;
    pid_t watcher=start_watchdog(gate,&signal_fd);
    struct submission work[BDW_BARRIER_ENGINES];
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        work[i] = submit(fd, ctx, i, i, seed, -1, i == BDW_BARRIER_RCS ? 0 : gate_handle);
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        observe_marker(work[i].view, work[i].data, work[i].marker);
    /* Finish the first RCS request while the four sibling queues are parked.
     * A fresh render context must execute after all five receipts. On BDW,
     * i915 installs its coherent L3 workaround in the indirect context batch. */
    verify(fd, &work[BDW_BARRIER_RCS]);
    struct drm_i915_gem_context_create second = {0};
    call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &second, "create second render context");
    require(second.ctx_id != ctx, "Render context identity did not change");
    if (compete) lower_holder_priority(fd, second.ctx_id);
    struct submission *render = &work[BDW_BARRIER_RCS];
    *render = submit(fd, second.ctx_id, BDW_BARRIER_RCS, BDW_BARRIER_ENGINES, seed, -1, gate_handle);
    observe_marker(render->view, render->data, render->marker);
    assert_waiting(work, gate);
    printf("Ordered render context switch %u -> %u after five engine receipts\n", ctx, second.ctx_id);
    const uint64_t *final_marker = &render->view[BDW_QUEUE_FINAL_FLUSH / 8];
    require(!__atomic_load_n(final_marker, __ATOMIC_ACQUIRE), "Final flush ran before engine drain");
    __atomic_store_n(&gate[BDW_GATE_RENDER_START / 4], 1, __ATOMIC_SEQ_CST);
    uint64_t deadline = monotonic_us() + COMPLETION_TIMEOUT_US;
    while (!__atomic_load_n(final_marker, __ATOMIC_ACQUIRE) && monotonic_us() < deadline) {
        const struct timespec pause = {.tv_nsec = 50000};
        nanosleep(&pause, NULL);
    }
    require(__atomic_load_n(final_marker, __ATOMIC_ACQUIRE) == render->marker,
            "Missing fresh render flush after the five engine drains");
    assert_waiting(work, gate);
    puts("All five drain markers preceded the final render flush");
    struct submission probes[BDW_BARRIER_ENGINES];
    if (compete) {
        struct drm_i915_gem_context_create challenger = {0};
        call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &challenger, "create competing context");
        int64_t first_priority = context_priority(fd, ctx);
        int64_t second_priority = context_priority(fd, second.ctx_id);
        int64_t challenge_priority = context_priority(fd, challenger.ctx_id);
        require(challenge_priority > first_priority && challenge_priority > second_priority,
                "Independent context must outrank both parked contexts");
        printf("context_priority original=%lld render=%lld challenger=%lld\n",
               (long long)first_priority, (long long)second_priority, (long long)challenge_priority);
        for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
            probes[i] = submit(fd, challenger.ctx_id, i, 2 * BDW_BARRIER_ENGINES + i, seed, -1, 0);
        /* Give the scheduler a vblank before the first mapping change. None
         * of these requests shares a GEM object or fence with the parked work. */
        uint64_t frame0 = frame(card, crtc);
        uint64_t until = monotonic_us() + COMPLETION_TIMEOUT_US;
        uint64_t current;
        do {
            const struct timespec pause = {.tv_nsec = 500000};
            nanosleep(&pause, NULL);
            current = frame(card, crtc);
            require(monotonic_us() < until && current >= frame0, "Panel stalled with competing requests queued");
        } while (current == frame0);
    }
    uint64_t first = frame(card, crtc);
    confirm_mapping(0);
    assert_waiting(work, gate);
    if (compete) check_competing_context(probes, 0);
    set_cache(fd, a.handle, alternate);
    bind_slot(fd, b.handle, 1, seed, bytes);
    confirm_mapping(1);
    assert_waiting(work, gate);
    if (compete) check_competing_context(probes, 1);
    set_cache(fd, b.handle, alternate);
    set_cache(fd, a.handle, cache.caching);
    require(bind_slot(fd, a.handle, 2, seed, bytes) == initial_offset, "Rollback changed object offset");
    uint64_t last = frame(card, crtc);
    uint64_t until = monotonic_us() + COMPLETION_TIMEOUT_US;
    while (last == first && monotonic_us() < until) {
        const struct timespec pause = {.tv_nsec = 500000};
        nanosleep(&pause, NULL);
        last = frame(card, crtc);
    }
    require(first < last && active_edp(card) == crtc, "Original panel stopped advancing");
    confirm_mapping(2);
    assert_waiting(work, gate);
    if (compete) check_competing_context(probes, 2);
    close(signal_fd);
    int status;
    require(waitpid(watcher, &status, 0) == watcher && WIFEXITED(status) && !WEXITSTATUS(status),
            "Watchdog released the gate before rollback finished");
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i) {
        verify(fd, &work[i]);
        require(__atomic_load_n(&work[i].view[BDW_QUEUE_RESUMED / 8], __ATOMIC_ACQUIRE) == work[i].data,
                "Missing post-release marker");
        printf("%s flushed, held and resumed\n", engines[i].name);
        if (compete) verify(fd, &probes[i]);
    }
    if (compete) puts("All five independent requests completed after release");
    printf("eDP CRTC %u advanced %llu -> %llu; 1024 original words restored\n", crtc,
           (unsigned long long)first, (unsigned long long)last);
}

/* Execute the native batch bytes and virtual addresses. i915 supplies its
 * own page tables and contexts. Only private, per-process mappings are used. */
static void private_image(int fd,int card,uint32_t crtc,uint32_t ctx,uint64_t epoch)
{
    static _Alignas(4096) struct bdw_private_vm_image image;
    /* The model bus address is used solely to validate table construction.
     * None of these page-table words are submitted to i915. */
    require(intel_bdw_private_vm_build(&image,0x100000,epoch) &&
            intel_bdw_private_vm_verify(&image,0x100000,epoch),"Invalid private VM image");
    uint32_t batch=create_page(fd), gate=create_page(fd);
    struct drm_i915_gem_pwrite upload={.handle=batch,.size=sizeof(image.commands),
                                       .data_ptr=(uintptr_t)image.commands};
    call(fd,DRM_IOCTL_I915_GEM_PWRITE,&upload,"upload exact native batch page");
    struct drm_i915_gem_mmap_offset off={.handle=gate,.flags=I915_MMAP_OFFSET_WB};
    call(fd,DRM_IOCTL_I915_GEM_MMAP_OFFSET,&off,"map gate");
    uint32_t *cpu=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,off.offset);
    require(cpu!=MAP_FAILED && !cpu[0] && !cpu[BDW_GATE_RENDER_START/4],"Gate was not clear");
    struct submission q[BDW_BARRIER_ENGINES];
    for(unsigned e=0;e<BDW_BARRIER_ENGINES;e++) {
        q[e].result=create_page(fd);
        q[e].view=map_result(fd,q[e].result);
        q[e].marker=(epoch<<8)|(e+1);
        for(unsigned i=0;i<512;i++) require(!q[e].view[i],"Stale engine scratch");
    }
    int signal_fd;
    pid_t watcher=start_watchdog(cpu,&signal_fd);
    for(unsigned e=0;e<BDW_BARRIER_ENGINES;e++) {
        struct drm_i915_gem_exec_object2 obj[]={
            {.handle=gate,.offset=(BDW_PRIVATE_GATE+1)*4096,.flags=EXEC_OBJECT_PINNED},
            {.handle=q[e].result,.offset=(2+e)*4096,.flags=EXEC_OBJECT_PINNED|EXEC_OBJECT_WRITE},
            {.handle=batch,.offset=4096,.flags=EXEC_OBJECT_PINNED}
        };
        struct drm_i915_gem_execbuffer2 request={.buffers_ptr=(uintptr_t)obj,.buffer_count=3,
            .batch_start_offset=e*256,.batch_len=256,.rsvd1=ctx,.flags=engines[e].flags|I915_EXEC_FENCE_OUT};
        call(fd,DRM_IOCTL_I915_GEM_EXECBUFFER2_WR,&request,"execute native queue batch");
        q[e].fence=(int)(request.rsvd2>>32);
    }
    for(unsigned e=0;e<BDW_BARRIER_ENGINES;e++)observe_marker(q[e].view,0,q[e].marker);
    assert_waiting(q,cpu);
    require(!__atomic_load_n(q[0].view+BDW_QUEUE_FINAL_FLUSH/8,__ATOMIC_ACQUIRE),"Early render marker");
    __atomic_store_n(cpu+BDW_GATE_RENDER_START/4,1,__ATOMIC_SEQ_CST);
    uint64_t deadline=monotonic_us()+COMPLETION_TIMEOUT_US;
    while(!__atomic_load_n(q[0].view+BDW_QUEUE_FINAL_FLUSH/8,__ATOMIC_ACQUIRE) && monotonic_us()<deadline) {
        const struct timespec delay={.tv_nsec=50000}; nanosleep(&delay,NULL);
    }
    require(q[0].view[BDW_QUEUE_FINAL_FLUSH/8]==q[0].marker,"Missing final render flush");
    uint64_t first=frame(card,crtc),last=first;
    deadline=monotonic_us()+COMPLETION_TIMEOUT_US;
    while(first==last && monotonic_us()<deadline)last=frame(card,crtc);
    require(last>first && active_edp(card)==crtc,"Panel stopped while engines waited");
    assert_waiting(q,cpu);
    close(signal_fd);
    int status;
    require(waitpid(watcher,&status,0)==watcher && WIFEXITED(status) && !WEXITSTATUS(status),
            "Watchdog expired");
    for(unsigned e=0;e<BDW_BARRIER_ENGINES;e++) {
        struct pollfd fence={.fd=q[e].fence,.events=POLLIN};
        require(poll(&fence,1,3000)==1 && fence_status(fence.fd)==1,"Private batch fence failed");
        for(unsigned i=0;i<512;i++) {
            uint64_t expected=i==BDW_QUEUE_RESUMED/8 ? ~q[e].marker :
                (i==BDW_QUEUE_DRAIN/8 || (!e && i==BDW_QUEUE_FINAL_FLUSH/8)) ? q[e].marker : 0;
            require(__atomic_load_n(q[e].view+i,__ATOMIC_ACQUIRE)==expected,"Unexpected scratch contents");
        }
        printf("%s marker=%016llx resumed=%016llx scratch verified\n",engines[e].name,
               (unsigned long long)q[e].marker,(unsigned long long)~q[e].marker);
    }
    printf("private-image eDP CRTC %u frames %llu -> %llu\n",crtc,(unsigned long long)first,(unsigned long long)last);
}

int main(int argc, char **argv)
{
    require(argc == 2 && geteuid() != 0, "Usage as the render user: intel-bdw-engine-drain-host TIMELINE_FD|--slot-loan|--independent-slot|--competing-context|--private-image");
    char *end;
    long timeline = strtol(argv[1], &end, 10);
    bool slot_trial = !strcmp(argv[1], "--slot-loan");
    bool compete = !strcmp(argv[1], "--competing-context");
    bool independent_trial = compete || !strcmp(argv[1], "--independent-slot");
    bool image_trial = !strcmp(argv[1], "--private-image");
    if (!slot_trial && !independent_trial && !image_trial) require(end != argv[1] && !*end && timeline >= 3 && timeline <= INT_MAX, "Invalid timeline FD");
    int fd = open("/dev/dri/by-path/pci-0000:00:02.0-render", O_RDWR | O_CLOEXEC);
    require(fd >= 0 && parameter(fd, I915_PARAM_CHIPSET_ID) == 0x1626 &&
            parameter(fd, I915_PARAM_HAS_ALIASING_PPGTT) >= 2 && parameter(fd, I915_PARAM_HAS_LLC) &&
            parameter(fd, I915_PARAM_HAS_EXEC_SOFTPIN), "Requires the Air7,2 i915 device with private softpin");
    int card = open("/dev/dri/by-path/pci-0000:00:02.0-card", O_RDONLY | O_CLOEXEC);
    require(card >= 0, "Cannot open integrated display card");
    uint32_t crtc = active_edp(card);
    struct drm_i915_gem_context_create context = {0};
    call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &context, "create private context");
    uint64_t seed;
    require(getrandom(&seed, sizeof(seed), 0) == sizeof(seed) && (uint32_t)(seed >> 32) != 0 &&
            (uint32_t)(seed >> 32) != UINT32_MAX, "Cannot obtain fresh markers");
    if (image_trial) {
        private_image(fd,card,crtc,context.ctx_id,seed>>8);
        return EXIT_SUCCESS;
    }
    if (slot_trial || independent_trial) {
        if (independent_trial) {
            unsigned ready;
            puts("READY");
            fflush(stdout);
            require(scanf("%u", &ready) == 1 && ready == 1, "Observer did not establish the baseline");
            separate_slots(fd);
        }
        slot_loan(fd, card, crtc, context.ctx_id, seed, compete);
        if (independent_trial) confirm_mapping(3); /* Before i915 closes any trial object. */
        return EXIT_SUCCESS;
    }
    struct sw_fence gate = {.value = 1, .name = "SrvrOS private successors"};
    call((int)timeline, _IOWR('W', 0, struct sw_fence), &gate, "create software fence");
    require(fence_status(gate.fence) == 0, "Timeline must be fresh");
    struct submission first[BDW_BARRIER_ENGINES], next[BDW_BARRIER_ENGINES];
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        first[i] = submit(fd, context.ctx_id, i, i, seed, -1, 0);
    for (unsigned i = 0; i < BDW_BARRIER_ENGINES; ++i)
        next[i] = submit(fd, context.ctx_id, i, i + BDW_BARRIER_ENGINES, seed, gate.fence, 0);

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
