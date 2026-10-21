#include <srvros/mt76_probe.h>

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool mt76_probe_identity(mt76_probe_read read, void *context, struct mt76_identity *out) {
    if (!out) return false;
    *out = (struct mt76_identity){0};
    if (!read) return false;
    uint32_t values[2];
    for (unsigned i = 0; i < 2; ++i) {
        /* Vendor/device IN, MULTI_READ, value=address high word,
         * index=address low word: ASIC_VERSION 0000, MAC_CSR0 1000. */
        const uint8_t setup[8] = {0xc0, 0x07, 0, 0, 0, i ? 0x10 : 0, 4, 0};
        uint8_t data[4] = {0};
        if (read(context, setup, data) != 4) return false;
        values[i] = le32(data);
        if (values[i] == 0 || values[i] == UINT32_MAX) return false;
    }
    *out = (struct mt76_identity){values[0], values[1]};
    return true;
}

bool mt76_firmware_layout(const uint8_t *data, size_t size, struct mt76_firmware_layout *out) {
    if (!out) return false;
    *out = (struct mt76_firmware_layout){0};
    if (!data || size < 32) return false;
    size_t ilm = le32(data), dlm = le32(data + 4);
    /* Subtract before comparing: do not overflow when validating hostile lengths. */
    if (ilm <= 64 || ilm > size - 32 || dlm != size - 32 - ilm) return false;
    *out = (struct mt76_firmware_layout){
        .ivb_offset = 32, .ivb_size = 64,
        .ilm_offset = 96, .ilm_size = ilm - 64,
        .dlm_offset = 32 + ilm, .dlm_size = dlm,
        .build_version = (uint16_t)(data[8] | ((uint16_t)data[9] << 8)),
        .firmware_version = (uint16_t)(data[10] | ((uint16_t)data[11] << 8)),
    };
    return true;
}
