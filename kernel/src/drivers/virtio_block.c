/* Modern VirtIO block, VirtIO 1.2 sections 5.2.3--5.2.6.
 * https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html
 * One synchronous owner; bounce DMA never aliases caller memory. */
#include <srvros/virtio_block.h>
#include <srvros/virtio_pci.h>
#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/service_window.h>

#define VBLK_MAX_DISKS 2u
#define VBLK_BYTES 16384u
#define VBLK_TIMEOUT_US 2000000ull
#define VBLK_POLL_LIMIT 1000000u
#define VBLK_POLL_BATCH 64u
#define VBLK_F_RO (1ull << 5)
#define VBLK_F_FLUSH (1ull << 9)
#define VBLK_IN 0u
#define VBLK_OUT 1u
#define VBLK_FLUSH 4u
struct block_request { uint32_t type, reserved; uint64_t sector; };
_Static_assert(sizeof(struct block_request) == 16, "VirtIO block request");
struct virtio_disk {
    struct virtio_pci_device *transport;
    struct block_device *block;
    uint64_t control, bounce, sectors;
    uint64_t commands, reads, writes, flushes, failures, service_windows;
    bool used, failed, busy, writable, flush, quarantined;
    char name[8];
};
struct request_owner { struct service_window service; uint64_t original_cr3, kernel_cr3; };
static struct virtio_disk disks[VBLK_MAX_DISKS];
static bool attempted;
static unsigned disk_count;
#ifdef VIRTIO_BLOCK_HOST_TEST
extern void virtio_block_test_pause(void);
#define block_pause() virtio_block_test_pause()
#else
#define block_pause() __asm__ volatile("pause" ::: "memory")
#endif
static void copy(void *dst, const void *src, uint64_t length) {
    uint8_t *d = dst; const uint8_t *s = src;
    for (uint64_t i = 0; i < length; ++i) d[i] = s[i];
}
static void zero(void *dst, unsigned length) {
    uint8_t *d = dst; for (unsigned i = 0; i < length; ++i) d[i] = 0;
}
static void address_space(uint64_t cr3) {
    if (vmm_current_address_space() != cr3) vmm_switch_address_space(cr3);
}
static bool begin(struct virtio_disk *d, struct request_owner *owner) {
    owner->service = service_window_begin();
    owner->original_cr3 = vmm_current_address_space();
    owner->kernel_cr3 = vmm_kernel_address_space();
    if (!d || !d->used || d->failed || d->busy || !owner->kernel_cr3 ||
        (!owner->service.held && scheduler_io_active())) {
        service_window_end(owner->service); return false;
    }
    d->busy = true;
    return true;
}
static void end(struct virtio_disk *d, struct request_owner owner) {
    address_space(owner.original_cr3);
    d->busy = false;
    service_window_end(owner.service);
}
static void checkpoint(struct virtio_disk *d, struct request_owner *owner) {
    address_space(owner->original_cr3);
    if (owner->service.held) ++d->service_windows;
    service_window_checkpoint(&owner->service);
}
/* Called only with IRQs excluded and kernel CR3. All failure paths invalidate
 * identity first, reset the device, then free caller backing after reset proof. */
static void retire(struct virtio_disk *d) {
    d->failed = true;
    if (d->block) block_invalidate(d->block);
    if (d->transport && !virtio_pci_stop(d->transport)) {
        d->quarantined = true; return;
    }
    if (d->control) pmm_free_frame(d->control);
    if (d->bounce) pmm_free_frame_range(d->bounce, VBLK_BYTES / 4096);
    d->control = d->bounce = 0;
}

