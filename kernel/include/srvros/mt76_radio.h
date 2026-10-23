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
/* Read-only BBP prerequisite; caller pins completed frame-format setup and
 * exclusive live transport. No writes, RF activation or arbitrary reads.
 * At most20 CORE0 reads; every callback is authorized and transport-bounded. */
struct mt76_bbp_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*read)(void *context,uint32_t address,uint32_t *value);
};
struct mt76_bbp_probe {
    bool attempted, complete;
    unsigned reads, core_reads;
    uint32_t mac_before,mac_after,status_before,status_after,pbf,core;
};
enum mt76_radio_status mt76_bbp_probe_ready(struct mt76_bbp_probe *session,
    const struct mt76_bbp_io *io);
#endif
