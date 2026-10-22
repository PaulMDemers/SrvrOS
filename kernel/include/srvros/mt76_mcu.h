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
#define MT76_MCU_REGISTER_PAIRS_MAX 24u
struct mt76_mcu_register_pair { uint32_t address, value; };
/* Pure packet builder for WLAN register offsets [0,0x3ffc], aligned to4.
 * Emits CMD_RANDOM_WRITE=12 or CMD_RANDOM_READ=10 with base0x410000.
 * Each read pair must have value0. Input/output must not overlap.
 * All pairs are validated before output changes. This is NOT an address/value
 * allowlist: native callers must authorize the exact table and current phase.
 * Sequence0/no-ack and implicit chunking are intentionally unsupported. */
size_t mt76_mcu_register_packet(bool write, uint8_t sequence,
    const struct mt76_mcu_register_pair *pairs, size_t count,
    uint8_t *out, size_t capacity);
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
/* Exact native contract for the fixed sequence2 MAC_SYS_CTRL=0 write.
 * Not a general register-write or multi-pair acknowledgement validator. */
bool mt76_mcu_validate_disabled_write_reply(const uint8_t *data, size_t length);
/* Exact sequence3 reply for the two-entry timing fixture, not other tables. */
bool mt76_mcu_validate_timing_reply(const uint8_t *data, size_t length);
struct mt76_mcu_timing_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*read)(void *context, uint32_t address, uint32_t *value);
    int (*exchange)(void *context, const uint8_t *packet, size_t length,
        uint8_t *reply, size_t capacity);
};
struct mt76_mcu_timing {
    bool attempted, write_attempted, ack_validated, complete;
    uint32_t before[2], after[2], mac_before, mac_after, reads;
    int received;
};
enum mt76_mcu_timing_status {
    MT76_TIMING_OK, MT76_TIMING_REFUSED, MT76_TIMING_IO,
    MT76_TIMING_CANCELLED, MT76_TIMING_STATE, MT76_TIMING_REPLY
};
/* Fixed 1100=33a41010,1104=209, sequence3 only. authorize must pin native
 * identity/firmware, prepared reset, strict seq1/2 replies and exclusive live
 * transport. Callbacks bound waits and retain/quarantine uncertain DMA.
 * One attempt per hardware lifecycle, including failure; never reset to retry.
 * No RF, power, channel, TX/RX enable or arbitrary table inputs. */
enum mt76_mcu_timing_status mt76_mcu_apply_timing(struct mt76_mcu_timing *session,
    const struct mt76_mcu_timing_io *io);
/* Fixed MAC basics plan only: rates, RX filter and maximum frame length.
 * Two pairs per command, sequences4/5; requires completed timing initialization.
 * No caller-defined tables or TX/RX enable. Exact replies and register readbacks accepted on the captured Pro adapter
 * in release190ab5353ae2f8b6; other identities remain unsupported. */
#define MT76_MCU_MAC_BATCHES 2u
struct mt76_mcu_mac {
    bool attempted, complete;
    unsigned batches_attempted, batches_completed, ack_mask, reads;
    uint32_t before[4], after[4], mac_before, mac_after;
    int received;
};
size_t mt76_mcu_mac_packet(unsigned batch, uint8_t *out, size_t capacity);
bool mt76_mcu_validate_mac_reply(unsigned batch, const uint8_t *data, size_t length);
/* Same callback obligations as timing IO, plus proven timing completion and
 * exclusive unused sequences4/5. No retries after any attempted operation. */
enum mt76_mcu_timing_status mt76_mcu_apply_mac_basics(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);
/* Serialized session: never recycle the four-bit sequence space. Exhaustion
 * requires a new, proven-drained transport session, not just clearing this
 * struct. An uncertain transfer poisons the session until transport teardown. */
struct mt76_mcu_session { uint8_t issued, pending; bool poisoned; };
bool mt76_mcu_begin(struct mt76_mcu_session *session, uint8_t *sequence);
bool mt76_mcu_retire(struct mt76_mcu_session *session, uint8_t sequence,
    bool validated_completion);
#endif
