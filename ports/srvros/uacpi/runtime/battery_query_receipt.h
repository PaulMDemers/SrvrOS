#ifndef SRVROS_BATTERY_QUERY_RECEIPT_H
#define SRVROS_BATTERY_QUERY_RECEIPT_H
#include <stdbool.h>
#include <stdint.h>
/* One whole query batch, including requeues. VM study only: this is not a
 * native AML allowlist or a firmware ownership grant. Writes are conservative
 * attempted effects, not rollback entries. */
struct battery_query_receipt {
    uint64_t sequence,generation,aml_calls,aml_returns,effects,completed_before,notifications_before;
    uint8_t code;
    bool transfer_done,in_aml,refused;
};
static inline bool battery_query_model_admits(uint8_t code) { return code==0x20; }
static inline bool battery_query_returned(const struct battery_query_receipt *r,
    uint64_t completed,bool idle) {
    return r && r->sequence && r->generation && r->code &&
        !battery_query_model_admits(r->code) && r->refused && r->transfer_done &&
        !r->in_aml && r->aml_calls==r->aml_returns && !r->effects &&
        completed>r->completed_before && idle;
}
#endif
