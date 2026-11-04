#ifndef SRVROS_VIRTIO_GPU_PROBE_H
#define SRVROS_VIRTIO_GPU_PROBE_H

#include <stdbool.h>
#include <stdint.h>

enum virtio_gpu_probe_status {
    VGPU_PROBE_UNAVAILABLE,
    VGPU_PROBE_OK,
    VGPU_PROBE_REFUSED,
    VGPU_PROBE_BAD_CAPABILITY,
    VGPU_PROBE_BAD_BAR,
    VGPU_PROBE_MAPPING_FAILED,
    VGPU_PROBE_NO_MEMORY,
    VGPU_PROBE_FEATURES_REJECTED,
    VGPU_PROBE_QUEUE_REJECTED,
    VGPU_PROBE_TIMEOUT,
    VGPU_PROBE_BAD_RESPONSE,
    VGPU_PROBE_RESET_FAILED,
};

struct virtio_gpu_probe_result {
    enum virtio_gpu_probe_status status;
    uint32_t width, height;
    bool fence_ok, reset_ok, quarantined;
};

/* Opt-in, once at boot, immediately after pci_init and before other drivers or
 * address spaces. Only a separate modern non-VGA GPU beside QEMU standard VGA
 * is eligible. This never creates resources, changes scanout or owns display. */
bool virtio_gpu_probe_run(uint64_t inherited_framebuffer_physical,
    struct virtio_gpu_probe_result *out);
const char *virtio_gpu_probe_status_name(enum virtio_gpu_probe_status status);

/* Separate opt-in primary QEMU virtio-vga backend. Prepare only validates and
 * records the inherited BAR0 framebuffer; begin creates two host resources but
 * leaves VGA scanout intact until the first frame. Calls require kernel CR3 and
 * a single external display owner with console framebuffer writes muted.
 * Frame input is the already-validated inherited 32-bit BGRX framebuffer.
 * No page mappings or allocations occur per frame. End restores PCI command
 * after a proven VirtIO reset; clean retirement permits another begin.
 * A failed frame retires automatically. Quarantine permanently refuses restart. */
bool virtio_gpu_present_prepare(uint64_t framebuffer_physical, uint32_t width, uint32_t height);
bool virtio_gpu_present_begin(void);
/* Only while inactive after a confirmed reset; allocates nothing. */
bool virtio_gpu_present_configure(uint32_t width, uint32_t height);
bool virtio_gpu_present_frame(const void *inherited_framebuffer, uint64_t pitch);
/* Report every successful staging write before frame(), in kernel CR3 with
 * external IRQ/owner exclusion. Writers are forbidden during a driver operation.
 * Coordinates are clipped without overflowing. Unknown/bulk writes must report
 * the full viewport; unreported writes cannot be detected by this backend. */
void virtio_gpu_present_damage(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
struct virtio_gpu_present_stats {
    uint64_t presented_frames, skipped_frames, copied_bytes, transferred_bytes;
};
/* Serialized caller snapshot, reset at each accepted begin attempt. Byte counts
 * include work done before a later failure; transferred bytes require its fence. */
void virtio_gpu_present_get_stats(struct virtio_gpu_present_stats *out);
bool virtio_gpu_present_end(void);
bool virtio_gpu_present_available(void);
bool virtio_gpu_present_active(void);
const struct virtio_gpu_probe_result *virtio_gpu_present_result(void);
/* Set outside a driver operation. Callback may run a bounded service window;
 * the external display owner must retain all staging/DMA ownership across it. */
void virtio_gpu_present_set_checkpoint(void (*fn)(void *), void *arg);

#endif