static bool transact(struct virtio_disk *d, struct request_owner *owner,
    uint32_t type, uint64_t sector, uint32_t length) {
    address_space(owner->kernel_cr3);
    /* Device resizing is an identity change for a mounted view. Do not silently
     * continue against either a smaller or a larger medium. */
    uint64_t capacity;
    if (!virtio_pci_config_read64(d->transport, 0, &capacity) || capacity != d->sectors) goto failed;
    struct block_request *request = pmm_phys_to_virt(d->control);
    volatile uint8_t *status = (uint8_t *)request + 16;
    *request = (struct block_request){type, 0, type == VBLK_FLUSH ? 0 : sector};
    *status = 0xff;
    struct virtio_pci_buffer buffers[3] = {{d->control, sizeof(*request), false}};
    unsigned count = 1;
    if (length) buffers[count++] = (struct virtio_pci_buffer){d->bounce, length, type == VBLK_IN};
    buffers[count++] = (struct virtio_pci_buffer){d->control + 16, 1, true};
    uint16_t token;
    if (!virtio_pci_submit(d->transport, 0, buffers, (uint16_t)count, &token)) goto failed;
    ++d->commands;
    if (type == VBLK_IN) ++d->reads;
    else if (type == VBLK_OUT) ++d->writes;
    else ++d->flushes;
    address_space(owner->original_cr3);
    bool timed = timer_reference_ready();
    uint64_t started = timed ? timer_monotonic_us() : 0;
    for (unsigned iteration = 0; iteration < VBLK_POLL_LIMIT;) {
        address_space(owner->kernel_cr3);
        for (unsigned batch = 0; batch < VBLK_POLL_BATCH && iteration < VBLK_POLL_LIMIT; ++batch, ++iteration) {
            struct virtio_pci_completion completion;
            int result = virtio_pci_poll(d->transport, 0, &completion);
            if (result < 0) goto failed;
            if (result > 0) {
                uint32_t expected = type == VBLK_IN ? length + 1 : 1;
                if (completion.token != token || completion.length != expected || *status != 0) goto failed;
                address_space(owner->original_cr3);
                return true;
            }
            block_pause();
        }
        address_space(owner->original_cr3);
        if (timed && timer_monotonic_us() - started >= VBLK_TIMEOUT_US) break;
        checkpoint(d, owner);
    }
failed:
    address_space(owner->kernel_cr3);
    ++d->failures;
    retire(d);
    address_space(owner->original_cr3);
    return false;
}

static bool transfer(struct block_device *block, uint64_t offset, void *buffer, uint64_t length, bool write) {
    if (!block || !buffer || !block->private_data || block->offline) return false;
    struct virtio_disk *d = block->private_data;
    uint64_t bytes = d->sectors * 512;
    if (offset > bytes || length > bytes - offset || (write && !d->writable)) return false;
    struct request_owner owner;
    if (!begin(d, &owner)) return false;
    bool ok = false;
    uint64_t done = 0;
    uint8_t *bounce = pmm_phys_to_virt(d->bounce);
    while (done < length) {
        uint64_t absolute = offset + done;
        unsigned within = (unsigned)(absolute & 511);
        uint64_t remaining = length - done;
        uint32_t amount, transfer_bytes;
        if (within || remaining < 512) {
            amount = 512 - within;
            if (amount > remaining) amount = (uint32_t)remaining;
            transfer_bytes = 512;
        } else {
            amount = remaining > VBLK_BYTES ? VBLK_BYTES : (uint32_t)remaining & ~511u;
            transfer_bytes = amount;
        }
        uint64_t sector = absolute / 512;
        /* Partial writes preserve the rest of the sector in the private bounce. */
        if (!write || amount != transfer_bytes) {
            if (!transact(d, &owner, VBLK_IN, sector, transfer_bytes)) goto finished;
        }
        if (write) {
            copy(bounce + within, (const uint8_t *)buffer + done, amount);
            if (!transact(d, &owner, VBLK_OUT, sector, transfer_bytes)) goto finished;
        } else copy((uint8_t *)buffer + done, bounce + within, amount);
        done += amount;
        if (done < length) checkpoint(d, &owner);
    }
    ok = true;
finished:
    end(d, owner);
    return ok;
}
static bool read_blocks(struct block_device *b, uint64_t off, void *buf, uint64_t len) {
    return transfer(b, off, buf, len, false);
}
static bool write_blocks(struct block_device *b, uint64_t off, const void *buf, uint64_t len) {
    return transfer(b, off, (void *)buf, len, true);
}
static bool flush_blocks(struct block_device *block) {
    if (!block || block->offline || !block->private_data) return false;
    struct virtio_disk *d = block->private_data;
    if (!d->flush || !d->writable) return false;
    struct request_owner owner;
    if (!begin(d, &owner)) return false;
    bool ok = transact(d, &owner, VBLK_FLUSH, 0, 0);
    end(d, owner); return ok;
}

