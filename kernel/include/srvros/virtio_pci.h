#ifndef SRVROS_VIRTIO_PCI_H
#define SRVROS_VIRTIO_PCI_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/pci.h>

#define VIRTIO_PCI_MAX_DEVICES 8u
#define VIRTIO_PCI_MAX_QUEUES 3u
#define VIRTIO_PCI_MAX_QUEUE_SIZE 256u
#define VIRTIO_PCI_VERSION_1 (1ull << 32)

struct virtio_pci_device;
enum virtio_pci_status {
    VIRTIO_PCI_OK, VIRTIO_PCI_REFUSED, VIRTIO_PCI_BAD_CAPABILITY,
    VIRTIO_PCI_BAD_BAR, VIRTIO_PCI_MAPPING_FAILED, VIRTIO_PCI_NO_MEMORY,
    VIRTIO_PCI_FEATURES_REJECTED, VIRTIO_PCI_QUEUE_REJECTED,
    VIRTIO_PCI_BAD_COMPLETION, VIRTIO_PCI_BAD_CONFIG, VIRTIO_PCI_RESET_FAILED,
    VIRTIO_PCI_STOPPED
};
struct virtio_pci_buffer {
    uint64_t physical;
    uint32_t length;
    bool device_write;
};
struct virtio_pci_completion { uint16_t token; uint32_t length; };

/* Boot-lifetime, bounded modern-only PCI transport, for VM-gated callers.
 * Every operation requires kernel CR3, external serialization and no concurrent
 * owner. No IRQ registration, yielding or implicit address-space switches.
 * Open claims the exact supplied function (PCI ID must be 0x1040 + device_type).
 * VERSION_1 is mandatory/automatic. Device bits 0..23 alone may be requested;
 * transport extensions (packed rings, indirect, event idx, access platform,
 * notification data, ring reset, etc.) are never negotiated.
 * Each PCI function gets one attempt; retained slots/mappings are not recycled.
 * On failed open, *out may still expose status; any owned attempt is retired.
 */
bool virtio_pci_open(const struct pci_device *pci, uint16_t device_type,
    uint64_t allowed_features, uint64_t required_features,
    uint32_t config_min_length, struct virtio_pci_device **out);
/* Explicit boot-only firmware handoff for modern block (1042) only. The caller
 * guarantees firmware execution has ended, no BIOS disk calls will resume, and
 * this is the VM opt-in boot initializer before any kernel block owner. Exact
 * firmware status 15 may be reset through validated PCI_CFG before BAR sizing.
 * Generic open remains status-zero-only. A claimed BDF is never adopted again;
 * firmware queue addresses are neither read nor freed by this driver. */
bool virtio_pci_open_boot_block(const struct pci_device *pci,
    uint64_t allowed_features, uint64_t required_features,
    uint32_t config_min_length, struct virtio_pci_device **out);
struct virtio_pci_admission {
    uint8_t status; /* 255 if selectors could not be validated. */
    bool selector_ok, firmware_reset, reset_ok;
};
void virtio_pci_get_admission(const struct virtio_pci_device *dev,
    struct virtio_pci_admission *out);
bool virtio_pci_queue_setup(struct virtio_pci_device *dev, uint16_t index, uint16_t size);
bool virtio_pci_start(struct virtio_pci_device *dev);

/* Caller owns contiguous physical buffers and keeps them pinned until a matching
 * completion or a successful stop. Chains are generated internally, with readable
 * buffers preceding writable buffers. Submit may precede start (no notification
 * before DRIVER_OK); start kicks populated queues. False submit never publishes.
 * token identifies a currently outstanding head, and may be reused after polling.
 * Full queues return false without faulting. Stop after other transport failures.
 */
bool virtio_pci_submit(struct virtio_pci_device *dev, uint16_t queue,
    const struct virtio_pci_buffer *buffers, uint16_t count, uint16_t *token);
/* 1: validated completion, 0: empty, -1: fault/refused. Length counts bytes written
 * by the device; caller must validate protocol-specific minimum/exact lengths.
 * Detects invalid, duplicate pending, non-outstanding IDs and oversized writes.
 * VirtIO IDs have no generation: callers needing detection of a malicious stale
 * completion after head reuse need an additional device-protocol fence.
 */
int virtio_pci_poll(struct virtio_pci_device *dev, uint16_t queue,
    struct virtio_pci_completion *completion);

/* Stable generation snapshots, bounded to 4096-byte device configuration. Byte
 * read is for byte fields/arrays only; use scalar functions for wider fields. */
bool virtio_pci_config_read(struct virtio_pci_device *dev, uint32_t offset, void *dst, uint32_t length);
bool virtio_pci_config_read16(struct virtio_pci_device *dev, uint32_t offset, uint16_t *value);
bool virtio_pci_config_read32(struct virtio_pci_device *dev, uint32_t offset, uint32_t *value);
bool virtio_pci_config_read64(struct virtio_pci_device *dev, uint32_t offset, uint64_t *value);
bool virtio_pci_config_write8(struct virtio_pci_device *dev, uint32_t offset, uint8_t value);

/* Reset proof precedes freeing ALL transport DMA, including partially allocated
 * queues. False means caller DMA must be retained; quarantine is permanent on
 * failed reset or unverified PCI disable. Disabling decode/bus mastering is
 * attempted on quarantine; failed readback may leave them enabled, so backing
 * remains pinned. MMIO mappings remain reserved for the boot lifetime,
 * including failed initialization.
 * Successful stop is idempotent. Closed devices cannot restart. */
bool virtio_pci_stop(struct virtio_pci_device *dev);
bool virtio_pci_quarantined(const struct virtio_pci_device *dev);
bool virtio_pci_active(const struct virtio_pci_device *dev);
uint64_t virtio_pci_features(const struct virtio_pci_device *dev);
enum virtio_pci_status virtio_pci_get_status(const struct virtio_pci_device *dev);
const char *virtio_pci_status_name(enum virtio_pci_status status);
#endif
