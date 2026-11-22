#ifndef SRVROS_MT76_PROBE_H
#define SRVROS_MT76_PROBE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MT76_BAND_2GHZ 1u
#define MT76_BAND_5GHZ 2u
/* Assembled-model capability, not generic chipset/EEPROM hints or RF permission. */
unsigned mt76_probe_bands(uint16_t vendor,uint16_t product,uint16_t revision);

bool mt76_probe_config(const uint8_t *data, size_t size);

struct mt76_identity { uint32_t asic, mac; };
/* Synchronous transport: exact received length, or negative failure. It must
 * bound its wait and retain/quarantine DMA after timeout. No retries here.
 * Setup is eight USB wire bytes. Only IN requests are generated. */
typedef int (*mt76_probe_read)(void *context, const uint8_t setup[8], uint8_t data[4]);
/* Does not classify a chipset or authorize initialization/transmission.
 * Caller must first gate on the captured USB identity/topology. */
bool mt76_probe_identity(mt76_probe_read read, void *context, struct mt76_identity *out);

struct mt76_firmware_layout {
    size_t ivb_offset, ivb_size, ilm_offset, ilm_size, dlm_offset, dlm_size;
    uint16_t build_version, firmware_version;
};
/* Structural validation only. Not hash/authenticity, licensing, ASIC matching,
 * calibration or permission to upload. No pointers retained, no hardware I/O. */
bool mt76_firmware_layout(const uint8_t *data, size_t size, struct mt76_firmware_layout *out);
#endif
