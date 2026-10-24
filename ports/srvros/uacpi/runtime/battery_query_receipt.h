#ifndef SRVROS_BATTERY_QUERY_RECEIPT_H
#define SRVROS_BATTERY_QUERY_RECEIPT_H
#include <stdbool.h>
#include <stdint.h>
#include "battery_query_policy.h"
/* One whole query batch, including requeues. this is not a
 * native AML allowlist or a firmware ownership grant. Writes are conservative
 * attempted effects, not rollback entries. */
struct battery_query_receipt {
    uint64_t sequence,generation,aml_calls,aml_returns,effects,completed_before,notifications_before;
    uint8_t code,policy;
    bool transfer_done,in_aml,refused;
};
static inline bool battery_query_returned(const struct battery_query_receipt *r,
    uint64_t completed,bool idle) {
    return r && r->sequence && (r->generation || r->policy==BATTERY_QUERY_AIR72 || r->policy==BATTERY_QUERY_AIR61) && r->code &&
        (r->policy==BATTERY_QUERY_MODEL || r->policy==BATTERY_QUERY_AIR72 || r->policy==BATTERY_QUERY_AIR61) &&
        battery_query_rejected(battery_query_classify(r->policy,r->code)) && r->refused && r->transfer_done &&
        !r->in_aml && r->aml_calls==r->aml_returns && !r->effects &&
        completed>r->completed_before && idle;
}
#endif