unsigned virtio_block_init(void) {
    uint64_t kernel = vmm_kernel_address_space();
    if (attempted || !kernel || vmm_current_address_space() != kernel) return 0;
    attempted = true;
    unsigned slots = 0;
    for (uint64_t i = 0; i < pci_device_count() && slots < VBLK_MAX_DISKS; ++i) {
        const struct pci_device *pci = pci_device_at(i);
        if (!pci || pci->vendor_id != 0x1af4 || pci->device_id != 0x1042 || (pci->header_type & 0x7f)) continue;
        struct virtio_disk *d = &disks[slots++];
        struct service_window service = service_window_begin();
        if (!service.held && scheduler_io_active()) { service_window_end(service); continue; }
        d->used = true;
        d->name[0] = 'v'; d->name[1] = 'i'; d->name[2] = 'r'; d->name[3] = 't';
        d->name[4] = 'i'; d->name[5] = 'o'; d->name[6] = (char)('0' + slots - 1); d->name[7] = 0;
        if (!virtio_pci_open_boot_block(pci, VBLK_F_RO | VBLK_F_FLUSH, 0, 8, &d->transport)) goto failed;
        uint64_t features = virtio_pci_features(d->transport);
        d->flush = (features & VBLK_F_FLUSH) != 0;
        d->writable = d->flush && !(features & VBLK_F_RO);
        if (!virtio_pci_config_read64(d->transport, 0, &d->sectors) || !d->sectors || d->sectors > UINT64_MAX / 512) goto failed;
        if (!virtio_pci_queue_setup(d->transport, 0, 8)) goto failed;
        d->control = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        d->bounce = pmm_alloc_frames_tagged(VBLK_BYTES / 4096, PMM_FRAME_DMA);
        if (!d->control || !d->bounce) goto failed;
        zero(pmm_phys_to_virt(d->control), 4096);
        zero(pmm_phys_to_virt(d->bounce), VBLK_BYTES);
        if (!virtio_pci_start(d->transport)) goto failed;
        d->block = block_register(d->name, 512, d->sectors, read_blocks,
            d->writable ? write_blocks : 0, d);
        if (!d->block) goto failed;
        d->block->max_transfer_bytes = VBLK_BYTES;
        d->block->flush = d->writable ? flush_blocks : 0;
        ++disk_count;
        service_window_end(service);
        /* Partition discovery may invoke the synchronous callback in its own
         * service gate. No initialization gate or borrowed CR3 survives here. */
        if (!block_register_gpt_partitions(d->block)) block_register_mbr_partitions(d->block);
        continue;
failed:
        retire(d);
        service_window_end(service);
    }
    return disk_count;
}
void virtio_block_print_status(void) {
    for (unsigned i = 0; i < VBLK_MAX_DISKS; ++i) {
        const struct virtio_disk *d = &disks[i];
        if (!d->used) continue;
        struct virtio_pci_admission admission;
        virtio_pci_get_admission(d->transport, &admission);
        console_printf("virtio-block: %s sectors=%u writable=%s flush=%s online=%s quarantine=%s transport=%s admission=%u selector=%u firmware_reset=%u reset_ok=%u commands=%u reads=%u writes=%u flushes=%u failures=%u windows=%u\n",
            d->name, d->sectors, d->writable ? "yes" : "no", d->flush ? "yes" : "no",
            d->block && !d->failed ? "yes" : "no", d->quarantined ? "yes" : "no",
            virtio_pci_status_name(virtio_pci_get_status(d->transport)), (uint64_t)admission.status,
            (uint64_t)admission.selector_ok, (uint64_t)admission.firmware_reset, (uint64_t)admission.reset_ok, d->commands, d->reads, d->writes, d->flushes, d->failures, d->service_windows);
    }
}
