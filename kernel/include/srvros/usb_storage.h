#ifndef SRVROS_USB_STORAGE_H
#define SRVROS_USB_STORAGE_H
#include <srvros/block.h>
/* Synchronous BOT transport. The controller owns/pins its DMA bounce buffer.
 * A false return makes this device unusable until the next boot. */
typedef bool (*usb_bulk_fn)(void *, bool, void *, uint32_t, uint32_t *);
struct usb_storage {
    void *context;
    usb_bulk_fn bulk;
    struct block_device *block;
    uint32_t tag, sector_size;
    uint64_t sectors, commands, errors;
    bool failed, busy, services;
    char name[12];
    uint8_t data[4096];
};
bool usb_storage_is_device(struct block_device *device);
bool usb_storage_enable_volume_writes(struct block_device *device);
bool usb_storage_disable_volume_writes(struct block_device *device);
bool usb_storage_attach(struct usb_storage *, void *, usb_bulk_fn, bool writable);
#endif
