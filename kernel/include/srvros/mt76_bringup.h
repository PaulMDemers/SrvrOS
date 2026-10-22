#ifndef SRVROS_MT76_BRINGUP_H
#define SRVROS_MT76_BRINGUP_H
#include <srvros/mt76_probe.h>
#define MT76_EFUSE_BYTES 512
/* Each operation must have a bounded wait. Failed/uncertain USB operations
 * must quarantine DMA in the transport; the engine never retries them.
 * delay returns false on cancellation. No callback may log calibration bytes. */
struct mt76_register_io {
    void *context;
    bool (*read)(void *, uint32_t address, uint32_t *value);
    bool (*write)(void *, uint32_t address, uint32_t value);
    bool (*delay)(void *, uint32_t microseconds);
};
enum mt76_capture_status {
    MT76_CAPTURE_OK, MT76_CAPTURE_REFUSED, MT76_CAPTURE_IO,
    MT76_CAPTURE_BUSY, MT76_CAPTURE_TIMEOUT, MT76_CAPTURE_CANCELLED,
    MT76_CAPTURE_EMPTY
};
struct mt76_efuse_capture {
    uint8_t bytes[MT76_EFUSE_BYTES]; /* Private data, includes hardware MAC. */
    uint32_t unmapped_blocks;
};
/* Logical read only. Restricts identity to the captured pair. No programming
 * mode, firmware upload or transmitter operations. Success is raw capture,
 * not validation of EEPROM version/calibration/regulatory data. */
enum mt76_capture_status mt76_capture_efuse(const struct mt76_register_io *io,
    const struct mt76_identity *identity, struct mt76_efuse_capture *out);

/* Pure ordered plan: bulk ILM, bulk DLM, then activation IVB control payload.
 * Caller must verify pinned hash, device/ownership and every transfer before
 * advancing. This function does not execute or authorize any transfer. */
enum mt76_fw_segment { MT76_FW_ILM, MT76_FW_DLM, MT76_FW_IVB };
struct mt76_fw_chunk {
    enum mt76_fw_segment segment;
    size_t file_offset, bytes, padded_bytes, wire_bytes;
    uint32_t destination;
};
bool mt76_firmware_chunk(const uint8_t *image, size_t size, size_t index,
    struct mt76_fw_chunk *out);
/* Transport-independent upload into an ALREADY prepared MCU/FCE. authorize
 * must pin the image/hash and exact device and confirm exclusive prepared DMA.
 * All callbacks are bounded; transport retains/quarantines scratch DMA on
 * uncertainty. Keep image and scratch alive until transport teardown. This API
 * does not reset/configure hardware, package firmware or authorize a radio. */
#define MT76_FW_PACKET_MAX 0x38f8u
struct mt76_firmware_io {
    void *context;
    bool (*authorize)(void *, const uint8_t *, size_t, const struct mt76_identity *);
    bool (*fce_write)(void *, uint32_t, uint32_t);
    bool (*read)(void *, uint32_t, uint32_t *);
    bool (*write)(void *, uint32_t, uint32_t);
    int (*bulk)(void *, const uint8_t *, size_t);
    int (*activate)(void *, const uint8_t *, size_t);
    bool (*delay)(void *, uint32_t);
};
struct mt76_firmware_session { bool attempted; };
enum mt76_upload_status { MT76_UPLOAD_OK, MT76_UPLOAD_REFUSED, MT76_UPLOAD_IO,
    MT76_UPLOAD_CANCELLED, MT76_UPLOAD_TIMEOUT };
size_t mt76_firmware_packet(const uint8_t *image, size_t size, size_t index,
    uint8_t *packet, size_t capacity);
enum mt76_upload_status mt76_upload_firmware(struct mt76_firmware_session *session,
    const struct mt76_firmware_io *io, const struct mt76_identity *identity,
    const uint8_t *image, size_t size, uint8_t *scratch, size_t capacity);
/* One-shot preparation; caller must own the device and validate firmware
 * before invoking. Reset callback is vendor device-mode 1, no data stage.
 * mac_startup explicitly selects temporary upstream MAC TX/RX enable, followed
 * by verified disable before prepared=true. No RF register writes.
 * A failure leaves prepared=false and must quarantine
 * the native transport; repeating preparation requires physical recovery. */
struct mt76_prepare_session { bool attempted, prepared, mac_startup; };
struct mt76_prepare_io {
    struct mt76_register_io registers;
    bool (*reset)(void *);
};
enum mt76_upload_status mt76_prepare_firmware(struct mt76_prepare_session *session,
    const struct mt76_prepare_io *io, const struct mt76_identity *identity);
bool mt76_firmware_verified(const uint8_t *image, size_t size);
#endif
