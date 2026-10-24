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

/* Physical usage-map read only: exactly two blocks at 1e0 and 1f0.
 * Mode1 is read, modes2/3 are never generated.
 * authorize must pin identity, exclusive ownership and current initialization.
 * Each callback is bounded; failures are one-shot with no transfer retry. */
struct mt76_efuse_map_io {struct mt76_register_io registers;bool (*authorize)(void *);};
struct mt76_efuse_map {
    bool attempted,complete,valid;
    unsigned reads,writes,polls,blocks,zero_count,first_zero,last_zero,free_span,reference_used;
};
/* Independent native transport sequencing. Zero-init once, terminal on error.
 * begin consumes admission BEFORE submission; finish consumes one completion.
 * No private map data is retained here. */
struct mt76_efuse_map_gate {
    bool pending,failed;
    unsigned phase,block,submitted,polls;
    uint32_t control,command,mac[4],core[4],dma[2];
};
bool mt76_efuse_map_begin(struct mt76_efuse_map_gate *,bool write,uint32_t address,uint32_t value);
bool mt76_efuse_map_finish(struct mt76_efuse_map_gate *,bool ok,uint32_t value);
bool mt76_efuse_map_delay(struct mt76_efuse_map_gate *,uint32_t us);
bool mt76_efuse_map_command(uint32_t previous,unsigned block,uint32_t *command);
bool mt76_efuse_map_analyze(const uint8_t *bytes,size_t length,struct mt76_efuse_map *state);
enum mt76_capture_status mt76_capture_efuse_map(const struct mt76_efuse_map_io *io,
    const struct mt76_identity *identity,struct mt76_efuse_map *state);

/* Private address setup plan after verified physical map and a fresh mapped
 * logical block0. The 16-byte header contains the factory address at4..9;
 * never log header, command/readback values or transport buffers. Invalid
 * factory addresses refuse; this diagnostic does not generate replacements.
 * Caller must independently enforce ownership and exact native I/O admission.
 * Result contains counters only, no address bytes. */
struct mt76_mac_address_result {bool attempted,complete;unsigned operations,reads,writes,verified;};
struct mt76_mac_address_io {
    void *context;
    bool (*authorize)(void *);
    bool (*transfer)(void *,unsigned operation,bool write,uint32_t address,uint32_t *value);
};
bool mt76_mac_address_header_valid(const uint8_t *header,size_t size);
bool mt76_mac_address_target(unsigned target,const uint8_t *header,size_t size,uint32_t previous,uint32_t *address,uint32_t *value);
enum mt76_capture_status mt76_mac_address_apply(struct mt76_mac_address_result *,
    const struct mt76_mac_address_io *,const struct mt76_efuse_map *,const uint8_t *header,size_t size);

/* Explicit captured7610 readback profile: keep write100c U2MEff, require
 * read100c upper16zero. All other bits/targets remain exactly verified. */
enum mt76_capture_status mt76_mac_address_apply_profile(struct mt76_mac_address_result *,
    const struct mt76_mac_address_io *,const struct mt76_efuse_map *,const uint8_t *header,size_t size,bool captured_readback);

/* Private logical block0 only. Output is cleared on entry/failure; caller
 * clears its successful output after use. Mode0 is fixed, never programming. */
struct mt76_mac_header_result {bool attempted,complete,valid;unsigned reads,writes,polls,blocks;};
bool mt76_mac_header_command(uint32_t previous,uint32_t *command);
bool mt76_mac_header_begin(struct mt76_efuse_map_gate *,bool write,uint32_t address,uint32_t value);
bool mt76_mac_header_finish(struct mt76_efuse_map_gate *,bool ok,uint32_t value);
enum mt76_capture_status mt76_capture_mac_header(const struct mt76_efuse_map_io *,
    const struct mt76_identity *,struct mt76_mac_header_result *,uint8_t output[16]);
enum mt76_mac_address_failure {
    MT76_ADDRESS_NONE, MT76_ADDRESS_ADMISSION, MT76_ADDRESS_TRANSPORT,
    MT76_ADDRESS_STATE, MT76_ADDRESS_MAC, MT76_ADDRESS_CORE,
    MT76_ADDRESS_DMA, MT76_ADDRESS_ALL_ONES, MT76_ADDRESS_MISMATCH
};
/* The gate retains only one private previous word; scrub it after use.
 * Comparison receipts exclude factory-address bits. Public controls are
 * recorded only for station/BSSID high words (targets1/3), bits31:16. */
struct mt76_mac_address_gate {
    bool pending,failed,captured_readback;unsigned operations,submitted;
    enum mt76_mac_address_failure failure;
    bool compared,address_match,control_match;
    uint32_t expected_control,observed_control;
    uint32_t previous,mac[40],core[40],dma[20];
};
bool mt76_mac_address_begin(struct mt76_mac_address_gate *,const uint8_t header[16],unsigned op,bool write,uint32_t address,uint32_t value);
bool mt76_mac_address_finish(struct mt76_mac_address_gate *,const uint8_t header[16],bool ok,uint32_t value);

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
