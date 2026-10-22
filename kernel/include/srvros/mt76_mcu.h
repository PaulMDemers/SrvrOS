#ifndef SRVROS_MT76_MCU_H
#define SRVROS_MT76_MCU_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MT76_MCU_PAYLOAD_MAX 192u
#define MT76_MCU_PACKET_MAX 200u
#define MT76_MCU_RESPONSE_MAX 1024u
/* Pure wire codec, NOT device/command authorization. Distinct payload/output
 * buffers required. The transport must pin identity, firmware and endpoint
 * ownership and retain DMA on uncertainty. Sequence 1..15 requests a reply;
 * zero is deliberately unsupported. No radio state changes occur here. */
size_t mt76_mcu_encode(uint8_t command, uint8_t sequence,
    const uint8_t *payload, size_t length, uint8_t *out, size_t capacity);
/* Fixed Q_SELECT=1,value=1,sequence=0: route replies; no reply requested.
 * This is the only no-reply command this diagnostic exposes. */
size_t mt76_mcu_select_response_queue(uint8_t *out, size_t capacity);
struct mt76_mcu_response {
    uint8_t sequence, event;
    uint16_t reported_length;
};
/* Decode only. A matching sequence is NOT successful command completion:
 * the caller must validate event, command-specific body and reported length
 * against the native observed response contract before accepting it. */
bool mt76_mcu_decode(const uint8_t *data, size_t length,
    struct mt76_mcu_response *out);
/* Exact native contract for the one-shot sequence1 MAC_CSR0 read on the
 * captured MT7610 adapter. Requires the observed header/address/value/trailer;
 * never use this as validation for arbitrary MCU commands or other identities. */
bool mt76_mcu_validate_identity_reply(const uint8_t *data, size_t length);
/* Serialized session: never recycle the four-bit sequence space. Exhaustion
 * requires a new, proven-drained transport session, not just clearing this
 * struct. An uncertain transfer poisons the session until transport teardown. */
struct mt76_mcu_session { uint8_t issued, pending; bool poisoned; };
bool mt76_mcu_begin(struct mt76_mcu_session *session, uint8_t *sequence);
bool mt76_mcu_retire(struct mt76_mcu_session *session, uint8_t sequence,
    bool validated_completion);
#endif
