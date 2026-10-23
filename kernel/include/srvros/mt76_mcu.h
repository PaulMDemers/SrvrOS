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
    uint32_t before[4], after[4], mac_before, mac_after, core_before, core_after;
    int received;
};
size_t mt76_mcu_mac_packet(unsigned batch, uint8_t *out, size_t capacity);
bool mt76_mcu_validate_mac_reply(unsigned batch, const uint8_t *data, size_t length);
/* Same callback obligations as timing IO, plus proven timing completion and
 * exclusive unused sequences4/5. No retries after any attempted operation. */
enum mt76_mcu_timing_status mt76_mcu_apply_mac_basics(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);
/* Fixed frame-format plan: 0250=0,0260=0,1410=1ff,150c=3, sequences6/7.
 * Same one-attempt and IO rules; caller must require completed MAC basics and
 * unused sequences6/7. Exact replies/readbacks accepted on the captured adapter
 * in release54d6def400fd2a6c; other identities remain unsupported.
 * No arbitrary addresses/values or MAC enable, RF, power or key writes. */
size_t mt76_mcu_format_packet(unsigned batch,uint8_t *out,size_t capacity);
bool mt76_mcu_validate_format_reply(unsigned batch,const uint8_t *data,size_t length);
enum mt76_mcu_timing_status mt76_mcu_apply_mac_format(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);
/* Fixed initial BBP CORE plan: 2004=2,2010=0,2060=0,2080=4003000a.
 * Sequences8/9; caller pins completed frame format and native BBP readiness.
 * Each batch also requires captured CORE0=f000f200 before/after. No arbitrary
 * table input or RF/channel/power/MAC-enable writes. Native accepted in release5f7b66b688f803cb. */
size_t mt76_mcu_bbp_packet(unsigned batch,uint8_t *out,size_t capacity);
bool mt76_mcu_validate_bbp_reply(unsigned batch,const uint8_t *data,size_t length);
enum mt76_mcu_timing_status mt76_mcu_apply_bbp_core(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);
/* Fixed CORE42/44, IBI11 and AGC0; accepted in release1874caf6830abdc0. */
size_t mt76_mcu_bbp_front_packet(unsigned batch,uint8_t *out,size_t capacity);
bool mt76_mcu_validate_bbp_front_reply(unsigned batch,const uint8_t *data,size_t length);
enum mt76_mcu_timing_status mt76_mcu_apply_bbp_front(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);
/* Fixed AGC1/2/15/22; accepted in releaseb245ac895a7d9396. */
size_t mt76_mcu_bbp_agc_packet(unsigned batch,uint8_t *out,size_t capacity);
bool mt76_mcu_validate_bbp_agc_reply(unsigned batch,const uint8_t *data,size_t length);
enum mt76_mcu_timing_status mt76_mcu_apply_bbp_agc(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);

/* Comparison only: rewrite accepted AGC values in one four-pair command,
 * sequence14, replacing the two-command AGC plan after completed IBI/AGC0.
 * Changed targets last; native batching pending. */
size_t mt76_mcu_bbp_batch_packet(unsigned batch,uint8_t *out,size_t capacity);
bool mt76_mcu_validate_bbp_batch_reply(unsigned batch,const uint8_t *data,size_t length);
enum mt76_mcu_timing_status mt76_mcu_apply_bbp_batch(struct mt76_mcu_mac *session,
    const struct mt76_mcu_timing_io *io);

/* Twelve fixed four-pair plans, sequences4..15; no caller-supplied tables.
 * First eleven accepted in release6477738b1d3cf7d5; phase11 failed native TXBE10 readback.
 * Transport enforces phase order, prior completion and readiness before phase2. */
#define MT76_MCU_COMPACT_PHASES 12u
bool mt76_mcu_compact_read_allowed(unsigned phase,uint32_t address);
size_t mt76_mcu_compact_packet(unsigned phase,uint8_t *out,size_t capacity);
bool mt76_mcu_validate_compact_reply(unsigned phase,const uint8_t *data,size_t length);
enum mt76_mcu_timing_status mt76_mcu_apply_compact(unsigned phase,
    struct mt76_mcu_mac *session,const struct mt76_mcu_timing_io *io);

/* Read-only observation after accepted phases0..10, never phase11. Eight
 * fixed samples; guards before/after each. Values are observations, not write
 * acceptance or a claim about register semantics. No exchange/write callback. */
#define MT76_TXBE_SAMPLES 8u
struct mt76_txbe_observation {
    bool attempted, complete;
    unsigned reads, samples;
    uint32_t values[MT76_TXBE_SAMPLES][8];
};
struct mt76_txbe_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*read)(void *context,uint32_t address,uint32_t *value);
};
bool mt76_txbe_read_allowed(uint32_t address);
enum mt76_mcu_timing_status mt76_txbe_observe(struct mt76_txbe_observation *s,
    const struct mt76_txbe_io *io);

/* Isolated TXBE9=20000000 at sequence15, between two fixed observations.
 * Requires eleven accepted phases and unused sequence15. No TXBE10 write.
 * Strict reply and TXBE9 readback; other TXBE values are observations only. */
struct mt76_txbe_first {
    bool attempted, write_attempted, ack_validated, complete;
    int received;
    struct mt76_txbe_observation before, after;
};
size_t mt76_txbe_first_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_txbe_first_apply(struct mt76_txbe_first *s,
    const struct mt76_mcu_timing_io *io);

/* Alternative, mutually exclusive lifecycle: TXBE10=0 at sequence15,
 * requiring TXBE9=0 before/after. Same bounded samples and no-retry rules. */
size_t mt76_txbe_second_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_txbe_second_apply(struct mt76_txbe_first *s,
    const struct mt76_mcu_timing_io *io);

/* Serialized session: never recycle the four-bit sequence space. Exhaustion
 * requires a new, proven-drained transport session, not just clearing this
 * struct. An uncertain transfer poisons the session until transport teardown. */
struct mt76_mcu_session { uint8_t issued, pending; bool poisoned; };
bool mt76_mcu_begin(struct mt76_mcu_session *session, uint8_t *sequence);
bool mt76_mcu_retire(struct mt76_mcu_session *session, uint8_t sequence,
    bool validated_completion);
#endif
