#ifndef SRVROS_EC_QUERY_BUDGET_H
#define SRVROS_EC_QUERY_BUDGET_H
#include <stdbool.h>
#include <stdint.h>

enum ec_query_fault {
    EC_QUERY_OK, EC_QUERY_CLOCK, EC_QUERY_DEADLINE, EC_QUERY_LIMIT,
    EC_QUERY_CONTEXT, EC_QUERY_TRANSPORT, EC_QUERY_AML, EC_QUERY_SCHEDULE
};
/* One notification/drain lifetime, including queued time and all reschedules.
 * The caller serializes access, keeps its source masked while draining and
 * checks for a pending event atomically with finishing/rearming the source.
 * This is a cooperative budget, not cancellation of AML or an EC transaction.
 * A separate lifecycle deadline must retain/halt on a worker that cannot return.
 * Failed state is sticky: reset only after external retirement/recovery. */
struct ec_query_budget {
    uint64_t started_us, last_us, timeout_us;
    unsigned attempts, limit;
    enum ec_query_fault fault;
    bool active;
};
bool ec_query_budget_begin(struct ec_query_budget *, uint64_t now,
    unsigned max_attempts, uint64_t timeout_us);
bool ec_query_budget_check(struct ec_query_budget *, uint64_t now);
bool ec_query_budget_take(struct ec_query_budget *, uint64_t now);
bool ec_query_budget_finish(struct ec_query_budget *, uint64_t now);
bool ec_query_budget_fail(struct ec_query_budget *, enum ec_query_fault);
const char *ec_query_fault_name(enum ec_query_fault);
#endif
