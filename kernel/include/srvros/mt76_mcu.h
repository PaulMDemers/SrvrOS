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
/* Read-only path comparison at unused sequence15; completion requires agreement
 * with all guarded EP0 observations, not acceptance of initialization writes. */
struct mt76_txbe_access {
    bool attempted, request_attempted, reply_validated, complete;
    bool control_attempted, control_written;
    int received;
    uint32_t value;
    unsigned matches;
    struct mt76_txbe_observation before, after;
};
size_t mt76_txbe_access_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_txbe_access_apply(struct mt76_txbe_access *s,
    const struct mt76_mcu_timing_io *io);

struct mt76_txbe_direct_io {
    struct mt76_mcu_timing_io access;
    bool (*write_zero)(void *context);
};
enum mt76_mcu_timing_status mt76_txbe_direct_apply(struct mt76_txbe_access *s,
    const struct mt76_txbe_direct_io *io);
size_t mt76_mcu_clock_pair_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mcu_apply_clock_pair(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
#define MT76_BBP_TABLE_ENTRIES 58
struct mt76_bbp_table {
    bool attempted, complete, ack_validated;
    unsigned reads, chunks_attempted, chunks_completed, mismatches;
    uint32_t before[MT76_BBP_TABLE_ENTRIES], after[MT76_BBP_TABLE_ENTRIES];
    uint32_t mac[4], core[4];
};
uint32_t mt76_bbp_table_address(unsigned index);
uint32_t mt76_bbp_table_value(unsigned index);
bool mt76_bbp_table_read_allowed(uint32_t address);
size_t mt76_bbp_table_packet(unsigned chunk,uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_bbp_table_capture(struct mt76_bbp_table *s,
    const struct mt76_mcu_timing_io *io);
#define MT76_MAC_SNAPSHOT_ENTRIES 63
struct mt76_mac_snapshot {
    bool attempted, complete;
    unsigned reads;
    uint32_t values[MT76_MAC_SNAPSHOT_ENTRIES], mac[2], core[2];
    uint32_t asic[2], repeat_1030;
    unsigned all_ones;
};
uint32_t mt76_mac_snapshot_address(unsigned index);
bool mt76_mac_snapshot_read_allowed(uint32_t address);
enum mt76_mcu_timing_status mt76_mac_snapshot_capture(struct mt76_mac_snapshot *s,
    const struct mt76_mcu_timing_io *io);
enum mt76_mcu_timing_status mt76_mac_raw_capture(struct mt76_mac_snapshot *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_route_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_route_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_policy_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_policy_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_protect_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_protect_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_contention_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_contention_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_txsw_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_txsw_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_vht_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_vht_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_buffer_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_buffer_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_beacon_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_beacon_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io);
size_t mt76_mac_fce_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_fce_observe(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask);
enum mt76_mcu_timing_status mt76_mac_fce_validate(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask);

enum mt76_mcu_timing_status mt76_mac_policy_observe(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask);
enum mt76_mcu_timing_status mt76_mac_policy_validate(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask);

size_t mt76_mac_ampdu_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_mac_ampdu_apply(struct mt76_mcu_mac *s,const struct mt76_mcu_timing_io *io);

#define MT76_BBP_SWITCH_ENTRIES 20
struct mt76_bbp_switch {
    bool attempted, complete;
    unsigned reads, writes_attempted, writes_completed, mismatches;
    uint32_t before[20], after[20], mac[4], core[4], write_mac[20], write_core[20];
};
struct mt76_bbp_switch_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*read)(void *context,uint32_t address,uint32_t *value);
    bool (*write)(void *context,unsigned index,uint32_t address,uint32_t value);
};
uint32_t mt76_bbp_switch_address(unsigned index);
uint32_t mt76_bbp_switch_value(unsigned index);
bool mt76_bbp_switch_read_allowed(uint32_t address);
enum mt76_mcu_timing_status mt76_bbp_switch_apply(struct mt76_bbp_switch *s,const struct mt76_bbp_switch_io *io);
#define MT76_DCOC_ENTRIES 9
struct mt76_dcoc {
    bool attempted, complete, command_attempted, ack_validated;
    unsigned reads, mismatches;
    uint32_t before[9], after[9], mac[4], core[4];
};
uint32_t mt76_dcoc_address(unsigned index);
uint32_t mt76_dcoc_value(unsigned index);
bool mt76_dcoc_read_allowed(uint32_t address);
size_t mt76_dcoc_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_dcoc_capture(struct mt76_dcoc *s,const struct mt76_mcu_timing_io *io);
struct mt76_skey {
    bool attempted,complete;
    unsigned operations,reads,writes,slots,mode_verified,key_zero;
    uint32_t mode_expected;
};
struct mt76_skey_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*transfer)(void *context,unsigned operation,bool write,uint32_t address,uint8_t *data,unsigned length);
};
bool mt76_skey_request(unsigned operation,bool *write,uint32_t *address,unsigned *length);
bool mt76_skey_transfer_valid(const struct mt76_skey *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_skey_reset(struct mt76_skey *s,const struct mt76_skey_io *io);
struct mt76_wcid {
    bool attempted,complete;
    unsigned operations,reads,writes,slots,attributes_zero,addresses_zero;
};
struct mt76_wcid_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*transfer)(void *context,unsigned operation,bool write,uint32_t address,uint8_t *data,unsigned length);
};
bool mt76_wcid_request(unsigned operation,bool *write,uint32_t *address,unsigned *length);
bool mt76_wcid_transfer_valid(const struct mt76_wcid *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_wcid_reset(struct mt76_wcid *s,const struct mt76_wcid_io *io);
struct mt76_mac_tail {bool attempted,complete;unsigned reads,all_ones;uint32_t values[22],mac[2],core[2];};
uint32_t mt76_mac_tail_address(unsigned index);
uint32_t mt76_mac_tail_request(unsigned operation);
enum mt76_mcu_timing_status mt76_mac_tail_capture(struct mt76_mac_tail *s,const struct mt76_mcu_timing_io *io);
struct mt76_mac_fixed {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[4],after[4],dma[4],mac[8],core[8];
};
struct mt76_mac_fixed_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*transfer)(void *context,unsigned operation,bool write,uint32_t address,uint8_t *data,unsigned length);
};
uint32_t mt76_mac_fixed_address(unsigned index);
uint32_t mt76_mac_fixed_initial(unsigned index);
uint32_t mt76_mac_fixed_value(unsigned index);
bool mt76_mac_fixed_request(unsigned operation,bool *write,uint32_t *address,unsigned *length);
bool mt76_mac_fixed_transfer_valid(const struct mt76_mac_fixed *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_mac_fixed_apply(struct mt76_mac_fixed *s,const struct mt76_mac_fixed_io *io);
struct mt76_mac_board {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[4],after[4],dma[4],mac[8],core[8];
};
struct mt76_mac_board_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*transfer)(void *context,unsigned operation,bool write,uint32_t address,uint8_t *data,unsigned length);
};
uint32_t mt76_mac_board_address(unsigned index);
uint32_t mt76_mac_board_initial(unsigned index);
uint32_t mt76_mac_board_value(unsigned index);
bool mt76_mac_board_request(unsigned operation,bool *write,uint32_t *address,unsigned *length);
bool mt76_mac_board_transfer_valid(const struct mt76_mac_board *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_mac_board_apply(struct mt76_mac_board *s,const struct mt76_mac_board_io *io);
struct mt76_ant_snapshot {bool attempted,complete;unsigned reads;uint32_t values[5],mac[2],core[2],dma[2];};
uint32_t mt76_ant_snapshot_address(unsigned index);
uint32_t mt76_ant_snapshot_request(unsigned operation);
enum mt76_mcu_timing_status mt76_ant_snapshot_capture(struct mt76_ant_snapshot *s,const struct mt76_mcu_timing_io *io);
struct mt76_ant_init {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[5],after[5],dma[5],mac[10],core[10];
};
struct mt76_ant_init_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*transfer)(void *context,unsigned operation,bool write,uint32_t address,uint8_t *data,unsigned length);
};
uint32_t mt76_ant_init_address(unsigned index);
uint32_t mt76_ant_init_initial(unsigned index);
uint32_t mt76_ant_init_value(unsigned index);
bool mt76_ant_init_request(unsigned operation,bool *write,uint32_t *address,unsigned *length);
bool mt76_ant_init_transfer_valid(const struct mt76_ant_init *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_ant_init_apply(struct mt76_ant_init *s,const struct mt76_ant_init_io *io);

/* Fixed USB RF0:22 observation only, Linux v7.0 RF map. No arbitrary RF writes.
 * Reply framing observed in release63fb5a9f9addbb21; calibration untested. */
struct mt76_rf_probe {
    bool attempted,request_attempted,reply_valid,complete;
    unsigned reads;
    int received;
    uint32_t mac[2],core[2],dma[2],words[4];
    uint8_t value;
};
size_t mt76_rf_probe_packet(uint8_t *out,size_t capacity);
uint32_t mt76_rf_probe_read_address(unsigned index);
enum mt76_mcu_timing_status mt76_rf_probe_apply(struct mt76_rf_probe *s,
    const struct mt76_mcu_timing_io *io);
/* Fixed MT7610 USB central-bank numeric settings. No caller-supplied table,
 * frequency trim, channel selection or MAC enable. One write/read pair per entry. */
#define MT76_RF_CENTRAL_COUNT 44u
#define MT76_RF_CENTRAL_OPS (MT76_RF_CENTRAL_COUNT*8u)
struct mt76_rf_central {
    bool attempted,complete;
    unsigned operations,reads,exchanges,acks,verified;
    int received;
    uint32_t guards[MT76_RF_CENTRAL_COUNT*2][3],words[4];
    uint8_t values[MT76_RF_CENTRAL_COUNT];
};
uint32_t mt76_rf_central_address(unsigned index);
unsigned mt76_rf_central_value(unsigned index);
unsigned mt76_rf_central_sequence(unsigned index,bool write);
uint32_t mt76_rf_central_guard_address(unsigned operation);
size_t mt76_rf_central_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_central_admit(const struct mt76_rf_central *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_central_apply(struct mt76_rf_central *s,
    const struct mt76_mcu_timing_io *io);
/* Read-only comparison after a deliberately bounded six-write central prefix.
 * Complete means observation only; central initialization remains incomplete. */
struct mt76_rf_six {
    bool attempted,complete;
    unsigned operations,reads,requests;
    int received;
    uint32_t guards[6][3],words[5][4];
    uint8_t values[5];
};
bool mt76_rf_six_prefix_valid(const struct mt76_rf_central *s,enum mt76_mcu_timing_status status);
uint32_t mt76_rf_six_guard_address(unsigned operation);
size_t mt76_rf_six_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_six_admit(const struct mt76_rf_six *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_six_apply(struct mt76_rf_six *s,const struct mt76_mcu_timing_io *io);

/* Fixed read-selector diagnostic, never sets RF-CSR WR or RF data bits.
 * 32 idle and 32 completion polls per target; one attempt, no recovery retry. */
struct mt76_rf_csr {
    bool attempted,complete;
    unsigned operations,reads,writes,index,phase;
    unsigned polls[5][2];
    uint32_t guards[10][3],idle[5],result[5];
};
struct mt76_rf_csr_io {
    void *context;
    bool (*authorize)(void *context);
    bool (*transfer)(void *context,unsigned operation,bool write,uint32_t address,uint32_t *value);
};
bool mt76_rf_six_complete_valid(const struct mt76_rf_six *s);
uint32_t mt76_rf_csr_selector(unsigned index);
bool mt76_rf_csr_request(const struct mt76_rf_csr *s,bool *write,uint32_t *address,uint32_t *value);
enum mt76_mcu_timing_status mt76_rf_csr_apply(struct mt76_rf_csr *s,const struct mt76_rf_csr_io *io);

/* One fixed RF0:6=41 CSR write followed by independent RF0:6 read.
 * Observation only; no table continuation, retries or caller-selected payload. */
struct mt76_rf_write {
    bool attempted,complete;
    unsigned operations,reads,writes,phase,polls[3];
    uint32_t guards[3][3],words[3];
};
bool mt76_rf_csr_zero_valid(const struct mt76_rf_csr *s);
bool mt76_rf_write_request(const struct mt76_rf_write *s,bool *write,uint32_t *address,uint32_t *value);
enum mt76_mcu_timing_status mt76_rf_write_apply(struct mt76_rf_write *s,const struct mt76_rf_csr_io *io);
/* Read-only RF controls after the completed zero-read comparison. */
#define MT76_RF_STATE_COUNT 8
struct mt76_rf_state {
    bool attempted,complete;
    unsigned reads;
    uint32_t guards[2][3],values[MT76_RF_STATE_COUNT];
};
uint32_t mt76_rf_state_address(unsigned index);
uint32_t mt76_rf_state_request(unsigned operation);
bool mt76_rf_state_admit(const struct mt76_rf_state *s);
enum mt76_mcu_timing_status mt76_rf_state_capture(struct mt76_rf_state *s,const struct mt76_mcu_timing_io *io);
/* Observation only: reference-order writes, then a separate read pass. */
#define MT76_RF_TRIM_COUNT 1u
#define MT76_RF_TRIM_COMMANDS 2u
#define MT76_RF_TRIM_OPS 14u
#define MT76_RF_G20_COUNT 19u
#define MT76_RF_G20_COMMANDS (MT76_RF_G20_COUNT*2u)
#define MT76_RF_G20_OPS (MT76_RF_G20_COMMANDS*7u)
#define MT76_RF_VGA_COUNT 35u
#define MT76_RF_VGA_COMMANDS (MT76_RF_VGA_COUNT*2u)
#define MT76_RF_VGA_OPS (MT76_RF_VGA_COMMANDS*7u)
#define MT76_RF_BANK6_COUNT 38u
#define MT76_RF_BANK6_COMMANDS (MT76_RF_BANK6_COUNT*2u)
#define MT76_RF_BANK6_OPS (MT76_RF_BANK6_COMMANDS*7u)
#define MT76_RF_BANK5_COUNT 68u
#define MT76_RF_BANK5_COMMANDS (MT76_RF_BANK5_COUNT*2u)
#define MT76_RF_BANK5_OPS (MT76_RF_BANK5_COMMANDS*7u)
#define MT76_RF_TABLE_COMMANDS (MT76_RF_CENTRAL_COUNT*2u)
#define MT76_RF_TABLE_OPS (MT76_RF_TABLE_COMMANDS*7u)
struct mt76_rf_table {
    bool attempted,complete;
    unsigned operations,reads,exchanges,acks,observed,equal;
    int received;
    uint64_t mismatch_mask,mismatch_hi;
    uint32_t guards[280][3],words[140][4];
    uint8_t values[70];
};
unsigned mt76_rf_table_sequence(unsigned command);
uint32_t mt76_rf_table_guard_address(unsigned operation);
size_t mt76_rf_table_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_table_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_table_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
/* Exact captured central-table proof; RF0:6 is observed zero, not accepted. */
bool mt76_rf_table_zero_profile_valid(const struct mt76_rf_table *s);
uint32_t mt76_rf_bank5_address(unsigned index);
unsigned mt76_rf_bank5_value(unsigned index);
unsigned mt76_rf_bank5_sequence(unsigned command);
uint32_t mt76_rf_bank5_guard_address(unsigned operation);
size_t mt76_rf_bank5_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_bank5_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_bank5_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
/* Complete fixed bank5 command/guard proof and exact68 matching readbacks. */
bool mt76_rf_bank5_exact_profile_valid(const struct mt76_rf_table *s);
uint32_t mt76_rf_bank6_address(unsigned index);
unsigned mt76_rf_bank6_value(unsigned index);
unsigned mt76_rf_bank6_sequence(unsigned command);
uint32_t mt76_rf_bank6_guard_address(unsigned operation);
size_t mt76_rf_bank6_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_bank6_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_bank6_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
/* Exact completed bank6 proof and all38 matching readbacks. */
bool mt76_rf_bank6_exact_profile_valid(const struct mt76_rf_table *s);
uint32_t mt76_rf_vga_address(unsigned index);
unsigned mt76_rf_vga_value(unsigned index);
unsigned mt76_rf_vga_sequence(unsigned command);
uint32_t mt76_rf_vga_guard_address(unsigned operation);
size_t mt76_rf_vga_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_vga_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_vga_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
/* Exact complete VGA proof and all35 matching observations. */
bool mt76_rf_vga_exact_profile_valid(const struct mt76_rf_table *s);
uint32_t mt76_rf_g20_address(unsigned index);
unsigned mt76_rf_g20_value(unsigned index);
unsigned mt76_rf_g20_sequence(unsigned command);
uint32_t mt76_rf_g20_guard_address(unsigned operation);
size_t mt76_rf_g20_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_g20_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_g20_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
bool mt76_rf_g20_exact_profile_valid(const struct mt76_rf_table *s);
uint32_t mt76_rf_trim_address(unsigned index);
unsigned mt76_rf_trim_value(unsigned index);
unsigned mt76_rf_trim_sequence(unsigned command);
uint32_t mt76_rf_trim_guard_address(unsigned operation);
size_t mt76_rf_trim_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_trim_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_trim_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
/* Fixed DAC power-up pulse and VCO initiation; final reads are observations. */
#define MT76_RF_CAL_COMMANDS 10u
#define MT76_RF_CAL_OPS 70u
struct mt76_rf_cal {
    bool attempted,complete;
    unsigned operations,reads,exchanges,acks,observed;
    int received;
    uint32_t guards[20][3],words[10][4];
};
bool mt76_rf_trim_exact_profile_valid(const struct mt76_rf_table *s);
unsigned mt76_rf_cal_sequence(unsigned command);
uint32_t mt76_rf_cal_address(unsigned command);
bool mt76_rf_cal_write(unsigned command);
uint32_t mt76_rf_cal_guard_address(unsigned operation);
bool mt76_rf_cal_admit(const struct mt76_rf_cal *s,unsigned operation);
size_t mt76_rf_cal_packet(const struct mt76_rf_cal *s,unsigned operation,uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_rf_cal_capture(struct mt76_rf_cal *s,const struct mt76_mcu_timing_io *io);
/* Read-only delayed VCO observations; no retry or additional RF write. */
#define MT76_RF_WAIT_COMMANDS 6u
#define MT76_RF_WAIT_OPS 42u
struct mt76_rf_wait {
    bool attempted,complete;
    unsigned operations,reads,exchanges,waits,delay_us,first_clear;
    int received;
    uint32_t guards[12][3],words[6][4];
};
struct mt76_rf_wait_io {struct mt76_mcu_timing_io command;bool (*delay)(void *,uint32_t);};
bool mt76_rf_cal_pending_profile_valid(const struct mt76_rf_cal *s);
/* Exact captured early-clear variant; does not replace pending admission. */
bool mt76_rf_cal_cleared_profile_valid(const struct mt76_rf_cal *s);
unsigned mt76_rf_wait_sequence(unsigned command);
unsigned mt76_rf_wait_delay(unsigned sample);
uint32_t mt76_rf_wait_guard_address(unsigned operation);
bool mt76_rf_wait_delay_admit(const struct mt76_rf_wait *s);
bool mt76_rf_wait_admit(const struct mt76_rf_wait *s,unsigned operation);
size_t mt76_rf_wait_packet(const struct mt76_rf_wait *s,unsigned operation,uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_rf_wait_capture(struct mt76_rf_wait *s,const struct mt76_rf_wait_io *io);
/* Captured MT7610 single-chain baseband path RMW; MAC remains disabled. */
struct mt76_path {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[2],after[2],guards[4][3];
};
struct mt76_path_io {void *context;bool (*authorize)(void *);bool (*transfer)(void *,unsigned,bool,uint32_t,uint8_t *,unsigned);};
bool mt76_rf_wait_clear_profile_valid(const struct mt76_rf_wait *s);
uint32_t mt76_path_address(unsigned index);
uint32_t mt76_path_value(unsigned index,uint32_t before);
bool mt76_path_request(unsigned operation,bool *write,uint32_t *address);
bool mt76_path_transfer_valid(const struct mt76_path *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_path_apply(struct mt76_path *s,const struct mt76_path_io *io);
/* Fixed read-only channel/calibration prerequisite inventory, not RF readiness. */
struct mt76_channel_state {bool attempted,complete,raw_ibi9;unsigned reads;uint32_t values[12],guards[2][3];};
bool mt76_path_exact_profile_valid(const struct mt76_path *s);
struct mt76_usb_cycle {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[1],after[1],guards[2][3];
};
struct mt76_usb_cycle_io {void *context;bool (*authorize)(void *);bool (*transfer)(void *,unsigned,bool,uint32_t,uint8_t *,unsigned);};
uint32_t mt76_usb_cycle_address(unsigned index);
uint32_t mt76_usb_cycle_value(unsigned index,uint32_t before);
bool mt76_usb_cycle_request(unsigned operation,bool *write,uint32_t *address);
bool mt76_usb_cycle_transfer_valid(const struct mt76_usb_cycle *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_usb_cycle_apply(struct mt76_usb_cycle *s,const struct mt76_usb_cycle_io *io);
bool mt76_usb_cycle_exact_profile_valid(const struct mt76_usb_cycle *s);
bool mt76_channel_observe_exact_profile_valid(const struct mt76_channel_state *s);
uint32_t mt76_channel_state_address(unsigned index);
uint32_t mt76_channel_state_request(unsigned operation);
bool mt76_channel_state_admit(const struct mt76_channel_state *s);
enum mt76_mcu_timing_status mt76_channel_state_capture(struct mt76_channel_state *s,const struct mt76_mcu_timing_io *io);
/* Separate observation policy: only IBI9 may report all-ones as raw data. */
bool mt76_channel_observe_admit(const struct mt76_channel_state *s);
unsigned mt76_channel_observe_all_ones(const struct mt76_channel_state *s);
enum mt76_mcu_timing_status mt76_channel_observe_capture(struct mt76_channel_state *s,const struct mt76_mcu_timing_io *io);
/* Fixed USB BW_SETTING=0 response observation; payload is not an ACK contract. */
struct mt76_bw_observe {bool attempted,complete;unsigned operations,reads,exchanges;int received;uint32_t guards[2][3],words[4];};
uint32_t mt76_bw_observe_guard_address(unsigned operation);
bool mt76_bw_observe_admit(const struct mt76_bw_observe *s,unsigned operation);
size_t mt76_bw_observe_packet(uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_bw_observe_capture(struct mt76_bw_observe *s,const struct mt76_mcu_timing_io *io);
/* Fixed disabled-MAC 20MHz, primary control0, 2GHz band/CCA layout. */
struct mt76_bbp20 {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[5],after[5],guards[10][3];
};
struct mt76_bbp20_io {void *context;bool (*authorize)(void *);bool (*transfer)(void *,unsigned,bool,uint32_t,uint8_t *,unsigned);};
uint32_t mt76_bbp20_address(unsigned index);
uint32_t mt76_bbp20_value(unsigned index,uint32_t before);
bool mt76_bbp20_request(unsigned operation,bool *write,uint32_t *address);
bool mt76_bbp20_transfer_valid(const struct mt76_bbp20 *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_bbp20_apply(struct mt76_bbp20 *s,const struct mt76_bbp20_io *io);
bool mt76_bbp20_exact_profile_valid(const struct mt76_bbp20 *s);
bool mt76_bw_observe_exact_profile_valid(const struct mt76_bw_observe *s);
uint32_t mt76_bbp20_initial(unsigned index);
/* Batched fixed RF-channel prerequisites:5 MAC observations+15 RF reads. */
struct mt76_channel_probe {bool attempted,complete;unsigned operations,reads,exchanges;int received;uint32_t guards[32][3],mac[5],words[15][4];};
uint32_t mt76_channel_probe_mac_address(unsigned index);
uint32_t mt76_channel_probe_rf_address(unsigned index);
bool mt76_channel_probe_request(unsigned operation,bool *command,uint32_t *address);
bool mt76_channel_probe_admit(const struct mt76_channel_probe *s,unsigned operation);
size_t mt76_channel_probe_packet(const struct mt76_channel_probe *s,unsigned operation,uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_channel_probe_capture(struct mt76_channel_probe *s,const struct mt76_mcu_timing_io *io);
/* Fixed reference 2GHz band step; no synthesizer, MAC enable or caller plan. */
#define MT76_RF_BAND2G_COUNT 70u
#define MT76_RF_BAND2G_COMMANDS 140u
#define MT76_RF_BAND2G_OPS 980u
uint32_t mt76_rf_band2g_address(unsigned index);
unsigned mt76_rf_band2g_value(unsigned index);
unsigned mt76_rf_band2g_sequence(unsigned command);
uint32_t mt76_rf_band2g_guard_address(unsigned operation);
size_t mt76_rf_band2g_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_rf_band2g_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_rf_band2g_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
bool mt76_rf_band2g_exact_profile_valid(const struct mt76_rf_table *s);
bool mt76_channel_probe_exact_profile_valid(const struct mt76_channel_probe *s);
struct mt76_band_mac {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[2],after[2],guards[4][3];
};
struct mt76_band_mac_io {void *context;bool (*authorize)(void *);bool (*transfer)(void *,unsigned,bool,uint32_t,uint8_t *,unsigned);};
uint32_t mt76_band_mac_address(unsigned index);
uint32_t mt76_band_mac_value(unsigned index,uint32_t before);
bool mt76_band_mac_request(unsigned operation,bool *write,uint32_t *address);
bool mt76_band_mac_transfer_valid(const struct mt76_band_mac *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_band_mac_apply(struct mt76_band_mac *s,const struct mt76_band_mac_io *io);
bool mt76_band_mac_exact_profile_valid(const struct mt76_band_mac *s);
uint32_t mt76_band_mac_initial(unsigned index);
/* Fixed channel1 synthesizer fields in reference order, never an RF-ready claim. */
struct mt76_synth1 {bool attempted,complete;unsigned operations,reads,exchanges,acks,observed;int received;uint32_t guards[126][3],words[63][4];};
uint32_t mt76_synth1_address(unsigned index);
unsigned mt76_synth1_mask(unsigned index);
unsigned mt76_synth1_bits(unsigned index);
unsigned mt76_synth1_initial(unsigned index);
unsigned mt76_synth1_value(unsigned index,unsigned before);
unsigned mt76_synth1_sequence(unsigned command);
uint32_t mt76_synth1_guard_address(unsigned operation);
bool mt76_synth1_admit(const struct mt76_synth1 *s,unsigned operation);
size_t mt76_synth1_packet(const struct mt76_synth1 *s,unsigned operation,uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_synth1_capture(struct mt76_synth1 *s,const struct mt76_mcu_timing_io *io);
bool mt76_synth1_exact_profile_valid(const struct mt76_synth1 *s);
/* Fixed post-channel20MHz/G-band replay; same numeric table, distinct owner/sequence. */
#define MT76_POST_COUNT 19u
#define MT76_POST_COMMANDS 38u
#define MT76_POST_OPS 266u
uint32_t mt76_post_address(unsigned index);
unsigned mt76_post_value(unsigned index);
unsigned mt76_post_sequence(unsigned command);
uint32_t mt76_post_guard_address(unsigned operation);
size_t mt76_post_packet(unsigned operation,uint8_t *out,size_t capacity);
bool mt76_post_admit(const struct mt76_rf_table *s,unsigned operation);
enum mt76_mcu_timing_status mt76_post_capture(struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io);
bool mt76_post_exact_profile_valid(const struct mt76_rf_table *s);
struct mt76_post_mac {
    bool attempted,complete;
    unsigned operations,reads,writes,verified;
    uint32_t before[3],after[3],guards[6][3];
};
struct mt76_post_mac_io {void *context;bool (*authorize)(void *);bool (*transfer)(void *,unsigned,bool,uint32_t,uint8_t *,unsigned);};
uint32_t mt76_post_mac_address(unsigned index);
uint32_t mt76_post_mac_value(unsigned index,uint32_t before);
bool mt76_post_mac_request(unsigned operation,bool *write,uint32_t *address);
bool mt76_post_mac_transfer_valid(const struct mt76_post_mac *s,unsigned operation,bool write,uint32_t address,const uint8_t *data,unsigned length);
enum mt76_mcu_timing_status mt76_post_mac_apply(struct mt76_post_mac *s,const struct mt76_post_mac_io *io);
bool mt76_post_mac_exact_profile_valid(const struct mt76_post_mac *s);
uint32_t mt76_post_mac_initial(unsigned index);
#endif
