#ifndef SRVROS_MT76_RADIO_H
#define SRVROS_MT76_RADIO_H
#include <srvros/mt76_bringup.h>
/* First initialization prerequisite only: no RF/channel/power programming.
 * authorize must confirm exclusive live transport, pinned firmware readiness,
 * and the captured descriptor/identity. Called before every I/O or delay.
 * Callbacks must bound waits and quarantine DMA on uncertain failure.
 * A failure after reset assertion may leave reset asserted; do not retry or
 * clear this session without transport teardown and a new hardware lifecycle. */
struct mt76_radio_io {
    struct mt76_register_io registers;
    bool (*authorize)(void *context);
};
enum mt76_radio_status {
    MT76_RADIO_OK, MT76_RADIO_REFUSED, MT76_RADIO_IO,
    MT76_RADIO_TIMEOUT, MT76_RADIO_CANCELLED, MT76_RADIO_STATE
};
struct mt76_radio_prepare {
    bool attempted, reset_attempted, reset_released, prepared;
    uint32_t reads, writes, last_address, last_value, delay_us;
};
enum mt76_radio_status mt76_radio_prepare_reset(struct mt76_radio_prepare *session,
    const struct mt76_radio_io *io, const struct mt76_identity *identity);
#endif
